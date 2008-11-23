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
#include "http.h"
#include "rt_sqlrs.h"

/* private functional prototypes */
RT_SQLRSD rt_sqlrs_from_lld(RT_LLD lld);
void rt_sqlrs_get_credentials(char *purl, TABLE *auth, CF_VALS *cookies, 
			      char **cookiejar);

const struct route_lowlevel rt_sqlrs_method = {
     rt_sqlrs_magic,      rt_sqlrs_prefix,     rt_sqlrs_description,
     rt_sqlrs_init,       rt_sqlrs_fini,       rt_sqlrs_access,
     rt_sqlrs_open,       rt_sqlrs_close,      rt_sqlrs_write,
     rt_sqlrs_twrite,     rt_sqlrs_tell,       rt_sqlrs_read,
     rt_sqlrs_tread
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
     rt->ringdesc = comment ? xnstrdup(comment) : NULL;
     rt->posttext = NULL;

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
 * A status line is returned as a result of the post together with
 * optional text to give further information. This data is held until the
 * next call to rt_sqlrs_write(); they can be retrieved by reading the
 * special p_urls of 'sqlrs:_WRITE_STATUS_' (defined to 
 * RT_SQLRS_WRITE_STATUS) and 'sqlrs:_WRITE_RETURN_' (defined to 
 * RT_SQLRS_WRITE_RETURN).
 * Data format is comma separated fat headed array: cvs fha and defined
 * by the habitat to harvest protocol.
 * Returns the number of characters written if successful or -1 for failure.
 * On failure, read 'sqlrs:_WRITE_STATUS_' to see why
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
     if (buflen > 1000) {
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
     tree_destroy(form);
     if (parts) 
          tree_destroy(parts);
     if (rt->posttext && strncmp(rt->posttext, "OK", 2) == 0) {
	  return buflen;
     } else {
          elog_printf(DIAG, "Repository rejected post: %s", rt->posttext);
	  return -1;
     }
}


/* Establish an SQLRS connection given the address provided in rt_sqlrs_open().
 * The write is carried out using an HTTP POST method.
 * If any text is returned as a result of the post, it is held until the
 * next call to rt_sqlrs_write(); it can be retrieved by reading the
 * special p_url of 'sqlrs:_WRITE_RETURN_', with caps and underlines 
 * preserved. Do not free this text, as it will be managed by rt_sqlrs.
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

     /* special locations that return the status of the last write action */
     if (strcmp(RT_SQLRS_WRITE_STATUS, rt->url) == 0) {
	  if (rt->posttext) {
	       len = strcspn(rt->posttext, "\n");
	       text = xnmemdup(rt->posttext, len+1);
	  } else {
	       len = 0;
	       text = xnmalloc(1);
	  }
	  text[len] = '\0';
     } else if (strcmp(RT_SQLRS_WRITE_RETURN, rt->url) == 0) {
	  if (rt->posttext) {
	       len = strcspn(rt->posttext, "\n");
	       text = xnstrdup(rt->posttext+len+1);
	  } else {
	       len = 0;
	       text = xnstrdup("");
	  }
     } else {
          /* normal connection */

          /* get authentication credentials */
          rt_sqlrs_get_credentials(rt->url, &auth, &cookies, &cookiejar);

          text = http_get(rt->geturl, cookies, cookiejar, auth, 0);

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
TABLE rt_sqlrs_tread  (RT_LLD lld,	/* route low level descriptor */
		       int seq,		/* IGNORED */
		       int offset	/* position to offset bytes before 
					 * returning data */)
{
     RT_SQLRSD rt;
     char *text, errtext[50];
     TABLE tab;
     int r, len;
     CF_VALS cookies;
     TABLE auth;
     char *cookiejar;

     rt = rt_sqlrs_from_lld(lld);

     if (strcmp(RT_SQLRS_WRITE_STATUS, rt->url) == 0) {
	  len = strcspn(rt->posttext, "\n");
	  text = util_strjoin("status\n--\n", rt->posttext, NULL);
	  text[len+10] = '\0';
     } else if (strcmp(RT_SQLRS_WRITE_RETURN, rt->url) == 0) {
	  len = strcspn(rt->posttext, "\n");
	  text = xnstrdup(rt->posttext+len+1);
     } else {
          /* normal connection */

          /* get authentication credentials */
          rt_sqlrs_get_credentials(rt->url, &auth, &cookies, &cookiejar);

	  /* carry out the fetch */
          text = http_get(rt->geturl, cookies, cookiejar, auth, 0);

	  /* free data */
	  if (auth) table_destroy(auth);
	  if (cookies) cf_destroy(cookies);
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
          len = strlen(text);
	  if (len > 0) {
	       strncpy(errtext, text, 50);
	       errtext[49] = '\0';
	       elog_printf(DIAG, "unable to parse table from text: %s%s "
			   "(length %d)",
			   errtext, len > 49 ? "...(truncated)" : "", len);
	  }

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
 * NEED TO REVIEW THE MEMORY ALLOCATIONS for TABLE an CF_VALS
*/
void rt_sqlrs_get_credentials(char *purl,	/* route name for diag msg */
			      TABLE *auth,	/* returned host configuration
						 * & authorisation table*/
			      CF_VALS *cookies,	/* returned cookies */
			      char **cookiejar	/* returned cookiejar fname */)
{
     char *auth_purl,    auth_expanded_purl[1024];
     char *cookies_purl, cookies_expanded_purl[1024];

     /* get the configuration details */
     if (rt_sqlrs_cf == NULL) {
          elog_printf(DIAG, "no configuration, can't get credentials");
	  *auth = NULL;
	  *cookies = NULL;
	  *cookiejar = NULL;
     } else {
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
		    route_expand(auth_expanded_purl, auth_purl, NULL, 0);
		    *auth = route_tread(auth_expanded_purl, NULL);
		    if (!*auth) {
		         elog_printf(DIAG, "Unable to read authorisation "
				     "route %s. Is it there? Is it readable?",
				     auth_expanded_purl);
		    } else {
		         if (table_ncols(*auth) == 1) {
			      /* its not a table, so attempt to parse it */
			 }

			 /* COULD BE TABULAR OR COULD BE A FLAT FILE (IN 
			  * WHICH CASE THERE WILL ON BE 'DATA' AS A COLUMN. 
			  * SO, IF ONLY DATA, ATTEMPT TO PARSE IT IN TO A 
			  * TABLE. IF THAT FAILS THEN AUTH MUST BE MARKED 
			  * AS NULL */
		    }
	       }
	  }
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
          *cookiejar = cf_getstr(rt_sqlrs_cf, RT_SQLRS_COOKIEJAR_FILEKEY);
	  if (*cookiejar == NULL) {
	       elog_printf(DIAG, "cookie jar configuration not found: %s, "
			   "proceeding with out the jar for %s",
			   RT_SQLRS_COOKIEJAR_FILEKEY, purl);
	  } else {
	       /* chop off any driver prefix given by mistake as CURL won't
	        * understand it */
	       if (strncmp(*cookiejar, "file:", 5) == 0)
	            *cookiejar += 5;
	       else if (strncmp(*cookiejar, "filea:", 6) == 0)
	            *cookiejar += 6;
	       else if (strncmp(*cookiejar, "fileov:", 7) == 0)
	            *cookiejar += 7;
	  }
     }
}



/*
 * Save cookies for use with the repository
 *
 * The cookies are held in a route pointed to by RT_SQLRS_COOKIES_URLKEY.
 * It will be saved in the habitata configuration text format.
 * It must not be sqlrs: or http:
 */


/*
 * Save host specific proxy configuration
 *
 * The cookies are held in a route pointed to by RT_SQLRS_COOKIES_URLKEY.
 * It will be saved in the habitata configuration text format.
 * It must not be sqlrs: or http:
 */


/* Save all the details you need to speak to a repository with sqlrs
 *
 * This routine finds the data locations from the main config (iiab_cf), 
 * opens each for writing and stores the supplied data if it can.
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
 * NEED TO REVIEW THE MEMORY ALLOCATIONS for TABLE an CF_VALS
*/
void rt_sqlrs_put_credentials(char *purl,	/* route name for diag msg */
			      TABLE *auth,	/* returned host configuration
						 * & authorisation table*/
			      CF_VALS *cookies,	/* returned cookies */
			      char **cookiejar	/* returned cookiejar fname */)
{
     char *auth_purl,    auth_expanded_purl[1024];
     char *cookies_purl, cookies_expanded_purl[1024];

     /* get the configuration details */
     if (rt_sqlrs_cf == NULL) {
          elog_printf(DIAG, "no configuration, can't get credentials");
	  *auth = NULL;
	  *cookies = NULL;
	  *cookiejar = NULL;
     } else {
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
		    route_expand(auth_expanded_purl, auth_purl, NULL, 0);
		    *auth = route_tread(auth_expanded_purl, NULL);
		    if (!*auth) {
		         elog_printf(DIAG, "Unable to read authorisation "
				     "route %s. Is it there? Is it readable?",
				     auth_expanded_purl);
		    } else {
		         if (table_ncols(*auth) == 1) {
			      /* its not a table, so attempt to parse it */
			 }

			 /* COULD BE TABULAR OR COULD BE A FLAT FILE (IN 
			  * WHICH CASE THERE WILL ON BE 'DATA' AS A COLUMN. 
			  * SO, IF ONLY DATA, ATTEMPT TO PARSE IT IN TO A 
			  * TABLE. IF THAT FAILS THEN AUTH MUST BE MARKED 
			  * AS NULL */
		    }
	       }
	  }
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
          *cookiejar = cf_getstr(rt_sqlrs_cf, RT_SQLRS_COOKIEJAR_FILEKEY);
	  if (*cookiejar == NULL) {
	       elog_printf(DIAG, "cookie jar configuration not found: %s, "
			   "proceeding with out the jar for %s",
			   RT_SQLRS_COOKIEJAR_FILEKEY, purl);
	  } else {
	       /* chop off any driver prefix given by mistake as CURL won't
	        * understand it */
	       if (strncmp(*cookiejar, "file:", 5) == 0)
	            *cookiejar += 5;
	       else if (strncmp(*cookiejar, "filea:", 6) == 0)
	            *cookiejar += 6;
	       else if (strncmp(*cookiejar, "fileov:", 7) == 0)
	            *cookiejar += 7;
	  }
     }
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
