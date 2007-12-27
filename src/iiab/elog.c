/*
 * Event log class (elog)
 *
 * Implements a event logging mechanism on top of the route class.
 * All events are classified by their origin and severity.
 * Origin is set by the programmer and severity may be one of five levels:
 * debug, info, warning, error and fatal. Each level or groups of levels may 
 * directed to different places as allowed by the route class.
 * Elog also has the ability of overriding the programmed severity levels
 * by use of patterns. Thus events may be upgraded or down graded in severity.
 * Finally, the setting of severity routes and overrinding patterns may be
 * specified in a file and parsed by evelog, thus making the behaviour
 * of a program externally configurable.
 *
 * Nigel Stuckey, May 1998. 
 * Modified Jan 1999 with flushes
 * Modified Jan 2000 to add the DIAG severity
 * Modified Apr 2000 to add elog_safe()
 * Copyright System Garden Limited 1998-2001. All rights reserved
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <regex.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "nmalloc.h"
#include "elog.h"
#include "route.h"
#include "rt_std.h"
#include "tree.h"
#include "itree.h"
#include "util.h"
#include "table.h"

ROUTE  elog_errors;	/* route to which default errors should be sent.
			 * this must be a `safe' route, one which would cause
			 * the minimum of further problems. Typically stderr */
int    elog_debug;	/* 1=debug 0=don't debug */
char  *elog_origin;	/* string decsription of event's software origin */
pid_t  elog_pid;	/* originating pid */
char  *elog_pname;	/* originating process name */
/*ROUTE  elog_route;*/	/* array of routes for different severities */
TREE  *elog_override;	/* list of elog_overridedat (compiled pattern-
			 * severity pairs) indexed by string pattern */
struct elog_overridedat {
     regex_t pattern;			/* compiled pattern */
     enum elog_severity severity;	/* severity code */
};
int    elog_isinit=0;	/* has elog been initialised? */

/* standard format strings */
char *elog_stdfmt[] = {
     ELOG_FMT1,
     ELOG_FMT2,
     ELOG_FMT3,
     ELOG_FMT4,
     ELOG_FMT5,
     ELOG_FMT6,
     ELOG_FMT7,
     ELOG_FMT8
};

/* Severity strings */
char *elog_sevstring[]={"nosev","debug","diag","info","warning","error",
			"fatal",NULL};
char elog_sevclower[] ={'?',    'd',    'g',   'i',   'w',      'e', 'f', 0};
char elog_sevcupper[] ={'?',    'D',    'G',   'I',   'W',      'E', 'F', 0};

/* status table header string */
char *elog_colnames[] = {"severity", "route", "format", NULL};

/*
 * Initialises the elog class.
 * Sets a default route for all severities (stderr) until it is
 * configured otherwise.
 * If cf is set, an attempt is made to configure elog using the cf_*
 * routines and that tree.
 * Relies on the route class, which must be initialised before calling.
 * Note, that error raising functions can be used with out initialising
 * elog, but they are all send to stderr. However, no configuration can
 * take place with out initialising properly.
 * Returns 1 for success, 0 for failure.
 */
int elog_init(int debug,	/* debug flag */
	      char *binname,	/* binary name, argv[0] */
	      CF_VALS cf	/* configuration structure */)
{
     int i;

     if (elog_isinit)
          return 1;	/* already initialised */

     elog_errors = route_open("stderr:", "default error", NULL, 0);
     elog_debug = debug;
     elog_origin = xnstrdup("");
     elog_pid = getpid();
     elog_pname = binname;

     /* initialise to NULL */
     for (i=0; i < ELOG_NSEVERITIES; i++) {
	  elog_opendest[i].purl   = NULL;
	  elog_opendest[i].route  = NULL;
	  elog_opendest[i].format = NULL;
     }

     elog_isinit++;

     /* set a base default, which is to log everything */
     if ( ! elog_setallroutes(elog_errors)) {
	  if (debug)
	       route_printf(elog_errors, 
			 "elog_init() unable to set all routes to default");
	  return 0;
     }
     if ((elog_override = tree_create()) == NULL) {
	  if (debug)
	       elog_printf(ERROR, "elog_init() unable to create override tree");
	  return 0;
     }

     /* if supplied, configure from the cf tree */
     if (cf)
          elog_configure(cf);

     return 1;
}

/* if elog has not been initialised, generate a fatal error and die */
void elog_checkinit() {
     if ( ! elog_isinit) {
          fprintf(stderr, "elog_checkinit() elog not initialised, cannot "
		  "continue\n");
	  exit(1);
     }
}

