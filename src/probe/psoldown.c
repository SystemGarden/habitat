/*
 * Solaris downtime probe for habitat
 *
 * Deviates from probe behaviour by having two arguments: the first is
 * a p-url for the alive timestamp and the second arg is the p-url to
 * the location of the boot timestamp.
 *
 * The alive timestamp is maintained by someone else (uptime probe)
 * and down probe can not work without that probe. 
 * For down time to be recorded, the down probe must run before the
 * alive probe. This is normally done by running the down probe at start
 * up of clockwork, and uptime is run after 60 seconds (say).
 * This down time probe maintains the boot timstamp.
 * If the alive datum does not exist, the a down record is not generated.
 * Output is only produced if down time has occured, otherwise there will 
 * be no output.
 *
 * Nigel Stuckey, June 2003
 *
 * Copyright System Garden Ltd 2003. All rights reserved.
 */

#if __svr4__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmpx.h>		/* uptime include */
#include "probe.h"
/* Linux specific includes */

char *psoldown_purl_boot=NULL;
char *psoldown_purl_alive=NULL;
char *psoldown_usage = "down <boot> <alive>\n"
  "where <boot>  Route p-url to boot information, created by this probe\n"
  "      <alive> Route p-url to uptime, created by the 'up' probe\n"
  "The 'up' probe needs to create the uptime information before this\n"
  "'down' probe can run";

/* table constants for system probe */
struct probe_sampletab psoldown_cols[] = {
     {"lastup",	  "", "i32", "abs", "", "", "time last alive in secs from "
                                            "epoch"},
     {"boot",     "", "i32", "abs", "", "", "time of boot in secs from epoch"},
     {"downtime", "", "i32", "abs", "", "", "secs unavailable"},
     PROBE_ENDSAMPLE
};

struct probe_rowdiff psoldown_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *psoldown_getcols()    {return psoldown_cols;}
struct probe_rowdiff   *psoldown_getrowdiff() {return psoldown_diffs;}
char                  **psoldown_getpub()     {return NULL;}

/*
 * Initialise probe for linux names information
 */
void psoldown_init(char *probeargs) {
     char *boot, *alive;

     if (probeargs == NULL || *probeargs == '\0')
	  return;
     boot = strtok(probeargs, " ");
     if (boot == NULL) {
	  elog_printf(ERROR, "boot p-url not given, unable to "
		      "initialise 'down' probe\nusage %s", psoldown_usage);
	  return;
     }

     alive = strtok(NULL, " ");
     if (alive == NULL) {
	  elog_printf(ERROR, "alive p-url not given, unable to "
		      "initialise 'down' probe\nusage: %s", psoldown_usage);
	  return;
     }

     psoldown_purl_boot  = xnstrdup(boot);
     psoldown_purl_alive = xnstrdup(alive);
}

/*
 * Linux specific routines
 */
void psoldown_collect(TABLE tab) {
     char *bootstr, *alivestr;
     int len, boot, alive;

     if (psoldown_purl_boot == NULL || psoldown_purl_alive == NULL) {
	  elog_printf(ERROR, "probe was not initialised properly with "
		      "p-urls for boot and alive\nusage: %s", psoldown_usage);
	  return;
     }

     /* read boot and alive values from holstore */
     bootstr = route_read(psoldown_purl_boot, NULL, &len);
     if (bootstr == NULL) {
	  /* No boot timestamp, which we have a responsibility to
	   * maintain. Stamp it now */
	  boot = psoldown_stampboot(psoldown_purl_boot);
	  elog_printf(DIAG, "No 'boot' timestamp at %s: "
		      "stamping now boot=%d", psoldown_purl_boot, boot);
     } else {
          boot = strtol(bootstr, NULL, 10);
     }

     alivestr = route_read(psoldown_purl_alive, NULL, &len);
     if (alivestr == NULL) {
	  /* No alive timestamp, which we rely on others to maintain
	   * in order to calculate an accurate down time.
	   * Give the current time. */
	  alive = psoldown_stampalive(psoldown_purl_alive);
	  elog_printf(DIAG, "No 'last alive' timestamp at %s: "
		      "stamping now alive=%d", psoldown_purl_alive, alive);
     } else {
          alive = strtol(alivestr, NULL, 10);
     }

     /* do we have work to do? */
     if (boot > alive) {
	  /* calculate time spent down and log it.
	   * use the ints to get over any termination problems */
	  table_addemptyrow(tab);
	  table_replacecurrentcell_alloc(tab, "lastup", util_i32toa(alive));
	  table_replacecurrentcell_alloc(tab, "boot", util_i32toa(boot));
	  table_replacecurrentcell_alloc(tab, "downtime", 
					 util_i32toa(boot-alive));

	  /* update boot timestamp in its holstore */
	  elog_printf(DIAG, "New boot detected: stamping boot and "
		      "alive now, down %d secs", boot-alive);
	  psoldown_stampboot (psoldown_purl_boot);
	  psoldown_stampalive(psoldown_purl_alive);
     }

     /* free up working space */
     if (bootstr)
	  nfree(bootstr);
     nfree(alivestr);
}


void psoldown_fini()
{
     if (psoldown_purl_boot)
	  nfree(psoldown_purl_boot);
     if (psoldown_purl_alive)
	  nfree(psoldown_purl_alive);
}



/*
 * Create or update the boot time stamp
 * returns boot time for success or 0 for failure
 */
int psoldown_stampboot(char *boot_purl)
{
     time_t down, boot;
     int r;
     ROUTE output;

     if (boot_purl == NULL || *boot_purl == '\0')
	  return 0;	/* failure */

     if (psoldown_getutmpxuptime(&down, &boot)) {
	  /* write epoch time to route */
	  output = route_open(boot_purl, "boot time stamp", NULL, 1);
	  if (output == NULL)
	       return 0;	/* failure */
	  r = route_printf(output, "%d ", boot);
	  route_close(output);
	  if (r <= 0)
	       return 0;	/* failure */
     } else {
	  return 0;		/* failure */
     }

     return boot;		/* success */
}


/*
 * Create or update the alive time stamp
 * returns alive time for success or 0 for failure
 */
int psoldown_stampalive(char *alive_purl)
{
     time_t alive;
     int r;
     ROUTE output;

     if (alive_purl == NULL || *alive_purl == '\0')
	  return 0;	/* failure */

     /* write epoch time to route */
     time(&alive);
     output = route_open(alive_purl, "alive time stamp", NULL, 1);
     if (output == NULL)
	  return 0;	/* failure */
     r = route_printf(output, "%d ", alive);
     route_close(output);
     if (r <= 0)
	  return 0;	/* failure */

     return alive;		/* success */
}


/* Extract the last down time and the current boot time from a 
 * utmp format file.
 * Returns 1 for success or 0 for failure */
int psoldown_getutmpxuptime(time_t *down, time_t *boot)
{
     struct utmpx *ut, getut;

     /*initialise */
     *down = *boot = 0;

     /* fetch the uptime */
     setutxent();
     getut.ut_type = BOOT_TIME;
     ut = getutxid(&getut);

     if (ut == NULL) {
	  /* solaris has problems */
	  endutxent();
	  return 0;
     }

     *boot = ut->ut_tv.tv_sec;

     return 1;		/* success */
}


void psoldown_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     psoldown_init();
     psoldown_collect();
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
