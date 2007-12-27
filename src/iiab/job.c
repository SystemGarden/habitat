/*
 * Job class
 * Executes periodic work with logging and I/O provided as a facility.
 * Uses runq underneath to provide the basic timing mechinism.
 *
 * Nigel Stuckey, December 1997
 * Modified January 1999 to use elog for error logging.
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nmalloc.h"
#include "itree.h"
#include "runq.h"
#include "job.h"
#include "elog.h"
#include "util.h"
#include "table.h"
#include "meth.h"
#include "callback.h"

char *job_cols[] =   {"start", "interval", "phase", "count", "key", \
		      "origin", "result", "errors", "keep", "method",
		      "command", NULL};

ITREE *job_tab;		/* tree list of jobs */
int job_debug=0;	/* debug flag */
time_t job_start_t;	/* job class start time */

/* Initialise job class */
void job_init()
{
     job_tab = itree_create();	/* List of jobs */
     callback_regcb(RUNQ_CB_EXPIRED, (void *) job_runqexpired);
     job_start_t = time(NULL);
}

void job_fini()
{
     job_clear();
     if (job_tab)
          itree_destroy(job_tab);
}

/* List the job information for diagnostics */
void job_dump()
{
     struct job_work *w;
     time_t t;
     
     elog_startsend(DEBUG, "Job table ------\n");
     /* List the job table */
     itree_traverse(job_tab) {
	  w = itree_get(job_tab);
	  t = itree_getkey(job_tab);
	  elog_contprintf(DEBUG, "    %8s %8s %14s %14s %d\n", 
		       w->runarg->key, w->origin, w->runarg->res_purl, 
		       w->runarg->err_purl, w->runarg->keep);
	  /* TODO: should get info from a runq call eg. runq_dumpwork() */
     }
     elog_endsend(DEBUG, "----------------");
}

/* 
 * Capture the information passed and add it to the job list.
 * Arranges for the low level work to be handled by the runq and meth classes.
 * Returns a reference of 0 or greater to the work record inserted 
 * in the job table, which can be used with job_rm(). Otherwise returns -1
 * if there was a failure or -2 if successfully completed the job without
 * the need of a job entry.
 *
 * Job_tab follows an insertion order only. The runq and meth classes 
 * should be initialised before this is called. Additionally, the requested
 * methods should be present in the meth class before hand. If a runtime
 * loadable method is to be used, use meth_load() before calling the add.
 *
 * All strings are copied by this routine, the caller does not have to
 * keep them allocated.
 */
int job_add(long start,		/* time job should start following 
				 * job_init() (may be negative) */
	    long interval,	/* time in between jobs (seconds); if 0 ?? */
	    long phase,		/* execution order at each time point */
	    long count,		/* number of times to run job or 0 to
				 * repeat indefinately */
	    char *key, 		/* job identification string */
	    char *origin, 	/* origin of job */
	    char *result, 	/* route for results (p-url spec) */
	    char *error, 	/* route for errors (p-url spec) */
	    int keep, 		/* keep most recent number of datum */
	    char *method,	/* String representation of method */
	    char *command	/* Command for method */ )
{
     struct job_work *work;
     int r, klen, clen, rlen, elen;
     METHID meth;
     struct meth_invoke *runarg;
     char *mem;

     /* The job class does its stuff by calling meth_run() at times 
      * set up in runq. In this method we create the arguments to
      * meth and submit the work to runq, using meth_run() as the 
      * callback.
      */

     /* Obtain the method id to use in the method_run callback */
     if (method == NULL || *method == '\0') {
          elog_printf(ERROR, "no method in job %s", key);
          return -1;
     }
     meth = meth_lookup(method);
     if (meth == NULL) {
          elog_printf(ERROR, "unknown method %s in job %s", method, key);
          return -1;
     }

     /* Compose an argument buffer for meth_add() using struct meth_invoke */
     klen = strlen(key);
     clen = strlen(command);
     rlen = strlen(result);
     elen = strlen(error);
     runarg = xnmalloc(sizeof(struct meth_invoke) + klen + clen + rlen 
		       + elen +4);
     mem = ((char *) runarg) + sizeof(struct meth_invoke);
     runarg->key = strcpy(mem, key);
     runarg->run = meth;
     mem += klen + 1;
     runarg->command = strcpy(mem, command);
     mem += clen + 1;
     runarg->res_purl = strcpy(mem, result);
     mem += rlen + 1;
     runarg->err_purl = strcpy(mem, error);
     runarg->keep = keep;

     /* Debug the call */
     elog_printf(DEBUG, "job added: %d %d %d %d %s %s %s %s %d %8s %s", 
		 start, interval, phase, count, key, origin, result, error, 
		 keep, method, command);

     /* Add the work */
     r = runq_add(job_start_t+start, interval, phase, count, key, 
		  meth_startrun_s, meth_execute_s, meth_isrunning_s, 
		  meth_endrun_s, runarg, sizeof(struct meth_invoke));
     if (r == -1 || r == -2) {
	  if (r == -1) {
	       elog_printf(ERROR, "bad parameter in job %s", key);
	       return -1;
	  } else
	       return -2;
     }

     /* Save the information for ourselves */
     work = xnmalloc(sizeof(struct job_work));
     work->origin = xnstrdup(origin);
     work->runarg = runarg;
     work->runq = r;

     r = itree_append(job_tab, work);

     return r;
}

