/*
 * Route driver for standard predefined channels, stdin, stdout and strerr
 *
 * Nigel Stuckey, July 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#ifndef _RT_STD_H_
#define _RT_STD_H_

#include <time.h>
#include "cf.h"
#include "route.h"

/* General definitions */
#define RT_STDIN_LLD_MAGIC  4299644
#define RT_STDOUT_LLD_MAGIC 7822399
#define RT_STDERR_LLD_MAGIC 1053976
#define RT_MAXBUF 4096

typedef struct rt_std_desc {
     int magic;
     char *prefix;
     char *description;
     int fd;
} * RT_STDD;

extern const struct route_lowlevel rt_stdin_method;
extern const struct route_lowlevel rt_stdout_method;
extern const struct route_lowlevel rt_stderr_method;

int    rt_stdin_magic();
char * rt_stdin_prefix();
char * rt_stdin_description();
int    rt_stdout_magic();
char * rt_stdout_prefix();
char * rt_stdout_description();
int    rt_stderr_magic();
char * rt_stderr_prefix();
char * rt_stderr_description();
void   rt_std_init   (CF_VALS cf, int debug);
void   rt_std_fini   ();
int    rt_std_access (char *p_url, char *password, char *basename, int flag);
RT_LLD rt_stdin_open (char *p_url, char *comment, char *password, int keep,
		     char *basename);
RT_LLD rt_stdout_open(char *p_url, char *comment, char *password, int keep,
		     char *basename);
RT_LLD rt_stderr_open(char *p_url, char *comment, char *password, int keep,
		     char *basename);
void   rt_std_close  (RT_LLD lld);
int    rt_std_write  (RT_LLD lld, const void *buf, int buflen);
int    rt_std_twrite (RT_LLD lld, TABLE tab);
int    rt_std_tell   (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_std_read   (RT_LLD lld, int seq, int offset);
TABLE  rt_std_tread  (RT_LLD lld, int seq, int offset);


#endif /* _RT_STD_H_ */
