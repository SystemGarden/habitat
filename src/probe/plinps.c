/*
 * Linux process probe for iiab
 * Nigel Stuckey, September 1999, March 2000, July 2009
 *
 * Copyright System Garden Ltd 1999-2001, 2009. All rights reserved.
 */

#if linux

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"
#include "../iiab/util.h"
#include "probe.h"

/* Linux specific routines */

/*
 * Problems with the linux version:-
 * 1. signal conversion from the int bitmap to sigset_t is optimistic rather
 *    than correct.
 * 2. many values need to be calculated as they are not available in first 
 *    generation form
 * 3. there are many vaules that just do not exist from procfs and can not
 *    be calculated. Manybe there are other places that they can be obtained?
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <search.h>
#include <pwd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "../iiab/tableset.h"

#define PLINPS_STATSZ 256

ITREE *plinps_uidtoname;	/* uid to username lookup */
int    plinps_pagesize;		/* pagesize in bytes */
float  plinps_pagetokb;		/* factor to multiply pages to get kb */
time_t plinps_boot_t;		/* time system booted */
long   plinps_total_mem;	/* total memory size */
time_t plinps_filter_t;		/* time stamp of filter route */
char * plinps_filter_purl;	/* p-url of the filter */
char * plinps_filter_cmds;	/* table of filter commands */
TABSET plinps_filter_tset;	/* compiled table set instance */

/* table constants for system probe */
struct probe_sampletab plinps_cols[] = {
  {"process",	"",     "str",  "abs", "", "1","short proc name + pid"},
  {"pid",	"",	"u32",	"abs", "", "", "process id"},
  {"state",     "",	"str",	"abs", "", "", "process state"},
  {"cmd",	"",	"str",	"abs", "", "", "command/name of exec'd file"},
  {"args",	"",	"str",	"abs", "", "", "full command string"},
  {"ppid",	"",	"u32",	"abs", "", "", "process id of parent"},
  {"pidglead",	"",	"u32",	"abs", "", "", "process id of process group "
                                               "leader"},
  {"sid",	"",	"u32",	"abs", "", "", "session id"},
  {"uid",	"",	"u32",	"abs", "", "", "real user id"},
  {"pwname",	"",	"str",	"abs", "", "", "name of real user"},
  {"euid",	"",	"u32",	"abs", "", "", "effective user id"},
  {"epwname",	"",	"str",	"abs", "", "", "name of effective user"},
  {"gid",	"",	"u32",	"abs", "", "", "real group id"},
  {"egid",	"",	"u32",	"abs", "", "", "effective group id"},
  {"size",	"",	"nano",	"abs", "", "", "virtual memory size of "
                                               "process image in Kb "
                                               "(code+data+stack)"},
  {"rss",	"",	"nano",	"abs", "", "", "resident set size in Kb"},
  {"shared",	"",	"u32",	"abs", "", "", "shared memory in Kb"},
  {"text_size",	"",	"u32",	"abs", "", "", "text segment (code) in Kb"},
  {"data_size",	"",	"u32",	"abs", "", "", "stack and data segment size "
                                               "in Kb"},
  {"library",	"",	"u32",	"abs", "", "", "library size in Kb"},
  {"dirty",	"",	"u32",	"abs", "", "", "dirty pages in Kb"},
  {"flag",	"",	"str",	"abs", "", "", "process flags (system "
                                               "dependent)"},
  {"tty",	"",	"str",	"abs", "", "", "controlling tty device"},
  {"pc_cpu",	"%cpu",	"nano", "abs", "", "", "% of cpu taken by process "
                                               "since starting"},
  {"pc_mem",	"%mem",	"nano", "abs", "", "", "% of system memory taken by "
                                               "RSS of process"},
  {"start",	"",	"nano",	"abs", "", "", "process start time from epoc"},
  {"time",	"",	"nano",	"abs", "", "", "total cpu time for this "
                                               "process"},
  {"childtime",	"",	"nano",	"abs", "", "", "total cpu time for reaped "
				               "child processes"},
  {"user_t",	"",	"nano",	"abs", "", "", "accumulated user level cpu "
                                               "time"},
  {"sys_t",	"",	"nano",	"abs", "", "", "accumulated sys call cpu "
                                               "time"},
  {"priority",	"",	"u32",	"abs", "", "", "standard nice value plus 15 "
                                               "thus never -ve"},
  {"nice",	"",	"u32",	"abs", "", "", "nice level for cpu scheduling:"
                                               " 19 (nicest) to -19 (not "
                                               "nice)"},
  {"wchan",	"",	"str",	"abs", "", "", "wait address for sleeping "
                                               "process"},
  {"wstat",	"",	"u32",	"abs", "", "", "if zombie, the wait() status"},
  {"minfaults",	"",	"u32",	"abs", "", "", "number of minor page faults "
                                               "the process has made which "
                                               "have not required loading a "
                                               "memory page from disk"},
  {"cminfaults","",	"u32",	"abs", "", "", "number of minor page faults "
                                               "the process's waited-for "
                                               "children have made"},
  {"majfaults",	"",	"u32",	"abs", "", "", "number of major page faults "
                                               "the process has made which "
                                               "have required loading a "
                                               "memory page from disk" },
  {"cmajfaults","",	"u32",	"abs", "", "", "number of major page faults "
                                               "the process's waited-for "
                                               "children have made"},
  {"irealvalue","",	"nano",	"abs", "", "", "time before next SIGALRM "
                                               "is sent to process"},
  {"nswaps",	"",	"u32",	"abs", "", "", "number of pages swapped"},
  {"sigs",	"",	"u32",	"abs", "", "", "signals received"},
  {"pendsig",	"",	"str",	"abs", "", "", "set of process pending signals"},
  {"stack_vaddr","",	"hex",	"abs", "", "", "virtual address of process stack"},
  {"stack_size","",	"hex",	"abs", "", "", "size of process stack in bytes"},
  {"pc_cpu_diff","",	"u32",	"abs", "", "", "blah blah blah"},
  PROBE_ENDSAMPLE
};


