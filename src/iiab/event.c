/*
 * Class to carry out events from a queue, generally having been raised by
 * pattern-action matching. (see pattern.c).
 *
 * Nigel Stuckey, November 2000
 * Copyright System Garden Limited 2001. All rights reserved
 */

#include <string.h>
#include <stdio.h>
#include <time.h>
#include "event.h"
#include "tree.h"
#include "itree.h"
#include "elog.h"
#include "nmalloc.h"
#include "util.h"
#include "route.h"
#include "job.h"

/* 
 * Create an event tracking instance, returning NULL for failure or else 
 * the handle of the event tracking instance.
 * Command should contain a space separated list p-urls, each of which
 * is an event queue. What ever goes in there will be converted into
 * an executable program.
 */
EVENTINFO event_init(char *command)
{
     ITREE *l, *lol;
     struct event_tracking *etrack;
     EVENTINFO einfo;
     int size;
     time_t modt;

     if (!command || !*command) {
 	  elog_printf(ERROR, "empty or null command");
          return NULL;
     }

     /* read routes into the event tracking list */
     util_parsetext(command, " \t", NULL, &lol);
     if (itree_empty(lol)) {
	  elog_printf(ERROR, "empty set of routes (1)");
	  return NULL;
     }
     itree_first(lol);
     l = itree_get(lol);
     if (itree_empty(l)) {
	  elog_printf(ERROR, "empty set of routes (2)");
	  return NULL;
     }

     /* successful parsing, create the event instance structures */
     einfo = xnmalloc(sizeof(struct event_information));
     einfo->track = tree_create();
     itree_traverse(l) {
	  etrack = xnmalloc(sizeof(struct event_tracking));
	  etrack->rtname  = itree_get(l);
	  etrack->rt      = route_open(etrack->rtname, NULL, NULL, 0);
	  if (etrack->rt == NULL)
	       etrack->lastseq = -1;
	  else
	       route_tell(etrack->rt, &etrack->lastseq, &size, &modt);
	  tree_add(einfo->track, itree_get(l), etrack);
     }
     util_freeparse_leavedata(lol);

     return einfo;
}


/*
 * Scan the routes that we are tracking and carrry out the actions contained 
 * therein. Returns -1 if there was an error.
 */
int event_action(EVENTINFO einfo, ROUTE output, ROUTE error)
{
     int seq, size;
     time_t modt;
     ITREE *bufchain;
     ROUTE_BUF *buf;
     struct event_tracking *etrack;

     tree_traverse(einfo->track) {
	  etrack = tree_get(einfo->track);
	  if (etrack->rt == NULL) {
	       /* attempt to open route */
	       etrack->rt = route_open(etrack->rtname, NULL, NULL, 0);
	       if (etrack->rt == NULL)
		    continue;
	  }

	  /* check if there are any updates to process */
	  route_tell(etrack->rt, &seq, &size, &modt);
	  if (seq > etrack->lastseq) {
	       bufchain = route_seekread(etrack->rt, seq, 0);
	       if (bufchain == NULL) {
		    elog_printf(ERROR, "unable to read changed items");
		    return -1;
	       }

	       /* carry out events */
	       itree_traverse(bufchain) {
		    buf = itree_get(bufchain);
		    if (event_execute(buf->buffer, output, error, 
				      etrack->rtname, seq) == 0)
			 elog_printf(ERROR, "unable to create event "
				     "job for `%s'", buf->buffer);
	       }

	       /* update */
	       etrack->lastseq = seq;
	       route_free_routebuf(bufchain);
	  }
     }

     return 0;
}


/*
 * Run the command in the event line format.
 * The first word dictates the method of event action, the remainder is the
 * command that is sent to that method. The first % in the line starts stdin
 * input, with subsequent % being converted into new lines. To use actually
 * use % in the command, it should be escaped with backslash, thus \%.
 * Note: The command line will be modified by this routine.
 * Output and error are used to set up the jobs, rtname and seq are used
 * to create a unique job reference.
 * Returns 1 for successful, 0 for failure.
 */
