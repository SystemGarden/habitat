/*
 * Route driver for sqlrs, a set of format conventions for http
 *
 * Nigel Stuckey, August 2003. 
 * Updates for harvest repository authentication January 2008
 * Copyright System Garden Ltd 2003-2008. All rights reserved
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
#include "route.h"
#include "http.h"
#include "rt_sqlrs.h"

/* private functional prototypes */
RT_SQLRSD rt_sqlrs_from_lld(RT_LLD lld);

const struct route_lowlevel rt_sqlrs_method = {
     rt_sqlrs_magic,      rt_sqlrs_prefix,     rt_sqlrs_description,
     rt_sqlrs_init,       rt_sqlrs_fini,       rt_sqlrs_access,
     rt_sqlrs_open,       rt_sqlrs_close,      rt_sqlrs_write,
     rt_sqlrs_twrite,     rt_sqlrs_tell,       rt_sqlrs_read,
     rt_sqlrs_tread,      rt_sqlrs_status
};
CF_VALS rt_sqlrs_cf;

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
		      char *comment,	/* comment, used as ring description */
		      char *password,	/* password, currently ignored */ 
		      int   keep,	/* flushed groups to keep, ignored */
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
     rt->ringdesc     = comment ? xnstrdup(comment) : NULL;
     rt->posttext     = NULL;

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
     nfree(rt->ringdesc);
     if (rt->posttext)
	  nfree(rt->posttext);
     nfree(rt);
}

/* Establish an SQLRS connection given the address provided in rt_sqlrs_open().
 * The write is carried out using an HTTP POST method.
 * A status line and optional information lines are returned as a result 
 * of the post, which can be retrived with route_status() [or directly with
 * rt_sqlrs_status()]. The buffer stays until the next write or twrite call
 * and errors are also sent to elog.
 * Do not free the error strings, as they will be managed by rt_sqlrs.
 * Data format is comma separated fat headed array: cvs fha and defined
 * by the habitat to harvest protocol.
 * Returns the number of characters written if successful or -1 for failure.
 * On failure, call rt_sqlrs_stat() to see why.
 */
int    rt_sqlrs_write (RT_LLD lld, const void *buf, int buflen)
{
     RT_SQLRSD rt;
     TREE *form, *parts;
     TABLE auth;
     char *cookiejar;
     CF_VALS cookies;

     rt = rt_sqlrs_from_lld(lld);

     /* is the buffer terminated? */
     if (((char *)buf)[buflen])
	  elog_die(FATAL, "buffer untruncated");

     /* get authentication credentials */
     rt_sqlrs_get_credentials(rt->url, &auth, &cookies, &cookiejar);

     /* compile the form - the route address (a) and host names are
      * provided in the url, but the description and ring length is not
      * and needs to be provided as additional form parameters.
      * (We don't bother with ring length currently as its managed 
      * independently by the repository, but this would be the place to put 
      * it) */
     form = tree_create();
     /*tree_add(form,  "a",           rt->addr);*/
     /*tree_add(form,  "host",        util_hostname());*/
     tree_add(form,  "description", rt->ringdesc);

     /* if the buffer is small, add it as a regular form parameter (updata),
      * or if its big then add it as a file upload (upfile). This is due to
      * efficiency */
     if (buflen > 1024) {
          parts = tree_create();
          tree_add(parts, "upfile", (void *) buf);
     } else {
          parts = NULL;
	  tree_add(form, "updata", (void *) buf);
     }

     /* clear the previous returned text, if any before starting next post */
     if (rt->posttext) {
	  nfree(rt->posttext);
	  rt->posttext = NULL;
     }

     /* post it */
     rt->posttext = http_post(rt->puturl, form, NULL, parts, cookies, 
			      cookiejar, auth, 0);

     /* free data */
     tree_destroy(form);
     if (parts) tree_destroy(parts);
     if (auth) table_destroy(auth);
     if (cookies) cf_destroy(cookies);
     if (cookiejar) nfree(cookiejar);

     /* deal with status reporting */
     if ( ! rt->posttext) {
          elog_printf(DIAG, "Repository gave no status, assume wider error "
		      "and rejection");
	  return -1;
     } else if (strncmp(rt->posttext, "OK", 2) == 0) {
	  return buflen;
     } else {
          elog_printf(DIAG, "Repository rejected post: %s", rt->posttext);
	  return -1;
     }
}


