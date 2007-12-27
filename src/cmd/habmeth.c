/*
 * Method command line program
 * Runs builtin methods contained in habitat
 *
 * Nigel Stuckey, October & November 2003
 * Copyright System Garden Ltd 2003. All rights reserved.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../iiab/table.h"
#include "../iiab/iiab.h"
#include "../iiab/elog.h"
#include "../iiab/meth.h"
#include "../iiab/meth_b.h"

char *cfdefaults = 
"nmalloc            0\n"	/* don't check mem leaks (-1 turns on) */
"elog.all           stderr:\n"	/* log text output to stderr */
"elog.below warning none:\n"	/* turn off warnings and below */
"elog.allformat     %17$s\n"	/* just want text of log */
;

char *usagetxt;

char *mkusage();
void listmeths(char *buf, int buflen);

/*
 * Main function
 */
int main(int argc, char *argv[]) {
     char *method, command[1024], argstr[8];
     int i, r, limit, clen=0;
     METHID methid;

     /* start up and check arguments */
     usagetxt = mkusage();
     iiab_start("", argc, argv, usagetxt, cfdefaults);
     if ( ! cf_defined(iiab_cmdarg, "argv1")) {
	  elog_printf(FATAL, "%s\nplease specify which method to run", 
		      usagetxt);
	  exit(1);
     }
     method=cf_getstr(iiab_cmdarg, "argv1");
     
     /* form a single argument from argv2..argvN */
     limit=cf_getint(iiab_cmdarg, "argc");
     clen=0;
     for (i=2; i<limit; i++) {
	  snprintf(argstr, 8, "argv%d", i);
	  clen += snprintf(command+clen, 1024-clen, "%s ", 
			   cf_getstr(iiab_cmdarg, argstr));
     }
     if (clen > 0)			/* ditch trailing space */
	  command[clen-1] = '\0';
     else
	  command[0] = '\0';

     /* run the method without invoking meth_execute(), which will honour
      * FORK types of method. We want to wait for the command to finish
      * as it is a simple command */
     meth_init();
     methid = meth_lookup(method);
     if (methid == TREE_NOVAL) {
	  elog_printf(FATAL, "%s\nmethod %s not recognised", 
		      usagetxt, cf_getstr(iiab_cmdarg, "argv1"));
	  exit(1);
     }
     
     if ((r = meth_actiononly(methid, command, "stdout", "stderr", 0)))
	  elog_printf(FATAL, "Method failed, returning %d", r);

     /* shutdown */
     meth_fini();
     iiab_stop();
     exit(r);
}


/* create usage text in a static string */
char *mkusage() {
     static char usage[2000];
     int i;

     strcpy(usage, "run a habitat method stand alone, where methods are:-\n");
     i=strlen(usage);
     listmeths(usage+i, 2000-i);
     return usage;
}


/* list methods to a string buffer */
void listmeths(char *buf, int buflen) {
     int i, curlen=0;

     for (i=0; meth_builtins[i].name; i++)
	  curlen += snprintf(buf+curlen, buflen-curlen, 
			     "      %-11.11s %s\n", 
			     meth_builtins[i].name(), 
			     meth_builtins[i].info());
}

