/*
 * Solaris I/O probe for iiab
 * Nigel Stuckey, December 1998, January, February, July 1999 & March 2000
 *
 * Copyright System Garden Ltd 1998-2001. All rights reserved.
 */

#if __svr4__

#include <stdio.h>
#include <strings.h>
#include <sys/mnttab.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <string.h>
#include "probe.h"
#include "psolio.h"

/* Solaris specific routines */
#include <kstat.h>		/* Solaris uses kstat */

/* table constants for system probe */
struct probe_sampletab psolio_cols[] = {
     {"id",	    "", "str",  "abs", "", "1","mount or kernel name"},
     {"device",	    "", "str",  "abs", "", "", "device name"},
     {"mount",	    "", "str",  "abs", "", "", "mount path of device"},
     {"fstype",	    "", "str",  "abs", "", "", "filesystem type"},
     {"size",	    "", "nano", "abs", "", "", "size of file system (MBytes)"},
     {"used",	    "", "nano", "abs", "", "", "space used on file system (MBytes)"},
     {"reserved",   "", "nano", "abs", "", "", "space reserved on file system (MBytes)"},
     {"pc_used","%used","nano", "abs", "", "", "% used of non-reserved space"},
     {"kread",	    "", "nano", "abs", "", "", "kBytes read per second"},
     {"kwritten",   "", "nano", "abs", "", "", "kBytes written per second"},
     {"rios",	    "", "nano", "abs", "", "", "number of read operations "
                                               "per second"},
     {"wios",	    "", "nano", "abs", "", "", "number of write operations "
                                               "per second"},
     {"wait_t",	    "", "nano", "abs", "", "", "pre-service wait time per "
                                               "second "},
     {"wait_len_t", "", "nano", "abs", "", "", "cumulative wait length*time "
                                               "product"},
     {"run_t",	    "", "nano", "abs", "", "", "service run time per second"},
     {"run_len_t",  "", "nano", "abs", "", "", "cumulative run length*time "
                                               "product"},
     {"wait_cnt",   "", "nano", "abs", "", "", "wait count"},
     {"run_cnt",    "", "nano", "abs", "", "", "run count"},
     PROBE_ENDSAMPLE
};

/* List of colums to diff */
struct probe_rowdiff psolio_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *psolio_getcols()    {return psolio_cols;}
struct probe_rowdiff   *psolio_getrowdiff() {return psolio_diffs;}
char                  **psolio_getpub()     {return NULL;}

/* last set of samples taken. key is id, value is struct psolio_assemble */
TREE *psolio_last_data=NULL;

/* path to inst list, key is the full solaris device name 
 * (/devices/sbus@1f,0/SUNW,fas@e,8800000/sd@0,0:a), value is the short 
 * instance id (sd0,a) */
TREE *psolio_p2i;

/* short dev to inst list, key is the short device name (/dev/dsk/c0t0d0s0), 
 * value is the short instance id (sd0,a) */
TREE *psolio_d2i;

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
 * Initialise probe for solaris I/O information
 */
void psolio_init() {
     psolio_p2i = psolio_path_to_inst("/etc/path_to_inst");
     psolio_d2i = tree_create();
}

/* shut down probe */
void psolio_fini() {
     psolio_free_assemble_tree(psolio_last_data);
     tree_clearoutandfree(psolio_p2i);
     tree_clearoutandfree(psolio_d2i);
}

/*
 * Solaris specific routines
 */
void psolio_collect(TABLE tab) {
     kstat_ctl_t *kc;
     kstat_t *ksp;
     TREE *current_data;	/* keyed by id (nmalloced), 
				 * value is a psolio_assemble struct. 
				 * Each element represents one device */

     current_data = tree_create();

     /* process kstat data of type KSTAT_TYPE_RAW */
     kc = kstat_open();

     for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
	  if (ksp->ks_type == KSTAT_TYPE_IO) {
	       /* collect row stats */
	       psolio_col_io(current_data, kc, ksp);
	  }

     kstat_close(kc);

     /* join in the mount information */
     psolio_col_mounts(current_data);

     /* now translate the structure into the TABLE, carrying out delta
      * operations if required */
     psolio_assemble_to_table(current_data, psolio_last_data, tab);
     if (psolio_last_data)
          psolio_free_assemble_tree(psolio_last_data);
     psolio_last_data = current_data;
}

