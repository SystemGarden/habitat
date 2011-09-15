/*
 * Route recieve command line tool
 * Provides an command line interface to recieve from the route class
 *
 * Nigel Stuckey, October 2003
 * Copyright system Garden Ltd 1996-2003. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../iiab/cf.h"
#include "../iiab/table.h"
#include "../iiab/route.h"
#include "../iiab/elog.h"
#include "../iiab/iiab.h"
#include "../iiab/nmalloc.h"

/* Globals */
char usagetxt[] = "[-f|-t] [-s <sep> -l -i] [-a|y|o] [-p <passwd>] [-E] <route>\n"
"where <route>     route address\n"
"      -f          print free text (where possible)\n"
"      -t          print a table [default] (fat headed array format)\n"
"      -l          if table - no column titles (header)\n"
"      -i          if table - no info and ruler lines in header\n"
"      -s <sep>    table value separator [default space justification]\n"
"      -a          print entire ring (route seq overrides)\n"
"      -y          print youngest sequence from ring (route seq overrides)\n"
"      -o          print oldest sequence from ring (address overrides)\n"
"      -p <passwd> optional password for reading ringed routes\n"
"      -E          escape text data whenever it is not printable";
char *optdefaults[] = {"t", "-1",		/* table/fha format */
		       "s", "\t",		/* table value sep */
		       "p", "",			/* ring password */
		       NULL, NULL};
char *cfdefaults = 
"nmalloc        0\n"		/* don't check mem leaks (-1 turns on) */
"elog.allformat %17$s\n"	/* only want text of log */
"elog.all       none:\n"	/* throw away logs */
"elog.above     warning stderr:"/* turn on log warnings */
;

int main(int argc, char *argv[]) {
     char *buf;
     int nread, readtext=0, withtitle=1, withinfo=1;
     TABLE tab;

     /* Initialisation */
     iiab_start("ftlis:ayop:E", argc, argv, usagetxt, cfdefaults);
     if ( ! cf_defined(iiab_cmdarg, "argv1")) {
          elog_printf(FATAL, "*** route not supplied\n"
		      "usage: %s %s\n", cf_getstr(iiab_cmdarg,"argv0"), 
		      usagetxt);
	  iiab_stop();
	  exit(1);
     }
     cf_default(iiab_cmdarg, optdefaults);
     if (cf_defined(iiab_cmdarg, "f"))  readtext  = 1;
     if (cf_defined(iiab_cmdarg, "l"))  withtitle = 0;
     if (cf_defined(iiab_cmdarg, "i"))  withinfo  = 0;

     if (readtext) {
	  /* open data as free text */
	  buf = route_read(cf_getstr(iiab_cmdarg, "argv1"), 
			    cf_getstr(iiab_cmdarg, "p"), &nread);
	  if ( ! buf ) {
	       elog_printf(FATAL, "no data returned");
	       iiab_stop();
	       exit(1);
	  }

	  /* send route to stdout */
	  write(1, buf, nread);
     } else {
	  /* get data as table */
	  tab = route_tread(cf_getstr(iiab_cmdarg, "argv1"), 
			     cf_getstr(iiab_cmdarg, "p"));
	  if (tab) {
	       if (cf_defined(iiab_cmdarg, "s")) {
		    buf = table_outtable_full(tab, 
					      *cf_getstr(iiab_cmdarg, "s"),
					      cf_getint(iiab_cmdarg, "l"),
					      cf_getint(iiab_cmdarg, "i"));
	       } else {
		    /* pretty print */
		    buf = table_print(tab);
	       }
	       printf("%s", buf);
	       nfree(buf);
	  }
     }
     /* destruction */
     iiab_stop();
     exit(0);
}
