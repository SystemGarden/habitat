/*
 * Solaris names probe for iiab
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
struct probe_sampletab psolnames_cols[] = {
     {"name",	"", "str",  "abs", "", "", "name"},
     {"vname",	"", "str",  "abs", "", "", "value name"},
     {"value",  "", "str",  "abs", "", "", "value"},
     PROBE_ENDSAMPLE
};

struct probe_rowdiff psolnames_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *psolnames_getcols()    {return psolnames_cols;}
struct probe_rowdiff   *psolnames_getrowdiff() {return psolnames_diffs;}
char                  **psolnames_getpub()     {return NULL;}

/*
 * Initialise probe for solaris names information
 */
void psolnames_init() {
}

/*
 * Solaris specific routines
 */
void psolnames_collect(TABLE tab) {
     kstat_ctl_t *kc;
     kstat_t *ksp;

     /* process kstat data of type KSTAT_TYPE_RAW */
     kc = kstat_open();

     for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
	  if (ksp->ks_type == KSTAT_TYPE_NAMED) {
	       /* collect row stats */
	       psolnames_col_names(tab, kc, ksp);
	  }

     kstat_close(kc);
}

/* gets an I/O structure out of the kstat block */
void psolnames_col_names(TABLE tab, kstat_ctl_t *kc, kstat_t *ksp) {
     kstat_named_t *s;
     char *str, *value=NULL;
     int i;

     /* read from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* update global structures */
     for (i=0; i<ksp->ks_ndata; i++) {
	  /* add new row to table */
	  table_addemptyrow(tab);
	  str = util_strjoin(ksp->ks_module, 
			     ",",
			     util_i32toa(ksp->ks_instance),
			     ",",
			     ksp->ks_name,
			     NULL);
	  table_freeondestroy(tab, str);
	  table_replacecurrentcell(tab, "name",  str);
	  table_replacecurrentcell_alloc(tab, "vname", s[i].name);

	  switch (s[i].data_type) {
	  case KSTAT_DATA_CHAR:
	       value = s[i].value.c;
	       break;
	  case KSTAT_DATA_INT32:
	       value = util_i32toa(s[i].value.i32);
	       break;
	  case KSTAT_DATA_UINT32:
	       value = util_u32toa(s[i].value.ui32);
	       break;
	  case KSTAT_DATA_INT64:
	       value = util_i64toa(s[i].value.i64);
	       break;
	  case KSTAT_DATA_UINT64:
	       value = util_i64toa(s[i].value.ui64);
	       break;
	  default:
	       break;
	  }
	  table_replacecurrentcell_alloc(tab, "value",   value);
     }
}

void psolnames_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     psolnames_init();
     psolnames_collect();
     if (argc > 1)
	  buf = table_outtable(tab);
     else
	  buf = table_print(tab);
     puts(buf);
     nfree(buf);
     table_destroy(tab);
     exit(0);
}

#endif /* TEST */

#endif /* __svr4__ */
