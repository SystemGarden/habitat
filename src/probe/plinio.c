/*
 * Linux I/O probe for iiab
 * Nigel Stuckey, September 1999, March 2000, May 2001, April 2004
 *
 * Copyright System Garden Ltd 1999-2005. All rights reserved.
 */

#if linux

#include <stdio.h>
#include <stdlib.h>
#include "probe.h"
#include "plinio.h"

/* Linux specific includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <mntent.h>
#include <sys/statvfs.h>

/* table constants for system probe */
struct probe_sampletab plinio_cols[] = {
     {"id",	  "", "str",  "abs", "", "1","mount or device identifier"},
     {"device",	  "", "str",  "abs", "", "", "device name"},
     {"mount",	  "", "str",  "abs", "", "", "mount point"},
     {"fstype",	  "", "str",  "abs", "", "", "filesystem type"},
     {"size",	  "", "nano", "abs", "", "", "size of filesystem or device "
                                             "(MBytes)"},
     {"used",	  "", "nano", "abs", "", "", "space used on device (MBytes)"},
     {"reserved", "", "nano", "abs", "", "", "reserved space in filesystem "
                                             "(KBytes)"},
     {"pc_used","%used","f64","abs","100","","% used on device"},
     {"kread",	  "", "nano", "abs", "", "", "volume of data read (KB/s)"},
     {"kwritten", "", "nano", "abs", "", "", "volume of data written (KB/s)"},
     {"rios",	  "", "nano", "abs", "", "", "number of read operations/s"},
     {"wios",	  "", "nano", "abs", "", "", "number of write operations/s"},
     {"read_svc_t","","nano", "abs", "", "", "average read service time (ms)"},
     {"write_svc_t","","nano","abs", "", "", "average write service time (ms)"},
     PROBE_ENDSAMPLE
};

/* List of colums to diff */
struct probe_rowdiff plinio_diffs[] = {
     PROBE_ENDROWDIFF
};

/* Private data structure for each io instance */
struct plinio_data {
     unsigned long long nread;
     unsigned long long nwritten;
     unsigned long reads;
     unsigned long writes;
};

/* Static data return methods */
struct probe_sampletab *plinio_getcols()    {return plinio_cols;}
struct probe_rowdiff   *plinio_getrowdiff() {return plinio_diffs;}
char                  **plinio_getpub()     {return NULL;}

/* Linux version; assume 3.0 being the latest */
int plinio_linuxversion=30;

/* last set of samples taken. key is short device name, value is struct 
 * plinio_assemble */
TREE *plinio_last_data=NULL;

/*
 * Initialise probe for linux I/O information
 */
void plinio_init() {
     char *data, *vpt;

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
          plinio_linuxversion=22;
     } else if (strncmp(vpt, "2.3.", 4) == 0 || strncmp(vpt, "2.4.", 4) == 0) {
          plinio_linuxversion=24;       
     } else if (strncmp(vpt, "2.5.", 4) == 0 || strncmp(vpt, "2.6.", 4) == 0) {
          plinio_linuxversion=26;       
     } else if (strncmp(vpt, "version 3.", 10) == 0) {
          plinio_linuxversion=30;       
     } else {
          elog_printf(ERROR, "unsupported linux kernel version");
     }

     nfree(data);
     return;
}


/* shut down probe */
void plinio_fini() {
     plinio_free_assemble_tree(plinio_last_data);
}


/*
 * Linux specific routines
 */
void plinio_collect(TABLE tab) {
     if (plinio_linuxversion == 22)
          plinio_collect22(tab);
     else if (plinio_linuxversion == 24)
          plinio_collect24(tab);
     else if (plinio_linuxversion == 26 || plinio_linuxversion == 30)
          plinio_collect26(tab);
}

void plinio_collect22(TABLE tab) {
     char *data;
     ITREE *lines;

     data = probe_readfile("/proc/stat");
     if (data) {
          util_scantext(data, " ", UTIL_MULTISEP, &lines);
	  plinio_col_stat(tab, lines);

	  /* clear up */
	  util_scanfree(lines);
	  table_freeondestroy(tab, data);
     }
}