/*
 * Remove the job indexed by ikey from the jobs queue.
 * Removes job structure and flags removal to underlying runq class.
 * No further work should be carried out for the job identified by key
 * but runq may take a while to catch up if there are any proccesses 
 * involved.
 * Returns 1=success, 0=error.
 */
int job_rm(int ikey /* job key */) {
     struct job_work *w;
     
     /* Find job details */
     if ((w = itree_find(job_tab, ikey)) == ITREE_NOVAL) {
          elog_printf(ERROR, "job not found (id %d)", ikey);
	  return 0;
     }

     /* Debug: Notify the removal from the job list */
     elog_printf(DEBUG, "remove job %s", w->runarg->key);

     /* Remove runq work instruction */
     if (!runq_rm(w->runq)) {
          elog_printf(DEBUG, "job %s can't remove runq (id %d)", 
		      w->runarg->key, w->runq);
	  return 0;
     }

     itree_rm(job_tab);			/* Remove job record */

     /* Free job storage excluding w->runarg which was reparented by runq */
     nfree(w->origin);
     nfree(w);

     return(1);
}

/* Completely empty the job queue and signal the removal of underlying 
 * runq work. Runq work will take longer to die, however, as their
 * processes may not have come to an end. */
void job_clear()
{
     ITREE *keys;

     if (!job_tab)	/* do nothing if not initialised */
          return;

     /* for safty make a duplicate list */
     keys = itree_create();
     tree_traverse(job_tab)
	  itree_add(keys, itree_getkey(job_tab), NULL);

     /* Traverse the job_tab list and delete the associated storage */
     /* The loop below always resets to a known state */
     elog_printf(DEBUG, "removing %d jobs", itree_n(job_tab));
     itree_traverse(keys)
	  job_rm( itree_getkey(keys) );

     itree_destroy(keys);
}



/*
 * Remove the job from the list as it has expired.
 * Callback routine for RUN_CB_FINISHED using the callback class.
 * Warning! nasty casting to transform int to voids *.
 */
void job_runqexpired(void *ikey)
{
     struct job_work *w;

     /* Find job details */
     itree_traverse(job_tab) {
	  w = itree_get(job_tab);
	  if (w->runq == (int) ikey) {
	       /* found the finished work */
	       elog_printf(DEBUG, "job %s finshed", w->runarg->key);
	       itree_rm(job_tab);			/* Remove job record */
	       /* Free job storage excluding w->runarg which was 
		* reparented by runq */
	       nfree(w->origin);
	       nfree(w);
	       return;
	  }
     }
}



/*
 * Read in job definitions from the pseudo-url and add them to the
 * job class. If there is a problem with a job definition, it will not
 * be added to the job class, but other jobs will continue to be processed.
 *
 * File format: one line per job. magic string of 'job 1' at top
 * Fields/columns match the job_add() spec and are:
 * 1. start time (seconds)
 * 2. interval (currently seconds, but ought to have weekly or monthly based
 *    intervals as well)
 * 3. phase (int; job order at each scheduled point)
 * 4. count (int; how many times to run; 0=indefinately)
 * 5. key (unique job id)
 * 6. origin (char *; no spaces)
 * 7. result (pseudo-url; where to send results)
 * 8. errors (pseudo-url; where to send errors)
 * 9. keep (number of results to keep in timestore or tablestore)
 * 10.method (char *; string representation of method)
 * 11.command (char *; command for method to run)
 *
 * The result url, error url and command are parsed for tokens of the
 * form %x, where x is a single letter. These are expanded into context
 * specific strings. Currently, %h is hostname and %j is jobname. See
 * route_expand() for more details.
 *
 * Returns the number of jobs added, or -1 for a major problem
 */
