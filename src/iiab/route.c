/*
 * IO routing
 * New implementation July 2003
 *
 * Copyright System Garden Ltd 2003. All rights reserved.
 */
#include <stdarg.h>
#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "nmalloc.h"
#include "http.h"
#include "cf.h"
#include "route.h"
#include "elog.h"
#include "util.h"

/* global structures */
TREE *  route_drivers=NULL;	/* key=prefix, val=ROUTE_METHOD */
int     route_debug;		/* additional debug flag */
CF_VALS route_cf;		/* Configuration tree, used to expand purls */

/* Private functional prototypes */
ROUTE_METHOD route_priv_get_driver(char *p_url, char **suffix);

/*
 * Initialise the route class.
 * All errors go to elog, which will call the route class to send 
 * then on their way. 
 * (Chicken and egg moment: elog uses the route class, thus there could be
 * and initialisation deadlock. Uninitialised elogs send their errors to
 * stderr to avoid this, so you should always intiialise route before elog.)
 */
void route_init(CF_VALS cf,	/* Configuration: used to expand p_urls */
		int debug	/* Debug flag */)
{
     if (route_drivers) 
          elog_die(FATAL, "attempted reinitialisation");
     route_debug = debug;
     route_drivers = tree_create();
     route_cf = cf;
}

/*
 * Shut down the route class
 */
void route_fini()
{
     tree_destroy(route_drivers);
     route_drivers = NULL;
}

/*
 * Register a driver as a route using a structure of methods that define
 * the implementation. If a driver already exists with the same 
 * prefix name, then it will be over written.
 */
void route_register(ROUTE_METHOD meth	/* driver's set of methods */)
{
     if ( tree_find(route_drivers, meth->ll_prefix()) == TREE_NOVAL )
          tree_add(route_drivers, meth->ll_prefix(), (void *) meth);
     else
          tree_put(route_drivers, (void *) meth);

     meth->ll_init(route_cf, route_debug);
}

/*
 * Remove the driver as a valid route.
 * No clearing up is currently done
 * Returns 1 if successful or 0 otherwise
 */
int route_unregister(char *prefix	/* dirver's prefix name */)
{
     if ( tree_find(route_drivers, prefix) == TREE_NOVAL ) {
          return 0;
     } else {
          tree_rm(route_drivers);
	  return 1;
     }
}

/*
 * Return a TREE list of registered methods. 
 * The TREE's key is the prefix name of the method, the corresponding value
 * is ROUTE_METHOD structure.
 * The returned TREE is internal, so it should not be modified or free'ed
 */
TREE * route_registered()
{
     return route_drivers;
}

/*
 * Return a method's function structure when given its pseudo-url.
 * Also, passes a pointer back to that part of the p-url where the
 * prefix ends, unless suffix is NULL.
 * Requires p-url address to be of the form <prefix>:<suffix>
 * Returns NULL if the driver does not exist
 */
ROUTE_METHOD route_priv_get_driver(char *p_url,  /* pseudo-url */
				   char **suffix /* p-url after prefix */ ) 
{
     static char prefix[21];
     int len;

     /* format is:
      *
      *    driver:location
      *
      * check the driver exists and apply defaults, etc
      */
     if ( ! strchr (p_url, ':') ) {
	  /* if there is no driver separator, assume the 'file:' driver, 
	   * which will append to files if written */
	  elog_printf(DIAG, "driver not specified in '%s', assuming 'file:%s'",
		      p_url, p_url);
	  strcpy(prefix, "file");
	  len = -1;
     } else {
	  /* we have a driver, but check the length */
	  len = strcspn(p_url, ":");
	  if (len > 20) {
	       elog_printf(ERROR, "driver identifer length greater than 20 "
			   "chars (%d, %s)", len, p_url);
	       return NULL;
	  }
	  strncpy(prefix, p_url, len);
	  prefix[len] = '\0';
     }

     if (suffix)
	  *suffix = p_url + len + 1;

     if (tree_find(route_drivers, prefix) == TREE_NOVAL) {
	  elog_printf(DIAG, "driver '%s' not recognised "
		      "(format is [driver:]location)", prefix);
	  return NULL;
     } else
	  return tree_get(route_drivers);
}

