/*
 * http client access using curl for the harvest application
 * Nigel Stuckey, June 2003.
 * Copyright System Garden Ltd 2003, All rights reserved
 */

#include <string.h>
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
     http_curlh = curl_easy_init();
     if (! http_curlh)
	  elog_die(FATAL, "unable to initialise curl");
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
 *    host		name of the web server
 *    userpwd		web server account details in the form user:password
 *    proxy		name of proxy server
 *    proxyuserpwd	proxy host account details in the form user:password
 *    sslkeypwd		password to unlock private key
 *    certfile		certificate file (held in etc)
 * The cookies tree may be modified by the received web page
 * Both cookies and auth may be NULL if you have nothing to pass
 * Returns the text of the page if successful or NULL for error.
 * The text should be nfree()ed after use
 */
char *http_get(char *url, 	/* standard url */
		TREE *cookies,	/* cookies: key-value list */
		TABLE auth,	/* authorisation table */
		int flags	/* flags */ )
{
     struct http_buffer buf = {NULL,0};
     CURLcode r;
     char *cert=NULL, *userpwd=NULL, *proxyuserpwd=NULL, *sslkeypwd=NULL;
     char *host, *certpath, cookies_str[HTTP_COOKIESTRLEN], *proxy=NULL;
     char errbuf[CURL_ERROR_SIZE];
     int hostlen, rowkey, i;

     /* Get host from url and look up in the auth table */
     if (auth) {
	  host = strstr(url, "://");
	  if (!host) {
	       elog_printf(ERROR, "url '%s' in unrecognisable format", 
			   url);
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
     curl_easy_setopt(http_curlh, CURLOPT_FAILONERROR);
     curl_easy_setopt(http_curlh, CURLOPT_FILE, (void *) &buf);
     curl_easy_setopt(http_curlh, CURLOPT_ERRORBUFFER, errbuf);
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

     /* load request with cookies */
     if (cookies && !tree_empty(cookies)) {
	  i=0;
	  tree_traverse(cookies) {
	       i += snprintf(cookies_str + i, HTTP_COOKIESTRLEN - i, 
			     "%s=%s; ", 
			     tree_getkey(cookies), (char *) tree_get(cookies));
	       if (i > HTTP_COOKIESTRLEN) {
		    cookies_str[HTTP_COOKIESTRLEN-1] = '\0';
		    break;
	       }
	  }
	  curl_easy_setopt(http_curlh, CURLOPT_COOKIE, cookies_str);
     }

     /* Diagnostic dump */
     elog_printf(DIAG, "HTTP GET %s", url);
     elog_printf(DIAG, "     ... userpwd=%s, proxy=%s, proxyuserpwd=%s, "
		       "sslkeypwd=%s, certpath=%s", 
		 userpwd      ? userpwd : "(none)",
		 proxy        ? proxy   : "(none)",
		 proxyuserpwd ? proxyuserpwd : "(none)",
		 sslkeypwd    ? sslkeypwd    : "(none)",
                 cert         ? certpath     : "(none)" );
     elog_printf(DIAG, "     ... cookies=%s", cookies ? cookies_str : "(none)");

     /* action the GET */
     r = curl_easy_perform(http_curlh);
     if (r)
	  elog_printf(ERROR, "HTTP GET error: %s", errbuf);
     else
	  elog_printf(DIAG,  "HTTP GET success");

     /* free and return */
     if (cert)
	  nfree(certpath);
     if (auth)
	  nfree(host);
     return(buf.memory);
}

/* interact with a web server using the POST protocol and multi-type
 * document content
 * Authorisation details are passed in a TABLE, one row per host, with
 * the following columns:-
 *    host		name of the web server
 *    userpwd		web server account details in the form user:password
 *    proxy		name of the proxy server
 *    proxyuserpwd	proxy host account details in the form user:password
 *    sslkeypwd		password to unlock private key
 *    certfile		certificate file (held in etc)
 * The cookies tree may be modified by the received web page
 * Returns the text sent back from the web server as a result of the 
 * form POST or NULL if there was a failure.
 * Returned text should be freed with nfree().
 */
char *http_post(char *url, 	/* standard url */
		TREE *form, 	/* key-value list of form items */
		TREE *files, 	/* file list: key=send name val=filename */
		TREE *upload, 	/* file list: key=send name val=data */
		TREE *cookies,	/* key-value list of cookies */
		TABLE auth,	/* authorisation table */
		int flags	/* flags */ )
{
     struct http_buffer buf = {NULL,0};
     CURLcode r;
     char *cert=NULL, *userpwd=NULL, *proxyuserpwd=NULL, *sslkeypwd=NULL;
     char *host, *certpath, cookies_str[HTTP_COOKIESTRLEN], *proxy=NULL;
     char errbuf[CURL_ERROR_SIZE], summary[50];
     int hostlen, rowkey, i;
     struct HttpPost *formpost=NULL;
     struct HttpPost *lastptr=NULL;

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
     curl_easy_setopt(http_curlh, CURLOPT_FAILONERROR);
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

     /* load form with cookies */
     if (cookies && !tree_empty(cookies)) {
	  i=0;
	  tree_traverse(cookies) {
	       i += snprintf(cookies_str + i, HTTP_COOKIESTRLEN - i, 
			     "%s=%s; ", 
			     tree_getkey(cookies), (char *) tree_get(cookies));
	       if (i > HTTP_COOKIESTRLEN) {
		    cookies_str[HTTP_COOKIESTRLEN-1] = '\0';
		    break;
	       }
	  }
	  curl_easy_setopt(http_curlh, CURLOPT_COOKIE, cookies_str);
     }

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
     elog_printf(DIAG, "HTTP POST %s", url);
     elog_printf(DIAG, "     ... userpwd=%s, proxy=%s, proxyuserpwd=%s, "
		       "sslkeypwd=%s, certpath=%s", 
		 userpwd      ? userpwd : "(none)",
		 proxy        ? proxy   : "(none)",
		 proxyuserpwd ? proxyuserpwd : "(none)",
		 sslkeypwd    ? sslkeypwd    : "(none)",
                 cert         ? certpath     : "(none)" );
     elog_printf(DIAG, "     ... cookies=%s", cookies ? cookies_str : "(none)");
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
			   tree_getkey(upload), summary, i>40 ? "..." : "", i);

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
     text = http_get("http://localhost", NULL, NULL, 0);
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
     text = http_get(sqlrs, NULL, NULL, 0);
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
