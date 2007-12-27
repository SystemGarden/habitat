/*
 * Habitat
 * GUI independent presentation and inspection of data
 * Designed to be used in conjunction with uichoice:: which selects
 * which data should be extracted.
 * Uidata:: should be called by specific GUI toolkits, which
 * will place the information into a few generic viewers.
 *
 * Nigel Stuckey, May & June 1999
 * Copyright System Garden 1999-2001. All rights reserved.
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "uidata.h"
#include "../iiab/nmalloc.h"
#include "../iiab/tree.h"
#include "../iiab/elog.h"
#include "../iiab/table.h"
#include "../iiab/util.h"
#include "../iiab/route.h"
#include "../iiab/rs.h"
#include "../iiab/rs_gdbm.h"
#include "../iiab/iiab.h"
#include "../iiab/table.h"
#include "../iiab/job.h"
#include "../iiab/cf.h"


char *uidata_schema_nameval[] =  {"name", "value", NULL};
char *uidata_rawspancols[] = {"from", "to", "header", NULL};
char *uidata_msglogcols[] =  {"time", "severity", "message", "function", 
			      "file", "line", NULL};
char *uidata_clockcols[] =   {"start", "interval", "phase", "count", "key",
			      "origin", "result", "errors", "keep", "method",
			      "command", NULL};
char *uidata_patactcols =     "pattern em-time em-count severity method "
                              "command message\n"
                       "s d d c:info;warning;error;critical s s s widget\n"
                       "\"regular expression to find\" \"embargo time: after "
                       "raising event, wait a number of seconds before "
                       "allowing another\" \"embargo count: after raising "
                       "event, count a number of matches before raising "
                       "another\" \"priority: debug, diag, info, warning, "
                       "error, critical\" \"how the event should be acted "
                       "on \" \"action command\" \"test message sent to "
                       "action\" info";
char *uidata_patwatchcols =   "\"source route\"\ns widget\n"
                              "\"route to check\" info";
char *uidata_eventcols =      "seq time method command\n"
                              "\"event order by sequence number\" "
                              "\"when event was raised\" \"event method\" "
                              "\"command to send to method\" info";

RESDAT uidata_messagelog;		/* table of accumulated messages */


void uidata_init(CF_VALS cf)
{
     uidata_messagelog.t = TRES_TABLE;
     uidata_messagelog.d.tab = table_create_a(uidata_msglogcols);
}


void uidata_fini()
{
     table_destroy(uidata_messagelog.d.tab);
}


/* count the tables in resdat, returning the number of lines and columns */
void uidata_countresdat(RESDAT dres, int *tables, int *lines, int *cols)
{
     TREE *c, *hds;

     *tables = 0;
     if (dres.t == TRES_TABLE) {
	  *lines = table_nrows(dres.d.tab);
	  *cols  = table_ncols(dres.d.tab);
	  (*tables)++;
     } else if (dres.t == TRES_TABLELIST) {
	  c = tree_create();
	  itree_traverse(dres.d.tablst) {
	       *lines += table_nrows( itree_get(dres.d.tablst) );
	       hds = table_getheader( (TABLE) itree_get(dres.d.tablst) );
	       tree_traverse(hds)
		    if (tree_find(c, tree_getkey(hds)) == TREE_NOVAL )
			 tree_add(c, tree_getkey(hds), NULL);
	       (*tables)++;
	  }
	  *lines += tree_n(c);
	  tree_destroy(c);
     }
}





/*
 * Read and interpret an event ring
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           fname     holstore filename/path
 *                           rname     event ring name
 * Returns TABLE in RESDAT, with the schema of the version data:-
 *                           NAME      DESCRIPTION
 *                           seq       sequence of event (from timestore)
 *                           time      time the event was raised
 *                           method    action method
 *                           command   action method's command
 * On error, it returns TRES_NONE in RESDATA. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_getevents(TREE *nodeargs)
{
#if 0
     int r, methlen;
     TABLE tab;
     ITREE *commands;
     RESDAT resdat;
     char *fname, *rname, *meth, *str;
     TS_RING tsid;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get args */
     fname = tree_find(nodeargs, "fname");
     if ( fname == TREE_NOVAL ) {
          elog_printf(ERROR, "no fname node argument");
	  return resdat;	/* in error */
     }
     if ( *fname == '\0' ) {
          elog_printf(ERROR, "fname node argument is blank");
	  return resdat;	/* in error */
     }
     rname = tree_find(nodeargs, "rname");
     if ( rname == TREE_NOVAL ) {
          elog_printf(ERROR, "no rname node argument");
	  return resdat;	/* in error */
     }
     if ( *rname == '\0' ) {
          elog_printf(ERROR, "rname node argument is blank");
	  return resdat;	/* in error */
     }

     /*
      * The strategy:
      * We create the information required by performing ts_mget_t(),
      * slicing the original values up into new smaller ones, then
      * inserting the new columns with old data back into the table.
      * The original column is either removed or truncated and renamed.
      * The original values will be cleared up as no more memory 
      * allocations take place.
      */

     /* open timestore */
     tsid = ts_open(fname, rname, NULL);
     if ( ! tsid ) {
          elog_printf(ERROR, "%s,%s is not a valid timestore ring", 
		      fname, rname);
	  return resdat;	/* in error */
     }

     /* perform collection, & volume checking */
     tab = ts_mget_t(tsid, UIDATA_MAXTSRECS);
     if (tab == NULL)
          elog_printf(ERROR, "error mgetting %s,%s", fname, rname);
     ts_close(tsid);
     r = table_nrows(tab);
     if (r == UIDATA_MAXTSRECS)
          elog_printf(ERROR, "collected oldest %d rows from %s,%s", 
		      UIDATA_MAXTSRECS, fname, rname);
     if (r == 0)
          elog_printf(INFO, "no data in %s,%s", fname, rname);

     /* slice the values into a list */
     commands = itree_create();
     table_traverse(tab) {
	  meth = table_getcurrentcell(tab, "value");
	  methlen = strcspn(meth, " \t");
	  meth[methlen] = '\0';
	  itree_append(commands, meth+methlen+1);
     }

     /* add columns back to table, add info and rename everthing */
     table_renamecol(tab, "_seq", "seq");
     table_renamecol(tab, "_time", "date time");
     table_renamecol(tab, "value", "method");
     table_addcol(tab, "command", commands);
     table_freeondestroy(tab, (str = xnstrdup("time when event was raised")));
     table_replaceinfocell(tab, "info", "date time", str);
     table_freeondestroy(tab, (str = xnstrdup("event method")));
     table_replaceinfocell(tab, "info", "method", str);
     table_freeondestroy(tab, (str = xnstrdup("command executed by method")));
     table_replaceinfocell(tab, "info", "command", str);

     /* clear up and return */
     itree_destroy(commands);
     resdat.t = TRES_TABLE;
     resdat.d.tab = tab;

     return resdat;
