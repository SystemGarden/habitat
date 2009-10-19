/*
 * HTTP class contains the server code necessary for to serve http
 * connections. All the code is single threaded and uses meth to handle
 * the central I/O select() statement.
 * Based on mini_httpd.c from Jef Poskanzer (jef@acme.com)
 *
 * Nigel Stuckey, January-March 2001
 * Copyright System Garden Limited 2001. All rights reserved.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/utsname.h>
#include "cf.h"
#include "iiab.h"
#include "util.h"
#include "nmalloc.h"
#include "tree.h"
#include "callback.h"
#include "meth.h"
#include "httpd.h"

char  *httpd_serve_interface;
int    httpd_serve_port;
char   httpd_hostname_buf[32];
int    httpd_listen4_fd, httpd_listen6_fd;
int    httpd_active=0;	/* connections accepted (1) or refused (0) */
TREE  *httpd_paths;	/* list of accepted paths */
char  *httpd_schema_nameval[] =  {"name", "value", NULL};
struct httpd_status {
     int number;
     char *title;
} httpd_status_text[] = {
     {100, "Continue"},
     {101, "Switching Protocols"},
     {200, "OK"},
     {201, "Created"},
     {202, "Accepted"},
     {203, "Non-Authoritative Information"},
     {204, "No Content"},
     {205, "Reset Content"},
     {206, "Partial Content"},
     {300, "Multiple Choices"},
     {301, "Moved Permanently"},
     {302, "Found"},
     {303, "See Other"},
     {304, "Not Modified"},
     {305, "Use Proxy"},
     {307, "Temporary Redirect"},
     {400, "Bad Request"},
     {401, "Unauthorized"},
     {402, "Payment Required"},
     {403, "Forbidden"},
     {404, "Not Found"},
     {405, "Method Not Allowed"},
     {406, "Not Acceptable"},
     {407, "Proxy Authentication Required"},
     {408, "Request Time-out"},
     {409, "Conflict"},
     {410, "Gone"},
     {411, "Length Required"},
     {412, "Precondition Failed"},
     {413, "Request Entity Too Large"},
     {414, "Request-URI Too Large"},
     {415, "Unsupported Media Type"},
     {416, "Requested range not satisfiable"},
     {417, "Expectation Failed"},
     {500, "Internal Server Error"},
     {501, "Not Implemented"},
     {502, "Bad Gateway"},
     {503, "Service Unavailable"},
     {504, "Gateway Time-out"},
     {505, "HTTP Version not supported"},
     {-1,  "Unknown status"}
};


/*
 * Initialise the http class.
 * This does not start http serving, which is controlled by
 * httpd_start() and httpd_stop().
 */
void httpd_init()
{
     if (cf_defined(iiab_cf, HTTPD_CF_INTERFACE))
	  httpd_serve_interface = cf_getstr(iiab_cf, HTTPD_CF_INTERFACE);
     else
	  httpd_serve_interface = NULL;

     if (cf_defined(iiab_cf, HTTPD_CF_PORT))
	  httpd_serve_port = cf_getint(iiab_cf, HTTPD_CF_PORT);
     else
	  httpd_serve_port = HTTPD_PORT_HTTP;

     httpd_paths = tree_create();
}


/* shutdown http class */
void httpd_fini()
{
     tree_destroy(httpd_paths);
}


/*
 * Add a path and call back pair to be served by httpd.
 * httpd_evaluate() will traverse the tree looking for a string subset match
 * starting from the begining of the strings. eg. adding "/tom" will
 * match a GET for /tom/dick/harry and will have the index set to 4.
 * 
 * The signature of the callback should be:-
 *   1. requested path (char *)
 *   2. length of path matched, use as an index on path to find
 *      the unmatched (wildcard) parts (int)
 *   3. method (eg HTTPD_METHOD_GET) (int)
 *   4. headers in the request (char *)
 *   5. inbound data (ie from POST) (char *)
 *   6. output length of returned string
 *   7. output headers
 *   8. modification time of data
 *   R. returns the nmalloc()ed string to send back, which will be
 *      freed by httpd_evaluate() after being sent back to users
 */
void httpd_addpath(char *path, char * (*cb)(char *, int, int, TREE *, char *,
					    int *, TREE **, time_t *))
{
     tree_add(httpd_paths, path, cb);
}


