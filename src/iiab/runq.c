/*
 * Run queue
 * Holds repetitive pieces of work and executes them in order at their
 * correct times. The execution methods are selectable and the timing
 * details have a superset of the functionality of cron(1) and at(1).
 * Even with one-off work requests, runq has the effect of a multiplexing
 * alarm() call.
 *
 * Obviously, do not use alarm() in conjunction with runq.
 *
 * Nigel Stuckey, August 1996
 * Revised March 1997
 * Renamed, reorganised and consolidated December 1997
 * Algorithm change December 2000
 *
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "nmalloc.h"
#include "util.h"
#include "sig.h"
#include "itree.h"
#include "runq.h"
#include "elog.h"
#include "callback.h"
#include "meth.h"

/*
 * Internally, all work is placed in the file global runq_tab. 
 * Runq continually revises a schedule of execution (called an event list) 
 * in global runq_event, which indexes entries in global runq_tab.
 *
 * runq_schedw() schedules specific jobs that are referred to it, and is
 * called by runq_dispatch() and runq_add().
 * runq_dispatch() is called by the SIGALRM and traverses the event queue
 * to start the the peices of work.
 *
 * For efficiency, repeated work execution is gathered together in `runs', 
 * detected by runq_schedw() and called by runq_dispatch(). These are
 * generally calls to open and shut I/O for efficiency.
 * Sometimes, the final execution in a run takes a while to complete, so 
 * a dummy run is scheduled for a further interval but the work structure 
 * is flagged as expired. This procedure will repeat untill the job has 
 * completed.
 *
 * If the clearup flag is set in the runq_work structure, the entry will
 * be removed from runq_tab.
 */
ITREE *runq_tab;	/* List of accepted work. Once work has expired and 
			 * its last method completed, it will be removed
			 * from this list. Keyed by a unique id that is 
			 * constant acorss the lifetimne of the work run */
ITREE *runq_event;	/* Time ordered tree of work references to runq_tab. 
			 * Represents the times at which the next execution 
			 * of each peice of work should occur. Once carried 
			 * out, the next execution should be rescheduled.
			 * Keyed by next execution time */
time_t runq_startup;	/* Time at which the runq was started */
int    runq_drain;	/* If set, don't dispatch any more work */
int    runq_nextid=0;	/* The id counter */

/* Initialise work queues and install the signal handlers */
void runq_init(time_t startup	/* time queue was initialised */ )
{
     /* set callbacks: alarm signal and meth finished */
     sig_setalarm(runq_sigdispatch);
     callback_regcb(METH_CB_FINISHED, (void *) runq_methfinished);

     runq_tab = itree_create();		/* Cached requests */
     runq_event = itree_create();	/* List of events */
     runq_startup = startup;		/* Resister the start time */
     runq_drain = 0;			/* Normally dispatch work */
}

void runq_fini()
{
     struct runq_work *w;

     alarm(0);
     if (runq_tab) {
          while ( ! itree_empty(runq_tab) ) {
	       itree_first(runq_tab);
	       w = itree_get(runq_tab);
	       nfree(w->desc);
	       nfree(w->argument);
	       nfree(w);
	       itree_rm(runq_tab);
	  }
	  itree_destroy(runq_tab);
     }
     if (runq_event) {
          while ( ! itree_empty(runq_event) ) {
	       itree_first(runq_event);
	       itree_rm(runq_event);
	  }
	  itree_destroy(runq_event);
     }
}