void plinio_collect24(TABLE tab) {
     char *data;
     ITREE *lines;

     data = probe_readfile("/proc/stat");
     if (data) {
          util_scantext(data, " ", UTIL_MULTISEP, &lines);
	  plinio_col_stat(tab, lines);

	  /* clear up */
	  util_scanfree(lines);
	  table_freeondestroy(tab, data);
     }
}



void plinio_collect26(TABLE tab) {
     char *disk, *part;
     ITREE *lines;
     TREE *current_data;	/* keyed by short device name (nmalloced), 
				 * value is a plinio_assemble struct. 
				 * Each element represents one device */

     /* read /proc/diskstats, join with /proc/partitions on part device name
      * and with /proc/mounts with the full device name, then find space 
      * used */
     current_data = tree_create();
     disk = probe_readfile("/proc/diskstats");
     if (disk) {
          util_scantext(disk, " ", UTIL_MULTISEP, &lines);
	  plinio_col_diskstats(current_data, lines);
	  util_scanfree(lines);
	  table_freeondestroy(tab, disk);
     }
     part = probe_readfile("/proc/partitions");
     if (part) {
          util_scantext(part, " ", UTIL_MULTISEP, &lines);
	  plinio_col_partitions(current_data, lines);
	  util_scanfree(lines);
	  table_freeondestroy(tab, part);
     }
     plinio_col_mounts(current_data);
     plinio_col_statvfs(current_data);

     /* now translate the structure into the TABLE, carrying out delta
      * operations if required */
     plinio_assemble_to_table(current_data, plinio_last_data, tab);
     if (plinio_last_data)
          plinio_free_assemble_tree(plinio_last_data);
     plinio_last_data = current_data;
}