/* remove the callback entry defined by path */
void httpd_rmpath(char *path)
{
     if (tree_find(httpd_paths, path) != TREE_NOVAL)
	  tree_rm(httpd_paths);
}


/*
 * Sets up a method of serving HTTP and HTTPS requests by
 * 1. seting up and listening on the appropreate sockets
 * 2. establishing a callback with meth to serve requests.
 * When this routine finishes, subsequent meth_relay() calls will
 * pickup pending HTTP/HTTPS requests and will emit callback events.
 * meth_accept() handles the callback events for both types, 
 * accepts the connection and handles the requests.
 * Stop the service with httpd_stop().
 * This routine is not called by httpd_init(), as servers are optional.
 * meth_init() should have been called before this is called.
 * Returns 1 for successful, 0 for failure.
 */ 
int httpd_start()
{
     httpd_usockaddr host_addr4;
     httpd_usockaddr host_addr6;
     int gotv4, gotv6;

     /* check if serving has been disabled */
     if (cf_getint(iiab_cf, HTTPD_CF_DISABLE) == -1) {
	  elog_printf(WARNING, "http serving requested but disabled in configuration");
	  return 0;
     }

     /* Look up hostname. */
     httpd_lookup_if(&host_addr4, sizeof(host_addr4), &gotv4,
		     &host_addr6, sizeof(host_addr6), &gotv6 );
     if ( httpd_serve_interface == NULL )
     {
	  gethostname( httpd_hostname_buf, sizeof(httpd_hostname_buf) );
	  httpd_serve_interface = httpd_hostname_buf;
     }
     if ( ! ( gotv4 || gotv6 ) )
     {
	  elog_printf(ERROR, "can't find any valid address" );
	  return 0;
     }

     /*
      * Initialize listen sockets.
      * Try v6 first because of a Linux peculiarity: unlike other systems, 
      * it has magical v6 sockets that also listen for v4,
      * but if you bind a v4 socket first then the v6 bind fails.
      */
     if ( gotv6 )
	  httpd_listen6_fd = httpd_listen( &host_addr6 );
     else
	  httpd_listen6_fd = -1;
     if ( gotv4 )
	  httpd_listen4_fd = httpd_listen( &host_addr4 );
     else
	  httpd_listen4_fd = -1;
     /* If we didn't get any valid sockets, fail. */
     if ( httpd_listen4_fd == -1 && httpd_listen6_fd == -1 )
     {
	  elog_printf(ERROR, "can't bind to any address");
	  return 0;
     }

#ifdef USE_SSL
     if ( do_ssl )
     {
	  SSLeay_add_ssl_algorithms();
	  SSL_load_error_strings();
	  ssl_ctx = SSL_CTX_new( SSLv23_server_method() );
	  if ( SSL_CTX_use_certificate_file( ssl_ctx, CERT_FILE, 
					     SSL_FILETYPE_PEM ) == 0 ||
	       SSL_CTX_use_PrivateKey_file( ssl_ctx, KEY_FILE, 
					    SSL_FILETYPE_PEM ) == 0 ||
	       SSL_CTX_check_private_key( ssl_ctx ) == 0 )
	  {
	       ERR_print_errors_fp( stderr );
	       return 0;
	  }
     }
#endif /* USE_SSL */

     /* establish callback event and tell meth to monitor the socket */
     callback_regcb(HTTPD_CB_ACCEPT, (void *) httpd_accept);
     if (gotv6)
	  meth_add_fdcallback(httpd_listen6_fd, HTTPD_CB_ACCEPT);
     else
	  meth_add_fdcallback(httpd_listen4_fd, HTTPD_CB_ACCEPT);

     httpd_active = 1;	/* accept connections */

     /* tell everybody */
     elog_printf(DIAG, "Listening for HTTP requests on interface %s port %d", 
		 httpd_serve_interface==NULL ? "(null)" : httpd_serve_interface, 
		 httpd_serve_port);

     return 1;
}



/*
 * Shut down the ability to handle HTTP/HTTPD requests.
 * The callback remains in place to sync any outstanding connection
 * requests, but a flag is set to ensure that nothing is done about
 * them
 */
void httpd_stop()
{
     /* disable work */
     httpd_active = 0;

     /* close socket and tell meth to ignore it */
     if (httpd_listen4_fd == -1) {
	  close(httpd_listen6_fd);
	  meth_rm_fdcallback(httpd_listen6_fd);
     } else {
	  close(httpd_listen6_fd);
	  meth_rm_fdcallback(httpd_listen4_fd);
     }
}



