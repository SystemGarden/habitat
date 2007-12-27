/*
 * Pattern - route watching and pattern matching class
 * Nigel Stuckey, March 2000
 *
 * Copyright System Garden Ltd 2000-2001, all rights reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "pattern.h"
#include "nmalloc.h"
#include "util.h"
#include "job.h"

/*
 * Initialise a pattern session
 */
WATCHED pattern_init(ROUTE out,		/* route output */
		     ROUTE err,		/* route errors */
		     char *patact, 	/* p-url of pattern actions */
		     char *watch	/* p-url of routes to watch */ )
{
     WATCHED w;

     if (patact == NULL || *patact == '\0')
	  elog_die(FATAL, "no pattern-action route");
     if (watch == NULL || *watch == '\0')
	  elog_die(FATAL, "no watch route");

     w = xnmalloc(sizeof(struct pattern_info));
     w->patact      = xnstrdup(patact);
     w->watch       = xnstrdup(watch);
     w->patact_modt = w->watch_modt = 0;
     w->patact_rt   = w->watch_rt   = NULL;
     w->patterns    = NULL;
     w->watchlist   = NULL;
     w->rundirectly = 0;

     /* kick off the first pattern waction, which should just force load
      * everything, scan the routes to be watched and set the times up */
/*     pattern_action(w, out, err);*/

     return w;
}

/*
 * Shut down pattern session
 */
void pattern_fini(WATCHED w)
{
     struct pattern_action *act;
     struct pattern_route  *wat;

     nfree(w->patact);
     nfree(w->watch);
     if (w->patact_rt != NULL)
	  route_close(w->patact_rt);
     if (w->watch_rt != NULL)
	  route_close(w->watch_rt);

     /* remove pattern list */
     if (w->patterns != NULL) {
	  tree_traverse(w->patterns) {
	       /* delete pattern_action nodes */
	       act = tree_get(w->patterns);
	       regfree(&act->comp);
	       if (act->action_method)
		    nfree(act->action_method);
	       if (act->action_arg)
		    nfree(act->action_arg);
	       if (act->action_message)
		    nfree(act->action_message);
	       nfree( tree_getkey(w->patterns) );
	       nfree( act );
	  }
	  tree_destroy(w->patterns);
     }

     /* remove watch list */
     if (w->watchlist != NULL) {
	  itree_traverse(w->watchlist) {
	       /* delete pattern_route nodes */
	       wat = tree_get(w->watchlist);
	       nfree( tree_getkey(w->watchlist) );
	       nfree( wat );
	  }
	  tree_destroy(w->watchlist);
     }

     nfree(w);
}

/* manipulate run directly flag */
void pattern_rundirectly(WATCHED w, int torf) { w->rundirectly = torf; }
int  pattern_isrundirectly(WATCHED w)         { return w->rundirectly; }

/*
 * Run a set pattern watching actions on all the observed routes
 */
int pattern_action(WATCHED w,	/* watch instance */
		   ROUTE out,	/* route output */
		   ROUTE err	/* route errors */ )
{
     int toksz, lasttok;
     struct pattern_route  *wat;
     ITREE *bufchain;
     ROUTE_BUF *buf;
     char *tok;

     /* load pattern-actions and watch subjects */
     pattern_load_patact(w);

     /* load watch list */
     pattern_load_watch(w);

     /* do I want to start? */
     if (w->watchlist == NULL || w->patterns == NULL)
	  return 0;	/* nothing happened successfully !! */

     /* iterate over watch list and stat the routes, raising events 
      * when matches are found */
     tree_traverse(w->watchlist) {

	  /* stat file to see if it has been changed */
	  wat = tree_get(w->watchlist);
	  if ( (bufchain = pattern_getchanged(wat)) ) {

	       /* traverse each data buffer located */
	       itree_traverse(bufchain) {
		    /* now check each line in each buffer */
		    buf = itree_get(bufchain);
		    tok = buf->buffer;
		    lasttok = 0;
		    while (1) {
			 /* terminate token */
			 toksz = strcspn(tok, "\n");
			 if (toksz == 0 && *tok == '\0')
			      break;
			 tok[toksz] = '\0';

			 /* do the work */
			 pattern_matchbuffer(out, err, w->patterns, wat,
					     tok, w->rundirectly);
			 tok += toksz+1;
		    }
	       }
	       route_free_routebuf(bufchain);
	  }
     }

     return 0;		/* success */
}