#endif
}





/*
 * Read the pattern action directives from the specified versionstore object
 * Nodeargs should contain: KEY        DESCRIPTION
 *                          fname      holstore filename/path
 *                          rname      version object name (ring name)
 * Returns TABLE in RESDAT, with the schema of the version data:-
 *                      NAME           DESCRIPTION
 *                      pattern        regular expression to match
 *                      embaro_time    wait time before reporting same event
 *                      embargo_count  count before reporting same event
 *                      severity       importance string
 *                      method         how to raise event
 *                      command        command used in raising event
 *                      message        event message template
 * On error, it returns TRES_NONE in RESDATA. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_getpatact(TREE *args)
{
#if 0
     RESDAT resdat;
     char *cols;

     cols = xnstrdup(uidata_patactcols);
     resdat = uidata_getlatestverintab(args, NULL, cols);
     if (resdat.t == TRES_NONE)
	  nfree(cols);
     else
	  table_freeondestroy(resdat.d.tab, cols);

     return resdat;
#endif
}


/*
 * Read the pattern action directives from the specified versionstore object
 * Nodeargs should contain: KEY        DESCRIPTION
 *                          fname      holstore filename/path
 *                          rname      version object name (ring name)
 * Returns TABLE in RESDAT, with the schema of the version data:-
 *                      NAME           DESCRIPTION
 *                      pattern        regular expression to match
 *                      embaro_time    wait time before reporting same event
 *                      embargo_count  count before reporting same event
 *                      severity       importance string
 *                      method         how to raise event
 *                      command        command used in raising event
 *                      message        event message template
 * On error, it returns TRES_NONE in RESDATA. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_edtpatact(TREE *args)
{
#if 0
     RESDAT r, resdat;
     char *cols;

     cols = xnstrdup(uidata_patactcols);
     r = uidata_getlatestverintab(args, NULL, cols);
     if (r.t == TRES_NONE) {
	  nfree(cols);
	  return r;
     } else
	  table_freeondestroy(r.d.tab, cols);

     /* prepare an enhanced return with callbacks and a reference to
      * the argument tree, which persists until the action has finished */
     resdat.t = TRES_EDTABLE;
     resdat.d.edtab.tab = r.d.tab;
     resdat.d.edtab.summary = uidata_sumpatact;
     resdat.d.edtab.create  = uidata_crtpatact;
     resdat.d.edtab.update  = uidata_updpatact;
     resdat.d.edtab.args = args;

     return resdat;
#endif
}

/* Summerise pattern action data & return an nmalloc'ed string */
char * uidata_sumpatact(TREE *row)
{
     char *r;

     r = util_strjoin( tree_find(row, "pattern"), " -> ",
		       tree_find(row, "method"),  " (",
		       tree_find(row, "command"),  ") - ",
		       tree_find(row, "message"),  NULL);
     return r;
}

/* Callback to update pattern-action data.
 * Returns 1 for success or 0 for failure */
int    uidata_updpatact(TREE *args, TABLE update)
{
     printf("reached update pattern-action\n");
     return 0;
}

/* Callback to create pattern-action data.
 * Returns 1 for success or 0 for failure */
int    uidata_crtpatact(TREE *args, TABLE new)
{
     printf("reached create pattern-action\n");
     return 0;
}



/*
 * Read the pattern action directives from the specified versionstore object
 * Nodeargs should contain: KEY        DESCRIPTION
 *                          fname      holstore filename/path
 *                          rname      version object name (ring name)
 * Returns TABLE in RESDAT, with the schema of the version data:-
 *                      NAME           DESCRIPTION
 *                      source route   route scanned for pattern
 * On error, it returns TRES_NONE in RESDATA. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_getpatwatch(TREE *args)
{
#if 0
     RESDAT resdat;
     char *cols;

     cols = xnstrdup(uidata_patwatchcols);
     resdat = uidata_getlatestverintab(args, NULL, cols);
     if (resdat.t == TRES_NONE)
	  nfree(cols);
     else
	  table_freeondestroy(resdat.d.tab, cols);

     return resdat;
#endif
}



/*
 * Edit the pattern action directives from the specified versionstore object
 * Nodeargs should contain: KEY        DESCRIPTION
 *                          fname      holstore filename/path
 *                          rname      version object name (ring name)
 * Returns TRES_EDTABLE in RESDAT, with the schema of the version data:-
 *                      NAME           DESCRIPTION
 *                      source route   route scanned for pattern
 * On error, it returns TRES_NONE in RESDATA. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_edtpatwatch(TREE *args)
{
#if 0
     RESDAT r, resdat;
     char *cols;

     cols = xnstrdup(uidata_patwatchcols);
     r = uidata_getlatestverintab(args, NULL, cols);
     if (r.t == TRES_NONE) {
	  nfree(cols);
	  return r;
     } else
	  table_freeondestroy(r.d.tab, cols);

     /* prepare an enhanced return with callbacks and a reference to
      * the argument tree, which persists until the action has finished */
     resdat.t = TRES_EDTABLE;
     resdat.d.edtab.tab = r.d.tab;
     resdat.d.edtab.summary = uidata_sumpatwatch;
     resdat.d.edtab.create  = uidata_crtpatwatch;
     resdat.d.edtab.update  = uidata_updpatwatch;
     resdat.d.edtab.args = args;

     return resdat;
#endif
}

/* Summerise pattern action data & return an nmalloc'ed string */
char * uidata_sumpatwatch(TREE *row)
{
     return xnstrdup(tree_find(row, "source route"));
}

/* Callback to update pattern-action data.
 * Returns 1 for success or 0 for failure */
int    uidata_updpatwatch(TREE *args, TABLE update)
{
     printf("reached update pattern watch list\n");
     return 0;
}

/* Callback to create pattern-action data.
 * Returns 1 for success or 0 for failure */
int    uidata_crtpatwatch(TREE *args, TABLE new)
{
     printf("reached create pattern watch list\n");
     return 0;
}