/* currently the diff does not work with multi instance data */
struct probe_rowdiff plinps_diffs[] = {
  PROBE_ENDROWDIFF,
     {"pc_cpu",  "pc_cpu_diff"}
};

/* static data return methods */
struct probe_sampletab *plinps_getcols()    {return plinps_cols;}
struct probe_rowdiff   *plinps_getrowdiff() {return plinps_diffs;}
char                  **plinps_getpub()     {return NULL;}

/* prototypes */
void plinps_load_filter(char *probeargs);
void plinps_compile_filter(TABLE tab);

/*
 * Initialise probe for linux system information
 * Takes an optional argument, which is the p-url name of a filter table.
 * If absent, the whole process table is used.
 */
void plinps_init(char *probeargs) {

     /* set filter parameters and carry out initial load */
     plinps_filter_t    = (time_t) 0;
     plinps_filter_purl = NULL;
     plinps_filter_cmds = NULL;
     plinps_filter_tset = NULL;
     plinps_load_filter(probeargs);

     /* create the uid-to-name user list */
     plinps_uidtoname = itree_create();

     /* conversion factors for pages */
     plinps_pagesize = getpagesize();
     plinps_pagetokb = plinps_pagesize / 1024;

     /* boot time */
     plinps_boot_t = plinps_getboot_t();

     /* elog_printf(DEBUG, "plinps_pagesize = %d, plinps_pagetokb = %f, "
	                   "plinps_getboot_t = %d", plinps_pagesize, 
			   plinps_pagetokb, plinps_boot_t);*/

     /* get memory size */
     plinps_total_mem = plinps_gettotal_mem();
}


/* destroy any structures that may be open following a run of sampling */
void plinps_fini() {
     itree_clearoutandfree(plinps_uidtoname);
     itree_destroy(plinps_uidtoname);
     if (plinps_filter_purl)
          nfree(plinps_filter_purl);
     if (plinps_filter_cmds)
          nfree(plinps_filter_cmds);
     if (plinps_filter_tset)
          tableset_destroy(plinps_filter_tset);
}



/*
 * Check for newer data from the route containing filter conditions and 
 * load them if available
 * If probeargs is not NULL, load from a new location before loading
 */