/*
 * Stat the pattern-action route to see if the data has been touched (by 
 * time), using the data in the instance's pattern_info structure.
 * If there is no change, leave it. If there is a change, then read the
 * data and update the pattern automata accordingly.
 * The format of the pattern-action route is tabular, suitable for 
 * reading into a TABLE with route_tread().
 * Returns 1 for successfully loaded and created pattern action automita,
 * or 0 if the data is not available
 */
int pattern_load_patact(WATCHED w		/* watch instancce */ )
{
     int seq, size, r, ivalue;
     time_t modt;
     TABLE patab;
     char *pat, *value;
     struct pattern_action *act;
     char errtext[PATTERN_ERRTEXTLEN];
     enum elog_severity evalue;

     /* read route cotaining pattern-action text if not done already
      * (insufficient parameters for creation)
      * Open the patact 'control' route and leave it open */
     if (w->patact_rt == NULL) {
	  w->patact_rt = route_open(w->patact, NULL, NULL, 0);
	  if (w->patact_rt == NULL)
	       return 0;	/* no route to read */
     }

     /* Read the pattern action data, which should be in format 
      * suitable for table scanning. 
      * The format is:- 
      *
      *     pattern embargo_time embargo_count severity action_meth action_args
      *
      * Where: pattern is the regular expression pattern to look for
      *               and when it is found, is considered an event.
      *               Normally, it should match a single line.
      *        embargo_time is the number of seconds that must elapse
      *               after the first event before another event
      *               may be raised of the same pattern from
      *               the same route.
      *        embargo_count is the maximum number of identical pattern 
      *               matches that can take place before another
      *               event is raised for that pattern.
      *        severity indicates how important the event is
      *        action_meth is one of the execution methods supported
      *               by meth().
      *        action_args are method specific to aid in writing the event.
      *        action_msg is the message template used to describe the
      *               event when it is sent on. It may contain special
      *               tokens of the form %<char> that describe 
      *               other information about event.
      * If embargo_time is reached, then embargo_count is reset;
      * if embargo_count is reached, then embargo_time is reset.
      *
      * Once pattern class has found this data, it composes a summary
      * of the event using all the information it has available. 
      * If rundirectly is set, then a one-off job to run immedeatly 
      * is created. Otherwise, the instructions are 
      * written to the output route as a job mill queue. The event
      * class should be able to pick them up and run them.
      */

     /* stat route and scan if out of date */
     route_tell(w->patact_rt, &seq, &size, &modt);
     if (modt != w->patact_modt) {
	  w->patact_modt = modt;
	  patab = route_tread(w->patact, NULL);
	  if ( ! patab) {
	       /* route has been touched but there is no data */
	       elog_printf(ERROR, "Error scanning pattern-action "
			   "file %s: abandoning whole pattern job", 
			   w->patact);
	       return 0;	/* no parsed data */
	  }

	  /* transform table into scanned structure */
	  if (w->patterns == NULL)
	       w->patterns = tree_create();
	  table_traverse(patab) {
	       /* only compile if expression has changed */
	       pat = table_getcurrentcell(patab, "pattern");
	       if ((act = tree_find(w->patterns, pat)) == TREE_NOVAL) {
		    /* compile regular expression and handle errors */
		    act = xnmalloc( sizeof (struct pattern_action) );
		    r = regcomp(&act->comp, pat, 
				(REG_EXTENDED | REG_NEWLINE | REG_NOSUB ));
		    if (r != 0) {
			 regerror(r, &act->comp, errtext, PATTERN_ERRTEXTLEN);
			 elog_printf(ERROR, "Problem with pattern: `%s': %s", 
				     pat, errtext);
			 nfree(act);
			 continue;
		    }

		    /* initialise structure and add */
		    act->embargo_time  = 0;
		    act->embargo_count = 0;
		    act->event_timeout = 0;
		    act->event_count   = 0;
		    act->severity      = DEBUG;
		    act->action_method = NULL;
		    act->action_arg    = NULL;
		    act->action_message= NULL;
		    act->ref           = 1;
		    tree_add(w->patterns, xnstrdup(pat), act);
		    elog_printf(DIAG, "add pattern: %s", pat);
	       }

	       /* at this point we have a valid action structure in
		* the pattern list and act points to it */

	       /* now patch up the action structure if changed */
	       ivalue = atoi(table_getcurrentcell(patab, "embargo_time"));
	       if (act->embargo_time != ivalue)
		    act->embargo_time  = ivalue;
	       ivalue = atoi(table_getcurrentcell(patab, "embargo_count"));
	       if (act->embargo_count != ivalue)
		    act->embargo_count = ivalue;
	       if (act->embargo_count > 0)
		    act->event_count = act->embargo_count;	/* raise 1st */
	       evalue = elog_strtosev(table_getcurrentcell(patab, "severity"));
	       if (evalue != NOELOG && evalue != act->severity)
		    act->severity      = evalue;
	       value = table_getcurrentcell(patab, "action_method");
	       if (value) {
		    if (act->action_method == NULL)
			 act->action_method = xnstrdup(value);
		    else if (strcmp(act->action_method, value)) {
			 nfree(act->action_method);
			 act->action_method = xnstrdup(value);
		    }

	       }
	       value = table_getcurrentcell(patab, "action_arg");
	       if (value) {
		    if (act->action_arg == NULL)
			 act->action_arg = xnstrdup(value);
		    else if (strcmp(act->action_arg, value)) {
			 nfree(act->action_arg);
			 act->action_arg = xnstrdup(value);
		    }

	       }
	       value = table_getcurrentcell(patab, "action_message");
	       if (value) {
		    if (act->action_message == NULL)
			 act->action_message = xnstrdup(value);
		    else if (strcmp(act->action_message, value)) {
			 nfree(act->action_message);
			 act->action_message = xnstrdup(value);
		    }
	       }
	       act->ref++;	/* to support weeding out unused entries */
	  }

	  table_destroy(patab);

	  /* decrement reference count over whole table & weed out 
	   * unsed entries*/
	  tree_first(w->patterns);
	  while ( ! tree_isbeyondend(w->patterns) ) {
	       act = tree_get(w->patterns);
	       act->ref--;
	       if (act->ref == 0) {
		    /* delete node */
		    regfree(&act->comp);
		    if (act->action_method)
			 nfree(act->action_method);
		    if (act->action_arg)
			 nfree(act->action_arg);
		    if (act->action_message)
			 nfree(act->action_message);
		    elog_printf(DIAG, "remove pattern: %s", 
				tree_getkey(w->patterns));
		    nfree( tree_getkey(w->patterns) );
		    nfree( act );
		    tree_rm(w->patterns);
	       } else
		    tree_next(w->patterns);
	  }
     }

     return 1;			/* up to date! */
}