/*
 * Read the pattern jobs from the clockwork versionstore object.
 * Pattern jobs names start with r.*
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           fname     holstore filename/path
 * Returns TABLE in RESDAT, with the schema of the version data:-
 *                           NAME      DESCRIPTION
 *                           start     start time of job
 *                           interval  interval of job in secs
 *                           phase     start group
 *                           count     number of times to run
 *                           key       unique key of job
 *                           origin    who originated the job
 *                           result    route for results
 *                           errors    route for errors
 *                           keep      how big a timestore should be
 *                           method    what should the job do
 *                           command   job arguments
 * On error, it returns TRES_NONE in RESDATA. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_getrecjobs(TREE *nodeargs)
{
#if 0
     int r, vlen, vver;
     TABLE tab;
     RESDAT resdat;
     char *fname, *vdata, *vauth, *vcmt;
     VS vs;
     time_t vtime;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get arg */
     fname = tree_find(nodeargs, "fname");
     if ( fname == TREE_NOVAL ) {
          elog_printf(ERROR, "no fname node argument");
	  return resdat;	/* in error */
     }
     if ( *fname == '\0' ) {
          elog_printf(ERROR, "fname node argument is blank");
	  return resdat;	/* in error */
     }

     /* open versionstore */
     vs = vers_open(fname, UIDATA_CLOCKWORKKEY, NULL);
     if ( ! vs ) {
          elog_printf(ERROR, "%s,%s is not a valid versionstore object",
		      fname, UIDATA_CLOCKWORKKEY);
	  return resdat;	/* in error */
     }

     /* get data */
     r = vers_getlatest(vs, &vdata, &vlen, &vauth, &vcmt, &vtime, &vver);
     vers_close(vs);
     if (r == 0)
          elog_printf(ERROR, "error getting %s,%s", fname, UIDATA_CLOCKWORKKEY);
     else {
	  /* scan the jobs and return if valid */
	  tab = job_scanintotable(vdata);
	  if (tab == NULL)
	       nfree(vdata);
	  else {
	       resdat.t = TRES_TABLE;
	       resdat.d.tab = tab;

	       /* remove all but those with keys begining in p. */
	       table_first(tab);
	       while ( !(table_isbeyondend(tab)) ) {
		    if (strncmp(table_getcurrentcell(tab, "key"), "r.", 2)!=0)
			 table_rmcurrentrow(tab);
		    else
			 table_next(tab);
	       }
	  }
     }

     /* clean up data and return */
     nfree(vauth);
     nfree(vcmt);
     return resdat;
#endif
}



/*
 * Edit the pattern action directives from the specified versionstore object
 * Nodeargs should contain: KEY        DESCRIPTION
 *                          fname      holstore filename/path
 *                          rname      version object name (ring name)
 * Returns TRES_EDTABLE in RESDAT, with the schema of the version data:-
 *                      NAME           DESCRIPTION
 *                      source route   route scanned for pattern
 * On error, it returns TRES_NONE in RESDATA. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_edtrecwatch(TREE *args)
{
#if 0
     RESDAT r, resdat;
     char *cols;

     cols = xnstrdup(uidata_patwatchcols);
     r = uidata_getlatestverintab(args, NULL, cols);
     if (r.t == TRES_NONE) {
	  nfree(cols);
	  return r;
     } else
	  table_freeondestroy(r.d.tab, cols);

     /* prepare an enhanced return with callbacks and a reference to
      * the argument tree, which persists until the action has finished */
     resdat.t = TRES_EDTABLE;
     resdat.d.edtab.tab = r.d.tab;
     resdat.d.edtab.summary = uidata_sumrecwatch;
     resdat.d.edtab.create  = uidata_crtrecwatch;
     resdat.d.edtab.update  = uidata_updrecwatch;
     resdat.d.edtab.args = args;

     return resdat;
#endif
}

/* Summerise pattern action data & return an nmalloc'ed string */
char * uidata_sumrecwatch(TREE *row)
{
     return xnstrdup(tree_find(row, "source route"));
}

/* Callback to update pattern-action data.
 * Returns 1 for success or 0 for failure */
int    uidata_updrecwatch(TREE *args, TABLE update)
{
     printf("reached update pattern watch list\n");
     return 0;
}

/* Callback to create pattern-action data.
 * Returns 1 for success or 0 for failure */
int    uidata_crtrecwatch(TREE *args, TABLE new)
{
     printf("reached create pattern watch list\n");
     return 0;
}



/*
 * Get local configuration table
 * No node args are required
 * Returns TABLE in RESDAT, with the schema of the version data:-
 *                           NAME      DESCRIPTION
 *                           name      name of configuration directive
 *                           arg       argument number if a vector
 *                           value     directive value
 * On error, it returns TRES_NONE in RESDATA. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_getlocalcf(TREE *nodeargs)
{
     int r;
     TABLE tab;
     RESDAT resdat;

     /* get table */
     tab = cf_getstatus( iiab_cf );
     r = table_nrows(tab);
     if (r == -1) {
          elog_printf(ERROR, "error getting rows from configuration");
	  resdat.t = TRES_NONE;
	  return resdat;
     }
     if (r == 0)
          elog_printf(DEBUG, "no data in configuration");

     /* close and return */
     resdat.t = TRES_TABLE;
     resdat.d.tab = tab;
     return resdat;
}


/*
 * Get configuration table from a route
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           fname     holstore p-url
 * Returns TABLE in RESDAT, with the schema of the version data:-
 *                           NAME      DESCRIPTION
 *                           name      name of configuration directive
 *                           arg       argument number if a vector
 *                           value     directive value
 * On error, it returns TRES_NONE in RESDAT. 
 * Free returned data with uidata_freeresdat.
 */
RESDAT uidata_getroutecf(TREE *args)
{
     RESDAT resdat;
     resdat.t = TRES_NONE;
     return resdat;
}



/*
 * Get local elog route status
 * No node args are required
 * Returns TABLE in RESDAT, with the schema of the version data:-
 *                           NAME      DESCRIPTION
 *                           severity  severity level
 *                           route     p-url of route
 * On error, it returns TRES_NONE in RESDAT. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_getlocalelogrt(TREE *nodeargs)
{
     int r;
     TABLE tab;
     RESDAT resdat;

     /* get table */
     tab = elog_getstatus();
     r = table_nrows(tab);
     if (r == -1) {
          elog_printf(ERROR, "error getting rows from configuration");
	  resdat.t = TRES_NONE;
	  return resdat;
     }
     if (r == 0)
          elog_printf(DEBUG, "no data in configuration");

     /* close and return */
     resdat.t = TRES_TABLE;
     resdat.d.tab = tab;
     return resdat;
}


/*
 * Get elog route stat from a route
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           fname     holstore p-url
 * Returns TABLE in RESDAT, with the schema of the version data:-
 *                           NAME      DESCRIPTION
 *                           severity  severity level
 *                           route     p-url of route
 * On error, it returns TRES_NONE in RESDAT. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_getrouteelogrt(TREE *args)
{
     RESDAT resdat;
     resdat.t = TRES_NONE;
     return resdat;
}


/* Log the message in the message table */
void uidata_logmessage(char ecode, time_t time, char *sev, char *file, 
		       char *func, char *line, char *text)
{
  TABLE tab;

  tab = uidata_messagelog.d.tab;

  table_addemptyrow(tab);
  table_replacecurrentcell_alloc(tab, "time", util_shortadaptdatetime(time));
  table_replacecurrentcell_alloc(tab, "severity", sev);
  table_replacecurrentcell_alloc(tab, "file", file);
  table_replacecurrentcell_alloc(tab, "function", func);
  table_replacecurrentcell_alloc(tab, "line", line);
  table_replacecurrentcell_alloc(tab, "message", text);
}


