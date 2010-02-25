/*
 * Mac OS X process probe for iiab
 * Nigel Stuckey, March 2010
 *
 * Copyright System Garden Ltd 2010. All rights reserved.
 */

#if MACOSX

#include <stdio.h>
#include "probe.h"

/* Linux specific includes */
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int   pmacsys_hz;

/* table constants for system probe */
struct probe_sampletab pmacsys_cols[] = {
     /* /proc/loadavg */
  {"load1",	"",	"nano", "abs", "4", "", "1 minute load average"},
  {"load5",	"",	"nano", "abs", "4", "", "5 minute load average"},
  {"load15",	"",	"nano", "abs", "4", "", "15 minute load average"},
  {"runque",	"",	"u32",  "abs", "", "", "num runnable procs"},
  {"nprocs",	"",	"u32",  "abs", "", "", "num of procs"},
  {"lastproc",	"",	"u32",  "abs", "", "", "last proc run"},
     /* /proc/meminfo */
  {"mem_tot",	"",	"u32",  "abs", "", "", "total memory (kB)"},
  {"mem_used",	"",	"u32",  "abs", "", "", "memory used (kB)"},
  {"mem_free",	"",	"u32",  "abs", "", "", "memory free (kB)"},
  {"mem_shared","",	"u32",  "abs", "", "", "used memory shared (kB)"},
  {"mem_buf",	"",	"u32",  "abs", "", "", "buffer memory (kB)"},
  {"mem_cache",	"",	"u32",  "abs", "", "", "cache memory (kB)"},
  {"swap_tot",	"",	"u32",  "abs", "", "", "total swap space (kB)"},
  {"swap_used",	"",	"u32",  "abs", "", "", "swap space used (kB)"},
  {"swap_free",	"",	"u32",  "abs", "", "", "swap space free (kB)"},
  /* /proc/stat */
  {"cpu_tick_user", "",	"u64",  "cnt", "", "", "accumulated ticks cpu spent "
                                               "in user space"},
  {"cpu_tick_nice", "",	"u64",  "cnt", "", "", "accumulated ticks cpu spent at"
					       " nice priority in user space"},
  {"cpu_tick_system","","u64",  "cnt", "", "", "accumulated ticks cpu spent "
                                               "in kernel"},
  {"cpu_tick_idle","",	"u64",  "cnt", "", "", "accumulated ticks cpu was "
                                               "idle"},
  {"cpu_tick_wait","",	"u64",  "cnt", "", "", "accumulated ticks cpu was "
                                               "idle but waiting for I/O"},
  {"cpu_tick_irq","",	"u64",  "cnt", "", "", "accumulated ticks cpu handles "
                                               "hardware interrupts"},
  {"cpu_tick_softirq","","u64", "cnt", "", "", "accumulated ticks cpu handles "
                                               "soft interrupts"},
  {"cpu_tick_steal","",	"u64",  "cnt", "", "", "accumulated ticks cpu was "
                                               "stolen by other virtual "
                                               "machines"},
  {"cpu_tick_guest","",	"u64",  "cnt", "", "", "accumulated ticks cpu was "
                                               "hosting a guest cpu under our "
                                               "control"},
  {"vm_pgpgin",	"",	"u32",  "cnt", "", "", "npages paged in"},
  {"vm_pgpgout","",	"u32",  "cnt", "", "", "npages paged out"},
  {"vm_pgswpin","",	"u32",  "cnt", "", "", "npages swapped in"},
  {"vm_pgswpout",""	"u32",  "cnt", "", "", "npages swapped out"},
  {"nintr",	"",	"u32",  "cnt", "", "", "total number of interrupts"},
  {"ncontext",	"",	"u32",  "cnt", "", "", "number of context switches"},
  {"nforks",	"",	"u32",  "cnt", "", "", "number of forks"},
  /* /proc/uptime */
  {"uptime",	"",	"nano", "abs", "", "", "secs system has been up"},
  {"idletime",	"",	"nano", "abs", "", "", "secs system has been idle"},
  /* calculated */
  {"pc_user",	"%user","nano", "abs", "100","","% time cpu was in user "
                                                "space"},
  {"pc_nice",	"%nice","nano", "abs", "100","","% time cpu was at nice "
                                                "priority in user space"},
  {"pc_system",	"%system","nano","abs","100","","% time cpu spent in kernel"},
  {"pc_idle",	"%idle","nano", "abs", "100","","% time cpu was idle"},
  {"pc_wait",	"%wait","nano", "abs", "100","","% time cpu was idle waiting "
                                               "for I/O"},
  {"pc_irq",	"%irq", "nano", "abs", "100","","% time cpu was handling hard "
                                               "interrupts"},
  {"pc_softirq","%softirq","nano","abs","100","","% time cpu was handling soft"
                                               " soft interrupts"},
  {"pc_steal",	"%steal","nano","abs", "100","","% time cpu was stolen to run "
                                               "peer VMs"},
  {"pc_guest",	"%guest","nano","abs", "100","","% time cpu was running "
                                               "guest CPUs under our control"},
  {"pc_work",	"%work","nano", "abs", "100","","% time cpu was working "
					       "(excludes %idle+%wait)"},
  {"pagein",	"",	"i32",  "abs", "", "", "pages paged in per second"},
  {"pageout",	"",	"i32",  "abs", "", "", "pages paged out per second"},
  {"swapin",	"",	"i32",  "abs", "", "", "pages swapped in per second"},
  {"swapout",	"",	"i32",  "abs", "", "", "pages swapped out per second"},
  {"interrupts","",	"u32",  "abs", "", "", "hardware interrupts per second"},
  {"contextsw",	"",	"u32",  "abs", "", "", "context switches per second"},
  {"forks",	"",	"u32",  "abs", "", "", "process forks per second"},
  PROBE_ENDSAMPLE
};