/*
 * Check whether the pseudo-url is accessable.
 * Returns 1 for true, 0 otherwise or in error
 */
int route_access(char *p_url,	/* Pseduo-url */
		 char *password,/* Password if supported by channel */
		 int flags	/* ROUTE_READOK or ROUTE_WRITE */ )
{
     ROUTE_METHOD meth;
     char *base;

     meth = route_priv_get_driver(p_url, &base);
     if (!meth) {
	  elog_printf(DIAG, "no known driver in %s", p_url);
	  return 0;
     }
     return meth->ll_access(p_url, password, base, flags);
}

/*
 * Plain p-url open.
 * Open a message route to the location specified by the pseudo-url string 
 * p_url. Depending on the method, password may be required, otherwise
 * it may be "" or NULL. Keep is needed when creating the location of 
 * methods, as it predefines the size.
 * If keep is 0, then new routes will not be created.
 * Returns an ROUTE on success or NULL on failure. Following a good
 * call, the ROUTE is considered a route and can be used as a handle 
 * in the route_printf() call.
 * If the p-url is a template (it contains %<x> characters), the call 
 * will fail and the user will need to open it with route_open_t(), which
 * requires additional arguments to resolve the wild cards.
 */
ROUTE route_open(char *p_url,	/* Pseduo-url */
		 char *comment,	/* Creation comment if supported by channel */
		 char *password,/* Password if supported by channel */
		 int keep	/* Creation keep setting if supported */ )
{
     ROUTE_METHOD meth;
     RT_LLD lld;
     ROUTE rt;
     char *purl, *base;

     /* drivers must have a copy of purl constant and valid between
      * their open and close callc */
     purl = xnstrdup(p_url);
     meth = route_priv_get_driver(purl, &base);
     if (! meth) {
	  nfree(purl);
	  return NULL;
     }

     lld  = meth->ll_open(purl, comment, password, keep, base);
     if (lld) {
	  rt = nmalloc(sizeof(struct route_handle));
	  rt->p_url  = purl;
	  rt->method = meth;
	  rt->handle = lld;
	  rt->unsent.buflen = 0;
	  rt->unsent.buffer = NULL;
	  return rt;
     } else {
	  nfree(purl);
	  return NULL;
     }
}

/*
 * Template p-url open.
 * Open a message route to the location defined by the template pseudo-url,
 * once resolved with additional information. See route_open() for the 
 * standard call arguments. Additional arguments are job name.
 * Returns ROUTE on success, NULL on failure.
 */
ROUTE route_open_t(char *p_url,		/* Pseduo-url */
		   char *comment,	/* Creation comment (if supported) */
		   char *password,	/* Password (if supported) */
		   int   keep,		/* Creation keep setting (if supp) */
		   char *jobname,	/* Job name for this p-url */
		   int   duration	/* Duration in seconds */ )
{
     char buf[1000];		/* Compose buffer */
     
     route_expand(buf, p_url, jobname, duration);
     return route_open(buf, comment, password, keep);
}

/*
 * Copy string src to dst, expanding the special tokens.
 * All tokens are of the form  %<x>, expansion is as follows:-
 *	%j - Current job name
 *      %h - Host name
 *      %m - domain name
 *      %f - Fully qualified hostname
 *      %d - Duration
 *      %v - iiab_dir_var directory (typically ../var)
 * Returns the number of tokens expanded or -1 for error
 */