/*
 * Get log messages
 * Node argument not required
 * Returns TABLE in RESDAT, with the following schema:-
 *                           NAME      DESCRIPTION
 *                           time      date and time log was raised
 *                           severity  severity level string
 *                           message   test of log message
 * On error, it returns TRES_NONE in RESDATA. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_getlocallogs(TREE *args)
{
     RESDAT resdat;

     if (uidata_messagelog.d.tab) {
	  table_incref(uidata_messagelog.d.tab);
	  resdat.t = TRES_TABLE;
	  resdat.d.tab = uidata_messagelog.d.tab;
     } else
	  resdat.t = TRES_NONE;

     return resdat;
}


/*
 * Get log messages from a route
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           fname     holstore p-url
 * Returns TABLE in RESDAT, with the following schema:-
 *                           NAME      DESCRIPTION
 *                           time      date and time log was raised
 *                           severity  severity level string
 *                           message   test of log message
 * On error, it returns TRES_NONE in RESDAT. 
 * Free returned data with uidata_freeresdat().
 */
RESDAT uidata_getroutelogs(TREE *args)
{
     RESDAT resdat;
     resdat.t = TRES_NONE;
     return resdat;
}




/*
 * Free data within the result data structure and set the structure to 
 * TRES_NONE (empty) 
 */
void uidata_freeresdat(RESDAT d)
{
     switch (d.t) {
     case TRES_TABLE:
	  table_destroy(d.d.tab);
	  break;
     case TRES_TABLELIST:
	  itree_traverse(d.d.tablst)
	       table_destroy( (TABLE) itree_get(d.d.tablst) );
	  itree_destroy(d.d.tablst);
	  break;
     case TRES_EDTABLE:
	  table_destroy(d.d.edtab.tab);
	  /* arg data is just a reference, don't destroy */
	  break;
     case TRES_NONE:
	  return;
     default:
	  elog_die(FATAL, "unknown result type");
	  break;
     }

     d.t = TRES_NONE;
}


/*
 * Get consolidated data between now and 'tsecs' seconds ago 
 * from a route using standard addressing.
 *
 * Nodeargs should contain:  KEY        DESCRIPTION
 *                           basepurl   base part of route address
 *                           tsecs      seconds duration
 *                           ring       name of ring
 *                 optional: labels     make ring names meaningful [1]
 *                           exclude    list of rings not to display
 *                           icons      list of ring icons [2]
 *
 * Lists are items separated by semi-colons (;).
 * [1] format is a list of ring=display, where ring is the matched name
 * and display is the visible name.
 * [2] format is a list of ring=<icon number>. where <icon number> is
 * the integer representing enum uichoice_icontype.
 * A list of TABLEs are returned in RESDATA, which should be freed with  
 * uidata_freeresdat().  On error, it returns TRES_NONE.
 */
RESDAT uidata_get_route_cons(TREE *nodeargs)
{
     char *basepurl, *ring, purl[512];
     time_t tnow;
     int *tsecs;
     TABLE tab;
     RESDAT resdat;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get args */
     basepurl = tree_find(nodeargs, "basepurl");
     if ( basepurl == TREE_NOVAL ) {
          elog_printf(FATAL, "no basepurl node argument");
	  return resdat;	/* in error */
     }
     tsecs = tree_find(nodeargs, "tsecs");
     if ( tsecs == TREE_NOVAL ) {
          elog_printf(FATAL, "no tsecs node argument");
	  return resdat;	/* in error */
     }
     ring = tree_find(nodeargs, "ring");
     if ( ring == TREE_NOVAL ) {
          elog_printf(FATAL, "no ring node argument");
	  return resdat;	/* in error */
     }
     if ( *basepurl == '\0' ) {
          elog_printf(ERROR, "basepurl node argument is blank");
	  return resdat;	/* in error */
     }
     if ( *tsecs == '\0' ) {
          elog_printf(ERROR, "tsecs node argument is blank");
	  return resdat;	/* in error */
     }
     if ( *ring == '\0' ) {
          elog_printf(ERROR, "ring node argument is blank");
	  return resdat;	/* in error */
     }

     /*tree_pintdump(nodeargs, "uidata_get_route_cons ");*/

     /* collect data from route using time
      * The route address requests consolidation across rings 
      * of all duration */
     time(&tnow);
     snprintf(purl, 512, "%s,%s,cons,*,t=%ld-", basepurl, ring, tnow - *tsecs);
     tab = route_tread(purl, NULL);
     if (!tab) {
          elog_printf(INFO, "No data available from '%s'", purl);
	  return resdat;	/* in error */
     }

     /* remove _ringid column if it exists */
     table_rmcol(tab, "_ringid");

     /* we now have a good return, prepare the RESDAT structure to hold it */
     resdat.t = TRES_TABLE;
     resdat.d.tab = tab;
     return resdat;	/* return */
}



/*
 * Get data from a single route between now and 'tsecs' seconds ago 
 * from a route using standard addressing.
 *
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                  either:  purl      complete purl
 *                  or:-     basepurl  base part of route address
 *                           ring      name of ring
 *                           duration  ring duration (ascii in secs)
 *                           tsecs     seconds of capture (unless lastonly set)
 *                  option   lastonly  if non-0 return the last sample
 * 
 * If purl is present, all the other arguments are superfluous.
 * If purl is missing, then basepurl, ring, duration, tsecs must all exist
 * If lastonly is set, then tsecs is ignored and the last record is returned
 *
 * A list of TABLEs are returned in RESDATA, which should be freed with  
 * uidata_freeresdat().  On error, it returns TRES_NONE.
 */