/* List of colums to diff */
struct probe_rowdiff pmacsys_diffs[] = {
     {"vm_pgpgin",  "pagein"},
     {"vm_pgpgout", "pageout"},
     {"vm_pgswpin", "swapin"},
     {"vm_pgswpout","swapout"},
     {"nintr",      "interrupts"},
     {"ncontext",   "contextsw"},
     {"nforks",     "forks"},
     PROBE_ENDROWDIFF
};

/* List of columns to publish */
char *pmacsys_pub[] = {
     "load1",
     "load5",
     "load15",
     "runque",
     "nprocs",
     "lastproc",
     "mem_tot",
     "mem_used",
     "mem_free",
     "mem_shared",
     "mem_buf",
     "mem_cache",
     "swap_tot",
     "swap_used",
     "swap_free",
     "uptime",
     "idletime",
     "pc_user",
     "pc_nice",
     "pc_system",
     "pc_idle",
     "pc_wait",
     "pc_irq",
     "pc_softirq",
     "pc_steal",
     "pc_guest",
     "pc_work",
     "pagein",
     "pageout",
     "swapin",
     "swapout",
     "interrupts",
     "contextsw",
     "forks",
     NULL
};

/* static data return methods */
struct probe_sampletab *pmacsys_getcols()    {return pmacsys_cols;}
struct probe_rowdiff   *pmacsys_getrowdiff() {return pmacsys_diffs;}
char                  **pmacsys_getpub()     {return pmacsys_pub;}

/* Linux version; assume 2.4 being the latest */
int pmacsys_linuxversion=24;

/*
 * Initialise probe for linux system information
 */
void pmacsys_init() {
     char *data, *vpt;
     long ticks;

     /* we need to work out which version of linux we are running */
     data = probe_readfile("/proc/version");
     if (!data) {
          elog_printf(ERROR, "unable to find the linux kernel version file");
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
          pmacsys_linuxversion=22;
     } else if (strncmp(vpt, "2.3.", 4) == 0 || strncmp(vpt, "2.4.", 4) == 0) {
          pmacsys_linuxversion=24;
     } else if (strncmp(vpt, "2.5.", 4) == 0 || strncmp(vpt, "2.6.", 4) == 0) {
          pmacsys_linuxversion=26;
     } else {
          elog_printf(ERROR, "unsupported linux kernel version");
     }

     /* get the tick frequency */
     ticks = sysconf(_SC_CLK_TCK);
     if ( ticks != -1 )
          pmacsys_hz = ticks;
     else {
	  elog_send(DEBUG, "HZ environment variable missing, assuming 100");
	  pmacsys_hz = 100;
     }
}



/*
 * Linux specific routines
 */
void pmacsys_collect(TABLE tab) {
     if (pmacsys_linuxversion == 22)
          pmacsys_collect22(tab);
     else if (pmacsys_linuxversion == 24)
          pmacsys_collect22(tab);
     else if (pmacsys_linuxversion == 26)
          pmacsys_collect26(tab);
}


