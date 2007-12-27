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
char *http_get(char *url, TREE *cookies, TABLE auth, int flags);
char *http_post(char *url, TREE *form, TREE *files, TREE *parts, 
		TREE *cookies, TABLE auth, int flags);
