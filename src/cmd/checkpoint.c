/*
 * Simple utility to checkpoint a holstore
 * usage: checkpoint <holstore>
 *
 * Nigel Stuckey, 1999
 * Copyright System Garden Ltd 1999-2001. All rights reserved
 */

#include <stdlib.h>
#include "../iiab/elog.h"
#include "../iiab/route.h"
#include "../iiab/holstore.h"
#include "../iiab/iiab.h"

char usagetxt[] = "[-h] <holstore>\n-h       help";

char helptxt[] = "checkpoints a holstore";

char *cfdefaults = 
"nmalloc    0\n"
"elog.all   none:\n"
"elog.above warning stderr:";

int main(int argc, char *argv[]) {
     HOLD holid;

     /* initialise and go to base directory */
     iiab_start("h", argc, argv, usagetxt, cfdefaults);
     /*chdir( cf_getstr(iiab_cf, IIAB_HOMEDIR) );*/

     /* The main work */
     if (argc != 2) {
	  elog_printf(FATAL, "usage: %s <holstore>", argv[0]);
	  exit(1);
     }

     holid = hol_open(argv[1]);
     if ( ! holid) {
	  elog_printf(FATAL, "unable to open %s", argv[1]);
	  exit(2);
     }

     hol_checkpoint(holid);

     hol_close(holid);

     iiab_stop();

     return 0;
}