void pmacsys_collect22(TABLE tab) {
     char *data;

     /* open and process the loadavg file */
     data = probe_readfile("/proc/loadavg");
     if (data) {
	  table_addemptyrow(tab);
	  pmacsys_col_loadavg(tab, data);
	  table_freeondestroy(tab, data);
     } else {
	  elog_send(ERROR, "no data from loadavg; no further "
		    "sampling will take place");
	  return;
     }

     /* open and process the meminfo file */
     data = probe_readfile("/proc/meminfo");
     if (data) {
	  pmacsys_col_meminfo(tab, data);
	  table_freeondestroy(tab, data);
     }

     /* open and process the stat file */
     data = probe_readfile("/proc/stat");
     if (data) {
	  pmacsys_col_stat(tab, data);
	  table_freeondestroy(tab, data);
     }

     /* open and process the uptime file */
     data = probe_readfile("/proc/uptime");
     if (data) {
	  pmacsys_col_uptime(tab, data);
	  table_freeondestroy(tab, data);
     }
}


void pmacsys_collect26(TABLE tab) {
     char *data;
     ITREE *lines;

     /* open and process the loadavg file */
     data = probe_readfile("/proc/loadavg");
     if (data) {
	  table_addemptyrow(tab);
	  pmacsys_col_loadavg(tab, data);
	  table_freeondestroy(tab, data);
     } else {
	  elog_send(ERROR, "no data from loadavg; no further "
		    "sampling will take place");
	  return;
     }

     /* open and process the meminfo file */
     data = probe_readfile("/proc/meminfo");
     if (data) {
          util_scantext(data, " :\t", UTIL_MULTISEP, &lines);
	  pmacsys_col_meminfo26(tab, lines);
	  util_scanfree(lines);
	  table_freeondestroy(tab, data);
     }

     /* open and process the stat file */
     data = probe_readfile("/proc/stat");
     if (data) {
          util_scantext(data, " ", UTIL_MULTISEP, &lines);
	  pmacsys_col_stat26(tab, lines);
	  util_scanfree(lines);
	  table_freeondestroy(tab, data);
     }

     /* open and process the uptime file */
     data = probe_readfile("/proc/uptime");
     if (data) {
          pmacsys_col_uptime(tab, data);
	  table_freeondestroy(tab, data);
     }

     /* open and process the vmstat file */
     data = probe_readfile("/proc/vmstat");
     if (data) {
          util_scantext(data, " ", UTIL_MULTISEP, &lines);
          pmacsys_col_vmstat(tab, lines);
	  util_scanfree(lines);
	  table_freeondestroy(tab, data);
     }
}


/* shut down probe */
void pmacsys_fini() {
}


/* interpret the data as a loadavg format and place it into pmacsys_tab */
void  pmacsys_col_loadavg(TABLE tab, char *data)
{
     /* load average looks like this:-
      *
      * 0.00 0.04 0.02 2/42 248
      */

     char *value;

     value = strtok(data, " ");
     table_replacecurrentcell(tab, "load1", value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "load5", value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "load15", value);
     value = strtok(NULL, "/");
     table_replacecurrentcell(tab, "runque", value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "nprocs", value);
     value = strtok(NULL, " \n");
     table_replacecurrentcell(tab, "lastproc", value);
}


