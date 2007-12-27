/*
 * Record - route watching and change recording class
 * Nigel Stuckey, April 2000
 *
 * Copyright System Garden Ltd 2000-2001, all rights reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "record.h"
#include "nmalloc.h"
#include "util.h"

/*
 * Initialise a recording session.
 * Routes specified by the list in `watch' are recorded in a version store 
 * using a file specified by `target' as its base. The output version store
 * is contained within its own namespace in the file.
 * If there are routes newer that their equivalent recording, then a new
 * recording is made and the version is incremented by one.
 * Record_action() should be called each time a new check is required.
 */
RECINFO record_init(ROUTE out,		/* route output */
		    ROUTE err,		/* route errors */
		    char *target,	/* base file for recordings */
		    char *watch		/* p-url of routes to watch */ )
{
     RECINFO w;

     if (target == NULL || *target == '\0')
	  elog_die(FATAL, "no target file");

     if (watch == NULL || *watch == '\0')
	  elog_die(FATAL, "no watch route");

     w = xnmalloc(sizeof(struct record_info));
     w->target     = xnstrdup(target);
     w->watch      = xnstrdup(watch);
     w->watch_modt = 0;
     w->watch_size = 0;
     w->watch_seq  = 0;
     w->watch_rt   = NULL;
     w->watchlist  = NULL;

     /* kick off the first recording round, which should just force load
      * everything, scan the routes to be watched and set the times up */
     record_action(w, out, err);

     return w;
}

/*
 * Shut down recording session
 */
void record_fini(RECINFO w)
{
     struct record_route  *wat;

     nfree(w->target);
     nfree(w->watch);
     if (w->watch_rt != NULL)
	  route_close(w->watch_rt);

     /* remove watch list */
     if (w->watchlist != NULL) {
	  itree_traverse(w->watchlist) {
	       /* delete watched nodes */
	       wat = tree_get(w->watchlist);
	       nfree( tree_getkey(w->watchlist) );
	       nfree( wat );
	  }
	  tree_destroy(w->watchlist);
     }

     nfree(w);
}

/*
 * Carry out the watch & log actions on all the observed routes
 */
int record_action(RECINFO w,	/* record instance */
		  ROUTE out,	/* route output */
		  ROUTE err	/* route errors */ )
{
     struct record_route  *wat;

     /* load watch list */
     record_load_watch(w);

     /* do I want to start? */
     if (w->watchlist == NULL)
	  return 0;	/* nothing happened successfully !! */

     /* iterate over watch list and stat the routes, raising events 
      * when matches are found */
     tree_traverse(w->watchlist) {

	  /* stat file to see if it has been changed */
	  wat = tree_get(w->watchlist);
	  if ( record_haschanged(wat) ) {

	       /* save the file in the appropreate version store */
	       record_save(out, err, w, wat);

	  }
     }

     return 0;		/* success */
}


/*
 * Stat the watch list route to see if the data has been touched, using
 * the data in the instance's record_info structure.
 * If there is no change, leave it all. If there is a change, then read 
 * and update list
 * Returns 1 for successfully loaded data or 0 if the data is not available
 */