/* List the event and work trees in a combined way */
void runq_dump() {
     struct runq_work *w;
     time_t t;
/*     char *buf;*/
     
     elog_startsend(DEBUG, "Work events -----\n");
     /* List the event queue */
     itree_traverse(runq_event) {
	  w = itree_get(runq_event);
	  t = itree_getkey(runq_event);
/*	  tm = localtime(&t);
	  strftime(str, RUNQ_TMPBUF, "%d-%b-%y %H:%M:%S", tm);
	  buf = util_bintostr(RUNQ_TMPBUF, w->argument, w->arglen);*/
	  elog_contprintf(DEBUG, "%8s %8s %2d %2d %3d %3d %10x %s\n", 
			  w->desc, util_shortadaptdatetime(t), w->start, 
			  w->interval, w->phase, w->count, w->command, 
			  w->expired?"done":"crnt");
/*	  nfree(buf);*/
     }
     /* List the work not in the event queue */
     itree_traverse(runq_tab) {
	  w = itree_get(runq_tab);
	  itree_traverse(runq_event)
	       if (itree_get(runq_event) == w)
		    goto ineventq;
	  /*buf = util_bintostr(RUNQ_TMPBUF, w->argument, w->arglen);*/
	  elog_contprintf(DEBUG, 
			  "%8s unshd    %2d %2d %3d %3d %10x\n", 
			  w->desc, w->start, w->interval, w->phase, w->count,
			  w->command);
/*	  nfree(buf);*/
     ineventq:
     	;
     }
     elog_endsend(DEBUG, "-----------------");
}

/* 
 * Capture the information passed and add it to the work table.
 * Once added, work is rescheduled, which may including running the
 * job straight away. In this case, no unique key is handed back.
 * Consequently, work MUST be ready to run when runq_add() is called.
 * (*startofrun)() and (*endofrun)() may be NULL, in which case they 
 * are not called.
 * Note, argument is reparented by runq_add() and the caller should
 * not attempt to free it.
 * Returns a reference of 0 or greater to the work record inserted 
 * in the work queue, which can be used with runq_rm(). 
 * If there is a failure, return -1, although the argument is still adopted 
 * and freed.
 * If the work is completed and expired during this call, return -2, which
 * is a successful state but not one which can return an ID.
 */
int runq_add(long start,	/* time work should start in GMT */
	     long interval,	/* time in between each run (seconds) */
	     long phase,	/* order in each scheduled time point */
	     long count,	/* number of times to run (0=continuous) */
	     char *desc,	/* description or string id of work */
	     int (*startofrun)(),/* Call at start of run set */
	     int (*command)(),	/* Command to execute */
	     int (*isrunning)(),/* Test for command still running */
	     int (*endofrun)(),	/* Call at end of run set */
	     void *argument,	/* nmalloc()ed argument buffer to reparent */
	     int arglen		/* Length of argument buffer */ )
{
     struct runq_work *work;

     if (start <0 || interval <0 || phase <0 || count <0 || command == NULL) {
          elog_printf(ERROR, "bad parameter: %d %d %d %d %p",
		      start, interval, phase, count, command);
	  nfree(argument);
	  return -1;
     }

     /* Create and assign a queue structure */
     work             = xnmalloc(sizeof(struct runq_work));
     work->start      = (start == 0 ? runq_startup : start);
     work->interval   = interval;
     work->phase      = phase;
     work->count      = count;
     work->desc       = xnstrdup(desc);
     work->startofrun = startofrun;
     work->command    = command;
     work->isrunning  = isrunning;
     work->endofrun   = endofrun;
     work->argument   = argument;
     work->arglen     = arglen;
     work->nruns      = 0;
     work->expired    = 0;
     work->clearup    = 0;
     
     /* Debug: List the addition to the work queue */
     elog_printf(DEBUG, "%s %d %d %d starts %.25s",
		 desc, interval, phase, count, util_shortadaptdatetime(start));

     /* Add to table of all work, schedule into an event and run if needed */
     itree_add(runq_tab, runq_nextid, work);
     if (runq_schedw(work, 0, time(NULL)) == 0)
	  return -2;
     runq_setdispatch();
     /*runq_dispatch();*//*try -NS 25/12/00*/
     if (itree_find(runq_tab, runq_nextid) == ITREE_NOVAL)
	  return -2;
     else
	  return runq_nextid++;
}

/*
 * Remove work indexed by ikey from the runq_tab.
 * This will not stop any running work, but will prevent any further work
 * from being dispatched.
 * All the structures and storage will be removed.
 * Returns 1=success, 0=error.
 */
