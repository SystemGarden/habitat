/*
 * Solaris system probe for iiab
 * Nigel Stuckey July 1999, March 2000
 *
 * Copyright System Garden Ltd 1999-2001. All rights reserved.
 */

#if __svr4__

#include <stdio.h>
#include "probe.h"
#include "psolsys.h"

/* Solaris specific routines */
#include <kstat.h>		/* Solaris uses kstat */
#include <sys/sysinfo.h>	/* sysinfo */
#include <sys/dnlc.h>		/* ncstats */
#include <sys/vmmeter.h>	/* flushmeter */
#include <sys/var.h>		/* var */


/* table constants for system probe */
struct probe_sampletab psolsys_cols[] = {
     /* sysinfo */
  {"updates",   "", "nano", "abs", "", "", ""},
  {"runque",	"", "nano", "abs", "", "", "num runnable procs"},
  {"runocc",	"", "nano", "abs", "", "", "if num runnable procs > 0"},
  {"swpque",	"", "nano", "abs", "", "", "number of swapped procs"},
  {"swpocc",	"", "nano", "abs", "", "", "if num swapped procs > 0"},
  {"waiting",   "", "nano", "abs", "", "", "number of jobs waiting for I/O"},
     /* vminfo */
  {"freemem",   "", "nano", "abs", "", "", "free mememory in pages"},
  {"swap_resv", "", "nano", "abs", "", "", "reserved swap in pages"},
  {"swap_alloc","", "nano", "abs", "", "", "allocated swap in pages"},
  {"swap_avail","", "nano", "abs", "", "", "unreserved swap in pages"},
  {"swap_free", "", "nano", "abs", "", "", "unallocated swap in pages"},
     /* cpu_sysinfo - detailed system information */
  {"pc_idle",	"%idle",  "nano", "abs", "", "", "time cpu was idle"},
  {"pc_wait",	"%wait",  "nano", "abs", "", "", "time cpu was idle, waiting "
						"for IO"},
  {"pc_user",	"%user",  "nano", "abs", "", "", "time cpu was in user space"},
  {"pc_system", "%system","nano", "abs", "", "", "time cpu was in kernel space"},
  {"pc_work",   "%work",  "nano", "abs", "", "", "time cpu was working "
						"(%user+%system)"},
  {"wait_io",   "", "nano", "abs", "", "", "time cpu was idle, waiting for IO"},
  {"wait_swap", "", "nano", "abs", "", "", "time cpu was idle, waiting for swap"},
  {"wait_pio",  "", "nano", "abs", "", "", "time cpu was idle, waiting for "
                                          "programmed I/O"},
  {"bread",	"", "nano", "abs", "", "", "physical block reads"},
  {"bwrite",	"", "nano", "abs", "", "", "physical block writes (sync+async)"},
  {"lread",	"", "nano", "abs", "", "", "logical block reads"},
  {"lwrite",	"", "nano", "abs", "", "", "logical block writes"},
  {"phread",	"", "nano", "abs", "", "", "raw I/O reads"},
  {"phwrite",   "", "nano", "abs", "", "", "raw I/O writes"},
  {"pswitch",   "", "nano", "abs", "", "", "context switches"},
  {"trap",	"", "nano", "abs", "", "", "traps"},
  {"intr",	"", "nano", "abs", "", "", "device interrupts"},
  {"syscall",   "", "nano", "abs", "", "", "system calls"},
  {"sysread",   "", "nano", "abs", "", "", "read() + readv() system calls"},
  {"syswrite",  "", "nano", "abs", "", "", "write() + writev() system calls"},
  {"sysfork",   "", "nano", "abs", "", "", "forks"},
  {"sysvfork",  "", "nano", "abs", "", "", "vforks"},
  {"sysexec",   "", "nano", "abs", "", "", "execs"},
  {"readch",	"", "nano", "abs", "", "", "bytes read by rdwr()"},
  {"writech",   "", "nano", "abs", "", "", "bytes written by rdwr()"},
  {"rawch",	"", "nano", "abs", "", "", "terminal input characters"},
  {"canch",	"", "nano", "abs", "", "", "chars handled in canonical mode"},
  {"outch",	"", "nano", "abs", "", "", "terminal output characters"},
  {"msg",	"", "nano", "abs", "", "","msg count (msgrcv()+msgsnd() calls)"},
  {"sema",	"", "nano", "abs", "", "","semaphore ops count (semop() calls)"},
  {"namei",	"", "nano", "abs", "", "", "pathname lookups"},
  {"ufsiget",   "", "nano", "abs", "", "", "ufs_iget() calls"},
  {"ufsdirblk", "", "nano", "abs", "", "", "directory blocks read"},
  {"ufsipage",  "", "nano", "abs", "", "", "inodes taken with attached pages"},
  {"ufsinopage","", "nano", "abs", "", "","inodes taked with no attached pages"},
  {"inodeovf",  "", "nano", "abs", "", "", "inode table overflows"},
  {"fileovf",   "", "nano", "abs", "", "", "file table overflows"},
  {"procovf",   "", "nano", "abs", "", "", "proc table overflows"},
  {"intrthread","", "nano", "abs", "", "","interrupts as threads (below clock)"},
  {"intrblk",   "", "nano", "abs", "", "", "intrs blkd/prempted/released "
                                          "(switch)"},
  {"idlethread","", "nano", "abs", "", "", "times idle thread scheduled"},
  {"inv_swtch", "", "nano", "abs", "", "", "involuntary context switches"},
  {"nthreads",  "", "nano", "abs", "", "", "thread_create()s"},
  {"cpumigrate","", "nano", "abs", "", "", "cpu migrations by threads"},
  {"xcalls",	"", "nano", "abs", "", "", "xcalls to other cpus"},
  {"mutex_adenters",
                "", "nano", "abs", "", "", "failed mutex enters (adaptive)"},
  {"rw_rdfails","", "nano", "abs", "", "", "rw reader failures"},
  {"rw_wrfails","", "nano", "abs", "", "", "rw writer failures"},
  {"modload",   "", "nano", "abs", "", "", "times loadable module loaded"},
  {"modunload", "", "nano", "abs", "", "", "times loadable module unloaded"},
  {"bawrite",   "", "nano", "abs", "", "", "physical block writes (async)"},
  /* cpu_syswait - detailed wait stats */
  {"iowait",	"", "nano", "abs", "", "", "procs waiting for block I/O"},
  /* cpu_vminfo - detailed virtual memory stats */
  {"pgrec",	"", "nano", "abs", "", "", "page reclaims (includes pageout)"},
  {"pgfrec",	"", "nano", "abs", "", "", "page reclaims from free list"},
  {"pgin",	"", "nano", "abs", "", "", "pageins"},
  {"pgpgin",	"", "nano", "abs", "", "", "pages paged in"},
  {"pgout",	"", "nano", "abs", "", "", "pageouts"},
  {"pgpgout",   "", "nano", "abs", "", "", "pages paged out"},
  {"swapin",	"", "nano", "abs", "", "", "swapins"},
  {"pgswapin",  "", "nano", "abs", "", "", "pages swapped in"},
  {"swapout",   "", "nano", "abs", "", "", "swapouts"},
  {"pgswapout", "", "nano", "abs", "", "", "pages swapped out"},
  {"zfod",	"", "nano", "abs", "", "", "pages zero filled on demand"},
  {"dfree",	"", "nano", "abs", "", "", "pages freed by daemon or auto"},
  {"scan",	"", "nano", "abs", "", "", "pages examined by pageout daemon"},
  {"rev",	"", "nano", "abs", "", "","revolutions of the page daemon hand"},
  {"hat_fault", "", "nano", "abs", "", "", "minor page faults via hat_fault()"},
  {"as_fault",  "", "nano", "abs", "", "", "minor page faults via as_fault()"},
  {"maj_fault", "", "nano", "abs", "", "", "major page faults"},
  {"cow_fault", "", "nano", "abs", "", "", "copy-on-write faults"},
  {"prot_fault","", "nano", "abs", "", "", "protection faults"},
  {"softlock",  "", "nano", "abs", "", "", "faults due to software locking req"},
  {"kernel_asflt", 
                "", "nano", "abs", "", "", "as_fault()s in kernel addr space"},
  {"pgrrun",	"", "nano", "abs", "", "", "times pager scheduled"},
  /* ncstats - dynamic name lookup cache statistics */
  {"nc_hits",   "", "nano", "abs", "", "", "hits that we can really use"},
  {"nc_misses", "", "nano", "abs", "", "", "cache misses"},
  {"nc_enters", "", "nano", "abs", "", "", "number of enters done"},
  {"nc_dblenters",
                "", "nano", "abs", "", "", "num of enters when already cached"},
  {"nc_longenter",
                "", "nano", "abs", "", "", "long names tried to enter"},
  {"nc_longlook",
                "", "nano", "abs", "", "", "long names tried to look up"},
  {"nc_mvtofront",
                "", "nano", "abs", "", "", "entry moved to front of hash chain"},
  {"nc_purges", "", "nano", "abs", "", "", "number of purges of cache"},
  /* flushmeter - virtual address cache flush instrumentation */
  {"flush_ctx", "", "nano", "abs", "", "", "num of context flushes"},
  {"flush_segment",
                "", "nano", "abs", "", "", "num of segment flushes"},
  {"flush_page","", "nano", "abs", "", "", "num of complete page flushes"},
  {"flush_partial",
                "", "nano", "abs", "", "", "num of partial page flushes"},
  {"flush_usr", "", "nano", "abs", "", "", "num of non-supervisor flushes"},
  {"flush_region",	
                "", "nano", "abs", "", "", "num of region flushes"},
  /* system configuration information */
  {"var_buf",   "", "nano", "abs", "", "", "num of I/O buffers"},
  {"var_call",  "", "nano", "abs", "", "", "num of callout (timeout) entries"},
  {"var_proc",  "", "nano", "abs", "", "", "max processes system wide"},
  {"var_maxupttl",	
                "", "nano", "abs", "", "", "max user processes system wide"},
  {"var_nglobpris",	
                "", "nano", "abs", "", "", "num of global scheduled priorities "
                                         "configured"},
  {"var_maxsyspri",	
                "", "nano", "abs", "", "", "max global priorities used by "
                                         "system class"},
  {"var_clist", "", "nano", "abs", "", "", "num of clists allocated"},
  {"var_maxup", "", "nano", "abs", "", "", "max number of processes per user"},
  {"var_hbuf",  "", "nano", "abs", "", "", "num of hash buffers to allocate"},
  {"var_hmask", "", "nano", "abs", "", "", "hash mask for buffers"},
  {"var_pbuf",  "", "nano", "abs", "", "", "num of physical I/O buffers"},
  {"var_sptmap","", "nano", "abs", "", "", "size of sys virt space alloc map"},
  {"var_maxpmem","","nano", "abs", "", "", "max physical memory to use in pages "
                                          "(if 0 use all available)"},
  {"var_autoup","", "nano", "abs", "", "", "min secs before a delayed-write "
                                          "buffer can be flushed"},
  {"var_bufhwm","", "nano", "abs", "", "", "high water mrk of buf cache in KB"},
  PROBE_ENDSAMPLE
};

