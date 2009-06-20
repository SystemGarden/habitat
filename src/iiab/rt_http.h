/*
 * Route driver for http using curl
 *
 * Nigel Stuckey, July 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#ifndef _RT_HTTP_H_
#define _RT_HTTP_H_

#include <time.h>
#include "cf.h"
#include "route.h"

/* General definitions */
#define RT_HTTP_LLD_MAGIC  998544
#define RT_HTTPS_LLD_MAGIC 998545

typedef struct rt_http_desc {
     int magic;
     char *prefix;
     char *description;
     char *url;
} * RT_HTTPD;

extern const struct route_lowlevel rt_http_method;
extern const struct route_lowlevel rt_https_method;

int    rt_http_magic();
char * rt_http_prefix();
char * rt_http_description();
int    rt_https_magic();
char * rt_https_prefix();
char * rt_https_description();
int    rt_http_magic();
char * rt_http_prefix();
char * rt_http_description();
void   rt_http_init  (CF_VALS cf, int debug);
void   rt_http_fini  ();
int    rt_http_access(char *p_url, char *password, char *basename, int flag);
RT_LLD rt_http_open  (char *p_url, char *comment, char *password, int keep,
		      char *basename);
RT_LLD rt_http_open  (char *p_url, char *comment, char *password, int keep,
		      char *basename);
void   rt_http_close (RT_LLD lld);
int    rt_http_write (RT_LLD lld, const void *buf, int buflen);
int    rt_http_twrite(RT_LLD lld, TABLE tab);
int    rt_http_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_http_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_http_tread (RT_LLD lld, int seq, int offset);
void   rt_http_status(RT_LLD lld, char **status, char **info);

#endif /* _RT_HTTP_H_ */