int route_expand(char *dst,	/* Destination (expanded) string */
		 char *src,	/* Source (template) string */
		 char *jobname,	/* %j value - jobname */
		 int   duration	/* %u value - duration */ )
{
     char *upt, *bpt, *pt;	/* p-url, buf and temp pointers */
     int l;			/* Temp length register */
     int ntok=0;		/* Number of tokens expanded */
     char *name;		/* host, domain and fqhost names */
     
     /* Copy p_url to buf, expanding %<x> tokens */
     pt = upt = src;
     bpt = dst;
     while ((pt = strchr(pt, '%'))) {
	  strncpy(bpt, upt, pt - upt);
	  bpt += pt-upt;
	  switch (*(++pt)) {
	  case 'j':	/* %j - Insert job name */
	       if (jobname == NULL)
		    l = 0;
	       else
		    l = strlen(jobname);
	       strncpy(bpt, jobname, l);
	       bpt += l;
	       break;
	  case 'h':	/* %h - Insert host name */
	       name = util_hostname();
	       if (!name)
		    name = "HOST_NAME_ERROR";
	       l = strlen(name);
	       strncpy(bpt, name, l);
	       bpt += l;
	       break;
	  case 'm':	/* %m - Insert domain name */
	       name = util_domainname();
	       if (!name)
		    name = "DOMAIN_NAME_ERROR";
	       l = strlen(name);
	       strncpy(bpt, name, l);
	       bpt += l;
	       break;
	  case 'f':	/* %f - Insert fully qualified host name */
	       name = util_fqhostname();
	       if (!name)
		    name = "FQ_HOST_NAME_ERROR";
	       l = strlen(name);
	       strncpy(bpt, name, l);
	       bpt += l;
	       break;
	  case 'd':	/* %d - Insert duration */
	       bpt += sprintf(bpt, "%d", duration);
	       break;
	  case 'v':	/* %v - Insert /var directory */
	       name = cf_getstr(route_cf, "iiab.dir.var");
	       l = strlen(name);
	       strncpy(bpt, name, l);
	       bpt += l;
	       break;
	  default:	/* Unknown switch */
	       elog_printf(ERROR, "unknown switch `%c'", *pt);
	       return -1;
	  }
	  upt = ++pt;	/* Continue at character following %<x> */
	  ntok++;
     }
     
     if (bpt == dst)		/* No buffer == no token => pass through */
	  strcpy(dst, src);
     else
	  strcpy(bpt, upt);
          /* *bpt = '\0'; */
     
     return ntok;
}

/*
 * Flush a route.
 * Outstanding data held in a buffer for a route is written to its destination
 * without closing the handle.
 * When rt==NULL, always returns success as it is used in initialisation.
 * Returns 1 if successful, 0 otherwise.
 */
int route_flush(ROUTE rt /* open, valid route */) {
     int r, ret;

     /* special case: initialising, always success */
     if ( ! rt)
	  return 1;

     if (rt->unsent.buflen == 0)
	  return 1;	/* nothing to write */

     /* debug info for flushing */
     if (route_debug)
	  fprintf(stderr, "flushing %s len=%d text=`%.30s'%s\n", 
		  rt->p_url, rt->unsent.buflen, (char *) rt->unsent.buffer,
		  (rt->unsent.buflen > 30) ? "...(trunc)" : "");

     /* flush the buffer */
     if (rt->unsent.buffer) {
	  r = rt->method->ll_write(rt->handle, rt->unsent.buffer, 
				   rt->unsent.buflen);
	  if (r < rt->unsent.buflen) {
	       elog_printf(ERROR, "can't write to %s, "
			   "discarding len=%d text=`%.20s'%s", rt->p_url, 
			   rt->unsent.buflen, rt->unsent.buffer,
			   (rt->unsent.buflen>20)?"...(truncated)":"");
	       ret = 0;	/* failure */
	  } else {
	       ret = 1;	/* success */
	  }
	  nfree(rt->unsent.buffer);
	  rt->unsent.buffer=NULL;
	  rt->unsent.buflen=0;
     }

     return ret;
}


/*
 * Return a pointer to the buffer of pending characters in the route 
 * and its length
 */
char *route_buffer(ROUTE rt,	/* open, valid route */
		   int *buflen	/* length of buffer */ )
{
     if (buflen)
	  *buflen = rt->unsent.buflen=0;
     return rt->unsent.buffer;
}


/*
 * Clear the buffer of pending characaters in the route
 */
void route_killbuffer(ROUTE rt,	/* open, valid route */
		      int freealloc	/* nfree() the allocated mem */)
{
     if (freealloc && rt->unsent.buffer)
	  nfree(rt->unsent.buffer);
     rt->unsent.buffer=NULL;
     rt->unsent.buflen=0;
}


/*
 * Close a route.
 * Outstanding data is flushed, the route is closed and the rt structue
 * is free()ed. Do not try to use rt after this call!!
 */
void route_close(ROUTE rt /* open, valid route */) {
     if (rt == NULL)
	  return;

     route_flush(rt);
     rt->method->ll_close(rt->handle);
     nfree(rt->p_url);
     if (rt->unsent.buffer)
	  nfree(rt->unsent.buffer);
     nfree(rt);
}