/* List of colums to diff */
struct probe_rowdiff psolsys_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *psolsys_getcols()    {return psolsys_cols;}
struct probe_rowdiff   *psolsys_getrowdiff() {return psolsys_diffs;}
char                  **psolsys_getpub()     {return NULL;}

/* sample storage */
struct psolsys_assemble psolsys_asmb1, psolsys_asmb2;
struct psolsys_assemble *psolsys_cur, *psolsys_last;
int psolsys_firsttime;

/*
 * Initialise probe for solaris system information
 */
void psolsys_init() {
     psolsys_cur  = &psolsys_asmb1;
     psolsys_last = &psolsys_asmb2;
     psolsys_clear_assemble(psolsys_cur);
     psolsys_clear_assemble(psolsys_last);
     psolsys_firsttime = 1;
}

void psolsys_fini() {
}

/*
 * Solaris specific routines
 */
void psolsys_collect(TABLE tab) {
     kstat_ctl_t *kc;
     kstat_t *ksp;
     struct psolsys_assemble *asmbtmp;

     /* process kstat data of type KSTAT_TYPE_RAW */
     kc = kstat_open();

     for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
	  if (ksp->ks_type == KSTAT_TYPE_RAW) {
	       if (strcmp(ksp->ks_name, "sysinfo") == 0) {
		    psolsys_cur->sample_t = ksp->ks_snaptime;
		    psolsys_col_sysinfo(psolsys_cur, kc, ksp);
	       }
	       else if (strcmp(ksp->ks_name, "vminfo") == 0)
		    psolsys_col_vminfo(psolsys_cur, kc, ksp);
	       else if (strcmp(ksp->ks_name, "cpu_stat0") == 0)
		    psolsys_col_cpustat0(psolsys_cur, kc, ksp);
	       else if (strcmp(ksp->ks_name, "kstat_headers") == 0)
		    psolsys_col_kstatheaders(psolsys_cur, kc, ksp);
	       else if (strcmp(ksp->ks_name, "ncstats") == 0)
		    psolsys_col_ncstats(psolsys_cur, kc, ksp);
	       else if (strcmp(ksp->ks_name, "flushmeter") == 0)
		    psolsys_col_flushmeter(psolsys_cur, kc, ksp);
	       else if (strcmp(ksp->ks_name, "var") == 0)
		    psolsys_col_var(psolsys_cur, kc, ksp);
	       else {
#if 0
		    elog_printf(INFO, "not reading raw: %s %d of %d", 
				ksp->ks_name,ksp->ks_ndata,ksp->ks_data_size);
#endif
		    ;
	       }
	  }

     kstat_close(kc);

     /* produce the diff'ed TABLE line and cycle the assembly structs */
     if (psolsys_firsttime) {
	  psolsys_firsttime = 0;
     } else {
	  psolsys_assemble_to_table(psolsys_cur, psolsys_last, tab);
     }
     asmbtmp = psolsys_last;
     psolsys_last = psolsys_cur;
     psolsys_cur = asmbtmp;
     psolsys_clear_assemble(psolsys_cur);
}