void plinps_load_filter(char *probeargs) {
     char *filter_purl = NULL;	/* new purl location */
     char *filter_cmds = NULL;	/* new commands */
     time_t check_t;		/* time of last check */
     int len, seq, r;

     /* check for a new p-url location supplied as an agrument */
     if (probeargs && *probeargs) {
          /* collect arguments from command line */
          filter_purl = strtok(probeargs, " ");
	  if (filter_purl == NULL) {
	       /* if arguments are specified but route does not exist, then
		* this causes the filter to be cleared and for ps to
		* carry on with out a filter */
	       elog_printf(ERROR, "no filter p-url in ps probe argument '%s'; "
			   "filtering turned off", probeargs);
	       if (plinps_filter_purl)
		    nfree(plinps_filter_purl);
	       plinps_filter_purl = NULL;
	       plinps_filter_t = 0;
	       if (plinps_filter_tset)
		    tableset_destroy(plinps_filter_tset);
	       plinps_filter_tset = NULL;
	       return;
	  }

	  /* hold new location and force load by resetting timestamp */
          if (plinps_filter_purl)
	       nfree(plinps_filter_purl);
	  plinps_filter_purl = xnstrdup(filter_purl);
	  plinps_filter_t = (time_t) 0;
          elog_printf(DIAG, "new ps probe filter '%s'", plinps_filter_purl);
     }

     /* empty route, no filter */
     if (!probeargs || !*probeargs)
          return;		/* no route name */

     /* check timestamp of filter route */
     r = route_stat(plinps_filter_purl, NULL, &seq, &len, &check_t);
     if ( ! r ) {
          if (plinps_filter_tset)
	       elog_printf(ERROR, "Unable to find '%s'; ps probe "
			   "continues without change", plinps_filter_purl);
	  else
	       elog_printf(ERROR, "Unable to find '%s'; no filtering "
			   "configured", plinps_filter_purl);
	  return;
     }
     if (check_t <= plinps_filter_t)
          return;	/* up to date, no work to do */

     /* fresh data from route: remove existing tableset and read in new one */
     plinps_filter_t = check_t;
     if (plinps_filter_tset)
          tableset_destroy(plinps_filter_tset);
     plinps_filter_tset = NULL;
     filter_cmds = route_read(filter_purl, NULL, &len);
     if (filter_cmds == NULL || len < 1) {
          /* if the route is empty, then treat it as wanting everything.
	   * Keep the route in place so the contents can be checked again
	   * but keep the table set at NULL */
          elog_printf(WARNING, "Empty filter '%s' to ps probe matches "
		      "everything; filtering turned off", filter_purl);
	  return;
     }

     /* save command text for compilation when probe is actioned */
     plinps_filter_cmds = filter_cmds;
}



/*
 * Compile the filter from commands brought in by plinps_load_filter()
 */
void plinps_compile_filter(TABLE tab) {
     int r;

     /* if no command text, abandon */
     if ( ! plinps_filter_cmds )
          return;

     /* if there is a tableset already, clear it */
     if (plinps_filter_tset)
          tableset_destroy(plinps_filter_tset);

     /* create tableset on this sample table & save
      * Having to compile on each sample table is an inefficiency !!! */
     plinps_filter_tset = tableset_create(tab);
     r = tableset_configure(plinps_filter_tset, plinps_filter_cmds);
     if ( ! r ) {
          /* tableset has failed so run without filter */
          elog_printf(ERROR, "Failed configuration '%s' turns off filtering",
		      plinps_filter_purl);
          tableset_destroy(plinps_filter_tset);
	  plinps_filter_tset = NULL;
	  return;	/* failure */
     }

     /* success ! */
}



/*
 * Linux specific routines
 */
