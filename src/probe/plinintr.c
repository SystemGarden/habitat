/*
 * Linux interrupt probe for iiab
 * Nigel Stuckey, September 1999, March 2000
 * Updated for the 2.4 kernel April 2001
 *
 * Copyright System Garden Ltd 1999-2001. All rights reserved.
 */

#if linux

#include <stdio.h>
#include <stdlib.h>
#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"
#include "../iiab/util.h"
#include "probe.h"

/* Linux specific includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/* table constants for system probe */
struct probe_sampletab plinintr_cols[] = {
     {"name",	  "", "str", "cnt", "", "1","device name"},
     {"hard",	  "", "u32", "cnt", "", "", "interrupts from hardware "
      					    "device"},
     {"soft",	  "", "u32", "cnt", "", "", "interrupts self induced by "
					    "system"},
     {"watchdog", "", "u32", "cnt", "", "", "interrupts from a periodic "
      					    "timer"},
     {"spurious", "", "u32", "cnt", "", "", "interrupts for unknown reason"},
     {"multisvc", "", "u32", "cnt", "", "", "multiple servicing during "
					    "single interrupt "},
     PROBE_ENDSAMPLE
};

/* List of colums to diff */
struct probe_rowdiff plinintr_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *plinintr_getcols()    {return plinintr_cols;}
struct probe_rowdiff   *plinintr_getrowdiff() {return plinintr_diffs;}
char                  **plinintr_getpub()     {return NULL;}

/* linux version; assume 2.x being the latest */
int plinintr_linuxversion=30;
int plinintr_ncpu=1;

/*
 * Initialise probe for linux interrupt information
 */
void plinintr_init() {
     char *data, *vpt;

     /* we need to work out which version of linux we are running */
     data = probe_readfile("/proc/version");
     if (!data) {
          elog_printf(ERROR, "unable to find the linux kernel "
		      "version file");
	  return;
     }
     vpt = strstr(data, "version ");
     if (!vpt) {
          elog_printf(ERROR, "unable to find the linux kernel version");
	  nfree(data);
	  return;
     }
     vpt += 8;
     if (strncmp(vpt, "2.1.", 4) == 0 || strncmp(vpt, "2.2.", 4) == 0) {
          plinintr_linuxversion=22;
     } else if (strncmp(vpt, "2.3.", 4) == 0 || strncmp(vpt, "2.4.", 4) == 0) {
          plinintr_linuxversion=24;
     } else if (strncmp(vpt, "2.5.", 4) == 0 || strncmp(vpt, "2.6.", 4) == 0) {
          plinintr_linuxversion=26;
     } else if (strncmp(vpt, "3.", 2) == 0) {
          plinintr_linuxversion=30;
     } else {
          elog_printf(ERROR, "unsupported linux kernel version");
     }

     /* free and return */
     nfree(data);
     return;
}

/*
 * Linux specific routines
 */
void plinintr_collect(TABLE tab) {
     char *data;
     ITREE *lines;

     /* open and process the interrupt file */
     data = probe_readfile("/proc/interrupts");
     if (data) {
	  /*elog_printf(DEBUG,"Read from /proc/interrupts: %s", data);*/
       util_scantext(data, ": +", UTIL_MULTISEP, &lines);	/* scan */
	  itree_first(lines);				/* count first line */
	  plinintr_ncpu = itree_n((ITREE *)itree_get(lines));
	  itree_traverse(lines) {
	       if (itree_isatstart(lines))
		    continue;
	       table_addemptyrow(tab);
	       plinintr_col_intr(tab, itree_get(lines) );
	  }

	  /* clear up */
	  util_scanfree(lines);
	  table_freeondestroy(tab, data);
     }
}

/* gets an I/O structure from /proc */
void plinintr_col_intr(TABLE tab, ITREE *idata) {
     /* /proc/interrupts in version 2.2 has a layout similar to this below:-
      *
      *  0:     371527   timer
      *  1:       5937   keyboard
      *  2:          0   cascade
      *  8:          2 + rtc
      * 12:       2522   PS/2 Mouse
      * 13:          1   math error
      * 14:       5773 + ide0
      * 15:          2   i82365
      *
      * The second column is the number of interrupts for a device,
      * the last column are the devices sharing that interrupt.
      * These are taken to fill the table plinintr_tab, columns name and hard.
      *
      * In version 2.4, 2.6 & 3.0, the interrupts are split into cpu's handling the 
      * interrupts, looking like this:-
      *
      *            CPU0       
      *   0:    2114914          XT-PIC  timer
      *   1:      40235          XT-PIC  keyboard
      *   2:          0          XT-PIC  cascade
      *  11:         10          XT-PIC  usb-uhci, i82365
      *  12:     590556          XT-PIC  PS/2 Mouse
      *  14:      53848          XT-PIC  ide0
      *  15:       1939          XT-PIC  ide1
      * NMI:          0 
      * ERR:          0
      *
      * The top column should be counted to see the number of cpus.
      * If a line has (ncpu+1) columns, then its format is (name, hard).
      * Otherwise, cols 2 to (ncpu+1) contain the data, the last col
      * (ncpu+3) contains the name.
      * Currently, we just count the interrupts from all the cpus together.
      */

     if (plinintr_linuxversion == 22) {
          if (itree_find(idata, 1) == ITREE_NOVAL)
	       return;
	  table_replacecurrentcell(tab, "hard", itree_get(idata));
	  itree_last(idata);
	  table_replacecurrentcell(tab, "name", itree_get(idata));
     } else if (plinintr_linuxversion == 24 || 
                plinintr_linuxversion == 26 ||
                plinintr_linuxversion == 30) {
          int i, val=0;

	  for (i=1; i <= plinintr_ncpu; i++) {
	       if (itree_find(idata, i) == ITREE_NOVAL)
		    return;
	       val += strtol(itree_get(idata), NULL, 10);
	  }

	  if (itree_n(idata) == plinintr_ncpu + 1)
	       /* no driver, use generic name */
	       itree_first(idata);
	  else
	       itree_last(idata);
	  table_replacecurrentcell(tab, "name", itree_get(idata));
	  table_replacecurrentcell_alloc(tab, "hard", util_u32toa(val));
     }
}


void plinintr_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     plinintr_init();
     plinintr_collect();
     if (argc > 1)
	  buf = table_outtable(plinintr_tab);
     else
	  buf = table_print(plinintr_tab);
     puts(buf);
     nfree(buf);
     table_destroy(plinintr_tab);
     exit(0);
}

#endif /* TEST */

#endif /* linux */