RESDAT uidata_get_route(TREE *nodeargs)
{
     char *purl, *basepurl, *ring, *duration, mypurl[512];
     time_t tnow;
     int *tsecs, *lastonly, lseqonly=0;
     TABLE tab;
     RESDAT resdat;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get args */
     purl = tree_find(nodeargs, "purl");
     if ( purl == TREE_NOVAL ) {
	  /* time slice mode */
	  basepurl = tree_find(nodeargs, "basepurl");
	  if ( basepurl == TREE_NOVAL ) {
	       elog_printf(FATAL, "no basepurl node argument");
	       return resdat;	/* in error */
	  }
	  ring = tree_find(nodeargs, "ring");
	  if ( ring == TREE_NOVAL ) {
	       elog_printf(FATAL, "no ring node argument");
	       return resdat;	/* in error */
	  }
	  duration = tree_find(nodeargs, "duration");
	  if ( ring == TREE_NOVAL ) {
	       elog_printf(FATAL, "no duration node argument");
	       return resdat;	/* in error */
	  }
	  tsecs = tree_find(nodeargs, "tsecs");
	  if ( tsecs == TREE_NOVAL ) {
	       elog_printf(FATAL, "no tsecs node argument");
	       return resdat;	/* in error */
	  }
	  lastonly = tree_find(nodeargs, "lastonly");
	  lseqonly = *lastonly ? atoi(lastonly) : 0;

	  if ( *basepurl == '\0' ) {
	       elog_printf(ERROR, "basepurl node argument is blank");
	       return resdat;	/* in error */
	  }
	  if ( *ring == '\0' ) {
	       elog_printf(ERROR, "ring node argument is blank");
	       return resdat;	/* in error */
	  }
	  if ( *duration == '\0' ) {
	       elog_printf(ERROR, "duration node argument is blank");
	       return resdat;	/* in error */
	  }
	  if ( *tsecs == '\0' && lseqonly==0) {
	       elog_printf(ERROR, "tsecs node argument is blank & no lastonly");
	       return resdat;	/* in error */
	  }

	  /* collect data from route using time
	   * The route address requests a specific duration */
	  time(&tnow);
	  if (lseqonly)
	       snprintf(mypurl, 512, "%s,%s,%s", 
			basepurl, ring, duration);
	  else
	       snprintf(mypurl, 512, "%s,%s,%s,t=%ld-", 
			basepurl, ring, duration, tnow - *tsecs);
	  purl = mypurl;
     } else {
	  /* absolute route mode */
	  if ( *purl == '\0' ) {
	       elog_printf(ERROR, "purl node argument is blank");
	       return resdat;	/* in error */
	  }
     }

     elog_printf(DEBUG, "reading %s", purl);
     tab = route_tread(purl, NULL);
     if (!tab) {
          elog_printf(ERROR, "Unable to read '%s'", purl);
	  return resdat;	/* in error */
     }

     /* remove _ringid column if it exists */
     table_rmcol(tab, "_ringid");

     /* we now have a good return, prepare the RESDAT structure to hold it */
     resdat.t = TRES_TABLE;
     resdat.d.tab = tab;
     return resdat;	/* return */
}



/*
 * Get the contents of a file
 *
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           fname     path to file
 *
 * A list of TABLEs are returned in RESDATA, which should be freed with  
 * uidata_freeresdat().  On error, it returns TRES_NONE.
 */
RESDAT uidata_get_file(TREE *nodeargs)
{
     char *fname, *fbuf;
     TABLE tab;
     RESDAT resdat;
     struct stat finfo;
     int r, fd;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get args */
     fname = tree_find(nodeargs, "fname");
     if ( fname == TREE_NOVAL ) {
          elog_printf(FATAL, "no fname node argument");
	  return resdat;	/* in error */
     }
     if ( *fname == '\0' ) {
          elog_printf(ERROR, "fname node argument is blank");
	  return resdat;	/* in error */
     }

     /* refuse to open dirs and device nodes */
     r = stat(fname, &finfo);
     if ( r ) {
          elog_printf(ERROR, "Unable to stat '%s'", fname);
	  return resdat;	/* in error */
     }
     if (S_ISDIR(finfo.st_mode) || S_ISCHR(finfo.st_mode) || 
	 S_ISBLK(finfo.st_mode)) {
          elog_printf(ERROR, "Unable to read directories or devices '%s'", 
		      fname);
	  return resdat;	/* in error */
     }

     /* open the file */
     fbuf = xnmalloc(finfo.st_size+1);
     fd = open(fname, O_RDONLY);
     if (fd == -1) {
          elog_printf(ERROR, "Unable to open '%s'", fname);
	  return resdat;	/* in error */
     }
     r = read(fd, fbuf, finfo.st_size);
     fbuf[finfo.st_size] = '\0';
     if (r == -1) {
          elog_printf(ERROR, "Unable to read '%s'", fname);
	  close(fd);
	  nfree(fbuf);
	  return resdat;	/* in error */
     }
#if 0
     fbuf = mmap(0, 0, PROT_WRITE, MAP_PRIVATE, fd, 0);
     if (fbuf == MAP_FAILED) {
          elog_printf(ERROR, "Unable to map '%s', %d: %s", fname, errno, 
		      strerror(errno));
	  close(fd);
	  return resdat;	/* in error */
     }
#endif

     /* attempt to read as a FHA with its standard formatting rules */
     elog_printf(INFO, "File '%s' trying fha format...", fname);
     tab = table_create();
     r = table_scan(tab, fbuf, "\t", TABLE_SINGLESEP, 
		    TABLE_HASCOLNAMES, TABLE_HASRULER);
     if (r == -1) {
	  /* now attempt to read as a csv */
          elog_printf(INFO, "File '%s' is not in fha format, trying csv...", 
		      fname);
	  /*munmap(fbuf);*/
	  table_destroy(tab);
	  /*fbuf = mmap(0, 0, PROT_WRITE, MAP_PRIVATE, fd, 0);*/
	  lseek(fd, 0, SEEK_SET);
	  r = read(fd, fbuf, finfo.st_size);
	  fbuf[finfo.st_size] = '\0';
	  tab = table_create();
	  r = table_scan(tab, fbuf, ",", TABLE_SINGLESEP, 
			 TABLE_HASCOLNAMES, TABLE_NORULER);
     }
     if (r == -1) {
	  /* now attempt to read as a less formal table */
          elog_printf(INFO, "File '%s' not tabular as csv, trying informal "
		      "format...", fname);
	  /*munmap(fbuf);*/
	  table_destroy(tab);
	  /*fbuf = mmap(0, 0, PROT_WRITE, MAP_PRIVATE, fd, 0);*/
	  lseek(fd, 0, SEEK_SET);
	  r = read(fd, fbuf, finfo.st_size);
	  fbuf[finfo.st_size] = '\0';
	  tab = table_create();
	  r = table_scan(tab, fbuf, "\t ", TABLE_MULTISEP, 
			 TABLE_HASCOLNAMES, TABLE_NORULER);
     }
     if (r == -1) {
	  /* now attempt to read as a table with a single column, 
	     simulating free text */
          elog_printf(INFO, "File '%s' not informally tabular, "
		      "treat as non-column...", fname);
	  /*munmap(fbuf);*/
	  table_destroy(tab);
	  /*fbuf = mmap(0, 0, PROT_WRITE, MAP_PRIVATE, fd, 0);*/
	  lseek(fd, 0, SEEK_SET);
	  r = read(fd, fbuf, finfo.st_size);
	  fbuf[finfo.st_size] = '\0';
	  tab = table_create();
	  r = table_scan(tab, fbuf, "", TABLE_MULTISEP, 
			 TABLE_NOCOLNAMES, TABLE_NORULER);
	  table_renamecol(tab, "column_0", "text");
     }
     if (r == -1) {
          elog_printf(ERROR, "File '%s' not readable, unable to display", 
		      fname);
	  /*munmap(fbuf);*/
	  table_destroy(tab);
	  nfree(fbuf);
	  return resdat;
     }

     close(fd);
     table_freeondestroy(tab, fbuf);

     /* we now have a good return, prepare the RESDAT structure to hold it */
     resdat.t = TRES_TABLE;
     resdat.d.tab = tab;
     return resdat;	/* return */
}