/* interpret the data as a meminfo format and place it into pmacsys_tab */
void  pmacsys_col_meminfo(TABLE tab, char *data)
{
     /* /proc/meminfo looks like this in 2.2:-
      *
      *    total:    used:    free:  shared: buffers:  cached:
      *    Mem:  31502336 28065792  3436544 18141184  6090752 11194368
      *    Swap: 73990144    77824 73912320
      *    MemTotal:     30764 kB
      *    MemFree:       3356 kB
      *    MemShared:    17716 kB
      *    Buffers:       5948 kB
      *    Cached:       10932 kB
      *    SwapTotal:    72256 kB
      *    SwapFree:     72180 kB
      *
      * and is extended with more attributes in 2.4. The top line is the header, 
      * we only want lines two and three, with the remaining lines being 
      * reformatted version of the first
      *
      * In 2.6, the file is just key-value pairs; the top two lines dont exist
      */
     char *value, *linecheck, *kvalue;

     /* read cpu line */
     linecheck = strtok(data, "\n");		/* skip header line */
     linecheck = strtok(NULL, " ");		/* read Mem: word */
     if (strcmp(linecheck, "Mem:")) {
          elog_printf(ERROR, "can't find Mem line; aborting probe");
	  return;
     }
     value = strtok(NULL, " ");
     kvalue = xnstrdup(util_i32toa(strtol(value, NULL, 10)/1024));
     table_replacecurrentcell(tab, "mem_tot", kvalue);
     table_freeondestroy(tab, kvalue);

     value = strtok(NULL, " ");
     kvalue = xnstrdup(util_i32toa(strtol(value, NULL, 10)/1024));
     table_replacecurrentcell(tab, "mem_used", kvalue);
     table_freeondestroy(tab, kvalue);

     value = strtok(NULL, " ");
     kvalue = xnstrdup(util_i32toa(strtol(value, NULL, 10)/1024));
     table_replacecurrentcell(tab, "mem_free", kvalue);
     table_freeondestroy(tab, kvalue);

     value = strtok(NULL, " ");
     kvalue = xnstrdup(util_i32toa(strtol(value, NULL, 10)/1024));
     table_replacecurrentcell(tab, "mem_shared", kvalue);
     table_freeondestroy(tab, kvalue);

     value = strtok(NULL, " ");
     kvalue = xnstrdup(util_i32toa(strtol(value, NULL, 10)/1024));
     table_replacecurrentcell(tab, "mem_buf", kvalue);
     table_freeondestroy(tab, kvalue);

     value = strtok(NULL, " \n");
     kvalue = xnstrdup(util_i32toa(strtol(value, NULL, 10)/1024));
     table_replacecurrentcell(tab, "mem_cache", kvalue);
     table_freeondestroy(tab, kvalue);

     /* read Swap line */
     linecheck = strtok(NULL, " ");		/* read Swap: word */
     if (strcmp(linecheck, "Swap:")) {
          elog_printf(ERROR, "can't find Swap line; aborting probe");
	  return;
     }
     value = strtok(NULL, " ");
     kvalue = xnstrdup(util_i32toa(strtol(value, NULL, 10)/1024));
     table_replacecurrentcell(tab, "swap_tot", kvalue);
     table_freeondestroy(tab, kvalue);

     value = strtok(NULL, " ");
     kvalue = xnstrdup(util_i32toa(strtol(value, NULL, 10)/1024));
     table_replacecurrentcell(tab, "swap_used", kvalue);
     table_freeondestroy(tab, kvalue);

     value = strtok(NULL, " \n");
     kvalue = xnstrdup(util_i32toa(strtol(value, NULL, 10)/1024));
     table_replacecurrentcell(tab, "swap_free", kvalue);
     table_freeondestroy(tab, kvalue);
}

/* interpret the data as a meminfo format and place it into pmacsys_tab */
void  pmacsys_col_meminfo26(TABLE tab, ITREE *lol)
{
     /* /proc/meminfo in 2.6 is a set of key-value pairs, like this:-
      *
      *   MemTotal:       515132 kB
      *   MemFree:        165692 kB
      *   Buffers:         10004 kB
      *   Cached:         133676 kB
      *   SwapCached:          0 kB
      *   ...etc...
      */
     char *value, *attr;
     ITREE *row;
     long result;
     struct sysinfo si;

     itree_traverse(lol) {
          row = itree_get(lol);
	  attr = itree_find(row , 0);
	  if (attr == ITREE_NOVAL)
	       continue;
	  itree_next(row);
	  value = itree_get(row);

	  if (strcmp(attr, "MemTotal") == 0)
	      table_replacecurrentcell(tab, "mem_tot", value);
	  if (strcmp(attr, "MemFree") == 0)
	      table_replacecurrentcell(tab, "mem_free", value);
	  if (strcmp(attr, "Buffers") == 0)
	      table_replacecurrentcell(tab, "mem_buf", value);
	  if (strcmp(attr, "Cached") == 0)
	      table_replacecurrentcell(tab, "mem_cache", value);
	  if (strcmp(attr, "SwapTotal") == 0)
	      table_replacecurrentcell(tab, "swap_tot", value);
	  if (strcmp(attr, "SwapFree") == 0)
	      table_replacecurrentcell(tab, "swap_free", value);
    }

     /* calculate derived values */
     result = strtol(table_getcurrentcell(tab, "mem_tot"), NULL, 10) -
       strtol(table_getcurrentcell(tab, "mem_free"), NULL, 10);
     table_replacecurrentcell_alloc(tab, "mem_used", util_i32toa(result));
     result = strtol(table_getcurrentcell(tab, "swap_tot"), NULL, 10) -
       strtol(table_getcurrentcell(tab, "swap_free"), NULL, 10);
     table_replacecurrentcell_alloc(tab, "swap_used", util_i32toa(result));

     /* now get the shared memory from sysinfo structure */
     sysinfo(&si);
     table_replacecurrentcell_alloc(tab, "mem_shared", 
				    util_u32toa(si.sharedram));
}