int event_execute(char *cmdln, ROUTE output, ROUTE error, char *rtname, 
		  int seq) 
{
     char *cmd, *input, jobid[64], *outpurl, *errpurl;
     int lnlen, len, r;

     /* translate carry out the translations */
     lnlen = strlen(cmdln);
     util_strgsub(cmdln, "\\%", "\001\001", lnlen+1);
     util_strgsub(cmdln, "%", "\n", lnlen+1);
     util_strgsub(cmdln, "\001\001", "%", lnlen+1);

     /* tokenise */
     len = strcspn(cmdln, "\n");
     *(cmdln+len) = '\0';
     input = cmdln + len + 1;
     len   = strcspn(cmdln, " \t");
     *(cmdln+len) = '\0';
     cmd   = cmdln + len + 1;

     /* command is executed by submitting a one off request to the job class */
     /* create parameters */
     outpurl = xnstrdup(route_getpurl(output));
     errpurl = xnstrdup(route_getpurl(error));
     /*snprintf(jobid, 64, "e-%s-%d", rtname, seq);*/
     snprintf(jobid, 64, "event-%d", seq);
     r = job_add(time(NULL), 0, 0, 1, jobid, "(event)", outpurl, errpurl, 
		 EVENT_KEEP, cmdln /*method*/, cmd);

     /* clear up and return */
     nfree(outpurl);
     nfree(errpurl);
     if (r == -1)
	  return 0;
     else 
	  return 1;
}


/* shut down the event instance */
void event_fini(EVENTINFO einfo)
{
     struct event_tracking *etrack;

     while ( ! tree_empty(einfo->track) ) {
	  tree_first(einfo->track);
	  etrack = tree_get(einfo->track);
	  nfree(etrack->rtname);
	  if (etrack->rt)
	       route_close(etrack->rt);
	  nfree(etrack);
	  tree_rm(einfo->track);
     }
     tree_destroy(einfo->track);
     nfree(einfo);
}



#if TEST

#include <stdlib.h>
#include <unistd.h>
#include "rs.h"
#include "rt_file.h"
#include "rt_std.h"
#include "rt_grs.h"
#include "sig.h"
#include "callback.h"
#include "runq.h"

#define TRING1 "t1"
#define TFILE1 "t.event.rs"
#define TPURL1 "grs:" TFILE1 "," TRING1 ",0"

int main(int argc, char **argv) {
     ROUTE eq, out, err;
     EVENTINFO einfo;
     int r;

     route_init(NULL, 0);
     route_register(&rt_filea_method);
     route_register(&rt_fileov_method);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     route_register(&rt_rs_method);
     if ( ! elog_init(1, "event test", NULL))
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
     eq = route_open(TPURL1, "event queue", NULL, 100);
     if (eq == NULL)
	  elog_die(FATAL, "[0] unable to open event queue");

     /* 1: initialise */
     einfo = event_init(TPURL1);
     if (einfo == NULL)
	  elog_die(FATAL, "[1] unable to initialise");

     /* 2: run queue */
     r = event_action(einfo, out, err);
     if (r == -1)
	  elog_die(FATAL, "[2] unable to action");

     /* 3: write an event and run queue */
     route_printf(eq, "sh uptime");
     route_flush(eq);
     r = event_action(einfo, out, err);
     if (r == -1)
	  elog_die(FATAL, "[2] unable to action");

     sleep(2);

     /* shudown */
     event_fini(einfo);
     route_close(eq);
     job_fini();
     meth_fini();
     runq_fini();
     callback_fini();
     route_close(out);
     route_close(err);
     unlink(TFILE1);
     rs_fini();
     elog_fini();
     route_fini();
     fprintf(stderr, "%s: tests finished successfully\n", argv[0]);
     exit(0);
}

#endif /* TEST */
