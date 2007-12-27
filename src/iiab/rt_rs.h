/*
 * Route driver for ringstore
 *
 * Nigel Stuckey, Oct 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#ifndef _RT_RS_H_
#define _RT_RS_H_

#include <time.h>
#include "cf.h"
#include "rs.h"
#include "route.h"

/* General definitions */
#define RT_RS_LLD_MAGIC 3877164

enum rt_rs_meta {rt_rs_none, rt_rs_info, rt_rs_linfo, rt_rs_cinfo, 
		 rt_rs_clinfo};

typedef struct rt_rs_desc {
     int    magic;
     char * prefix;
     char * description;
     char * p_url;
     char * filepath;	/* file name and path */
     char * ring;	/* storage specific address */
     char * password;	/* password, if any */
     int    duration;	/* duration */
     time_t from_t;	/* optional time bounds */
     time_t to_t;
     long   from_s;	/* optional sequence bounds */
     long   to_s;
     RS     rs_id;	/* ringstore id */
     enum   rt_rs_meta meta; /* special meta commands */
     int    cons;	/* consolidation flag */
} * RT_RSD;

extern const struct route_lowlevel rt_rs_method;

int    rt_rs_magic ();
char * rt_rs_prefix();
char * rt_rs_description();
void   rt_rs_init  (CF_VALS cf, int debug);
void   rt_rs_fini  ();
int    rt_rs_access(char *p_url, char *password, char *basename, int flag);
RT_LLD rt_rs_open  (char *p_url, char *comment, char *password, int keep,
		    char *basename);
void   rt_rs_close (RT_LLD lld);
int    rt_rs_write (RT_LLD lld, const void *buf, int buflen);
int    rt_rs_twrite(RT_LLD lld, TABLE tab);
int    rt_rs_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
int    rt_rs_stat  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_rs_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_rs_tread (RT_LLD lld, int seq, int offset);


#endif /* _RT_RS_H_ */