int runq_rm(int ikey /* work key */) {
     struct runq_work *w;
     int r;
     
     /* locate work structure in table */
     if ((w = itree_find(runq_tab, ikey)) == ITREE_NOVAL) {
          elog_printf(DEBUG, "work doesn't exist with id %d", ikey);
	  return 0;
     }

     /* flag as expired */
     w->expired++;

     /* remove work from event table if scheduled */
     itree_traverse(runq_event)	{
	  if (itree_get(runq_event) == w) {
	       itree_rm(runq_event);
	       break;
	  }
     }

     /* is the work running? */
     if (w->isrunning && (*w->isrunning)(w->argument, w->arglen)) {
	  /* schedule a clearup attempt after a the defult time */ 
	  itree_add(runq_event, time(NULL) + RUNQ_EXPIREWAITDEF, 
		    itree_get(runq_tab));
	  elog_printf(DEBUG, "%s expired but removal delayed", w->desc);

	  return 1;	/* still a success */
     }

     /* work is not running, clear up now */
     itree_rm(runq_tab);		/* Remove work */
     if (w->nruns > 0 && w->endofrun) {
	  /* if the work is in the event queue and it has already
	   * been run before, assume that it is part of a run and
	   * call its endofrun() routine */
	  elog_printf(DEBUG, "end-of-run for doomed job %s (%d runs)", 
		      w->desc, w->nruns);
	  r = (*w->endofrun)(w->argument, w->arglen);
	  if (r == -1)
	       elog_printf(ERROR, "endofrun() failed for %s", w->desc);
     }
     
     elog_printf(DEBUG, "job %s removed by request", w->desc);

     nfree(w->desc);
     nfree(w->argument);
     nfree(w);

     return 1;
}


/*
 * Completely empty the runq and event trees
 */
void runq_clear() {
     struct runq_work *d;

     elog_send(DEBUG, "runq_clear() remove everything");

     /* Remove the contents (references) of the runq_event tree */
     runq_schedrmall();

     /* Traverse the runq_tab tree and delete the associated storage */
     itree_first(runq_tab);
     while (!itree_empty(runq_tab)) {
	  d = itree_get(runq_tab);
	  itree_rm(runq_tab);	/* Remove reference from tree */

	  nfree(d->desc);		/* Remove record */
	  nfree(d->argument);
	  nfree(d);
     }

}


/* Return the number of non-expired jobs in the potential work table */
int runq_ntab() {
     struct runq_work *w;
     int n=0;

     itree_traverse(runq_tab) {
	  w = itree_get(runq_tab);
	  if ( ! w->expired )
	       n++;
     }

     return n;
}


/* Return the number of non-expired jobs that are scheduled */
int runq_nsched() {
     struct runq_work *w;
     int n=0;

     itree_traverse(runq_event) {
	  w = itree_get(runq_event);
	  if ( ! w->expired )
	       n++;
     }

     return n;
}


/*
 * Schedule the next execution of work specified by ikey in runq_tab
 * Returns 1 if successfully scheduled, 0 if not scheduled because all the
 * work in in the past, or -1 if the key does not exist. 
 * If failed, it is up to the caller to remove the entry from runq_tab().
 */
int  runq_sched(int ikey,	/* index key of work */
		time_t last,	/* last time this work was run or 0 
				 * if new or unknown */
		time_t now	/* reference time, supplied by caller
				 * and should usually be time(NULL) */)
{
     struct runq_work *w;

     w = itree_find(runq_tab, ikey);
     if (w == ITREE_NOVAL)
	  return -1;

     return runq_schedw(w, last, now);
}

