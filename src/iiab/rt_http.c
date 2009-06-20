/*
 * Route driver for http client using curl
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
#include "http.h"
#include "rt_http.h"

/* private functional prototypes */
RT_HTTPD rt_http_from_lld(RT_LLD lld);

const struct route_lowlevel rt_http_method = {
     rt_http_magic,      rt_http_prefix,     rt_http_description,
     rt_http_init,       rt_http_fini,       rt_http_access,
     rt_http_open,       rt_http_close,      rt_http_write,
     rt_http_twrite,     rt_http_tell,       rt_http_read,
     rt_http_tread,      rt_http_status
};

const struct route_lowlevel rt_https_method = {
     rt_https_magic,     rt_https_prefix,    rt_https_description,
     rt_http_init,       rt_http_fini,       rt_http_access,
     rt_http_open,       rt_http_close,      rt_http_write,
     rt_http_twrite,     rt_http_tell,       rt_http_read,
     rt_http_tread,      rt_http_status
};

char *rt_http_tabschema[] = {"data", "_time", NULL};

int    rt_http_magic()		{ return RT_HTTP_LLD_MAGIC; }
char * rt_http_prefix()		{ return "http"; }
char * rt_http_description()	{ return "http client access using curl"; }
int    rt_https_magic()		{ return RT_HTTPS_LLD_MAGIC; }
char * rt_https_prefix()	{ return "https"; }
char * rt_https_description()	{ return "secure http client access using curl"; }

void   rt_http_init  (CF_VALS cf, int debug) {}

void   rt_http_fini  () {}

/* Check accessability of a URL. Always returns 0 for failure */
int    rt_http_access(char *p_url, char *password, char *basename, int flag)
{
     return 0;
}

/* open an http route. 
 * A connection is not actualy established until rt_http_read() or 
 * rt_http_write() is called.
 * Returns a descriptor for success or NULL for failure */
RT_LLD rt_http_open (char *p_url,	/* address valid until ..close() */
		     char *comment,	/* comment, ignored in this method */
		     char *password,	/* password, currently ignored */ 
		     int keep,		/* flushed groups to keep, ignored */
		     char *basename	/* non-prefix of p-url */)
{
     RT_HTTPD rt;

     rt = nmalloc(sizeof(struct rt_http_desc));
     rt->magic = rt_http_magic();
     rt->prefix = rt_http_prefix();
     rt->description = rt_http_description();
     rt->url = p_url;	/* guaranteed to be valid by caller */

     return rt;
}

void   rt_http_close (RT_LLD lld)
{
     RT_HTTPD rt;

     rt = rt_http_from_lld(lld);

     rt->magic = 0;	/* don't use again */
     nfree(rt);
}

/* Establish an HTTP connection given the address provided in rt_http_open().
 * The write is carried out using an HTTP POST method.
 * Declarations in the configuration [passed in rt_http_init()] dictate
 * the proxy, user accounts, passwords, cookie environment and ssl tokens
 * so that it is hidden from normal use
 * Return the number of characters written if successful or -1 for failure.
 */
int    rt_http_write (RT_LLD lld, const void *buf, int buflen)
{
     RT_HTTPD rt;
     char *rtext;

     rt = rt_http_from_lld(lld);

     /* compile the form */

     /* post it */
     rtext = http_post(rt->url, NULL, NULL, NULL, NULL, "", NULL, 0);
     if (!rtext)
	  return -1;
     else
	  return buflen;
}


/* Establish an HTTP connection given the address provided in rt_http_open().
 * The write is carried out using an HTTP POST method.
 * Declarations in the configuration [passed in rt_http_init()] dictate
 * the proxy, user accounts, passwords, cookie environment and ssl tokens
 * so that it is hidden from normal use
 * Return 1 for success or 0 for failure.
 */
int    rt_http_twrite (RT_LLD lld, TABLE tab)
{
     char *text;
     int r;

     return -1;
#if 0
     text = table_outtable(tab);
     if ( ! text)
       return 1;	/* empty table, successfully don't writing anything */

     r = rt_http_write(lld, text, strlen(text));
     nfree(text);

     if (r == -1)
	  return 0;
     else
	  return 1;
#endif
}


/* Sets file size and modification time; sequence is set to -1
 * Returns 1 for success, 0 for failure */
int    rt_http_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     *seq  = 0;
     *size = 0;
     *modt = 0;
     return 1;	/* always succeed for http. There is no concept of 
		 * file size currently */
}

/* Establish an HTTP connection given the address provided in rt_http_open().
 * The read is carried out using an HTTP GET method.
 * Sequence is ignored, but offset is honoured, returning data from
 * offset bytes.
 * Declarations in the configuration [passed in rt_http_init()] dictate
 * the proxy, user accounts, passwords, cookie environment and ssl tokens
 * so that it is hidden from normal use
 */