/* interpret the data as a stat format and place it into pmacsys_tab */
void  pmacsys_col_stat(TABLE tab, char *data)
{
     /* /proc/stat in 2.2 & 2.4 has a layout similar to this below:-
      *
      * cpu  2311 11281 8225 357467
      * cpu0 2311 11281 8225 357467
      * cpu1 2311 11281 8225 357467
      * disk 69387 0 0 0
      * disk_rio 63419 0 0 0
      * disk_wio 5968 0 0 0
      * disk_rblk 94198 0 0 0
      * disk_wblk 11984 0 0 0
      * page 89312 12118
      * swap 2 21
      * intr 486990 379284 16853 0 2 5 0 2 2 2 0 2 0 21572 1 69263 2
      * ctxt 342746
      * btime 939365888
      * processes 11940
      *
      * we are intrested in a subset of this information, which if we 
      * search for the tokens on the left will make the it work for 
      * 2.4 kernel which moves disk information somewhere else and 
      * gives lots more information.
      *
      * For 2.6 kernels, see below pmacsys_col_stat26
      */
     char *value, *linecheck;

     /* read cpu line */
     linecheck = strtok(data, " ");
     if (strcmp(linecheck, "cpu")) {
          elog_printf(ERROR,"can't find cpu line as first line in /proc/stat, "
		      "have %s; aborting probe", linecheck);
	  return;
     }

     /* collect cpu ticks to place into table and calculate the sum */
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "cpu_tick_user", value);

     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "cpu_tick_nice", value);

     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "cpu_tick_system", value);

     value = strtok(NULL, " \n");
     table_replacecurrentcell(tab, "cpu_tick_idle", value);

     table_replacecurrentcell(tab, "cpu_tick_wait", "0");
     table_replacecurrentcell(tab, "cpu_tick_irq", "0");
     table_replacecurrentcell(tab, "cpu_tick_softirq", "0");
     table_replacecurrentcell(tab, "cpu_tick_steal", "0");
     table_replacecurrentcell(tab, "cpu_tick_guest", "0");

     /* skip over \0 from strtok and go to paging line */
     linecheck = strchr(value, '\0')+1;
     linecheck = strstr(linecheck, "page ");
     if (! linecheck) {
          elog_printf(ERROR,"can't find page line, have %s; aborting probe", 
		      linecheck);
	  return;
     }
     /* read page line */
     /* NOTE: i probably want to convert this to bytes not pages to make
      * it consistant */
     linecheck = strtok(linecheck, " ");	/* page word */
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "vm_pgpgin", value);
     value = strtok(NULL, " \n");
     table_replacecurrentcell(tab, "vm_pgpgout", value);

     /* skip over \0 from strtok and go to swap line */
     linecheck = strchr(value, '\0')+1;
     linecheck = strstr(linecheck, "swap ");
     if (! linecheck) {
          elog_printf(ERROR,"can't find swap line, have %s; aborting probe",
		      linecheck);
	  return;
     }
     /* read swap line */
     linecheck = strtok(NULL, " ");		/* swap word */
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "vm_pgswpin", value);
     value = strtok(NULL, " \n");
     table_replacecurrentcell(tab, "vm_pgswpout", value);

     /* skip over \0 from strtok and go to intr line */
     linecheck = strchr(value, '\0')+1;
     linecheck = strstr(linecheck, "intr ");
     if (! linecheck) {
	  elog_printf(ERROR,"can't find intr line;aborting probe");
	  return;
     }
     /* read intr line (only want the summary) */
     linecheck = strtok(NULL, " ");		/* intr word */
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "nintr", value);

     /* skip over \0 from strtok and go to ctxt line */
     linecheck = strchr(value, '\0')+1;
     linecheck = strstr(linecheck, "ctxt ");
     if (! linecheck) {
          elog_printf(ERROR,"can't find ctxt line, have %s; aborting probe",
		      linecheck);
	  return;
     }
     /* read ctxt line */
     linecheck = strtok(NULL, " ");		/* ctxt word */
     value = strtok(NULL, " \n");
     table_replacecurrentcell(tab, "ncontext", value);

     /* skip over \0 from strtok and go to processes line */
     linecheck = strchr(value, '\0')+1;
     linecheck = strstr(linecheck, "processes ");
     if (! linecheck) {
          elog_printf(ERROR,"can't find processes line, have %s; aborting probe",
		      linecheck);
	  return;
     }
     /* read processes line */
     linecheck = strtok(NULL, " \n");		/* processes word */
     value = strtok(NULL, " \n");
     table_replacecurrentcell(tab, "nforks", value);
}

