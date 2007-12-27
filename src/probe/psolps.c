/*
 * Solaris process probe for iiab
 * Nigel Stuckey, December 1998, January, July 1999 & March 2000
 *
 * Copyright System Garden Ltd 1998-2001. All rights reserved.
 */

#if __svr4__

#include <stdio.h>
#include <errno.h>
#include "probe.h"

/* Solaris specific includes */
#include <limits.h>
#include <dirent.h>
#include <procfs.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>

ITREE *psolps_uidtoname;

/* table constants for system probe */
struct probe_sampletab psolps_cols[] = {
  {"process",	"",	"str",	"abs", "", "1","short proc name + pid"},
  {"pid",	"",	"u32",	"abs", "", "", "process id"},
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
  {"size",	"",	"u32",	"abs", "", "", "size of process image in Kb"},
  {"rss",	"",	"u32",	"abs", "", "", "resident set size in Kb"},
  {"flag",	"",	"str",	"abs", "", "", "process flags (system "
      					       "dependent)"},
  {"nlwp",	"",	"u32",	"abs", "", "", "number of lightweight process"
				               "es within this process"},
  {"tty",	"",	"str",	"abs", "", "", "controlling tty device"},
  {"pc_cpu",	"%cpu",	"u32",	"abs", "", "", "% of recent cpu time"},
  {"pc_mem",	"%mem",	"u32",	"abs", "", "", "% of system memory"},
  {"start",	"",	"nano",	"abs", "", "", "process start time from epoc"},
  {"time",	"",	"nano",	"abs", "", "", "total cpu time for this "
      					       "process"},
  {"childtime",	"",	"nano",	"abs", "", "", "total cpu time for reaped "
				               "child processes"},
  {"nice",	"",	"u32",	"abs", "", "", "nice level for scheduling"},
  {"syscall",	"",	"u32",	"abs", "", "", "system call number (if in "
					       "kernel)"},
  {"pri",	"",	"u32",	"abs", "", "", "priority (high value=high "
					       "priority)"},
  {"wchan",	"",	"str",	"abs", "", "", "wait address for sleeping "
					       "process"},
  {"wstat",	"",	"u32",	"abs", "", "", "if zombie, the wait() status"},
  {"cmd",	"",	"str",	"abs", "", "", "command/name of exec'd file"},
  {"args",	"",	"str",	"abs", "", "", "full command string"},
  {"user_t",	"",	"nano",	"abs", "", "", "user level cpu time"},
  {"sys_t",	"",	"nano",	"abs", "", "", "sys call cpu time"},
  {"otrap_t",	"",	"nano",	"abs", "", "", "other system trap cpu time"},
  {"textfault_t",   "",	"nano",	"abs", "", "", "text page fault sleep time"},
  {"datafault_t",   "",	"nano",	"abs", "", "", "data page fault sleep time"},
  {"kernelfault_t", "",	"nano",	"abs", "", "", "kernel page fault sleep time"},
  {"lockwait_t",    "",	"nano",	"abs", "", "", "user lock wait sleep time"},
  {"osleep_t",	"",	"nano",	"abs", "", "", "all other sleep time"},
  {"waitcpu_t",	"",	"nano",	"abs", "", "", "wait-cpu (latency) time"},
  {"stop_t",	"",	"nano",	"abs", "", "", "stopped time"},
  {"minfaults",	"",	"u32",	"abs", "", "", "minor page faults"},
  {"majfaults",	"",	"u32",	"abs", "", "", "major page faults"},
  {"nswaps",	"",	"u32",	"abs", "", "", "number of swaps"},
  {"inblock",	"",	"u32",	"abs", "", "", "input blocks"},
  {"outblock",	"",	"u32",	"abs", "", "", "output blocks"},
  {"msgsnd",	"",	"u32",	"abs", "", "", "messages sent"},
  {"msgrcv",	"",	"u32",	"abs", "", "", "messages received"},
  {"sigs",	"",	"u32",	"abs", "", "", "signals received"},
  {"volctx",	"",	"u32",	"abs", "", "", "voluntary context switches"},
  {"involctx",	"",	"u32",	"abs", "", "", "involuntary context switches"},
  {"syscalls",	"",	"u32",	"abs", "", "", "system calls"},
  {"chario",	"",	"u32",	"abs", "", "", "characters read and written"},
  {"pendsig",	"",	"str",	"abs", "", "", "set of process pending "
					       "signals"},
  {"heap_vaddr",  "",	"hex",	"abs", "", "", "virtual address of process "
					       "heap"},
  {"heap_size",	  "",	"hex",	"abs", "", "", "size of process heap bytes"},
  {"stack_vaddr", "",	"hex",	"abs", "", "", "virtual address of process "
					       "stack"},
  {"stack_size",  "",	"hex",	"abs", "", "", "size of process stack bytes"},
     PROBE_ENDSAMPLE
};