int job_loadroute(char *purl 	/* p-url location of file */ )
{
     int start, interval, phase, count, keep;
     char *key, *origin, *result, *error, *method, *command;
     char *endint;
     ITREE *jobdefs, *job;
     int r, njobdefs, jobsadded = 0;
     char key_t[60], result_t[256], error_t[256], command_t[1024];

     /* read jobs into parse structure */
     njobdefs = util_parseroute(purl, " \t", "job 1", &jobdefs);
     if (njobdefs < 1)
	  return -1;

     util_parsedump(jobdefs);
     itree_traverse(jobdefs) {
          job = itree_get(jobdefs);

	  /* check the size of each row */
	  if (itree_n(job) != 11) {
	       elog_startprintf(ERROR, "%s row %d has %d fields, want 11 (",
				purl, itree_getkey(jobdefs)+1, itree_n(job) );
	       itree_traverse(job)
		    elog_contprintf(ERROR, "%s ", (char *) itree_get(job));
	       elog_endprintf(ERROR, ")");
	       continue;
	  }

	  /* 
	   * validate each column 
	   * we dont need to strdup() any space because job_add() will
	   * do that for itself
	   */
	  /* column 1: start time */
	  itree_find(job, 0);
	  start = strtol( itree_get(job), &endint, 10 );
	  if (start == LONG_MIN || start == LONG_MAX || 
	      itree_get(job) == endint) {
	       elog_printf(ERROR, "%s row %d start time (column 1) is "
			   "incorrect: '%s'; skipping", 
			   purl, itree_getkey(jobdefs), itree_get(job));
	       continue;
	  }

	  /* column 2: interval */
	  itree_find(job, 1);
	  interval = strtol( itree_get(job), &endint, 10 );
	  if (interval == LONG_MIN || interval == LONG_MAX ||
	      itree_get(job) == endint) {
	       elog_printf(ERROR, "%s row %d interval (column 2) is "
			   "incorrect: '%s'; skipping", 
			   purl, itree_getkey(jobdefs), itree_get(job));
	       continue;
	  }

	  /* column 3: phase */
	  itree_find(job, 2);
	  phase = strtol( itree_get(job), &endint, 10 );
	  if (phase == LONG_MIN || phase == LONG_MAX ||
	      itree_get(job) == endint) {
	       elog_printf(ERROR, "%s row %d phase (column 3) is incorrect: "
			   "'%s'; skipping",
			   purl, itree_getkey(jobdefs), itree_get(job));
	       continue;
	  }

	  /* column 4: count */
	  itree_find(job, 3);
	  count = strtol( itree_get(job), &endint, 10 );
	  if (count == LONG_MIN || count == LONG_MAX ||
	      itree_get(job) == endint) {
	       elog_printf(ERROR, "%s row %d count (column 4) is incorrect: "
			   "'%s'; skipping",  purl, itree_getkey(jobdefs), 
			   itree_get(job));
	       continue;
	  }

	  /* column 5: key */
	  itree_find(job, 4);
	  key = itree_get(job);
	  if (route_expand(key_t, key, key, interval) != -1)
	       key = key_t;

	  /* column 6: origin  */
	  itree_find(job, 5);
	  origin = itree_get(job);

	  /* column 7: result  */
	  itree_find(job, 6);
	  result = itree_get(job);
	  if (route_expand(result_t, result, key, interval) != -1)
	       result = result_t;

	  /* column 8: error  */
	  itree_find(job, 7);
	  error = itree_get(job);
	  if (route_expand(error_t, error, key, interval) != -1)
	       error = error_t;

	  /* column 9: keep */
	  itree_find(job, 8);
	  keep = strtol( itree_get(job), &endint, 10 );
	  if (keep == LONG_MIN || keep == LONG_MAX ||
	      itree_get(job) == endint) {
	       elog_printf(ERROR, "%s row %d keep (column 9) is incorrect: "
			   "'%s'; skipping", purl, itree_getkey(jobdefs), 
			   itree_get(job));
	       continue;
	  }

	  /* column 10: method  */
	  itree_find(job, 9);
	  method = itree_get(job);

	  /* column 11: command  */
	  itree_find(job, 10);
	  command = itree_get(job);
	  if (route_expand(command_t, command, key, interval) != -1)
	       command = command_t;

	  elog_printf(DEBUG, "%s row %d read: (1) %ld (2) %ld (3) %ld "
		      "(4) %ld (5) %s (6) %s (7) %s (8) %s (9) %ld (10) %s "
		      "(11) %s", purl, itree_getkey(jobdefs), start, 
		      interval, phase, count, key, origin, result, error, 
		      keep, method, command);

	  if (meth_check(method)) {
	       elog_printf(ERROR, "%s row %d method %s not loaded; skipping",
			    purl, itree_getkey(jobdefs), method);
	       continue;
	  }

	  /* insert into job class */
	  r = job_add(start, interval, phase, count, key, origin, result, 
		      error, keep, method, command);
	  if (r == -1)
	       elog_printf(ERROR, "%s row %d unable to add job; skipping",
			   purl, itree_getkey(jobdefs));
	  else
	       jobsadded++;
     }

     util_freeparse(jobdefs);

     return jobsadded;
}



