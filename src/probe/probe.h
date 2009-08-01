/*
 * Probe command line program
 * Runs probes contained in probelib
 *
 * Nigel Stuckey, August 1999, March 2000, May 2003
 * Copyright System Garden Ltd 1999-2003. All rights reserved.
 */

#ifndef _PROBE_H_
#define _PROBE_H_

#include "../iiab/table.h"
#include "../iiab/route.h"
#include "../iiab/meth.h"
#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"
#include "../iiab/util.h"
#include "plinio.h"
#include "psolio.h"
#include "psolsys.h"

/* 
 * Individual probes using the probe method should describe their 
 * samples in terms of an array of the following structure.
 * Terminate the array with the #define'd row PROBE_ENDSAMPLE
 */
struct probe_sampletab {
     char *name;	/* name of data column */
     char *rname;	/* replacement, sql unfriendly name */
     char *type;	/* habitat specific data type in text */
     char *sense;	/* abs=absolute value, cnt=count/increments */
     char *max;		/* maximum value in col */
     char *key;		/* 1=primary key, 2= secondary etc blank=not */
     char *info;	/* text description */
};
#define PROBE_ENDSAMPLE {NULL, NULL, NULL, NULL, NULL, NULL}

/* Data structure for each running probe (held in ITREE) */
struct probe_datainfo {
     TABLE old;				/* previous data */
     TABLE new;				/* new data */
     struct probe_rowdiff *rowdiff;	/* row diff list */
     char **pub;			/* published column list */
     void (*derive)(TABLE,TABLE);	/* routine to derive new calcs */
};

/* Difference calculation structure, termincated by PROBE_ENDROWDIFF */
struct probe_rowdiff {
     char *source;			/* source columns to difference */
     char *result;			/* difference stored in this column */
};
#define PROBE_ENDROWDIFF {NULL, NULL}

/* functions */
TABLE probe_tabinit(struct probe_sampletab *hd);
int   probe_init(char *command,ROUTE out,ROUTE err, struct meth_runset *rset);
char *probe_id();
char *probe_info();
enum exectype probe_type();
int   probe_action(char *command,ROUTE out,ROUTE err,struct meth_runset *rset);
int   probe_fini(char *command,ROUTE out,ROUTE err,struct meth_runset *rset);
char *probe_readfile(char *fname);
extern struct meth_info probe_cbinfo;

#define PROBE_STATSZ 8192

#if __svr4__

#include <kstat.h>		/* Solaris uses kstat */
#include <procfs.h>

struct probe_sampletab *psolintr_getcols();
struct probe_rowdiff   *psolintr_getrowdiff();
char                  **psolintr_getpub();
void  psolintr_init();
void  psolintr_collect(TABLE tab);
void  psolintr_col_intr(TABLE tab, kstat_ctl_t *kc, kstat_t *ksp);
void  psolintr_derive(TABLE prev, TABLE cur);

struct probe_sampletab *psolnames_getcols();
struct probe_rowdiff   *psolnames_getrowdiff();
char                  **psolnames_getpub();
void  psolnames_init();
void  psolnames_collect(TABLE tab);
void  psolnames_col_names(TABLE tab, kstat_ctl_t *kc, kstat_t *ksp);
void  psolnames_derive(TABLE prev, TABLE cur);

struct probe_sampletab *psolps_getcols();
struct probe_rowdiff   *psolps_getrowdiff();
char                  **psolps_getpub();
void  psolps_init();
void  psolps_fini();
void  psolps_collect(TABLE tab);
void  psolps_col_psinfo(TABLE tab, psinfo_t *ps, ITREE *uidtoname);
void  psolps_col_usage(TABLE tab, prusage_t *pu);
void  psolps_col_status(TABLE tab, pstatus_t *pu);
char *psolps_getuser(int uid, ITREE *uidtoname);
char *psolps_getsig(sigset_t *s);
void  psolps_derive(TABLE prev, TABLE cur);

struct probe_sampletab *psoltimer_getcols();
struct probe_rowdiff   *psoltimer_getrowdiff();
char                  **psoltimer_getpub();
void  psoltimer_init();
void  psoltimer_collect(TABLE tab);
void  psoltimer_col_timer(TABLE tab, kstat_ctl_t *kc, kstat_t *ksp);
void  psoltimer_derive(TABLE prev, TABLE cur);

struct probe_sampletab *psolup_getcols();
struct probe_rowdiff   *psolup_getrowdiff();
char                  **psolup_getpub();
void  psolup_init();
void  psolup_fini();
void  psolup_collect(TABLE tab);
void  psolup_col_utmpx(TABLE tab);
void  psolup_col_procinfo(TABLE tab);
void  psolup_col_vendor(TABLE tab);
void  psolup_derive(TABLE prev, TABLE cur);