/*
 * Write data to an opened route (in fact it is queued for writing).
 * Use route_flush() or route_close() to complete the write.
 * Returns the number of characters queued for sending or -1 for error
 */
int route_write(ROUTE rt,		/* Open valid route */
		const void *buf,	/* Data to send to route */
		int buflen		/* Length of data to send */ )
{
     void *newbuf;

     if ( ! rt)
	  return -1;
     if (buflen == 0)
	  return 0;
     if (buflen < 0) {
          /* use stderr to avoid loops */
	  fprintf(stderr, "spurious buflen %d in %s; write aborted\n",
		  buflen, rt->p_url);
	  return -1;
     }

     /* Append data to buffer, make sure to append a terminating NULL */
     if (route_debug)
          /* use stderr to avoid loops */
	  fprintf(stderr, "enlarge %s buffer by %d: %d -> %d\n",
		  rt->p_url, buflen, rt->unsent.buflen, 
		  rt->unsent.buflen+buflen);
     newbuf = xnrealloc(rt->unsent.buffer, rt->unsent.buflen + buflen +1);
     memcpy(newbuf + rt->unsent.buflen, buf, buflen);
     rt->unsent.buflen += buflen;
     rt->unsent.buffer = newbuf;
     ((char *)rt->unsent.buffer)[rt->unsent.buflen] = '\0';

     return buflen;
}


/*
 * Write table data to an opened route straight away (unbuffered).
 * Existing data in the buffer will be flushed, then the table data sent
 * using this call, no data will be left in the buffer afterwards.
 * Returns 1 for success or 0 for faliure.
 */
int route_twrite(ROUTE rt,		/* Open valid route */
		 TABLE tab		/* Data in table to send */ )
{
     if ( ! rt)
	  return 0;	/* failure */
     if ( ! tab)
	  return 0;	/* failure */

     if (!route_flush(rt))
	  return 0;
     return rt->method->ll_twrite(rt->handle, tab);
}


/*
 * Send a text message to a route.
 * The arguments to route_printf follow the printf() format and are 
 * implemented using vsnprint() and varargs.
 * Returns number of characters send to route [output of vsnprintf()].
 */
int route_printf(ROUTE rt,		/* Route */
		 const char* format, 	/* Format string; c.f. printf() */
		 ...			/* varargs */ )
{
     char msg_buffer[ROUTE_BUFSZ];
     int n;		/* Return values */
     va_list ap;

     if ( ! rt )
	  return 0;

     /* Create the string */
     va_start(ap, format);
     n = vsnprintf(msg_buffer, ROUTE_BUFSZ, format, ap);
     va_end(ap);
     
     /* Now, send it on its way */
     if (n > 0)
	  route_write(rt, msg_buffer, n);
     
     return n;
}

/*
 * Send a text message to a route and exit(1).
 * The arguments to route_die() follow the printf() format and are 
 * implemented using vsnprint() and varargs.
 * As route_printf, but never returns. Instead it carries out an exit(1)
 * when finished. If you want this to return, use route_printf().
 */
void route_die(ROUTE rt,		/* route */
	       const char* format,	/* format strings; cf string */
	       ...			/* varargs */ )
{
     char msg_buffer[ROUTE_BUFSZ];
     int n;		/* Return values */
     va_list ap;
     
     if ( ! rt )
	  return;

     /* Create the string */
     va_start(ap, format);
     n = vsnprintf(msg_buffer, ROUTE_BUFSZ, format, ap);
     va_end(ap);
     
     /* Now, send it on its way */
     if (n > 0)
	  route_write(rt, msg_buffer, n);
     
     exit(1);
}



/*
 * Open a pseudo-url source, get the last message and close it.
 * Different methods (channels opened by a p-url) read different quantities.
 *   ringstore - get the data from the last sequence
 *   file      - return the whole file.
 * Returns a pointer to a nmalloc'ed buffer if successful or NULL
 * if unable to get a message. Please free buffer after use.
 */
