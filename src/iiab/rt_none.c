/*
 * Route driver for null channels, all output discarded
 *
 * Nigel Stuckey, February 2004
 * Copyright System Garden Ltd 2004. All rights reserved
 */

#include "cf.h"
#include "rt_none.h"

const struct route_lowlevel rt_none_method = {
     rt_none_magic,    rt_none_prefix,   rt_none_description,
     rt_none_init,     rt_none_fini,     rt_none_access,
     rt_none_open,     rt_none_close,    rt_none_write,
     rt_none_twrite,   rt_none_tell,     rt_none_read,
     rt_none_tread,    rt_none_status
};

int    rt_none_magic() { return RT_NONE_LLD_MAGIC; }
char * rt_none_prefix() { return "none"; }
char * rt_none_description() { return "null channel"; }
void   rt_none_init  (CF_VALS cf, int debug) {}
void   rt_none_fini  () {}
int    rt_none_access(char *p_url, char *password, char *basename, int flag)
                     {return 1;}
RT_LLD rt_none_open (char *p_url, char *comment, char *password, int keep,
		     char *basename)  {return (RT_LLD) 1;}
void   rt_none_close (RT_LLD lld) {}
int    rt_none_write (RT_LLD lld, const void *buf, int buflen) {return buflen;}
int    rt_none_twrite(RT_LLD lld, TABLE tab) {return 1;}
int    rt_none_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
                     {return 0;}
ITREE *rt_none_read  (RT_LLD lld, int seq, int offset) {return NULL;}
TABLE  rt_none_tread (RT_LLD lld, int seq, int offset) {return NULL;}
void   rt_none_status(RT_LLD lld, char **status, char **info) {
     if (status)
          *status = NULL;
     if (info)
          *info = NULL;
}



#if TEST

#endif /* TEST */
