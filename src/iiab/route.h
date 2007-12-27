/*
 * IO routing
 * New implementation, July 2003
 *
 * Copyright System Garden Ltd 2003. All rights reserved.
 */

#ifndef _ROUTE_H_
#define _ROUTE_H_

#include <time.h>
#include "cf.h"
#include "itree.h"
#include "table.h"

/* General definitions */
#define ROUTE_BUFSZ PIPE_BUF
#define ROUTE_PURLLEN 1024
#define ROUTE_HOSTNAMELEN 32
#define ROUTE_READOK 1
#define ROUTE_WRITEOK 2

/* ----- public structures ----- */

/* low level descriptor */
typedef void * RT_LLD;

/* Call vectors to low level route implementations */
typedef const struct route_lowlevel {
     int    (*ll_magic)();
     char * (*ll_prefix)();
     char * (*ll_description)();
     void   (*ll_init)  (CF_VALS cf, int debug);
     void   (*ll_fini)  ();
     int    (*ll_access)(char *p_url, char *password,char *basename,int flag);
     RT_LLD (*ll_open)  (char *p_url, char *comment, char *password, int keep,
			 char *basename);
     void   (*ll_close) (RT_LLD lld);
     int    (*ll_write) (RT_LLD lld, const void *buf, int buflen);
     int    (*ll_twrite)(RT_LLD lld, TABLE tab);
     int    (*ll_tell)  (RT_LLD lld, int *seq, int *size, time_t *modt);
     ITREE *(*ll_read)  (RT_LLD lld, int  seq, int  offset);
     TABLE  (*ll_tread) (RT_LLD lld, int  seq, int  offset);
} *ROUTE_METHOD;

/* buffer structure */
typedef struct route_buffer {
     int buflen;	/* Length of buffer */
     char *buffer;	/* Buffer */
} ROUTE_BUF;

/*
 * The following structure describes an open route and its address
 * is used as a file handle
 */
typedef struct route_handle {
     char *p_url;			/* full pseudo-url */
     ROUTE_METHOD method;		/* method function vectors */
     RT_LLD handle;			/* implementation instance struct */
     ROUTE_BUF unsent;			/* Uncommitted message buffer */
} *ROUTE;
#define _ROUTE_DEFINED_

/* Functional prototypes */
void   route_init    (CF_VALS cf, int debug);
void   route_fini    ();
void   route_register(ROUTE_METHOD meth);
int    route_unregister(char *prefix);
TREE * route_registered();
int    route_access  (char *p_url, char *password, int flags);
ROUTE  route_open    (char *p_url, char *comment, char *password, int keep);
ROUTE  route_open_t  (char *p_url, char *comment, char *password, int keep, 
		      char *jobname, int duration);
int    route_expand  (char *dst, char *src, char *jobname, int duration);
int    route_flush   (ROUTE rt);
char * route_buffer  (ROUTE rt, int *buflen);
void   route_killbuffer  (ROUTE rt, int freealloc);
void   route_close   (ROUTE rt);
int    route_write   (ROUTE rt, const void *buf, int buflen);
int    route_twrite  (ROUTE rt, TABLE tab);
int    route_printf  (ROUTE rt, const char *format, ...);
void   route_die     (ROUTE rt, const char *format, ...);
char * route_read    (char *p_url, char *password, int *length);
TABLE  route_tread   (char *p_url, char *password);
int    route_tell    (ROUTE rt,    int *seq, int *size, time_t *modt);
int    route_stat    (char *p_url, char *password, int *seq, int *size, 
		      time_t *modt);
ITREE *route_seekread(ROUTE rt, int seq, int offset);
TABLE  route_seektread(ROUTE rt, int seq, int offset);
void   route_free_routebuf(ITREE *buflist);
char * route_getpurl (ROUTE rt);

#endif /* _ROUTE_H_ */