/* gets an I/O structure out of the kstat block */
void psolio_col_io(TREE *assemble, kstat_ctl_t *kc, kstat_t *ksp) {
     kstat_io_t *s;
     struct psolio_assemble *asmb;

     /* read from kstat */
     kstat_read(kc, ksp, NULL);
     if (ksp->ks_data) {
          s = ksp->ks_data;
     } else {
          elog_send(ERROR, "null kdata");
	  return;
     }

     /* get record for this device */
     asmb = psolio_get_assemble_record(assemble, ksp->ks_name);

     /* assign the information I know */
     /* asmb->id assigned by psolio_get_assemble_record() */
     asmb->sample_t   = ksp->ks_snaptime;
     asmb->kread      = s->nread/1024;
     asmb->kwritten   = s->nwritten/1024;
     asmb->rios       = s->reads;
     asmb->wios       = s->writes;
     asmb->wait_t     = s->wtime/1000000000;
     asmb->wait_len_t = s->wlentime/1000000000;
     asmb->wait_cnt   = s->wcnt;
     asmb->run_t      = s->rtime/1000000000;
     asmb->run_len_t  = s->rlentime/1000000000;
     asmb->run_cnt    = s->rcnt;
}

/* Collect data from the mnttab file and size each filesystem */
void  psolio_col_mounts(TREE *assemble)
{
     FILE *fd;
     struct mnttab m;
     struct statvfs statbuf;
     int r;
     char *inst;
     struct psolio_assemble *asmb;

     fd = fopen("/etc/mnttab", "r");
     r = getmntent(fd, &m);
     while (r != -1) {
	  /* find instance name */
	  if (strcmp(m.mnt_fstype, "nfs") == 0) {
	       inst = psolio_nfsopts_to_inst(m.mnt_mntopts);
	  } else if (strncmp(m.mnt_special, "/", 1) == 0) {
	       inst = psolio_dev_to_inst(psolio_p2i, psolio_d2i, 
					 m.mnt_special);
	  } else {
	       inst = TREE_NOVAL;
	  }

	  /* save name */
	  if (inst != TREE_NOVAL) {
	       /* find the partial struct */
	       asmb = tree_find(assemble, inst);
	       if (asmb == TREE_NOVAL) {
		    elog_printf(ERROR, "unable to find %s", inst);
	       } else {
		    /* add data to assembly */
		    asmb->device = xnstrdup(m.mnt_special);
		    asmb->mount  = xnstrdup(m.mnt_mountp);
		    asmb->fstype = xnstrdup(m.mnt_fstype);

		    /* stat the file system for sizes */
		    if (statvfs(m.mnt_mountp, &statbuf) == 0) {
#if 0
			 elog_printf(WARNING, "statvfs fs=%s, f_bsize=%u, "
				     "f_frsize=%u\n f_blocks=%u, f_bfree=%u, "
				     "f_bavail=%u", 
				     asmb->mount,
				     statbuf.f_bsize, statbuf.f_frsize, 
				     statbuf.f_blocks, statbuf.f_bfree, 
				     statbuf.f_bavail);
#endif
			 asmb->size = ( (long long) statbuf.f_blocks 
					* statbuf.f_frsize) / 1048576;
			 asmb->used = ( (long long) statbuf.f_blocks 
					- statbuf.f_bavail ) 
			      * statbuf.f_frsize / 1048576.0;
			 asmb->reserved = (long long) (statbuf.f_bfree 
						       - statbuf.f_bavail) 
			      * statbuf.f_frsize / 1024.0;
			 asmb->pc_used = ((float) ( statbuf.f_blocks 
						    - statbuf.f_bavail )
					  / statbuf.f_blocks) * 100.0;
		    } else {
			 elog_printf(ERROR, "unable to get statvfs details "
				     "on %s", m.mnt_special);
		    }
	       }
	  } else {
	       elog_printf(DEBUG, "instance not found - spec=%s mount=%s", 
			   m.mnt_special, 
			   m.mnt_mountp);
 	  }
	  r = getmntent(fd, &m);
     }
     fclose(fd);
}

void psolio_derive(TABLE prev, TABLE cur) {}

/* return a pointer to the assembly record corresponding to device.
 * If it does not exist, allocate space, create an empty record in
 * the tree, initialise it with time and key, finally returning its address.
 * Record should be removed with the whole of the tree by using 
 * psolio_free_assemble_tree(). */