/*
 * Accept connections for an HTTP request.
 * The routine is written to handle events from the callback class (hence
 * the void* casts as an argument when we really want an int). 
 * The first argument is the file descriptor to read, the remainder are unused.
 * Returns nothing.
 */
void httpd_accept(void *fd /* would be an int */)
{
     httpd_usockaddr usa;
     int conn_fd, sz, totbytes=0, bytes=0, method;
     char buf[10000], *request=NULL;
     char *path, *reqdata=NULL, *datablock=NULL;
     TREE *headers;
     int content_length, datalen, r;

     /* establish a connection to recieve data */
     if ( (int) (long) fd == -1 ) {
	  elog_send(ERROR, "unable to accept");
	  return /*0*/;
     } else {
	  sz = sizeof(usa);
	  conn_fd = accept( (int) (long) fd, &usa.sa, &sz );
     }

     if (conn_fd < 0) {
	  if (errno != EINTR) {
	       elog_printf(ERROR, "stopping HTTP service - accept %d error: %s", 
			   (int) (long) fd, strerror(errno));
	       httpd_stop();
	  }
	  return /*0*/;
     }

     /* if we are in an inactive state, close the connection */
     if ( ! httpd_active ) {
	  close((int) (long) fd);
	  return;
     }

#ifdef USE_SSL
    if ( do_ssl )
	{
	ssl = SSL_new( ssl_ctx );
	SSL_set_fd( ssl, conn_fd );
	if ( SSL_accept( ssl ) == 0 )
	    {
		 ERR_print_errors_fp( stderr );
		 exit( 1 );
	    }
	}
#endif /* USE_SSL */

    /* collect the data into an nmalloc()ed buffer */
    for (;;) {
	 bytes = read( conn_fd, buf, sizeof(buf) );
	 if (bytes <= 0)
	      break;
	 request = xnrealloc(request, totbytes + bytes +1);
	 strncpy(request + totbytes, buf, bytes);
	 totbytes += bytes;
	 request[totbytes] = '\0';

	 /* check for the end of client's headers: a blank line */
	 if ( (reqdata = strstr( request, "\r\n\r\n" )) != NULL ) {
	      memset(reqdata, 0, 4);	/* terminate headers */
	      reqdata += 4;
	      break;
	 }
	 if ( (reqdata = strstr( request, "\n\n" )) != NULL ) {
	      memset(reqdata, 0, 2);	/* terminate headers */
	      reqdata += 2;
	      break;
	 }
    }

    /* At this stage, we have all the headers in request and any data
     * from the request in data. We do nothing with reqdata now, just 
     * process the headers */
    if (httpd_active) {
	 /* break code down into important parts */
	 method = httpd_request_scan(conn_fd, request, &path, &headers);
	 if (method == HTTPD_METHOD_FAIL)
	      goto close_connection;

	 elog_printf(DEBUG, "HTTP request %s %s", request, path);
	 /*tree_strdump(headers, "");*/

	 /* consume reqdata depending on the Content-length header */
	 if (tree_find(headers, "Content-length") != TREE_NOVAL) {
	      /* do we need to get more data from the socket? */
	      content_length = strtol(tree_get(headers), NULL, 10);
	      datalen = totbytes - (reqdata - buf);
	      if (content_length > datalen) {
		   /* we need more data that we have collected in the 
		    * header block.  Copy it to a data block so
		    * we leave the header pointers intact and 
		    * continue to fetch data */
		   datablock = xnmalloc(content_length+1);
		   strncpy(datablock, reqdata, datalen);
		   if (datalen < content_length) {
			r = read(conn_fd, datablock+datalen, 
				 content_length - datalen);
			if (r == 0 || r <= 0) {
			     /* I dont want to deal with slow, large 
			      * transfers, so abort this request. 
			      * Sorry but I'm not a web server!! */
			     goto close_connection;
			}
		   }
		   datablock[content_length] = '\0';
		   reqdata = datablock;	   
	      }
	 }

	 /* evaluate and send back a response */
	 httpd_response_evaluate(conn_fd, method, path, headers, reqdata);
    }

 close_connection:
    if (datablock)
	 nfree(datablock);
    close(conn_fd);
    nfree(request);
    tree_destroy(headers);

    return /*1*/;
}