char *route_read(char *p_url,		/* pseudo url */
		 char *password,	/* password (if supported) */
		 int  *length		/* return length of message */ )
{
     ROUTE rt;
     int r, len, seq;
     time_t t;			/* Insertion time */
     ITREE *chain;
     char *buf;

     /* Open channel with insufficient parameters for creation */
     rt = route_open(p_url, NULL, password, 0);
     if (!rt)
	  return NULL;
     
     /* for this routine, we want the latest sequence or the whole file */
     r = route_tell(rt, &seq, &len, &t);
     if (!r) {
	  route_close(rt);
	  return NULL;
     }
     chain = rt->method->ll_read(rt->handle, seq, 0);
     if (!chain) {
	  route_close(rt);
	  return NULL;
     }
     route_close(rt);

     /* extract the data from the ROUTE_BUF chain */
     if (itree_n(chain) == 0)
          return NULL;
     itree_last(chain);
     buf = ((ROUTE_BUF *) itree_get(chain))->buffer;
     *length = ((ROUTE_BUF *) itree_get(chain))->buflen;
     nfree(itree_get(chain));
     itree_destroy(chain);

     return buf;
}

/*
 * Open a pseudo-url source, get the last message and close it.
 * Returns a TABLE if successful or NULL otherwise.
 * If the source is not able to provide a table format, data is held in
 * the column 'data'. The columns '_seq' and '_time' are present and set
 * if the source is able to provide the sequence number and time of insertion.
 */
TABLE route_tread(char *p_url,		/* pseudo url */
		  char *password	/* password (if supported) */ )
{
     ROUTE rt;
     int r, len, seq;
     time_t t;
     TABLE tab;

     /* Open channel with insufficient parameters for creation */
     rt = route_open(p_url, NULL, password, 0);
     if (!rt)
	  return NULL;
     
     /* for this routine, we want the latest sequence or the whole file */
     r = route_tell(rt, &seq, &len, &t);
     if (!r) {
	  route_close(rt);
	  return NULL;
     }
     tab = rt->method->ll_tread(rt->handle, seq, 0);
     route_close(rt);
     return tab;
}

/*
 * Get the maximum sequence or size of an already opened route.
 * On success, 1 is returned and `seq' is set to the current sequence 
 * number for sequence capable routes (or -1 if it's a file), 
 * `size' is set to the size of data (for file based routes)
 * and `modt' is set to the most recent modification time.
 * If there is an error, 0 is returned.
 * Ringstores return a sequence (size set to -1), files return a 
 * size (seq set to -1) and other routes that cannot support addressing
 * return size=seq=-1, modt=0.
 */
int route_tell(ROUTE rt,	/* opened route */
	       int *seq,	/* return: next sequence number of route */
	       int *size,	/* return: size of route */
	       time_t *modt	/* return: time of last update */ )
{
     int ret;

     /* be pessimistic -- prepare for failure */
     *size = -1;
     *seq  = -1;
     *modt = 0;

     if (!rt)
	  return 0;

     ret = rt->method->ll_tell(rt->handle, seq, size, modt);

     if (route_debug)
          /* use stderr to avoid loops */
	  elog_printf(DEBUG, "stat of %s: seq=%d, len=%d, time=%d\n", 
		      rt->p_url, *seq, *size, *modt);

     return ret;
}


/*
 * Get statictics about an unopened route.
 * On success, 1 is returned and `seq' is set to the latest sequence 
 * number for sequence capable routes (or -1 if it's a file), 
 * `size' is set to the size of data (for file based routes)
 * and `modt' is set to the most recent modification time.
 * If there is an error, 0 is returned.
 * Ringstores return a sequence (size set to -1), files return a 
 * size (seq set to -1) and other routes that cannot support addressing
 * return size=seq=-1, modt=0.
 */
int route_stat(char *purl,	/* purl of route */
	       char *password,	/* password for route */
	       int *seq,	/* return: last sequence number of route */
	       int *size,	/* return: size of route */
	       time_t *modt	/* return: time of last update */ )
{
     ROUTE rt;
     int ret;

     /* prepare for failure */
     *size = -1;
     *seq  = -1;
     *modt =  0;

     rt = route_open(purl, NULL, NULL, 0);
     if (!rt)
	  return 0;

     ret = route_tell(rt, seq, size, modt);
     route_close(rt);

     return ret;
}



