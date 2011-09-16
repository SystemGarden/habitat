/*
 * Route driver for local, current implemented with http
 *
 * Nigel Stuckey, July 2011
 * Copyright System Garden Ltd 2011. All rights reserved
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
#include "httpd.h"
#include "rt_http.h"
#include "rt_local.h"

/* private functional prototypes */
RT_LOCALD rt_local_from_lld(RT_LLD lld);

const struct route_lowlevel rt_local_method = {
     rt_local_magic,      rt_local_prefix,     rt_local_description,
     rt_local_init,       rt_local_fini,       rt_local_access,
     rt_local_open,       rt_local_close,      rt_local_write,
     rt_local_twrite,     rt_local_tell,       rt_local_read,
     rt_local_tread,      rt_local_status,     rt_local_checkpoint
};

const struct route_lowlevel rt_localmeta_method = {
     rt_localmeta_magic,  rt_localmeta_prefix, rt_localmeta_description,
     rt_local_init,       rt_local_fini,       rt_local_access,
     rt_localmeta_open,   rt_local_close,      rt_local_write,
     rt_local_twrite,     rt_local_tell,       rt_local_read,
     rt_local_tread,      rt_local_status,     rt_local_checkpoint
};

char *rt_local_tabschema[] = {"data", "_time", NULL};

int    rt_local_magic()		{ return RT_LOCAL_LLD_MAGIC; }
char * rt_local_prefix()	{ return RT_LOCAL_PREFIX; }
char * rt_local_description()	{ return RT_LOCAL_DESCRIPTION; }

int    rt_localmeta_magic()		{ return RT_LOCALMETA_LLD_MAGIC; }
char * rt_localmeta_prefix()		{ return RT_LOCALMETA_PREFIX; }
char * rt_localmeta_description()	{ return RT_LOCALMETA_DESCRIPTION; }

void   rt_local_init  (CF_VALS cf, int debug) {}

void   rt_local_fini  () {}

/* Check accessability of a pURL on the local host. 
 * Always returns 0 for failure */
int    rt_local_access(char *p_url, char *password, char *basename, int flag)
{
     char *local;
     int r;

     /* local access is of the form http://localhost:1324/localtsv/<url> */
     local = util_strjoin("http://localhost:", HTTPD_PORT_HTTP_STR,
			  "/localtsv/", p_url, NULL);

     r = rt_http_access(local, password, basename, flag);

     nfree(local);

     return r;
}

/* open a route to local data. 
 * A connection is not actualy established until rt_local_read() or 
 * rt_local_write() is called.
 * Returns a descriptor for success or NULL for failure */
RT_LLD rt_local_open (char *p_url,	/* address valid until ..close() */
		      char *comment,	/* comment, ignored in this method */
		      char *password,	/* password, currently ignored */ 
		      int keep,		/* flushed groups to keep, ignored */
		      char *basename	/* non-prefix of p-url */)
{
     RT_LOCALD rt;
     char *localbase, *localpurl;

     /* local access is of the form http://localhost:1324/localtsv/<url> */
     /* Names passed to HTTP need to exist during the life of the instance */
     localpurl = util_strjoin("http://localhost:", HTTPD_PORT_HTTP_STR,
			      "/localtsv/", basename, NULL);
     localbase = localpurl+5;

     rt = nmalloc(sizeof(struct rt_local_desc));
     rt->magic = rt_local_magic();
     rt->prefix = rt_local_prefix();
     rt->description = rt_local_description();
     rt->url = p_url;	/* guaranteed to be valid by caller */
     rt->basepurl = localpurl;
     rt->hrt = rt_http_open(localpurl, comment, password, keep, localbase);

     return rt;
}

/* open a route to local meta data. 
 * A connection is not actualy established until rt_local_read() or 
 * rt_local_write() is called.
 * Returns a descriptor for success or NULL for failure */
RT_LLD rt_localmeta_open(char *p_url,	/* address valid until ..close() */
			 char *comment,	/* comment, ignored in this method */
			 char *password,/* password, currently ignored */ 
			 int keep,	/* flushed groups to keep, ignored */
			 char *basename	/* non-prefix of p-url */)
{
     RT_LOCALD rt;
     char *localbase, *localpurl;

     /* local access is of the form http://localhost:1324/localtsv/<url> */
     /* Names passed to HTTP need to exist during the life of the instance */
     localpurl = util_strjoin("http://localhost:", HTTPD_PORT_HTTP_STR,
			      "/", basename, NULL);
     localbase = localpurl+5;

     rt = nmalloc(sizeof(struct rt_local_desc));
     rt->magic = rt_localmeta_magic();
     rt->prefix = rt_localmeta_prefix();
     rt->description = rt_localmeta_description();
     rt->url = p_url;	/* guaranteed to be valid by caller */
     rt->basepurl = localpurl;
     rt->hrt = rt_http_open(localpurl, comment, password, keep, localbase);

     return rt;
}

