/*
 * Route driver for standard input, output and error
 *
 * Nigel Stuckey, July 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "nmalloc.h"
#include "cf.h"
#include "elog.h"
#include "util.h"
#include "rt_std.h"

/* private fiunctional prototypes */
RT_STDD rt_std_from_lld(RT_LLD lld);

const struct route_lowlevel rt_stdin_method = {
     rt_stdin_magic,    rt_stdin_prefix,   rt_stdin_description,
     rt_std_init,       rt_std_fini,       rt_std_access,
     rt_stdin_open,     rt_std_close,      rt_std_write,
     rt_std_twrite,     rt_std_tell,       rt_std_read,
     rt_std_tread 
};

const struct route_lowlevel rt_stdout_method = {
     rt_stdout_magic,   rt_stdout_prefix,  rt_stdout_description,
     rt_std_init,       rt_std_fini,       rt_std_access,
     rt_stdout_open,    rt_std_close,      rt_std_write,
     rt_std_twrite,     rt_std_tell,       rt_std_read,
     rt_std_tread 
};

const struct route_lowlevel rt_stderr_method = {
     rt_stderr_magic,   rt_stderr_prefix,  rt_stderr_description,
     rt_std_init,       rt_std_fini,       rt_std_access,
     rt_stderr_open,    rt_std_close,      rt_std_write,
     rt_std_twrite,     rt_std_tell,       rt_std_read,
     rt_std_tread 
};

char *rt_std_tabschema[] = {"data", "_time", NULL};


int    rt_stdin_magic() { return RT_STDIN_LLD_MAGIC; }
char * rt_stdin_prefix() { return "stdin"; }
char * rt_stdin_description() { return "standard input"; }

int    rt_stdout_magic() { return RT_STDOUT_LLD_MAGIC; }
char * rt_stdout_prefix() { return "stdout"; }
char * rt_stdout_description() { return "standard output"; }

int    rt_stderr_magic() { return RT_STDERR_LLD_MAGIC; }
char * rt_stderr_prefix() { return "stderr"; }
char * rt_stderr_description() { return "standard error"; }

void   rt_std_init  (CF_VALS cf, int debug) {}

void   rt_std_fini  () {}

/* Check accessability of stdin, out or err. Always 1 (success) */
int    rt_std_access(char *p_url, char *password, char *basename, int flag)
{
     return 1;
}

/* open stdin */
RT_LLD rt_stdin_open (char *p_url, char *comment, char *password, int keep,
		      char *basename)
{
     RT_STDD rt;

     rt = nmalloc(sizeof(struct rt_std_desc));
     rt->magic = rt_stdin_magic();
     rt->prefix = rt_stdin_prefix();
     rt->description = rt_stdin_description();

     return rt;
}

/* open stdout */
RT_LLD rt_stdout_open (char *p_url, char *comment, char *password, int keep,
		       char *basename)
{
     RT_STDD rt;

     rt = nmalloc(sizeof(struct rt_std_desc));
     rt->magic = rt_stdout_magic();
     rt->prefix = rt_stdout_prefix();
     rt->description = rt_stdout_description();

     return rt;
}

/* open stdin */
RT_LLD rt_stderr_open (char *p_url, char *comment, char *password, int keep,
		       char *basename)
{
     RT_STDD rt;

     rt = nmalloc(sizeof(struct rt_std_desc));
     rt->magic = rt_stderr_magic();
     rt->prefix = rt_stderr_prefix();
     rt->description = rt_stderr_description();

     return rt;
}

void   rt_std_close (RT_LLD lld)
{
     RT_STDD rt;

     rt = rt_std_from_lld(lld);

     rt->magic = 0;	/* don't use again */
     nfree(rt);
}

int    rt_std_write (RT_LLD lld, const void *buf, int buflen)
{
     int n;
     RT_STDD rt;

     rt = rt_std_from_lld(lld);

     if (rt->magic == RT_STDERR_LLD_MAGIC)
	  n = write(2, buf, buflen);	/* stderr */
     else
	  n = write(1, buf, buflen);	/* stdout all other cases */

     if (n == -1)
	  elog_printf(ERROR, "write() system call returns -1: %s",
		      strerror(errno));
     return n;
}

int    rt_std_twrite (RT_LLD lld, TABLE tab)
{
     int n;
     RT_STDD rt;
     char *buf;

     rt = rt_std_from_lld(lld);

     buf = table_outtable(tab);
     if ( ! buf)
       return 1;	/* empty table, successfully don't writing anything */

     n = rt_std_write(lld, buf, strlen(buf));
     nfree(buf);

     if (n == -1)
	  return 0;
     else
	  return 1;
}


/* report the current location of reading. always 0 with stdin */
int    rt_std_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     return 0;
}

/* read from stdin, regardless of wether the route was opened for stdout 
 * or std err. Sequence and offset are ignored; data is returned in
 * a list of ROUTE_BUF; alternatively NULL is returned for failure */
ITREE *rt_std_read  (RT_LLD lld, int seq, int offset)
{
     ROUTE_BUF *storebuf;
     char buffer[RT_MAXBUF];
     int r;
     ITREE *buflist;

     r = read(0, buffer, RT_MAXBUF);
     if (r == -1) {
	  elog_printf(ERROR, "unable to read %d bytes from stdin",
		      RT_MAXBUF);
	  return NULL;
     }

     /* create the list */
     storebuf = xnmalloc(sizeof(ROUTE_BUF));
     storebuf->buffer = xnmemdup(buffer, r+1);
     storebuf->buflen = r;
     buflist = itree_create();
     itree_append(buflist, storebuf);

     return buflist;
}


