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

char usagetxt[]= "run the data collection probe stand alone\n"
                 "probes are: intr, io, names, ps, sys, timer, up, down, net\n"
                 "      -2          run probe again after 5 seconds";

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

     /* start up and check arguments */
     iiab_start("2", argc, argv, usagetxt, cfdefaults);
     if ( ! cf_defined(iiab_cmdarg, "argv1")) {
	  elog_printf(FATAL, "%s\nplease specify which probe to run", 
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

     /* run */
     out = route_open("stdout:", NULL, NULL, 0);
     err = route_open("stderr:", NULL, NULL, 0);
     if (probe_init(command, out, err, NULL) == -1) {
	  elog_printf(FATAL, "%s\nPlease specify a valid probe name",
		      usagetxt);
     } else {
	  probe_action(command, out, err, NULL);
	  if (cf_defined(iiab_cf, "2")) {
	       sleep(5);
	       probe_action(command, out, err, NULL);
	  }
	  probe_fini(command, out, err, NULL);
     }

     /* shutdown */
     route_close(out);
     route_close(err);
     iiab_stop();
     exit(0);
}