struct probe_rowdiff psolps_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *psolps_getcols()    {return psolps_cols;}
struct probe_rowdiff   *psolps_getrowdiff() {return psolps_diffs;}
char                  **psolps_getpub()     {return NULL;}

/* taken from kstat.h...*/
        /*
         * Accumulated time and queue length statistics.
         *
         * Accumulated time statistics are kept as a running sum
         * of "active" time.  Queue length statistics are kept as a
         * running sum of the product of queue length and elapsed time
         * at that length -- i.e., a Riemann sum for queue length
         * integrated against time.  (You can also think of the active time
         * as a Riemann sum, for the boolean function (queue_length > 0)
         * integrated against time, or you can think of it as the
         * Lebesgue measure of the set on which queue_length > 0.)
         *
         *              ^
         *              |                       _________
         *              8                       | i4    |
         *              |                       |       |
         *      Queue   6                       |       |
         *      Length  |       _________       |       |
         *              4       | i2    |_______|       |
         *              |       |           i3          |
         *              2_______|                       |
         *              |    i1                         |
         *              |_______________________________|
         *              Time->  t1      t2      t3      t4
         *
         * At each change of state (entry or exit from the queue),
         * we add the elapsed time (since the previous state change)
         * to the active time if the queue length was non-zero during
         * that interval; and we add the product of the elapsed time
         * times the queue length to the running length*time sum.
         *
         * This method is generalizable to measuring residency
         * in any defined system: instead of queue lengths, think
         * of "outstanding RPC calls to server X".
         */

/*
 * Initialise probe for solaris system information
 */
void psolps_init() {
     /* create the user list */
     psolps_uidtoname = itree_create();
}


/* destroy any structures that may be open following a run of sampling */
void psolps_fini() {
     itree_clearoutandfree(psolps_uidtoname);
     itree_destroy(psolps_uidtoname);
}


/*
 * Solaris specific routines
 */
void psolps_collect(TABLE tab) {
     DIR *dir;
     struct dirent *d;
     char pfile[PATH_MAX];
     void *data;

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
     while ( (d = readdir(dir)) ) {
	  /* the solaris /proc only contains pids and dot files 
	   * -- nothing else */
	  if (strcmp(d->d_name, ".") == 0)
	       continue;
	  if (strcmp(d->d_name, "..") == 0)
	       continue;

	  /* open pid's status file */
	  sprintf(pfile, "/proc/%s/psinfo", d->d_name);
	  data = probe_readfile(pfile);
	  if (data) {
	       /* point of no return: start collecting table data */
	       table_addemptyrow(tab);
	       psolps_col_psinfo(tab, data, psolps_uidtoname);
	       nfree(data);
	  } else
	       continue;

	  sprintf(pfile, "/proc/%s/usage", d->d_name);
	  data = probe_readfile(pfile);
	  if (data) {
	       psolps_col_usage(tab, data);
	       table_freeondestroy(tab, data);
	  }

	  sprintf(pfile, "/proc/%s/status", d->d_name);
	  data = probe_readfile(pfile);
	  if (data) {
	       psolps_col_status(tab, data);
	       nfree(data);
	  }
     }

     /* close procfs and clean up */
     closedir(dir);
     /*itree_clearoutandfree(psolps_uidtoname); TODO: destroy only when probe is*/
}