/* read from stdin, regardless of wether the route was opened for stdout 
 * or std err. Sequence and offset are ignored; data is returned in
 * table if successful (columns called 'data' and '_time') or NULL
 * if there is a failure
 */
TABLE rt_std_tread  (RT_LLD lld, int seq, int offset)
{
     char buffer[RT_MAXBUF], *buf;
     int r;
     TABLE tab;

     r = read(0, buffer, RT_MAXBUF);
     if (r == -1) {
	  elog_printf(ERROR, "unable to read %d bytes from stdin",
		      RT_MAXBUF);
	  return NULL;
     }
     buf = xnmalloc(r+1);
     memcpy(buf, buffer, r);

     /* create the list */
     tab = table_create_a(rt_std_tabschema);
     table_addemptyrow(tab);
     table_replacecurrentcell(tab, "data", buf);
     table_replacecurrentcell(tab, "_time", util_i32toa(time(NULL)));
     table_freeondestroy(tab, buf);

     return tab;
}


/* --------------- Private routines ----------------- */


RT_STDD rt_std_from_lld(RT_LLD lld	/* typeless low level data */)
{
     if (!lld)
	  elog_die(FATAL, "passed NULL low level descriptor");
     if ( (((RT_STDD)lld)->magic != RT_STDIN_LLD_MAGIC) &&
	  (((RT_STDD)lld)->magic != RT_STDOUT_LLD_MAGIC) &&
	  (((RT_STDD)lld)->magic != RT_STDERR_LLD_MAGIC) )
	  elog_die(FATAL, "magic type mismatch: we were given "
		   "%s (%s) but can only handle either %s (%s) or %s (%s)"
		   "of %s (%s)", 
		   ((RT_STDD)lld)->prefix, 
		   ((RT_STDD)lld)->description,
		   rt_stdin_prefix(),  rt_stdin_description(),
		   rt_stdout_prefix(), rt_stdout_description(),
		   rt_stderr_prefix(), rt_stderr_description());

     return (RT_STDD) lld;
}


#if TEST

#define TURL1 "stdin:"
#define TURL2 "stdout:"
#define TURL3 "stderr:"

int main(int argc, char **argv) {
     int r, seq1, size1;
     CF_VALS cf;
     RT_LLD lld1;
     time_t time1;
     ITREE *chain;
     ROUTE_BUF *rtbuf;

     cf = cf_create();
     rt_std_init(cf, 1);

     /* test 1: open stdin and check the non-supported calls do nothing */
     lld1 = rt_stdin_open(TURL1, "blah", NULL, 10, "");
     if (!lld1)
	  elog_die(FATAL, "[1] stdin wasn't opened");
     if (!rt_std_access(TURL1, "", "", 0))
	  elog_die(FATAL, "[1] rt_std_access() failed");
     r = rt_std_tell(lld1, &seq1, &size1, &time1);
     if (r != 0)
	  elog_die(FATAL, "[1] rt_std_tell() didn't return 0 (%d)", r);
     rt_std_close(lld1);

     /* test 2: write some data to stdin (should come out on stdout) */
     lld1 = rt_stdin_open(TURL1, "blah", NULL, 0, "");
     if (!lld1)
	  elog_die(FATAL, "[2] can't open stdin");
     r = rt_std_write(lld1, "tom, dick and harry\n", 20);
     if (r != 20)
	  elog_die(FATAL, "[2] wrote %d chars instead of 20", r);
     rt_std_close(lld1);

     /* test 3: write some data to stdout */
     lld1 = rt_stdout_open(TURL2, "blah", NULL, 0, "");
     if (!lld1)
	  elog_die(FATAL, "[3] can't open stdout");
     r = rt_std_write(lld1, "tom, dick and harry\n", 20);
     if (r != 20)
	  elog_die(FATAL, "[3] wrote %d chars instead of 20", r);
     rt_std_close(lld1);

     /* test 4: write some data to stderr */
     lld1 = rt_stderr_open(TURL3, "blah", NULL, 0, "");
     if (!lld1)
	  elog_die(FATAL, "[4] can't open stderr");
     r = rt_std_write(lld1, "tom, dick and harry\n", 20);
     if (r != 20)
	  elog_die(FATAL, "[4] wrote %d chars instead of 20", r);
     rt_std_close(lld1);

     /* test 5: read the data back */
     lld1 = rt_stdin_open(TURL1, "blah", NULL, 0, "");
     if (!lld1)
	  elog_die(FATAL, "[5] can't open stdin");
     r = rt_std_write(lld1, "type 'fred', then hit <return>\n", 31);
     if (r != 31)
	  elog_die(FATAL, "[5] wrote %d chars instead of 31", r);
     chain = rt_std_read(lld1, 0, 0);
     if (itree_n(chain) != 1)
	  elog_die(FATAL, "[5] wrong number of buffers: %d", 
		   itree_n(chain));
     itree_first(chain);
     rtbuf = itree_get(chain);
     if (!rtbuf)
	  elog_die(FATAL, "[5] no buffer");
     if (rtbuf->buflen != 5)
	  elog_die(FATAL, "[5] buffer length is %d not 5", 
		   rtbuf->buflen);
     if (!rtbuf->buffer)
	  elog_die(FATAL, "[5] NULL buffer");
     if (strncmp(rtbuf->buffer, "fred\n", 5))
	  elog_die(FATAL, "[5] buffer is different: %s", rtbuf->buffer);
     route_free_routebuf(chain);
     rt_std_close(lld1);

     cf_destroy(cf);
     rt_std_fini();
     return 0;
}

#endif /* TEST */