/* finalise and deconstruct the eventlog */
void elog_fini() {
     int i, j;

     elog_checkinit();

     /* close any open purls, that is severities for which elog_ opened
      * a route. Routes present in elog_opendest[] without purl values
      * were suplied externally and it is their responsibility to clear up! */
     for (i=0; i < ELOG_NSEVERITIES; i++)
          if (elog_opendest[i].purl) {
	       /* is there another severity with the same purl?
		* If so, clear that severity */
	       for (j=i+1; j < ELOG_NSEVERITIES; j++) {
		    if ( elog_opendest[j].purl &&
			 strcmp(elog_opendest[j].purl, 
				elog_opendest[i].purl) == 0 ) {
		         nfree(elog_opendest[j].purl);
			 elog_opendest[j].purl = NULL;
			 elog_opendest[j].route = NULL;
		    }
	       }

	       /* close old route opened and owned by us */
	       route_close(elog_opendest[i].route);
	       nfree(elog_opendest[i].purl);
	       elog_opendest[i].purl = NULL;
	       elog_opendest[i].route = NULL;
	  }

     /* free any format strings */
     for (i=0; i < ELOG_NSEVERITIES; i++)
          if (elog_opendest[i].format) {
	       nfree(elog_opendest[i].format);
	       elog_opendest[i].format = NULL;
	  }

     tree_clearoutandfree(elog_override);
     tree_destroy(elog_override);
     if (elog_origin)
          nfree(elog_origin);
     route_close(elog_errors);
}

/* set origin part of event message */

void elog_setorigin(char *origin	/* string identifing origin */ )
{
     elog_checkinit();

     if (elog_origin)
	  nfree(elog_origin);
     elog_origin = xnstrdup(origin);
}

/* Set a route for a given severity to be an already opened ROUTE supplied 
 * by the caller.
 * If a route already exists for that severity, it will NOT be closed
 * before being overwritten, unless elog_ had opened the route from a purl
 * (with elog_setsevpurl()) and it is not in use for any other severity. 
 * If that is the case, then the route will be closed.
 * Returns 1 for success, 0 for failure
 */
int elog_setsevroute(enum elog_severity severity,	/* severity level */
		     ROUTE route			/* opened route */ )
{
     int i;

     elog_checkinit();

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0)
	  return 0;

     /* Do we have to bother to clear up? */
     if (elog_opendest[severity].purl != NULL) {
          /* elog_ opened this routine with setsevpurl(); 
	   * is there another severity with the same route? */
	  for (i=0; i < ELOG_NSEVERITIES; i++) {
	       if (i == severity)
		    continue;		/* don't count ourselves */
	       if (elog_opendest[severity].route == elog_opendest[i].route) {
		    nfree(elog_opendest[severity].purl);
		    goto noclose;	/* route is used elsewhere */
	       }
	  }

	  /* we have to clearup the old route as we allocated it but
	   * no other severity is using it */
	  route_close(elog_opendest[severity].route);
	  nfree(elog_opendest[severity].purl);
     }

noclose:
     elog_opendest[severity].purl = NULL;
     elog_opendest[severity].route = route;

     return 1;
}

/* set a route for a severity, as _setsevroute(). But open a purl to
 * obtain the route. If the route is already known, reuse it.
 * return 0 for failure or 1 for success.
 */
int elog_setsevpurl(enum elog_severity severity,	/* severity level */
		    char *purl				/* purl of route */ )
{
     ROUTE lroute;
     int i;

     elog_checkinit();

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0)
	  return 0;

     /* do we already have this purl open? */
     if ( elog_opendest[severity].purl && 
	  strcmp(purl, elog_opendest[severity].purl) == 0 )
	  return 1;

     /* scan already open routes to reuse them */
     for (i=0; i < ELOG_NSEVERITIES; i++) {
	  if (elog_opendest[i].purl && 
	      strcmp(purl, elog_opendest[i].purl) == 0 ) {
	       /* a match is found, reuse it */
	       elog_opendest[severity].purl = 
		    xnstrdup(elog_opendest[i].purl);
	       elog_opendest[severity].route = elog_opendest[i].route;
	       return 1;
	  }
     }

     /* no existing open route: attempt to open a new one */
     lroute = route_open(purl, "Event log", NULL, ELOG_KEEPDEF);
     if ( ! lroute) {
          elog_printf(ERROR, "unable to open %s to log errors\n", purl);
	  return 0;
     }
/*     lpurl = xnstrdup(purl);*/

     /* now close down the existing route for this severity */
     if (elog_opendest[severity].purl) {
	  /* is there another severity with the same purl? */
	  for (i=0; i < ELOG_NSEVERITIES; i++) {
	       if (i == severity)
		    continue;		/* don't count ourselves */
	       if (elog_opendest[i].purl &&
		   strcmp(elog_opendest[severity].purl,
			  elog_opendest[i].purl) == 0) {
		    nfree(elog_opendest[severity].purl);
		    goto noclose2;
	       }
	  }

	  /* we have to clearup the old route as we allocated it but
	   * no other severity is using it */
	  route_close(elog_opendest[severity].route);
	  nfree(elog_opendest[severity].purl);
     }

noclose2:

     /* comit the allocation */
     elog_opendest[severity].purl = xnstrdup(purl);
     elog_opendest[severity].route = lroute;

     return 1;
}

/*
 * set all routes to the already opened ROUTE route
 * returns 0 if there was an error doing so
 */
int elog_setallroutes(ROUTE route	/* opened route */ )
{
     int i;

     elog_checkinit();

     for (i=0; i < ELOG_NSEVERITIES; i++)
	  if ( ! elog_setsevroute(i, route)) {
	       elog_printf(ERROR, "can't setsevroute(%d,%x)", i, route);
	       return 0;
	  }
     return 1;
}