int record_load_watch(RECINFO w		/* watch instance */ )
{
     char *watchbuf, *tok;
     struct record_route *wat;
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
     if ( modt != w->watch_modt || 
	 (seq  == -1 && size != w->watch_size) ||
	 (size == -1 && seq  != w->watch_seq ) ) {

	  /* route has been modified */
	  w->watch_modt = modt;
	  w->watch_size = size;
	  w->watch_seq  = seq;

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

	       /* do the work */
	       if ( (wat = tree_find(w->watchlist, tok)) == TREE_NOVAL) {
		    /* route not watched, so do it...
		     * Attempt to load each route to find its base size.
		     * We then assume that we do not want to look at data
		     * before the route was refered to us. */
		    wat = xnmalloc( sizeof( struct record_route ) );
		    wat->key       = xnstrdup(tok);
		    wat->ref       = 1;
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
 * Returns 1 if the purl described by `wat' was been changed since last
 * processed or 0 if no changes have been carried out
 * If the route has been changed by truncating to null, then 1
 * is also returned.
 */
int record_haschanged(struct record_route *wat	/* watch */ )
{
     int seq, size;
     time_t modt;

     /* get change information */
     /*r = */ route_stat(wat->key /*purl*/, NULL, &seq, &size, &modt);
#if 0
     if (r != 1) {
	  wat->last_size = 0;
	  wat->last_seq  = 0;
	  wat->last_modt = 0;
	  return 0;		/* can't report change, say none */
     }
#endif

     /* file changes */
     if ( modt != wat->last_modt ||
	 (seq  == -1 && size != wat->last_size) ||
	 (size == -1 && seq  != wat->last_seq ) ) {

	  /* has the route data shrunk? */
	  if (seq == -1 && size < wat->last_size)
	       wat->last_size = 0;	/* yes! */

	  /* amend the last seed route stats with the latest */
	  wat->last_size = size;
	  wat->last_seq  = seq;
	  wat->last_modt = modt;
	  return 1;	/* change */
     }
     else
	  return 0;	/* no change */
}



/* Save the route in a version store route in the file specified
 * as target */
void record_save(ROUTE out,			/* output route */
		 ROUTE err,			/* error route */
		 RECINFO w,			/* contains target */
		 struct record_route *wat	/* purl of test route */)
{
     char rtvername[256], rtvercmt[256];
     char *data;
     ROUTE rt;
     int length;

     /* collect data and handle special cases */
     data = route_read(wat->key, NULL, &length);
     if (data == NULL) {
	  elog_printf(INFO, "monitored route %s does not exist",
		      wat->key);
	  return;
     }
     if (length == 0) {
	  elog_printf(INFO, "monitored route %s is zero length",
		      wat->key);
	  nfree(data);
	  return;
     }

     /* prepare output name by taking the storage type, the target filename
      * a standard namespace prefix and finally the monitored route, with 
      * all the special tokens replace to avoid problems. */
     snprintf(rtvername, 250, "%s%s%s%s", RECORD_VERPREFIX, w->target,
	      RECORD_RINGPREFIX, wat->key);

     /* open the output version store route */
     snprintf(rtvercmt, 256, "changes in route %s", wat->key);
     rt = route_open(rtvername, rtvercmt, NULL, 100);
     if (rt == NULL) {
	  /* truncate data for log message */
	  if (length > 40)
	       data[40] = '\0';
	  elog_printf(ERROR, "route changed (%s) but unable to open "
		      "output (%s) to record. Data is: %s%s", 
		      wat->key, rtvername, data, 
		      (length > 40 ? "...(truncated)" : ""));
     } else {
	  /* copy route data across */
	  if (route_write(rt, data, length) == -1) {
	       /* truncate data for log message */
	       if (length > 40)
		    data[40] = '\0';
	       elog_printf(ERROR, "route changed (%s), output opened "
			   "(%s) but unable to write. Data is: %s%s", 
			   wat->key, rtvername, data, 
			   (length > 40 ? "...(truncated)" : ""));
	  }
	  route_close(rt);
     }
     nfree(data);

     return;
}


#if TEST

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "timestore.h"
#include "rs.h"
#include "rt_file.h"
#include "rt_std.h"
#include "rt_rs.h"

#define TRING1 "t1"
#define TFILE1 "t.record1.rs"
#define TPURL1 "rs:" TFILE1 "," TRING1 ",0"
#define TPURL1CHANGES RECORD_VERPREFIX TFILE3 RECORD_RINGPREFIX TPURL1
#define TFILE2 "t.record2.txt"
#define TPURL2 "file:" TFILE2
#define TPURL2CHANGES RECORD_VERPREFIX TFILE3 RECORD_RINGPREFIX TPURL2
#define TFILE3 "t.record3.dat"
#define TROUTEPURL "ver:" TFILE3 ",d.r.t1"	/* data routes */
#define LINE1 "tom, dick and harry"
#define PAT1 "dick 0 0 info sh cat \"found a dick word\""
#define PAT2 "dotman 0 0 info sh cat \"found a dotman word\""
#define PAT3 "beer 0 3 info sh cat \"found beer word\""	/*em-count*/
#define PAT4 "bra 2 0 info sh cat \"found bra word\""	/*em-time*/

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
     RECINFO w1;
     ROUTE towatch, watched1, watched2;
     TS_RING ts;
     int r, seq, size;
     time_t modt;
     ROUTE out, err;

     route_init(NULL, 0);
     route_register(&rt_filea_method);
     route_register(&rt_fileov_method);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     route_register(&rt_rs_method);
     if ( ! elog_init(1, "route test", NULL))
	  elog_die(FATAL, "didn't initialise elog\n");
     out = route_open("stdout", NULL, NULL, 0);
     err = route_open("stderr", NULL, NULL, 0);
     rs_init();
     sig_init();
     callback_init();
     runq_init(time(NULL));
     meth_init();
     job_init();
     sig_on();

     unlink(TFILE1);
     unlink(TFILE2);
     unlink(TFILE3);

     /* test 1: run record without any files being set up */
     w1 = record_init(out, err, TFILE3, TROUTEPURL);
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[1] action failed");
     record_fini(w1);

     /* test 2: run watch as files 'suddenly' appear */
     w1 = record_init(out, err, TFILE3, TROUTEPURL);
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[2a] action failed");
             /* 2b */
     towatch = route_open(TROUTEPURL, "route watch", NULL, 10);
     if (towatch == NULL)
	  elog_die(FATAL, "[2b] can't write to watched list");
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[2b] action failed");
             /* 2c: write the first monitored file to the watch list */
     route_printf(towatch, "%s\n", TPURL1);
     route_flush(towatch);
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[2c] action failed");
             /* 2d : and again... */
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[2d] action failed");

     /* test 3: create watched route */
     watched1 = route_open(TPURL1, "This should be subject to monitoring", 
			   NULL, 10);
     route_printf(watched1, "blah blah blah");
     route_flush(watched1);
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[3] action failed");
     if (route_access(TPURL1CHANGES, "", NULL, 10) == 0)
	  elog_die(FATAL, "[3] can not access new results");

     /* test 4: lets change some files */
     route_printf(watched1, "This route should now be changed");
     route_flush(watched1);
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[4] action failed");
     r = route_stat(TPURL1CHANGES, &seq, &size, &modt);
     if (r != 1)
	  elog_die(FATAL, "[4] stat failed");
     if (seq != 2)	/* next sequence */
	  elog_die(FATAL, "[4] seq != 2");

     /* test 5: truncation & deletion: new version should be recorded
      * which should be blank */
     ts = ts_open(TFILE1, TRING1, NULL);
     ts_purge(ts, 999);
     ts_close(ts);
     route_printf(watched1, " ");	/* unable to save empty buffer! */
     route_flush(watched1);
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[5] action failed");
     r = route_stat(TPURL1CHANGES, &seq, &size, &modt);
     if (r != 1)
	  elog_die(FATAL, "[5] stat failed");
     if (seq != 3)	/* next sequence */
	  elog_die(FATAL, "[5] seq != 3");

     /* test 6: now add a file based route */
     route_printf(towatch, "%s\n%s\n", TPURL1, TPURL2);
     route_flush(towatch);
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[6a] action failed");
     if (tree_n(w1->watchlist) != 2)
	  elog_die(FATAL, "[6a] watchlist != 2");
             /* 6b : and again... */
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[6b] action failed");

     /* test 7: create watched route */
     watched2 = route_open(TPURL2, "This should also be monitored", NULL, 10);
     route_printf(watched2, "blah blah blah");
     route_flush(watched2);
     r = record_action(w1, out, err);
     if (r != 0)
	  elog_die(FATAL, "[7] action failed");
     if (route_access(TPURL2CHANGES, "", NULL, 10) == 0)
	  elog_die(FATAL, "[7] can not access new results");

REACHED HERE

     /* test 8: lets change some files */
     route_printf(watched2, "This route should now be changed");
     route_flush(watched2);
     r = record_action(w1, err, err);
     if (r != 0)
	  elog_die(FATAL, "[8] action failed");
     r = route_stat(TPURL2CHANGES, &seq, &size, &modt);
     if (r != 1)
	  elog_die(FATAL, "[8] stat failed");
     if (size != 46)
	  elog_die(FATAL, "[8] size != 46");

     /* test 9: deletion: error generated */
     unlink(TFILE2);
     r = record_action(w1, err, err);
     if (r != 0)
	  elog_die(FATAL, "[9] action failed");
     r = route_stat(TPURL2CHANGES, &seq, &size, &modt);
     if (r != 1)
	  elog_die(FATAL, "[9] stat failed");
     if (size != 46)
	  elog_die(FATAL, "[9] size != 46");

     /* test 10: null file: empty result */
     creat(TFILE2, 0644);
     r = record_action(w1, err, err);
     if (r != 0)
	  elog_die(FATAL, "[10] action failed");
     r = route_stat(TPURL2CHANGES, &seq, &size, &modt);
     if (r != 1)
	  elog_die(FATAL, "[10] stat failed");
     if (size != 46)
	  elog_die(FATAL, "[10] size != 46");
     record_fini(w1);

     /* test 11: check state persistance, no new events should be raised */
     route_printf(watched2, "some more text");
     route_flush(watched2);
     w1 = record_init(err, err, TFILE3, TROUTEPURL);
     r = record_action(w1, err, err);
     if (r != 0)
	  elog_die(FATAL, "[11] action failed");
     r = route_stat(TPURL2CHANGES, &seq, &size, &modt);
     if (r != 1)
	  elog_die(FATAL, "[11] stat failed");
     if (size != 46)
	  elog_die(FATAL, "[11] lines != 46");

     /* shutdown for memory check */
     route_close(watched1);
     route_close(watched2);
     record_fini(w1);     
     route_close(towatch);
     route_close(err);
     elog_fini();
     route_fini();
     fprintf(stderr, "%s: tests finished successfully\n", argv[0]);
     exit(0);
}

#endif /* TEST */