void plinps_collect(TABLE tab) {
     DIR *dir;
     struct dirent *d;
     char pfile[PATH_MAX];
     void *data;
     TABLE filtered_tab;

     /* open procfs */
     dir = opendir("/proc");
     if ( ! dir ) {
	  elog_printf(ERROR, "can't open /proc: %d %s",
		      errno, strerror(errno));
	  return;
     }

     /* traverse process entries.
      * remember to take into consideration the transigent nature of 
      * processes, which may not be there when we come to opening them */
     while ((d = readdir(dir))) {
	  /* the linux /proc contains pids, dot files and system status files
	   * we are only interested in the pid directories whose filenames
	   * contain only digits */
	  if (strcmp(d->d_name, ".") == 0)
	       continue;
	  if (strcmp(d->d_name, "..") == 0)
	       continue;
	  if ( ! isdigit(*d->d_name) )
	       continue;

	  /* open pid's short stat file */
	  sprintf(pfile, "/proc/%s/stat", d->d_name);
	  data = probe_readfile(pfile);
	  if (data) {
	       /* point of no return: start collecting table data */
	       table_addemptyrow(tab);
	       /*plinps_col_fperm(tab, pfile, plinps_uidtoname);*/
	       plinps_col_stat(tab, data, plinps_uidtoname);
	       table_freeondestroy(tab, data);
	  } else
	       continue;

	  /* add information from the longer status file */
	  sprintf(pfile, "/proc/%s/status", d->d_name);
	  data = probe_readfile(pfile);
	  if (data) {
	       plinps_col_status(tab, data, plinps_uidtoname);
	       table_freeondestroy(tab, data);
	  }

	  /* add memory stats */
	  sprintf(pfile, "/proc/%s/statm", d->d_name);
	  data = probe_readfile(pfile);
	  if (data) {
	       plinps_col_statm(tab, data, plinps_uidtoname);
	       table_freeondestroy(tab, data);
	  }

	  /* find the command line */
	  sprintf(pfile, "/proc/%s/cmdline", d->d_name);
	  data = probe_readfile(pfile);
	  if (data) {
	       plinps_col_cmd(tab, data);
	       table_freeondestroy(tab, data);
	  }
     }

     /* close procfs and clean up */
     closedir(dir);

     /* check to see if there is any change in the route content (and
      * thus the filter clause), then compile the filter on this tab 
      * (inefficient, ought to compile once)  */
     plinps_load_filter(NULL);
     plinps_compile_filter(tab);
     if (plinps_filter_tset) {
          /* filter the main table into a subset, then replace the original
	   * with the subset */
          filtered_tab = tableset_into(plinps_filter_tset);
	  table_rmallrows(tab);
	  if (table_nrows(filtered_tab))
	       if (table_addtable(tab, filtered_tab, 0) == -1) {
		    elog_printf(FATAL, "unable to replace table");
	       }
	  table_destroy(filtered_tab);
     }
}

/* finds the owner of the process file and thus the process */
void plinps_col_fperm(TABLE tab, char *fname, ITREE *uidtoname)
{
     struct stat buf;

     if (stat(fname, &buf))
	  elog_printf(ERROR, "unable to stat: %s: %d %s", 
		      fname, errno, strerror(errno));
     else {
	  table_replacecurrentcell_alloc(tab, "uid", util_i32toa(buf.st_uid));
	  table_replacecurrentcell_alloc(tab, "gid", util_i32toa(buf.st_gid));
     	  table_replacecurrentcell(tab, "pwname", 
				   plinps_getuser(buf.st_uid, uidtoname));
     }
}

/* find the full command line */
void plinps_col_cmd(TABLE tab, char *value)
{
     table_replacecurrentcell(tab, "args", value);
}