void   rt_local_close (RT_LLD lld)
{
     RT_LOCALD rt;

     rt = rt_local_from_lld(lld);
     rt_http_close(rt->hrt);

     rt->magic = 0;	/* don't use again */
     nfree(rt->basepurl);
     nfree(rt);
}

/* Connect to clockwork on the local host following an rt_local_open()
 * and write the buffer to it.
 * Implemented with http and rt_http_write()
 * Return the number of characters written if successful or -1 for failure.
 */
int    rt_local_write (RT_LLD lld, const void *buf, int buflen)
{
     RT_LOCALD rt;
     int r;

     rt = rt_local_from_lld(lld);
     r = rt_http_write(rt->hrt, buf, buflen);

     return r;
}


/* Establish a local connection given the address provided in rt_local_open()
 * and write the table to it.
 * Uses http with rt_http_twrite().
 * Return 1 for success or 0 for failure.
 */
int    rt_local_twrite (RT_LLD lld, TABLE tab)
{
     RT_LOCALD rt;
     int r;

     rt = rt_local_from_lld(lld);
     r = rt_http_twrite(rt->hrt, tab);

     return r;
}


/* Sets file size and modification time; sequence is set to -1
 * Returns 1 for success, 0 for failure */
int    rt_local_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     *seq  = 0;
     *size = 0;
     *modt = 0;
     return 1;	/* always succeed for http. There is no concept of 
		 * file size currently */
}

/* Establish a local connection given the address provided in rt_local_open()
 * and read the list of data buffers.
 * Currently implemented with http using rt_http_read().
 */
ITREE *rt_local_read  (RT_LLD lld, int seq, int offset)
{
     RT_LOCALD rt;
     ITREE *it;

     rt = rt_local_from_lld(lld);
     it = rt_http_read(rt->hrt, seq, offset);

     return it;
}


/* Establish a local connection given the address provided in rt_local_open()
 * and read a table of data
 * Currently implemented with http using rt_http_tread().
 * NULL is returned on failure or if there is no data.
 */
TABLE rt_local_tread  (RT_LLD lld, int seq, int offset)
{
     RT_LOCALD rt;

     rt = rt_local_from_lld(lld);
     return rt_http_tread(rt->hrt, seq, offset);
}


/*
 * Return the status of an open FILE descriptor.
 * Free the data from status and info with nfree() if non NULL.
 * If no data is available, either or both status and info may return NULL
 */
void   rt_local_status(RT_LLD lld, char **status, char **info) {
     if (status)
          *status = NULL;
     if (info)
          *info = NULL;
}


/* Checkpoint always returns true and does nothing as yet
 * Return 1 for success or 0 for failure.
 */
int    rt_local_checkpoint (RT_LLD lld)
{
     return 1;
}
/* --------------- Private routines ----------------- */


RT_LOCALD rt_local_from_lld(RT_LLD lld	/* typeless low level data */)
{
     if (!lld)
	  elog_die(FATAL, "passed NULL low level descriptor");

     if ( (((RT_LOCALD)lld)->magic != RT_LOCAL_LLD_MAGIC) &&
	  (((RT_LOCALD)lld)->magic != RT_LOCALMETA_LLD_MAGIC) )
	  elog_die(FATAL, "magic type mismatch: we were given "
		   "%s (%s) but can handle only %s (%s)", 
		   ((RT_LOCALD)lld)->prefix, 
		   ((RT_LOCALD)lld)->description,
		   rt_local_prefix(),  rt_local_description() );

     return (RT_LOCALD) lld;
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
     local_init();
     rt_local_init(cf, 1);

     /* test 1: is it there? */
     r = rt_local_access(TURL1, NULL, TURL1, ROUTE_READOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have read access to http %s", 
		   TURL1);
     r = rt_local_access(TURL1, NULL, TURL1, ROUTE_WRITEOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have write access to http %s", 
		   TURL1);

     /* test 2: open for read only should not create http */
     lld1 = rt_local_open(TURL1, "blah", NULL, 0, TURL1);
     if (!lld1)
	  elog_die(FATAL, "[2] no open http descriptor");

     /* test 3: read an http location */
     chain = rt_local_read(lld1, 0, 0);
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
     r = rt_local_tell(lld1, &seq1, &size1, &time1);
     if (r)
	  elog_die(FATAL, "[4] http tell should always fail");
     rt_local_close(lld1);

     cf_destroy(cf);
     rt_local_fini();
     return 0;
}

#endif /* TEST */