/*
 * open the purl as a route and set all severity levels to output to it
 * returns 0 if there was an error doing so
 */
int elog_setallpurl(char *purl	/* purl of route */ )
{
     int i;

     elog_checkinit();

     for (i=0; i < ELOG_NSEVERITIES; i++)
	  if ( ! elog_setsevpurl(i, purl)) {
	       elog_printf(ERROR, "can't setsevpurl(%d,%s)", i, purl);
	       return 0;
	  }
     return 1;
}


/* sets routes below and including the severity to use ROUTE as their
 * new routes. if routes already exist, they will NOT be closed, but their
 * reference will be overwritten.
 * returns 1 if successful, 0 otherwise.
 */
int elog_setbelowroute(enum elog_severity severity,	/* severity level */
		       ROUTE route			/* opened route */ )
{
     int i;

     elog_checkinit();

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0)
	  return 0;

     for (i=0; i <= severity; i++)
	  if ( ! elog_setsevroute(i, route)) {
	       elog_printf(ERROR, "can't setsevroute(%d,%x)", i, route);
	       return 0;
	  }

     return 1;
}

/* sets severities below and including the argument severity to use purl
 * as their new routes. if an overwritten route was opened from a ...purl()
 * invokation [rather than a ...route() invocation] then it will be closed
 * if it is the only reference left
 * returns 1 if successful, 0 otherwise.
 */
int elog_setbelowpurl(enum elog_severity severity,	/* severity level */
		      char *purl			/* purl string */ )
{
     int i;

     elog_checkinit();

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0)
	  return 0;

     for (i=0; i <= severity; i++)
	  if ( ! elog_setsevpurl(i, purl)) {
	       elog_printf(ERROR, "can't setsevpurl(%d,%s)", i, purl);
	       return 0;
	  }

     return 1;
}

/* Sets routes above and including the severity to use ROUTE as their
 * new routes. if a route already exists, it will NOT be closed, but its 
 * reference will be overwritten
 * returns 1 if successful, 0 otherwise.
 */
int elog_setaboveroute(enum elog_severity severity,	/* severity level */
		       ROUTE route			/* opened route */ )
{
     int i;

     elog_checkinit();

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0)
	  return 0;

     for (i=severity; i <= ELOG_NSEVERITIES; i++)
	  if ( ! elog_setsevroute(i, route)) {
	       elog_printf(ERROR, "can't setsevroute(%d,%x)", i, route);
	       return 0;
	  }

     return 1;
}

/* Sets routes above and including the severity to use purl as their
 * new routes. if an overwritten route was opened from a ...purl() invokation 
 * [rather than a ...route() invokation] then it will be closed if it is the
 * only reference left
 * returns 1 if successful, 0 otherwise.
 */
int elog_setabovepurl(enum elog_severity severity,	/* severity level */
		      char *purl			/* purl string */ )
{
     int i;

     elog_checkinit();

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0)
	  return 0;

     for (i=severity; i < ELOG_NSEVERITIES; i++)
	  if ( ! elog_setsevpurl(i, purl)) {
	       elog_printf(ERROR, "can't setsevpurl(%d,%s)", i, purl);
	       return 0;
	  }

     return 1;
}


/* Set a severity to use the given sprintf output format.
 * Returns 1 for successful or 0 for failure 
 */
int elog_setformat(enum elog_severity severity, char *format)
{
     elog_checkinit();

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0)
	  return 0;

     if (elog_opendest[severity].format)
	  nfree(elog_opendest[severity].format);
     elog_opendest[severity].format = xnstrdup(format);

     return 1;
}

/* Set a severity to use the given sprintf output format.
 * Returns 1 for successful or 0 for failure 
 */
int elog_setallformat(char *format)
{
     int i;

     elog_checkinit();

     for (i=0; i < ELOG_NSEVERITIES; i++)
	  if ( ! elog_setformat(i, format) ) {
	       elog_printf(ERROR, "can't setformat(%d,%s)", i, format);
	       return 0;
	  }
     return 1;
}



/* sets a pattern to override the severity stated by the program
 * or application for an error. The pattern is applied to the event text
 * each time _send() is called. Each override will take some space
 * so unused ones should be discarded with _rmoverride().
 * returns 1 if successfully added to list, or 0 if unable to add.
 */
int elog_setoverride(enum elog_severity severity,	/* severity level */
		     char *re_pattern			/* reg exp pattern */ )
{
     int r;
     char errbuf[ELOG_STRLEN];
     struct elog_overridedat *over;

     elog_checkinit();

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0)
	  return 0;

     over = xnmalloc(sizeof(struct elog_overridedat));
     if ((r = regcomp(&over->pattern, re_pattern, (REG_EXTENDED|REG_NOSUB)))) {
	  regerror(r, &over->pattern, errbuf, ELOG_STRLEN);
	  elog_printf(ERROR, "elog_setoverride() problem with key "
		      "pattern: %s\nError is %s\n", re_pattern, errbuf);
	  nfree(over);
	  return 0;
     }

     over->severity = severity;
     tree_add(elog_override, xnstrdup(re_pattern), over);

     return 1;
}