/*
 * Scan the HTTP request headers in request and output the following.
 * path    - path text
 * headers - Returned TREE containing the header tokens in key, their
 *           strings in value.
 * Returns the HTTP method or -1 (HTTPD_METHOD_FAIL) for failure, in which
 * case none of the outputs are valid. If an error is returned, a suitable
 * message will have been sent to fd.
 * When finished with the outputs, call util_freescan() on headers.
 * All other data is provided by request. Warning, request will be altered.
 */
int httpd_request_scan(int fd, char *request, char **path, TREE **headers)
{
     ITREE *lines, *cols;
     char *pt, *tok, *start, *hdr;
     int method, r;

     /* get rid of quaint carrage returns and put spaces in its place */
     for (pt=request; *pt != '\0'; pt++)
	  if (*pt == '\r')
	       *pt = ' ';

     /* remove leading whitespace */
     for (start = request; isspace(*start); start++)
	  ;
     
     /* split the start line from the headers */
     hdr = strchr(start, '\n');
     *hdr = '\0';
     hdr++;

     /* match: <method> <path> <protocol> */
     tok = strchr(start, ' ');		/* path */
     *tok = '\0';
     tok++;
     pt = strchr(tok, ' ');		/* protocol */
     *pt = '\0';

     /* parse method */
     if (strcmp(start, "GET") == 0) {
	  method = HTTPD_METHOD_GET;
     } else if (strcmp(start, "POST") == 0) {
	  method = HTTPD_METHOD_POST;
     } else if (strcmp(start, "HEAD") == 0) {
	  method = HTTPD_METHOD_HEAD;
     } else {
	  method = HTTPD_METHOD_FAIL;
	  httpd_error_send(fd, 501, "Method not implemented");
	  return method;
     }

     /* decode the path */
     util_strdecode(tok, tok);
     if ( *tok != '/' ) {
	  httpd_error_send(fd, 400, "Bad path" );
	  return HTTPD_METHOD_FAIL;
     }
     *path = tok;

     /* scan the request headers into columns and rows, split column on 
      * colon, which means we capture spaces!! */
     r = util_scantext(hdr, ":", UTIL_MULTISEP, &lines);
     if (r <= 0)
	  return HTTPD_METHOD_FAIL;

     /* ignore the rest of the start line and scan the headers */
     *headers = tree_create();
     itree_traverse(lines) {
	  cols = itree_get(lines);
	  if (itree_n(cols) < 2)
	       continue;		/* not enough tokens */

	  itree_first(cols);		/* token name */
	  tok = itree_get(cols);
	  itree_next(cols);		/* token value (strip leading ws) */
	  pt = itree_get(cols);
	  while (isspace(*pt))
	       pt++;
	  tree_add(*headers, tok, pt);
     }

     return method;
}



/*
 * Evaluate the request and send back a response for it.
 */
void httpd_response_evaluate(int fd, int method, char *path, TREE *headers, 
			     char *data)
{
     char *key;
     int len, rlen;
     char *(*cb)();
     char *result;
     TREE *rheaders;
     time_t modt;

     tree_traverse(httpd_paths) {
	  key = tree_getkey(httpd_paths);
	  len = strlen(key);
	  if (strncmp(path, key, len) == 0) {
	       cb = tree_get(httpd_paths);
	       result = cb(path, len, method, headers, data, &rlen, &rheaders, 
			   &modt);
	       httpd_header_send(fd, rheaders, 200, "text/html", rlen, modt);
	       write(fd, result, rlen);
	       nfree(result);
	       /* rheaders ignored for now and not freed */
	       return;
	  }
     }

     httpd_error_send(fd, 404, "can't find that thing for you");
}


/* returns the text of the status number */
char *httpd_status_title(int status)
{
     int i;
     char *title;

     /* get the right status title */
     for (i=0; httpd_status_text[i].number != -1; i++) {
	  if (httpd_status_text[i].number == status) {
	       title = httpd_status_text[i].title;
	       break;
	  }
     }
     if (httpd_status_text[i].number == -1)
	  title = httpd_status_text[i].title;

     return title;
}



/*
 * Send an error message back to the expetant requestor using 
 * file descriptor fd.
 * Errnum is the standard HTTP error codes (see RFC2616)
 * Extra_headers is text to fill with returned headers
 * Text is more specific information
 */