struct psolio_assemble *psolio_get_assemble_record(TREE *assemble_tree, 
						   char *id) {
     struct psolio_assemble *asmb;
     char *key;

     asmb = tree_find(assemble_tree, id);
     if (asmb == TREE_NOVAL) {
          /* instance has not been created */
	  key = xnstrdup(id);
          asmb = nmalloc(sizeof(struct psolio_assemble));
	  asmb->sample_t   = 0;
	  asmb->id         = key;
	  asmb->device     = NULL;
	  asmb->mount      = NULL;
	  asmb->fstype     = NULL;
	  asmb->size       = 0;
	  asmb->used       = 0;
	  asmb->reserved   = 0;
	  asmb->pc_used    = 0.0;
	  asmb->kread      = 0.0;
	  asmb->kwritten   = 0.0;
	  asmb->rios       = 0.0;
	  asmb->wios       = 0.0;
	  asmb->wait_t     = 0.0;
	  asmb->wait_len_t = 0.0;
	  asmb->wait_cnt   = 0;
	  asmb->run_t      = 0.0;
	  asmb->run_len_t  = 0.0;
	  asmb->run_cnt    = 0;
	  tree_add(assemble_tree, key, asmb);
	  /* elog_printf(DEBUG, "new instance %s", key);*/
     }

     return asmb;
}

/* translate the assemble structure to a table */
void psolio_assemble_to_table(TREE *assemble_tree, TREE *last_tree, TABLE tab) {
     struct psolio_assemble *asmb, *last;
     float svc;
     hrtime_t delta_hrt;
     float delta_t;

     tree_traverse(assemble_tree) {
          asmb = tree_get(assemble_tree);

	  /* if we have a last tree, carry out deltas */
	  if (last_tree)
	       last = tree_find(last_tree, asmb->id);
	  else
	       last = TREE_NOVAL;

	  if (last != TREE_NOVAL) {
	       /* carry out the deltas between now and last */
	       /* timings , transform to nanoseconds */
	       delta_hrt = asmb->sample_t - last->sample_t;
	       delta_t = delta_hrt / 1000000000;
	       if (delta_t == 0.0)
		    delta_t = 1;
	  } else {
	       /* No last data, dont carry out the reporting */
	       return;
	  }

          table_addemptyrow(tab);

	  if (asmb->mount && *(asmb->mount))
	       table_replacecurrentcell_alloc(tab, "id",  asmb->mount);
	  else
	       table_replacecurrentcell_alloc(tab, "id",  asmb->id);
	  table_replacecurrentcell_alloc(tab, "device",   asmb->device);
	  table_replacecurrentcell_alloc(tab, "mount",    asmb->mount);
	  table_replacecurrentcell_alloc(tab, "fstype",   asmb->fstype);
	  table_replacecurrentcell_alloc(tab, "size",
					 util_i32toa(asmb->size));
	  table_replacecurrentcell_alloc(tab, "used",
					 util_i32toa(asmb->used));
	  table_replacecurrentcell_alloc(tab, "reserved",
					 util_i32toa(asmb->reserved));
	  table_replacecurrentcell_alloc(tab, "pc_used",
					 util_ftoa  (asmb->pc_used));
	  table_replacecurrentcell_alloc(tab, "kread",
		util_ftoa((asmb->kread - last->kread)/delta_t));
	  table_replacecurrentcell_alloc(tab, "kwritten", 
		util_ftoa((asmb->kwritten - last->kwritten)/delta_t));
	  table_replacecurrentcell_alloc(tab, "rios",
		util_ftoa((asmb->rios - last->rios)/delta_t));
	  table_replacecurrentcell_alloc(tab, "wios",
		util_ftoa((asmb->wios - last->wios)/delta_t));
	  table_replacecurrentcell_alloc(tab, "wait_t", 
		util_ftoa((asmb->wait_t - last->wait_t)/delta_t));
	  table_replacecurrentcell_alloc(tab, "wait_len_t", 
		util_ftoa((asmb->wait_len_t - last->wait_len_t)/delta_t));
	  table_replacecurrentcell_alloc(tab, "run_t", 
		util_ftoa((asmb->run_t - last->run_t)/delta_t));
	  table_replacecurrentcell_alloc(tab, "run_len_t", 
		util_ftoa((asmb->run_len_t - last->run_len_t)/delta_t));
	  table_replacecurrentcell_alloc(tab, "wait_cnt", 
		util_ftoa((asmb->wait_cnt - last->wait_cnt)/delta_t));
	  table_replacecurrentcell_alloc(tab, "run_cnt", 
		util_ftoa((asmb->run_cnt - last->run_cnt)/delta_t));
     }
}

