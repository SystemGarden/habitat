/*
 * Linux uptime probe for habitat
 * Nigel Stuckey, May 2002
 *
 * Copyright System Garden Ltd 2002. All rights reserved.
 */

#if linux

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "probe.h"

/* Linux specific includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <utmp.h>

/* table constants for system probe */
struct probe_sampletab plinup_cols[] = {
  {"uptime",	"", "i32", "abs", "", "", "uptime in secs"},
  {"boot",	"", "i32", "abs", "", "", "time of boot in secs from epoch"},
  {"suspend",	"", "i32", "abs", "", "", "secs suspended"},
  {"vendor",	"", "str", "abs", "", "", "vendor name"},
  {"model",	"", "str", "abs", "", "", "model name"},
  {"nproc",	"", "i32", "abs", "", "", "number of processors"},
  {"mhz",	"", "i32", "abs", "", "", "processor clock speed"},
  {"cache",	"", "i32", "abs", "", "", "size of cache in kb"},
  {"fpu",	"", "str", "abs", "", "", "floating point unit available"},
  PROBE_ENDSAMPLE
};

struct probe_rowdiff plinup_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *plinup_getcols()    {return plinup_cols;}
struct probe_rowdiff   *plinup_getrowdiff() {return plinup_diffs;}
char                  **plinup_getpub()     {return NULL;}

/*
 * Initialise probe for linux names information
 */
void plinup_init() {
}

/*
 * Linux specific routines
 */
void plinup_collect(TABLE tab) {
     char * data;

     /* open and process the /proc/uptime file */
     data = probe_readfile("/proc/uptime");
     if (data) {
	  table_addemptyrow(tab);
	  plinup_col_uptime(tab, data);
	  table_freeondestroy(tab, data);
     } else {
	  elog_send(ERROR, "no data from uptime; no further sampling "
		    "will take place");
	  return;
     }

     /* open and process the /proc/cpuinfo file */
     data = probe_readfile("/proc/cpuinfo");
     if (data) {
	  plinup_col_cpuinfo(tab, data);
	  table_freeondestroy(tab, data);
     }
}


void plinup_fini()
{
}


/* interpret the data as an uptime format and place it into plinup_tab */
void  plinup_col_uptime(TABLE tab, char *data)
{
     /* uptime looks like this:-
      *
      * 22462.41 20636.43
      *
      * The first figure is the one we want: its the number of seconds 
      * the system has been up.
      */

     char *uptime;
     time_t now, down, boot;

     /* uptime, the number of seconds since boot */
     uptime = strtok(data, " ");
     table_replacecurrentcell(tab, "uptime", uptime);
     
     /* boottime from utmp; if utmp is unreadable, use now-uptime.
      * If the host uses power suspension, then (now-uptime) will not 
      * be boot time and uptime will be the time the host has been running.
      * No fix has been created for this.
      */
     boot=0;
     time(&now);		/* current time */
     if (access("/var/run/utmp", R_OK) == 0) {
	  /* utmp is readable under linux */
	  if (plinup_getutmpuptime(&down, &boot, "/var/run/utmp"))
	       /* successfully obtained, store the values */
	       table_replacecurrentcell_alloc(tab, "boot", util_i32toa(boot));
	  else
	       elog_send(WARNING, "Unable to read downtime from utmp");
     }

     if (boot == 0)
	  /* calculate (now-uptime) version of boot time */
	  table_replacecurrentcell_alloc(tab, "boot", 
					 util_i32toa(now-atoi(uptime)));

     table_replacecurrentcell_alloc(tab, "suspend", 
				    util_i32toa((now-atoi(uptime))-boot));
}


/* interpret the data as a cpuinfo format and place the data into the
 * current row of  plinup_tab */