/*
 * Stat the watch list route to see if the data has been touched, using
 * the data in the instance's pattern_info structure.
 * If there is no change, leave it all. If there is a change, then read 
 * and update list
 * The format of the watch route should be text oriented, with a single 
 * line per entry; each entry is a route to be watched.
 * Returns 1 for successfully loaded data or 0 if the data is not available
 */
int pattern_load_watch(WATCHED w		/* watch instance */ )
{
     char *watchbuf, *tok;
     struct pattern_route *wat;
     int seq, size, r, toksz, lasttok;
     time_t modt;

     /* read route cotaining watch text if not done already
      * (insufficient parameters for creation) */
     if (w->watch_rt == NULL) {
	  w->watch_rt = route_open(w->watch, NULL, NULL, 0);
	  if (w->watch_rt == NULL)
	       return 0;	/* no route to read */
     }

     /* stat route and scan if out of date */
     r = route_tell(w->watch_rt, &seq, &size, &modt);
     if (r == 0)
	  return 0;		/* can't stat */
     if (modt != w->watch_modt) {
	  w->watch_modt = modt;
	  watchbuf = route_read(w->watch, NULL, &size);
	  if (watchbuf == NULL)
	       return 0;	/* no data available */

	  /* transform table into scanned structure */
	  if (w->watchlist == NULL)
	       w->watchlist = tree_create();

	  /* Read the watch list: one line is one route */
	  tok = watchbuf;
	  lasttok = 0;
	  while (1) {
	       /* terminate token */
	       toksz = strcspn(tok, "\n");
	       if (toksz == 0 && *tok == '\0')
		    lasttok++;
	       else
		    tok[toksz] = '\0';

	       if (     util_is_str_whitespace(tok) || 
		    ( ! util_is_str_printable(tok) )   )
		    goto next_file;

	       /* now we have a route, do the work */
	       if ( (wat = tree_find(w->watchlist, tok)) == TREE_NOVAL) {
		    /* route not watched, so do it...
		     * Attempt to load each route to find its base size.
		     * We then assume that we do not want to look at data
		     * before the route was refered to us. */
		    wat      = xnmalloc( sizeof( struct pattern_route ) );
		    wat->key = xnstrdup(tok);
		    wat->ref = 1;
		    if (route_stat(wat->key /*purl*/, NULL, &wat->last_seq, 
				   &wat->last_size, 
				   &wat->last_modt) != 1) {
		         wat->last_size = 0;
			 wat->last_seq  = 0;
			 wat->last_modt = 0;
		    }
		    tree_add(w->watchlist, wat->key, wat);
		    elog_printf(DIAG, "add watched route: %s",
				wat->key);
	       }
	       wat->ref++;

	  next_file:
	       /* iterator */
	       if (lasttok)
		    break;
	       else
		    tok += toksz+1;
	  }

	  nfree(watchbuf);	/* free source data */

	  /* decrement reference count over whole table & weed out 
	   * unsed entries*/
	  tree_first(w->watchlist);
	  while ( ! tree_isbeyondend(w->watchlist) ) {
	       wat = tree_get(w->watchlist);
	       wat->ref--;
	       if (wat->ref == 0) {
		    /* delete node */
		    elog_printf(DIAG, "remove watched route: %s", 
				wat->key);
		    nfree( tree_getkey(w->watchlist) );
		    nfree( wat );
		    tree_rm(w->watchlist);
	       } else
		    tree_next(w->watchlist);
	  }
     }

     return 1;
}