/* Establish an SQLRS connection given the address provided in rt_sqlrs_open().
 * The write is carried out using an HTTP POST method.
 * A status line and optional information lines are returned as a result 
 * of the post, which can be retrived with route_status() [or directly with
 * rt_sqlrs_status()]. The buffer stays until the next write or twrite call
 * and errors are also sent to elog.
 * Do not free the error strings, as they will be managed by rt_sqlrs.
 * Returns 1 for success or 0 for failure
 */
int    rt_sqlrs_twrite (RT_LLD lld,	/* route low level descriptor */
			TABLE tab	/* input table */)
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


/* Returns the current position in file, position in sequence and 
 * modification time.
 * Currently 'sqlrs:' is stateless, so the call will always succeed and
 * -1 (file), 0 (seq) will  be returned.
 * Returns 1 for success, 0 for failure */
int    rt_sqlrs_tell  (RT_LLD lld,	/* route low level descriptor */
		       int *seq, 	/* returned sequence */
		       int *size,	/* returned size */
		       time_t *modt	/* returned modification time */)
{
     *seq=0;
     *size=-1;
     *modt=-1;
     return 1;	/* always succeed at returning null */
}

/* Establish an SQLRS connection given the address provided in rt_sqlrs_open().
 * The read is carried out using an SQLRS GET method.
 * Sequence is ignored, but offset is honoured, returning data from
 * offset bytes.
 * Declarations in the configuration [passed in rt_sqlrs_init()] dictate
 * the proxy, user accounts, passwords, cookie environment and ssl tokens
 * so that it is hidden from normal use
 * Returns an ordered list of sequence buffers, unless no data is available
 * or there is an error, when NULL is returned.
 */