/*
 * Schedule the next execution of work specified by runq_work (the list
 * of potential work) and place the instruction to run into runq_event.
 * Work is scheduled at a number of seconds past the epoc in GMT and at
 * periods thereafter until the end of its run. 
 * The next eligable execution is NOW or the next FOREWARD time; past or
 * missed times are always ignored.
 *
 * The data contained in each peice of work and their rules are:-
 * start    = Execute the work `start' seconds from unix epoch GMT.
 *	      If start=0, the dispatch class initialisation time is used.
 * interval = Wait interval seconds after start time before executing line
 *	      If interval=0, line is started immedatly and `count' is ignored
 * phase    = Order that work is run at each time point: if multiple pieces
 *            of work occur for the same second, those with lower phases
 *            are executed first. Those with identical phases have an 
 *            indeterminate order.
 * count    = Execute line `count' times, waiting `interval' seconds between
 *	      If count=0, repeat lines indefinately
 * desc     = text description of work
 * startofrun=Code run when a run of executions is expected
 * command  = Address of function to run, prototype: int func(void *)
 * endofrun = code run by this routine when there are no further executions
 * argument = Buffer to pass to function
 * arglen   = Length of argument buffer
 * nruns    = number of times this work has been executed
 *
 * This function does not execute anything: start-of-run, end-of-run or
 * the execution. That is done by runq_dispatch() ONLY.
 *
 * Returns 1 if successfully scheduled as an event or 0 if the work has
 * passed and there are no further eligable executions in the run.
 * This includes when runq has been disabled and is being drained.
 */
int  runq_schedw(struct runq_work *w,	/* work structure */
		 time_t lastw,		/* last time this work was run
					 * or 0 if new or unknown */
		 time_t now		/* reference time, supplied by caller
					 * and should usually be time(NULL) */)
{
     time_t last, base, next, final;

     /* if set, don't dispatch any more work; allow to drain down */
     if (runq_drain)
          return 0;	/* disabled: draining */

     /* set up times */
     last  = lastw == 0    ? now          : lastw;
     base  = w->start == 0 ? runq_startup : w->start;
     final = base + (w->count-1) * w->interval;

     /* reform questionable parameters before they harm someone */
     if (w->interval == 0 && w->count != 1) {
          w->count = 1;		/* only count==1 make sense */
	  elog_printf(WARNING, "%s set count=1 as interval==0", w->desc);
     }
     if (w->interval == 0)
	  w->interval = 1;

     /* catagorise events and handle separately */
     if (base > now) {
	  /* future events */
	  next = base;
     } else if (w->count != 0 && final < now) {
	  /* non-continous and past */
	  next = 0;		/* work is in the past */
     } else {
	  /* continuous or current */
	  next = base + ((last - base) / w->interval +1) * w->interval;
	  if (lastw != 0 && next == lastw) {
	       /* don't reschedule at the same as last time */
	       next += w->interval;
	       if (next > final) {
		    next=0;	/* work is in the past */
	       }
	  }
     }

     /* insert work as an event and log it */
     if (next) {
	  /* work is current */
          elog_printf(DEBUG, "%s next run at %s (in %ds)", w->desc, 
		      util_decdatetime(next), next-now);

	  /* DEBUG: check for duplicates */
	  itree_traverse(runq_event)
	       if (itree_get(runq_event) == w) {
		    runq_dump();
		    elog_die(FATAL, "found a duplicate in runq_event "
			     "for %s", w->desc);
	       }

	  itree_add(runq_event, next, w);

	  return 1;
     } else {
	  /* work is in the past */
	  if (w->nruns)
	       elog_printf(DEBUG, "%s expired (%d runs)", w->desc, 
			   w->nruns);
	  else
	       elog_printf(DEBUG, "%s expired (never run)", w->desc);

	  return 0;
     }
}

/*
 * Remove all events and fill the list by traversing all the accepted 
 * work and schedule each one
 */
void runq_schedall() {
     runq_schedrmall();     /* Remove the contents of the runq_event tree */
     
     /* Schedule each piece of in the runq_tab tree */
     itree_traverse(runq_tab)
	  runq_sched(itree_getkey(runq_tab), 0, time(NULL));
}

/*
 * Remove the planned execution of work specified by runq_tab's ikey.
 * Returns 1 for success or 0 for failure.
 */