/*
 * Get job information from the ring clockwork,0 if pointed to a ringstore.
 *
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           basepurl  base part of route address
 * 
 * A list of TABLEs are returned in RESDATA, which should be freed with  
 * uidata_freeresdat().  On error, it returns TRES_NONE.
 */
RESDAT uidata_get_jobs(TREE *nodeargs)
{
     char *basepurl, purl[512];
     TABLE tab;
     RESDAT resdat;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get args */
     basepurl = tree_find(nodeargs, "basepurl");
     if ( basepurl == TREE_NOVAL ) {
	  elog_printf(FATAL, "no basepurl node argument");
	  return resdat;	/* in error */
     }
     if ( *basepurl == '\0' ) {
	  elog_printf(ERROR, "basepurl node argument is blank");
	  return resdat;	/* in error */
     }

     /* collect data from route using time
      * The route address requests consolidation across rings 
      * of all duration */
     snprintf(purl, 512, "%s,clockwork,0", basepurl);
     elog_printf(DEBUG, "reading %s", purl);
     tab = route_tread(purl, NULL);
     if (!tab) {
          elog_printf(ERROR, "Unable to read '%s'", purl);
	  return resdat;	/* in error */
     }

     /* remove _ringid column if it exists */
     table_rmcol(tab, "_ringid");
     table_rmcol(tab, "_time");
     table_rmcol(tab, "_seq");

     /* we now have a good return, prepare the RESDAT structure to hold it */
     resdat.t = TRES_TABLE;
     resdat.d.tab = tab;
     return resdat;	/* return */
}



/*
 * Get system uptime information from the ring up,0 if pointed 
 * to a ringstore.
 *
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           basepurl  base part of route address
 * 
 * A list of TABLEs are returned in RESDATA, which should be freed with  
 * uidata_freeresdat().  On error, it returns TRES_NONE.
 */
RESDAT uidata_get_uptime(TREE *nodeargs)
{
     char *basepurl, purl[512];
     TABLE tab, tab2;
     ITREE *cols;
     RESDAT resdat;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get args */
     basepurl = tree_find(nodeargs, "basepurl");
     if ( basepurl == TREE_NOVAL ) {
	  elog_printf(FATAL, "no basepurl node argument");
	  return resdat;	/* in error */
     }
     if ( *basepurl == '\0' ) {
	  elog_printf(ERROR, "basepurl node argument is blank");
	  return resdat;	/* in error */
     }

     /* collect data from route using time
      * The route address requests consolidation across rings 
      * of all duration */
     snprintf(purl, 512, "%s,up,0", basepurl);
     elog_printf(DEBUG, "reading %s", purl);
     tab = route_tread(purl, NULL);
     if (!tab) {
          elog_printf(ERROR, "Unable to read '%s'", purl);
	  return resdat;	/* in error */
     }

     /* remove _ringid column if it exists */
     table_rmcol(tab, "_ringid");
     table_rmcol(tab, "_time");
     table_rmcol(tab, "_seq");

     /* grab the last row and turn into a two column table */
     tab2 = table_create_a(uidata_schema_nameval);
     table_last(tab);
     cols = table_getcolorder(tab);
     itree_traverse(cols) {
	  table_addemptyrow(tab2);
	  table_replacecurrentcell_alloc(tab2, "name", itree_get(cols));
	  table_replacecurrentcell_alloc(tab2, "value", 
			      table_getcurrentcell(tab, itree_get(cols)));
     }
     table_destroy(tab);

     /* we now have a good return, prepare the RESDAT structure to hold it */
     resdat.t = TRES_TABLE;
     resdat.d.tab = tab2;
     return resdat;	/* return */
}


/*
 * Return this host's info via a route.
 * Make a private tree, copy hostinfo to purl and call uidata_get_route().
 * The data is cleaned up before returning the table.
 */
RESDAT uidata_get_hostinfo(TREE *nodeargs)
{
     TABLE tab;
     struct utsname uts;
     int r;
     RESDAT resdat;
     char *hostinfo, *purl;
     TREE *priv_nodeargs;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get args */
     hostinfo = tree_find(nodeargs, "hostinfo");
     if ( hostinfo == TREE_NOVAL ) {
	  elog_printf(FATAL, "no hostinfo node argument");
	  return resdat;	/* in error */
     }
     if ( *hostinfo == '\0' ) {
	  elog_printf(ERROR, "hostinfo node argument is blank");
	  return resdat;	/* in error */
     }

     /* copy hostinfo to purl in a private tree & call 
      * uidata_get_route() */
     priv_nodeargs = tree_create();
     tree_traverse(nodeargs) {
	  tree_add(priv_nodeargs, tree_getkey(nodeargs), 
		   tree_get(nodeargs));
     }
     tree_add(priv_nodeargs, "purl", hostinfo);

     /* fetch data from route */
     resdat = uidata_get_route(priv_nodeargs);
     tree_destroy(priv_nodeargs);

     return resdat;	/* return */
}



/*
 * Get information about a ringstore file
 *
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           fname     path to file
 *
 * A list of TABLEs are returned in RESDATA, which should be freed with  
 * uidata_freeresdat().  On error, it returns TRES_NONE.
 */
RESDAT uidata_get_rsinfo(TREE *nodeargs)
{
     TABLE supertab;
     RESDAT resdat;
     RS_SUPER super;
     int ntabs=0;
     char *fname;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get args */
     fname = tree_find(nodeargs, "fname");
     if ( fname == TREE_NOVAL ) {
          elog_printf(FATAL, "no fname node argument");
	  return resdat;	/* in error */
     }
     if ( *fname == '\0' ) {
          elog_printf(ERROR, "fname node argument is blank");
	  return resdat;	/* in error */
     }
     /*tree_pintdump(nodeargs, "uidata_rsinfo  ");*/

     /* gather data from the ringstore */
     super = rs_info_super(&rs_gdbm_method, fname);
     if ( ! super ) {
          elog_printf(ERROR, "unable to read superblock from %s", fname);
     } else {
          supertab = table_create_a(uidata_schema_nameval);
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "Storage");
	  table_replacecurrentcell_alloc(supertab, "value", "Ringstore");
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "Version");
	  table_replacecurrentcell_alloc(supertab, "value", 
					 util_i32toa(super->version));
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "Created");
	  table_replacecurrentcell_alloc(supertab, "value", 
					 util_decdatetime(super->created));
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "OS Name");
	  table_replacecurrentcell_alloc(supertab, "value", super->os_name);
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "OS Release");
	  table_replacecurrentcell_alloc(supertab, "value", super->os_release);
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "OS Version");
	  table_replacecurrentcell_alloc(supertab, "value", super->os_version);
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "Hostname");
	  table_replacecurrentcell_alloc(supertab, "value", super->hostname);
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "Domainname");
	  table_replacecurrentcell_alloc(supertab, "value", super->domainname);
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "Machine");
	  table_replacecurrentcell_alloc(supertab, "value", super->machine);
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name",  "GMT offset");
	  table_replacecurrentcell_alloc(supertab, "value", 
					 util_i32toa(super->timezone));
	  table_addemptyrow(supertab);
	  table_replacecurrentcell_alloc(supertab, "name", "Number of rings");
	  table_replacecurrentcell_alloc(supertab, "value", 
					 util_i32toa(super->ringcounter));
	  ntabs++;
	  rs_free_superblock(super);
     }

     /* we now have a good return, prepare the RESDAT structure to hold it */
     resdat.t = TRES_TABLE;
     resdat.d.tab = supertab;
     return resdat;	/* return */
}