/* gets the sysinfo structure out of the kstat block */
void psolsys_col_sysinfo(struct psolsys_assemble *asmb, 
			 kstat_ctl_t *kc, 
			 kstat_t *ksp) {
     sysinfo_t *s;

     /* read from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* update global structures */
     asmb->updates = s->updates;
     asmb->runque  = s->runque;
     asmb->runocc  = s->runocc;
     asmb->swpque  = s->swpque;
     asmb->swpocc  = s->swpocc;
     asmb->waiting = s->waiting;
}

/* gets the vminfo structure out of the kstat block */
void psolsys_col_vminfo(struct psolsys_assemble *asmb, 
			kstat_ctl_t *kc, 
			kstat_t *ksp) {
     vminfo_t *s;

     /* read from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* update global structures */
     asmb->freemem    = s->freemem;
     asmb->swap_resv  = s->swap_resv;
     asmb->swap_alloc = s->swap_alloc;
     asmb->swap_avail = s->swap_avail;
     asmb->swap_free  = s->swap_free;
}


void  psolsys_col_kstatheaders(struct psolsys_assemble *asmb, 
			       kstat_ctl_t *kc, 
			       kstat_t *ksp) {
}

/* read cpu status from kernel */
void  psolsys_col_cpustat0(struct psolsys_assemble *asmb, 
			   kstat_ctl_t *kc, 
			   kstat_t *ksp) {
     struct cpu_stat *s;

     /* read cpu_stat struct from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* update global structures */
     asmb->pc_idle    = s->cpu_sysinfo.cpu[CPU_IDLE];
     asmb->pc_user    = s->cpu_sysinfo.cpu[CPU_USER];
     asmb->pc_system  = s->cpu_sysinfo.cpu[CPU_KERNEL];
     asmb->pc_wait    = s->cpu_sysinfo.cpu[CPU_WAIT];
     asmb->pc_work    = s->cpu_sysinfo.cpu[CPU_USER] + 
                        s->cpu_sysinfo.cpu[CPU_KERNEL];
     asmb->wait_io    = s->cpu_sysinfo.cpu[W_IO];
     asmb->wait_swap  = s->cpu_sysinfo.cpu[W_SWAP];
     asmb->wait_pio   = s->cpu_sysinfo.wait[W_PIO];
     asmb->bread      = s->cpu_sysinfo.bread;
     asmb->bwrite     = s->cpu_sysinfo.bwrite;
     asmb->lread      = s->cpu_sysinfo.lread;
     asmb->lwrite     = s->cpu_sysinfo.lwrite;
     asmb->phread     = s->cpu_sysinfo.phread;
     asmb->phwrite    = s->cpu_sysinfo.phwrite;
     asmb->pswitch    = s->cpu_sysinfo.pswitch;
     asmb->trap       = s->cpu_sysinfo.trap;
     asmb->intr       = s->cpu_sysinfo.intr;
     asmb->syscall    = s->cpu_sysinfo.syscall;
     asmb->sysread    = s->cpu_sysinfo.sysread;
     asmb->syswrite   = s->cpu_sysinfo.syswrite;
     asmb->sysfork    = s->cpu_sysinfo.sysfork;
     asmb->sysvfork   = s->cpu_sysinfo.sysvfork;
     asmb->sysexec    = s->cpu_sysinfo.sysexec;
     asmb->readch     = s->cpu_sysinfo.readch;
     asmb->writech    = s->cpu_sysinfo.writech;
     asmb->rawch      = s->cpu_sysinfo.rawch;
     asmb->canch      = s->cpu_sysinfo.canch;
     asmb->outch      = s->cpu_sysinfo.outch;
     asmb->msg        = s->cpu_sysinfo.msg;
     asmb->sema       = s->cpu_sysinfo.sema;
     asmb->namei      = s->cpu_sysinfo.namei;
     asmb->ufsiget    = s->cpu_sysinfo.ufsiget;
     asmb->ufsdirblk  = s->cpu_sysinfo.ufsdirblk;
     asmb->ufsipage   = s->cpu_sysinfo.ufsipage;
     asmb->ufsinopage = s->cpu_sysinfo.ufsinopage;
     asmb->inodeovf   = s->cpu_sysinfo.inodeovf;
     asmb->fileovf    = s->cpu_sysinfo.fileovf;
     asmb->procovf    = s->cpu_sysinfo.procovf;
     asmb->intrthread = s->cpu_sysinfo.intrthread;
     asmb->intrblk    = s->cpu_sysinfo.intrblk;
     asmb->idlethread = s->cpu_sysinfo.idlethread;
     asmb->inv_swtch  = s->cpu_sysinfo.inv_swtch;
     asmb->nthreads   = s->cpu_sysinfo.nthreads;
     asmb->cpumigrate = s->cpu_sysinfo.cpumigrate;
     asmb->xcalls     = s->cpu_sysinfo.xcalls;
     asmb->mutex_adenters = s->cpu_sysinfo.mutex_adenters;
     asmb->rw_rdfails = s->cpu_sysinfo.rw_rdfails;
     asmb->rw_wrfails = s->cpu_sysinfo.rw_wrfails;
     asmb->modload    = s->cpu_sysinfo.modload;
     asmb->modunload  = s->cpu_sysinfo.modunload;
     asmb->bawrite    = s->cpu_sysinfo.bawrite;
     asmb->iowait     = s->cpu_syswait.iowait;
     asmb->pgrec      = s->cpu_vminfo.pgrec;
     asmb->pgfrec     = s->cpu_vminfo.pgfrec;
     asmb->pgin       = s->cpu_vminfo.pgin;
     asmb->pgpgin     = s->cpu_vminfo.pgpgin;
     asmb->pgout      = s->cpu_vminfo.pgout;
     asmb->pgpgout    = s->cpu_vminfo.pgpgout;
     asmb->swapin     = s->cpu_vminfo.swapin;
     asmb->pgswapin   = s->cpu_vminfo.pgswapin;
     asmb->swapout    = s->cpu_vminfo.swapout;
     asmb->pgswapout  = s->cpu_vminfo.pgswapout;
     asmb->zfod       = s->cpu_vminfo.zfod;
     asmb->dfree      = s->cpu_vminfo.dfree;
     asmb->scan       = s->cpu_vminfo.scan;
     asmb->rev        = s->cpu_vminfo.rev;
     asmb->hat_fault  = s->cpu_vminfo.hat_fault;
     asmb->as_fault   = s->cpu_vminfo.as_fault;
     asmb->maj_fault  = s->cpu_vminfo.maj_fault;
     asmb->cow_fault  = s->cpu_vminfo.cow_fault;
     asmb->prot_fault = s->cpu_vminfo.prot_fault;
     asmb->softlock   = s->cpu_vminfo.softlock;
     asmb->kernel_asflt = s->cpu_vminfo.kernel_asflt;
     asmb->pgrrun     = s->cpu_vminfo.pgrrun;
}