/* scan the I/O information from /proc/stat, provided as a list of lists */
void plinio_col_stat(TABLE tab, ITREE *lol) {

     /* /proc/stat in kernel 2.2 has a layout similar to this below:-
      *
      *   cpu  3907 9 3802 206429
      *   disk 64227 0 0 0
      *   disk_rio 58695 0 0 0
      *   disk_wio 5532 0 0 0
      *   disk_rblk 83659 0 0 0
      *   disk_wblk 11190 0 0 0
      *   page 82180 17757
      *   swap 8 21
      *   intr 292563 214147 4470 0 2 5 0 2 2 2 0 2 0 9833 1 64095 2
      *   ctxt 211361
      *   btime 937292737
      *   processes 893
      *
      * The disk* lines are the ones we want, each disk device being a 
      * different column, which we can only label disk_index %d.
      *
      *   disk:      total number of operations
      *   disk_rio:  number of read io operations
      *   disk_wio:  number of write io operaations
      *   disk_rblk: number of blocks read
      *   disk_wblk: number of blocks written
      *
      *-----------
      *
      * /proc/stat in kernel version 2.4 has a more simple layout:-
      *
      *   disk_io: (2,0):(10,10,20,0,0) (3,0):(64942,50608,1376514,
      *            14334,380904) (22,2):(664,664,56786,0,0) 
      *
      * It is a single line (broken above to fit in) prefixed with 
      * `disk_io:'. Each device is space seperated with the following
      * pattern (major,disk):(total,rio,rblk,wio,wblk).
      */

     char *attr, devname[20], *alloc_devname;
     ITREE *row;
     int i;

     itree_traverse( lol ) {
          row = itree_get( lol );
	  attr = itree_find(row , 0);
	  if (attr == ITREE_NOVAL)
	       continue;

	  if ( strncmp(attr, "disk", 5) == 0 ) {
	       /* allocate space based on disk line - linux 2.2 */
	       i=-1;
	       itree_traverse( row ) {
		    i++;
		    if (i==0)
		         continue;
		    sprintf(devname, "d%d", i-1);
		    alloc_devname = xnstrdup(devname);
		    table_addemptyrow(tab);
		    table_replacecurrentcell(tab, "device", 
					     alloc_devname);
		    table_freeondestroy(tab, alloc_devname);
	       }
	  } else if ( strcmp(attr, "disk_rio") == 0 ) {
	       /* number of read operations - linux 2.2 */
	       i=-1;
	       itree_traverse( row ) {
		    i++;
		    if (i==0)
		         continue;
		    table_replacecell_noalloc(tab, i-1, "c_reads", 
					      itree_get(row));
	       }
	  } else if ( strcmp(attr, "disk_wio") == 0 ) {
	       /* number of write operations - linux 2.2 */
	       i=-1;
	       itree_traverse( row ) {
		    i++;
		    if (i==0)
		         continue;
		    table_replacecell_noalloc(tab, i-1, "c_writes", 
					      itree_get(row));
	       }
	  } else if ( strcmp(attr, "disk_rblk") == 0 ) {
	       /* number of disk blocks read - linux 2.2 */
	       /* convert to bytes to normalise, but under linux, blocks are
		* sectors which can be 1024 or 2048 bytes; the latter is for
		* CDROMS. Assume 1024 for now */
	       i=-1;
	       itree_traverse( row ) {
		    i++;
		    if (i==0)
		         continue;
		    table_replacecell_noalloc(tab, i-1, "c_nread", 
					      util_i32toa( strtol(
						itree_get(row), NULL, 10) ));
	       }
	  } else if ( strcmp(attr, "disk_wblk") == 0 ) {
	       /* number of disk blocks read - linux 2.2 */
	       /* convert to bytes to normalise, but under linux, blocks are
		* sectors which can be 1024 or 2048 bytes; the latter is for
		* CDROMS. Assume 1024 for now */
	       i=-1;
	       itree_traverse( row ) {
		    i++;
		    if (i==0)
		         continue;
		    table_replacecell_noalloc(tab, i-1, "c_nwritten", 
					      util_i32toa( strtol(
						itree_get(row), NULL, 10)) );
	       }
	  } else if ( strcmp(attr, "disk_io:") == 0) {
	       /* complete disk stats - linux 2.4 */
	       char maj[5], dsk[5], tot[15], rio[15], wio[15];
	       int rblk, wblk;
	       itree_next(row);
	       while ( ! itree_isbeyondend( row ) ) {
		    /* pattern is (major,disk):(total,rio,rblk,wio,wblk) */
		    sscanf(itree_get(row), 
			   "(%[^,],%[^)]):(%[^,],%[^,],%d,%[^,],%d)", 
			   maj, dsk, tot, rio, &rblk, wio, &wblk);
		    sprintf(devname, "d%s-%s", maj, dsk);
		    table_addemptyrow(tab);
		    table_replacecurrentcell_alloc(tab, "device", devname);
		    table_replacecurrentcell_alloc(tab, "c_reads", rio);
		    table_replacecurrentcell_alloc(tab, "c_writes", wio);
		    table_replacecurrentcell_alloc(tab, "c_nread", 
						   util_i32toa(rblk));
		    table_replacecurrentcell_alloc(tab, "c_nwritten", 
						   util_i32toa(wblk));
		    itree_next(row);
	       }
	  }
     }
}


/* scan the I/O information from /proc/diskstats, provided as a list of lists
 * and save it in a TREE of assemble */
