/*
 * Route driver for sqlrs, a set of format conventions for http
 *
 * Nigel Stuckey, August 2003
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
#include "rt_sqlrs.h"

/* private functional prototypes */
RT_SQLRSD rt_sqlrs_from_lld(RT_LLD lld);

const struct route_lowlevel rt_sqlrs_method = {
     rt_sqlrs_magic,      rt_sqlrs_prefix,     rt_sqlrs_description,
     rt_sqlrs_init,       rt_sqlrs_fini,       rt_sqlrs_access,
     rt_sqlrs_open,       rt_sqlrs_close,      rt_sqlrs_write,
     rt_sqlrs_twrite,     rt_sqlrs_tell,       rt_sqlrs_read,
     rt_sqlrs_tread
};
CF_VALS rt_sqlrs_cf;
char *rt_sqlrs_posttext=NULL;

int    rt_sqlrs_magic()		{ return RT_SQLRS_LLD_MAGIC; }
char * rt_sqlrs_prefix()	{ return "sqlrs"; }
char * rt_sqlrs_description()	{ return "SQL ringstore client using curl"; }

void   rt_sqlrs_init  (CF_VALS cf, int debug) {rt_sqlrs_cf = cf;}

void   rt_sqlrs_fini  () {}

/* Check accessability of a URL. Always returns 0 for failure */
int    rt_sqlrs_access(char *p_url, char *password, char *basename, int flag)
{
     return 0;
}

/* open an sqlrs route. 
 * A connection is not actualy established until rt_sqlrs_read() or 
 * rt_sqlrs_write() is called, just like rt_http_open().
 * For successful operation, p-urls should be of the form 'sqlrs: ... !tsv'.
 * Returns a descriptor for success or NULL for failure */
RT_LLD rt_sqlrs_open (char *p_url,	/* address valid until ..close() */
		     char *comment,	/* comment, ignored in this method */
		     char *password,	/* password, currently ignored */ 
		     int keep,		/* flushed groups to keep, ignored */
		     char *basename	/* non-prefix of p-url */)
{
     RT_SQLRSD rt;
     char *url;

     rt = nmalloc(sizeof(struct rt_sqlrs_desc));
     rt->magic  = rt_sqlrs_magic();
     rt->prefix = rt_sqlrs_prefix();
     rt->description = rt_sqlrs_description();
     rt->url    = p_url;	/* valid through the life of the transaction */
     rt->addr   = util_strjoin("sqlrs:",	/* method */
			       basename,	/* sqlrs address */
			       "!csv",		/* comma sep vals format */
			       NULL);

     /* GET operations have the following address
      *    [RT_SQLRS_PUT_URLKEY]?a=[address]
      *    from cf                 from basename */
     url = cf_getstr(rt_sqlrs_cf, RT_SQLRS_GET_URLKEY);
     if (rt_sqlrs_cf == NULL || url == NULL) {
	  elog_printf(DIAG, "repository URL not configured: "
		      "unable to open %s; set config variable '%s'",
		      p_url, RT_SQLRS_GET_URLKEY);
	  nfree(rt);
	  return NULL;
     }
     rt->geturl = util_strjoin(url, "?a=sqlrs:", basename, "!csv", NULL);

     /* POST operations pass the address arguments when they post */
     url = cf_getstr(rt_sqlrs_cf, RT_SQLRS_PUT_URLKEY);
     if (rt_sqlrs_cf == NULL || url == NULL) {
	  elog_printf(DIAG, "repository URL not configured: "
		      "unable to open %s; set config variable '%s'",
		      p_url, RT_SQLRS_PUT_URLKEY);
	  nfree(rt);
	  return NULL;
     }
     rt->puturl = util_strjoin(url, "?a=sqlrs:", basename, "!csv", NULL);
     /*rt->puturl = xnstrdup(url);*/

     return rt;
}

void   rt_sqlrs_close (RT_LLD lld)
{
     RT_SQLRSD rt;

     rt = rt_sqlrs_from_lld(lld);

     rt->magic = 0;	/* don't use again */
     nfree(rt->addr);
     nfree(rt->geturl);
     nfree(rt->puturl);
     nfree(rt);
}

/* Establish an SQLRS connection given the address provided in rt_sqlrs_open().
 * The write is carried out using an HTTP POST method.
 * A status line is returned as a result of the post together with
 * optional text to give further information. This data is held until the
 * next call to rt_sqlrs_write(); they can be retrieved by reading the
 * special p_urls of 'sqlrs:_WRITE_STATUS_' (defined to 
 * RT_SQLRS_WRITE_STATUS) and 'sqlrs:_WRITE_RETURN_' (defined to 
 * RT_SQLRS_WRITE_RETURN)
 * Data format is comma separated fat headed array: cvs fha and defined
 * by the habitat to harvest  protocol.
 * Returns the number of characters written if successful or -1 for failure.
 */