/* takes data from /procs's stat structure into the table */
void plinps_col_stat(TABLE tab, char *ps, ITREE *uidtoname)
{
     char *value, *value2;
     long long _utime, _stime, _cutime, _cstime, runtime, runstart;
     unsigned pid, flag;
     float pcpu, rss;

     /* Format in 2.6 is similar to:
      *    921 (bash) R 4909 4921 4921 34817 7121 0 2028 488573 0 53 
      *    2 1 2127 221 16 0 1 0 10170 3190784 473 4294967295 134512640 
      *    135106276 3221222928 3221221568 4294960144 0 65538 3686404 
      *    1266761467 0 0 0 17 0 0 0
      * Col 1.  pid
      *     2.  command
      *     3.  state - process state
      *     4.  ppid - parent pid
      *     5.  pidglead - process group leading PID
      *     6.  sid - session ID
      *     7.  tty_nr - terminal number
      *     8.  tpgid - gid of process owning the tty
      *     9.  flags - flags
      *     10. minflt - number of minor faults: not needing disk page
      *     11. cminflt - minflt of children when in wait()
      *     12. majflt - number of major faults: needing disk page
      *     13. cmajflt - majflt of children when in wait()
      *     14. utime - number of jiffies this process has been 
      *                 scheduled in user mode since its creation
      *     15. stime - number of jiffies this process has been 
      *                 scheduled in kernel mode since its creation
      *     16. cutime - jiffies of children scheduled in user mode when
      *                  this process is in wait() since its creation
      *     17. cstime - jiffies of children scheduled in kernel mode when
      *                  this process is in wait() since its creation
      *     18. priority - nice value 19 (nice) to -19 (not nice)
      *     19. (ignore)
      *     20. irealvalue - jiffies before next SIGALRM
      *     21. starttime - start time of process in jiffies since system start
      *     22. vsize - virtual memory size in bytes
      *     23. rss - resident set size: number of pages in real memory, 
      *               minus 3 for administrative purposes. This is just the 
      *               pages which count towards text, data, or stack space.
      *               This does not include pages which have not been
      *               demand-loaded in, or which are swapped out
      *     24. rlim - limit in bytes on the rss
      *     25. startcode - address above which program text can run
      *     26. endcode - address below which program text can run
      *     27. startstack - address of start of stack
      *     28. kstkesp - stack pointer (esp)
      *     29. kstkeip - instruction pointer (eip)
      *     30. signal - bitmap of pending signals
      *     31. blocked - bitmap of blocked signals
      *     32. sigignore - bitmap of ignored signals
      *     33. sigcatch - bitmap of signals to catch
      *     34. wchan - wait channel: address of system call
      *     35. nswap - number of pages swapped
      *     36. cnswap - nswap for all children
      *     37. exit_signal - signal to be sent to parent when proc dies
      *     38. processor - CPU number last executed on
      */

     value = strtok(ps, " ");
     pid = strtol(value, NULL, 10);
     table_replacecurrentcell(tab, "pid",       value);

     /* command is parenthetised; remove them */
     value = strtok(NULL, " ")+1;
     value[strlen(value)-1] = '\0';
     table_replacecurrentcell(tab, "cmd",       value);

     /* id - human readable id made from cmd and pid */
     value2 = xnmalloc(strlen(value)+16);
     sprintf(value2, "%s (%d)", value, pid);
     table_replacecurrentcell(tab, "process",   value2);
     table_freeondestroy(tab, value2);

     /* PROCESS STATE CODES
      *   D    Uninterruptible sleep (usually IO)
      *   R    Running or runnable (on run queue)
      *   S    Interruptible sleep (waiting for an event to complete)
      *   T    Stopped, either by a job control signal or because it 
      *        is being traced.
      *   W    paging (not valid since the 2.6.xx kernel)
      *   X    dead (should never be seen)
      *   Z    Defunct ("zombie") process, terminated but not reaped by 
      *        its parent.
      *   <    high-priority (not nice to other users)
      *   N    low-priority (nice to other users)
      *   L    has pages locked into memory (for real-time and custom IO)
      *   s    is a session leader
      *   l    is multi-threaded (using CLONE_THREAD, like NPTL pthreads do)
      *   +    is in the foreground process group
      */
     value = strtok(NULL, " ");		/*state*/
     if (*value = 'R')
	  value = "Running";
     else if (*value = 'S')
	  value = "Sleeping";
     else if (*value = 'D')
	  value = "Disk waiting";
     else if (*value = 'Z')
	  value = "Zombie";
     else if (*value = 'T')
	  value = "Traced/stopped";
     else if (*value = 'W')
	  value = "Paging";
     table_replacecurrentcell_alloc(tab, "state", value);

     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "ppid",      value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "pidglead",  value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "sid",       value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "tty",       value);

     /* TODO: tpgid (controlling tty process group id) is currently 
      * ignored */
     value = strtok(NULL, " ");

     /* PROCESS FLAGS
      *    1   forked but didn't exec
      *    4   used super-user privileges
      */
     value = strtok(NULL, " ");
     flag = strtoul(value, (char **)NULL, 10);
     table_replacecurrentcell_alloc(tab, "flag",      util_u32toaoct(
					 (unsigned)(flag>>6U)&0x7U));
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "minfaults", value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "cminfaults", value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "majfaults", value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "cmajfaults", value);

     value = strtok(NULL, " ");
     _utime = strtol(value, NULL, 10);
     value = strtok(NULL, " ");
     _stime = strtol(value, NULL, 10);
     value = strtok(NULL, " ");
     _cutime = strtol(value, NULL, 10);
     value = strtok(NULL, " ");
     _cstime = strtol(value, NULL, 10);
     table_replacecurrentcell_alloc(tab, "time", util_jiffytoa(_utime+_stime));
     table_replacecurrentcell_alloc(tab, "childtime", 
			  util_jiffytoa(_cutime+_cstime/*-_utime-_stime*/));
     table_replacecurrentcell_alloc(tab, "user_t", util_jiffytoa(_utime));
     table_replacecurrentcell_alloc(tab, "sys_t",  util_jiffytoa(_stime));

     value = strtok(NULL, " ");		/* priority */
     table_replacecurrentcell(tab, "priority", value);
     value = strtok(NULL, " ");		/* nice */
     table_replacecurrentcell(tab, "nice", value);

     /* timeout is ignored, in 2.6 it is removed */
     value = strtok(NULL, " ");

     value = strtok(NULL, " ");
     table_replacecurrentcell_alloc(tab, "irealvalue", 
				    util_jiffytoa(strtol(value, NULL, 10)));

     value = strtok(NULL, " ");
     runstart = plinps_boot_t + (strtol(value, NULL, 10) / 100);
     table_replacecurrentcell_alloc(tab, "start", util_i32toa(runstart));

     /* %cpu -- time taken on CPU over life of process
      *         thus, the incremental value and time is recorded since 
      *         the last sample for the process key */
     /* TODO: not yet implemented, atm it is just averaged over its life */
     runtime = time(NULL) - runstart;
     pcpu = (float) (_utime+_stime) / runtime;
     if (pcpu < 0.0)
	  pcpu = 0.0;
     table_replacecurrentcell_alloc(tab, "pc_cpu",    util_ftoa(pcpu));

     value = strtok(NULL, " ");
     table_replacecurrentcell_alloc(tab, "size",
			util_ftoa((float) strtol(value, NULL, 10) / 1024.0));
     value = strtok(NULL, " ");
     rss = (float) strtol(value, NULL, 10) * plinps_pagetokb;
     table_replacecurrentcell_alloc(tab, "rss",       util_ftoa(rss));
				      

     /* %mem */
     table_replacecurrentcell_alloc(tab, "pc_mem",    util_ftoa(
					 rss * 100ULL / plinps_total_mem));

     /* TODO: rlim (current limit in bytes on the rss of the process 
      * (usually 4294967295 on i386) is currently ignored */
     value = strtok(NULL, " ");

     /* TODO: startcode (The address above which program text can run)
	is currently ignored */
     value = strtok(NULL, " ");

     /* TODO: endcode (The address below which program text can run) 
      * is currently ignored */
     value = strtok(NULL, " ");

     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "stack_vaddr", value);
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "stack_size",  value);

     /* TODO: kstkesip (the instruction pointer) is currently ignored */
     value = strtok(NULL, " ");

     /* TODO: check the signal value is really a pending signal */
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "pendsig",  value);

     /* TODO: blocked is currently ignored */
     value = strtok(NULL, " ");

     /* TODO: sigignore is currently ignored */
     value = strtok(NULL, " ");

     /* TODO: sigcatch is currently ignored */
     value = strtok(NULL, " ");

     /* TODO: wchan is currently unexpanded into a symbol */
     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "wchan",     value);

     value = strtok(NULL, " ");
     table_replacecurrentcell(tab, "nswaps",     value);

     /* TODO: cnswap is currently ignored */
     value = strtok(NULL, " ");

     /* TODO: exit_signal is currently ignored */
     value = strtok(NULL, " ");

     /* TODO: processor is currently ignored */
     value = strtok(NULL, " ");