int runq_schedrm(int ikey /* work key */) {
     struct runq_work *w;
     
     /* Find the data pointer */
     if (itree_find(runq_tab, ikey) == ITREE_NOVAL)
	  return 0;
     w = itree_get(runq_tab);
     
     /* Search the event list for the one pointing to the same data */
     itree_traverse(runq_event)
	  if (itree_get(runq_event) == w) {
	       itree_rm(runq_event);
	       /* Debug: Event removed */
	       elog_printf(DEBUG, "%d at %d has been unscheduled", ikey, 
			   itree_getkey(runq_event));
	       return 1;
	  }

     return 0;
}


/*
 * Remove all scheduled events leaving an empty list and cancel the alarm
 */
void runq_schedrmall() {
     /* Remove the contents of the runq_event tree */
     alarm(0);
     itree_first(runq_event);
     while (!itree_empty(runq_event))
	  itree_rm(runq_event);
}


/*
 * Finds the time of the first runnable event in runq_event and sets
 * the alarm() call [in unix] so that all the work at that and earlier 
 * time intervals may be dispatched.
 */
void runq_setdispatch()
{
     time_t now, next;

     /* 
      * Find the next event and set the alarm to go off then.
      * If there was a high workload in running work above, we may be
      * into the next second or even later. Despite this, force a 
      * minimum wait of 1 second to avoid too much recursion
      * and other dizzyness that I dont want to deal with now.
      * We may be able to catch up in the next time point.
      */
     if (!itree_empty(runq_event)) {
	  itree_first(runq_event);
	  now = time(NULL);
	  if (itree_getkey(runq_event) <= now)
	       next = 1;
	  else
	       next = itree_getkey(runq_event) - now;

	  alarm(next);

	  /* Debug: Say when we wake up */
	  elog_printf(DEBUG, "will wake in %d seconds", next);
     } else {
          elog_send(DEBUG, "empty event queue");
     }

}


/*
 * Dispatch the next piece of work.
 * Execute any work that is waiting to be run and reorder all submitted work.
 * Called from an alarm() routine set up by the previous dispatch.
 *
 * Dispatches any work in the global structure `runq_event' that is
 * to be run at this or an earlier time. If expired, no work is carried out. 
 * 
 * As each piece of work is dispatched, it is removed from runq_event, 
 * its next invocation is calculated by runq_schedw() and added back to 
 * runq_event at the new time.
 *
 * When all the work for this time have been delt with, the dispatcher 
 * will set up an alarm() by using the delta between now and the 
 * first pending event. This is carried out by runq_sedispatch().
 *
 * When the work is part of a run, its (*startofrun)() routine will be 
 * called before the first run and its (*endofrun)() routine will be 
 * called after its final execution.
 * 
 * When the last peice of work in the run starts and finishes
 * before meth returns control to dispatch, all details are removed 
 * from runq_tab after (*endofrun)().
 * However if the work is still running when meth returns, no further
 * events are scheduled, the details are kept in runq_tab and marked
 * as expired. When the work comes to an end, meth singals it with a 
 * callback, which calls (*endofrun)() and removes all the work.
 *
 * Finally dispatch() returns so other work may be carried out.
 * NB. If new jobs are added to runq_event, runq_setdispatch() should 
 * be called afterwards to reset the alarm() call.
 * If any jobs missed their time, they will still be executed.
 */
