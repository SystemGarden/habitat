/*
 * http client access using curl for the harvest application
 * Nigel Stuckey, June 2003.
 * Copyright System Garden Ltd 2003, All rights reserved
 */

#include "tree.h"
#include "table.h"

#define HTTP_COOKIESTRLEN 8192

/* buffer structure to interface with curl */
struct http_buffer {
  char *memory;
  size_t size;
};


void  http_init();
void  http_fini();
char *http_get(char *url, TREE *cookies, char *cookiejar, TABLE auth, 
	       int flags);
char *http_post(char *url, TREE *form, TREE *files, TREE *parts, 
		TREE *cookies, char *cookiejar, TABLE auth, int flags);

#define HTTP_CFNAME               "http."
#define HTTP_CF_DNS_CACHE_TIMEOUT HTTP_CFNAME "dnscache_timout"
#define HTTP_CF_CONNECT_TIMEOUT   HTTP_CFNAME "connect_timeout"
#define HTTP_DNS_CACHE_TIMEOUT    3600
#define HTTP_CONNECT_TIMEOUT      15
#define HTTP_CF_PROXY_CONNECT_TIMEOUT HTTP_CFNAME "proxy_connect_timeout"
#define HTTP_CF_PROXY_TIMEOUT         HTTP_CFNAME "proxy_timeout"
#define HTTP_PROXY_CONNECT_TIMEOUT    15
#define HTTP_PROXY_TIMEOUT            60
#define HTTP_CF_NONPROXY_CONNECT_TIMEOUT HTTP_CFNAME "nonproxy_connect_timeout"
#define HTTP_CF_NONPROXY_TIMEOUT         HTTP_CFNAME "nonproxy_timeout"
#define HTTP_NONPROXY_CONNECT_TIMEOUT    8
#define HTTP_NONPROXY_TIMEOUT            60