/* interpret the data as a stat format and place it into pmacsys_tab */
void  pmacsys_col_stat26(TABLE tab, ITREE *lol)
{
     /* /proc/stat in 2.6 has a layout similar to this below:-
      *   cpu  11712 38 1358 104634 4200 81 0
      *   cpu0 11712 38 1358 104634 4200 81 0
      *   intr 1288186 1220272 2926 0 2 2 0 0 2 1 2 91 175 44144 0 ...etc...
      *   ctxt 795440
      *   btime 1083346844
      *   processes 2995
      *   procs_running 4
      *   procs_blocked 0
      *
      * Added on to the cpu lines are:-
      *   In 2.6.18 Steal are the number of ticks spent in other VMs
      *   In 2.6.24 Guest is the time spent running guest VMs
      */
     char *value, *attr;
     ITREE *row;

     itree_traverse(lol) {
          row = itree_get(lol);
	  attr = itree_find(row , 0);
	  if (attr == ITREE_NOVAL)
	       continue;

	  if (strcmp(attr, "cpu") == 0) {
	       itree_next(row);		/* user cpu jiffies / ticks */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "cpu_tick_user", value);

	       itree_next(row);		/* nice user cpu jiffies / ticks */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "cpu_tick_nice", value);

	       itree_next(row);		/* system cpu jiffies / ticks */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "cpu_tick_system", value);

	       itree_next(row);		/* idle cpu jiffies / ticks */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "cpu_tick_idle", value);

	       itree_next(row);		/* iowait cpu jiffies / ticks */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "cpu_tick_wait", value);

	       itree_next(row);		/* irq cpu jiffies / ticks */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "cpu_tick_irq", value);

	       itree_next(row);		/* softirq cpu jiffies / ticks */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "cpu_tick_softirq", value);

	       /* virtual machine figures if present */
	       itree_next(row);		/* steal cpu jiffies / ticks */
	       if (!itree_isbeyondend(row)) {
		    value = itree_get(row);
		    table_replacecurrentcell(tab, "cpu_tick_steal", value);
	       } else
		    table_replacecurrentcell(tab, "cpu_tick_steal", "0");

	       itree_next(row);		/* guest cpu jiffies / ticks */
	       if (!itree_isbeyondend(row)) {
		 value = itree_get(row);
		 table_replacecurrentcell(tab, "cpu_tick_guest", value);
	       } else
		 table_replacecurrentcell(tab, "cpu_tick_guest", "0");
	  }

	  if (strcmp(attr, "intr") == 0) {
	       itree_next(row);		/* interrupts */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "nintr", value);
	  }

	  if (strcmp(attr, "ctxt") == 0) {
	       itree_next(row);		/* number of contect switches */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "ncontext", value);
	  }

	  if (strcmp(attr, "processes") == 0) {
	       itree_next(row);		/* number of process forks */
	       value = itree_get(row);
	       table_replacecurrentcell(tab, "nforks", value);
	  }
     }
}

/* interpret the data as a uptime format and place it into the table */
void  pmacsys_col_uptime(TABLE tab, char *data)
{
     /* /proc/uptime looks like this:-
      *
      * 10105.01 10056.12
      * uptime   idle
      */

     char *value;

     value = strtok(data, " ");
     table_replacecurrentcell(tab, "uptime", value);
     value = strtok(NULL, " \n");
     table_replacecurrentcell(tab, "idletime", value);
}