int    rt_sqlrs_write (RT_LLD lld, const void *buf, int buflen)
{
     RT_SQLRSD rt;
     TREE *form, *parts;

     rt = rt_sqlrs_from_lld(lld);

     /* is the buffer terminated? */
     if (((char *)buf)[buflen])
	  elog_die(FATAL, "buffer untruncated");

     /* compile the form */
     form = tree_create();
     tree_add(form, "a",    rt->addr);
     tree_add(form, "host", util_hostname());
     parts = tree_create();
     tree_add(parts, "upfile", (void *) buf);

     /* post it */
     if (rt_sqlrs_posttext) {
	  nfree(rt_sqlrs_posttext);
	  rt_sqlrs_posttext = NULL;
     }
     rt_sqlrs_posttext = http_post(rt->puturl, form, NULL, parts, NULL, 
				   NULL, 0);
     tree_destroy(form);
     tree_destroy(parts);
     if (rt_sqlrs_posttext && strncmp(rt_sqlrs_posttext, "OK", 2) == 0)
	  return buflen;
     else
	  return -1;
}


/* Establish an SQLRS connection given the address provided in rt_sqlrs_open().
 * The write is carried out using an HTTP POST method.
 * If any text is returned as a result of the post, it is held until the
 * next call to rt_sqlrs_write(); it can be retrieved by reading the
 * special p_url of 'sqlrs:_WRITE_RETURN_', with caps and underlines 
 * preserved. Do not free this text, as it will be managed by rt_sqlrs.
 * Returns 1 for success or 0 for failure
 */
int    rt_sqlrs_twrite (RT_LLD lld, TABLE tab)
{
     char *text;
     RT_SQLRSD rt;
     int r;

     rt = rt_sqlrs_from_lld(lld);

     /* output full table using CSV format */
     text = table_outtable_full(tab, ',', TABLE_WITHCOLNAMES, TABLE_WITHINFO);
     if ( ! text)
       return 1;	/* empty table, successfully don't writing anything */

     r = rt_sqlrs_write(lld, text, strlen(text));
     nfree(text);

     if (r == -1)
	  return 0;
     else
	  return 1;
}


/* Sets position in file, position in sequence and modification time.
 * Currently sqlrs: is stateless, so the call will always succeed and
 * -1 (file), 0 (seq) will  be returned.
 * Returns 1 for success, 0 for failure */
int    rt_sqlrs_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     *seq=0;
     *size=-1;
     *modt=-1;
     return 1;	/* always succeed */
}

/* Establish an SQLRS connection given the address provided in rt_sqlrs_open().
 * The read is carried out using an SQLRS GET method.
 * Sequence is ignored, but offset is honoured, returning data from
 * offset bytes.
 * Declarations in the configuration [passed in rt_sqlrs_init()] dictate
 * the proxy, user accounts, passwords, cookie environment and ssl tokens
 * so that it is hidden from normal use
 * Special addresses exist to read from the internal status of SQLRS
 * (no HTTP request is made).
 *     '_WRITE_STATUS_'  (#defined as RT_SQLRS_WRITE_STATUS)  
 *         The status line (1st) from a previous rt_sqlrs_write()
 *     '_WRITE_RETURN_'  (#defined as RT_SQLRS_WRITE_STATUS)
 *         The data lines (2nd on) from a previous rt_sqlrs_write()
 * Returns an ordered list of sequence buffers, unless no data is available
 * or there is an error, when NULL is returned.
 */
