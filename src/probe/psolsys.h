/*
 * Solaris system probe for iiab
 * Nigel Stuckey July 1999, March 2000, November 2004
 *
 * Copyright System Garden Ltd 1999-2004. All rights reserved.
 */

#ifndef _PSOLSYS_H_
#define _PSOLSYS_H_

#if __svr4__

#include <kstat.h>		/* Solaris uses kstat */
#include <sys/time.h>

/* table constants for system probe */
struct psolsys_assemble {
     hrtime_t	sample_t;	/* time sample was taken */
     /* sysinfo */
     float	updates;	/**/
     float 	runque;		/* += num runnable procs */
     float 	runocc;		/* ++ if num runnable procs > 0 */
     float 	swpque;		/* += num swapped procs */
     float 	swpocc;		/* ++ if num swapped procs > 0 */
     float 	waiting;	/* += jobs waiting for I/O */
     /* vminfo */
     float 	freemem;	/* += freemem in pages */
     float 	swap_resv;	/* += reserved swap in pages */
     float 	swap_alloc;	/* += allocated swap in pages */
     float 	swap_avail;	/* += unreserved swap in pages */
     float 	swap_free;	/* += unallocated swap in pages */
     /* cpu_sysinfo - detailed system information */
     float 	pc_idle;	/* %idle, time cpu was idle */
     float 	pc_wait;	/* %wait, time cpu was idle, waiting for IO */
     float 	pc_user;	/* %user, time cpu was in user space */
     float 	pc_system;	/* %system, time cpu was in kernel space */
     float 	pc_work;	/* %work, time cpu was working (%user+%system) */
     float 	wait_io;	/* time cpu was idle, waiting for IO */
     float 	wait_swap;	/* time cpu was idle, waiting for swap */
     float 	wait_pio;	/* time cpu was idle, waiting for programmed I/O */
     float 	bread;		/* physical block reads */
     float 	bwrite;		/* physical block writes (sync+async) */
     float 	lread;		/* logical block reads */
     float 	lwrite;		/* logical block writes */
     float 	phread;		/* raw I/O reads */
     float 	phwrite;	/* raw I/O writes */
     float 	pswitch;	/* context switches */
     float 	trap;		/* traps */
     float 	intr;		/* device interrupts */
     float 	syscall;	/* system calls */
     float 	sysread;	/* read() + readv() system calls */
     float 	syswrite;	/* write() + writev() system calls */
     float 	sysfork;	/* forks */
     float 	sysvfork;	/* vforks */
     float 	sysexec;	/* execs */
     float 	readch;		/* bytes read by rdwr() */
     float 	writech;	/* bytes written by rdwr() */
     float 	rawch;		/* terminal input characters */
     float 	canch;		/* chars handled in canonical mode */
     float 	outch;		/* terminal output characters */
     float 	msg;		/* msg count (msgrcv()+msgsnd() calls) */
     float 	sema;		/* semaphore ops count (semop() calls) */
     float 	namei;		/* pathname lookups */
     float 	ufsiget;	/* ufs_iget() calls */
     float 	ufsdirblk;	/* directory blocks read */
     float 	ufsipage;	/* inodes taken with attached pages */
     float 	ufsinopage;	/* inodes taked with no attached pages */
     float 	inodeovf;	/* inode table overflows */
     float 	fileovf;	/* file table overflows */
     float 	procovf;	/* proc table overflows */
     float 	intrthread;	/* interrupts as threads (below clock) */
     float 	intrblk;	/* intrs blkd/prempted/released (switch) */
     float 	idlethread;	/* times idle thread scheduled */
     float 	inv_swtch;	/* involuntary context switches */
     float 	nthreads;	/* thread_create()s */
     float 	cpumigrate;	/* cpu migrations by threads */
     float 	xcalls;		/* xcalls to other cpus */
     float 	mutex_adenters;	/* failed mutex enters (adaptive) */
     float 	rw_rdfails;	/* rw reader failures */
     float 	rw_wrfails;	/* rw writer failures */
     float 	modload;	/* times loadable module loaded */
     float 	modunload;	/* times loadable module unloaded */
     float 	bawrite;	/* physical block writes (async) */
     /* cpu_syswait - detailed wait stats */
     float 	iowait;		/* procs waiting for block I/O */
     /* cpu_vminfo - detailed virtual memory stats */
     float 	pgrec;		/* page reclaims (includes pageout) */
     float 	pgfrec;		/* page reclaims from free list */
     float 	pgin;		/* pageins */
     float 	pgpgin;		/* pages paged in */
     float 	pgout;		/* pageouts */
     float 	pgpgout;	/* pages paged out */
     float 	swapin;		/* swapins */
     float 	pgswapin;	/* pages swapped in */
     float 	swapout;	/* swapouts */
     float 	pgswapout;	/* pages swapped out */
     float 	zfod;		/* pages zero filled on demand */
     float 	dfree;		/* pages freed by daemon or auto */
     float 	scan;		/* pages examined by pageout daemon */
     float 	rev;		/* revolutions of the page daemon hand */
     float 	hat_fault;	/* minor page faults via hat_fault() */
     float 	as_fault;	/* minor page faults via as_fault() */
     float 	maj_fault;	/* major page faults */
     float 	cow_fault;	/* copy-on-write faults */
     float 	prot_fault;	/* protection faults */
     float 	softlock;	/* faults due to software locking req */
     float 	kernel_asflt;	/* as_fault()s in kernel addr space */
     float 	pgrrun;		/* times pager scheduled */
     /* ncstats - dynamic name lookup cache statistics */
     float 	nc_hits;	/* hits that we can really use */
     float 	nc_misses;	/* cache misses */
     float 	nc_enters;	/* number of enters done */
     float 	nc_dblenters;	/* num of enters when already cached */
     float 	nc_longenter;	/* long names tried to enter */
     float 	nc_longlook;	/* long names tried to look up */
     float 	nc_mvtofront;	/* entry moved to front of hash chain */
     float 	nc_purges;	/* number of purges of cache */
     /* flushmeter - virtual address cache flush instrumentation */
     float 	flush_ctx;	/* num of context flushes */
     float 	flush_segment;	/* num of segment flushes */
     float 	flush_page;	/* num of complete page flushes */
     float 	flush_partial;	/* num of partial page flushes */
     float 	flush_usr;	/* num of non-supervisor flushes */
     float 	flush_region;	/* num of region flushes */
     /* system configuration information */
     float 	var_buf;	/* num of I/O buffers */
     float 	var_call;	/* num of callout (timeout) entries */
     float 	var_proc;	/* max processes system wide */
     float 	var_maxupttl;	/* max user processes system wide */
     float 	var_nglobpris;	/* num of global scheduled priorities configured */
     float 	var_maxsyspri;	/* max global priorities used by system class */
     float 	var_clist;	/* num of clists allocated */
     float 	var_maxup;	/* max number of processes per user */
     float 	var_hbuf;	/* num of hash buffers to allocate */
     float 	var_hmask;	/* hash mask for buffers */
     float 	var_pbuf;	/* num of physical I/O buffers */
     float 	var_sptmap;	/* size of sys virt space  lloc map */
     float 	var_maxpmem;	/* max physical memory to use in pages (if 0 use all available) */
     float 	var_autoup;	/* min secs before a delayed-write "uffer can be flushed */
     float 	var_bufhwm;	/* high water mrk of buf cache in KB */
#if 0
     /* file and record locking, rectot may overflow */
     float 	flock_reccnt;	/* num of records currently in use */
     float 	flock_rectot;	/* num of records used since boot */
#endif
};


