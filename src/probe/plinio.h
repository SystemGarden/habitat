/*
 * Linux I/O probe for iiab
 * Nigel Stuckey, April 2004
 *
 * Copyright System Garden Ltd 1999-2005. All rights reserved.
 */

#ifndef _PLINIO_H_
#define _PLINIO_H_

#if linux

struct plinio_assemble {
  time_t sample_t;	/* time of sample in seconds from 1/1/1970 epoch */
  char *device;		/* short device name */
  char *mount;		/* mount path of device or NULL */
  char *fstype;		/* string of file system type */
  float size;		/* size of device or file system in MBytes */
  float used;		/* amount of device used in MBytes */
  float reserved;	/* amount of filesystem reserved in KBytes */
  float pc_used;	/* % of device used */
  float kread;		/* kBytes / sec read (counter, needs delta) */
  float kwritten;	/* kBytes / sec written (counter, needs delta) */
  float rios;		/* number of read ops / sec (counter, needs delta) */
  float wios;		/* number of write ops / sec (counter, needs delta) */
  float read_svc_t;	/* all time spent in all reads */
  float write_svc_t;	/* all time spent in all writes */
  int n_cur_ios;
  int cur_ios_t;
};


/* functional prototypes */
struct probe_sampletab *plinio_getcols();
struct probe_rowdiff   *plinio_getrowdiff();
char                  **plinio_getpub();
void  plinio_init();
void  plinio_fini();
void  plinio_collect(TABLE tab);
void  plinio_collect22(TABLE tab);
void  plinio_collect24(TABLE tab);
void  plinio_collect26(TABLE tab);
void  plinio_col_stat(TABLE tab, ITREE *lol);
void  plinio_col_diskstats(TREE *assemble, ITREE *lol);
void  plinio_col_partitions(TREE *assemble, ITREE *lol);
void  plinio_col_mounts(TREE *assemble);
void  plinio_col_statvfs(TREE *assemble);
void  plinio_derive(TABLE prev, TABLE cur);
struct plinio_assemble *plinio_get_assemble_record(TREE *assemble_tree, 
						   char *device);
void  plinio_free_assemble_tree(TREE *assemble);
void  plinio_assemble_to_table(TREE *assemble_tree, TREE *last_tree, TABLE tab);


#endif /* linux */

#endif /* _PLINIO_H_ */