/*
 * remove a pattern previously set by _setoverride().
 * returns 1 if pattern was found and successfully deleted or 0 if there
 * was no pattern
 */
int elog_rmoverride(char *re_pattern	/* regular expression pattern */ )
{
     struct elog_overridedat *over;

     elog_checkinit();

     if ((over = tree_find(elog_override, re_pattern)) != TREE_NOVAL) {
	  regfree(&over->pattern);
	  nfree(over);
	  nfree(tree_getkey(elog_override));
	  tree_rm(elog_override);

	  return 1;
     }

     return 0;
}


/*
 * Configure the elog class from the CF_VALS, using the cf_* routines.
 * The format of each configuration statement should be:-
 *    elog.all       <route>
 *    elog.above     <severity> <route>
 *    elog.below     <severity> <route>
 *    elog.set       <severity> <route>
 *    elog.format    <severity> <format>
 *    elog.allformat <format>
 *    elog.pattern   <severity> <regexp> 
 * Routes are expanded [with cf_expand()] before using as a directve.
 * The configurations amend the current settings. Note that "elog" leading
 * the config ids is defined (and may be changed) by ELOG_CFPREFIX.
 */
void elog_configure(CF_VALS cf)
{
     char *val1, *val2, val_t[1024];
     ITREE *args;

     elog_checkinit();

     /* When a config is stored in a CF_VALS structure, we see
      *    VEC  KEY         VAL
      *    0    elog.all    <route>
      *    1    elog.above  <severity> <route>
      *    ...etc
      */
     val1 = cf_getstr(cf, ELOG_CFPREFIX ".all");
     if (val1) {
	  route_expand(val_t, val1, "NOJOB", 0);
          elog_setallpurl(val_t);
     }

     args = cf_getvec(cf, ELOG_CFPREFIX ".above");
     if (args) {
	  if (itree_n(args) != 2)
	       elog_printf(ERROR, "command " ELOG_CFPREFIX ".above: "
			   "%d arguments supplied when 3 required\n",
			   itree_n(args)+1);
	  else {
	       itree_first(args); val1 = itree_get(args);
	       itree_next(args);  val2 = itree_get(args);
	       route_expand(val_t, val2, "NOJOB", 0);
	       elog_setabovepurl(elog_strtosev(val1), val_t);
	  }
     }

     args = cf_getvec(cf, ELOG_CFPREFIX ".below");
     if (args) {
	  if (itree_n(args) != 2)
	       elog_printf(ERROR, "command " ELOG_CFPREFIX ".below: "
			   "%d arguments supplied when 3 required\n",
			   itree_n(args)+1);
	  else {
	       itree_first(args); val1 = itree_get(args);
	       itree_next(args);  val2 = itree_get(args);
	       route_expand(val_t, val2, "NOJOB", 0);
	       elog_setbelowpurl(elog_strtosev(val1), val_t);
	  }
     }

     args = cf_getvec(cf, ELOG_CFPREFIX ".set");
     if (args) {
	  if (itree_n(args) != 2)
	       elog_printf(ERROR, "command " ELOG_CFPREFIX ".set: "
			   "%d arguments supplied when 3 required\n",
			   itree_n(args)+1);
	  else {
	       itree_first(args); val1 = itree_get(args);
	       itree_next(args);  val2 = itree_get(args);
	       route_expand(val_t, val2, "NOJOB", 0);
	       elog_setsevpurl(elog_strtosev(val1), val_t);
	  }
     }

     args = cf_getvec(cf, ELOG_CFPREFIX ".format");
     if (args) {
	  if (itree_n(args) != 2)
	       elog_printf(ERROR, "command " ELOG_CFPREFIX ".format: "
			   "%d arguments supplied when 3 required\n",
			   itree_n(args)+1);
	  else {
	       itree_first(args); val1 = itree_get(args);
	       itree_next(args);  val2 = itree_get(args);
	       elog_setformat(elog_strtosev(val1), val2);
	  }
     }

     val1 = cf_getstr(cf, ELOG_CFPREFIX ".allformat");
     if (val1)
          elog_setallformat(val1);

     args = cf_getvec(cf, ELOG_CFPREFIX ".pattern");
     if (args) {
	  if (itree_n(args) != 2)
	       elog_printf(ERROR, "command " ELOG_CFPREFIX ".pattern: "
			   "%d arguments supplied when 3 required\n",
			   itree_n(args)+1);
	  else {
	       itree_first(args); val1 = itree_get(args);
	       itree_next(args);  val2 = itree_get(args);
	       elog_setoverride(elog_strtosev(val1), val2);
	  }
     }

     return;
}

/*
 * Takes a string and attempts to match it against all possible severity 
 * strings, returning the index enum elog_severity. 
 * Returns NOELOG if no match was found.
 */
enum elog_severity elog_strtosev(char *sevstring)
{
     char candidate[ELOG_STRLEN], *pt, *ptlower;
     int sev;

