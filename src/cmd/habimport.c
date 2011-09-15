/*
 * Import data in the form of tables from stdin and place the data into either 
 * timestore or tablestores..
 *
 * Nigel Stuckey, July 2000
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "../iiab/cf.h"
#include "../iiab/route.h"
#include "../iiab/elog.h"
#include "../iiab/iiab.h"
#include "../iiab/nmalloc.h"
#include "../iiab/conv.h"

/* Globals and constants */
char usagetxt[] = 
"[-n <nslots> -t <desc> -p <passwd> -e <seps> -i -s -h -r] <holstore> <ring>\n"
"Import table data in fha format from stdin into a timestore or tablestore.\n"
"Unless inhibited, _host and _ring columns will be ignored\n"
"where <holstore>  holstore filename\n"
"      <ring>      name of the destination ring\n"
"      -n <nslots> slots in ring before table import (default 0 [unbound])\n"
"      -t <desc>   text description of ring\n"
"      -p <passwd> optional password for ring\n"
"      -s <seps>   set of characters used to separate values (default \\t)\n"
"      -T          inhibit recognition of time column '_time'\n"
"      -S          inhibit recognition of sequence column '_seq'\n"
"      -H          inhibit recognition of host column '_host'\n"
"      -R          inhibit recognition of ring column '_ring'\n"
"      -U          inhibit recognition of duration column '_dur'\n"
"      -f          no column titles (header) in import text\n"
"      -r          no ruler and info lines in header of import text";

char *optdefaults[] = {"n", "0",		/* number of slots */
		       "t", "imported data",	/* ring description */
		       "p", "",			/* ring password */
		       "S", "\t",		/* separators */
		       NULL, NULL};
char *optstr = "n:t:p:s:TSHRUfr";
char *cfdefaults =
"nmalloc        0\n"		/* don't check mem leaks (-1 turns on) */
"elog.allformat %17$s\n"	/* only want text of log */
"elog.all       none:\n"	/* throw away logs */
"elog.above     warning stderr:"/* turn on logs from warnings and above */
;


int main(int argc, char *argv[]) {
     int r, nread=0, totread=0;
     int withtitle=1, withruler=1, withtime=1, withseq=1, withhost=1, 
	  withring=1, withdur=1;
     char *buf, *bufnext;

     /* Initialisation */
     iiab_start(optstr, argc, argv, usagetxt, cfdefaults);

     /* process command line and switches */
     if ( ! cf_defined(iiab_cmdarg, "argv1")) {
          elog_printf(FATAL, "holstore file not supplied\n"
		      "usage: %s %s\n", cf_getstr(iiab_cmdarg,"argv0"), 
		      usagetxt);
	  iiab_stop();
	  exit(1);
     }
     if ( ! cf_defined(iiab_cmdarg, "argv2")) {
          elog_printf(FATAL, "destination ring not supplied\n"
		      "usage: %s %s\n", cf_getstr(iiab_cmdarg,"argv0"), 
		      usagetxt);
	  iiab_stop();
	  exit(1);
     }
     cf_default(iiab_cmdarg, optdefaults);
     if (cf_defined(iiab_cmdarg, "f"))  withtitle = 0;
     if (cf_defined(iiab_cmdarg, "r"))  withruler = 0;
     if (cf_defined(iiab_cmdarg, "T"))  withtime  = 0;
     if (cf_defined(iiab_cmdarg, "S"))  withseq   = 0;
     if (cf_defined(iiab_cmdarg, "H"))  withhost  = 0;
     if (cf_defined(iiab_cmdarg, "R"))  withring  = 0;
     if (cf_defined(iiab_cmdarg, "U"))  withdur   = 0;

     /* read stdin to memory.
      * reallocate a buffer, keeping at PIPE_BUF bytes of data free at the
      * end. read() can then append new data to old */
     bufnext = buf = xnmalloc(PIPE_BUF+1);
     while ( (nread = read(0, bufnext, PIPE_BUF)) > 0) {
          totread += nread;
          buf = xnrealloc(buf, totread + PIPE_BUF+1);
          bufnext = buf + totread;
     }
     *(buf+totread) = '\0';

     /* check for empty input */
     if (totread == 0) {
          elog_printf(FATAL, "empty input\n");
	  iiab_stop();
	  exit(1);
     }

     /* carry out import conversion */
     r = conv_mem2ring(buf,
		       cf_getstr(iiab_cmdarg, "argv1") /* holstore */,
		       0644,
		       cf_getstr(iiab_cmdarg, "argv2") /* ring */,
		       cf_getstr(iiab_cmdarg, "t")     /* description */,
		       cf_getstr(iiab_cmdarg, "p")     /* password */,
		       cf_getint(iiab_cmdarg, "n")     /* nslots */,
		       cf_getstr(iiab_cmdarg, "s")     /* separators */,
		       withtitle, 
		       withruler, 
		       withtime, 
		       withseq,
		       withhost,
		       withring,
		       withdur);

     if ( r == -1 ) {
          elog_send(FATAL, "unable to import data");
	  iiab_stop();
	  exit(1);
     }

     /* Destruction */
     iiab_stop();

     exit(0);
}