/*
 * Returns the additonal data if the purl described by `wat' was been 
 * changed since last processed or NULL if no changes have been carried 
 * out. If the route has been changed by truncating to null, then NULL
 * is also returned.
 */
ITREE * pattern_getchanged(struct pattern_route *wat	/* pattern-action */ )
{
     int seq, size, r;
     time_t modt;
     ITREE *bufchain;
     ROUTE rt;

     /* get change information */
     r = route_stat(wat->key /*purl*/, NULL, &seq, &size, &modt);
     if (r != 1) {
	  wat->last_size = 0;
	  wat->last_seq  = -1;
	  wat->last_modt = 0;
	  return NULL;		/* can't report change */
     }

     /* file changes */
     if ( modt != wat->last_modt ||
	 (seq  == -1 && size != wat->last_size) ||
	 (size == -1 && seq  != wat->last_seq ) ) {

	  /* has the route data shrunk? */
	  if (seq == -1 && size < wat->last_size)
	       wat->last_size = 0;	/* assume all data is new */

	  /* fetch data from last known position */
	  rt = route_open(wat->key /*purl*/, NULL, NULL, 10);
	  if (rt == NULL) {
	       elog_printf(ERROR, "unable to open route %s for "
			   "seekread()", wat->key);
	       return NULL;
	  }
	  bufchain = route_seekread(rt, wat->last_seq+1, wat->last_size+1);
	  route_close(rt);

	  /* amend the last seed route stats with the latest */
	  wat->last_size = size;
	  wat->last_seq  = seq;
	  wat->last_modt = modt;
	  return bufchain;
     }
     else
	  return NULL;	/* no change */
}


/* 
 * Search the text buffer for the pattern compiled in `wat'.
 * If a match is found, raise the action described.
 */
void pattern_matchbuffer(ROUTE out,		/* output route */
			 ROUTE err,		/* error route */
			 TREE *palist,		/* pattern & action list */
			 struct pattern_route *wat, /* purl of test route */
			 char *buf,		/* match against this buf */
			 int rundirectly	/* event to be run directly */)
{
     int r;
     struct pattern_action *act;

     if (util_is_str_whitespace(buf))
	  return;

     /* iterate over the set of compiled patterns to find a match.
      * only one match per line is currently allowed, so the first
      * ones in the list have priority */
     tree_traverse(palist) {
	  act = tree_get(palist);
	  r = regexec(&act->comp, buf, 0, NULL, 0);
	  if (r == 0) {
	       /* raise action */
	       pattern_raiseevent(out, err, act, wat, buf, rundirectly);
	       return;		/* dont process any more patterns */
	  }
     }
}