     /* make duplicate lowercase string */
     ptlower = candidate;
     for (pt = sevstring; *pt != '\0'; pt++)	/* lowercase string */
          *ptlower++ = tolower(*pt);
     *ptlower = '\0';

     /* check against severity strings */
     for (sev=0; elog_sevstring[sev] != NULL; sev++)
          if ( strcmp(elog_sevstring[sev], candidate) == 0)
	       break;

     /* no match */
     if (elog_sevstring[sev] == NULL)
          return NOELOG;
     else
          return sev;
}


/* output the string associcated with the severity. Dont attempt to free
 * the string */
char *elog_sevtostr(enum elog_severity sev)
{
     return elog_sevstring[sev];
}

/*
 * Send an event, suggesting a given severity level and origin string.
 * The severity must be given but the origin may be NULL, in which case
 * the default supplied by _setorigin() will be used.
 * The event details are represented by a code and string, which may be
 * 0 for the code, NULL or "" for the string.
 * The event will be raised at the specifed severity, unless the log text
 * matches a regular expression entered in _setoverride(), in which case
 * the overriding severity will be used.
 * Data is output in the following order, which may be formatted with
 * a suitable printf format string [see printf(3)]:-
 *
 *    1. s 22 `DEC' style date and time
 *    2. s 28 Unix style date and time
 *    3. s  9 Short, adaptive date and time 
 *    4. d    Seconds since the `epoch' (Jan 1 1970)
 *    5. s  7 Severity string
 *    6. c  1 Severity character in lower case
 *    7. c  1 Severity character in upper case
 *    8. s    Short process name (stripped of file path)
 *    9. s    Full process name (may include file path)
 *   10. d    Process ID
 *   11. d    Thread ID (where applicable: not linux)
 *   12. s    File in which log was raised
 *   13. s    Function in which log was raised
 *   14. d    Line number of initial error function
 *   15. s    Origin sent by application
 *   16. d    Log code sent by application
 *   17. s    Log text sent by application
 *
 * if any spaces are required in the process name or origin, they should
 * be quoted "thus". logtext may include spaces without being quoted, 
 * but no newlines as the field terminates at the end of its line.
 * This version does not flush the route, but holds the text pending
 * in the route_ class. Use _contsend() to continue the same log and
 * _endsend() to finalize the message and send it.
 * Note that is another log is sent for the same route with this message
 * pending, the text will be jumbled with the other log.
 * Returns 1 success or 0 for failure.
 */
int elog_fstartsend(enum elog_severity severity,/* severity level */
		    char *file,			/* raised in file */
		    int line,			/* raised fron line number */
		    const char *function,	/* raised in function */
		    char *logtext		/* log a string */ )
{
     int r;
     char *ctimestr, *ctimenl;
     time_t logtime;

     /* to stderr if uninitialised */
     if ( ! elog_isinit) {
	  fprintf(stderr, "%s: %s (%s:%d:%s)", elog_sevstring[severity], 
		  logtext, file, line, function);
	  return 1;
     }

     /* get time */
     logtime = time(NULL);
     ctimestr = ctime(&logtime);
     ctimenl = strchr(ctimestr, '\n');	/* naughty!! */
     *ctimenl = '\0';			/* patch the returned static */

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0) {
	  elog_printf(ERROR, "elog_fstartsend(): severity %d out of "
		      "range (0..%d). original error is:-\n"
		      "location: %s:%s:%d\n"
		      "text:     %s\n",
		      severity, ELOG_NSEVERITIES-1, 
		      file, function, line, logtext);
	  return 0;
     }

     r = route_printf(elog_opendest[severity].route, 
		         (elog_opendest[severity].format ? 
			  elog_opendest[severity].format : ELOG_DEFFORMAT), 
		      util_decdatetime(logtime),/* DEC datetime */
		      ctimestr,			/* unix datetime */
		      util_shortadaptdatetime(logtime),	/* short datetime */
		      logtime,			/* epoch time */
		      elog_sevstring[severity], /* severity string */
		      elog_sevclower[severity], /* severity char low */
		      elog_sevcupper[severity], /* severity char up */
		      util_basename(elog_pname),/* short process name*/
		      elog_pname,		/* long process name*/
		      elog_pid,			/* process id */
		      0, 			/* thread id */
		      file,			/* file name */
		      function,			/* function */
		      line,			/* line number */
		      util_nonull(elog_origin),	/* origin string */
		      0,			/* code to log */
		      util_nonull(logtext));	/* text to log */

     if (r <= 0)
	  return 0;

     return 1;
}

/* continue a log message enstablished by a _startsend or _startprintf() */
void elog_fcontsend(enum elog_severity severity, char *logtext) 
{
     /* to stderr if uninitialised */
     if ( ! elog_isinit) {
	  fprintf(stderr, "%s", logtext);
	  return;
     }

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0) {
          elog_printf(ERROR, "elog_contsend(): severity out of range\n");
	  return;
     }

     route_printf(elog_opendest[severity].route, "%s", logtext);
}