/* gets the ncstats structure (dynamic name lookup cache) out of kstat block */
void psolsys_col_ncstats(struct psolsys_assemble *asmb, 
			 kstat_ctl_t *kc, 
			 kstat_t *ksp) {
     struct ncstats *s;

     /* read from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* update global structures */
     asmb->nc_hits      = s->hits;
     asmb->nc_misses    = s->misses;
     asmb->nc_enters    = s->enters;
     asmb->nc_dblenters = s->dbl_enters;
     asmb->nc_longenter = s->long_enter;
     asmb->nc_longlook  = s->long_look;
     asmb->nc_mvtofront = s->move_to_front;
     asmb->nc_purges    = s->purges;
}

/* gets the flushmeter structure (virtual address cache) out of kstat block */
void psolsys_col_flushmeter(struct psolsys_assemble *asmb, 
			    kstat_ctl_t *kc, 
			    kstat_t *ksp) {
     struct flushmeter *s;

     /* read from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* update global structures */
     asmb->flush_ctx     = s->f_ctx;
     asmb->flush_segment = s->f_segment;
     asmb->flush_page    = s->f_page;
     asmb->flush_partial = s->f_partial;
     asmb->flush_usr     = s->f_usr;
     asmb->flush_region  = s->f_region;
}

/* gets the system configuration out of kstat block */
void psolsys_col_var(struct psolsys_assemble *asmb, 
		     kstat_ctl_t *kc, 
		     kstat_t *ksp) {
     struct var *s;

     /* read from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* update global structures */
     asmb->var_buf       = s->v_buf;
     asmb->var_call      = s->v_call;
     asmb->var_proc      = s->v_proc;
     asmb->var_maxupttl  = s->v_maxupttl;
     asmb->var_nglobpris = s->v_nglobpris;
     asmb->var_maxsyspri = s->v_maxsyspri;
     asmb->var_clist     = s->v_clist;
     asmb->var_maxup     = s->v_maxup;
     asmb->var_hbuf      = s->v_hbuf;
     asmb->var_hmask     = s->v_hmask;
     asmb->var_pbuf      = s->v_pbuf;
     asmb->var_sptmap    = s->v_sptmap;
     asmb->var_maxpmem   = s->v_maxpmem;
     asmb->var_autoup    = s->v_autoup;
     asmb->var_bufhwm    = s->v_bufhwm;
}