/*
 * Scan the text provided by jobtext and return a TABLE containing the 
 * data if successful or NULL otherwise.
 * The text will be modified and adopted by the TABLE, so it should not
 * be used or freed by the caller unless the routine fails.
 */
TABLE job_scanintotable(char *jobtext)
{
     TABLE tab;
     int r;

     tab = table_create_a(job_cols);
     r = strncmp(jobtext, "job 1\n", 6);
     if (r != 0 ||
	 table_scan(tab, jobtext+6, " \t", TABLE_CFMODE, TABLE_NOCOLNAMES, 
		    TABLE_NORULER)== -1) {

	  /* clear up from error */
	  elog_printf(ERROR, "unable to scan clockwork table");
	  table_destroy(tab);
	  return NULL;
     }

     table_freeondestroy(tab, jobtext);
     return tab;
}




#if TEST
#include <stdio.h>
#include <unistd.h>
#include "sig.h"
#include "route.h"
#include "rt_file.h"
#include "rt_std.h"

extern ITREE *runq_tab;
extern ITREE *runq_event;

char tmsg1[] = "echo \"hello, world\n\"";

int main(int argc, char **argv) {
     time_t now;

     /* Initialise route, runq and job classes */
     now = time(NULL);
     route_init(NULL, 0);
     route_register(&rt_filea_method);
     route_register(&rt_fileov_method);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     if ( ! elog_init(1, "job test", NULL))
	  elog_die(FATAL, "didn't initialise elog\n");
     sig_init();
     callback_init();
     runq_init(now);
     meth_init();
     job_init();

     /* Test should fail due to incorrect method */
     elog_printf(DEBUG, "Expect a complaint! -> ");
     if (job_add(5, 5, 0, 1, "test1a1", "internal_test", "stdout", 
		  "stderr", 100, NULL, "echo \"Hello, world\"") != -1)
     {
	  elog_die(FATAL, "[1a] Shouldn't be able to add\n");
     }

     /* Single test in five seconds, never to run */
     if (job_add(5, 5, 0, 1, "test1a2", "internal_test", "stdout", 
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1a] Can't add\n");
     }
				/* Attention: some white box testing */
     itree_first(runq_event);
     if (itree_getkey(runq_event) != now+5) {
	  elog_die(FATAL, "[1a] Queued at an incorrect time\n");
     }
     job_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1a] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);
     
     /* Two tests both in five seconds, never to run */
     if (job_add(5, 5, 0, 1, "test1b1", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1b] Can't add first\n");
     }
     if (job_add(5, 5, 0, 1, "test1b2", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
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
     job_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1b] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);

     /* Two tests one in five seconds, the other in six, never to run */
     if (job_add(6, 6, 0, 1, "test1c1", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1c] Can't add first\n");
     }
     if (job_add(now+5, 5, 0, 1, "test1c2", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
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
     job_clear();
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, 
		  "[1c] Trees not emptied. runq_events=%d, runq_tab=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab));
     }
     now = time(NULL);
     
     /* Continuous single test supposed to start two seconds ago, 
      * next run in three; never to run */
     if (job_add(-2, 5, 0, 0, "test1d1", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1d] Can't add\n");
     }
     itree_first(runq_event);
     if (itree_getkey(runq_event) != now+3) {
	  elog_die(FATAL, 
		  "[1d] Event queued at an incorrect time: bad=%d good=%ld\n", 
		  itree_getkey(runq_event), now+3);
     }
     job_clear();
     if (runq_nsched() > 0) {
	  elog_die(FATAL, "[1d] Still active work scheduled. runq_events=%d, "
		  "runq_tab=%d runq_nsched()=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab), runq_nsched());
	  runq_dump();
     }
     now = time(NULL);
     
     /* Two continous tests, starting two seconds ago, next next run in four;
      * never to run */
     if (job_add(-2, 6, 0, 0, "test1e1", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1e] Can't add first\n");
     }
     if (job_add(-3, 5, 0, 0, "test1e2", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1e] Can't add second\n");
     }
     itree_first(runq_event);
     while (((struct runq_work*) itree_get(runq_event))->expired)
	  itree_next(runq_event);
     if (itree_getkey(runq_event) != now+2) {
	  elog_die(FATAL, "[1e] First queued at an incorrect time\n");
     }
     itree_next(runq_event);
     while (((struct runq_work*) itree_get(runq_event))->expired)
	  itree_next(runq_event);
     if (itree_getkey(runq_event) != now+4) {
	  elog_die(FATAL, "[1e] Second queued at an incorrect time\n");
     }
     job_clear();
     if (runq_nsched() > 0) {
	  elog_die(FATAL, "[1e] Still active work scheduled. runq_events=%d, "
		  "runq_tab=%d runq_nsched()=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab), runq_nsched());
	  runq_dump();
     }
     now = time(NULL);
     
     /* Two 5 run jobs, scheduled to start 10 seconds ago, with periods
      * of 5 and 6 seconds; never to run */
     if (job_add(-10, 6, 0, 5, "test1f1", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1f] Can't add first\n");
     }
     if (job_add(-10, 5, 0, 5, "test1f2", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1f] Can't add second\n");
     }

     itree_first(runq_event);
     while (((struct runq_work*) itree_get(runq_event))->expired)
	  itree_next(runq_event);
     if (itree_getkey(runq_event) != now+2) {
	  elog_die(FATAL, "[1f] First queued at an incorrect time\n");
     }
     itree_next(runq_event);
     while (((struct runq_work*) itree_get(runq_event))->expired)
	  itree_next(runq_event);
     if (itree_getkey(runq_event) != now+5) {
	  elog_die(FATAL, "[1f] Second queued at an incorrect time\n");
     }
     job_clear();
     if (runq_nsched() > 0) {
	  elog_die(FATAL, "[1f] Still active work scheduled. runq_events=%d, "
		  "runq_tab=%d runq_nsched()=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab), runq_nsched());
	  runq_dump();
     }
     now = time(NULL);
     
     /* Two 5 run jobs, scheduled to start 100 seconds ago, with periods
      * of 5 and 6 seconds; they should never be scheduled */
     if (job_add(-100, 6, 0, 5, "test1g1", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1g] Can't add first\n");
     }
     if (job_add(-100, 5, 0, 5, "test1g2", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1g] Can't add second\n");
     }
     if (runq_nsched() > 0) {
	  elog_die(FATAL, "[1g] Still active work scheduled. runq_events=%d, "
		  "runq_tab=%d runq_nsched()=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab), runq_nsched());
	  runq_dump();
     }
     job_clear();
     now = time(NULL);

     /* Two five run tests, starting at different times in the past,
      * five runs each wittth different periods; they should both
      * run now */
     if (job_add(-24, 6, 0, 5, "test1h1", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1h] Can't add first\n");
     }
     if (job_add(-20, 5, 0, 5, "test1h2", "internal_test", "stdout",
		  "stderr", 100, "exec", "echo \"Hello, world\"") == -1)
     {
	  elog_die(FATAL, "[1h] Can't add second\n");
     }
     if (runq_nsched() != 2) {
	  elog_die(FATAL, "[1h] Two jobs should be scheduled not %d\n",
		  runq_nsched());
	  runq_dump();
     }
     sig_on();
     sleep(6);		/* let it run */
     sleep(1);		/* let it run */
     sleep(1);		/* let it run */
     sleep(1);		/* let it run */
     sig_off();
     if (runq_nsched() > 0) {
	  elog_die(FATAL, "[1h] Still active work scheduled. runq_events=%d, "
		  "runq_tab=%d runq_nsched()=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab), runq_nsched());
	  runq_dump();
     }

     job_clear();

#if 0
     /* check all tables/lists are empty */
     if (!itree_empty(runq_event) || !itree_empty(runq_tab)) {
	  elog_die(FATAL, "[1i] Still entries in tables. runq_events=%d, "
		  "runq_tab=%d runq_nsched()=%d\n", 
		  itree_n(runq_event), itree_n(runq_tab), runq_nsched());
	  runq_dump();
     }
#endif

     job_fini();
     meth_fini();
     runq_fini();
     elog_fini();
     route_fini();
     callback_fini();

     printf("%s: tests finished\n", argv[0]);
     exit(0);
}
#endif /* TEST */
