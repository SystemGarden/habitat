/*
 * HTTP class contains the server code necessary for to serve http
 * connections. All the code is single threaded and uses meth to handle
 * the central I/O select() statement.
 * Based on mini_httpd.c from Jef Poskanzer (jef@acme.com)
 *
 * Nigel Stuckey, January-February 2001
 * Copyright System Garden Limited 2001. All rights reserved.
 */

#ifndef _HTTPD_H_
#define _HTTPD_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "tree.h"

/* A multi-family sockaddr. */
typedef union {
     struct sockaddr sa;
     struct sockaddr_in sa_in;
#ifdef HAVE_SOCKADDR_IN6
     struct sockaddr_in6 sa_in6;
#endif /* HAVE_SOCKADDR_IN6 */
#ifdef HAVE_SOCKADDR_STORAGE
     struct sockaddr_storage sa_stor;
#endif /* HAVE_SOCKADDR_STORAGE */
} httpd_usockaddr;

void httpd_init();
void httpd_fini();
void httpd_addpath(char *path, char * (*cb)(char *, int, int, TREE *, char *,
					    int *, TREE **, time_t *));
void httpd_rmpath(char *path);
int  httpd_start();
void httpd_stop();
void httpd_accept(void *fd);
int  httpd_request_scan(int fd, char *request, char **path, TREE **headers);
void httpd_response_evaluate(int fd, int method, char *path, TREE *headers, 
			     char *data);
char *httpd_status_title(int status);
void httpd_error_send(int fd, int errnum, char *text);
int  httpd_header_send(int fd, TREE *headers, int user_status, char *mime_type, 
		       int content_length, time_t last_modified);
int  httpd_lookup_if(httpd_usockaddr* usa4P, size_t sa4_len, int* gotv4P,
		     httpd_usockaddr* usa6P, size_t sa6_len, int* gotv6P);
size_t httpd_sockaddr_len(httpd_usockaddr* usaP);
int    httpd_listen( httpd_usockaddr* usaP );
char *httpd_builtin_ping(char *path, int match, int method, TREE *headers_in, 
			 char *data_in, int *length_out, TREE **headers_out, 
			 time_t *modt_out);
char *httpd_builtin_cf(char *path, int match, int method, TREE *headers_in, 
		       char *data_in, int *length_out, TREE **headers_out, 
		       time_t *modt_out);
char *httpd_builtin_elog(char *path, int match, int method, TREE *headers_in, 
			 char *data_in, int *length_out, TREE **headers_out, 
			 time_t *modt_out);
char *httpd_builtin_info(char *path, int match, int method, TREE *headers_in, 
			 char *data_in, int *length_out, TREE **headers_out, 
			 time_t *modt_out);
char *httpd_builtin_local(char *path, int match, int method, TREE *headers_in, 
			 char *data_in, int *length_out, TREE **headers_out, 
			 time_t *modt_out);

#define HTTPD_CF_DISABLE "httpd.disable"
#define HTTPD_CF_INTERFACE "httpd.interface"
#define HTTPD_CF_PORT "httpd.port"
#define HTTPD_PORT_HTTP 8096
#define HTTPD_PORT_HTTPS 8097
#define HTTPD_CB_ACCEPT "httpd_server_accept"
#define HTTPD_METHOD_FAIL -1
#define HTTPD_METHOD_GET   1
#define HTTPD_METHOD_POST  2
#define HTTPD_METHOD_HEAD  3
#define HTTPD_SOFTWARE "habitat"
#define HTTPD_URL "http://www.systemgarden.com"

#endif /* _HTTPD_H_ */
