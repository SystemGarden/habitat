/*
 * http client access using curl for the harvest application
 * Nigel Stuckey, June 2003.
 * Copyright System Garden Ltd 2003, All rights reserved
 */

#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include "elog.h"
#include "itree.h"
#include "tree.h"
#include "util.h"
#include "nmalloc.h"
#include "iiab.h"
#include "http.h"

/* globals */
CURL *http_curlh = NULL;

/* private functional prototypes */
size_t http_receive(void *buffer, size_t size, size_t nmemb, void *userp);
size_t http_send(void *ptr, size_t size, size_t nmemb, void *userp);

/* initialise the curl class */
void http_init()
{
     unsetenv("HTTP_PROXY");	/* stop libcurl being affected by the 
				 * environment */
     http_curlh = curl_easy_init();
     if (! http_curlh)
	  elog_die(FATAL, "unable to initialise curl");

     /* general curl configuration  - avoid signals, don't call DNS 
      * excesively and dont wait too long to connect */
     curl_easy_setopt(http_curlh, CURLOPT_NOSIGNAL, (long) 1);
     curl_easy_setopt(http_curlh, CURLOPT_DNS_CACHE_TIMEOUT, (long) 3600);
     curl_easy_setopt(http_curlh, CURLOPT_CONNECTTIMEOUT, (long) 15);
}

/* shut down the curl class */
void http_fini()
{
     curl_global_cleanup();
}

/* 
 * Interact with a web server using the GET protocol.
 * Authorisation details are passed in a TABLE, one row per host, with
 * the following columns:-
 *    host		name of web server, key for url
 *    userpwd		web server account details in the form user:password
 *    proxy		url of the proxy server
 *    proxyuserpwd	proxy server account credentials - user:password
 *    sslkeypwd		password to unlock private key
 *    certfile		certificate file (held in etc)
 * Cookie is a key-value TREE list, the contents of which gets added 
 * to the stored cookies fron the cookie jar.
 * The cookie jar is a file name on the local machine to store the 
 * sookies from the web response, such as the session key for authentication.
 * Cookies may be NULL if there is nothing to pass. cookiejar may be NULL to
 * disable cookie storage, '-' to send then to stdout (debugging) or the 
 * value of a valid filename.
 * Returns the text of the page if successful or NULL for error.
 * The text should be nfree()ed after use
 */