/* takes data from /procs's psinfo structure into the table */
void psolps_col_psinfo(TABLE tab, psinfo_t *ps, ITREE *uidtoname)
{
     char *value;

     value = xnmalloc(strlen(ps->pr_fname)+16);
     sprintf(value, "%s (%d)", value, ps->pr_fname);
     table_replacecurrentcell      (tab, "process",   value);
     table_replacecurrentcell_alloc(tab, "pid",       util_u32toa(ps->pr_pid));
     table_replacecurrentcell_alloc(tab, "ppid",      util_u32toa(ps->pr_ppid));
     table_replacecurrentcell_alloc(tab, "pidglead",  util_u32toa(ps->pr_pgid));
     table_replacecurrentcell_alloc(tab, "sid",       util_u32toa(ps->pr_sid));
     table_replacecurrentcell_alloc(tab, "uid",       util_u32toa(ps->pr_uid));
     table_replacecurrentcell_alloc(tab, "pwname",    psolps_getuser(ps->pr_uid, 
							       uidtoname));
     table_replacecurrentcell_alloc(tab, "euid",      util_u32toa(ps->pr_euid));
     table_replacecurrentcell_alloc(tab, "epwname",   psolps_getuser(ps->pr_euid, 
							       uidtoname));
     table_replacecurrentcell_alloc(tab, "gid",       util_u32toa(ps->pr_gid));
     table_replacecurrentcell_alloc(tab, "egid",      util_u32toa(ps->pr_egid));
     table_replacecurrentcell_alloc(tab, "size",      util_u32toa(ps->pr_size));
     table_replacecurrentcell_alloc(tab, "rss",       util_u32toa(ps->pr_rssize));
     table_replacecurrentcell_alloc(tab, "flag",      util_u32toa(ps->pr_flag));
     table_replacecurrentcell_alloc(tab, "nlwp",      util_u32toa(ps->pr_nlwp));
     table_replacecurrentcell_alloc(tab, "tty",       util_u32toa(ps->pr_ttydev));
     table_replacecurrentcell_alloc(tab, "%cpu",      util_u32toa(ps->pr_pctcpu));
     table_replacecurrentcell_alloc(tab, "%mem",      util_u32toa(ps->pr_pctmem));
     table_replacecurrentcell_alloc(tab, "start",     util_tstoa(&ps->pr_start));
     table_replacecurrentcell_alloc(tab, "time",      util_tstoa(&ps->pr_time));
     table_replacecurrentcell_alloc(tab, "childtime", util_tstoa(&ps->pr_ctime));
     table_replacecurrentcell_alloc(tab, "nice",      
			      util_u32toa(ps->pr_lwp.pr_nice));
     table_replacecurrentcell_alloc(tab, "syscall",
			      util_u32toa(ps->pr_lwp.pr_syscall));
     table_replacecurrentcell_alloc(tab, "pri",       
			      util_u32toa(ps->pr_lwp.pr_pri));
     table_replacecurrentcell_alloc(tab, "wchan",     
			      util_u32toa(ps->pr_lwp.pr_wchan));
     table_replacecurrentcell_alloc(tab, "wstat",     util_u32toa(ps->pr_wstat));
     table_replacecurrentcell_alloc(tab, "cmd",       ps->pr_fname);
     table_replacecurrentcell_alloc(tab, "args",      ps->pr_psargs);
}