/* raise an event */
void pattern_raiseevent(ROUTE out,	/* output route */
			ROUTE err,	/* error route */
			struct pattern_action *act, /* pattern & actions */
			struct pattern_route *wat,  /* purl of test route */
			char *text,	/* text that produced match */
			int rundirectly	/* if set, create a job */)
{
     time_t now_t;
     char summary[PATTERN_SUMTEXTLEN];
     char *outpurl, *errpurl, jobid[64];
     static int eventseq=0;
     int r;

     now_t = time(NULL);

     /* check to see if the event is embargoed:
      * sorry about the goto's, but it makes the logic easier to follow */
     act->event_count++;

     if ( act->embargo_count || act->embargo_time ) {
	  if (act->embargo_count && act->event_count   >= act->embargo_count)
	       goto evraise;
	  if (act->embargo_time  && act->event_timeout <= now_t)
	       goto evraise;
     } else
	  goto evraise;

     elog_printf(DIAG, "event raised (%s) but embargoed "
		 "(ev_ct %d < em_ct %d) (em_to=%d ev_tm %d > now %d)", text,
		 act->event_count, act->embargo_count,
		 act->embargo_time, act->event_timeout, now_t);

     return;

 evraise:

     /*
      * Prepare summary line
      * note: job has no way of accepting stdin for the message yet, so
      * we combine method args (the command) with the message
      */
     snprintf(summary, PATTERN_SUMTEXTLEN, "%s %s:%s:%s", act->action_arg,
	      elog_sevtostr(act->severity), util_decdatetime(wat->last_modt),
	      act->action_message);

     /*
      * Raise an event
      * This may be done in one of two ways:-
      * 1. directly execute the action now
      * 2. queue instructions to indirectly execute
      * This is decided by the rundirectly flag
      */
     if (rundirectly) {

	  /* convert event into a one off job request to execute NOW */
	  outpurl = xnstrdup(route_getpurl(out));
	  errpurl = xnstrdup(route_getpurl(err));
	  snprintf(jobid, 64, "pattern-%d", eventseq++);

	  elog_printf(INFO, "raise event (job id %s) to "
		      "%s %s: %s <= %s", jobid, act->action_method, 
		      act->action_arg, text, summary);

	  r = job_add(time(NULL), 0, 0, 1, jobid, "(pattern)", 
		      outpurl, errpurl, PATTERN_KEEP, 
		      act->action_method, summary /*act->action_arg*/);

	  if (r == -1)
	       elog_printf(ERROR, "unable to action event "
			   "%s %s: %s <= %s", act->action_method, 
			   act->action_arg, text, summary);

	  /* clear up and return */
	  nfree(outpurl);
	  nfree(errpurl);

     } else {
	  /* output event using a standard text format. The output queue
	   * from this mmethod will be redirected to a job mill for serial
	   * execution. There is scope for cutsomisation here, by setting
	   * output formats etc with the actions */

	  elog_printf(INFO, "event raised to %s %s: %s <= %s", 
		      act->action_method, act->action_arg, text, summary);

	  /* send to output route, which is our job queue: format is
	   * <method> <command/arguments> <message> */
	  route_printf(out, "%s %s\n", act->action_method, summary);
	  route_flush(out);

#if 0
	  /* work out what to do with it */
	  if (strcasecmp(act->action_method, "send") == 0) {
	       /* send summary line to a specific route, specified in 
		* action_args */
	       rt = route_open(act->action_arg, NULL, NULL, 10);
	       route_printf(rt, "%s\n", summary);
	       route_close(rt);
	  } else if (strcasecmp(act->action_method, "sh") == 0) {
	       /* run action_args as shell script with summary as stdin */
	       route_printf(out, "sh %s %s\n", act->action_arg, summary);
	       route_flush(out);
	  } else if (strcasecmp(act->action_method, "exec") == 0) {
	       /* exec binary action_args with summary as stdin */
	       route_printf(out, "exec %s %s\n", act->action_arg, summary);
	       route_flush(out);
	  } else {
	       route_printf(err, "unknown action method: `%s', event text: "
			    "`%s'", act->action_method, summary);
	  }
#endif
     }

     /* reset embargo counters due to success */
     act->event_count = 0;
     if (act->embargo_time)
	  act->event_timeout = now_t + act->embargo_time;
}