#if 0

the following have yet to be collected or computed

     table_replacecurrentcell(tab, "wstat",     value);
     table_replacecurrentcell_alloc(tab, "sigs",    util_u32toa(pu->pr_sigs));
     table_replacecurrentcell_alloc(tab, "volctx",  util_u32toa(pu->pr_vctx));
     table_replacecurrentcell_alloc(tab, "involctx",util_u32toa(pu->pr_ictx));
     table_replacecurrentcell_alloc(tab, "syscalls",util_u32toa(pu->pr_sysc));
     table_replacecurrentcell_alloc(tab, "chario",  util_u32toa(pu->pr_ioch));

#endif
}


/* takes data from /procs's status structure into the table */
void plinps_col_status(TABLE tab, char *ps, ITREE *uidtoname)
{
     ITREE *lines, *cols;
     char *key, *val;
     int seen_vmdata=0, seen_vmlib=0, seen_vmexe=0;

     /* Format is similar to:-
      *    Name:   bash
      *    State:  R (running)
      *    SleepAVG:       100%
      *    Tgid:   4916
      *    Pid:    4916
      *    PPid:   4903
      *    TracerPid:      0
      *    Uid:    501     501     501     501
      *    Gid:    501     501     501     501
      *    FDSize: 256
      *    Groups: 501
      *    VmSize:     3120 kB
      *    VmLck:         0 kB
      *    VmRSS:      1852 kB
      *    VmData:      952 kB
      *    VmStk:        24 kB
      *    VmExe:       580 kB
      *    VmLib:      1264 kB
      *    Threads:        1
      *    SigPnd: 0000000000000000
      *    ShdPnd: 0000000000000000
      *    SigBlk: 0000000000010002
      *    SigIgn: 0000000000384004
      *    SigCgt: 000000004b813efb
      *    CapInh: 0000000000000000
      *    CapPrm: 0000000000000000
      *    CapEff: 0000000000000000
      */

     util_scantext(ps, " \t:", UTIL_MULTISEP, &lines);
     itree_traverse(lines) {
	  cols = itree_get(lines);
	  key = itree_find(cols, 0);
	  if (key == ITREE_NOVAL)
	       continue;

	  switch(key[0]) {
	  case 'N':	/* Name */
	       break;
	  case 'S':	/* State, SleepSVG, Sig* */
	       break;
	  case 'T':	/* Tgid, TracerPid, Threads  */
	       break;
	  case 'P':	/* Pid, PPid */
	       break;
	  case 'U':	/* Uid */
	       if (strcmp(key, "Uid") == 0) {
		    /* real uid */
		    itree_next(cols);
		    val = itree_get(cols);
		    table_replacecurrentcell(tab, "uid",    val);
		    table_replacecurrentcell(tab, "pwname", 
			plinps_getuser(strtol(val, NULL, 10), uidtoname));

		    /* effective uid */
		    itree_next(cols);
		    val = itree_get(cols);
		    table_replacecurrentcell(tab, "euid",    val);
		    table_replacecurrentcell(tab, "epwname", 
			plinps_getuser(strtol(val, NULL, 10), uidtoname));
#if 0
		    itree_next(cols);
		    val = itree_get(cols);	/* suid - saved user id */
		    itree_next(cols);
		    val = itree_get(cols);	/* fuid - fs user id */
#endif
	       }
	       break;
	  case 'G':	/* Gid, Groups */
	       if (strcmp(key, "Gid") == 0) {
		    /* real gid */
		    itree_next(cols);
		    val = itree_get(cols);
		    table_replacecurrentcell(tab, "gid",    val);

		    /* effective gid */
		    itree_next(cols);
		    val = itree_get(cols);
		    table_replacecurrentcell(tab, "egid",    val);
#if 0
		    itree_next(cols);
		    val = itree_get(cols);	/* sgid - saved group id */
		    itree_next(cols);
		    val = itree_get(cols);	/* fgid - fs group id */
#endif
	       }
	       break;
	  case 'F':	/* FDSize */
	       break;
	  case 'V':	/* Vm* */
	       if (strcmp(key, "VmData") == 0) {
		    itree_next(cols);
		    val = itree_get(cols);
		    table_replacecurrentcell(tab, "data_size", val);
		    seen_vmdata++;
	       }
	       if (strcmp(key, "VmLib") == 0) {
		    itree_next(cols);
		    val = itree_get(cols);
		    table_replacecurrentcell(tab, "library", val);
		    seen_vmlib++;
	       }
	       if (strcmp(key, "VmExe") == 0) {
		    itree_next(cols);
		    val = itree_get(cols);
		    table_replacecurrentcell(tab, "text_size", val);
		    seen_vmexe++;
	       }
	       break;
	  case 'C':	/* Cap* */
	       break;
	  default:
	       break;
	  }
     }
     util_scanfree(lines);

     if (!seen_vmdata)
	  table_replacecurrentcell_alloc(tab, "data_size", "0");
     if (!seen_vmlib)
	  table_replacecurrentcell_alloc(tab, "library", "0");
     if (!seen_vmexe)
	  table_replacecurrentcell_alloc(tab, "text_size", "0");
}