/* terminate a log message enstablished by a _startsend or _startprintf() */
void elog_fendsend(enum elog_severity severity, char *logtext)
{
     /* to stderr if uninitialised */
     if ( ! elog_isinit) {
	  fprintf(stderr, "%s\n", logtext);
	  return;
     }

     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0) {
          elog_printf(ERROR, "elog_endsend(): severity out of range\n");
	  return;
     }

     route_printf(elog_opendest[severity].route, "%s\n", logtext);
     if ( route_flush(elog_opendest[severity].route) != 1)
          /* not a fatal failure, we may be able to send it again
	   * flag this as an error to stderr so as not to get in an
	   * infinite loop */
       fprintf(stderr, "elog_endsend(): route_flush() failed: %s: %s\n",
	       elog_opendest[severity].purl, logtext);
}

/*
 * As elog_pending(), but flushes the output. you do not need to call any
 * other other method to send the log
 */
int elog_fsend(enum elog_severity severity,	/* severity level */
	       char *file,			/* raised in file */
	       int line,			/* raised fron line number */
	       const char *function,		/* raised in function */
	       char *logtext			/* log a string */ )
{
     int r;

     r = elog_fstartsend(severity, file, line, function, logtext);
     elog_fendsend(severity, "");
     return r;
}


/*
 * Send a pending event using printf style formatting without flushing the
 * underlying route_ class but holding the text pending. 
 * Use _contprintf() to continue the same log and
 * _endprintf() to finalize the message and send it.
 * Note that is another log is sent for the same route with this message
 * pending, the text will be jumbled with the other log.
 * See elog_startsend() and vsnprintf() and varargs.
 * Returns number of characters sent to the severity's route
 * [output of vsnprintf()].
 */
int elog_fstartprintf(enum elog_severity severity, /* severity */ 
		      char *file,	/* raised in file */
		      int line,		/* raised fron line number */
		      const char *function,/* raised in function */
		      const char *format,  /* format string; c.f. printf() */
		      ...		/* varargs */ )
{
     char logtext[ELOG_STRLEN];		/* post-format text */
     int n;				/* return value from vsnprintf */
     va_list ap;			/* vararg pointers */
     
     /* format the text */
     va_start(ap, format);
     n = vsnprintf(logtext, ELOG_STRLEN, format, ap);
     va_end(ap);

     /* send the formatted message */
     elog_fstartsend(severity, file, line, function, logtext);
     if (n == -1)
          elog_fcontsend(severity, "...(error message truncated)...");
     
     return n;
}

/* continue a log message enstablished by a _startsend or _startprintf() */
void elog_fcontprintf(enum elog_severity severity, const char *format, ...)
{
     char logtext[ELOG_STRLEN];		/* post-format text */
     int n;				/* return value from vsnprintf */
     va_list ap;			/* vararg pointers */
     
     /* format the text */
     va_start(ap, format);
     n = vsnprintf(logtext, ELOG_STRLEN, format, ap);
     va_end(ap);

     /* send the formatted message */
     elog_fcontsend(severity, logtext);
     if (n == -1)
          elog_fcontsend(severity, "...(error message truncated)...");
}

/* terminate a log message enstablished by a _startsend or _startprintf() */
void elog_fendprintf(enum elog_severity severity, const char *format, ...)
{
     char logtext[ELOG_STRLEN];		/* post-format text */
     int n;				/* return value from vsnprintf */
     va_list ap;			/* vararg pointers */
     
     /* format the text */
     va_start(ap, format);
     n = vsnprintf(logtext, ELOG_STRLEN, format, ap);
     va_end(ap);

     /* send the formatted message */
     if (n == -1) {
          elog_fcontsend(severity, logtext);
          elog_fendsend(severity, "...(error message truncated)");
     } else {
          elog_fendsend(severity, logtext);
     }
}

/*
 * Send an event using printf style formatting, flushing the log text
 * See elog_send() and vsnprintf() and varargs.
 * Returns number of characters sent to the severity's route
 * [output of vsnprintf()].
 */
int elog_fprintf(enum elog_severity severity, /* severity */ 
		 char *file,		     /* raised in file */
		 int line,		     /* raised fron line number */
		 const char *function,	     /* raised in function */
		 const char *format, 	     /* format string; c.f. printf() */
		 ...			     /* varargs */ )
{
     char logtext[ELOG_STRLEN];		/* post-format text */
     int n;				/* return value from vsnprintf */
     va_list ap;			/* vararg pointers */
     
     /* format the text */
     va_start(ap, format);
     n = vsnprintf(logtext, ELOG_STRLEN, format, ap);
     va_end(ap);

     /* send the formatted message */
     elog_fsend(severity, file, line, function, logtext);
     if (n == -1)
          elog_send(severity, "...(error message truncated)");

     return n;
}

/*
 * Send an event using printf style formatting, then abort(1), which 
 * should dump a core for debugging. Then exit(1) for good measure, 
 * but should never be called, due to abort().
 * See elog_fprintf(), elog_send() and vsnprintf() and varargs.
 */