/* Derive new calculations and metrics from current and previous data */
void pmacsys_derive(TABLE prev, TABLE cur)
{
     unsigned long long new_tick_user, new_tick_nice, new_tick_system, 
          new_tick_idle, new_tick_wait, new_tick_irq, new_tick_softirq,
          new_tick_steal, new_tick_guest;
     unsigned long long old_tick_user, old_tick_nice, old_tick_system, 
          old_tick_idle, old_tick_wait, old_tick_irq, old_tick_softirq,
          old_tick_steal, old_tick_guest;
     unsigned long long diff_ticks;
     float userfp, systemfp, nicefp, idlefp, waitfp, irqfp, softirqfp,
          stealfp, guestfp, workfp;
     char *value;

     /* extract cpu tick figures from old table */
     table_first(prev);
     new_tick_user   = strtoull(table_getcurrentcell (cur,  "cpu_tick_user"),
				NULL, 10);
     new_tick_nice   = strtoull(table_getcurrentcell (cur,  "cpu_tick_nice"),
				NULL, 10);
     new_tick_system = strtoull(table_getcurrentcell (cur,  "cpu_tick_system"),
				NULL, 10);
     new_tick_idle   = strtoull(table_getcurrentcell (cur,  "cpu_tick_idle"),
				NULL, 10);
     new_tick_wait   = strtoull(table_getcurrentcell (cur,  "cpu_tick_wait"),
				NULL, 10);
     new_tick_irq    = strtoull(table_getcurrentcell (cur,  "cpu_tick_irq"),
				NULL, 10);
     new_tick_softirq= strtoull(table_getcurrentcell (cur, "cpu_tick_softirq"),
				NULL, 10);
     new_tick_steal  = strtoull(table_getcurrentcell (cur,  "cpu_tick_steal"),
				NULL, 10);
     new_tick_guest  = strtoull(table_getcurrentcell (cur,  "cpu_tick_guest"),
				NULL, 10);
     old_tick_user   = strtoull(table_getcurrentcell (prev, "cpu_tick_user"),
				NULL, 10);
     old_tick_nice   = strtoull(table_getcurrentcell (prev, "cpu_tick_nice"),
				NULL, 10);
     old_tick_system = strtoull(table_getcurrentcell (prev, "cpu_tick_system"),
				NULL, 10);
     old_tick_idle   = strtoull(table_getcurrentcell (prev, "cpu_tick_idle"),
				NULL, 10);
     old_tick_wait   = strtoull(table_getcurrentcell (prev, "cpu_tick_wait"),
				NULL, 10);
     old_tick_irq    = strtoull(table_getcurrentcell (prev, "cpu_tick_irq"),
				NULL, 10);
     old_tick_softirq= strtoull(table_getcurrentcell (prev,"cpu_tick_softirq"),
				NULL, 10);
     old_tick_steal  = strtoull(table_getcurrentcell (prev, "cpu_tick_steal"),
				NULL, 10);
     old_tick_guest  = strtoull(table_getcurrentcell (prev, "cpu_tick_guest"),
				NULL, 10);

     /* calculate % */
     diff_ticks =   new_tick_user   + new_tick_nice 
	          + new_tick_system + new_tick_idle
	          + new_tick_irq    + new_tick_irq
	          + new_tick_steal  + new_tick_guest
	          - old_tick_user   - old_tick_nice 
	          - old_tick_system - old_tick_idle
	          - new_tick_irq    - new_tick_irq
	          - old_tick_steal  - old_tick_guest;
     if (diff_ticks < 1) {
	  elog_printf(ERROR, "improbable difference, no %");
     } else {
	  /* store values */
	  userfp = (float)(new_tick_user - old_tick_user) * 100/diff_ticks;
	  if (userfp > 100.0)
	       userfp = 100.0;
	  value = xnstrdup(util_ftoa(userfp));
	  table_replacecurrentcell(cur, "pc_user", value);
	  table_freeondestroy(cur, value);

	  systemfp=(float)(new_tick_system - old_tick_system)*100/diff_ticks;
	  if (systemfp > 100.0)
	       systemfp = 100.0;
	  value = xnstrdup(util_ftoa(systemfp));
	  table_replacecurrentcell(cur, "pc_system", value);
	  table_freeondestroy(cur, value);

	  nicefp = (float)(new_tick_nice - old_tick_nice) * 100/diff_ticks;
	  if (nicefp > 100.0)
	       nicefp = 100.0;
	  value = xnstrdup(util_ftoa(nicefp));
	  table_replacecurrentcell(cur, "pc_nice", value);
	  table_freeondestroy(cur, value);

	  idlefp = (float)(new_tick_idle - old_tick_idle) * 100/diff_ticks;
	  if (idlefp > 100.0)
	       idlefp = 100.0;
	  value = xnstrdup(util_ftoa(idlefp));
	  table_replacecurrentcell(cur, "pc_idle", value);
	  table_freeondestroy(cur, value);

	  waitfp = (float)(new_tick_wait - old_tick_wait) * 100/diff_ticks;
	  if (waitfp > 100.0)
	       waitfp = 100.0;
	  value = xnstrdup(util_ftoa(waitfp));
	  table_replacecurrentcell(cur, "pc_wait", value);
	  table_freeondestroy(cur, value);

	  irqfp = (float)(new_tick_irq - old_tick_irq) * 100/diff_ticks;
	  if (irqfp > 100.0)
	       irqfp = 100.0;
	  value = xnstrdup(util_ftoa(irqfp));
	  table_replacecurrentcell(cur, "pc_irq", value);
	  table_freeondestroy(cur, value);

	  softirqfp = (float)(new_tick_softirq - old_tick_softirq) 
	                        * 100/diff_ticks;
	  if (softirqfp > 100.0)
	       softirqfp = 100.0;
	  value = xnstrdup(util_ftoa(softirqfp));
	  table_replacecurrentcell(cur, "pc_softirq", value);
	  table_freeondestroy(cur, value);

	  stealfp = (float)(new_tick_steal - old_tick_steal) * 100/diff_ticks;
	  if (stealfp > 100.0)
	       stealfp = 100.0;
	  value = xnstrdup(util_ftoa(stealfp));
	  table_replacecurrentcell(cur, "pc_steal", value);
	  table_freeondestroy(cur, value);

	  guestfp = (float)(new_tick_guest - old_tick_guest) * 100/diff_ticks;
	  if (guestfp > 100.0)
	       guestfp = 100.0;
	  value = xnstrdup(util_ftoa(guestfp));
	  table_replacecurrentcell(cur, "pc_guest", value);
	  table_freeondestroy(cur, value);

	  workfp = userfp + systemfp + nicefp + stealfp + guestfp;
	  if (workfp > 100.0)
	       workfp = 100.0;
	  value = xnstrdup(util_ftoa(workfp));
	  table_replacecurrentcell(cur, "pc_work", value);
	  table_freeondestroy(cur, value);
     }
}