char *http_get(char   *url, 	/* standard url */
	       CF_VALS cookies,	/* input cookies */
	       char   *cookiejar,/* filename for storage of returned cookies */
	       TABLE   auth,	/* authorisation table */
	       int     flags	/* flags */ )
{
     struct http_buffer buf = {NULL,0};
     CURLcode r;
     char *cert=NULL, *userpwd=NULL, *proxyuserpwd=NULL, *sslkeypwd=NULL;
     char *host, *certpath, cookies_str[HTTP_COOKIESTRLEN], *proxy=NULL;
     char errbuf[CURL_ERROR_SIZE];
     int hostlen, rowkey, i;
     TREE *cookie_list;

     /* Get host from url and look up in the auth table */
     host = strstr(url, "://");
     if (!host) {
          elog_printf(ERROR, "url '%s' in unrecognisable format", url);
	  return NULL;
     }
     host += 3;
     hostlen = strcspn(host, ":/");
     if (hostlen) {
          host = xnmemdup(host, hostlen+1);
	  host[hostlen] = '\0';
     } else
          host = xnstrdup("localhost");

     /* lookup auth and proxy config */
     if (auth) {
	  rowkey = table_search(auth, "host", host);
	  if (rowkey != -1) {
	       userpwd      = table_getcurrentcell(auth, "userpwd");
	       proxy        = table_getcurrentcell(auth, "proxy");
	       proxyuserpwd = table_getcurrentcell(auth, "proxyuserpwd");
	       sslkeypwd    = table_getcurrentcell(auth, "sslkeypwd");
	       cert         = table_getcurrentcell(auth, "cert");
	  }
     }

     /* prime curl with the auth information */
     curl_easy_setopt(http_curlh, CURLOPT_URL, url);
     curl_easy_setopt(http_curlh, CURLOPT_WRITEFUNCTION, http_receive);
     curl_easy_setopt(http_curlh, CURLOPT_FAILONERROR, NULL);
     curl_easy_setopt(http_curlh, CURLOPT_FILE, (void *) &buf);
     curl_easy_setopt(http_curlh, CURLOPT_ERRORBUFFER, errbuf);
     if (userpwd && *userpwd)
	  curl_easy_setopt(http_curlh, CURLOPT_USERPWD, userpwd);
     if (proxy && *proxy)
	  curl_easy_setopt(http_curlh, CURLOPT_PROXY, proxy);
     if (proxyuserpwd && *proxyuserpwd)
	  curl_easy_setopt(http_curlh, CURLOPT_PROXYUSERPWD, proxyuserpwd);
     if (sslkeypwd && *sslkeypwd)
	  curl_easy_setopt(http_curlh, CURLOPT_SSLKEYPASSWD, sslkeypwd);
     if (cert && *cert) {
	  certpath = util_strjoin(iiab_dir_etc, "/", cert, NULL);
	  curl_easy_setopt(http_curlh, CURLOPT_SSLCERT, certpath);
     }

     /* load request with cookies, which expects the format 
      * cookie=value; [c=v; ...] */
     cookies_str[0] = '\0';
     if (cookies) {
          cookie_list = cf_gettree(cookies);
	  i=0;
	  tree_traverse(cookie_list) {
	       i += snprintf(cookies_str + i, HTTP_COOKIESTRLEN - i, 
			     "%s=%s; ", 
			     tree_getkey(cookie_list), 
			     (char *) tree_get(cookie_list));
	       if (i > HTTP_COOKIESTRLEN) {
		    cookies_str[HTTP_COOKIESTRLEN-1] = '\0';
		    break;
	       }
	  }
	  curl_easy_setopt(http_curlh, CURLOPT_COOKIE, cookies_str);
	  tree_clearoutandfree(cookie_list);
	  tree_destroy(cookie_list);
     }
     if (cookiejar && *cookiejar)
	  curl_easy_setopt(http_curlh, CURLOPT_COOKIEJAR, cookiejar);

     /* Diagnostic dump */
     elog_printf(DIAG, "HTTP GET %s  ... userpwd=%s, proxy=%s, "
		 "proxyuserpwd=%s, sslkeypwd=%s, certpath=%s, cookies=<<%s>>"
		 "cookiejar=%s", 
		 url,
		 userpwd      && *userpwd      ? userpwd      : "(none)",
		 proxy        && *proxy        ? proxy        : "(none)",
		 proxyuserpwd && *proxyuserpwd ? proxyuserpwd : "(none)",
		 sslkeypwd    && *sslkeypwd    ? sslkeypwd    : "(none)",
                 cert         && *cert         ? certpath     : "(none)",
		 *cookies_str                  ? cookies_str  : "(none)",
		 cookiejar    && *cookiejar    ? cookiejar    : "(none)");

     /* action the GET */
     r = curl_easy_perform(http_curlh);
     if (r)
          elog_printf(DIAG, "HTTP GET error: %s (url=%s)", errbuf, url);
     else
	  elog_printf(DIAG, "HTTP GET success");

     /* free and return */
     if (cert && *cert)
	  nfree(certpath);
     if (host)
	  nfree(host);
     return(buf.memory);
}

/* interact with a web server using the POST protocol and multi-type
 * document content
 * Authorisation details are passed in a TABLE, one row per host, with
 * the following columns:-
 *    host		name of web server, key for url
 *    userpwd		web server account details in the form user:password
 *    proxy		url of the proxy server
 *    proxyuserpwd	proxy server account credentials - user:password
 *    sslkeypwd		password to unlock private key
 *    certfile		certificate file (held in etc)
 * Cookie is a key-value TREE list, the contents of which gets added 
 * to the stored cookies fron the cookie jar.
 * The cookie jar is a file name on the local machine to store the 
 * sookies from the web response, such as the session key for authentication.
 * Cookies may be NULL if there is nothing to pass. cookiejar may be NULL to
 * disable cookie storage, '-' to send then to stdout (debugging) or the 
 * value of a valid filename.
 * Returns the text sent back from the web server as a result of the 
 * form POST or NULL if there was a failure.
 * Returned text should be freed with nfree().
 */