/*
 * Get information about a plain file
 *
 * Nodeargs should contain:  KEY       DESCRIPTION
 *                           fname     path to file
 *
 * A list of TABLEs are returned in RESDATA, which should be freed with  
 * uidata_freeresdat().  On error, it returns TRES_NONE.
 */
RESDAT uidata_get_fileinfo(TREE *nodeargs)
{
     TABLE stattab;
     RESDAT resdat;
     struct stat statbuf;
     int ntabs=0;
     char *fname;

     resdat.t = TRES_NONE;	/* default to pessimistic return */

     /* get args */
     fname = tree_find(nodeargs, "fname");
     if ( fname == TREE_NOVAL ) {
          elog_printf(FATAL, "no fname node argument");
	  return resdat;	/* in error */
     }
     if ( *fname == '\0' ) {
          elog_printf(ERROR, "fname node argument is blank");
	  return resdat;	/* in error */
     }
     /*tree_pintdump(nodeargs, "uidata_fileinfo  ");*/

     /* gather information from the plain file */
     if (stat(fname, &statbuf)) {
          elog_printf(ERROR, "unable to stat file %s", fname);
     } else {
          stattab = table_create_a(uidata_schema_nameval);
	  table_addemptyrow(stattab);
	  table_replacecurrentcell_alloc(stattab, "name",  "Storage");
	  table_replacecurrentcell_alloc(stattab, "value", "Plain File");
	  table_addemptyrow(stattab);
	  table_replacecurrentcell_alloc(stattab, "name",  "Created");
	  table_replacecurrentcell_alloc(stattab, "value", 
				     util_decdatetime(statbuf.st_ctime));
	  table_addemptyrow(stattab);
	  table_replacecurrentcell_alloc(stattab, "name",  "Modified");
	  table_replacecurrentcell_alloc(stattab, "value", 
				     util_decdatetime(statbuf.st_mtime));
	  table_addemptyrow(stattab);
	  table_replacecurrentcell_alloc(stattab, "name",  "Accessed");
	  table_replacecurrentcell_alloc(stattab, "value", 
				     util_decdatetime(statbuf.st_atime));
	  table_addemptyrow(stattab);
	  table_replacecurrentcell_alloc(stattab, "name",  "Size");
	  table_replacecurrentcell_alloc(stattab, "value", 
				     util_i32toa(statbuf.st_size));
	  table_addemptyrow(stattab);
	  table_replacecurrentcell_alloc(stattab, "name",  "Owner");
	  table_replacecurrentcell_alloc(stattab, "value", 
				     util_i32toa(statbuf.st_uid));
	  table_addemptyrow(stattab);
	  table_replacecurrentcell_alloc(stattab, "name",  "Owner's Group");
	  table_replacecurrentcell_alloc(stattab, "value", 
				     util_i32toa(statbuf.st_gid));
	  table_addemptyrow(stattab);
	  table_replacecurrentcell_alloc(stattab, "name",  "File Mode");
	  table_replacecurrentcell_alloc(stattab, "value", 
				     util_i32toa(statbuf.st_mode));
	  table_addemptyrow(stattab);
	  table_replacecurrentcell_alloc(stattab, "name",  "I-Node");
	  table_replacecurrentcell_alloc(stattab, "value", 
				     util_i32toa(statbuf.st_ino));

	  ntabs++;
     }

     /* we now have a good return, prepare the RESDAT structure to hold it */
     resdat.t = TRES_TABLE;
     resdat.d.tab = stattab;
     return resdat;	/* return */
}



#if TEST
#define TEST_FILE1 "t.uidata.1.dat"
#define TEST_RING1 "tring1"
#define TEST_TABLE1 "ttable1"
#define TEST_TABLE2 "ttable2"
#define TEST_TABLE3 "ttable3"
#define TEST_VER1 "vobj1"
#define TEST_VER2 "vobj2"
#define TEST_VER3 "vobj3"
#define TEST_VTEXT1 "eeny meeny"
#define TEST_VTEXT2 "miny"
#define TEST_VTEXT3 "mo"
#define TEST_VAUTHOR "nigel"
#define TEST_VCMT "some text"