void psolsys_derive(TABLE prev, TABLE cur) {}


/* clear the assembly an structure */
void psolsys_clear_assemble(struct psolsys_assemble *asmb)
{
     asmb->sample_t	= 0;
     asmb->updates	= 0;
     asmb->runque	= 0;
     asmb->runocc	= 0;
     asmb->swpque	= 0;
     asmb->swpocc	= 0;
     asmb->waiting	= 0;
     asmb->freemem	= 0;
     asmb->swap_resv	= 0;
     asmb->swap_alloc	= 0;
     asmb->swap_avail	= 0;
     asmb->swap_free	= 0;
     asmb->pc_idle	= 0;
     asmb->pc_wait	= 0;
     asmb->pc_user	= 0;
     asmb->pc_system	= 0;
     asmb->pc_work	= 0;
     asmb->wait_io	= 0;
     asmb->wait_swap	= 0;
     asmb->wait_pio	= 0;
     asmb->bread	= 0;
     asmb->bwrite	= 0;
     asmb->lread	= 0;
     asmb->lwrite	= 0;
     asmb->phread	= 0;
     asmb->phwrite	= 0;
     asmb->pswitch	= 0;
     asmb->trap		= 0;
     asmb->intr		= 0;
     asmb->syscall	= 0;
     asmb->sysread	= 0;
     asmb->syswrite	= 0;
     asmb->sysfork	= 0;
     asmb->sysvfork	= 0;
     asmb->sysexec	= 0;
     asmb->readch	= 0;
     asmb->writech	= 0;
     asmb->rawch	= 0;
     asmb->canch	= 0;
     asmb->outch	= 0;
     asmb->msg		= 0;
     asmb->sema		= 0;
     asmb->namei	= 0;
     asmb->ufsiget	= 0;
     asmb->ufsdirblk	= 0;
     asmb->ufsipage	= 0;
     asmb->ufsinopage	= 0;
     asmb->inodeovf	= 0;
     asmb->fileovf	= 0;
     asmb->procovf	= 0;
     asmb->intrthread	= 0;
     asmb->intrblk	= 0;
     asmb->idlethread	= 0;
     asmb->inv_swtch	= 0;
     asmb->nthreads	= 0;
     asmb->cpumigrate	= 0;
     asmb->xcalls	= 0;
     asmb->mutex_adenters = 0;
     asmb->rw_rdfails	= 0;
     asmb->rw_wrfails	= 0;
     asmb->modload	= 0;
     asmb->modunload	= 0;
     asmb->bawrite	= 0;
     asmb->iowait	= 0;
     asmb->pgrec	= 0;
     asmb->pgfrec	= 0;
     asmb->pgin		= 0;
     asmb->pgpgin	= 0;
     asmb->pgout	= 0;
     asmb->pgpgout	= 0;
     asmb->swapin	= 0;
     asmb->pgswapin	= 0;
     asmb->swapout	= 0;
     asmb->pgswapout	= 0;
     asmb->zfod		= 0;
     asmb->dfree	= 0;
     asmb->scan		= 0;
     asmb->rev		= 0;
     asmb->hat_fault	= 0;
     asmb->as_fault	= 0;
     asmb->maj_fault	= 0;
     asmb->cow_fault	= 0;
     asmb->prot_fault	= 0;
     asmb->softlock	= 0;
     asmb->kernel_asflt	= 0;
     asmb->pgrrun	= 0;
     asmb->nc_hits	= 0;
     asmb->nc_misses	= 0;
     asmb->nc_enters	= 0;
     asmb->nc_dblenters	= 0;
     asmb->nc_longenter	= 0;
     asmb->nc_longlook	= 0;
     asmb->nc_mvtofront	= 0;
     asmb->nc_purges	= 0;
     asmb->flush_ctx	= 0;
     asmb->flush_segment= 0;
     asmb->flush_page	= 0;
     asmb->flush_partial= 0;
     asmb->flush_usr	= 0;
     asmb->flush_region	= 0;
     asmb->var_buf	= 0;
     asmb->var_call	= 0;
     asmb->var_proc	= 0;
     asmb->var_maxupttl	= 0;
     asmb->var_nglobpris= 0;
     asmb->var_maxsyspri= 0;
     asmb->var_clist	= 0;
     asmb->var_maxup	= 0;
     asmb->var_hbuf	= 0;
     asmb->var_hmask	= 0;
     asmb->var_pbuf	= 0;
     asmb->var_sptmap	= 0;
     asmb->var_maxpmem	= 0;
     asmb->var_autoup	= 0;
     asmb->var_bufhwm	= 0;
}