/* takes data from /proc's prusage structure into the table */
void psolps_col_usage(TABLE tab, prusage_t *pu)
{
     table_replacecurrentcell_alloc(tab, "user_t",     util_tstoa(&pu->pr_utime));
     table_replacecurrentcell_alloc(tab, "sys_t",      util_tstoa(&pu->pr_stime));
     table_replacecurrentcell_alloc(tab, "otrap_t",    util_tstoa(&pu->pr_ttime));
     table_replacecurrentcell_alloc(tab, "textfault_t",util_tstoa(&pu->pr_tftime));
     table_replacecurrentcell_alloc(tab, "datafault_t",util_tstoa(&pu->pr_dftime));
     table_replacecurrentcell_alloc(tab, "kernelfault_t",util_tstoa(&pu->pr_kftime));
     table_replacecurrentcell_alloc(tab, "lockwait_t", util_tstoa(&pu->pr_ltime));
     table_replacecurrentcell_alloc(tab, "osleep_t",   util_tstoa(&pu->pr_slptime));
     table_replacecurrentcell_alloc(tab, "waitcpu_t",  util_tstoa(&pu->pr_wtime));
     table_replacecurrentcell_alloc(tab, "stop_t",     util_tstoa(&pu->pr_stoptime));
     table_replacecurrentcell_alloc(tab, "minfaults",  util_u32toa(pu->pr_minf));
     table_replacecurrentcell_alloc(tab, "majfaults",  util_u32toa(pu->pr_majf));
     table_replacecurrentcell_alloc(tab, "nswaps",     util_u32toa(pu->pr_nswap));
     table_replacecurrentcell_alloc(tab, "inblock",    util_u32toa(pu->pr_inblk));
     table_replacecurrentcell_alloc(tab, "outblock",   util_u32toa(pu->pr_oublk));
     table_replacecurrentcell_alloc(tab, "msgsnd",     util_u32toa(pu->pr_msnd));
     table_replacecurrentcell_alloc(tab, "msgrcv",     util_u32toa(pu->pr_mrcv));
     table_replacecurrentcell_alloc(tab, "sigs",       util_u32toa(pu->pr_sigs));
     table_replacecurrentcell_alloc(tab, "volctx",     util_u32toa(pu->pr_vctx));
     table_replacecurrentcell_alloc(tab, "involctx",   util_u32toa(pu->pr_ictx));
     table_replacecurrentcell_alloc(tab, "syscalls",   util_u32toa(pu->pr_sysc));
     table_replacecurrentcell_alloc(tab, "chario",     util_u32toa(pu->pr_ioch));
}


/* takes data from /procs's pstatus structure into the table */
void psolps_col_status(TABLE tab, pstatus_t *pu)
{
     table_replacecurrentcell_alloc(tab, "pendsig",  
			      psolps_getsig(&pu->pr_sigpend));
     table_replacecurrentcell_alloc(tab, "heap_vaddr",  util_u32toa(pu->pr_brkbase));
     table_replacecurrentcell_alloc(tab, "heap_size",   util_u32toa(pu->pr_brksize));
     table_replacecurrentcell_alloc(tab, "stack_vaddr", util_u32toa(pu->pr_stkbase));
     table_replacecurrentcell_alloc(tab, "stack_size",  util_u32toa(pu->pr_brksize));
}


/* get the name of a user by uid, caching results in the ITREE */
char *psolps_getuser(int uid, ITREE *uidtoname)
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
char *psolps_getsig(sigset_t *s) {
     return NULL;
#if 0
     for (i=0; i<NSIG; i++)
	  if (prismember(s, i))
	       printf("found %d\n", i);
     return "";
#endif
}


void psolps_derive(TABLE prev, TABLE cur) {}



#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     iiab_start("", argc, argv, "", NULL);
     psolps_init();
     psolps_collect();
     if (argc > 1)
	  buf = table_outtable(tab);
     else
	  buf = table_print(tab);
     if (buf)
	  puts(buf);
     else
	  elog_die(FATAL, "no output produced");
     nfree(buf);
     table_destroy(tab);
     iiab_stop();
     exit(0);
}

#endif /* TEST */

#endif /* __svr4__ */