char *http_post(char   *url, 	/* standard url */
		TREE   *form, 	/* key-value list of form items */
		TREE   *files, 	/* file list: key=send name val=filename */
		TREE   *upload,	/* file list: key=send name val=data */
		CF_VALS cookies,/* input cookies */
		char   *cookiejar,/* filenane to store returned cookies */
		TABLE   auth,	/* authorisation table */
		int     flags	/* flags */ )
{
     struct http_buffer buf = {NULL,0};
     CURLcode r;
     char *cert=NULL, *userpwd=NULL, *proxyuserpwd=NULL, *sslkeypwd=NULL;
     char *host, *certpath, cookies_str[HTTP_COOKIESTRLEN], *proxy=NULL;
     char errbuf[CURL_ERROR_SIZE], summary[50];
     int hostlen, rowkey, i;
     struct curl_httppost *formpost=NULL;
     struct curl_httppost *lastptr=NULL;
     TREE *cookie_list;

     /* Get host from url and look up in the auth table */
     if (auth) {
	  host = strstr(url, "://");
	  if (!host) {
	       elog_printf(ERROR, "url '%s' in unrecognisable format", url);
	       return NULL;
	  }
	  host += 3;
	  hostlen = strspn(host, "/");
	  if (hostlen) {
	       host = xnmemdup(host, hostlen+1);
	       host[hostlen] = '\0';
	  } else
	       host = xnstrdup("localhost");
	  rowkey = table_search(auth, "host", host);
	  if (rowkey != -1) {
	       userpwd      = table_getcurrentcell(auth, "userpwd");
	       proxy        = table_getcurrentcell(auth, "proxy");
	       proxyuserpwd = table_getcurrentcell(auth, "proxyuserpwd");
	       sslkeypwd    = table_getcurrentcell(auth, "sslkeypwd");
	       cert         = table_getcurrentcell(auth, "cert");
	  }
     }

     /* prime curl with the auth information */
     curl_easy_setopt(http_curlh, CURLOPT_URL, url);
     curl_easy_setopt(http_curlh, CURLOPT_WRITEFUNCTION, http_receive);
     curl_easy_setopt(http_curlh, CURLOPT_FAILONERROR, NULL);
     curl_easy_setopt(http_curlh, CURLOPT_FILE, (void *) &buf);
     if (userpwd)
	  curl_easy_setopt(http_curlh, CURLOPT_USERPWD, userpwd);
     if (proxy)
	  curl_easy_setopt(http_curlh, CURLOPT_PROXY, proxy);
     if (proxyuserpwd)
	  curl_easy_setopt(http_curlh, CURLOPT_PROXYUSERPWD, proxyuserpwd);
     if (sslkeypwd)
	  curl_easy_setopt(http_curlh, CURLOPT_SSLKEYPASSWD, sslkeypwd);
     if (cert) {
	  certpath = util_strjoin(iiab_dir_etc, "/", cert, NULL);
	  curl_easy_setopt(http_curlh, CURLOPT_SSLCERT, certpath);
     }

     /* load request with cookies, which expects the format 
      * cookie=value; [c=v; ...] */
     if (cookies) {
          cookie_list = cf_gettree(cookies);
	  i=0;
	  tree_traverse(cookie_list) {
	       i += snprintf(cookies_str + i, HTTP_COOKIESTRLEN - i, 
			     "%s=%s; ", 
			     tree_getkey(cookie_list), 
			     (char *) tree_get(cookie_list));
	       if (i > HTTP_COOKIESTRLEN) {
		    cookies_str[HTTP_COOKIESTRLEN-1] = '\0';
		    break;
	       }
	  }
	  curl_easy_setopt(http_curlh, CURLOPT_COOKIE, cookies_str);
	  tree_clearoutandfree(cookie_list);
	  tree_destroy(cookie_list);
     }
     if (cookiejar)
	  curl_easy_setopt(http_curlh, CURLOPT_COOKIEJAR, cookiejar);

     /* load environment with form parameters */
     if (form && !tree_empty(form)) {
	  i=0;
	  tree_traverse(form) {
	       curl_formadd(&formpost, 
			    &lastptr,
			    CURLFORM_COPYNAME,     tree_getkey(form),
			    CURLFORM_COPYCONTENTS, tree_get(form),
			    CURLFORM_END);
	  }
     }


     /* load form structure with files */
     /*headers = curl_slist_append(headers, "Content-Type: text/xml");*/
     if (files && !tree_empty(files)) {
	  tree_traverse(files) {
	       curl_formadd(&formpost, 
			    &lastptr,
			    CURLFORM_FILENAME,    tree_getkey(files),
			    CURLFORM_FILECONTENT, tree_get(files),
			    CURLFORM_END);
	  }
     }

     /* load form with data upload */
     if (upload && !tree_empty(upload)) {
	  tree_traverse(upload) {
	       curl_formadd(&formpost, 
			    &lastptr,
			    CURLFORM_COPYNAME,     tree_getkey(upload),
			    CURLFORM_BUFFER,       tree_getkey(upload),
			    CURLFORM_BUFFERPTR,    tree_get(upload),
			    CURLFORM_BUFFERLENGTH, strlen(tree_get(upload)),
			    CURLFORM_END);
	  }
     }

     /* Diagnostic dump */
     elog_printf(DIAG, "HTTP POST %s  ... userpwd=%s, proxy=%s, "
		 "proxyuserpwd=%s, sslkeypwd=%s, certpath=%s, cookies=<<%s>>"
		 "cookiejar=%s", 
		 url,
		 userpwd      ? userpwd      : "(none)",
		 proxy        ? proxy        : "(none)",
		 proxyuserpwd ? proxyuserpwd : "(none)",
		 sslkeypwd    ? sslkeypwd    : "(none)",
                 cert         ? certpath     : "(none)",
		 cookies      ? cookies_str  : "(none)",
		 cookiejar    ? cookiejar    : "(none)");

     if (form) {
	  tree_traverse(form)
	       elog_printf(DIAG, "     ... form %s=%s", tree_getkey(form), 
			   tree_get(form));
     }
     if (files) {
	  tree_traverse(files)
	       elog_printf(DIAG, "     ... files %s=%s", tree_getkey(files), 
			   tree_get(files));
     }
     if (upload) {
	  tree_traverse(upload) {
	       strncpy(summary, tree_get(upload), 40);
	       summary[40] = '\0';
	       i=strlen(tree_get(upload));
	       elog_printf(DIAG, "     ... upload %s=%s%s (%d)", 
			   tree_getkey(upload), summary, 
			   i>40 ? "...(truncated)" : "", i);

	  }
     }


     /* add the form and action the POST */
     curl_easy_setopt(http_curlh, CURLOPT_HTTPPOST, formpost);
     /*curl_easy_setopt(http_curlh, CURLOPT_HTTPGET, TRUE);*/
     /*elog_printf(DEBUG, "REP - sending\n");*/
     r = curl_easy_perform(http_curlh);
     if (r)
	  elog_printf(ERROR, "HTTP POST error: %s", errbuf);
     else
	  elog_printf(DIAG,  "HTTP POST success");

     /* free and return */
     curl_formfree(formpost);
     if (cert)
	  nfree(certpath);
     if (auth)
	  nfree(host);
     /*elog_printf(DEBUG, "after send, response is %s\n", buf.memory);*/
     if (buf.memory && buf.memory[0] == '<' && buf.memory[1] == '!') {
	  /* server side error, which should be flagged */
	  elog_printf(ERROR, "HTTP server-side posting error: %s",
		      buf.memory);
	  return xnstrdup("HTTP server-side posting error (see log)");
     }
     return(buf.memory);
}


