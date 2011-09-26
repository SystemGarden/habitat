/*
 * Route driver for Ringstores
 *
 * Nigel Stuckey, April 2011, Oct 2003
 * Copyright System Garden Ltd 2011, 2003. All rights reserved
 */


#ifndef _RT_RS_H_
#define _RT_RS_H_

#include <time.h>
#include "cf.h"
#include "rs.h"
#include "route.h"

/* Identity definitions */
#define RT_RS_GDBM_LLD_MAGIC   3877164
#define RT_RS_GDBM_PREFIX      "grs"
#define RT_RS_GDBM_DESCRIPTION "GDBM Ringstore"
#define RT_RS_BERK_LLD_MAGIC   7887134
#define RT_RS_BERK_PREFIX      "brs"
#define RT_RS_BERK_DESCRIPTION "Berkeley DB Ringstore"


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

extern const struct route_lowlevel rt_grs_method;
/*extern const struct route_lowlevel rt_brs_method;*/

void   rt_rs_init  (CF_VALS cf, int debug);
void   rt_rs_fini  ();

int    rt_grs_magic ();
char * rt_grs_prefix();
char * rt_grs_description();
int    rt_brs_magic ();
char * rt_brs_prefix();
char * rt_brs_description();

int    rt_grs_access(char *p_url, char *password, char *basename, int flag);
int    rt_brs_access(char *p_url, char *password, char *basename, int flag);
RT_LLD rt_grs_open  (char *p_url, char *comment, char *password, int keep,
		     char *basename);
RT_LLD rt_brs_open  (char *p_url, char *comment, char *password, int keep,
		     char *basename);

void   rt_rs_close (RT_LLD lld);
int    rt_rs_write (RT_LLD lld, const void *buf, int buflen);
int    rt_rs_twrite(RT_LLD lld, TABLE tab);
int    rt_rs_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
int    rt_rs_stat  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_rs_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_grs_tread(RT_LLD lld, int seq, int offset);
TABLE  rt_brs_tread(RT_LLD lld, int seq, int offset);
void   rt_rs_status(RT_LLD lld, char **status, char **info);
int    rt_rs_checkpoint(RT_LLD lld);

#endif /* _RT_RS_H_ */