void plinio_col_diskstats(TREE* assemble, ITREE *lol) {
     /*
      * In kernel 2.6, I/O information comes from /proc/diskstats and
      * goes down to partition level, in a layout similar to below:-
      *
      *   1    0 ram0 0 0 0 0 0 0 0 0 0 0 0
      *   1    1 ram1 0 0 0 0 0 0 0 0 0 0 0
      *   1    2 ram2 0 0 0 0 0 0 0 0 0 0 0
      *   1    3 ram3 0 0 0 0 0 0 0 0 0 0 0
      *   1    4 ram4 0 0 0 0 0 0 0 0 0 0 0
      *   1    5 ram5 0 0 0 0 0 0 0 0 0 0 0
      *   1    6 ram6 0 0 0 0 0 0 0 0 0 0 0
      *   1    7 ram7 0 0 0 0 0 0 0 0 0 0 0
      *   1    8 ram8 0 0 0 0 0 0 0 0 0 0 0
      *   1    9 ram9 0 0 0 0 0 0 0 0 0 0 0
      *   1   10 ram10 0 0 0 0 0 0 0 0 0 0 0
      *   1   11 ram11 0 0 0 0 0 0 0 0 0 0 0
      *   1   12 ram12 0 0 0 0 0 0 0 0 0 0 0
      *   1   13 ram13 0 0 0 0 0 0 0 0 0 0 0
      *   1   14 ram14 0 0 0 0 0 0 0 0 0 0 0
      *   1   15 ram15 0 0 0 0 0 0 0 0 0 0 0
      *   3    0 hda 8128 711 300633 109646 2077 4500 53408 41889 0 67323 151535
      *   3    1 hda1 16 128 0 0
      *   3    2 hda2 1 2 0 0
      *   3    5 hda5 19 19 0 0
      *   3    6 hda6 6792 261986 1929 15432
      *   3    7 hda7 1 8 0 0
      *   3    8 hda8 1916 37722 4750 37976
      *   9    0 md0 0 0 0 0 0 0 0 0 0 0 0
      *  22    0 hdc 0 0 0 0 0 0 0 0 0 0 0
      *
      * Format is <major> <minor> <dev> <stats>, where major and minor
      * are the device nodes and the dev can be either a device or partition.
      * Stats come in 4 or 11 column verities, with partitions having only 4.
      * The 11 columns are:-
      *
      *   1. *   rio:     number of reads issued
      *   2. *   rmerge:  reads merged (adjacent blocks make a big one)
      *   3. *   rsect:   number of sectors read
      *   4. *   ruse:    total milliseconds spent in all reads
      *   5. *   wio:     number of writes completed
      *   6. *   wmerge:  writes merged (adjacnt blocks make a big one)
      *   7. *   wsect:   number of sectors written
      *   8. *   wuse:    total milliseconds spent in all writes
      *   9. *   running: number of I/Os currently in progress
      *  10. *   use:     total milliseconds spent in all I/O ops
      *  11. *   aveq:    weighted milliseconds spent doing I/O
      *
      * The 4 columns are:-
      *
      *   1. rio:  number of read io operations
      *   2. wio:  number of write io operaations
      *   3. rblk: number of blocks read
      *   4. wblk: number of blocks written
      */

     if (plinio_linuxversion == 26) {
	  ITREE *row;
	  void *attr;
	  struct plinio_assemble *asmb;

	  itree_traverse( lol ) {
	       row = itree_get( lol );
	       attr = itree_find(row , 2);	/* device name */
	       asmb = plinio_get_assemble_record(assemble, (char *) attr);
	       if (itree_n(row) == 7) {
		    /* short form */
		    attr = itree_find(row , 3);	/* 1. rio == rios */
		    asmb->rios = atoll(attr);
		    itree_next(row);		/* 2. wio == wios */
		    attr = itree_get(row);
		    asmb->wios = atoll(attr);
		    itree_next(row);		/* 3. rblk == kread */
		    attr = itree_get(row);
		    asmb->kread = atoll(attr);
		    itree_next(row);		/* 4. wblk == kwritten */
		    attr = itree_get(row);
		    asmb->kwritten = atoll(attr);
	       } else if (itree_n(row) == 14) {
		    /* long form */
		    attr = itree_find(row , 3);	/* 1. rio == rios */
		    asmb->rios = atoll(attr);
		    attr = itree_find(row , 7);	/* 5. wio == wios */
		    asmb->wios = atoll(attr);
		    attr = itree_find(row , 5);	/* 3. rsect == kread */
		    asmb->kread = atoll(attr);
		    attr = itree_find(row , 9);	/* 7. wsect == kwritten */
		    asmb->kwritten = atoll(attr);
		    attr = itree_find(row, 6);	/* 4. ruse ==  read_svc_t */
		    asmb->read_svc_t = atoll(attr);
		    attr = itree_find(row, 10);	/* 8. wuse ==  write_svc_t */
		    asmb->write_svc_t = atoll(attr);
	       }
	  }
     }
}


