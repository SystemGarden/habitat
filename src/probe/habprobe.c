/*
 * Probe command line program
 * Runs probes contained in probelib
 *
 * Nigel Stuckey, August 1999, March 2000
 * Copyright system Garden Ltd 1999-2001. All rights reserved.
 */

#include <unistd.h>
#include <stdlib.h>
#include "probe.h"
#include "../iiab/table.h"
#include "../iiab/iiab.h"
#include "../iiab/elog.h"
#include "../iiab/util.h"

char usagetxt[]= IIAB_DEFUSAGE "[-i <interval> [-n <count>]] probe [probe-args]\n"
                 "Clockwork's data collection probe on the command line\n"
                 "where: probe         one of: intr, io, names, ps, sys, timer, up, down, net\n"
                 "       probe-args    optional arguments needed by probes\n"
                 "      -i <interval>  seconds between probe runs, infinite runs\n"
                 "      -n <count>     limits number of times to run probe to <count>\n"
  IIAB_DEFWHERE;

char *cfdefaults = 
"nmalloc            0\n"	/* don't check mem leaks (-1 turns on) */
"elog.all           stderr:\n"	/* log text output to stderr */
"elog.below diag    none:\n"	/* turn off diagnostics and below */
"elog.allformat     %17$s\n"	/* just want text of log */
;

/*
 * Main function
 */
int main(int argc, char *argv[]) {
     ROUTE out, err;
     char *command;
     int i, interval=0, count=0;

     /* start up and check arguments */
     iiab_start("i:n:", argc, argv, usagetxt, cfdefaults);
     if ( ! cf_defined(iiab_cmdarg, "argv1")) {
	  elog_printf(FATAL, "*** Missing probe name: "
		      "please specify which probe to run\n\n"
		      "usage: %s %s", cf_getstr(iiab_cmdarg,"argv0"), 
		      usagetxt);
	  exit(1);
     }
     if (cf_getint(iiab_cmdarg, "argc") <= 2) {
	  command = cf_getstr(iiab_cmdarg, "argv1");
     } else {
	  command = util_strjoin(cf_getstr(iiab_cmdarg, "argv1"), " ",
				 cf_getstr(iiab_cmdarg, "argv2"), " ",
				 cf_getstr(iiab_cmdarg, "argv3"), NULL);
     }
     if (cf_defined(iiab_cf, "i"))
          interval = cf_getint(iiab_cf, "i");
     if (cf_defined(iiab_cf, "n"))
          count = cf_getint(iiab_cf, "n");

     /* check count and interval permutations */
     if (count >1 && !interval) {
          elog_printf(FATAL, 
		      "*** must set an interval (with -i) if count (-n) >1");
	  exit (1);
     } else if (!interval && !count)
          count = 1;
     else if (interval >= 1 && !count)
          count = INT_MAX;

     /* run */
     out = route_open("stdout:", NULL, NULL, 0);
     err = route_open("stderr:", NULL, NULL, 0);
     if (probe_init(command, out, err, NULL) == -1) {
	  elog_printf(FATAL, "%s\nPlease specify a valid probe name",
		      usagetxt);
     } else {
          for (i=0; i < count; i++) {
	       probe_action(command, out, err, NULL);
	       if (interval && i < count-1) {
		    /* interval sleep unless it is the last in the run */
		    sleep(interval);
	       }
	  }
	  probe_fini(command, out, err, NULL);
     }

     /* shutdown */
     route_close(out);
     route_close(err);
     iiab_stop();
     exit(0);
}