ITREE *rt_sqlrs_read  (RT_LLD lld,	/* route low level descriptor */
		       int seq,		/* IGNORED */
		       int offset	/* position to offset bytes before 
					 * returning data */)
{
     ROUTE_BUF *storebuf;
     ITREE *buflist;
     RT_SQLRSD rt;
     char *text;
     int len;
     CF_VALS cookies;
     TABLE auth;
     char *cookiejar;

     rt = rt_sqlrs_from_lld(lld);

     /* get authentication credentials */
     rt_sqlrs_get_credentials(rt->url, &auth, &cookies, &cookiejar);

     text = http_get(rt->geturl, cookies, cookiejar, auth, 0);

     /* free data */
     if (auth) table_destroy(auth);
     if (cookies) cf_destroy(cookies);
     if (cookiejar) nfree(cookiejar);

     if (!text)
	  return NULL;

     /* check for error: the convention is that errors start with 'ERROR\n' */
     if (strncmp(text, "ERROR\n", 6) == 0) {
          elog_printf(ERROR, "repository error: %s", text+6);
	  nfree(text);
          return NULL;	/* return NULL as not valid data... its an error */
     }

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
TABLE rt_sqlrs_tread  (RT_LLD lld,	/* route low level descriptor */
		       int seq,		/* IGNORED */
		       int offset	/* position to offset bytes before 
					 * returning data */)
{
     RT_SQLRSD rt;
     char *text, *copytext, errtext[150], *pt;
     TABLE tab;
     int r, len;
     CF_VALS cookies;
     TABLE auth;
     char *cookiejar;

     rt = rt_sqlrs_from_lld(lld);

     /* DEPRECATED */
     if (strcmp(RT_SQLRS_WRITE_STATUS, rt->url) == 0) {
          /* special token for the previous write status in a table */
	  len = strcspn(rt->posttext, "\n");
	  text = util_strjoin("status\n--\n", rt->posttext, NULL);
	  text[len+10] = '\0';
     } else if (strcmp(RT_SQLRS_WRITE_INFO, rt->url) == 0) {
          /* special token for the previous status (2nd) line */
	  len = strcspn(rt->posttext, "\n");
	  text = xnstrdup(rt->posttext+len+1);
	  len = strlen(rt->posttext);
     } else {
          /* normal connection */

          /* get authentication credentials */
          rt_sqlrs_get_credentials(rt->url, &auth, &cookies, &cookiejar);

	  /* carry out the fetch */
          text = http_get(rt->geturl, cookies, cookiejar, auth, 0);

	  /* free data */
	  if (auth) table_destroy(auth);
	  if (cookies) cf_destroy(cookies);
	  if (cookiejar) nfree(cookiejar);
     }
     if (!text)
	  return NULL;

     /* check for error: the convention is that errors start with 'ERROR\n' */
     if (strncmp(text, "ERROR\n", 6) == 0) {
          elog_printf(ERROR, "repository error: %s", text+6);
	  nfree(text);
          return NULL;
     }

     /* create the table, assuming headers exist and duplicate the first few
      * bytes in case we have to print an error (500 bytes) */
     copytext = xnstrndup(text, 500);
     tab = table_create();
     table_freeondestroy(tab, text);
     r = table_scan(tab, text, ",", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r < 0) {
	  /* table scanning error, so not valid data. 
	   * Text has been altered by table_scan(), so we look at copytext
	   * which contains a small copy of the data to extract an error.
	   * The error message is probably HTML (due to HTTP transport) and 
	   * is best viewed by an HTML browser.
	   * Possible this should be another elog use case?
	   * For text log, we rely on a 'large enough' sample of it */

          if (*copytext) {
	       /* there is text, delete HTML formatting and remove \n */
	       util_html2text(copytext);
	       util_strtrim(copytext);
	       for (pt=copytext; *pt; pt++) {
		    if (*pt == '\n')
		         *pt = '-';
	       }
	       elog_printf(ERROR, "Repository error: %s", copytext);
	  } else {
	       elog_printf(ERROR, "Empty data from repository");
	  }

	  nfree(copytext);
	  table_destroy(tab);
	  tab = NULL;
     } else if (r == 0) {
          elog_printf(DIAG, "No data from repository");
     }

     return tab;
}


/*
 * Return the status of an open SQLRS descriptor.
 * Free the data from status and info with nfree() if non NULL.
 * If no data is available, either or both status and info may return NULL
 */
void   rt_sqlrs_status(RT_LLD lld, char **status, char **info) {
     RT_SQLRSD rt;
     int len;

     rt = rt_sqlrs_from_lld(lld);

     if (status != NULL) {
          if (rt->posttext) {
	       len = strcspn(rt->posttext, "\n");
	       *status = xnmemdup(rt->posttext, len+1);
	       (*status)[len] = '\0';
	  } else {
	       *status = NULL;
	  }
     }

     if (info != NULL) {
          if (rt->posttext) {
	       len = strcspn(rt->posttext, "\n");
	       *info = xnstrdup(rt->posttext+len+1);
	  } else {
	       info = NULL;
	  }
     }
}

/* --------------- Private routines ----------------- */


RT_SQLRSD rt_sqlrs_from_lld(RT_LLD lld	/* typeless low level data */)
{
     if (!lld)
	  elog_die(FATAL, "passed NULL low level descriptor");
     if (((RT_SQLRSD)lld)->magic != RT_SQLRS_LLD_MAGIC)
	  elog_die(FATAL, "magic type mismatch: we were given "
		   "%s (%s) [%d] but can handle only %s (%s) [%d]", 
		   ((RT_SQLRSD)lld)->prefix, 
		   ((RT_SQLRSD)lld)->description,
		   ((RT_SQLRSD)lld)->magic,
		   rt_sqlrs_prefix(),  rt_sqlrs_description(),
		   RT_SQLRS_LLD_MAGIC);

     return (RT_SQLRSD) lld;
}


/* Return all the details you need to speak to a repository with sqlrs
 *
 * This routine finds the data locations from the main config (iiab_cf), 
 * opens each and processes them into standard Habitat data structures.
 *
 * Three data structures are returned:-
 * 1. Table of connection details and credentials (TABLE)
 * 2. Config of repository account details (CF_VALS)
 * 3. Cookiejar filename, used to store repository session key (char *)
 * Each is explained below
 *
 * Authorisation is held in a route pointed to by the config value
 * RT_SQLES_AUTH_URLKEY (the auth data is held away from the main config so 
 * its file permissions can be made read-only for user like ssh (mode 400) ).
 * It must be a table or be text that is parsable into a table. 
 * The route may NOT be sqlrs:, http: or https: to avoid infinite reursion.
 * Just in case, this routine will spot those driver prefixes and refuse the
 * call to route_tread(), thereby avoiding the loop and will return auth 
 * set to NULL.
 *
 * Cookie jar is a simple filename referring to local storage, not a purl route
 *
 * The cookies are held in a route pointed to by RT_SQLRS_COOKIES_URLKEY.
 * This should be free text in a configuration format, parasable by the
 * cf class. This config structure is then returned.
 * Again, it must not be sqlrs: or http:
 *
 * Auth should be freed with table_destroy() if non-NULL, cookies with 
 * cf_destroy() if non-NULL and cookiejar with nfree() if non-NULL.
 *
 * REFACTOR -- SHOULDN'T REALLY BE HERE, SHOULD BE IN A PROPERTIES FILE
 */
void rt_sqlrs_get_credentials(char *purl,	/* route name for diag msg */
			      TABLE *auth,	/* returned host configuration
						 * & authorisation table*/
			      CF_VALS *cookies,	/* returned cookies */
			      char **cookiejar	/* returned cookiejar fname */)
{
     char *auth_purl,    auth_expanded_purl[1024], *authtxt;
     char *cookies_purl, cookies_expanded_purl[1024];
     char *cookiejar_unexpanded, *cookiejar_expanded;
     int len, r;

     /* get the configuration details */
     if (rt_sqlrs_cf == NULL) {
          elog_printf(DIAG, "no SQLRS configuration, can't get credentials");
	  *auth = NULL;
	  *cookies = NULL;
	  *cookiejar = NULL;
     } else {
          /* get authorisation & connection details */
          auth_purl = cf_getstr(rt_sqlrs_cf, RT_SQLRS_AUTH_URLKEY);
	  if (auth_purl == NULL) {
	       elog_printf(DIAG, "authorisation configuration not found: %s, "
			   "proceeding without authorisation for %s", 
			   RT_SQLRS_AUTH_URLKEY, purl);
	       *auth = NULL;
	  } else {
	       /* prevent loops with ourself */
	       if (strncmp(auth_purl, "http:",  5) == 0 ||
		   strncmp(auth_purl, "https:", 6) == 0 ||
		   strncmp(auth_purl, "sqlrs:", 6) == 0) {
	            elog_printf(DIAG, "can't use HTTP based routes to find "
				"authentication for HTTP methods (%s=%s); "
				"loop avoided, proceeding without "
				"authentication configuration for %s", 
				RT_SQLRS_AUTH_URLKEY, auth_purl, purl);
		    *auth = NULL;
	       } else {
		    /* grab the data as text, then parse it as a table */
		    route_expand(auth_expanded_purl, auth_purl, NULL, 0);
		    authtxt = route_read(auth_expanded_purl, NULL, &len);
		    if (!authtxt) {
		         elog_printf(DIAG, "Unable to read authorisation "
				     "route %s. Is it there? Is it readable?",
				     auth_expanded_purl);
			 *auth = NULL;
		    } else {
		         *auth = table_create();
			 r = table_scan(*auth, authtxt, "\t", TABLE_SINGLESEP,
					TABLE_HASCOLNAMES, TABLE_HASRULER);
			 table_freeondestroy(*auth, authtxt);
 		    }
	       }
	  }

          /* get cookies, containing Harvest credentials */
          cookies_purl = cf_getstr(rt_sqlrs_cf, RT_SQLRS_COOKIES_URLKEY);
	  if (cookies_purl == NULL) {
	       elog_printf(DIAG, "cookie configuration not found: %s, "
			   "proceeding without configuration for %s", 
			   RT_SQLRS_COOKIES_URLKEY, purl);
		    *cookies = NULL;
	  } else {
	       /* prevent loops with ourself */
	       if (strncmp(cookies_purl, "http:",  5) == 0 ||
		   strncmp(cookies_purl, "https:", 6) == 0 ||
		   strncmp(cookies_purl, "sqlrs:", 6) == 0) {
	            elog_printf(DIAG, "can't use HTTP based routes to find "
				"authentication for HTTP methods (%s=%s); "
				"loop avoided, proceeding without "
				"authentication configuration for %s", 
				RT_SQLRS_COOKIES_URLKEY, cookies_purl, purl);
		    *cookies = NULL;
	       } else {
		    /* parse the text as a key-value configuration file */
		    *cookies = cf_create();
		    route_expand(cookies_expanded_purl, cookies_purl, NULL, 0);
		    if ( ! cf_scanroute(*cookies, NULL, 
					cookies_expanded_purl, 1)) {
		         /* unsuccessful parse */
			 cf_destroy(*cookies);
			 *cookies = NULL;
		    }
	       }
	  }

	  /* cookiejar file name */
          cookiejar_unexpanded = cf_getstr(rt_sqlrs_cf, 
					   RT_SQLRS_COOKIEJAR_FILEKEY);
	  if (cookiejar_unexpanded == NULL) {
	       elog_printf(DIAG, "cookie jar configuration not found: %s, "
			   "proceeding with out the jar for %s",
			   RT_SQLRS_COOKIEJAR_FILEKEY, purl);
	       *cookiejar = NULL;
	  } else {
	       /* chop off any driver prefix given by mistake as CURL won't
	        * understand it */
	       cookiejar_expanded = xnmalloc(strlen(cookiejar_unexpanded)+250);
	       if (strncmp(cookiejar_unexpanded, "file:", 5) == 0)
		    route_expand(cookiejar_expanded, cookiejar_unexpanded+5,
				 NULL, 0);
	       else if (strncmp(cookiejar_unexpanded, "filea:", 6) == 0)
		    route_expand(cookiejar_expanded, cookiejar_unexpanded+6,
				 NULL, 0);
	       else if (strncmp(cookiejar_unexpanded, "fileov:", 7) == 0)
		    route_expand(cookiejar_expanded, cookiejar_unexpanded+7,
				 NULL, 0);
	       else
		    route_expand(cookiejar_expanded, cookiejar_unexpanded, 
				 NULL, 0);
	       *cookiejar = cookiejar_expanded;
	  }
     }
}



/*
 * Save cookies for use with the repository
 *
 * The cookies are held in a route pointed to by RT_SQLRS_COOKIES_URLKEY.
 * It will be saved in the habitat configuration text format.
 * It must not be sqlrs: or http:
 *
 * Returns 1 for success, 0 for failure
 *
 * REFACTOR -- SHOULDN'T REALLY BE HERE, SHOULD BE IN A PROPERTIES FILE
 */
int rt_sqlrs_put_cookies_cred(char *purl,	/* route name for diag msg */
			     CF_VALS cookies	/* cookies */ )
{
     char *cookies_purl, cookies_expanded_purl[1024];
     int r;

     /* check we have data to save */
     if (tree_n(cookies) <= 0) {
          elog_printf(DEBUG, "no cookies to save");
	  return 1;	/* a success, but nothing saved */
     }

     /* get the configuration details */
     if (rt_sqlrs_cf == NULL) {
          elog_printf(DIAG, "no configuration, can't get credentials");
	  return 0;	/* failure */
     }

     cookies_purl = cf_getstr(rt_sqlrs_cf, RT_SQLRS_COOKIES_URLKEY);
     if (cookies_purl == NULL) {
          elog_printf(DIAG, "cookie configuration not found: %s, "
		      "unable to continue without configuration for %s", 
		      RT_SQLRS_COOKIES_URLKEY, purl);
	  return 0;	/* failure */
     }

     /* prevent loops with ourself and bail out */
     if (strncmp(cookies_purl, "http:",  5) == 0 ||
	 strncmp(cookies_purl, "https:", 6) == 0 ||
	 strncmp(cookies_purl, "sqlrs:", 6) == 0) {
          elog_printf(DIAG, "can't use HTTP based routes to find "
		      "authentication for HTTP methods (%s=%s); "
		      "loop avoided, proceeding without "
		      "authentication configuration for %s", 
		      RT_SQLRS_COOKIES_URLKEY, cookies_purl, purl);
	  return 0;	/* failure */
     }

     /* personalise the cookie url by expansion to get 
      * the user's homedir */
     route_expand(cookies_expanded_purl, cookies_purl, NULL, 0);
     cf_updatelines(cookies, cookies, cookies_expanded_purl, NULL);

     return 1;
}


/*
 * Save host specific proxy configuration and authentication
 *
 * The data will be saved in a text representation of a table.
 * 
 * Authorisation is held in a route pointed to by the config value
 * RT_SQLRS_AUTH_URLKEY (the auth data is held away from the main config so 
 * its file permissions can be made read-only for user like ssh (mode 400) ).
 * The route may NOT be sqlrs:, http: or https: to avoid infinite reursion.
 * Just in case, this routine will spot those driver prefixes and refuse the
 * call to route_twrite(), thereby avoiding the loop.
 * Attemtps to make sure the file is mode 600, u=rw only.
 *
 * Returns 1 for success, 0 for failure
 *
 * REFACTOR -- SHOULDN'T REALLY BE HERE, SHOULD BE IN A PROPERTIES FILE
 */
int rt_sqlrs_put_proxy_cred(char *purl,		/* route name for diag msg */
			    TABLE proxy		/* proxy table */ )
{
     char *proxy_purl, proxy_expanded_purl[1024];
     ROUTE rt;
     int r;
     mode_t old_mode;

     /* check we have data to save */
     if (table_nrows(proxy) <= 0) {
          elog_printf(DEBUG, "no proxy details to save");
	  return 1;	/* a success, but nothing saved */
     }

     /* get the configuration details */
     if (rt_sqlrs_cf == NULL) {
          elog_printf(DIAG, "no SQLRS configuration, can't get credentials");
	  return 0;	/* failure */
     }

     proxy_purl = cf_getstr(rt_sqlrs_cf, RT_SQLRS_AUTH_URLKEY);
     if (proxy_purl == NULL) {
          elog_printf(DIAG, "authorisation configuration not found: %s, "
		      "unable to continue saving proxy details for %s", 
		      RT_SQLRS_AUTH_URLKEY, purl);
	  return 0;	/* failure */
     }

     /* prevent loops with ourself */
     if (strncmp(proxy_purl, "http:",  5) == 0 ||
	 strncmp(proxy_purl, "https:", 6) == 0 ||
	 strncmp(proxy_purl, "sqlrs:", 6) == 0) {
          elog_printf(DIAG, "can't use HTTP based routes to find "
		      "authentication for HTTP methods (%s=%s); "
		      "loop avoided, proceeding without "
		      "authentication configuration for %s", 
		      RT_SQLRS_AUTH_URLKEY, proxy_purl, purl);
	  return 0;	/* failure */
     }

     /* personalise the proxy url by expansion to get 
      * the user's homedir */
     old_mode = umask(S_IRWXG | S_IRWXO | S_IXUSR);	/* u=rw, mode 600 */
     route_expand(proxy_expanded_purl, proxy_purl, NULL, 0);
     rt = route_open(proxy_expanded_purl, "Proxy config information", NULL, 
		     10);
     if (rt == NULL) {
          elog_printf(ERROR, "unable to open proxy configuration file for "
		      "writing: %s", proxy_expanded_purl);
	  return 0;	/* failure */
     }
     r = route_twrite(rt, proxy);
     umask(old_mode);
     route_close(rt);

     return r;
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