/*
 * Seek to the location given by the parameters and read the data from that
 * point to the end. Position information is given in way that is as 
 * indepedendent and flexible as possible. The rules are:-
 *    FILES      only offset used
 *    RINGSTORES only seq used
 * Returns the data as an ITREE list of ROUTE_BUF'fers.
 * Files return a single element array containing data from `offset' onwards.
 * Ringstores and derivatives return each datum in a one element (as 
 * defined by their sequence).
 * In both cases, the data (in ROUTE_BUF.buffer) will be terminated so that
 * string functions may be used. This includes binary data.
 * Free list using route_free_routebuf().
 * Returns NULL if there is an error.
 */
ITREE *route_seekread(ROUTE rt,		/* route */
		      int seq,		/* read from sequence onwards */
		      int offset	/* read from offset */ )
{
     if (!rt)
	  return NULL;
     
     return rt->method->ll_read(rt->handle, seq, offset);
}



/*
 * Seek to the location given by the parameters and read the data from that
 * point to the end. Position information is given in way that is as 
 * indepedendent and flexible as possible. The rules are:-
 *    FILES      only offset used
 *    RINGSTORES only seq used
 * Returns a TABLE if successful or NULL otherwise.
 * If the source is not able to provide a table format, data is held in
 * the column 'data'. The columns '_seq' and '_time' are present and set
 * if the source is able to provide the sequence number and time of insertion.
 * Free data with table_destroy()
 */
TABLE route_seektread(ROUTE rt,		/* route */
		      int seq,		/* read from sequence onwards */
		      int offset	/* read from offset */ )
{
     if (!rt)
	  return NULL;
     
     return rt->method->ll_tread(rt->handle, seq, offset);
}



/* Free a list of ROUTE_BUF */
void route_free_routebuf(ITREE *chain	/* ROUTE_BUF list */ )
{
     itree_traverse(chain)
	  nfree(((ROUTE_BUF*) itree_get(chain))->buffer);
     itree_clearoutandfree(chain);
     itree_destroy(chain);
}

/*
 * Return a string containing the p-url of the open route, which will exist
 * as long as the route is open.
 */
char *route_getpurl(ROUTE rt) { return rt->p_url; }


#if TEST

#include "elog.h"
#include "rt_file.h"
#include "rt_std.h"
#define TRING1 "ts:t.route.dat,t1"
#define TFILE1 "t.route.dat"
#define TRING2 "tab:t.route.dat,t2"
#define TRING3 "tabt:t.route.dat,t2"
#define TRING4 "none:"

void testcb(char *msg, int msglen) {
     write(1, "callback: ", 10);
     write(1, msg, msglen);
     write(1, "\n", 1);
}