ITREE *rt_sqlrs_read  (RT_LLD lld, int seq, int offset)
{
     ROUTE_BUF *storebuf;
     ITREE *buflist;
     RT_SQLRSD rt;
     char *text;
     int len;

     rt = rt_sqlrs_from_lld(lld);

     if (strcmp(RT_SQLRS_WRITE_STATUS, rt->url) == 0) {
	  if (rt_sqlrs_posttext) {
	       len = strcspn(rt_sqlrs_posttext, "\n");
	       text = xnmemdup(rt_sqlrs_posttext, len+1);
	  } else {
	       len = 0;
	       text = xnmalloc(1);
	  }
	  text[len] = '\0';
     } else if (strcmp(RT_SQLRS_WRITE_RETURN, rt->url) == 0) {
	  if (rt_sqlrs_posttext) {
	       len = strcspn(rt_sqlrs_posttext, "\n");
	       text = xnstrdup(rt_sqlrs_posttext+len+1);
	  } else {
	       len = 0;
	       text = xnstrdup("");
	  }
     } else {
	  text = http_get(rt->geturl, NULL, NULL, 0);
     }
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


/* Establish an SQLRS connection given the address provided in rt_sqlrs_open().
 * The read is carried out using an HTTP GET method.
 * Sequence is ignored, but offset is honoured, returning data from
 * offset bytes.
 * Declarations in the configuration [passed in rt_sqlrs_init()] dictate
 * the proxy, user accounts, passwords, cookie environment and ssl tokens
 * so that it is hidden from normal use.
 * A table is returned if successful, assuming that the text payload
 * is comma separated fat headed array: csv fha.
 * NULL is returned if there is no data to read or if there is a failure.
 */
TABLE rt_sqlrs_tread  (RT_LLD lld, int seq, int offset)
{
     RT_SQLRSD rt;
     char *text;
     TABLE tab;
     int r, len;

     rt = rt_sqlrs_from_lld(lld);

     if (strcmp(RT_SQLRS_WRITE_STATUS, rt->url) == 0) {
	  len = strcspn(rt_sqlrs_posttext, "\n");
	  text = util_strjoin("status\n--\n", rt_sqlrs_posttext, NULL);
	  text[len+10] = '\0';
     } else if (strcmp(RT_SQLRS_WRITE_RETURN, rt->url) == 0) {
	  len = strcspn(rt_sqlrs_posttext, "\n");
	  text = xnstrdup(rt_sqlrs_posttext+len+1);
     } else {
	  text = http_get(rt->geturl, NULL, NULL, 0);
     }
     if (!text)
	  return NULL;

     /* create the table, assuming headers exist */
     tab = table_create();
     table_freeondestroy(tab, text);
     r = table_scan(tab, text, ",", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r < 1) {
	  /* empty table, no data or error */
	  table_destroy(tab);
	  tab = NULL;
     }

     return tab;
}


/* --------------- Private routines ----------------- */


RT_SQLRSD rt_sqlrs_from_lld(RT_LLD lld	/* typeless low level data */)
{
     if (!lld)
	  elog_die(FATAL, "passed NULL low level descriptor");
     if (((RT_SQLRSD)lld)->magic != RT_SQLRS_LLD_MAGIC)
	  elog_die(FATAL, "magic type mismatch: we were given "
		   "%s (%s) but can handle only %s (%s)", 
		   ((RT_SQLRSD)lld)->prefix, 
		   ((RT_SQLRSD)lld)->description,
		   rt_sqlrs_prefix(),  rt_sqlrs_description() );

     return (RT_SQLRSD) lld;
}


#if TEST

#define TURL1 "sqlrs:"
#define TURL2 "sqlrs:alex"
#define TURL3 "sqlrs:alex,justcpu"

int main(int argc, char **argv) {
     int r, seq1, size1;
     CF_VALS cf;
     RT_LLD lld1;
     time_t time1;
     ITREE *chain;
     ROUTE_BUF *rtbuf;

     cf = cf_create();
     cf_addstr(cf, RT_SQLRS_PUT_URLKEY, 
	       "http://localhost/harvest/cgi-bin/sqlrs_get.cgi");
     cf_addstr(cf, RT_SQLRS_GET_URLKEY, 
	       "http://localhost/harvest/cgi-bin/sqlrs_get.cgi");
     http_init();
     rt_sqlrs_init(cf, 1);

     /* test 1: is it there? */
     r = rt_sqlrs_access(TURL1, NULL, TURL1, ROUTE_READOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have read access to sqlrs %s", 
		   TURL1);
     r = rt_sqlrs_access(TURL1, NULL, TURL1, ROUTE_WRITEOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have write access to sqlrs %s", 
		   TURL1);

     /* test 2: open for read only */
     lld1 = rt_sqlrs_open(TURL1, "blah", NULL, 0, TURL1);
     if (!lld1)
	  elog_die(FATAL, "[2] no open sqlrs descriptor");

     /* test 3: read an sqlrs location */
     chain = rt_sqlrs_read(lld1, 0, 0);
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
     r = rt_sqlrs_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[4] sqlrs tell should always succeed");
     rt_sqlrs_close(lld1);

     cf_destroy(cf);
     rt_sqlrs_fini();
     return 0;
}

#endif /* TEST */