/* gets the partition information from /proc */
void plinio_col_partitions(TREE *assemble, ITREE *lol) {

     /* /proc/partitions in kernel 2.4 has a layout similar to this below:-
      *
      * major minor  #blocks  name     rio rmerge rsect ruse wio wmerge wsect wuse running use aveq

      *    3     0    9820440 hda 51474 123156 1395050 765640 14455 33332 383560 1515930 0 622910 2302430
      *    3     1    2096451 hda1 289 0 290 90 0 0 0 0 0 90 90
      *    3     2          1 hda2 0 0 0 0 0 0 0 0 0 0 0
      *    3     5    2096451 hda5 47284 119311 1332786 534060 11563 29888 332792 1411160 0 392920 1966070
      *    3     6     192748 hda6 12 19 248 300 54 247 2408 3730 0 1360 4030
      *    3     7    5429938 hda7 3888 3826 61724 231170 2838 3197 48360 101040 0 246240 332220
      *   22     0     661412 hdc 0 0 0 0 0 0 0 0 0 0 0
      *
      *   rio:     number of read io operations (reads in output)
      *   rmerge:  don't know
      *   rsect:   don't know
      *   ruse:    don't know
      *   wio:     number of write io operations (writes in output)
      *   wmerge:  don't know
      *   wsecr:   don't know
      *   wuse:    don't know
      *   running: don't know
      *   use:     don't know
      *   aveq:    don't know
      */
     if (plinio_linuxversion == 24) {
	  ITREE *row;
	  void *attr;
	  struct plinio_assemble *asmb;

	  itree_traverse( lol ) {
	       row = itree_get( lol );
	       if (itree_n(row) < 4)
		    continue;
	       attr = itree_find(row , 3);	/* device name */
	       asmb = plinio_get_assemble_record(assemble, (char *) attr);
	       attr = itree_find(row , 2);	/* number of blocks */
	       asmb->size = atoll(attr);
	       /* now fill in the rest */
	  }
     }

     /* /proc/partitions in linux kernel 2.6 & 3.x just gives the block sizes
      * of each device and its major and minor number. It lokks like this:-
      *   1     0      32000 ram0
      *   1     1      32000 ram1
      *   ...etc...
      *   3     0   39070080 hda
      *   3     1      10240 hda1
      *   3     2          1 hda2
      *   3     5   10843843 hda5
      *   3     6    4120641 hda6
      *   3     7     401593 hda7
      *   3     8   23687811 hda8
      */
     if (plinio_linuxversion == 26 || plinio_linuxversion == 30) {
	  ITREE *row;
	  void *attr;
	  struct plinio_assemble *asmb;

	  itree_traverse( lol ) {
	       row = itree_get( lol );
	       if (itree_n(row) < 4)
		    continue;
	       attr = itree_find(row , 3);	/* device name */
	       asmb = plinio_get_assemble_record(assemble, (char *) attr);
	       attr = itree_find(row , 2);	/* number of blocks */
	       asmb->size = atof(attr) / 1024.0;
	  }
     }
}


/* gets the mount information from /etc/mnttab */
void plinio_col_mounts(TREE *assemble) {
     FILE *fp;
     struct mntent *ment;
     struct plinio_assemble *asmb;
     char *special;

     fp = setmntent("/etc/mtab", "r");
     if ( ! fp ) {
	  elog_printf(ERROR, "unable to open /etc/mtab");
	  return;
     }
     while ((ment = getmntent(fp))) {
	  /* device pathname */
	  if (strncmp(ment->mnt_fsname, "/dev/", 5) != 0)
	       continue;
	  special = ment->mnt_fsname + 5;	/* remove /dev/ part */
	  if (strcmp(special, "root") == 0) {
	       /* root needs to be translated to the disk name */
	       /* dont know how to do this yet, so the best thing 
		* is to not use the entry */
	       continue;
	  }
	  asmb = plinio_get_assemble_record(assemble, special);
	  asmb->mount  = xnstrdup(ment->mnt_dir);
	  asmb->fstype = xnstrdup(ment->mnt_type);
     }
     endmntent(fp);
}