/* get statistics from /proc/<pid>/statm */
void plinps_col_statm(TABLE tab, char *ps, ITREE *uidtoname)
{
     char *value;

     /* format is typically:-
      *     779 454 536 151 0 628 0
      * col 1  size       total program size
      * col 2  resident   resident set size
      * col 3  share      shared pages
      * col 4  trs        text (code)
      * col 5  drs        data/stack
      * col 6  lrs        library
      * col 7  dt         dirty pages
      */

     value = strtok(ps, " ");	/* size */
     value = strtok(NULL, " ");	/* rss */
     value = strtok(NULL, " ");	/* share */
     table_replacecurrentcell_alloc(tab, "shared", 
				    util_i32toa(strtol(value, NULL, 10)
							  *plinps_pagetokb));
     value = strtok(NULL, " ");	/* text_size */
#if 0
     table_replacecurrentcell_alloc(tab, "text_size", 
				    util_i32toa(strtol(value, NULL, 10)
							  *plinps_pagetokb));
#endif
     value = strtok(NULL, " ");	/* data_size */
#if 0
     table_replacecurrentcell_alloc(tab, "data_size", 
				    util_i32toa(strtol(value, NULL, 10)
							  *plinps_pagetokb));
#endif
     value = strtok(NULL, " ");	/* library */
#if 0
     table_replacecurrentcell_alloc(tab, "library",
				    util_i32toa(strtol(value, NULL, 10)
							  *plinps_pagetokb));
#endif
     value = strtok(NULL, " ");	/* dirty */
     table_replacecurrentcell_alloc(tab, "dirty", 
				    util_i32toa(strtol(value, NULL, 10)
							  *plinps_pagetokb));
}