/* free a tree of psolio_assemble records */
void  psolio_free_assemble_tree(TREE *assemble_tree) {
     struct psolio_assemble *asmb;

     if (!assemble_tree)
          return;
     tree_traverse(assemble_tree) {
          asmb = tree_get(assemble_tree);
	  if (!asmb)		continue;
	  if (asmb->id)		nfree(asmb->id);
	  if (asmb->device)	nfree(asmb->device);
	  if (asmb->mount)	nfree(asmb->mount);
	  if (asmb->fstype)	nfree(asmb->fstype);
	  nfree(asmb);
     }
     tree_destroy(assemble_tree);
}


/* Read the file /etc/path_to_inst to create a list from full device name
 * to a short instance name. Free the resutling list with 
 * tree_clearoutandfree() */
TREE *psolio_path_to_inst(char *fname)
{
     char *pathfile, *pt, *pt2, *key, *val, part[2], slice;
     int keylen;
     TREE *p2i;

     p2i = tree_create();
     pathfile = probe_readfile(fname);
     pt = strtok(pathfile, "\n");
     while(pt) {
	  if (*pt == '\"') {
	       /* extract the key */
	       pt++;
	       pt2 = pt + strcspn(pt, "\"");
	       *pt2 = '\0';
	       key = util_strjoin("/devices", pt, NULL);

	       /* make the instance from remaining tokens */
	       pt2 += 2;
	       pt  = pt2 + strcspn(pt2, "\"") + 1;
	       pt[strcspn(pt, "\"")] = '\0';
	       pt2[strcspn(pt2, " ")] = '\0';
	       val = util_strjoin(pt, pt2, NULL);

	       /* store in the list */
	       tree_add(p2i, key, val);
	  }
	  pt = strtok(NULL, "\n");
     }
     nfree(pathfile);
     return p2i;
}

/*
 * Return the short instance name when given the device name (/dev/<name>)
 * and cache the entry in a list to speed up subsequent fetches.
 * Each fetch may involve disk access to resolve the name.
 * Returns TREE_NOVAL if there no association or the instance name if 
 * successful (which should NOT be nfree()ed).
 */
char *psolio_dev_to_inst(TREE *p2i, TREE *d2i, char *devname)
{
     char devicebuf[256], *devicename, *inst, *pt, *myinst;
     char part[2];
     int len, slice=-1;

     /* if lookup is cached, return it */
     if (tree_find(d2i, devname) != TREE_NOVAL)
	  return tree_get(d2i);	/* success !! */

     /* /dev/<name> for storage in solaris is always a link to /device.
      * Open it, collect the link data and cache the relationship.
      * The snag is, however, that only the first partition is stored in
      * the p2i list, thus we need to convert that instance. */
     len = readlink(devname, devicebuf, 255);
     if (len == -1) {
	  /* no link, so it can't be a device */
	  elog_printf(DEBUG, "readlink of %s failed %d: %s", devname,
		      errno, strerror(errno));
     } else {
	  devicebuf[len] = '\0';
	  devicename = devicebuf+5;	/* chop off leading '../..' */
	  if ((pt = strrchr(devicename, ':')) == NULL) {
	       part[0] = '\0';
	  } else {
	       part[0] = *(pt+1);
	       part[1] = '\0';
	       *pt = '\0';		/* chop off partition */
	  }
	  inst = tree_find(p2i, devicename);
	  if (inst == TREE_NOVAL) {
	       elog_printf(ERROR, "device path %s not found in p2i", 
			   devicename);
	  } else {
	       /* its new, but its still a success !! */
	       myinst = util_strjoin(inst, ",", part, NULL);
	       tree_add(d2i, xnstrdup(devname), myinst);
	       return myinst;
	  }
     }

     return TREE_NOVAL;
}

/*
 * Find the device string from an NFS mnttab option and turn it into 
 * an NFS instance id. Returns TREE_NOVAL if unable to find the dev entry.
 */
char *psolio_nfsopts_to_inst(char *nfsopts)
{
     int r=0, dev;
     static char tdev[20];
     char *nfsdev;

     nfsdev = strstr(nfsopts, "dev=");
     if (nfsdev)
	  r = sscanf(nfsdev, "dev=%x", &dev);
     if (r) {
	  snprintf(tdev, 20, "nfs%d", dev & 0x3ffff);
	  return tdev;
     } else {
	  return TREE_NOVAL;
     }
}


#if TEST

/*
 * Main function
 */
main(int argc, char arvg[]) {
     char *buf;

     psolio_init();
     psolio_collect();
     if (argc > 1)
	  buf = table_outtable(psolio_tab);
     else
	  buf = table_print(psolio_tab);
     puts(buf);
     nfree(buf);
     table_destroy(psolio_tab);
     exit(0);
}

#endif /* TEST */

#endif /* __svr4__ */
