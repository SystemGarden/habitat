/*
 * Solairis I/O probe for iiab
 * Nigel Stuckey, November 2004
 *
 * Copyright System Garden Ltd 1999-2004. All rights reserved.
 */

#ifndef _PSOLIO_H_
#define _PSOLIO_H_

#if __svr4__

#include <kstat.h>		/* Solaris uses kstat */

struct psolio_assemble {
     hrtime_t sample_t;	/* time sample was taken (interval timer) */
     char *id;		/* kernel id for device */
     char *device;	/* device name */
     char *mount;	/* mount path of device or NULL */
     char *fstype;	/* string of file system type */
     long size;		/* size of device or file system in MBytes */
     long used;		/* amount of device used in MBytes */
     long reserved;	/* amount of device reserved in MBytes */
     float pc_used;	/* % of non-reserved space used */
     float kread;	/* number of kBytes read (counter, needs delta) */
     float kwritten;	/* number of kBytes written (counter, needs delta) */
     float rios;	/* number of read operations (counter, needs delta) */
     float wios;	/* number of write operations (counter, needs delta) */
     float wait_t;	/* pre-service wait time */
     float wait_len_t;	/* cumulative wait length*time product */
     float wait_cnt;	/* pre-service wait time */
     float run_t;	/* service run time */
     float run_len_t;	/* cumulative run length*time product */
     float run_cnt;	/* run count */
};


/* functional prototypes */
struct probe_sampletab *psolio_getcols();
struct probe_rowdiff   *psolio_getrowdiff();
char                  **psolio_getpub();
void  psolio_init();
void  psolio_collect(TABLE tab);
void  psolio_col_io(TREE *assemble, kstat_ctl_t *kc, kstat_t *ksp);
void  psolio_col_mounts(TREE *assemble);
void  psolio_derive(TABLE prev, TABLE cur);
struct psolio_assemble *psolio_get_assemble_record(TREE *assemble_tree, 
						   char *device);
void  psolio_free_assemble_tree(TREE *assemble);
void  psolio_assemble_to_table(TREE *assemble_tree, TREE *last_tree, 
			       TABLE tab);
TREE *psolio_path_to_inst(char *fname);
char *psolio_dev_to_inst(TREE *p2i, TREE *d2i, char *devname);
char *psolio_nfsopts_to_inst(char *nfsopts);

#endif /* __svr4__ */

#endif /* _PSOLIO_H_ */
