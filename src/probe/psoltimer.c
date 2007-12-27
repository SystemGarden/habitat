/*
 * Solaris timer probe for iiab
 * Nigel Stuckey, December 1998, January & July 1999
 *
 * Copyright System Garden Ltd 1999-2001. All rights reserved.
 */

#if __svr4__

#include <stdio.h>
#include "probe.h"

/* Solaris specific routines */
#include <kstat.h>		/* Solaris uses kstat */

/* table constants for system probe */
struct probe_sampletab psoltimer_cols[] = {
     {"kname",	    "str",  "abs", "", "",  "timer name"},
     {"name",	    "str",  "abs", "", "",  "event name"},
     {"nevents",    "u64",  "cnt", "", "",  "number of events"},
     {"elapsed_t",  "nano", "cnt", "", "",  "cumulative elapsed time"},
     {"min_t",      "nano", "cnt", "", "",  "shortest event duration"},
     {"max_t",      "nano", "cnt", "", "",  "longest event duration"},
     {"start_t",    "nano", "cnt", "", "",  "previous event start time"},
     {"stop_t",     "nano", "cnt", "", "",  "previous event stop time"},
     PROBE_ENDSAMPLE
};

/* List of colums to diff */
struct probe_rowdiff psoltimer_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *psoltimer_getcols()    {return psoltimer_cols;}
struct probe_rowdiff   *psoltimer_getrowdiff() {return psoltimer_diffs;}
char                  **psoltimer_getpub()     {return NULL;}

/*
 * Initialise probe for solaris timer information
 */
void psoltimer_init() {
}

/* destroy any structures that may be open following a run of sampling */
void psoltimer_fini() {
}

/*
 * Solaris specific routines
 */
void psoltimer_collect(TABLE tab) {
     kstat_ctl_t *kc;
     kstat_t *ksp;

     /* process kstat data of type KSTAT_TYPE_RAW */
     kc = kstat_open();

     for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
	  if (ksp->ks_type == KSTAT_TYPE_TIMER) {
	       /* add new row to table */
	       table_addemptyrow(tab);
	       /* collect row stats */
	       psoltimer_col_timer(tab, kc, ksp);
	  }

     kstat_close(kc);
}

/* gets an I/O structure out of the kstat block */
void psoltimer_col_timer(TABLE tab, kstat_ctl_t *kc, kstat_t *ksp) {
     kstat_timer_t *s;

     /* read from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* update global structures */
     table_replacecurrentcell_alloc(tab, "kname",     ksp->ks_name);
     table_replacecurrentcell_alloc(tab, "name",      s->name);
     table_replacecurrentcell_alloc(tab, "nevents",   util_hrttoa(s->num_events));
     table_replacecurrentcell_alloc(tab, "elapsed_t", util_hrttoa(s->elapsed_time));
     table_replacecurrentcell_alloc(tab, "min_t",     util_hrttoa(s->min_time));
     table_replacecurrentcell_alloc(tab, "max_t",     util_hrttoa(s->max_time));
     table_replacecurrentcell_alloc(tab, "start_t",   util_hrttoa(s->start_time));
     table_replacecurrentcell_alloc(tab, "stop_t",    util_hrttoa(s->stop_time));
}


void psoltimer_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     psoltimer_init();
     psoltimer_collect();
     if (argc > 1)
	  buf = table_outtable(tab);
     else
	  buf = table_print(tab);
     puts(buf);
     nfree(buf);
     table_deepdestroy(tab);
     exit(0);
}

#endif /* TEST */

#endif /* __svr4__ */
