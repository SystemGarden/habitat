/*
 * Route driver for files
 *
 * Nigel Stuckey, July 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#ifndef _RT_FILE_H_
#define _RT_FILE_H_

#include <time.h>
#include "cf.h"
#include "route.h"

/* General definitions */
#define RT_FILEA_LLD_MAGIC  5592885
#define RT_FILEOV_LLD_MAGIC 9224581

typedef struct rt_file_desc {
     int magic;
     char *prefix;
     char *description;
     int fd;
     char *p_url;
     char *filepath;
} * RT_FILED;

extern const struct route_lowlevel rt_filea_method;
extern const struct route_lowlevel rt_fileov_method;

int    rt_filea_magic();
char * rt_filea_prefix();
char * rt_filea_description();
int    rt_fileov_magic();
char * rt_fileov_prefix();
char * rt_fileov_description();
void   rt_file_init  (CF_VALS cf, int debug);
void   rt_file_fini  ();
int    rt_file_access(char *p_url, char *password, char *basename, int flag);
RT_LLD rt_filea_open (char *p_url, char *comment, char *password, int keep,
		      char *basename);
RT_LLD rt_fileov_open(char *p_url, char *comment, char *password, int keep,
		      char *basename);
void   rt_file_close (RT_LLD lld);
int    rt_file_write (RT_LLD lld, const void *buf, int buflen);
int    rt_file_twrite(RT_LLD lld, TABLE tab);
int    rt_file_tell  (RT_LLD lld, int *seq, int *size, time_t *modt);
int    rt_file_stat  (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_file_read  (RT_LLD lld, int seq, int offset);
TABLE  rt_file_tread (RT_LLD lld, int seq, int offset);
void   rt_file_status(RT_LLD lld, char **status, char **info);

#endif /* _RT_FILE_H_ */
