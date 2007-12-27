/*
 * Program configuration class.
 * Scans the route sepcified for lines that match configuration patterns.
 *
 * Nigel Stuckey, December 1997
 * Major revisions July 1999
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _CF_H_
#define _CF_H_

#include <limits.h>
#include "tree.h"
#include "itree.h"
#include "table.h"

#define TOKLEN 128
#define LINELEN 1024
#define CF_UNDEF INT_MIN
#define CF_OVERWRITE 1
#define CF_CAPITULATE 0
#define CF_TEXTLINESIZE 512

/* entry structure */
struct cf_entval {
     int vector;
     union {
	  char *arg;
	  ITREE *vec;
     } data;
};
/* cf_valent is stored in a tree, but we abstract it anyway */
typedef TREE* CF_VALS;

CF_VALS cf_create();
void    cf_destroy(CF_VALS cf);
int     cf_scanroute(CF_VALS cf, char *magic, char *cfroute, int overwrite);
int     cf_scantext(CF_VALS cf, char *magic, char *cftext, int overwrite);
int     cf_scan(CF_VALS cf, ITREE *lol, int overwrite);
int     cf_cmd(CF_VALS cf, char *opts, int argc, char **argv, char *usage);
int     cf_getint(CF_VALS cf, char *key);
char *  cf_getstr(CF_VALS cf, char *key);
ITREE * cf_getvec(CF_VALS cf, char *key);
void    cf_putint(CF_VALS cf, char *key, int newval);
void    cf_putstr(CF_VALS cf, char *key, char *newval);
void    cf_putvec(CF_VALS cf, char *key, ITREE *newval);
int     cf_check(CF_VALS cf, char **key);
int     cf_default(CF_VALS cf, char **defaults);
int     cf_defaultcf(CF_VALS cf, CF_VALS defaults);
int     cf_defined(CF_VALS cf, char *key);
int     cf_isvector(TREE *cf, char *key);
void    cf_dump(CF_VALS cf);
TABLE   cf_getstatus(CF_VALS cf);
void    cf_addstr(CF_VALS cf, char *name, char *value);
void    cf_entreplace(TREE *t, char *key, struct cf_entval *);
void    cf_entfree(struct cf_entval *entry);
int     cf_directive(char *key, struct cf_entval *entry, char *buffer, 
		     int buflen);
#ifdef _ROUTE_DEFINED_
int     cf_writeroute(CF_VALS cf, char *magic, ROUTE route);
#else
int     cf_writeroute();
#endif
char *  cf_writetext(CF_VALS cf, char *magic);
int     cf_updateline(CF_VALS cf, char *key, char *cfroute, char *magic);

#endif /* _CF_H_ */