ITREE *rt_http_read  (RT_LLD lld, int seq, int offset)
{
     ROUTE_BUF *storebuf;
     ITREE *buflist;
     RT_HTTPD rt;
     char *text;

     rt = rt_http_from_lld(lld);

     text = http_get(rt->url, NULL, NULL, NULL, 0);
     if (!text)
	  return NULL;

     /* create the list */
     buflist = itree_create();
     storebuf = xnmalloc(sizeof(ROUTE_BUF));
     storebuf->buffer = text;
     storebuf->buflen = strlen(text);
     itree_append(buflist, storebuf);

     return buflist;
}


/* Establish an HTTP connection given the address provided in rt_http_open().
 * The read is carried out using an HTTP GET method.
 * Sequence is ignored, but offset is honoured, returning data from
 * offset bytes.
 * Declarations in the configuration [passed in rt_http_init()] dictate
 * the proxy, user accounts, passwords, cookie environment and ssl tokens
 * so that it is hidden from normal use.
 * A table is returned if successful, with a 'data' column and a '_time'
 * column for the time stamp.
 * NULL is returned on failure or if there is no data.
 */
TABLE rt_http_tread  (RT_LLD lld, int seq, int offset)
{
     RT_HTTPD rt;
     char *text;
     TABLE tab;
     int r;

     rt = rt_http_from_lld(lld);

     text = http_get(rt->url, NULL, NULL, NULL, 0);
     if (!text)
	  return NULL;

     /* create the list */
     tab = table_create();
     r = table_scan(tab, text, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
#if 0
     tab = table_create_a(rt_http_tabschema);
     table_addemptyrow(tab);
     table_replacecurrentcell(tab, "data", text);
     table_replacecurrentcell_alloc(tab, "_time", util_i32toa(time(NULL)));
#endif
     table_freeondestroy(tab, text);
     if (r < 1) {
	  /* empty table, no data or error */
	  table_destroy(tab);
	  tab = NULL;
     }

     return tab;
}


/*
 * Return the status of an open FILE descriptor.
 * Free the data from status and info with nfree() if non NULL.
 * If no data is available, either or both status and info may return NULL
 */
void   rt_http_status(RT_LLD lld, char **status, char **info) {
     if (status)
          *status = NULL;
     if (info)
          *info = NULL;
}


/* --------------- Private routines ----------------- */


RT_HTTPD rt_http_from_lld(RT_LLD lld	/* typeless low level data */)
{
     if (!lld)
	  elog_die(FATAL, "passed NULL low level descriptor");
     if ( (((RT_HTTPD)lld)->magic != RT_HTTP_LLD_MAGIC) &&
	  (((RT_HTTPD)lld)->magic != RT_HTTPS_LLD_MAGIC) )
	  elog_die(FATAL, "magic type mismatch: we were given "
		   "%s (%s) but can handle only %s (%s) or %s (%d)", 
		   ((RT_HTTPD)lld)->prefix, 
		   ((RT_HTTPD)lld)->description,
		   rt_http_prefix(),  rt_http_description(),
		   rt_https_prefix(), rt_https_description() );

     return (RT_HTTPD) lld;
}


#if TEST

#define TURL1 "http://localhost"
#define TRUL2 "https://localhost"

int main(int argc, char **argv) {
     int r, seq1, size1;
     CF_VALS cf;
     RT_LLD lld1;
     time_t time1;
     ITREE *chain;
     ROUTE_BUF *rtbuf;

     cf = cf_create();
     http_init();
     rt_http_init(cf, 1);

     /* test 1: is it there? */
     r = rt_http_access(TURL1, NULL, TURL1, ROUTE_READOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have read access to http %s", 
		   TURL1);
     r = rt_http_access(TURL1, NULL, TURL1, ROUTE_WRITEOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have write access to http %s", 
		   TURL1);

     /* test 2: open for read only should not create http */
     lld1 = rt_http_open(TURL1, "blah", NULL, 0, TURL1);
     if (!lld1)
	  elog_die(FATAL, "[2] no open http descriptor");

     /* test 3: read an http location */
     chain = rt_http_read(lld1, 0, 0);
     if (itree_n(chain) != 1)
	  elog_die(FATAL, "[3] wrong number of buffers: %d", 
		   itree_n(chain));
     itree_first(chain);
     rtbuf = itree_get(chain);
     if (!rtbuf)
	  elog_die(FATAL, "[3] no buffer");
     if (!rtbuf->buffer)
	  elog_die(FATAL, "[3] NULL buffer");
     if (rtbuf->buflen != strlen(rtbuf->buffer))
	  elog_die(FATAL, "[3] buffer length mismatch %d != %d",
		   rtbuf->buflen, strlen(rtbuf->buffer));
     route_free_routebuf(chain);

     /* test 4: tell test */
     r = rt_http_tell(lld1, &seq1, &size1, &time1);
     if (r)
	  elog_die(FATAL, "[4] http tell should always fail");
     rt_http_close(lld1);

     cf_destroy(cf);
     rt_http_fini();
     return 0;
}

#endif /* TEST */
