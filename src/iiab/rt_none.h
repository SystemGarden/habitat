/*
 * Route driver for null channels, all output discarded
 *
 * Nigel Stuckey, February 2004
 * Copyright System Garden Ltd 2004. All rights reserved
 */

#ifndef _RT_NONE_H_
#define _RT_NONE_H_

#include <time.h>
#include "cf.h"
#include "route.h"

/* General definitions */
#define RT_NONE_LLD_MAGIC  887766

extern const struct route_lowlevel rt_none_method;

int    rt_none_magic();
char * rt_none_prefix();
char * rt_none_description();
void   rt_none_init   (CF_VALS cf, int debug);
void   rt_none_fini   ();
int    rt_none_access (char *p_url, char *password, char *basename, int flag);
RT_LLD rt_none_open (char *p_url, char *comment, char *password, int keep,
		     char *basename);
void   rt_none_close  (RT_LLD lld);
int    rt_none_write  (RT_LLD lld, const void *buf, int buflen);
int    rt_none_twrite (RT_LLD lld, TABLE tab);
int    rt_none_tell   (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_none_read   (RT_LLD lld, int seq, int offset);
TABLE  rt_none_tread  (RT_LLD lld, int seq, int offset);


#endif /* _RT_NONE_H_ */