/* collect the contents of /proc/vmstat */
void  pmacsys_col_vmstat(TABLE tab, ITREE *lol) {
     /*
      * /proc/vmstat in 2.6 has a layout similar to this below:-
      *   nr_dirty 14
      *   nr_writeback 0
      *   nr_unstable 0
      *   nr_page_table_pages 359
      *   nr_mapped 57887
      *   nr_slab 3627
      *   pgpgin 150894
      *   pgpgout 19818
      *   pswpin 0
      *   pswpout 0
      *   pgalloc 1919882
      *   pgfree 1958128
      *   pgactivate 22360
      *   pgdeactivate 0
      *   pgfault 735050
      *   pgmajfault 1866
      *   pgscan 0
      *   pgrefill 0
      *   pgsteal 0
      *   pginodesteal 0
      *   kswapd_steal 0
      *   kswapd_inodesteal 0
      *   pageoutrun 0
      *   allocstall 0
      *   pgrotated 0
      */
     char *value, *attr;
     ITREE *row;

     itree_traverse(lol) {
          row = itree_get(lol);
	  attr = itree_find(row , 0);
	  if (attr == ITREE_NOVAL)
	       continue;
	  itree_next(row);
	  value = itree_get(row);

	  if (strcmp(attr, "pgpgin") == 0)
	       table_replacecurrentcell(tab, "vm_pgpgin", value);
	  if (strcmp(attr, "pgpgout") == 0)
	       table_replacecurrentcell(tab, "vm_pgpgout", value);
	  if (strcmp(attr, "pswpin") == 0)
	       table_replacecurrentcell(tab, "vm_pgswpin", value);
	  if (strcmp(attr, "pswpout") == 0)
	       table_replacecurrentcell(tab, "vm_pgswpout", value);
     }
}



#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     pmacsys_init();
     pmacsys_collect();
     if (argc > 1)
	  buf = table_outtable(pmacsys_tab);
     else
	  buf = table_print(pmacsys_tab);
     puts(buf);
     nfree(buf);
     table_destroy(pmacsys_tab);
     exit(0);
}

#endif /* TEST */

#endif /* linux */
