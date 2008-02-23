/*
 * Route driver for sqlrs, a set of format conventions over http
 *
 * Nigel Stuckey, August July 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#ifndef _RT_SQLRS_H_
#define _RT_SQLRS_H_

#include <time.h>
#include "cf.h"
#include "route.h"

/* General definitions */
#define RT_SQLRS_LLD_MAGIC        503765
#define RT_SQLRS_GET_URLKEY        "route.sqlrs.geturl"
#define RT_SQLRS_PUT_URLKEY        "route.sqlrs.puturl"
#define RT_SQLRS_AUTH_URLKEY       "route.sqlrs.authurl"
#define RT_SQLRS_COOKIES_URLKEY    "route.sqlrs.cookieurl"
#define RT_SQLRS_COOKIEJAR_FILEKEY "route.sqlrs.cookiejar"
#define RT_SQLRS_WRITE_STATUS      "sqlrs:_WRITE_STATUS_"
#define RT_SQLRS_WRITE_RETURN      "sqlrs:_WRITE_RETURN_"

typedef struct rt_sqlrs_desc {
     int   magic;
     char *prefix;
     char *description;
     char *url;
     char *addr;
     char *puturl;
     char *geturl;
     char *ringdesc;
     char *posttext;
} * RT_SQLRSD;

extern const struct route_lowlevel rt_sqlrs_method;

int    rt_sqlrs_magic();
char * rt_sqlrs_prefix();
char * rt_sqlrs_description();
int    rt_sqlrs_magic();
char * rt_sqlrs_prefix();
char * rt_sqlrs_description();
void   rt_sqlrs_init  (CF_VALS cf, int debug);
void   rt_sqlrs_fini  ();
int    rt_sqlrs_access(char *p_url, char *password, char *basename, int flag);
RT_LLD rt_sqlrs_open  (char *p_url, char *comment, char *password, int keep,
		      char *basename);
RT_LLD rt_sqlrs_open  (char *p_url, char *comment, char *password, int keep,
		      char *basename);
void   rt_sqlrs_close (RT_LLD lld);
int    rt_sqlrs_write (RT_LLD lld, const void *buf, int buflen);
int    rt_sqlrs_twrite(RT_LLD lld, TABLE tab);
int    rt_sqlrs_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_sqlrs_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_sqlrs_tread (RT_LLD lld, int seq, int offset);


#endif /* _RT_SQLRS_H_ */