void runq_dispatch() {
     time_t now;
     struct runq_work *w=NULL;
     ITREE *resched;
     int r, ikey;

     resched = itree_create();
     now = time(NULL);		/* Find the current time */

     /*
      * Traverse the event list in order, finding the events to run
      * at this second of before.
      * Run these events and mark them for reschedualing.
      * Use a traversal loop which can cope with removals
      */
     elog_printf(DEBUG, "before dispatching - size of event queue %d", 
		 itree_n(runq_event));

     /* 
      * The code below traverses the tree, executing the code with time
      * keys that are smaller than or equal to 'now'. Once executed, 
      * the work is removed from the event tree and placed in the resched 
      * tree to have a new commencement time calculated.
      */
     while (!itree_empty(runq_event)) {
          itree_first(runq_event);
	  if (itree_getkey(runq_event) <= now) {
	       w = itree_get(runq_event);
	       if ( !w ) {
		    runq_dump();
		    elog_die(ERROR, 
			     "unable to get event details key=%d now=%d", 
			     itree_getkey(runq_event), now);
	       }

	       /* expired? -- don't run! just ask to be rescheduled */
	       if (w->expired)
		    goto expired;

	       /* start of run? */
	       if (w->nruns == 0 && w->startofrun) {
		    r = (*w->startofrun)(w->argument, w->arglen);
		    if (r == -1)
			 elog_printf(ERROR, "startofrun() failed for %s", 
				     w->desc);
	       }

	       /* run counter */
	       w->nruns++;

	       /* command */
	       r = (*w->command)(w->argument, w->arglen);
	       if (r == -1)
		    elog_printf(ERROR, "command() failed for %s", 
				w->desc);

	  expired:
	       /* add to reschedule queue & remove from run queue */
	       itree_add(resched, now, w);
	       itree_rm(runq_event);
	  } else
	       break;
     }

     elog_printf(DEBUG, "after dispatching - size of event queue %d", 
		 itree_n(runq_event));

     /*
      * All jobs run have been marked for rescheduling in the ITREE
      * resched. Traverse this now to do the actual rescheduling.
      * Destroy the tree when finished, but don't touch the values
      * as they are just referencies to runq_tab, which 'owns' them.
      */
     itree_traverse(resched)
          if (runq_schedw(itree_get(resched), itree_getkey(resched), 
			  time(NULL)) == 0) {
	       /* work has probably expired (or it was bad) mark
		* it as expired for later clearing and don't schedule */
	       w->expired++;
	  }
     itree_destroy(resched);

     /* traverse runq_tab to find work that has been expired and needs 
      * garbage collection. Make sure that it is not still running (and
      * may have outstanding I/O) then remove from the table. */
     itree_first(runq_tab);
     if (itree_isbeyondend(runq_tab)) {
	  w = itree_get(runq_tab);
	  ikey = itree_getkey(runq_tab);
	  if (w->expired) {
	       if (w->isrunning && (*w->isrunning)(w->argument, w->arglen)) {
		    /* Could poll again the next time, but now we use 
		     * callbacks to signal callers */
	       } else {
		    /* there is nothing to stop us from shutting down */
		    if (w->nruns > 0 && w->endofrun) {
			 /* if the work is in the event queue and it has
			  * already been run before, assume that it is
			  * part of a run and call its endofrun() routine */
			 elog_printf(DEBUG, "end-of-run for doomed job %s "
				     "(%d runs)", w->desc, w->nruns);
			 r = (*w->endofrun)(w->argument, w->arglen);
			 if (r == -1)
			      elog_printf(ERROR, "endofrun() failed for %s", 
					  w->desc);
		    }

		    /* check there are no other events using the same structure
		     * the remove the table entry and free the sstructure */
		    elog_printf(DEBUG, "clearing up %s", w->desc);
		    itree_traverse(runq_event)
			 if (itree_get(runq_event) == w)
			      itree_rm(runq_event);
		    nfree(w->desc);
		    nfree(w->argument);
		    nfree(w);
		    itree_rm(runq_tab);
		    callback_raise(RUNQ_CB_EXPIRED, (void *) (long) ikey, 
				   NULL, NULL, NULL);
	       }
	  } else {
	       itree_next(runq_tab);
	  }
     }

     runq_setdispatch();

     /* Our work is finished. Return and trust that the calling 
      * routine will do some other work or pause for the signal that
      * we have set up.
      */
     return;
}

#if 1
/*
 * Dispatch signal handler
 * Disables signals for the duration of runq_dispatch()
 */
void runq_sigdispatch() {
     sig_off();
     runq_dispatch();
     sig_on();
}
#endif