/* curl callback to receive data */
size_t http_receive(void *buffer, size_t size, size_t nmemb, void *userp) {
     int realsize = size * nmemb;
     struct http_buffer *mem = (struct http_buffer *) userp;

     mem->memory = (char *) xnrealloc(mem->memory, mem->size + realsize + 1);
     if (mem->memory) {
	  memcpy(&(mem->memory[mem->size]), buffer, realsize);
	  mem->size += realsize;
	  mem->memory[mem->size] = 0;
     }
     return realsize;
}

/* curl callback to send data */
size_t http_send(void *ptr, size_t size, size_t nmemb, void *userp) {
     struct http_buffer *mem = (struct http_buffer *) userp;

     if(size*nmemb < 1)
	  return 0;

     /* the below needs some work */
     if(mem->size) {
	  *(char *)ptr = mem->memory[0]; /* copy one single byte */
	  mem->memory++;                 /* advance pointer */
	  mem->size--;                   /* less data left */
	  return 1;                      /* we return 1 byte at a time! */
     }

     return -1;                         /* no more data left to deliver */
}


#if TEST

#include <stdlib.h>
#include "iiab.h"

int main(int argc, char **argv) {
     char *text, *sqlrs;

     iiab_start("", argc, argv, "", "");

     http_init();

     /* test 1: get a sample page from localhost with no options */
     text = http_get("http://localhost", NULL, NULL, NULL, 0);
     if (!text)
	  elog_die(FATAL, "[1] no text returned");

     /* remaining tests require harvest location from cmdline */
     if (argc < 2) {
	  elog_printf(INFO, "** Usage for further tests: %s <local_sqlrs_url>",
		      cf_getstr(iiab_cf, "argv0"));
	  http_fini();
	  iiab_stop();
	  exit(0);
     }
     sqlrs = util_strjoin("http://localhost/harvestapp/pl/sqlrs_get.pl?a=",
			  cf_getstr(iiab_cf, "argv1"), NULL);

     /* test 2: get a list of hosts in harvest */
     printf("fetching %s\n", sqlrs);
     text = http_get(sqlrs, NULL, NULL, NULL, 0);
     if (!text)
	  elog_die(FATAL, "[2] no text returned");
     puts(text);

     nfree(sqlrs);
     http_fini();

     iiab_stop();
     fprintf(stderr, "%s: tests finished successfully\n", argv[0]);
     exit(0);
}

#endif /* TEST */