void httpd_error_send(int fd, int errnum, char *text)
{
     char buf[2048];
     int n, idx, i;
     char *status;

     status = httpd_status_title(errnum);
     n = snprintf(buf, sizeof(buf),
		  "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\n"
		  "<BODY BGCOLOR=\"#ffffff\">"
		  "<font face=\"helvetica,sans-serif\" color=#006699 size=3>"
		  "<b>s y s t e m<br>g a r d e n<br>"
		  "<font color=#003366>h a b i t a t</font></b></font>"
		  "<br><br><br><H4>%d %s</H4>\n%s\n<!--",
		  errnum, status, errnum, status, text);
     
     for (i=0; i<6; i++) {
	  n += snprintf(buf+n, sizeof(buf)-n, "Padding so that MSIE deigns "
			"to show this error instead of its own canned one.\n");
     }
     n += snprintf(buf+n, sizeof(buf)-n, "-->\n<HR>\n<ADDRESS><A HREF=\"%s\">"
		   "%s</A></ADDRESS>\n</BODY></HTML>\n", 
		   HTTPD_SOFTWARE, HTTPD_URL);

     idx = httpd_header_send(fd, NULL, errnum, "text/html", n, 0);
     write(fd, buf, n);

     return;
}


/*
 * Send headers back to the requestor using file descriptor fd.
 * The header Status will override the status value.
 * headers may be NULL, status should always exist,
 * mime_type may be NULL, content_length may be 0 and so can last_modified.
 * Reutrns an index into httpd_status_text that corresponds to the
 * the appropreate status
 */
int httpd_header_send(int fd, TREE *headers, int user_status, char *mime_type, 
		      int content_length, time_t last_modified)
{
     time_t now;
     int status, n, i;
     char buf[2048], nowstr[50], modstr[50], *title;
     char *rfc1123_fmt = "%a, %d %b %Y %H:%M:%S GMT";

     /* format dates */
     now = time(NULL);
     strftime(nowstr, 50, rfc1123_fmt, gmtime(&now));
     strftime(modstr, 50, rfc1123_fmt, gmtime(&last_modified));
/*     cftime(nowstr, rfc1123_fmt, &now);
       cftime(modstr, rfc1123_fmt, &last_modified);*/

     /* get the status response */
     status = user_status;
     if (headers) {
	  if (tree_find(headers, "Status") != TREE_NOVAL)
	    status = strtol(tree_get(headers), NULL, 10);
	  if (tree_find(headers, "Location") != TREE_NOVAL)
	       status = 302;
     }

     title = httpd_status_title(status);

     /* write status */
     n = snprintf(buf, sizeof(buf), "HTTP/1.0 %d %s\r\n", status, title);

     /* write additional_headers */
     if (headers)
	  tree_traverse(headers)	  
	       n += snprintf(buf+n, sizeof(buf)-n, "%s: %s\r\n", 
			     tree_getkey(headers), (char *) tree_get(headers));

     /* write standard headers */
     n += snprintf(buf+n, sizeof(buf)-n, 
		   "Server: %s\r\n"
		   "Date: %s\r\n",
		   HTTPD_SOFTWARE, nowstr);
     if (mime_type && *mime_type)
	  n += snprintf(buf+n, sizeof(buf)-n, "Content-type: %s\r\n", 
			mime_type);
     if (content_length > 0)
	  n += snprintf(buf+n, sizeof(buf)-n, "Content-length: %d\r\n",
			content_length);
     if (last_modified > 0)
	  n += snprintf(buf+n, sizeof(buf)-n, "Last-modified: %s\r\n", modstr);
     n += snprintf(buf+n, sizeof(buf)-n, "Connection: close\r\n\r\n");

     write(fd, buf, strlen(buf));

     return i;
}


/*
 * Supports both v4 and v6 IP adresses
 * Returns 1 for successful or 0 otherwise
 */
