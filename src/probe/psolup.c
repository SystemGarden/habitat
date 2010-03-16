/*
 * Solaris uptime probe for habitat
 * Nigel Stuckey, May 2003
 *
 * Copyright System Garden Ltd 2003. All rights reserved.
 */

#if __svr4__

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include <kstat.h>		/* Solaris uses kstat */
#include <sys/kstat.h>		/* kstat_t structure */
#include <sys/time.h>		/* definition of hrtime_t */
#include <sys/cpuvar.h>		/* cpu details */
#include <utmpx.h>		/* uptime include */
#include <sys/systeminfo.h>	/* standard SYSV sysinfo */
#include "probe.h"

/* table constants for system probe */
struct probe_sampletab psolup_cols[] = {
  {"uptime",	"", "i32",  "abs", "", "", "uptime in secs"},
  {"boot",	"", "i32",  "abs", "", "", "time of boot in secs from epoch"},
  {"suspend",	"", "i32",  "abs", "", "", "secs suspended"},
  {"vendor",	"", "str",  "abs", "", "", "vendor name"},
  {"model",	"", "str",  "abs", "", "", "model name"},
  {"nproc",	"", "i32",  "abs", "", "", "number of processors"},
  {"mhz",	"", "i32",  "abs", "", "", "processor clock speed"},
  {"cache",	"", "i32",  "abs", "", "", "size of cache in kb"},
  {"fpu",	"", "str",  "abs", "", "", "floating point unit available"},
  PROBE_ENDSAMPLE
};

struct probe_rowdiff psolup_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *psolup_getcols()    {return psolup_cols;}
struct probe_rowdiff   *psolup_getrowdiff() {return psolup_diffs;}
char                  **psolup_getpub()     {return NULL;}

/*
 * Initialise probe for solaris names information
 */
void psolup_init() {
}

/*
 * Solaris specific routines
 */
void psolup_collect(TABLE tab) {
     table_addemptyrow(tab);
     psolup_col_utmpx(tab);
     psolup_col_procinfo(tab);
     psolup_col_vendor(tab);
}


/* Collect uptime from /var/adm/utmpx */
void psolup_col_utmpx(TABLE tab) {
     struct utmpx *ut, getut;

     /* fetch the uptime */
     setutxent();
     getut.ut_type = BOOT_TIME;
     ut = getutxid(&getut);

     if (ut == NULL) {
	  /* solaris has problems */
	  endutxent();
	  return;
     }

     /* load into table */
     table_replacecurrentcell_alloc(tab, "boot",    
				    util_i32toa(ut->ut_tv.tv_sec));
     table_replacecurrentcell_alloc(tab, "uptime",  
				    util_i32toa(time(NULL)-ut->ut_tv.tv_sec));
     table_replacecurrentcell_alloc(tab, "suspend", "0");

     endutxent();
}


/* Collect processor information */
void psolup_col_procinfo(TABLE tab) {
     kstat_ctl_t *kc;
     kstat_t *ksp;
     kstat_named_t *s;

     /* get cpu information from the kstat structures */
     kc = kstat_open();

     /* get the number of cpus */
     ksp = kstat_lookup(kc, "unix", 0, "system_misc"); 
     if (! ksp ) {
	  elog_send(ERROR, "null kstat data for (unix,0,system_misc)");
	  return;
     }
     kstat_read(kc, ksp, NULL);
     s = kstat_data_lookup(ksp, "ncpus");
     if (! s) {
	  elog_send(ERROR, "null kstat data for ncpus");
	  return;
     }
     table_replacecurrentcell_alloc(tab, "nproc", util_i32toa(s->value.i32));

     /* look for the first cpu as a representative of any others 
      * that may be present. Really, you should have identical clock 
      * speeds in a multiprocessor system anyway */
     ksp = kstat_lookup(kc, "cpu_info", -1, NULL);
     if (! ksp ) {
	  elog_send(ERROR, "null kstat data for (cpu_info)");
	  return;
     }
     kstat_read(kc, ksp, NULL);
     s = kstat_data_lookup(ksp, "clock_MHz");
     if (! s) {
	  elog_send(ERROR, "null kstat data for clock_MHz");
	  return;
     }
     table_replacecurrentcell_alloc(tab, "mhz", util_i32toa(s->value.i32));
     s = kstat_data_lookup(ksp, "fpu_type");
     if (! s) {
	  elog_send(ERROR, "null kstat data for fpu_type");
	  return;
     }
     table_replacecurrentcell_alloc(tab, "fpu", s->value.c);

     kstat_close(kc);


     /* unreported values */
     /*table_replacecurrentcell_alloc(tab, "cache", util_i32toa(0));*/

}


/* Collect vendor and model information */
void psolup_col_vendor(TABLE tab) {
     char buf[50];

     if (sysinfo(SI_HW_PROVIDER, buf, 50) > 50) {
	  elog_send(ERROR, "provider string too long");
     } else {
	  table_replacecurrentcell_alloc(tab, "vendor", buf);
     }

     if (sysinfo(SI_PLATFORM, buf, 50) > 50) {
	  elog_send(ERROR, "platform string too long");
     } else {
	  table_replacecurrentcell_alloc(tab, "model", buf);
     }
}

void psolup_fini()
{
}


void psolup_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     psolup_init();
     psolup_collect();
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
