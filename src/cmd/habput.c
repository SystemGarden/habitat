/*
 * Route send command line tool
 * Provides an command line interface to send to the route class
 *
 * Nigel Stuckey, October 1993
 * Copyright System Garden Ltd 1996-2003. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../iiab/cf.h"
#include "../iiab/table.h"
#include "../iiab/route.h"
#include "../iiab/elog.h"
#include "../iiab/iiab.h"

/* Globals */
char usagetxt[] = "[-f|-t] [-s <sep> -l -i] [-s <nslots> -m <desc>] [-p <passwd>] <route>\n"
"send a table [default] or free text [-f] from stdin to a route\n"
"where <route>     destination route address\n"
"      -f          free text on stdin, cancels table mode\n"
"      -t          table on stdin (fat headed array format) [default]\n"
"      -l          if table - no column titles (labels/header)\n"
"      -i          if table - no info and ruler lines in header\n"
"      -s <sep>    value separators [default tab]\n"
"      -n <nslots> number of slots for creating ringed routes [default 1000]\n"
"      -m <desc>   text description for creating ringed routes\n"
"      -p <passwd> optional password for creating ringed routes";

char *optdefaults[] = {"t", "-1",			/* table/fha format */
		       "s", "\t",			/* table value sep */
		       "n", "1000",			/* number of slots */
		       "m", "Sample description",	/* ring description */
		       "p", "",				/* ring password */
		       NULL, NULL};

char *cfdefaults =
"nmalloc        0\n"		/* don't check mem leaks (-1 turns on) */
"elog.allformat %17$s\n"	/* only want text of log */
"elog.all       none:\n"	/* throw away logs */
"elog.above     warning stderr:"/* turn on log warnings */
;

char *buf[PIPE_BUF];

int main(int argc, char *argv[]) {
     int r, nread, readtext=0, withtitle=1, withinfo=1;
     ROUTE out;
     TABLE tab;
     char *route_name;

     /* Initialisation */
     iiab_start("ftlis:n:m:p:", argc, argv, usagetxt, cfdefaults);

     /* process command line and switches */
     if ( ! cf_defined(iiab_cmdarg, "argv1")) {
          elog_printf(FATAL, "route not supplied\n"
		      "usage: %s %s\n", cf_getstr(iiab_cmdarg,"argv0"), 
		      usagetxt);
	  iiab_stop();
	  exit(1);
     }
     cf_default(iiab_cmdarg, optdefaults);
     if (cf_defined(iiab_cmdarg, "f"))  readtext  = 1;
     if (cf_defined(iiab_cmdarg, "l"))  withtitle = 0;
     if (cf_defined(iiab_cmdarg, "i"))  withinfo  = 0;

     /* open output route */
     route_name = cf_getstr(iiab_cmdarg, "argv1");
     out = route_open(route_name,
		      cf_getstr(iiab_cmdarg, "m"), 
		      cf_getstr(iiab_cmdarg, "p"),
		      cf_getint(iiab_cmdarg, "n"));
     if ( ! out ) {
          elog_printf(FATAL, "unable to open route: %s", route_name);
	  iiab_stop();
	  exit(1);
     }

     /* send stdin to be buffered in route */
     while ( (nread = read(0, buf, PIPE_BUF)) > 0) {
	  r = route_write(out, buf, nread);
	  if (r == -1)
	       elog_die(FATAL, "error buffering to route: %s", route_name);
     }

     /* now write the route buffer in text mode or table mode */
     if (readtext) {
	  r = route_flush(out);
	  if (!r)
	       elog_die(FATAL, "error writing text to route: %s", route_name);
     } else {
	  /* table mode - grab the text back from the route and scan it 
	   * into a table */
	  tab = table_create();
	  r = table_scan(tab, route_buffer(out, NULL), 
			 cf_getstr(iiab_cmdarg, "s"), TABLE_SINGLESEP,
			 withtitle, withinfo);
	  table_freeondestroy(tab, route_buffer(out, NULL));
	  route_killbuffer(out, 0);
	  if (r == -1) {
	       elog_send(FATAL, "unable to scan text from stdin into table");
	  } else {
	       r = route_twrite(out, tab);
	       if ( !r )
		    elog_printf(FATAL, "unable to write table to route: %s",
				route_name);
	  }
	  table_destroy(tab);
     }

     /* close route */
     route_close(out);

     /* Destruction */
     iiab_stop();

     exit(0);
}