void elog_fdie(enum elog_severity severity, /* severity */ 
	       char *file,		/* raised in file */
	       int line,		/* raised fron line number */
	       const char *function,	/* raised in function */
	       const char *format, 	/* format string; c.f. printf() */
	       ...			/* varargs */ )
{
     char logtext[ELOG_STRLEN];	/* post-format text */
     int n;				/* return value from vsnprintf */
     va_list ap;			/* vararg pointers */
     
     /* format the text */
     va_start(ap, format);
     n = vsnprintf(logtext, ELOG_STRLEN, format, ap);
     va_end(ap);

     /* send the formatted message */
     elog_fsend(severity, file, line, function, logtext);
     if (n == -1)
          elog_send(severity, "...(error message truncated)");

     elog_send(FATAL, "coredumping for debug");
     abort();
     exit(1);
}


/*
 * Send an event using printf style formatting using only routes
 * that are considered `safe', that is ones that will cause no further errors
 * in their use. For example, low level storage routes (such as holstore) 
 * need to raise logs/errors which do not rely on services that they
 * provide. Routes which have been set to NOROUTE are honoured, otherwise
 * information is sent to STDERR
 * See elog_fprintf(), elog_send() and vsnprintf() and varargs.
 */
void elog_fsafeprintf(enum elog_severity severity, /* severity */ 
		      char *file,	/* raised in file */
		      int line,		/* raised fron line number */
		      const char *function,/* raised in function */
		      const char *format,  /* format string; c.f. printf() */
		      ...		/* varargs */ )
{
     char logtext[ELOG_STRLEN];		/* post-format text */
     int n;				/* return value from vsnprintf */
     va_list ap;			/* vararg pointers */
     ROUTE origroute;			/* hold original route */
     int sev_in_range=1;		/* severity in range flag */
     
     /* severity in range? */
     if (severity >= ELOG_NSEVERITIES || severity < 0)
	  sev_in_range = 0;	/* no */

     /* for higher performance, it would be good to find a faster way 
      * of expressing a NULL route */
#if 0
     if (sev_in_range && elog_opendest[severity].route->method == NOROUTE)
	  return;
#endif

     /* format the text */
     va_start(ap, format);
     n = vsnprintf(logtext, ELOG_STRLEN, format, ap);
     va_end(ap);

     /* if severity is out of range, dump the error data in a fali safe way */
     if ( ! sev_in_range) {
	  elog_printf(ERROR, "elog_fsafe(): severity %d out of "
		      "range (0..%d). original error is:-\n"
		      "location: %s:%s:%d\n"
		      "text:     %s\n",
		      severity, ELOG_NSEVERITIES-1, 
		      file, function, line, logtext);
	  return;
     }

#if 0
     /* Substitute the route at given severity with a safe one for 
      * just the following call, then replace it after use */
     origroute = elog_opendest[severity].route;
     elog_opendest[severity].route = elog_errors;
     elog_fsend(severity, file, line, function, logtext);
     if (n == -1)
          elog_send(severity, "...(error message truncated)");
     elog_opendest[severity].route = origroute;
#endif
}


/*
 * Get elog status in the form of a TABLE
 */
TABLE elog_getstatus()
{
     TABLE tab;
     TREE *row;
     int i;
     char *purl;

     /* create row */
     row = tree_create();
     tree_add(row, "severity", NULL);
     tree_add(row, "route", NULL);
     tree_add(row, "format", NULL);

     /* create table and add rows to it, made from the severity table */
     tab = table_create_a(elog_colnames);
     for (i=0; i<ELOG_NSEVERITIES; i++) {
	  tree_find(row, "severity");
	  tree_put(row, elog_sevstring[i]);
	  tree_find(row, "route");
	  purl = xnstrdup( route_getpurl(elog_opendest[i].route) );
	  tree_put(row, purl);
	  table_freeondestroy(tab, purl);
	  tree_find(row, "format");
	  if (elog_opendest[i].format)
	       tree_put(row, elog_opendest[i].format);
	  else
	       tree_put(row, ELOG_DEFFORMAT);
	  table_addrow_alloc(tab, row);
     }

     tree_destroy(row);

     return tab;
}


/* Return the route of a particular severity */
ROUTE elog_getroute(enum elog_severity sev)
{
     return elog_opendest[sev].route;
}


/* Return the purl of a particular severity. Do not free the string */
char *elog_getpurl(enum elog_severity sev)
{
     return route_getpurl(elog_opendest[sev].route);
}



#if TEST

#include "route.h"
#include "rt_file.h"
#include "rt_rs.h"
#include "rs.h"

#define RS1 "rs:t.elog.2.rs,elog,0"
#define FILE1 "file:t.elog.1.dat"