/* statvfs each mount */
void  plinio_col_statvfs(TREE *assemble)
{
     struct plinio_assemble *asmb;
     struct statvfs statbuf;

     tree_traverse( assemble ) {
	  asmb = tree_get(assemble);
	  if (! asmb->mount)
	       continue;
	  if (statvfs(asmb->mount, &statbuf) == -1)
	       continue;
#if 0
	  elog_printf(WARNING, "statvfs fs=%s, f_bsize=%u, f_frsize=%u\n"
		      "f_blocks=%u, f_bfree=%u, f_bavail=%u", 
		      asmb->mount,
		      statbuf.f_bsize, statbuf.f_frsize, statbuf.f_blocks, 
		      statbuf.f_bfree, statbuf.f_bavail);
#endif
	  asmb->size = ( (long long) statbuf.f_blocks * statbuf.f_frsize) 
	                 / 1048576;
	  asmb->used = ( (long long) statbuf.f_blocks - statbuf.f_bavail ) 
	               * statbuf.f_frsize / 1048576.0;
	  asmb->reserved = (long long) (statbuf.f_bfree - statbuf.f_bavail) 
	                   * statbuf.f_frsize / 1024.0;
	  asmb->pc_used = ((float) ( statbuf.f_blocks - statbuf.f_bavail )
			   / statbuf.f_blocks) * 100.0;
	  if (asmb->pc_used > 100.0)
	       asmb->pc_used = 100.0;
     }
}


void plinio_derive(TABLE prev, TABLE cur) {}


/* return a pointer to the assembly record corresponding to device.
 * If it does not exist, allocate space, create an empty record in
 * the tree, initialise it with time and key, finally returning its address.
 * Record should be removed with the whole of the tree by using 
 * plinio_free_assemble_tree(). */
struct plinio_assemble *plinio_get_assemble_record(TREE *assemble_tree, 
						   char *device) {
     struct plinio_assemble *asmb;
     char *key;

     asmb = tree_find(assemble_tree, device);
     if (asmb == TREE_NOVAL) {
          /* instance has not been created */
	  key = xnstrdup(device);
          asmb = nmalloc(sizeof(struct plinio_assemble));
	  asmb->sample_t =  time(NULL);
	  asmb->device      = key;
	  asmb->mount       = NULL;
	  asmb->fstype      = NULL;
	  asmb->size        = 0.0;
	  asmb->reserved    = 0.0;
	  asmb->used        = 0.0;
	  asmb->pc_used     = 0.0;
	  asmb->kread       = 0.0;
	  asmb->kwritten    = 0.0;
	  asmb->rios        = 0.0;
	  asmb->wios        = 0.0;
	  asmb->read_svc_t  = 0.0;
	  asmb->write_svc_t = 0.0;
	  asmb->n_cur_ios   = 0;
	  asmb->cur_ios_t   = 0;
	  tree_add(assemble_tree, key, asmb);
     }

     return asmb;
}

/* translate the assemble structure to a table, leaving out rows that
 * are 'not interesting' (have 0's everywhere) */