main(int argc, char **argv)
{
     HOLD hid;
     TS_RING tsid;
     TAB_RING tabid;
     VS vs1;
     rtinf err;
     TABLE res1;
     TREE *arg1;
     ITREE *ires1, *ires2, *ires3;
     int i, r, n, *seq1;
     char *buf1;

     route_init("stderr", 0);
     err = route_open("stderr", NULL, NULL, 0);
     elog_init(err, 0, argv[0], NULL);
     uidata_init();
     hol_init(0, 0);

     /* test 1: holstore  */
     unlink(TEST_FILE1);
     hid = hol_create(TEST_FILE1, 0644);
     hol_put(hid, "whitley",   "test 1", 7);
     hol_put(hid, "milford",   "test 2", 7);
     hol_put(hid, "godalming", "test 3", 7);
     hol_put(hid, "farncombe", "test 4", 7);
     hol_put(hid, "guildford", "test 5", 7);
     hol_put(hid, "woking",    "test 6", 7);
     hol_put(hid, "waterloo",  "test 7", 7);
     hol_close(hid);

     arg1 = tree_create();
     tree_add(arg1, "fname", TEST_FILE1);
     tree_add(arg1, "pattern", "*");
     res1 = uidata_getrawhol(arg1);
     buf1 = table_print(res1);
     printf(buf1);
     nfree(buf1);
     table_destroy(res1);
     tree_destroy(arg1);

     /* test 2: timestore */
     tsid = ts_create(TEST_FILE1, 0644, TEST_RING1, "five slot ring", NULL, 5);
     if ( ! tsid )
          elog_die(FATAL, "[2] unable to create ring");
     ts_put(tsid, "twhitley",   9);
     ts_put(tsid, "tmilford",   9);
     ts_put(tsid, "tgodalming", 11);
     ts_put(tsid, "tfarncombe", 11);
     ts_put(tsid, "tguildford", 11);
     ts_put(tsid, "twoking",    8);
     ts_put(tsid, "twaterloo",  10);
     ts_close(tsid);

     arg1 = tree_create();
     tree_add(arg1, "fname", TEST_FILE1);
     tree_add(arg1, "rname", TEST_RING1);
     res1 = uidata_getrawts(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[2] unable to getrawts()");
     if (table_nrows(res1) != 5)
          elog_die(FATAL, "[2] n (%d) != 5", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 2:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);
     tree_destroy(arg1);

     /* test 3: spanstore */
     tabid = tab_create(TEST_FILE1, 0644, TEST_TABLE1, "tom dick harry", 
			TAB_HEADINCALL, "table storage 1", NULL, 5);
     if ( ! tabid )
          elog_die(FATAL, "[3a] unable to create table ring");
     tab_put(tabid, "first second third");
     tab_close(tabid);
     tabid = tab_create(TEST_FILE1, 0644, TEST_TABLE2, "tom dick harry", 
			TAB_HEADINCALL, "table storage 2", NULL, 5);
     if ( ! tabid )
          elog_die(FATAL, "[3b] unable to create table ring");
     tab_put(tabid, "one two three");
     tab_put(tabid, "isaidone againtwo andoncemorethree");
     tab_close(tabid);
     tabid = tab_create(TEST_FILE1, 0644, TEST_TABLE3, "tom dick harry", 
			TAB_HEADINCALL, "table storage 3", NULL, 5);
     if ( ! tabid )
          elog_die(FATAL, "[3c] unable to create table ring");
     tab_put(tabid, "nipples thighs bollocks");
     tab_put(tabid, "lies damnlies andstatistics");
     tab_put(tabid, "and another thing");
     tab_close(tabid);

     arg1 = tree_create();
     tree_add(arg1, "fname", TEST_FILE1);
     tree_add(arg1, "rname", TEST_TABLE1);
     res1 = uidata_getrawspans(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[3d] unable to getrawspans()");
     if (table_nrows(res1) != 1)
          elog_die(FATAL, "[3d] n (%d) != 1", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 3d:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);
     tree_find(arg1, "rname");
     tree_put(arg1, TEST_TABLE2);
     res1 = uidata_getrawspans(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[3e] unable to getrawspans()");
     if (table_nrows(res1) != 1)
          elog_die(FATAL, "[3e] n (%d) != 1", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 3e:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);
     tree_find(arg1, "rname");
     tree_put(arg1, TEST_TABLE3);
     res1 = uidata_getrawspans(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[3f] unable to getrawspans()");
     if (table_nrows(res1) != 1)
          elog_die(FATAL, "[3f] n (%d) != 1", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 3f:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);
     tree_destroy(arg1);

     /* test 4: tablestore */
     arg1 = tree_create();
     tree_add(arg1, "fname", TEST_FILE1);
     tree_add(arg1, "rname", TEST_TABLE1);
     seq1 = xnmalloc(sizeof(int));
     *seq1 = 0;
     tree_add(arg1, "seqnum", seq1);
     res1 = uidata_getrawtabs(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[4a] unable to getrawtabs()");
     if (table_nrows(res1) != 1)
          elog_die(FATAL, "[4a] n (%d) != 1", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 4a:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);
     tree_find(arg1, "rname");
     tree_put(arg1, TEST_TABLE2);
     res1 = uidata_getrawtabs(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[4b] unable to getrawtabs()");
     if (table_nrows(res1) != 2)
          elog_die(FATAL, "[4b] n (%d) != 2", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 4b:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);
     tree_find(arg1, "rname");
     tree_put(arg1, TEST_TABLE3);
     res1 = uidata_getrawtabs(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[4c] unable to getrawtabs()");
     if (table_nrows(res1) != 3)
          elog_die(FATAL, "[4c] n (%d) != 3", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 4c:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);
     tree_destroy(arg1);
     nfree(seq1);


     /* test 5: versionstore */
     vs1 = vers_create(TEST_FILE1, 0644, TEST_VER1, NULL, "test vobject 1");
     if ( ! vs1 )
          elog_die(FATAL, "[5a] unable to create version obj");
     r = vers_new(vs1, TEST_VTEXT1, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 0)
          elog_die(FATAL, "[5a1] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT2, 0, TEST_VAUTHOR, "");
     if (r != 1)
          elog_die(FATAL, "[5a2] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT3, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 2)
          elog_die(FATAL, "[5a3] verison number wrong");
     vers_close(vs1);
     vs1 = vers_create(TEST_FILE1, 0644, TEST_VER2, NULL, "test vobject 2");
     if ( ! vs1 )
          elog_die(FATAL, "[5b] unable to create version obj");
     r = vers_new(vs1, TEST_VTEXT1, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 0)
          elog_die(FATAL, "[5b1] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT2, 0, TEST_VAUTHOR, "");
     if (r != 1)
          elog_die(FATAL, "[5b2] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT3, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 2)
          elog_die(FATAL, "[5b3] verison number wrong");
     vers_close(vs1);
     vs1 = vers_create(TEST_FILE1, 0644, TEST_VER3, NULL, "test vobject 3");
     if ( ! vs1 )
          elog_die(FATAL, "[5c] unable to create version obj");
     r = vers_new(vs1, TEST_VTEXT1, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 0)
          elog_die(FATAL, "[5c1] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT2, 0, TEST_VAUTHOR, "");
     if (r != 1)
          elog_die(FATAL, "[5c2] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT3, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 2)
          elog_die(FATAL, "[5c3] verison number wrong");
     vers_close(vs1);

     arg1 = tree_create();
     tree_add(arg1, "fname", TEST_FILE1);
     tree_add(arg1, "rname", TEST_VER1);
     res1 = uidata_getrawvers(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[5d] unable to getrawvers()");
     if (table_nrows(res1) != 3)
          elog_die(FATAL, "[5d] n (%d) != 3", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 5d:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);

     tree_find(arg1, "rname");
     tree_put(arg1, TEST_VER2);
     res1 = uidata_getrawvers(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[5e] unable to getrawvers()");
     if (table_nrows(res1) != 3)
          elog_die(FATAL, "[5e] n (%d) != 3", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 5e:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);

     tree_find(arg1, "rname");
     tree_put(arg1, TEST_VER3);
     res1 = uidata_getrawvers(arg1);
     if ( ! res1 )
          elog_die(FATAL, "[5f] unable to getrawvers()");
     if (table_nrows(res1) != 3)
          elog_die(FATAL, "[5f] n (%d) != 3", table_nrows(res1));
     buf1 = table_print(res1);
     printf("test 5f:-\n%s", buf1);
     nfree(buf1);
     table_destroy(res1);

     tree_destroy(arg1);


     uidata_fini();
     elog_fini();
     route_close(err);
     route_fini();
     exit(0);
}

#endif /* TEST */