/* get boot time from system */
time_t plinps_getboot_t() {
     time_t t;
     char *data;

     t = time(NULL);
     data = probe_readfile("/proc/uptime");
     if (data) {
          t -= strtol(strtok(data, " "), NULL, 10);
     } else {
	  elog_printf(ERROR, "unable to read uptime, setting ps boot to 0");
	  t = 0;
     }

     return t;
}


/* get total memory size */
long plinps_gettotal_mem() {
     char *data, *pt;

     data = probe_readfile("/proc/meminfo");
     if (data) {
	  /* Look for MemTotal:   12345678 kB */
	  pt = strstr(data, "MemTotal:");
	  if (!pt) {
	       elog_printf(ERROR, "unable to find MemTotal in meminfo, "
			   "setting size to 1");
	       return 1;
	  }
	  pt += 9;
	  while (*pt && *pt == ' ')
	       pt++;
	  return strtol(strtok(pt, " "), NULL, 10);
     } else {
	  elog_printf(ERROR, "unable to read meminfo, setting size to 1");
	  return 1;
     }

}


/* get the name of a user by uid, caching results in the ITREE */
char *plinps_getuser(int uid, ITREE *uidtoname)
{
     char *name;
     struct passwd *pwent;

     /* return name if in table */
     name = itree_find(uidtoname, uid);
     if (name != ITREE_NOVAL)
	  return name;

     /* fetch pw entry and load name into table */
     pwent = getpwuid(uid);
     if (pwent)
	  itree_add(uidtoname, uid, xnstrdup(pwent->pw_name));
     else
	  itree_add(uidtoname, uid, xnstrdup("unknown"));

     return itree_find(uidtoname, uid);
}

/* get a test representation of the signal set */
char *plinps_getsig(sigset_t *s) {
     return NULL;
#if 0
     int i;
     for (i=0; i<NSIG; i++)
	  if (prismember(s, i))
	       printf("found %d\n", i);
     return "";
#endif
}


void plinps_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     iiab_start("", argc, argv, "", NULL);
     plinps_init();
     plinps_collect();
     if (argc > 1)
	  buf = table_outtable(plinps_tab);
     else
	  buf = table_print(plinps_tab);
     if (buf)
	  puts(buf);
     else
	  elog_die(FATAL, "plinps", 0, "no output produced");
     nfree(buf);
     table_destroy(plinps_tab);
     plinps_fini();
     iiab_stop();
     exit(0);
}

#endif /* TEST */

#endif /* linux */