struct probe_sampletab *psoldown_getcols();
struct probe_rowdiff   *psoldown_getrowdiff();
char                  **psoldown_getpub();
void  psoldown_init(char *probeargs);
void  psoldown_collect(TABLE tab);
void  psoldown_fini();
int   psoldown_stampboot (char *boot_purl);
int   psoldown_stampalive(char *alive_purl);
int   psoldown_getutmpxuptime(time_t *down, time_t *boot);
void  psoldown_derive(TABLE prev, TABLE cur);

#elif linux

#include <signal.h>

struct probe_sampletab *plinps_getcols();
struct probe_rowdiff   *plinps_getrowdiff();
char                  **plinps_getpub();
void   plinps_init(char *probeargs);
void   plinps_fini();
void   plinps_collect(TABLE tab);
void   plinps_col_fperm(TABLE tab, char *pfile, ITREE *uidtoname);
void   plinps_col_cmd(TABLE tab, char *value);
void   plinps_col_stat(TABLE tab, char *ps, ITREE *uidtoname);
void   plinps_col_status(TABLE tab, char *ps, ITREE *uidtoname);
void   plinps_col_statm(TABLE tab, char *ps, ITREE *uidtoname);
time_t plinps_getboot_t();
long   plinps_gettotal_mem();
char * plinps_getuser(int uid, ITREE *uidtoname);
char * plinps_getsig(sigset_t *s);
void   plinps_derive(TABLE prev, TABLE cur);

struct probe_sampletab *plinintr_getcols();
struct probe_rowdiff   *plinintr_getrowdiff();
char                  **plinintr_getpub();
void  plinintr_init();
void  plinintr_collect(TABLE tab);
void  plinintr_col_intr(TABLE tab, ITREE *idata);
void  plinintr_derive(TABLE prev, TABLE cur);

struct probe_sampletab *plinnames_getcols();
struct probe_rowdiff   *plinnames_getrowdiff();
char                  **plinnames_getpub();
void  plinnames_init();
void  plinnames_fini();
void  plinnames_collect(TABLE tab);
void  plinnames_readalldir(char *rootdir, TREE *list);
void  plinnames_derive(TABLE prev, TABLE cur);

struct probe_sampletab *plinsys_getcols();
struct probe_rowdiff   *plinsys_getrowdiff();
char                  **plinsys_getpub();
void  plinsys_init();
void  plinsys_fini();
void  plinsys_collect(TABLE tab);
void  plinsys_collect22(TABLE tab);
void  plinsys_collect26(TABLE tab);
void  plinsys_col_loadavg(TABLE tab, char *data);
void  plinsys_col_meminfo(TABLE tab, char *data);
void  plinsys_col_meminfo26(TABLE tab, ITREE *lol);
void  plinsys_col_stat(TABLE tab, char *data);
void  plinsys_col_stat26(TABLE tab, ITREE *data);
void  plinsys_col_uptime(TABLE tab, char *data);
void  plinsys_col_vmstat(TABLE tab, ITREE *data);
void  plinsys_derive(TABLE prev, TABLE cur);

struct probe_sampletab *plinnet_getcols();
struct probe_rowdiff   *plinnet_getrowdiff();
char                  **plinnet_getpub();
void  plinnet_init();
void  plinnet_collect(TABLE tab);
void  plinnet_col_netdev(TABLE tab, ITREE *idata);
void  plinnet_derive(TABLE prev, TABLE cur);

struct probe_sampletab *plinup_getcols();
struct probe_rowdiff   *plinup_getrowdiff();
char                  **plinup_getpub();
void  plinup_init();
void  plinup_fini();
void  plinup_collect(TABLE tab);
void  plinup_col_uptime(TABLE tab, char *data);
void  plinup_col_cpuinfo(TABLE tab, char *data);
int   plinup_getutmpuptime(time_t *down, time_t *boot, char *filename);
void  plinup_derive(TABLE prev, TABLE cur);

struct probe_sampletab *plindown_getcols();
struct probe_rowdiff   *plindown_getrowdiff();
char                  **plindown_getpub();
void  plindown_init(char *probeargs);
void  plindown_collect(TABLE tab);
void  plindown_fini();
int   plindown_stampboot (char *boot_purl);
int   plindown_stampalive(char *alive_purl);
int   plindown_getutmpuptime(time_t *down, time_t *boot, char *filename);
void  plindown_derive(TABLE prev, TABLE cur);

#else
#endif

#endif /* _PROBE_H_ */
