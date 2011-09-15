/*
 * Clockwork - a periodic execution utility for iiab
 *
 * Nigel Stuckey, December 1998
 * Copyright System Garden Limited 1998-2008. All rights reserved.
 *
 * Think of clockwork as cron with `knobs on'.
 * Using functionality in libiiab, it has more timing control
 * than cron and has the ability to execute with arbitary execution types.
 * It also pipes its jobs through the route_ class, thus using timestore
 * from stdout of a job.
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "../iiab/route.h"
#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/cf.h"
#include "../iiab/util.h"
#include "../iiab/meth.h"
#include "../iiab/elog.h"
#include "../iiab/iiab.h"
#include "../iiab/job.h"
#include "../iiab/nmalloc.h"
#include "../iiab/httpd.h"
#include "../iiab/sig.h"
#include "../iiab/runq.h"
#include "../iiab/job.h"
#include "../probe/probe.h"

int main(int, char **);
int load_jobs(char *purl);
void stopclock_meth();
void stopclock_sig(int sig /* signal vector */);
void stopclock();

/* initialise globals */
char usagetxt[] = "\n                  [-j <stdjob> | -J <jobrt>] [-sf]\n"
     "where -j <stdjob> select from standard job tables\n"
     "      -J <jobrt>  use jobs from route <jobrt> in foreground not as \n"
     "                  a daemon (imply -sf options)\n"
     "      -f          run in foreground, don't daemonise, don't lock, don't serve\n"
     "      -s          server off: do not listen for data requests from network";
char helptxt[] = "no help for the weary";
char *cfdefaults = 
     "iiab.debug -1\n"
     "job.debug  -1\n"
     "nmalloc    0\n"	/* 0: memory checking off, !0: memory checking on */
     "log        stderr:\n"
     "jobs       file:%l/norm.jobs\n"
     "elog.all   none:\n"
     "elog.above warning stderr:"
  /*     "elog.above warning grs:%v/%h.grs,log\n"*/
;

int   debug=0;			/* Debug state */
int   clock_done_init=0;

#define CLOCKWORK_KEYNAME "clockwork"