/*
 * Callback signal from meth class that a long running method has finished.
 * The method was probably an external process (METH_FORK), in which case
 * meth_child() sent it.
 * The argument is math's key, which job_ gives us a description.
 * We only care about this callback when the work has expired and we can 
 * clear up the runq_work and call (*endofrun)().
 */
void runq_methfinished(void *key)
{
     struct runq_work *w;
     int r, ikey;

     itree_traverse(runq_tab) {
	  w = itree_get(runq_tab);
	  ikey = itree_getkey(runq_tab);
	  if (w->expired && strcmp(w->desc, key) == 0) {
	       /* found expired job */
	       elog_printf(DEBUG, "end-of-run for long running job %s"
			   "(%d runs)", w->desc, w->nruns);
	       r = (*w->endofrun)(w->argument, w->arglen);
	       if (r == -1)
		    elog_printf(ERROR, "endofrun() failed for %s", 
				w->desc);
	       itree_rm(runq_tab);
	       callback_raise(RUNQ_CB_EXPIRED, (void *) (long) ikey, NULL, 
			      NULL, NULL);
	       nfree(w->desc);
	       nfree(w->argument);
	       nfree(w);
	  }
     }
}



/*
 * Stop new work being dispatched and empty the event queue.
 * Leave runq_tab alone as its data is needed elsewhere and we may want to 
 * start work again
 */
void runq_disable() {
  elog_send(DEBUG, "draining runq_event");
  runq_drain = 1;	/* allow to drain */
  runq_schedrmall();
}

/*
 * Enable the dispatching of new work and go through runq_tab scheduling 
 * new work
 */
void runq_enable() {
  elog_send(DEBUG, "enabled runq_event, setting up new work");
  runq_drain = 0;	/* stop draining */
  runq_schedall();
}

#if TEST
#include <stdio.h>
#include "route.h"
#include "rs.h"
#include "rt_file.h"
#include "rt_std.h"

char tmsg1[] = "hello, world\n";

/* Test function */
int test1(char *arg, int arglen) {
     puts(arg);		/* We know that arg will be null terminalted */
     return 0;		/* Success */
}