void  plinup_col_cpuinfo(TABLE tab, char *data)
{
     /* /proc/cpuinfo looks like this:-
      *
      * processor	\t: 0
      * vendor_id	\t: GenuineIntel
      * cpu family	\t: 6
      * model		\t: 5
      * model name	\t: Pentium II (Deschutes)
      * stepping	\t: 2
      * cpu MHz		\t: 299.946
      * cache size	\t: 512 KB
      * fdiv_bug	\t: no
      * hlt_bug		\t: no
      * f00f_bug	\t: no
      * coma_bug	\t: no
      * fpu		\t: yes
      * fpu_exception	\t: yes
      * cpuid level	\t: 2
      * wp		\t: yes
      * flags		\t: fpu vme de pse tsc msr pae mce cx8 sep [... etc]
      * bogomips	\t: 598.01
      *
      * we want vendor_id, model name, cpu MHz, cache size and fpu.
      * Only one line is produced regardless of the number of processors.
      * The details of the last processor in the /proc/cpuinfo list will
      * be used.
      */

     char *linecheck, *name;
     int nproc=0;

     /* scan the data into a table */
     

     /* read data one line at a time and recognise the values we want
      * so that the lines can be in any order */
     linecheck = data;
     while (linecheck != NULL) {
	  linecheck = strtok(linecheck, "\t:");		/* read name */
	  if (linecheck == NULL)
	       break;
	  else if (strcmp(linecheck, "vendor_id") == 0)
	       name = "vendor";
	  else if (strcmp(linecheck, "model name") == 0)
	       name = "model";
	  else if (strcmp(linecheck, "cpu MHz") == 0)
	       name = "mhz";
	  else if (strcmp(linecheck, "cache size") == 0)
	       name = "cache";
	  else if (strcmp(linecheck, "fpu") == 0)
	       name = "fpu";
	  else {
	       /* not needed, flush this line and position at next.
		* but before that, check the processor line to count 
		* the number seen */
	       if (strcmp(linecheck, "processor") == 0)
		    nproc++;
	       linecheck = strtok(NULL, "\n");		/* read value */
	       if (linecheck == NULL)
		    break;
	       linecheck += strlen(linecheck)+1;	/* start next line */
	       continue;
	  }

	  /* read value into table */
	  linecheck = strtok(NULL, "\n");		/* read value */
	  while (!isalnum(*linecheck))			/* strip leading */
	       linecheck++;

	  /* save line to table */
	  table_replacecurrentcell(tab, name, linecheck);
	  linecheck += strlen(linecheck)+1;		/* start next line */
     }

     /* save number of processors */
     table_replacecurrentcell_alloc(tab, "nproc", util_i32toa(nproc));
}



/* Extract the last down time and the current boot time from a 
 * utmp format file.
 * Returns 1 for success or 0 for failure */
int plinup_getutmpuptime(time_t *down, time_t *boot, char *filename)
{
     struct stat utmp_stat;
     struct utmp *ut;
     int fd;
     void *map;

     /*initialise */
     *down = *boot = 0;

     /* open, size and map utmp file */
     fd = open(filename, O_RDONLY);
     if (fd == 0) {
	  elog_printf(WARNING, "unable to open file %s, no downtime", 
		      filename);
	  return 0;	/* fail */
     }
     fstat(fd, &utmp_stat);
     map = mmap(0, utmp_stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
     if (map == MAP_FAILED) {
	  elog_printf(WARNING, "unable to map file %s, no downtime", 
		      filename);
	  close(fd);
	  return 0;
     }

     /* walk structure and find the user records we want */
     for (ut = map; (char *)ut < ((char *)map) + utmp_stat.st_size; ut++) {
	  if (ut->ut_type == 2) {
	       /*printf("utmp boot time %d\n", ut->ut_tv.tv_sec);*/
	       *boot = ut->ut_tv.tv_sec;
	       break;
	  }
     }

     /* release and close file */
     munmap(map, utmp_stat.st_size);
     close(fd);

     return 1;		/* success */
}


void plinup_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     plinup_init();
     plinup_collect();
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

#endif /* linux */
