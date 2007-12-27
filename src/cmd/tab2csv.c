/*
 * Tool to convert stdin, whitespace seperated data using the util_parsetest()
 * rules into a csv file on stdout
 *
 * Nigel Stuckey,  May 1999.
 * Copyright System Garden Ltd 1999-2001, All rights reserved.
 */


#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include "../iiab/route.h"
#include "../iiab/elog.h"
#include "../iiab/util.h"
#include "../iiab/nmalloc.h"
#include "../iiab/iiab.h"

/* Globals */
char usagetxt[] = "[ <whitespace-table> ]\n"
"Converts whitespace seperated file (or stdin) into comma separated values\n\n"
"(CSV) on stdout";

char *cfdefaults =
"nmalloc        0\n"		/* don't check mem leaks (-1 turns on) */
"elog.allformat %17$s\n"	/* only want text of log */
"elog.all       none:\n"	/* throw away logs */
"elog.above     warning stderr:"/* turn on log warnings */
;

int main(int argc, char *argv[])
{
     char *buf, *bufnext, *s;
     int nread=0, totread=0, r, nabol=0;
     ITREE *lol, *l;

     /* initialise */
     iiab_start("", argc, argv, usagetxt, cfdefaults);

     /* reallocate a buffer, keeping at PIPE_BUF bytes of data free at the
      * end. read() can then append new data to old */
     bufnext = buf = xnmalloc(PIPE_BUF+1);
     while ( (nread = read(0, bufnext, PIPE_BUF)) > 0) {
          totread += nread;
          buf = xnrealloc(buf, totread + PIPE_BUF+1);
          bufnext = buf + totread;
     }

     /* parse input */
     r = util_parsetext(buf, " ", NULL, &lol);
     if (r == -1)
          elog_die(FATAL, "unable to parse input into table");
     util_parsedump(lol);

     if (r > 0) {
	  /* output table in a csv file */
	  itree_traverse(lol) {
	       l = itree_get(lol);
	       nabol = 0;
	       itree_traverse(l) {
		    if (nabol++)
			 putchar(',');
		    s = itree_get(l);
		    if (s)
			 fputs(s, stdout);
	       }
	       putchar('\n');
	  }
     }

     /* shutdown and free */
     util_freeparse(lol);
     nfree(buf);
     iiab_stop();

     return 0;
}