#if TEST

#include <stdlib.h>
#include <unistd.h>
#include "route.h"
#include "rs.h"
#include "rs_gdbm.h"
#include "rt_std.h"
#include "rt_file.h"
#include "rt_rs.h"
#include "callback.h"
#include "sig.h"
#include "runq.h"

#define TRING1 "pattern"
#define TFILE1 "t.pattern1.rs"
#define TPURL1 "rs:" TFILE1 "," TRING1 ",0"
#define TFILE2 "t.pattern2.txt"
#define TPURL2 "file:" TFILE2
#define TFILE3 "t.pattern3.rs"
#define TPATACTPURL "rs:" TFILE3 ",pattern,0"	/* data patterns */
#define TROUTEPURL  "rs:" TFILE3 ",data,0"	/* data routes */
#define TLOGTXTPURL "rs:" TFILE3 ",log,0"	/* log text */
#define LINE1 "tom, dick and harry"
#define PAT1  PATTERN_PATACT_HEAD "\n--\n" \
		"dick\t0\t0\tinfo\tsh\techo\tfound a dick word"
#define PAT2  PATTERN_PATACT_HEAD "\n--\n" \
		"dotman\t0\t0\tinfo\tsh\techo\tfound a dotman word"
#define PAT3  PATTERN_PATACT_HEAD "\n--\n" \
		"beer\t0\t3\tinfo\tsh\techo\tfound beer word"	/*em-count*/
#define PAT4  PATTERN_PATACT_HEAD "\n--\n" \
		"bra\t2\t0\tinfo\tsh\techo\tfound bra word"	/*em-time*/

int count_seq(ROUTE rt) {
     int seq, size, r;
     time_t modt;

     r = route_tell(rt, &seq, &size, &modt);
     if (r == 0)
	  return 0;
     return seq+1;
}

int count_lines(char *purl) {
     char *buf, *pt;
     int len, nlines=0;

     buf = route_read(purl, NULL, &len);
     buf[len] = '\0';
/*     if (len > 0)
       nlines++;*/
     elog_printf(DEBUG, "found %s", buf);
     pt = buf;
     while ( (pt=strchr(pt, '\n')) != NULL ) {
	  pt++;
	  nlines++;
     }
     nfree(buf);

     return nlines;
}