int main(int argc, char **argv) {
     int njobs, opt_s=0, opt_f=0, opt_j=0, opt_J=0, errorstatus=0;
     char *jobpurl, jobpurl_t[1024], *jobcf=NULL;
     time_t clock;

     /* start up with default options */
     iiab_start("sfj:J:", argc, argv, usagetxt, cfdefaults);
     clock = time(NULL);

     /* process switches and arguments */
     if (cf_defined(iiab_cf, "d") && cf_getint(iiab_cf, "d") == -1)
	  debug++;     /* debug flag */
     if (cf_defined(iiab_cf, "s"))
	  opt_s++;     /* no server flag */
     if (cf_defined(iiab_cf, "f"))
	  opt_f++;     /* foreground flag */
     if (cf_defined(iiab_cf, "j"))
	  opt_j++;     /* standard job flag */
     if (cf_defined(iiab_cf, "J")) {
	  opt_J++;     /* route job flag */
	  opt_s++;     /* don't implement server daemon */
	  opt_f++;     /* don't background */
     }

     if (opt_j && opt_J) {
          elog_printf(FATAL, "Can't specify -j and -J together, please pick "
		      "one only\n"
		      "%s %s", argv[0], usagetxt);
	  iiab_stop();
	  exit(10);	/* dont allow -j and -J */
     }
     if (opt_j) {
          /* replace job config with different standard table */
          jobcf = util_strjoin("file:%l/", cf_getstr(iiab_cf, "j"), ".jobs",
			       NULL);
          cf_putstr(iiab_cf, "jobs", jobcf);
	  nfree(jobcf);
     }
     if (opt_J) {
          /* replace jobs with user supplied route */
          cf_putstr(iiab_cf, "jobs", cf_getstr(iiab_cf, "J"));
     }

     /* check 'jobs' directive exists, and expand it if so */
     jobpurl = cf_getstr(iiab_cf, "jobs");
     route_expand(jobpurl_t, jobpurl, "NOJOB", 0);
     if (*jobpurl_t == '\0') {
	  fprintf(stderr, "Unable to load jobs, as there was no valid "
		  "configuration directive. Please specify -j, -J or set "
		  "the directive `jobs' in the configuration file to the "
		  "route containing a job table. For example, "
		  "`jobs=file:/etc/clockwork.jobs' will look for the "
		  "file /etc/clockwork.jobs");
	  elog_printf(FATAL, "Unable to load jobs without valid config "
		      "directive (looking for 'jobs' in cf)");
	  iiab_stop();
	  exit(1);
     }

     /* Access the expanded route location to see if it exists. If it does not,
      * then error and stop further operation */
     if ( route_access(jobpurl_t, NULL, ROUTE_READOK) != 1 ) {
	  /* jobs directive set but no file there => error */
          elog_printf(FATAL, "Unable to access %s to read jobs", jobpurl_t);
	  if (opt_J) {
	       fprintf(stderr, "Unable to access route '%s' to read jobs\n"
		       "Please check the name & location and start again\n",
		       jobpurl_t);
	  } else if (opt_j) {
	       fprintf(stderr, "Unable to read standard jobs '%s'\n"
		       "  (looking for %s)\n"
		       "  Please check the name of the job file and start "
		       "again\n", cf_getstr(iiab_cf, "j"), jobpurl_t);
	  } else {
	       fprintf(stderr, "Unable to read default jobs\n"
		       "  (looking for %s)\n"
		       "  Please check the installation to ensure all support "
		       "files are in place\n", jobpurl_t);
	  }
	  iiab_stop();
	  exit(2);
     }

     /* initialise the classes needed in addition to those started by 
      * iiab_start(): signals, methods, runq and jobs. Method class is a 
      * library of action code (including probes), jobs is a record of 
      * work activities against time and runq combines the previous two 
      * by actually doing the dispatching.
      */
     sig_init();
     meth_init(argc, argv, stopclock_meth);
     meth_add(&probe_cbinfo);
     runq_init(time(NULL));
     job_init();
     clock_done_init++;
     if ( ! opt_f ) {
          /* default running: we want to be a daemon! */
          iiab_daemonise();
	  iiab_lockordie(CLOCKWORK_KEYNAME);

	  /* only in daemon mode can we provide the server unless its 
	   * turned off */
	  if ( ! opt_s ) {
	       /* start http server with the following services */
	       httpd_init();
	       httpd_addpath("/ping",     httpd_builtin_ping);
	       httpd_addpath("/cf",       httpd_builtin_cf);
	       httpd_addpath("/cftsv",    httpd_builtin_cf);
	       httpd_addpath("/elog",     httpd_builtin_elog);
	       httpd_addpath("/info",     httpd_builtin_info);
	       httpd_addpath("/local/",   httpd_builtin_local);
	       httpd_addpath("/localtsv/",httpd_builtin_local);
	       httpd_start();
	  }
     }

     /* set up signal handlers */
     sig_setexit(stopclock_sig);

     /* load jobs */
     njobs = job_loadroute( jobpurl_t );
     if (njobs == -1) {
          elog_die(FATAL, "unable to start due to a failure to read jobs "
		   "from %s. Please check that the file is readable and that "
		   "the table location exists.", jobpurl_t);
	  errorstatus = 5;
	  goto end_app;
     } else
          elog_printf(INFO, "loaded %d jobs", njobs);

     elog_printf(INFO, "Running %s in %s,%s listening to network, "
		 "jobs from '%s', started at %s", 
		 argv[0],
		 opt_f ? "foreground" : "background",
		 opt_s ? " not" : "",
		 (opt_J ? cf_getstr(iiab_cf, "J") : 
		    (opt_j ? cf_getstr(iiab_cf, "j") : "norm.jobs") ),
		 ctime(&clock) );

     /* run jobs in var dir if we have a public responsibility to 
      * be the data server for the host, otherwise stay in the launch dir */
     if ( ! opt_f )
	  chdir(iiab_dir_var);
     while(1) {
          elog_printf(DEBUG, "relay returns %d", meth_relay());
	  /*runq_dump();*/
     }

     /* shut down and clear up */
 end_app:     
     job_fini();
     runq_fini();
     meth_fini();
     iiab_stop();
     if (errorstatus) {
	  fprintf(stderr, "%s: exit with errorstatus %d at %s", argv[0], 
		  errorstatus, ctime(&clock));
     }
     exit(errorstatus);
}


/* shutdown clockwork by a method */
void stopclock_meth() {
     elog_printf(INFO, "clockwork shutting down from a method");
     stopclock();
}

/* shutdown clockwork by a signal */
void stopclock_sig(int sig /* signal vector */) {
     sig_off();
     elog_printf(INFO, "clockwork shutting down from signal %d", sig);
     stopclock();
}

/* shutdown clockwork */
void stopclock() {
     int r;
     time_t clock;

     if (!clock_done_init)	/* full shutdown not needed, but not an error */
          _exit(0);

     runq_disable();		/* prevents further work */
     r = meth_shutdown();	/* kills running processes */
     if (r) {
	  clock = time(NULL);
          elog_printf(WARNING, "%d jobs did not shutdown normally", r);
	  fprintf(stderr, "%s: shutdown, meth_shutdown() %d "
		  "at %s", cf_getstr(iiab_cmdarg, "argv0"), 
		  r, ctime(&clock));
     } else {
          elog_printf(INFO, "%s successfully shutdown", 
		      cf_getstr(iiab_cmdarg, "argv0"));
     }
     /*job_clear();*/

     /* shut down and clear up */
     job_fini();
     runq_fini();
     meth_fini();
     iiab_stop();
     exit(r);
}