int main(int argc, char **argv) {
     time_t now;
     char *str;
     
     now = time(NULL);
     route_init(NULL, 0);
     route_register(&rt_filea_method);
     route_register(&rt_fileov_method);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     if ( ! elog_init(1, "runq test", NULL))
	  elog_die(FATAL, "didn't initialise elog\n");
     sig_init();
     callback_init();
     runq_init(now);

     /* Test should fail due to incorrect method */
     elog_printf(DEBUG, "[1a] Expect an error --> ");
     if (runq_add(now+5, 5, 0, 1, "1a", NULL, NULL, NULL, NULL, 
		  (str=xnstrdup(tmsg1)), sizeof(tmsg1)) >= 0)
     {
	  elog_die(FATAL, "[1a] Shouldn't be able to add\n");
     }

     /* One off five second test */
     if (runq_add(now+5, 5, 0, 1, "1a", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1a] Can't add\n");
     }
     itree_first(runq_event);
     if (itree_getkey(runq_event) != now+5) {
	  elog_die(FATAL, "[1a] Queued at an incorrect time\n");
     }
     runq_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1a] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);
     
     /* Two five second tests at the same time in the future */
     if (runq_add(now+5, 5, 0, 1, "1b", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1b] Can't add first\n");
     }
     if (runq_add(now+5, 5, 0, 1, "1b", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1b] Can't add second\n");
     }
     itree_first(runq_event);
     if (itree_getkey(runq_event) != now+5) {
	  elog_die(FATAL, "[1b] First queued at an incorrect time\n");
     }
     itree_next(runq_event);
     if (itree_getkey(runq_event) != now+5) {
	  elog_die(FATAL, "[1b] Second queued at an incorrect time\n");
     }
     runq_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1b] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);
     
     /* Two five second tests at the different times in the future */
     if (runq_add(now+6, 6, 0, 1, "1c", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1c] Can't add first\n");
     }
     if (runq_add(now+5, 5, 0, 1, "1c", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1c] Can't add second\n");
     }
     itree_first(runq_event);
     if (itree_getkey(runq_event) != now+5) {
	  elog_die(FATAL, "[1c] First queued at an incorrect time\n");
     }
     itree_next(runq_event);
     if (itree_getkey(runq_event) != now+6) {
	  elog_die(FATAL, "[1c] Second queued at an incorrect time\n");
     }
     runq_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1c] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);
     
     /* Continuous test: single job */
     if (runq_add(now-2, 5, 0, 0, "1d", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1d] Can't add\n");
     }
     itree_first(runq_event);
     if (itree_getkey(runq_event) != now+3) {
	  elog_die(FATAL, 
		  "[1d] Event queued at an incorrect time: bad=%d good=%ld\n", 
		  itree_getkey(runq_event), now+3);
     }
     runq_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1d] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);
     
     /* Continous test: two jobs */
     if (runq_add(now-2, 6, 0, 0, "1e", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1e] Can't add first\n");
     }
     if (runq_add(now-3, 5, 0, 0, "1e", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1e] Can't add second\n");
     }
     itree_first(runq_event);
     if (itree_getkey(runq_event) != now+2) {
	  elog_die(FATAL, "[1e] First queued at an incorrect time\n");
     }
     itree_next(runq_event);
     if (itree_getkey(runq_event) != now+4) {
	  elog_die(FATAL, "[1e] Second queued at an incorrect time\n");
     }
     runq_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1e] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);
     
     /* Current limited jobs: two of them in the middle of their run */
     if (runq_add(now-10, 6, 0, 5, "1f1", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1f] Can't add first\n");
     }
     if (runq_add(now-10, 5, 0, 5, "1f2", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) < 0)
     {
	  elog_die(FATAL, "[1f] Can't add second\n");
     }
     itree_first(runq_event);
     if (itree_getkey(runq_event) != now+2) {
	  elog_die(FATAL, "[1f] 1st queued at wrong time, want %ld got %d\n",
		  now+2, itree_getkey(runq_event));
          runq_dump();
     }
     itree_next(runq_event);
     if (itree_getkey(runq_event) != now+5) {
          /* should have run already in the runq_add as it was eligable  */
	  elog_die(FATAL, "[1f] 2nd queued at wrong time, want %ld got %d\n",
		  now+5, itree_getkey(runq_event));
          runq_dump();
     }
     runq_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1f] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);
     
     /* Past limited test: well in the past */
     if (runq_add(now-100, 6, 0, 5, "1g", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) == -1)
     {
	  elog_die(FATAL, "[1g] Can't add first\n");
     }
     if (runq_add(now-100, 5, 0, 5, "1g", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) == -1)
     {
	  elog_die(FATAL, "[1g] Can't add second\n");
     }
     if (!itree_empty(runq_event)) {
	  elog_die(FATAL, "[1g] Event queue should be empty\n");
     }
     runq_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1g] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);
     
     /* Past limited test: last event now */
     if (runq_add(now-30, 6, 0, 5, "1h1", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) == -1)
     {
	  elog_die(FATAL, "[1h] Can't add first\n");
     }
     if (runq_add(now-25, 5, 0, 5, "1h2", NULL, test1, NULL, NULL, 
		  xnstrdup(tmsg1), sizeof(tmsg1)) == -1)
     {
	  elog_die(FATAL, "[1h] Can't add second\n");
     }
     sleep(6);		/* let it run */
     sleep(2);		/* let it run */
     sleep(2);		/* let it run */
     if (!itree_empty(runq_event)) {
	  elog_die(FATAL, "[1h] Event queue should be empty\n");
	  runq_dump();
     }
     runq_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1h] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);

     runq_fini();
     elog_fini();
     route_fini();
     callback_fini();
     
     printf("%s: tests finished successfully\n", argv[0]);
     exit(0);
}
#endif /* TEST */