int httpd_lookup_if(httpd_usockaddr* usa4P, size_t sa4_len, int* gotv4P, 
		    httpd_usockaddr* usa6P, size_t sa6_len, int* gotv6P )
{
#if defined(HAVE_GETADDRINFO) && defined(HAVE_GAI_STRERROR)

     struct addrinfo hints;
     struct addrinfo* ai;
     struct addrinfo* ai2;
     struct addrinfo* aiv4;
     struct addrinfo* aiv6;
     int gaierr;
     char strport[10];

     memset( &hints, 0, sizeof(hints) );
     hints.ai_family = AF_UNSPEC;
     hints.ai_flags = AI_PASSIVE;
     hints.ai_socktype = SOCK_STREAM;
     snprintf( strport, sizeof(strport), "%d", port );
     if ( (gaierr = getaddrinfo( httpd_serve_interface, strport, &hints, &ai )) != 0 ) {
	  elog_printf(ERROR, "getaddrinfo %.80s - %s\n", hostname, 
		      gai_strerror( gaierr ) );
	  return 0;
     }

     /* Find the first IPv4 and IPv6 entries. */
     aiv4 = (struct addrinfo*) 0;
     aiv6 = (struct addrinfo*) 0;
     for ( ai2 = ai; ai2 != (struct addrinfo*) 0; ai2 = ai2->ai_next ) {
	  switch ( ai2->ai_family ) {
	  case AF_INET:
	       if ( aiv4 == (struct addrinfo*) 0 )
		    aiv4 = ai2;
	       break;
#if defined(AF_INET6) && defined(HAVE_SOCKADDR_IN6)
	  case AF_INET6:
	       if ( aiv6 == (struct addrinfo*) 0 )
		    aiv6 = ai2;
	       break;
#endif /* AF_INET6 && HAVE_SOCKADDR_IN6 */
	  }
     }

     if ( aiv4 == (struct addrinfo*) 0 )
	  *gotv4P = 0;
     else
     {
	  if ( sa4_len < aiv4->ai_addrlen ) {
	       elog_printf(ERROR, "%.80s - sockaddr too small (%d < %d)\n",
			   httpd_serve_interface, sa4_len, aiv4->ai_addrlen );
	       return 0;
	  }
	  memset( usa4P, 0, sa4_len );
	  memcpy( usa4P, aiv4->ai_addr, aiv4->ai_addrlen );
	  *gotv4P = 1;
     }
     if ( aiv6 == (struct addrinfo*) 0 )
	  *gotv6P = 0;
     else {
	  if ( sa6_len < aiv6->ai_addrlen ) {
	       elog_printf(ERROR, "%.80s - sockaddr too small (%d < %d)\n",
			   httpd_serve_interface, sa6_len, aiv6->ai_addrlen );
	       return 0;
	  }
	  memset( usa6P, 0, sa6_len );
	  memcpy( usa6P, aiv6->ai_addr, aiv6->ai_addrlen );
	  *gotv6P = 1;
     }

     freeaddrinfo( ai );

#else /* HAVE_GETADDRINFO && HAVE_GAI_STRERROR */

     struct hostent* he;
     
     *gotv6P = 0;
     
     memset( usa4P, 0, sa4_len );
     usa4P->sa.sa_family = AF_INET;
     if ( httpd_serve_interface == (char*) 0 )
	  usa4P->sa_in.sin_addr.s_addr = htonl( INADDR_ANY );
     else
     {
	  usa4P->sa_in.sin_addr.s_addr = inet_addr( httpd_serve_interface );
	  if ( (int) usa4P->sa_in.sin_addr.s_addr == -1 )
	  {
	       he = gethostbyname( httpd_serve_interface );
	       if ( he == (struct hostent*) 0 )
	       {
#ifdef HAVE_HSTRERROR
		    elog_printf(ERROR, "gethostbyname %.80s - %s\n", 
				httpd_serve_interface, hstrerror( h_errno ) );
#else /* HAVE_HSTRERROR */
		    elog_printf(ERROR, "gethostbyname %.80s failed\n",
				httpd_serve_interface );
#endif /* HAVE_HSTRERROR */
		    return 0;
	       }
	       if ( he->h_addrtype != AF_INET )
	       {
		    elog_printf(ERROR, "%.80s - non-IP network address\n", 
				httpd_serve_interface );
		    return 0;
	       }
	       (void) memcpy(
		    &usa4P->sa_in.sin_addr.s_addr, he->h_addr, he->h_length );
	  }
     }
     usa4P->sa_in.sin_port = htons( httpd_serve_port );
     *gotv4P = 1;

#endif /* HAVE_GETADDRINFO && HAVE_GAI_STRERROR */

     return 1;
}




