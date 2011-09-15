/*
 * Export data from a either a tablestore or timestore to a text table format 
 * on stdout
 *
 * Nigel Stuckey, July 2000
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "../iiab/cf.h"
#include "../iiab/route.h"
#include "../iiab/elog.h"
#include "../iiab/iiab.h"
#include "../iiab/nmalloc.h"
#include "../iiab/conv.h"

/* Globals and constants */
char usagetxt[] = 
"[-f <fdt> -l <ldt> -m <dt-fmt> -p <passwd> -t <sep> -isHRSTr] <holstore> <ring>\n"
"Export timestore or tablestore ring to a text table format on stdout\n"
"where <holstore>  holstore filename\n"
"      <ring>      name of the destination ring\n"
"      -f <fdt>    begining date time of export range\n"
"      -l <ldt>    last date time of export range\n"
"      -m <dt-fmt> strftime() date time format (default secs since 1/1/1970)\n"
"      -p <passwd> optional password for ring\n"
"      -t <sep>    value separator (default ',')\n"
"      -i          inhibit generation of time column '_time'\n"
"      -s          inhibit generation of sequence column '_seq'\n"
"      -H          inhibit generation of host column '_host'\n"
"      -R          inhibit generation of ring column '_ring'\n"
"      -S          inhibit generation of duration column '_dur'\n"
"      -T          no column titles (header) in export text\n"
"      -r          no ruler and info lines in header of export text";

char *optdefaults[] = {"p", "",				/* ring password */
		       "m", "",				/* date time format */
		       "t", ",",			/* separator */
		       NULL, NULL};
char *optstr = "f:l:m:p:t:isHRSTr";
char *cfdefaults =
"nmalloc        0\n"		/* don't check mem leaks (-1 turns on) */
"elog.allformat %17$s\n"	/* only want text of log */
"elog.all       none:\n"	/* throw away logs */
"elog.above     warning stderr:"/* turn on logs from warnings and above */
;


int main(int argc, char *argv[]) {
     int withtitle=1, withruler=1, withtime=1, withseq=1, withhost=1, 
	  withring=1, withdur=1;
     char *buf;
     time_t first_t, last_t;
     struct tm date_tm;

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
          elog_printf(FATAL, "source ring not supplied\n"
		      "usage: %s %s\n", cf_getstr(iiab_cmdarg,"argv0"), 
		      usagetxt);
	  iiab_stop();
	  exit(1);
     }
     cf_default(iiab_cmdarg, optdefaults);
     if (cf_defined(iiab_cmdarg, "T"))  withtitle = 0;
     if (cf_defined(iiab_cmdarg, "r"))  withruler = 0;
     if (cf_defined(iiab_cmdarg, "i"))  withtime  = 0;
     if (cf_defined(iiab_cmdarg, "s"))  withseq   = 0;
     if (cf_defined(iiab_cmdarg, "H"))  withhost  = 0;
     if (cf_defined(iiab_cmdarg, "R"))  withring  = 0;
     if (cf_defined(iiab_cmdarg, "S"))  withdur  = 0;

     /* parse the time -- would like to have used strptime, but due to
      * a bug in old gnu libc, I've gone for explicit parsing */
     date_tm.tm_sec = date_tm.tm_min = date_tm.tm_hour = 0;
     if (cf_defined(iiab_cmdarg, "f")) {
	  sscanf(cf_getstr(iiab_cmdarg, "f"), "%d/%d/%d", &date_tm.tm_mday,
		&date_tm.tm_mon, &date_tm.tm_year);
	  date_tm.tm_mon--;		/* 0 == jan */
	  date_tm.tm_year -= 1900;	/* years since 1900 */
	  first_t = mktime(&date_tm);
     } else {
	  first_t = -1;
     }
     if (cf_defined(iiab_cmdarg, "l")) {
	  sscanf(cf_getstr(iiab_cmdarg, "l"), "%d/%d/%d", &date_tm.tm_mday,
		   &date_tm.tm_mon, &date_tm.tm_year);
	  date_tm.tm_mon--;		/* 0 == jan */
	  date_tm.tm_year -= 1900;	/* years since 1900 */
	  last_t = mktime(&date_tm) + 86399;	/* add 23h 59m 59s */
     } else {
	  last_t = -1;
     }

     /* carry out export conversion */
     buf = conv_ring2mem(cf_getstr(iiab_cmdarg, "argv1") /* holstore */,
			 cf_getstr(iiab_cmdarg, "argv2") /* ring */,
			 cf_getstr(iiab_cmdarg, "p")     /* password */,
			 *cf_getstr(iiab_cmdarg, "t")    /* separator */,
			 withtitle, 
			 withruler, 
			 withtime,
			 cf_getstr(iiab_cmdarg, "m")     /* dt fmt */,
			 withseq,
			 withhost,
			 withring,
			 withdur,
			 first_t,
			 last_t);

     if ( buf == NULL ) {
          elog_send(FATAL, "unable to export data");
	  iiab_stop();
	  exit(1);
     }

     /* write memory to stdout */
     write(1, buf, strlen(buf));
     write(1, "\n", 1);

     /* Destruction */
     nfree(buf);
     iiab_stop();

     exit(0);
}
