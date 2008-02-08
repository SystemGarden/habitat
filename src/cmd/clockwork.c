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
void stopclock(int sig /* signal vector */);

/* initialise globals */
char usagetxt[] = "[-j <jobs>] [-sf]\n" \
     "where -j <jobs>   use jobs from route <jobs>; don't daemonise (imply -s)\n"
     "      -f          run in foreground, don't daemonise\n"
     "      -s          server off: do not listen for data requests from network";
char helptxt[] = "no help for the weary";
char *cfdefaults = 
     "iiab.debug -1\n"
     "job.debug  -1\n"
     "nmalloc    0\n"	/* 0: memory checking off, !0: memory checking on */
     "log        stderr:\n"
     /*"jobs       file:%l/clockwork.jobs\n"*/
     "jobs       rs:%v/%h.rs,clockwork,0\n"
     "elog.all   none:\n"
     "elog.above warning rs:%v/%h.rs,log\n";
char *cfdefaults_userjobs = 
     "iiab.debug -1\n"
     "job.debug  -1\n"
     "nmalloc    0\n"	/* 0: memory checking off, !0: memory checking on */
     "log        stderr:\n"
     "elog.all   none:\n"
     "elog.above warning stderr:\n";
int   debug=0;			/* Debug state */
int   clock_done_init=0;

#define CLOCKWORK_KEYNAME "clockwork"
#define DEFJOBS_TYPE "file:"
#define DEFJOBS_FILE "clockwork.jobs"