int main(int argc, char **argv) {
     WATCHED w1;
     ROUTE logtxt, patact, towatch, watched1, err;
     int r;
     RS rs;
     TABLE tab;
     char *tabstr;

     route_init(NULL, 0);
     route_register(&rt_filea_method);
     route_register(&rt_fileov_method);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     route_register(&rt_rs_method);
     if ( ! elog_init(1, "pattern test", NULL))
	  elog_die(FATAL, "didn't initialise elog\n");
     err = route_open("stderr", NULL, NULL, 0);
     rs_init();

     unlink(TFILE1);
     unlink(TFILE2);
     unlink(TFILE3);

     logtxt = route_open(TLOGTXTPURL, "logtxt queue", NULL, 100);

     /* test 1: run watch without any files being set up */
     w1 = pattern_init(logtxt, err, TPATACTPURL, TROUTEPURL);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[1] action failed");
     pattern_fini(w1);

     /* test 2: run watch as files 'suddenly' appear */
     w1 = pattern_init(logtxt, err, TPATACTPURL, TROUTEPURL);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[2a] action failed");
             /* 2b */
     towatch = route_open(TROUTEPURL, "route watch", NULL, 10);
     if (towatch == NULL)
	  elog_die(FATAL, "[2b] can't write to watched list");
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[2b] action failed");
             /* 2c */
     route_printf(towatch, "%s\n", TPURL1);
     route_flush(towatch);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[2c] action failed");
             /* 2d */
     patact = route_open(TPATACTPURL, "patterns and actions", NULL, 10);
     if (patact == NULL)
	  elog_die(FATAL, "[2d] can't write to patact list");
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[2d] action failed");
             /* 2e */
     tab = table_create();
     table_freeondestroy(tab, tabstr = xnstrdup(PAT1));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     route_twrite(patact, tab);
     table_destroy(tab);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[2e] action failed");
             /* 2f : and again... */
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[2f] action failed");
     pattern_fini(w1);

     /* test 3: lets match some patterns */
     w1 = pattern_init(logtxt, err, TPATACTPURL, TROUTEPURL);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3a] action failed");
             /* 3b */
     watched1 = route_open(TPURL1, "watched subject 1", NULL, 100);
     if (watched1 == NULL)
	  elog_die(FATAL, "[3b] can't write to watched1 route");
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3b1] action failed");
     route_printf(watched1, "mary had a little lamb\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3b2] action failed");
             /* 3c */
     route_printf(watched1, "postman pat, postman pat, postman pat and...\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3c] action failed");
             /* 3d: should find this line */
     route_printf(watched1, "tom, dick and harry\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3d] action failed");
     if (count_seq(logtxt) != 1)
          elog_die(FATAL, "[3d] lines != 1 (%d)", count_seq(logtxt));
             /* 3e: shouldn't repeat  */
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3e] action failed");
     if (count_seq(logtxt) != 1)
	  elog_die(FATAL, "[3e] lines != 1");

     sleep(1);	/* make further tests work */

             /* 3f: more text */
     route_printf(watched1, "he was an old cloth cat, worn and a bit saggy "
		  "at the seams; but emily loved him\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3f1] action failed");
     route_printf(watched1, "dotmat, dotman to the rescue\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3f2] action failed");
             /* 3g: should find this line */
     tab = table_create();
     table_freeondestroy(tab, tabstr = xnstrdup(PAT1));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, tabstr = xnstrdup(PAT2));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     route_twrite(patact, tab);
     table_destroy(tab);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3g1] action failed");
             /* and again... */
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3g2] action failed");
             /* 3h: should find these lines */
     route_printf(watched1, "tom, dick and harry\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3h1] action failed");
     if (count_seq(logtxt) != 2)
	  elog_die(FATAL, "[3h1] lines != 2");
     route_printf(watched1, "dotmat, dotman to the rescue\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3h2] action failed");
     if (count_seq(logtxt) != 3)
	  elog_die(FATAL, "[3h2] lines != 3");
             /* 3i: two potential matches should produce only one 
	      * event (the first) */
     route_printf(watched1, "dotman rescued dick from certain peril\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3i] action failed");
     if (count_seq(logtxt) != 4)
	  elog_die(FATAL, "[3i] lines != 4");
             /* 3j: multiple matches */ 
     route_printf(watched1, "dotman to the rescue\n"
		  "no one expects the spanish enquisition\n"
		  "dotman to the rescue");
     route_flush(watched1);
     route_printf(watched1, "dick, dick, dick\n"
		  "dick richard dick\n"
		  "tum-te-tum\n"
		  "dickey-de-dick");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[3j] action failed");
     if (count_seq(logtxt) != 9)
	  elog_die(FATAL, "[3j] lines != 9");

     /* test 4: truncation, reading should start from the begining */
     rs = rs_open(&rs_gdbm_method, TFILE1, 0644, TRING1, "being watched", 
		  "file to be watched", 10, 0, 0);
     rs_purge(rs, 999);
     rs_close(rs);
     route_printf(watched1, "dotman to the rescue\n"
		  "no one expects the spanish enquisition\n"
		  "dotman to the rescue");
     route_flush(watched1);
     route_printf(watched1, "dick, dick, dick\n"
		  "dick richard dick\n"
		  "tum-te-tum\n"
		  "dickey-de-dick");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[4] action failed");
     if (count_seq(logtxt) != 14)	/* another 5 events */
	  elog_die(FATAL, "[4] lines != 14");
     pattern_fini(w1);

     /* test 5: check state persistance, no new events should be raised */
     w1 = pattern_init(logtxt, err, TPATACTPURL, TROUTEPURL);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[5] action failed");
     if (count_seq(logtxt) != 14)
	  elog_die(FATAL, "[5] lines != 14");
     pattern_fini(w1);

     /* test 6: count embargo */
             /* new pattern with embargo */
     w1 = pattern_init(logtxt, err, TPATACTPURL, TROUTEPURL);
     tab = table_create();
     table_freeondestroy(tab, tabstr = xnstrdup(PAT1));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, tabstr = xnstrdup(PAT2));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, tabstr = xnstrdup(PAT3));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     route_twrite(patact, tab);
     table_destroy(tab);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[6a] action failed");
     route_printf(watched1, "beer 1, should be found\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[6b] action failed");
     if (count_seq(logtxt) != 15)
	  elog_die(FATAL, "[6b] lines != 15");
     route_printf(watched1, "beer 2, should not be detected\n");
     route_flush(watched1);
     route_printf(watched1, "beer 3, should not be detected\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[6c] action failed");
     if (count_seq(logtxt) != 15)
	  elog_die(FATAL, "[6c] lines != 15");
     route_printf(watched1, "beer 4, should be found\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[6d] action failed");
     if (count_seq(logtxt) != 16)
	  elog_die(FATAL, "[6d] lines != 16");
     route_printf(watched1, "beer 5, should not be detected\n");
     route_flush(watched1);
     route_printf(watched1, "beer 6, should not be detected\n");
     route_flush(watched1);
     route_printf(watched1, "beer 7, should be found\n");
     route_flush(watched1);
     route_printf(watched1, "beer 8, should not be detected\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[6e] action failed");
     if (count_seq(logtxt) != 17)
	  elog_die(FATAL, "[6e] lines != 17");
     pattern_fini(w1);

     /* test 7: time embargo */
     w1 = pattern_init(logtxt, err, TPATACTPURL, TROUTEPURL);
     tab = table_create();
     table_freeondestroy(tab, tabstr = xnstrdup(PAT1));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, tabstr = xnstrdup(PAT2));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, tabstr = xnstrdup(PAT3));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, tabstr = xnstrdup(PAT4));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     route_twrite(patact, tab);
     table_destroy(tab);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[7a] action failed");
             /* first one allowed */
     route_printf(watched1, "bra 1, should be found \n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[7b] action failed");
     if (count_seq(logtxt) != 18)
	  elog_die(FATAL, "[7b] lines != 18");
     sleep(1);
     route_printf(watched1, "bra 2, should not be detected\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[7c] action failed");
     if (count_seq(logtxt) != 18)
	  elog_die(FATAL, "[7c] lines != 18");
     sleep(1);
     route_printf(watched1, "bra 3, should be found\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[7d] action failed");
     if (count_seq(logtxt) != 19)
	  elog_die(FATAL, "[7d] lines != 19");
     route_printf(watched1, "bra 4, should not be detected\n");
     route_flush(watched1);
     route_printf(watched1, "bra 5, should not be detected\n");
     route_flush(watched1);
     route_printf(watched1, "bra 6, should not be detected\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[7e] action failed");
     if (count_seq(logtxt) != 19)
	  elog_die(FATAL, "[7e] lines != 19");
     sleep(2);
     route_printf(watched1, "bra 7, should be found\n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[7f] action failed");
     if (count_seq(logtxt) != 20)
	  elog_die(FATAL, "[7f] lines != 20");
     pattern_fini(w1);

     /* test 8: direct submission to job class */
     callback_init();
     sig_init();
     meth_init();
     runq_init(time(NULL));
     job_init();
     sig_on();
     w1 = pattern_init(logtxt, err, TPATACTPURL, TROUTEPURL);
     pattern_rundirectly(w1, 1);
     tab = table_create();
     table_freeondestroy(tab, tabstr = xnstrdup(PAT1));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, tabstr = xnstrdup(PAT2));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, tabstr = xnstrdup(PAT3));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, tabstr = xnstrdup(PAT4));
     table_scan(tab, tabstr, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     route_twrite(patact, tab);
     table_destroy(tab);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[8a] action failed");
             /* first one allowed */
     route_printf(watched1, "bra 8, should be found \n");
     route_flush(watched1);
     r = pattern_action(w1, logtxt, err);
     if (r != 0)
	  elog_die(FATAL, "[8b] action failed");
     sleep(2);
     pattern_fini(w1);
     job_fini();
     runq_fini();
     meth_fini();
     callback_fini();

     /* shutdown for memory check */
     route_close(watched1);
     route_close(towatch);
     route_close(patact);
     route_close(logtxt);
     route_close(err);
     rs_fini();
     elog_fini();
     route_fini();
     fprintf(stderr, "%s: tests finished successfully\n", argv[0]);
     exit(0);
}

#endif /* TEST */