/* carry out differences between two assemble structs and save the result
 * in a new row to the TABLE */
void psolsys_assemble_to_table(struct psolsys_assemble *cur, 
			       struct psolsys_assemble *last, 
			       TABLE tab)
{
     char *b;
     int nl, l;
     hrtime_t delta_hrt;
     float delta_t;

     /* timings , transform to nanoseconds */
     delta_hrt = cur->sample_t - last->sample_t;
     delta_t = delta_hrt / 1000000000;
     if (delta_t == 0.0)
	  delta_t = 1;

     /* add new row to table */
     table_addemptyrow(tab);

     /* save data to that row, using a single memory block for allocation 
      * for speed. 
      * There are roughly 130 values saved (atm), with 15 bytes per val (say)
      * resulting in 10k. Make it 10K per row */
     b = nmalloc(10000);
     table_freeondestroy(tab, b);
     l = nl = 0;

     nl += snprintf(b+l, 10000, "%.2f", (cur->updates - last->updates)/delta_t);
     table_replacecurrentcell(tab, "updates",  b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->runque - last->runque)/delta_t);
     table_replacecurrentcell(tab, "runque", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->runocc - last->runocc)/delta_t);
     table_replacecurrentcell(tab, "runocc", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->swpque - last->swpque)/delta_t);
     table_replacecurrentcell(tab, "swpque", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->swpocc - last->swpocc)/delta_t);
     table_replacecurrentcell(tab, "swpocc", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->waiting - last->waiting)/delta_t);
     table_replacecurrentcell(tab, "waiting", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->freemem - last->freemem)/delta_t);
     table_replacecurrentcell(tab, "freemem", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->swap_resv - last->swap_resv)/delta_t);
     table_replacecurrentcell(tab, "swap_resv", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->swap_alloc - last->swap_alloc)/delta_t);
     table_replacecurrentcell(tab, "swap_alloc", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->swap_avail - last->swap_avail)/delta_t);
     table_replacecurrentcell(tab, "swap_avail", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->swap_free - last->swap_free)/delta_t);
     table_replacecurrentcell(tab, "swap_free", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pc_idle - last->pc_idle)/delta_t);
     table_replacecurrentcell(tab, "pc_idle", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pc_user - last->pc_user)/delta_t);
     table_replacecurrentcell(tab, "pc_user", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pc_system - last->pc_system)/delta_t);
     table_replacecurrentcell(tab, "pc_system", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pc_wait - last->pc_wait)/delta_t);
     table_replacecurrentcell(tab, "pc_wait", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pc_work - last->pc_work)/delta_t);
     table_replacecurrentcell(tab, "pc_work", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->wait_io - last->wait_io)/delta_t);
     table_replacecurrentcell(tab, "wait_io", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->wait_swap - last->wait_swap)/delta_t);
     table_replacecurrentcell(tab, "wait_swap", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->wait_pio - last->wait_pio)/delta_t);
     table_replacecurrentcell(tab, "wait_pio", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->bread - last->bread)/delta_t);
     table_replacecurrentcell(tab, "bread", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->bwrite - last->bwrite)/delta_t);
     table_replacecurrentcell(tab, "bwrite", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->lread - last->lread)/delta_t);
     table_replacecurrentcell(tab, "lread", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->lwrite - last->lwrite)/delta_t);
     table_replacecurrentcell(tab, "lwrite", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->phread - last->phread)/delta_t);
     table_replacecurrentcell(tab, "phread", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->phwrite - last->phwrite)/delta_t);
     table_replacecurrentcell(tab, "phwrite", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pswitch - last->pswitch)/delta_t);
     table_replacecurrentcell(tab, "pswitch", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->trap - last->trap)/delta_t);
     table_replacecurrentcell(tab, "trap", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->intr - last->intr)/delta_t);
     table_replacecurrentcell(tab, "intr", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->syscall - last->syscall)/delta_t);
     table_replacecurrentcell(tab, "syscall", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->sysread - last->sysread)/delta_t);
     table_replacecurrentcell(tab, "sysread", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->syswrite - last->syswrite)/delta_t);
     table_replacecurrentcell(tab, "syswrite", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->sysfork - last->sysfork)/delta_t);
     table_replacecurrentcell(tab, "sysfork", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->sysvfork - last->sysvfork)/delta_t);
     table_replacecurrentcell(tab, "sysvfork", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->sysexec - last->sysexec)/delta_t);
     table_replacecurrentcell(tab, "sysexec", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->readch - last->readch)/delta_t);
     table_replacecurrentcell(tab, "readch", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->writech - last->writech)/delta_t);
     table_replacecurrentcell(tab, "writech", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->rawch - last->rawch)/delta_t);
     table_replacecurrentcell(tab, "rawch", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->canch - last->canch)/delta_t);
     table_replacecurrentcell(tab, "canch", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->outch - last->outch)/delta_t);
     table_replacecurrentcell(tab, "outch", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->msg - last->msg)/delta_t);
     table_replacecurrentcell(tab, "msg", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->sema - last->sema)/delta_t);
     table_replacecurrentcell(tab, "sema", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->namei - last->namei)/delta_t);
     table_replacecurrentcell(tab, "namei", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->ufsiget - last->ufsiget)/delta_t);
     table_replacecurrentcell(tab, "ufsiget", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->ufsdirblk - last->ufsdirblk)/delta_t);
     table_replacecurrentcell(tab, "ufsdirblk", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->ufsipage - last->ufsipage)/delta_t);
     table_replacecurrentcell(tab, "ufsipage", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->ufsinopage - last->ufsinopage)/delta_t);
     table_replacecurrentcell(tab, "ufsinopage", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->inodeovf - last->inodeovf)/delta_t);
     table_replacecurrentcell(tab, "inodeovf", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->fileovf - last->fileovf)/delta_t);
     table_replacecurrentcell(tab, "fileovf", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->procovf - last->procovf)/delta_t);
     table_replacecurrentcell(tab, "procovf", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->intrthread - last->intrthread)/delta_t);
     table_replacecurrentcell(tab, "intrthread", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->intrblk - last->intrblk)/delta_t);
     table_replacecurrentcell(tab, "intrblk", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->idlethread - last->idlethread)/delta_t);
     table_replacecurrentcell(tab, "idlethread", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->inv_swtch - last->inv_swtch)/delta_t);
     table_replacecurrentcell(tab, "inv_swtch", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->nthreads - last->nthreads)/delta_t);
     table_replacecurrentcell(tab, "nthreads", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->cpumigrate - last->cpumigrate)/delta_t);
     table_replacecurrentcell(tab, "cpumigrate", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->xcalls - last->xcalls)/delta_t);
     table_replacecurrentcell(tab, "xcalls", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->mutex_adenters - last->mutex_adenters)/delta_t);
     table_replacecurrentcell(tab, "mutex_adenters", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->rw_rdfails - last->rw_rdfails)/delta_t);
     table_replacecurrentcell(tab, "rw_rdfails", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->rw_wrfails - last->rw_wrfails)/delta_t);
     table_replacecurrentcell(tab, "rw_wrfails", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->modload - last->modload)/delta_t);
     table_replacecurrentcell(tab, "modload", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->modunload - last->modunload)/delta_t);
     table_replacecurrentcell(tab, "modunload", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->bawrite - last->bawrite)/delta_t);
     table_replacecurrentcell(tab, "bawrite", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->iowait - last->iowait)/delta_t);
     table_replacecurrentcell(tab, "iowait", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pgrec - last->pgrec)/delta_t);
     table_replacecurrentcell(tab, "pgrec", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pgfrec - last->pgfrec)/delta_t);
     table_replacecurrentcell(tab, "pgfrec", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pgin - last->pgin)/delta_t);
     table_replacecurrentcell(tab, "pgin", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pgpgin - last->pgpgin)/delta_t);
     table_replacecurrentcell(tab, "pgpgin", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pgout - last->pgout)/delta_t);
     table_replacecurrentcell(tab, "pgout", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pgpgout - last->pgpgout)/delta_t);
     table_replacecurrentcell(tab, "pgpgout", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->swapin - last->swapin)/delta_t);
     table_replacecurrentcell(tab, "swapin", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pgswapin - last->pgswapin)/delta_t);
     table_replacecurrentcell(tab, "pgswapin", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->swapout - last->swapout)/delta_t);
     table_replacecurrentcell(tab, "swapout", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pgswapout - last->pgswapout)/delta_t);
     table_replacecurrentcell(tab, "pgswapout", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->zfod - last->zfod)/delta_t);
     table_replacecurrentcell(tab, "zfod", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->dfree - last->dfree)/delta_t);
     table_replacecurrentcell(tab, "dfree", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->scan - last->scan)/delta_t);
     table_replacecurrentcell(tab, "scan", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->rev - last->rev)/delta_t);
     table_replacecurrentcell(tab, "rev", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->hat_fault - last->hat_fault)/delta_t);
     table_replacecurrentcell(tab, "hat_fault", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->as_fault - last->as_fault)/delta_t);
     table_replacecurrentcell(tab, "as_fault", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->maj_fault - last->maj_fault)/delta_t);
     table_replacecurrentcell(tab, "maj_fault", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->cow_fault - last->cow_fault)/delta_t);
     table_replacecurrentcell(tab, "cow_fault", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->prot_fault - last->prot_fault)/delta_t);
     table_replacecurrentcell(tab, "prot_fault", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->softlock - last->softlock)/delta_t);
     table_replacecurrentcell(tab, "softlock", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->kernel_asflt - last->kernel_asflt)/delta_t);
     table_replacecurrentcell(tab, "kernel_asflt", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->pgrrun - last->pgrrun)/delta_t);
     table_replacecurrentcell(tab, "pgrrun", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->nc_hits - last->nc_hits)/delta_t);
     table_replacecurrentcell(tab, "nc_hits", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->nc_misses - last->nc_misses)/delta_t);
     table_replacecurrentcell(tab, "nc_misses", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->nc_enters - last->nc_enters)/delta_t);
     table_replacecurrentcell(tab, "nc_enters", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->nc_dblenters - last->nc_dblenters)/delta_t);
     table_replacecurrentcell(tab, "nc_dblenters", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->nc_longenter - last->nc_longenter)/delta_t);
     table_replacecurrentcell(tab, "nc_longenter", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->nc_longlook - last->nc_longlook)/delta_t);
     table_replacecurrentcell(tab, "nc_longlook", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->nc_mvtofront - last->nc_mvtofront)/delta_t);
     table_replacecurrentcell(tab, "nc_mvtofront", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->nc_purges - last->nc_purges)/delta_t);
     table_replacecurrentcell(tab, "nc_purges", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->flush_ctx - last->flush_ctx)/delta_t);
     table_replacecurrentcell(tab, "flush_ctx", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->flush_segment - last->flush_segment)/delta_t);
     table_replacecurrentcell(tab, "flush_segment", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->flush_page - last->flush_page)/delta_t);
     table_replacecurrentcell(tab, "flush_page", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->flush_partial - last->flush_partial)/delta_t);
     table_replacecurrentcell(tab, "flush_partial", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->flush_usr - last->flush_usr)/delta_t);
     table_replacecurrentcell(tab, "flush_usr", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->flush_region - last->flush_region)/delta_t);
     table_replacecurrentcell(tab, "flush_region", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_buf - last->var_buf)/delta_t);
     table_replacecurrentcell(tab, "var_buf", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_call - last->var_call)/delta_t);
     table_replacecurrentcell(tab, "var_call", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_proc - last->var_proc)/delta_t);
     table_replacecurrentcell(tab, "var_proc", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_maxupttl - last->var_maxupttl)/delta_t);
     table_replacecurrentcell(tab, "var_maxupttl", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_nglobpris - last->var_nglobpris)/delta_t);
     table_replacecurrentcell(tab, "var_nglobpris", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_maxsyspri - last->var_maxsyspri)/delta_t);
     table_replacecurrentcell(tab, "var_maxsyspri", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_clist - last->var_clist)/delta_t);
     table_replacecurrentcell(tab, "var_clist", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_maxup - last->var_maxup)/delta_t);
     table_replacecurrentcell(tab, "var_maxup", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_hbuf - last->var_hbuf)/delta_t);
     table_replacecurrentcell(tab, "var_hbuf", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_hmask - last->var_hmask)/delta_t);
     table_replacecurrentcell(tab, "var_hmask", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_pbuf - last->var_pbuf)/delta_t);
     table_replacecurrentcell(tab, "var_pbuf", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_sptmap - last->var_sptmap)/delta_t);
     table_replacecurrentcell(tab, "var_sptmap", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_maxpmem - last->var_maxpmem)/delta_t);
     table_replacecurrentcell(tab, "var_maxpmem", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_autoup - last->var_autoup)/delta_t);
     table_replacecurrentcell(tab, "var_autoup", b+l);
     l = ++nl;

     nl += snprintf(b+l, 10000, "%.2f", (cur->var_bufhwm - last->var_bufhwm)/delta_t);
     table_replacecurrentcell(tab, "var_bufhwm", b+l);
     l = ++nl;
}

#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     psolsys_init();
     psolsys_collect();
     if (argc > 1)
	  buf = table_outtable(psolsys_tab);
     else
	  buf = table_print(psolsys_tab);
     puts(buf);
     nfree(buf);
     table_deepdestroy(psolsys_tab);
     exit(0);
}

#endif /* TEST */

#endif /* __svr4__ */