/* Return length of appropreate sockaddr or 0 for error */
size_t httpd_sockaddr_len( httpd_usockaddr* usaP )
{
     switch ( usaP->sa.sa_family )
     {
     case AF_INET: return sizeof(struct sockaddr_in);
#if defined(AF_INET6) && defined(HAVE_SOCKADDR_IN6)
     case AF_INET6: return sizeof(struct sockaddr_in6);
#endif /* AF_INET6 && HAVE_SOCKADDR_IN6 */
     default:
	  elog_printf(ERROR, "unknown sockaddr family - %d", 
		      usaP->sa.sa_family );
	  return 0;
     }
}



/*
 * Set up the http sockets on the server so that they accept incomming 
 * requests from clients.
 * Returns the file descriptor if successful or -1 for failure.
 */
int httpd_listen( httpd_usockaddr* usaP )
{
     int listen_fd;
     int i;

     listen_fd = socket( usaP->sa.sa_family, SOCK_STREAM, 0 );
     if ( listen_fd < 0 ) {
	  elog_printf(ERROR, "unable to open server socket: %s", 
		      strerror(errno));
	  return -1;
     }

     /* keep open over exec call */
     fcntl( listen_fd, F_SETFD, 1 );

     i = 1;
     if ( setsockopt( listen_fd, SOL_SOCKET, SO_REUSEADDR, (char*) &i, 
		      sizeof(i) ) < 0 ) {
	  elog_printf(ERROR, "unable to set server socket options: %s", 
		      strerror(errno));
	  return -1;
     }
     if ( bind( listen_fd, &usaP->sa, httpd_sockaddr_len( usaP ) ) < 0 ) {
	  elog_printf(ERROR, "unable to bind to server socket: %s", 
		      strerror(errno));
	  return -1;
     }
     if ( listen( listen_fd, 1024 ) < 0 ) {
	  elog_printf(ERROR, "unable to set listening on server "
		      "socket: %s", strerror(errno));
	  return -1;
     }

     return listen_fd;
}



/* ping agent */
char *httpd_builtin_ping(char *path, int match, int method, TREE *headers_in, 
			 char *data_in, int *length_out, TREE **headers_out, 
			 time_t *modt_out) {
     char *r;

     r = xnstrdup("hello, world\n");
     *length_out = strlen(r);
     *headers_out = NULL;
     *modt_out = time(NULL);
     return r;
}

/* current instance configuration */
char *httpd_builtin_cf(char *path, int match, int method, TREE *headers_in, 
		       char *data_in, int *length_out, TREE **headers_out, 
		       time_t *modt_out) {
     char *r;
     TABLE t;

     t = cf_getstatus(iiab_cf);
     r = table_html(t, -1, -1, NULL);
     table_destroy(t);
     *length_out = strlen(r);
     *headers_out = NULL;
     *modt_out = time(NULL);
     return r;
}

/* logging locations */
char *httpd_builtin_elog(char *path, int match, int method, TREE *headers_in, 
			 char *data_in, int *length_out, TREE **headers_out, 
			 time_t *modt_out) {
     char *r;
     TABLE t;

     t = elog_getstatus(iiab_cf);
     r = table_html(t, -1, -1, NULL);
     table_destroy(t);
     *length_out = strlen(r);
     *headers_out = NULL;
     *modt_out = time(NULL);
     return r;
}


/* host information in tsv format */
char *httpd_builtin_info(char *path, int match, int method, TREE *headers_in, 
			 char *data_in, int *length_out, TREE **headers_out, 
			 time_t *modt_out) {
     TABLE tab;
     struct utsname uts;
     char *rstr;
     TABLE t;
     int r;

     /* grab the uname structure */
     r = uname(&uts);
     if (r < 0) {
	  rstr = xnstrdup("Error\nUnable to return infomation (1). See "
			  "server-side error logs\n");
	  elog_printf(ERROR, "unable to uname(). errno=%d %s",
		      errno, strerror(errno));

	  /* report badness */
	  *length_out = strlen(rstr);
	  *headers_out = NULL;
	  *modt_out = time(NULL);
	  return rstr;
     }

     /* create and assign records to table */
     tab = table_create_a(httpd_schema_nameval);
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, "name",  "Hostname");
     table_replacecurrentcell_alloc(tab, "value", uts.nodename);
#ifdef _GNU_SOURCE
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, "name",  "Domanname");
     table_replacecurrentcell_alloc(tab, "value", uts.domainname);