struct probe_sampletab *psolsys_getcols();
struct probe_rowdiff   *psolsys_getrowdiff();
char                  **psolsys_getpub();
void  psolsys_init();
void  psolsys_fini();
void  psolsys_collect(TABLE tab);
void  psolsys_col_sysinfo(struct psolsys_assemble *asmb, kstat_ctl_t *kc, 
			  kstat_t *ksp);
void  psolsys_col_vminfo(struct psolsys_assemble *asmb, kstat_ctl_t *kc, 
			 kstat_t *ksp);
void  psolsys_col_kstatheaders(struct psolsys_assemble *asmb, kstat_ctl_t *kc, 
			       kstat_t *ksp);
void  psolsys_col_cpustat0(struct psolsys_assemble *asmb, kstat_ctl_t *kc, 
			   kstat_t *ksp);
void  psolsys_col_ncstats(struct psolsys_assemble *asmb, kstat_ctl_t *kc, 
			  kstat_t *ksp);
void  psolsys_col_flushmeter(struct psolsys_assemble *asmb, kstat_ctl_t *kc, 
			     kstat_t *ksp);
void  psolsys_col_var(struct psolsys_assemble *asmb, kstat_ctl_t *kc, 
		      kstat_t *ksp);
void  psolsys_col_flckinfo(struct psolsys_assemble *asmb, kstat_ctl_t *kc, 
			   kstat_t *ksp);
void  psolsys_derive(TABLE prev, TABLE cur);
void  psolsys_assemble_to_table(struct psolsys_assemble *cur, 
				struct psolsys_assemble *last, TABLE tab);
void  psolsys_clear_assemble(struct psolsys_assemble *asmb);

#endif /* __svr4___ */

#endif /* _PSOLSYS_H_ */
