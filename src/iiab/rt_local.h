/*
 * Route driver for local, current implemented with http
 *
 * Nigel Stuckey, July 2011
 * Copyright System Garden Ltd 2011. All rights reserved
 */

#ifndef _RT_LOCAL_H_
#define _RT_LOCAL_H_

#include <time.h>
#include "cf.h"
#include "route.h"
#include "http.h"

/* General definitions */
#define RT_LOCAL_LLD_MAGIC   672049
#define RT_LOCAL_PREFIX      "local"
#define RT_LOCAL_DESCRIPTION "Local host data from clockwork"
#define RT_LOCALMETA_LLD_MAGIC   676051
#define RT_LOCALMETA_PREFIX      "localmeta"
#define RT_LOCALMETA_DESCRIPTION "Meta information from local clockwork instance"

typedef struct rt_local_desc {
     int magic;
     char *prefix;
     char *description;
     char *url;
     char *basepurl;
     RT_HTTPD hrt;
} * RT_LOCALD;

extern const struct route_lowlevel rt_local_method;
extern const struct route_lowlevel rt_localmeta_method;

int    rt_local_magic();
int    rt_localmeta_magic();
char * rt_local_prefix();
char * rt_localmeta_prefix();
char * rt_local_description();
char * rt_localmeta_description();
int    rt_local_magic();
char * rt_local_prefix();
char * rt_local_description();
void   rt_local_init  (CF_VALS cf, int debug);
void   rt_local_fini  ();
int    rt_local_access(char *p_url, char *password, char *basename, int flag);
RT_LLD rt_local_open  (char *p_url, char *comment, char *password, int keep,
		      char *basename);
RT_LLD rt_localmeta_open(char *p_url, char *comment, char *password, int keep,
			 char *basename);
void   rt_local_close (RT_LLD lld);
int    rt_local_write (RT_LLD lld, const void *buf, int buflen);
int    rt_local_twrite(RT_LLD lld, TABLE tab);
int    rt_local_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_local_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_local_tread (RT_LLD lld, int seq, int offset);
void   rt_local_status(RT_LLD lld, char **status, char **info);
int    rt_local_checkpoint(RT_LLD lld);

#endif /* _RT_LOCAL_H_ */