int main(int argc, char **argv) {
     ROUTE err, saveroute;
     int i;
     TABLE tab;
     char *str;

     route_init(NULL, 0);
     route_register(&rt_filea_method);
     route_register(&rt_fileov_method);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     route_register(&rt_rs_method);
     rs_init();
     if ( ! elog_init(1, argv[0], NULL))
	  elog_die(FATAL, "Didn't initialise elog\n");
     err = route_open("stderr", NULL, NULL, 0);

     /* first lot of messages sent to the default places */
     elog_send(INFO, "This is an eventlog test");
     elog_send(INFO, NULL);
     elog_send(INFO, "");
     elog_send(INFO, "Event!!");
     elog_send(DEBUG, "Event!!");
     elog_send(WARNING, "Event!!");
     elog_send(ERROR, "Event!!");
     elog_send(INFO, "Event!!");

     /* change origin */
     elog_setorigin("etest");
     elog_send(INFO, "test of set origin");

     /* set one new purl route */
     elog_setsevpurl(DEBUG, FILE1);
     elog_send(INFO, "on screen");
     elog_send(DEBUG, "in file");
     elog_send(WARNING, "on screen");

     /* set second identical purl route to reuse the previous one */
     elog_setsevpurl(ERROR, FILE1);
     if (elog_opendest[DEBUG].route != 
	 elog_opendest[ERROR].route)
	  route_die(err, "[13] didnt reuse already open DEBUG route\n");
     elog_send(ERROR, "in file");

     /* set identical below purl route */
     if ( !elog_setbelowpurl(INFO, FILE1))
	  route_die(err, "[14] unable to setbelowpurl() file\n");
     if (elog_opendest[DEBUG].route != 
	 elog_opendest[ERROR].route ||
	 elog_opendest[INFO].route != 
	 elog_opendest[ERROR].route)
	  route_die(err, "[14] didnt reuse already open ERROR route\n");
     elog_send(DEBUG, "in file");
     elog_send(INFO, "in file");
     elog_send(WARNING, "on screen");
     elog_send(ERROR, "in file");
     elog_send(FATAL, "on screen");

     /* set identical above purl route */
     if ( !elog_setabovepurl(ERROR, FILE1))
	  route_die(err, "[19] unable to setabovepurl() file\n");
     if (elog_opendest[ERROR].route != 
	 elog_opendest[INFO].route ||
	 elog_opendest[FATAL].route != 
	 elog_opendest[INFO].route)
	  route_die(err, "[19] didnt reuse already open INFO route\n");
     elog_send(DEBUG, "in file");
     elog_send(INFO, "in file");
     elog_send(WARNING, "on screen");
     elog_send(ERROR, "in file");
     elog_send(FATAL, "in file");

     /* set identical all purl route */
     saveroute = elog_opendest[DEBUG].route;
     if ( !elog_setallpurl(FILE1))
	  route_die(err, "[24] unable to setallpurl() file\n");
     for (i=0; i < ELOG_NSEVERITIES; i++)
	  if (elog_opendest[i].route != saveroute)
	       route_die(err, "[24] didnt reuse already open %s route\n",
			 elog_sevstring[i]);
     elog_send(DEBUG, "in file");
     elog_send(INFO, "in file");
     elog_send(WARNING, "in file");
     elog_send(ERROR, "in file");
     elog_send(FATAL, "in file");

     /* set one different purl - timestore that we currently have to set up
      * ourselves */
     saveroute = route_open(RS1, "event log test", NULL, 10);
     if ( ! saveroute)
	  route_die(err, "[29] unable to create/open timestore\n");
     route_close(saveroute);
     if ( ! elog_setsevpurl(INFO, RS1))
	  route_die(err, "[29] unable to setsevpurl() timestore\n");
     if (elog_opendest[INFO].route == 
	 elog_opendest[WARNING].route)
	  route_die(err, "[29] different route same as WARNING\n");
     elog_send(DEBUG, "in file");
     elog_send(INFO, "in timestore");
     elog_send(WARNING, "in file");
     elog_send(ERROR, "in file");
     elog_send(FATAL, "in file");

     /* set one different route */

#if 0
     elog_send(INFO, "");
     elog_send(INFO, "Event!!");
     elog_send(DEBUG, "Event!!");
     elog_send(WARNING, "Event!!");
     elog_send(ERROR, "Event!!");
     elog_send(INFO, "Event!!");

     elog_send(INFO, "Event!!");
     elog_send(INFO, "Event!!");
     elog_send(INFO, "Event!!");
     elog_send(INFO, "Event!!");
     elog_send(INFO, "Event!!");
     elog_send(INFO, "Event!!");
     elog_send(INFO, "Event!!");
#endif

     /* change format */
     elog_setsevroute(WARNING, err);
     elog_setformat(WARNING, "%s %s");
     elog_send(WARNING, "still works??");

     /* safe logging */
     elog_safeprintf(INFO, "This is an eventlog test 35");
     elog_safeprintf(INFO, NULL);
     elog_safeprintf(INFO, "");
     elog_safeprintf(INFO, "Event!! 38");
     elog_safeprintf(DEBUG, "Event!! 39");
     elog_safeprintf(WARNING, "Event!! 40");
     elog_safeprintf(ERROR, "Event!! 41");
     elog_safeprintf(INFO, "Event!!");

     /* print the status out */
     tab = elog_getstatus();
     str = table_printcols_a(tab, elog_colnames);
     printf("%s\n", str);
     nfree(str);
     table_destroy(tab);

     rs_fini();
     elog_fini();
     route_close(err);
     route_fini();

     exit(0);
}

#endif /* TEST */
