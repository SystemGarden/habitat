/*
 * Route driver for *store classes: holstore, timestore, tablestore 
 * and version store
 *
 * Nigel Stuckey, July 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#ifndef _RT_STORE_H_
#define _RT_STORE_H_

#include <time.h>
#include "cf.h"
#include "holstore.h"
#include "timestore.h"
#include "tablestore.h"
#include "versionstore.h"
#include "route.h"

/* General definitions */
#define RT_STOREHOL_LLD_MAGIC 8941952
#define RT_STORETIME_LLD_MAGIC 8941953
#define RT_STORETAB_LLD_MAGIC 8941954
#define RT_STOREVER_LLD_MAGIC 8941955

typedef struct rt_store_desc {
     int magic;
     char *prefix;
     char *description;
     char *p_url;
     char *filepath;	/* file name and path */
     char *object;	/* storage specific address */
     char *password;	/* password, if any */
     HOLD     hol_id;	/* holstore id */
     TS_RING  ts_id;	/* timestore id */
     TAB_RING tab_id;	/* tablestore id */
     VS       vs_id;	/* versionstore id */
} * RT_STORED;

extern const struct route_lowlevel rt_storehol_method;
extern const struct route_lowlevel rt_storetime_method;
extern const struct route_lowlevel rt_storetab_method;
extern const struct route_lowlevel rt_storever_method;

int    rt_storehol_magic ();
char * rt_storehol_prefix();
char * rt_storehol_description();
void   rt_storehol_init  (CF_VALS cf, int debug);
void   rt_storehol_fini  ();
int    rt_storehol_access(char *p_url, char *password, char *basename, 
			  int flag);
RT_LLD rt_storehol_open  (char *p_url, char *comment, char *password, int keep,
			  char *basename);
void   rt_storehol_close (RT_LLD lld);
int    rt_storehol_write (RT_LLD lld, const void *buf, int buflen);
int    rt_storehol_twrite(RT_LLD lld, TABLE tab);
int    rt_storehol_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_storehol_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_storehol_tread (RT_LLD lld, int seq, int offset);

int    rt_storetime_magic ();
char * rt_storetime_prefix();
char * rt_storetime_description();
void   rt_storetime_init  (CF_VALS cf, int debug);
void   rt_storetime_fini  ();
int    rt_storetime_access(char *p_url, char *password, char *basename, 
			   int flag);
RT_LLD rt_storetime_open  (char *p_url, char *comment, char *password,int keep,
			   char *basename);
void   rt_storetime_close (RT_LLD lld);
int    rt_storetime_write (RT_LLD lld, const void *buf, int buflen);
int    rt_storetime_twrite(RT_LLD lld, TABLE tab);
int    rt_storetime_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_storetime_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_storetime_tread (RT_LLD lld, int seq, int offset);

int    rt_storetab_magic ();
char * rt_storetab_prefix();
char * rt_storetab_description();
void   rt_storetab_init  (CF_VALS cf, int debug);
void   rt_storetab_fini  ();
int    rt_storetab_access(char *p_url, char *password, char *basename, 
			  int flag);
RT_LLD rt_storetab_open  (char *p_url, char *comment, char *password, int keep,
			  char *basename);
void   rt_storetab_close (RT_LLD lld);
int    rt_storetab_write (RT_LLD lld, const void *buf, int buflen);
int    rt_storetab_twrite(RT_LLD lld, TABLE tab);
int    rt_storetab_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_storetab_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_storetab_tread (RT_LLD lld, int seq, int offset);

int    rt_storever_magic ();
char * rt_storever_prefix();
char * rt_storever_description();
void   rt_storever_init  (CF_VALS cf, int debug);
void   rt_storever_fini  ();
int    rt_storever_access(char *p_url, char *password, char *basename, 
			  int flag);
RT_LLD rt_storever_open  (char *p_url, char *comment, char *password, int keep,
			  char *bvername);
void   rt_storever_close (RT_LLD lld);
int    rt_storever_write (RT_LLD lld, const void *buf, int buflen);
int    rt_storever_twrite(RT_LLD lld, TABLE tab);
int    rt_storever_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_storever_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_storever_tread (RT_LLD lld, int seq, int offset);

#endif /* _RT_STORE_H_ */
