/*
 * Solaris interrupt probe for iiab
 * Nigel Stuckey, December 1998, January, July 1999 & March 2000
 *
 * Copyright System Garden Ltd 1998-2001. All rights reserved.
 */

#if __svr4__

#include <stdio.h>
#include "probe.h"

/* Solaris specific routines */
#include <kstat.h>		/* Solaris uses kstat */

/* table constants for system probe */
struct probe_sampletab psolintr_cols[] = {
     {"name",	  "",  "str", "cnt", "", "", "device name"},
     {"hard",	  "",  "u32", "cnt", "", "", "interrupt from hardware "
      					     "device"},
     {"soft",	  "",  "u32", "cnt", "", "", "interrupt self induced by "
					     "system"},
     {"watchdog", "",  "u32", "cnt", "", "", "interrupt from periodic "
					     "timer"},
     {"spurious", "",  "u32", "cnt", "", "", "interrupt for unknown reason"},
     {"multisvc", "",  "u32", "cnt", "", "", "multiple servicing during "
					     "single interrupt "},
     PROBE_ENDSAMPLE
};

/* List of colums to diff */
struct probe_rowdiff psolintr_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *psolintr_getcols()    {return psolintr_cols;}
struct probe_rowdiff   *psolintr_getrowdiff() {return psolintr_diffs;}
char                  **psolintr_getpub()     {return NULL;}

/* taken from kstat.h...*/
/*
 * Interrupt statistics.
 *
 * An interrupt is a hard interrupt (sourced from the hardware device
 * itself), a soft interrupt (induced by the system via the use of
 * some system interrupt source), a watchdog interrupt (induced by
 * a periodic timer call), spurious (an interrupt entry point was
 * entered but there was no interrupt condition to service),
 * or multiple service (an interrupt condition was detected and
 * serviced just prior to returning from any of the other types).
 *
 * Measurement of the spurious class of interrupts is useful for
 * autovectored devices in order to pinpoint any interrupt latency
 * problems in a particular system configuration.
 *
 * Devices that have more than one interrupt of the same
 * type should use multiple structures.
 */


/*
 * Initialise probe for solaris interrupt information
 */
void psolintr_init() {
}

/*
 * Solaris specific routines
 */
void psolintr_collect(TABLE tab) {
     kstat_ctl_t *kc;
     kstat_t *ksp;

     /* process kstat data of type KSTAT_TYPE_RAW */
     kc = kstat_open();

     for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
	  if (ksp->ks_type == KSTAT_TYPE_INTR) {
	       /* add new row to table */
	       table_addemptyrow(tab);
	       /* collect row stats */
	       psolintr_col_intr(tab, kc, ksp);
	  }

     kstat_close(kc);
}

/* gets an I/O structure out of the kstat block */
void psolintr_col_intr(TABLE tab, kstat_ctl_t *kc, kstat_t *ksp) {
     kstat_intr_t *s;

     /* read from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* update global structures */
     table_replacecurrentcell_alloc(tab, "name",     ksp->ks_name);
     table_replacecurrentcell_alloc(tab, "hard",     
			      util_u32toa(s->intrs[KSTAT_INTR_HARD]));
     table_replacecurrentcell_alloc(tab, "soft",     
			      util_u32toa(s->intrs[KSTAT_INTR_SOFT]));
     table_replacecurrentcell_alloc(tab, "watchdog", 
			      util_u32toa(s->intrs[KSTAT_INTR_WATCHDOG]));
     table_replacecurrentcell_alloc(tab, "spurious", 
			      util_u32toa(s->intrs[KSTAT_INTR_SPURIOUS]));
     table_replacecurrentcell_alloc(tab, "multisvc", 
			      util_u32toa(s->intrs[KSTAT_INTR_MULTSVC]));
}

void psolintr_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     psolintr_init();
     psolintr_collect();
     if (argc > 1)
	  buf = table_outtable(psolintr_tab);
     else
	  buf = table_print(psolintr_tab);
     puts(buf);
     nfree(buf);
     table_deepdestroy(psolintr_tab);
     exit(0);
}

#endif /* TEST */

#endif /* __svr4__ */