#endif
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, "name",  "Machine");
     table_replacecurrentcell_alloc(tab, "value", uts.machine);
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, "name",  "OS Name");
     table_replacecurrentcell_alloc(tab, "value", uts.sysname);
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, "name",  "OS Release");
     table_replacecurrentcell_alloc(tab, "value", uts.release);
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, "name",  "OS Version");
     table_replacecurrentcell_alloc(tab, "value", uts.version);

     /* get time zone info and assign records from there */
     tzset();
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, "name",  "Timezone");
     table_replacecurrentcell      (tab, "value", tzname[daylight]);
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, "name",  "GMT offset");
     table_replacecurrentcell_alloc(tab, "value",
					 util_i32toa(timezone));

     /* return */
     rstr = table_outtable(tab);
     table_destroy(tab);
     *length_out = strlen(rstr);
     *headers_out = NULL;
     *modt_out = time(NULL);
     return rstr;

}


/* access to local data
 * This is the consolidated view of short term memory stores and the 
 * consolidated on-disk ringstore
 * Path format is as for a standard route address, but with the 
 * file/host identifier removed (for security arbitary hosts may not 
 * be addressed): /local/ring,duration[,t=t1[-[t2]]][,s=s1[-[s2]]]
 * '/localtsv' ay be substuted for '/local', which is special measure to
 * product tab separated values without having '!tsv' appended as a formatter
 * Duration may be 'cons' for consolidated and all the meta operators
 * also work (?info, ?linfo, ?cinfo, ?lcinfo).
 * A leading comma (,) before ring in the path is silently ignored
 */
char *httpd_builtin_local(char *path, int match, int method, TREE *headers_in, 
			  char *data_in, int *length_out, TREE **headers_out, 
			  time_t *modt_out) {
     TABLE t;
     char *r, *rspath, *rspath_t;
     int rspathlen, tsv=0;

     /* take the partial ringstore address out of path.
      * its starts from the second slash (/) onwards and will be
      * the ring,duration and the optional parts */
     if ( ! (r = strchr(path, '/')) ) {
	  r = xnstrdup("Error\nUnable to find a valid address (1)\n");
	  goto local_prep_ret;
     }
     if ( ! (r = strchr(r+1, '/')) ) {
	  r = xnstrdup("Error\nUnable to find a valid address (2)\n");
	  goto local_prep_ret;
     }
     r++;
     if (*r == ',')		/* skip leading comma */
	  r++;
     while (*r == '/')		/* skip additional slashes */
	  r++;

     /* special behaviour for /localtsv */
     if (strncmp(path, "/localtsv", 9) == 0)
	  tsv++;

     /* work out the file to open */
     rspath    = util_strjoin("rs:", iiab_dir_var, "/%h.rs", 
			      *r=='?' ? "": ",", r, NULL);
     rspathlen = strlen(rspath);
     rspath_t  = xnmalloc(rspathlen+100);
     route_expand(rspath_t, rspath, "NOJOB", 0);

     /* while I want to give them the memrs, its not ready -- 
      * just give them the ringstore */
     elog_printf(DIAG, "asked to deliver: %s, sending %s", path, rspath_t);
     t = route_tread(rspath_t, NULL);
     if (!t) {
	  r = xnstrdup("Error\nUnable to open object\n");
	  goto local_prep_ret;
     }
     if ( tsv )
	  r = table_outtable(t);
     else
	  r = table_html(t, -1, -1, NULL);
     table_destroy(t);
     nfree(rspath_t);
     nfree(rspath);

local_prep_ret:
     *length_out = strlen(r);
     *headers_out = NULL;
     *modt_out = time(NULL);
     return r;
}


#if TEST

#include "sig.h"

int main(int argc, char **argv)
{
     iiab_start("", argc, argv, "", NULL);

     /* initialise job classes over those setup by iiab_start */
     sig_init();
     meth_init();
     httpd_init();
     httpd_addpath("/ping",     httpd_builtin_ping);
     httpd_addpath("/cf",       httpd_builtin_cf);
     httpd_addpath("/elog",     httpd_builtin_elog);
     httpd_addpath("/local/",   httpd_builtin_local);
     httpd_addpath("/localtsv/",httpd_builtin_local);
     httpd_start();

     printf("press ^C to end\n");

     /* run jobs */
     while(1) {
          elog_printf(DEBUG,  0, "relay returns %d", meth_relay());
	  /*runq_dump();*/
     }

     httpd_stop();
     iiab_stop();
}

#endif /* TEST */