int main(int argc, char **argv) {
     int r, joblen, errorstatus=0;
     char *jobpurl, jobpurl_t[1024], *jobtxt, *defjobs, buf[1024];
     time_t clock;
     ROUTE jobrt;

     /* initialise, fetching the directory locations & expanding them before 
      * hand, then ensure we are the only clockwork running on this box.
      * By default send errors to the default data store, unless 
      * errors are overridden or -j switch is given. */
#if 0
     iiab_dir_locations(argv[0]);
     if (iiab_iscmdopt("j", argc, argv))
          route_expand(buf, cfdefaults_userjobs, NULL, NULL);
     else
          route_expand(buf, cfdefaults, NULL, NULL);
     iiab_start("sj:", argc, argv, usagetxt, buf);
#endif
     if (iiab_iscmdopt("j", argc, argv))
          iiab_start("sfj:", argc, argv, usagetxt, cfdefaults_userjobs);
     else
          iiab_start("sfj:", argc, argv, usagetxt, cfdefaults);


     /* process switches and arguments */
     /* debug flag */
     if (cf_defined(iiab_cf, "d") && cf_getint(iiab_cf, "d") == -1)
	  debug++;

     /* initialise the classes needed in addition to those started by 
      * iiab_start(); mostly these are for job & dispatching.
      */
     sig_init();
     meth_init();
     meth_add(&probe_cbinfo);
     runq_init(time(NULL));
     job_init();
     clock_done_init++;
     if (cf_defined(iiab_cf, "j")) {
          /* replacement jobs and private mode */
          cf_putstr(iiab_cf, "jobs", cf_getstr(iiab_cf, "j"));
     } else {
          if ( ! cf_defined(iiab_cf, "f"))
	       /* default running: we want to be a daemon! */
	       iiab_daemonise();
	  iiab_lockordie(CLOCKWORK_KEYNAME);

	  /* only in daemon mode can we provide the server unless its 
	   * turned off */
	  if ( ! cf_defined(iiab_cf, "s") ) {
	       /* start http server with the following services */
	       httpd_init();
	       httpd_addpath("/ping",     httpd_builtin_ping);
	       httpd_addpath("/cf",       httpd_builtin_cf);
	       httpd_addpath("/elog",     httpd_builtin_elog);
	       httpd_addpath("/info",     httpd_builtin_info);
	       httpd_addpath("/local/",   httpd_builtin_local);
	       httpd_addpath("/localtsv/",httpd_builtin_local);
	       httpd_start();
	  }
     }

     /* set up signal handlers */
     sig_setexit(stopclock);

     /* check 'jobs' variable exists, if not we can't set one up */
     jobpurl = cf_getstr(iiab_cf, "jobs");
     route_expand(jobpurl_t, jobpurl, "NOJOB", 0);
     if (jobpurl_t == NULL || jobpurl_t == '\0') {
	  elog_printf(FATAL, "unable to load jobs, as the "
		      "configuration directive was not set. Please set "
		      "the directive `jobs' in the configuration file "
		      "to the route containing jobs. For example, "
		      "`jobs=file:/etc/clockwork.jobs' will look for the "
		      "file /etc/clockwork.jobs. If the route is set but "
		      "does not exist, then it will be created using the "
		      "default job definitions");
	  errorstatus = 1;
	  goto end_app;
     }

     /* Access the job table to see if it exists. If it does not, then
      * assume that thats were we want the default job config created */
     if ( route_access(jobpurl_t, NULL, ROUTE_READOK) != 1 ) {
	  /* jobs directive set but no file there => create it */

	  /* read the default jobs file and save them in the standard
	   * place. This is normally the data store, but could be 
	   * a file */
	  defjobs = util_strjoin(DEFJOBS_TYPE, iiab_dir_lib, "/",
				 DEFJOBS_FILE, NULL);
	  jobtxt = route_read(defjobs, NULL, &joblen);
	  if (jobtxt == NULL) {
	       elog_printf(FATAL, "unable to read default job table. "
			   "Either (1) create job table in expected place "
			   "(%s) so avoiding use of defaults or (2) make "
			   "default jobs readable/available (%s)",
			   jobpurl_t, defjobs);
	       errorstatus = 2;
	       goto end_app;
	  }

	  jobrt = route_open(jobpurl_t, "Job table for clockwork", NULL, 10);
	  if (jobrt == NULL) {
	       elog_printf(FATAL, "unable to create initial job table "
			   "to write defaults. Please make job table "
			   "writable (%s) or create manually",
			   jobpurl_t);
	       nfree(jobtxt);
	       errorstatus = 3;
	       goto end_app;
	  }

	  r = route_write(jobrt, jobtxt, joblen);
	  if (r == -1) {
	       elog_printf(FATAL, "unable to create initial job table "
			   "to write defaults. Please make job table"
			   "writable (%s) or create manually",
			   jobpurl_t);
	       route_close(jobrt);
	       nfree(jobtxt);
	       errorstatus = 4;
	       goto end_app;
	  }

	  elog_printf(INFO, "no jobs found in %s: created default", 
		      jobpurl_t);
	  route_close(jobrt);
	  nfree(jobtxt);
     }

     /* load jobs */
     r = job_loadroute( jobpurl_t );
     if (r == -1)
          elog_die(FATAL, "failed to load jobs from %s",
		   cf_getstr(iiab_cf, "jobs") );
     else
          elog_printf(INFO, "loaded %d jobs", r);

     /* run jobs in var if we have a public responsibility */
     if ( ! cf_defined(iiab_cf, "j") )
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
	  clock = time(NULL);
	  fprintf(stderr, "%s: exit with errorstatus %d at %s", argv[0], 
		  errorstatus, ctime(&clock));
     }
     exit(errorstatus);
}


/* shutdown clockwork */
void stopclock(int sig /* signal vector */) {
     int r;
     time_t clock;

     if (!clock_done_init)
          _exit(r);

     sig_off();
     elog_printf(INFO, "clockwork shutting down from signal %d", sig);

     runq_disable();
     r = meth_shutdown();
     if (r) {
	  clock = time(NULL);
          elog_printf(WARNING, "%d jobs did not shutdown normally", r);
	  fprintf(stderr, "%s: shutdown from signal %d, meth_shutdown() %d "
		  "at %s", cf_getstr(iiab_cmdarg, "argv0"), sig, 
		  r, ctime(&clock));
     } else
          elog_printf(INFO, "%s successfully shutdown", 
		      cf_getstr(iiab_cmdarg, "argv0"));
     /*job_clear();*/

     /* shut down and clear up */
     job_fini();
     runq_fini();
     meth_fini();
     iiab_stop();
     exit(r);
}