int main(int argc, char **argv) {
     int ir, from, to, dlen;
     char *data, *purl;
     ROUTE r, err;
     CF_VALS cf;

     cf = cf_create();
     route_init(cf, 0);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     elog_init(0, "route test", NULL);

     /* test 1 */
     r = route_open("stdout", NULL, NULL, 0);
     route_printf(r, "hello\n");
     purl = route_getpurl(r);
     if (strcmp("stdout", purl))
	  elog_die(ERROR, "[1] getpurl()==%s not stdout", purl);
     route_close(r);

     /* test 2 */
     unlink(TFILE1);
#if 0
     elog_send(DEBUG, "[2] expect 1 error next -->");
     r = route_open(TRING1, "test ring 1", NULL, 10);
     if ( ! r )
          elog_die(ERROR, "[2] unable to open");
     purl = route_getpurl(r);
     if (strcmp(TRING1, purl))
	  elog_die(ERROR, "[2] getpurl()==%s not " TRING1);
     route_printf(r, "hello");
     route_flush(r);
     route_printf(r, "there");
     route_close(r);
#endif

     /* test 3 callback output route */
#if 0
     route_cbregister("fred", testcb);
     r = route_open("cb:fred", "this is a callback test", NULL, 0);
     if ( ! r )
          elog_die(ERROR, "[3] unable to open");
     purl = route_getpurl(r);
     if (strcmp("cb:fred", purl))
	  elog_die(ERROR, "[3] getpurl()==%s not cb:fred");
     route_printf(r, "hello");
     route_flush(r);
     route_printf(r, "there");
     route_close(r);
     route_cbunregister("fred");
#endif

     /* test 4 tablestore output route */
#if 0
     r = route_open(TRING2, "test table ring 2", NULL, 10);
     if ( ! r )
          elog_die(ERROR, "[4] unable to open");
     purl = route_getpurl(r);
     if (strcmp(TRING2, purl))
	  elog_die(ERROR, "[4] getpurl()==%s not " TRING2);
     route_printf(r, "c1 c2 c3\n-- -- --\ntinkywinky mary daddy\n");
     if ( ! route_flush(r) )
          elog_die(ERROR, "[4a] unable to flush");
     route_printf(r, "c1 c2 c3\n");
     route_printf(r, "-- -- --\n");
     route_printf(r, "dipsy mungo mummy\n");
     route_printf(r, "lala midge baby\n");
     if ( ! route_flush(r) )
          elog_die(ERROR, "[4b] unable to flush");
     route_printf(r, "po\n");
     elog_send(ERROR, "[4c] expect several errors next -->");
     if ( route_flush(r) )
          elog_die(ERROR, "[4c] shouldn't be able to flush");
     route_close(r);
     r = route_open(TRING2, NULL, NULL, 0);
     if ( ! r )
          elog_die(ERROR, "[4d] unable to open");
     route_printf(r, "c1 c2 c3\n");
     route_printf(r, "-- -- --\n");
     route_printf(r, "tinkywinky mary daddy\n");
     route_printf(r, "still using the\n");
     route_printf(r, "previous columns but\n");
     route_printf(r, "it will be\n");
     route_printf(r, "a different session\n");
     if ( ! route_flush(r) )
          elog_die(ERROR, "[4e] unable to flush");
     route_close(r);
     r = route_open(TRING2, NULL, NULL, 0);
     if ( ! r )
          elog_die(ERROR, "[4f] unable to open");
     route_printf(r, "alpha beta gamma delta epsilon\n");
     route_printf(r, "-- -- -- -- --\n");
     route_printf(r, "fear and loathing in scotland\n");
     route_printf(r, "using a different header now\n");
     route_printf(r, "but it should still work\n");
     if ( ! route_flush(r) )
          elog_die(ERROR, "[4g] unable to flush");
     route_printf(r, "now a line with too many columns in it\n");
     elog_send(ERROR, "[4h] expect several errors next -->");
     if ( route_flush(r) )
          elog_die(ERROR, "[4i] shouldn't be able to flush");
     			/* check the presence of headers under the covers */
     route_close(r);
#endif
#if 0     
     /* test 5 type'd tablestore output route */
     r = route_open(TRING3, NULL, NULL, 10);
     if ( ! r )
          elog_die(ERROR, "[5] unable to open");
     purl = route_getpurl(r);
     if (strcmp(TRING3, purl))
	  elog_die(ERROR, "[5] getpurl()==%s not " TRING3);
     route_printf(r, "tinkywinky mary daddy\nint str str\n");
     if ( ! route_flush(r) )
          elog_die(ERROR, "[5a] unable to flush");
     route_printf(r, "dipsy mungo mummy\n");
     route_printf(r, "lala midge baby\n");
     if ( ! route_flush(r) )
          elog_die(ERROR, "[5b] unable to flush");
     route_printf(r, "po\n");
     elog_send(ERROR, "[5c] expect several errors next -->");
     if ( route_flush(r) )
          elog_die(ERROR, "[5c] shouldn't be able to flush");
     route_close(r);
#endif

     /* test 6 */
#if 0
     r = route_open(TRING4, "test p-url 4 to nowhere", NULL, 10);
     if ( ! r )
          elog_die(ERROR, "[6] unable to open");
     purl = route_getpurl(r);
     if (strcmp(TRING4, purl))
	  elog_die(ERROR, "[6] getpurl()==%s not " TRING4);
     route_printf(r, "hello");
     route_flush(r);
     route_printf(r, "there");
     route_close(r);
#endif

     /* shutdown for memory check */
     elog_fini();
     route_fini();
     cf_destroy(cf);
     fprintf(stderr, "%s: tests finished successfully\n", argv[0]);
     exit(0);
}
#endif /* TEST */