void plinio_assemble_to_table(TREE *assemble_tree, TREE *last_tree, TABLE tab) {
     struct plinio_assemble *asmb, *last, delta;
     float svc;

     tree_traverse(assemble_tree) {
          asmb = tree_get(assemble_tree);

	  /* if we have a last tree, carry out deltas */
	  if (last_tree)
	       last = tree_find(last_tree, asmb->device);
	  else
	       last = TREE_NOVAL;

	  if (last != TREE_NOVAL) {
	       /* carry out the deltas between now and last */
	       delta.sample_t    = asmb->sample_t     - last->sample_t;
	       delta.kread       = (asmb->kread        - last->kread)
		                   / delta.sample_t;
	       delta.kwritten    = (asmb->kwritten     - last->kwritten)
		                   / delta.sample_t;
	       delta.rios        = (asmb->rios         - last->rios)
                                   / delta.sample_t;
	       delta.wios        = (asmb->wios         - last->wios)
                                   / delta.sample_t;
	       svc = asmb->read_svc_t  - last->read_svc_t;
	       delta.read_svc_t  = (svc == 0.0) ? 0.0 : 
		                        (svc * delta.rios) / delta.sample_t;
	       svc = asmb->write_svc_t - last->write_svc_t;
	       delta.write_svc_t = (svc == 0.0) ? 0.0 : 
		                        (svc * delta.wios) / delta.sample_t;
	  } else {
	       /* unable to report on any counter data, so just report on the
		* absolute information and set counters to 0 */
	       delta.kread       = 0;
	       delta.kwritten    = 0;
	       delta.rios        = 0;
	       delta.wios        = 0;
	       delta.wios        = 0;
	       delta.read_svc_t  = 0;
	       delta.write_svc_t = 0;
	  }

	  /* if a row is not 'interesting', try again */
	  if (asmb->size == 0.0 &&
	      asmb->used == 0.0 &&
	      asmb->reserved == 0.0 &&
	      asmb->pc_used == 0.0 &&
	      delta.kread == 0.0 &&
	      delta.kwritten == 0.0 &&
	      delta.rios == 0.0 &&
	      delta.wios == 0.0 &&
	      delta.read_svc_t == 0.0 &&
	      delta.write_svc_t == 0.0)
	  {
	       continue;
	  }

	  /* create a new row in the table and save data as text */
          table_addemptyrow(tab);
	  if (asmb->mount && *asmb->mount)
	       table_replacecurrentcell(tab, "id",      asmb->mount);
	  else
	       table_replacecurrentcell(tab, "id",      asmb->device);
	  table_replacecurrentcell(tab, "device",   asmb->device);
	  table_replacecurrentcell(tab, "mount",    asmb->mount);
	  table_replacecurrentcell(tab, "fstype",   asmb->fstype);
	  table_replacecurrentcell_alloc(tab, "size",
					 util_ftoa(asmb->size));
	  table_replacecurrentcell_alloc(tab, "used",
					 util_ftoa(asmb->used));
	  table_replacecurrentcell_alloc(tab, "reserved",
					 util_ftoa(asmb->reserved));
	  table_replacecurrentcell_alloc(tab, "pc_used",
					 util_ftoa(asmb->pc_used));
	  table_replacecurrentcell_alloc(tab, "kread",
					 util_ftoa(delta.kread));
	  table_replacecurrentcell_alloc(tab, "kwritten", 
					 util_ftoa(delta.kwritten));
	  table_replacecurrentcell_alloc(tab, "rios",
					 util_ftoa(delta.rios));
	  table_replacecurrentcell_alloc(tab, "wios",
					 util_ftoa(delta.wios));
	  table_replacecurrentcell_alloc(tab, "read_svc_t", 
					 util_ftoa(delta.read_svc_t));
	  table_replacecurrentcell_alloc(tab, "write_svc_t", 
					 util_ftoa(delta.write_svc_t));
#if 0
	  table_replacecurrentcell_alloc(tab, "n_cur_ios", 
					 util_ftoa(asmb->size));
	  table_replacecurrentcell_alloc(tab, "cur_ios_t", 
					 util_ftoa(asmb->size));
#endif
     }
}
/* free a tree of plinio_assemble records */
void  plinio_free_assemble_tree(TREE *assemble_tree) {
     struct plinio_assemble *asmb;

     if (!assemble_tree)
          return;
     tree_traverse(assemble_tree) {
          asmb = tree_get(assemble_tree);
	  nfree(asmb->device);
	  nfree(asmb->mount);
	  nfree(asmb->fstype);
	  nfree(asmb);
     }
     tree_destroy(assemble_tree);
}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     plinio_init();
     plinio_collect();
     if (argc > 1)
	  buf = table_outtable(plinio_tab);
     else
	  buf = table_print(plinio_tab);
     puts(buf);
     nfree(buf);
     table_destroy(plinio_tab);
     exit(0);
}

#endif /* TEST */

#endif /* linux */
