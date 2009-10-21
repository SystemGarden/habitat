/*
 * Ringstore low level storage using an abstracted interface
 * GDBM (GNU DBM) implementataion
 *
 * Nigel Stuckey, September 2001 using code from January 1998 onwards
 * Copyright System Garden Limited 1998-2001. All rights reserved.
 */

#include <errno.h>
#include <sys/types.h>
#if defined(linux)
  #include <sys/statfs.h>
#else
  #include <sys/statvfs.h>
#endif
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "nmalloc.h"
#include "elog.h"
#include "util.h"
#include "rs.h"
#include "rs_gdbm.h"

/* private functional prototypes */

const struct rs_lowlevel rs_gdbm_method = {
     rs_gdbm_init,          rs_gdbm_fini,          rs_gdbm_open,
     rs_gdbm_close,         rs_gdbm_exists,        rs_gdbm_lock,
     rs_gdbm_unlock,        rs_gdbm_read_super,    rs_gdbm_write_super,
     rs_gdbm_read_rings,    rs_gdbm_write_rings,   rs_gdbm_read_headers,
     rs_gdbm_write_headers, rs_gdbm_read_index,    rs_gdbm_write_index, 
     rs_gdbm_rm_index,      rs_gdbm_append_dblock, rs_gdbm_read_dblock, 
     rs_gdbm_expire_dblock, rs_gdbm_read_substr,   rs_gdbm_read_value,
     rs_gdbm_write_value,   rs_gdbm_checkpoint,    rs_gdbm_footprint,
     rs_gdbm_dumpdb,        rs_gdbm_errstat
};

/* statics */
int   rs_gdbm_isinit=0;			/* initialised */
int   rs_gdbm_errno=0;			/* unknown status */
char *rs_gdbm_errstr[] = {"unknown"};	/* unknown status */

/* initialise */
void   rs_gdbm_init         ()
{
     rs_gdbm_isinit=1;
}


/* finalise */
void   rs_gdbm_fini         ()
{
}


/*
 * Open the gdbm file to support the ringstore low level interface.
 * If create is set, call GDBM with the filename and the mode and create
 * if not already there. Otherwise, just attempt to open the file for
 * reading.
 * Because of the way GDBM works, the file is not kept open and this call
 * only opens the file to check for or write the superblock. The details are
 * held in the low level descriptor so that calls to rs_gdbm_lock()
 * actually open the GDBM file for work with the appropreate lock
 * which is completed with rs_gdbm_unlock().
 * Returns the low level descriptor if successful or NULL otherwise.
 */
RS_LLD rs_gdbm_open         (char *filename,	/* name of GDBM file */
			     mode_t perm, 	/* unix file creation perms */
			     int create		/* 1=create */ )
{
     GDBM_FILE gdbm;
     RS_GDBMD rs;
     RS_SUPER superblock;

     if (! rs_gdbm_isinit)
          elog_die(FATAL, "rs_gdbm unitialised");

     /* Check the file exists and if so, is it a valid ringstore format */
     superblock = rs_gdbm_read_super_file(filename);
     if ( ! superblock ) {
	  if (access(filename, F_OK) == 0) {
	       /* a non-GDBM file exists, so we leave it alone */
	       elog_printf(DIAG, "%s exists but is not a GDBM file; "
			   "refuse open", filename);
	       return NULL;
	  }

	  /* readers go no further */
	  if ( ! create ) {
	       /* file does not exist and won't create it */
	       elog_printf(DIAG, "db file %s does not exist", filename);
	       return NULL;
	  }

	  /* File does not exist. Attempt to
	   * create the GDBM and within it the superblock */
	  gdbm = gdbm_open(filename, 0, GDBM_WRCREAT, perm, rs_gdbm_dberr);
	  if (gdbm) {
	       /* the database was created; create the superblock
		* from the base class library routine and write it
		* out to the empty GDBM */
	       superblock = rs_create_superblock();
	       if ( ! rs_gdbm_write_super_fd(gdbm, superblock)) {
		    elog_printf(ERROR, "unable to write superblock to %s", 
				filename);
		    gdbm_close(gdbm);
		    return NULL;
	       }
	       gdbm_close(gdbm);
	  } else {
	       /* unable to write to db, assume we have hit a race
		* condition and it has already being done */
	       superblock = rs_gdbm_read_super_file(filename);
	       if ( ! superblock ) {
		    elog_printf(ERROR, "unable to re-read superblock "
				"from %s after race", filename);
		    return NULL;
	       }
	  }
     }

     /* The GDBBM db now contains a superblock.
      *	Create, complete and return the descriptor */
     rs = xnmalloc(sizeof(struct rs_gdbm_desc));
     rs->lld_type = RS_LLD_TYPE_GDBM;	/* descriptor type */
     rs->name = xnstrdup(filename);	/* file name */
     rs->mode = perm;			/* gdbm file descriptor */
     rs->ref = NULL;			/* working open GDBBM descriptor */
     rs->super = superblock;		/* super block structure */
     rs->lastkey = NULL;		/* no traversal */
     rs->lock = RS_UNLOCK;		/* unlocked */
     rs->inhibitlock = 0;		/* inhibit lock flag */

     return rs;
}


/* Close and free up an existing rs_gdbm descritpor */
void   rs_gdbm_close        (RS_LLD lld	/* RS generic low level descriptor */)
{
     RS_GDBMD rs;

     rs = rs_gdbmd_from_lld(lld);
     if (rs->lock != RS_UNLOCK)			/* unlock if needed */
	  rs_gdbm_unlock(rs);
     nfree(rs->name);
     rs_free_superblock(rs->super);
     nfree(rs);
}


/* Checks to see if the filename is a RS_GDBM file and can carry out the
 * what is required in 'todo'.
 * A return of 0 means yes, non-0 means no of which can indicate several 
 * states. 
 * 1=the file exists but is not a GDBM file, 
 * 2=the file does not exist, 
 * 3=the file exists but would be unable to carry out 'todo' */
int    rs_gdbm_exists       (char *filename, enum rs_db_writable todo)
{
     RS_SUPER superblock;

     /* Check the file exists and if so, is it a valid ringstore format */
     superblock = rs_gdbm_read_super_file(filename);
     if ( ! superblock ) {
	  if (access(filename, F_OK) == 0) {
	       /* a non-GDBM file exists, so we leave it alone */
	       elog_printf(DIAG, "%s exists but is not a GDBM file", filename);
	       return 1;	/* bad */
	  } else {
	       /* no file exists */
	       elog_printf(DIAG, "%s does not exist", filename);
	       return 2;	/* bad */
	  }
     }

     rs_free_superblock(superblock);
     if (todo == RS_RW && access(filename, W_OK) != 0) {
	  /* unable to write as asked */
	  elog_printf(DIAG, "GDBM %s exists but unable to write as asked", 
		      filename);
	  return 1;	/* bad */
     }

     return 0;	/* good */
}


/*
 * Lock the GDBM db for work and keep it locked until rs_gdbm_unlock()
 * is called. If successive calls are made, the locks will be converted
 * to the newest's request.
 * A lock can be read-only (RS_RDLOCK) or read-write (RS_RWLOCK) and will 
 * repeatedly poll with an intervening time delay to wait until the db 
 * becomes free. The number of times to poll and delay as currently 
 * untunable. An alternative set of locks can be specified to avoid the
 * polling behaviour (RS_RDLOCKNOW and RS_WRLOCKNOW).
 * In reality, the lock calls gdbm_open with the required flag, so even
 * though the RS_GDBMD descriptor may be in the 'open' state, the
 * underlying GDBM file stays firmly closed.
 * Lock conversion (from RD to RW) is supperted but does nothing
 * magical: it unlocks before locking for RW. If the lock fails, then
 * your original read lock will be lost.
 * Returns 1 for success or 0 for failure
 */
int    rs_gdbm_lock(RS_LLD lld,		/* RS generic low level descriptor */
		    enum rs_db_lock rw, /* lock to take on db */
		    char *where		/* caller id */)
{
     RS_GDBMD rs;

     /* param checking */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not opened before locking");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->inhibitlock)
          return 1;		/* inhibit causes a success */
     if (rs->lock == RS_WRLOCK && (rw == RS_WRLOCK || rw == RS_WRLOCKNOW)) {
	  elog_printf(ERROR, "%s already have write lock; do nothing",
		      where);
	  return 1;		/* success */
     }
     if (rs->lock == RS_RDLOCK && (rw == RS_RDLOCK || rw == RS_RDLOCKNOW)) {
	  elog_printf(ERROR, "%s already have read lock; do nothing",
		      where);
	  return 1;		/* success */
     }

     /* lock escalation rd-rw */
     if (rs->lock == RS_RDLOCK && (rw == RS_WRLOCK || rw == RS_WRLOCKNOW)) {
	  /* unable to go cleanly from one to another, so we have to
	   * close first, then wee can wait in line to open the file.
	   * If we fail, then the underlying GDBM will always be
	   * closed.*/
	  rs_gdbm_dbclose(rs);
     }

     /* obtain lock and record in descriptor */
     if (!rs_gdbm_dbopen(rs, where, rw))
	  return 0;	/* failure */
     switch (rw) {
     case RS_WRLOCK:
     case RS_WRLOCKNOW:
     case RS_CRLOCKNOW:
	  rs->lock = RS_WRLOCK;
	  break;
     case RS_RDLOCK:
     case RS_RDLOCKNOW:
	  rs->lock = RS_RDLOCK;
	  break;
     default:
          elog_printf(DEBUG, "%s called with mode=%d", where, rw);
	  return 0;
     }

     return 1;	/* success */
}


/* Unlock the GDBM, which actually closes the underlying file */
void   rs_gdbm_unlock       (RS_LLD lld	/* RS generic low level descriptor */)
{
     RS_GDBMD rs;

     /* param checking */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not opened before unlocking");
	  return;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->inhibitlock)
          return;		/* inhibit causes a success */
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return;
     }

     rs_gdbm_dbclose(rs);
     rs->lock = RS_UNLOCK;
}

/* 
 * Read the superblock from an opened, locked GDBM file and return
 * a superblock structure if successful or NULL otherwise.
 * Replaces the superblock copy in the descriptor as well, to keep 
 * it up-to-date.
 * Free superblock with rs_free_superblock().
 */
RS_SUPER rs_gdbm_read_super (RS_LLD lld	/* RS generic low level descriptor */)
{
     RS_GDBMD rs;
     RS_SUPER super;

     /* param checking */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open to read superblock");
	  return NULL;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return NULL;
     }

     /* read and cache superblock */
     super = rs_gdbm_read_super_fd(rs->ref);
     if ( ! super )
	  return NULL;

     rs_free_superblock(rs->super);
     rs->super = rs_copy_superblock(super);

     return super;
}


/* 
 * Read the superblock from an unopened GDBM file that is uninitialised
 * (or being investigated) by rs_gdbm and return
 * a superblock structure if successful or NULL otherwise.
 * Use rs_gdbm_free_super() to clear the structure after use.
 * The file will be open and closed in this call.
 */
RS_SUPER rs_gdbm_read_super_file (char *dbname	/* name of GDBM file */)
{
     GDBM_FILE db;
     RS_SUPER super;

     /* open the GDBM using a direct low level call */
     if (access(dbname, R_OK) == -1)
	  return NULL;
     db = gdbm_open(dbname, 0, GDBM_READER || GDBM_NOLOCK, 
		    RS_GDBM_READ_PERM, rs_gdbm_dberr);
     if (!db) {
	  elog_printf(DIAG, "unable to open %s as GDBM file (%d:%s)", 
		      dbname, gdbm_errno, gdbm_strerror(gdbm_errno));
	  return NULL;
     }

     super = rs_gdbm_read_super_fd(db);
     gdbm_close(db);

     return super;
}

/* 
 * Read the superblock from an opened GDBM file that is uninitialised
 * (or being investigated) by rs_gdbm and return
 * a superblock structure if successful or NULL otherwise.
 * Use rs_gdbm_free_super() to clear the structure after use.
 */
RS_SUPER rs_gdbm_read_super_fd   (GDBM_FILE fd	/* GDBM file descriptor */ )
{
     datum d, k;
     RS_SUPER super;
     char *magic;

     /* attempt to read an existing ringstore superblock */
     k.dptr = RS_GDBM_SUPERNAME;
     k.dsize = RS_GDBM_SUPERNLEN;
     d = gdbm_fetch(fd, k);
     if (d.dptr == NULL)
	  return NULL;

     /* check the magic string */
     magic = util_strtok_sc(d.dptr, "|");
     if (strcmp(magic, RS_GDBM_MAGIC) != 0) {
	  free(d.dptr);		/* malloc memory */
	  return NULL;
     }

     /* break down the superblock string representation into the
      * superblock structure */
     super = xnmalloc(sizeof(struct rs_superblock));     
     super->version    = strtol(  util_strtok_sc(NULL, "|"), NULL, 10);
     super->created    = strtol(  util_strtok_sc(NULL, "|"), NULL, 10);
     super->os_name    = xnstrdup(util_strtok_sc(NULL, "|"));
     super->os_release = xnstrdup(util_strtok_sc(NULL, "|"));
     super->os_version = xnstrdup(util_strtok_sc(NULL, "|"));
     super->hostname   = xnstrdup(util_strtok_sc(NULL, "|"));
     super->domainname = xnstrdup(util_strtok_sc(NULL, "|"));
     super->machine    = xnstrdup(util_strtok_sc(NULL, "|"));
     super->timezone   = strtol(  util_strtok_sc(NULL, "|"), NULL, 10);
     super->generation = strtol(  util_strtok_sc(NULL, "|"), NULL, 10);
     super->ringcounter= strtol(  util_strtok_sc(NULL, "|"), NULL, 10);
     free(d.dptr);	/* malloc memory */

     return super;
}


/*
 * Write the superblock to an opened, locked GDBM file and return 1
 * if successful or 0 for error.
 * If the write is successful, the copy in the descrptor is updated
 * with the new version.
 */
int    rs_gdbm_write_super(RS_LLD lld,    /* RS generic low level descriptor */
			   RS_SUPER super /* superblock */)
{
     RS_GDBMD rs;
     int r;

     /* param checking */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open to write superblock");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     /* write and if successful, update the descriptor's superblock cache */
     r = rs_gdbm_write_super_fd(rs->ref, super);
     if (r) {
	  rs_free_superblock(rs->super);
	  rs->super = rs_copy_superblock(super);
     }

     return r;
}


/*
 * Open the GDBM file for writing and store the given superblock.
 * It will not create the file and will return 1 for success or 
 * 0 for  failure.
 */
int    rs_gdbm_write_super_file  (char *dbname,	 /* GDBBM file name */
				  mode_t perm,   /* file create permissions */
				  RS_SUPER super /* superblock */ )
{
     GDBM_FILE db;
     int r;

     /* open the GDBM using a direct low level call */
     db = gdbm_open(dbname, 0, GDBM_WRITER, perm, rs_gdbm_dberr);
     if (!db) {
	  elog_printf(DIAG, "unable to open %s as GDBM file for writing",
		      dbname);
	  return 0;
     }

     r = rs_gdbm_write_super_fd(db, super);
     gdbm_close(db);

     return r;
}


/*
 * Write a superblock to an opened GDBM file that is not initialised
 * in the normal way (or is in the process of initialising). Therfore
 * it does not use the RS_GDBMD structure.
 * Return 1 for successfully written superblock or 0 for an error.
 */
int    rs_gdbm_write_super_fd (GDBM_FILE fd,	/* GDBM file descriptor */
			       RS_SUPER super	/* superblock */)
{
     char superblock[RS_GDBM_SUPERMAX];
     int r;
     datum k, d;
     d.dsize = 1+sprintf(superblock, "%s|%d|%u|%s|%s|%s|%s|%s|%s|%d|%d|%d", 
			 RS_GDBM_MAGIC, super->version, super->created,
			 super->os_name, super->os_release, super->os_version,
			 super->hostname, super->domainname, super->machine,
			 super->timezone, super->generation, 
			 super->ringcounter);
     d.dptr = superblock;
     k.dptr = RS_GDBM_SUPERNAME;
     k.dsize = RS_GDBM_SUPERNLEN;
     r = gdbm_store(fd, k, d, GDBM_REPLACE);
     if (r) {
	  /* error: unable to store superblock */
	  elog_printf(ERROR, "unable to store superblock");
	  return 0;	/* bad */
     }
     return 1;		/* good */
}


/*
 * Read the ring directory and return a table of existing rings in the
 * existing and locked GDBM. The table contains a row per ring, with
 * the following columns (if successful).
 *   name   ring name
 *   id     ring id, used for low level storing
 *   long   long name of ring
 *   about  description of ring
 *   size   size of ring in slots or 0 for unlinited
 *   dur    duration of samples
 * Returns NULL if there is an error, or the TABLE otherwise.
 */
TABLE rs_gdbm_read_rings   (RS_LLD lld	/* RS generic low level descriptor */)
{
     char *ringdir;
     int length;
     TABLE rings;
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return NULL;
     }

     /* read in the ring directory and parse */
     ringdir = rs_gdbm_dbfetch(rs, RS_GDBM_RINGDIR, &length);

     /* create table from ring buffer text */
     rings = table_create_a(rs_ringdir_hds);
     if (rings) {
          table_scan(rings, ringdir, "\t", TABLE_SINGLESEP, TABLE_NOCOLNAMES, 
		     TABLE_NORULER);
	  table_freeondestroy(rings, ringdir);
     }

     /* return the table regardless of success. 
      * If a ring directory was found, then the table will have some 
      * rows, otherwise it will not */
     return rings;
}


/*
 * Save the rings held in the table back out to disk.
 * The table should be of the format returned by rs_gdbm_read_rings()
 * and generally will have been modified to add rings, remove rings
 * or change their details.
 * The contents of the table goes on to form the new ring directory.
 * The GDBM should be open for writing. 
 * Returns 1 for success or 0 for failure.
 */
int    rs_gdbm_write_rings (RS_LLD lld, /* RS generic low level descriptor */
			    TABLE rings /* table of rings */)
{
     char *ringdir;
     int length, r;
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     /* convert table to a string and write it to the GDBM */
     ringdir = table_outbody(rings);
     if (! ringdir)
	  ringdir = xnstrdup("");
     length = strlen(ringdir)+1;	/* include \0 */
     r = rs_gdbm_dbreplace(rs, RS_GDBM_RINGDIR, ringdir, length);
     nfree(ringdir);

     return r;
}


/*
 * Read the table of headers into a single list and return.
 * The keys are the hash keys that correspond to the data headers, 
 * the values are the header and info strings from the data tables, 
 * as produced by util_join(table_outheader(), "\n", table_outinfo(), NULL).
 * Returns an empty list if there are no headers.
 * The returned list has its values nmalloc()ed and, therefore, takes 
 * a little time to create.
 * However, it means that new items can be added or existing items
 * can be removed easily without leaking memory or ackwardly keeping
 * tabs on allocated memory.
 * Free the list using itree_clearoutandfree(), then itree_destroy().
 */
ITREE *rs_gdbm_read_headers (RS_LLD lld	/* RS generic low level descriptor */)
{
     char *headstr, *hd_val, *tok;
     int length;
     unsigned int hd_hash;
     ITREE *hds;
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "store not locked: underlying GDBM not open");
	  return NULL;
     }

     /* read in the ring directory and parse */
     headstr = rs_gdbm_dbfetch(rs, RS_GDBM_HEADDICT, &length);
     hds = itree_create();
     if (headstr) {
	  /* fast, simple list reader for <hd_hash>|<hd_val>\001 */
          hd_hash = strtoul(strtok(headstr, "|"), NULL, 10);
	  hd_val = strtok(NULL, "\001");
	  itree_add(hds, hd_hash, xnstrdup(hd_val));
	  while ((tok = strtok(NULL, "|"))) {
	       hd_hash = strtoul(tok, NULL, 10);
	       hd_val = strtok(NULL, "\001");
	       itree_add(hds, hd_hash, xnstrdup(hd_val));
	  }
	  nfree(headstr);
     }

     return hds;
}


/*
 * Write the passed list representing headers to the header dictionary
 * in the GDBM datastore. The list should have the header hash as the key
 * and the header & info string as the value.
 * Returns 1 if successful or 0 if the operation has failed.
 */
int    rs_gdbm_write_headers (RS_LLD lld,    /* generic low level descriptor */
			      ITREE *headers /* list of header strings */)
{
     int length, r, sz = 0;
     char *headstr, *pt;
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     /* count the size of buffer needed to store the header string 
      * and allocate a bufer to store */
     itree_traverse(headers) 
	  sz += strlen(itree_get(headers))+14;
     headstr = nmalloc(sz);

     /* print header dictionary as a single string.
      * Field delimiters are pipe (|) symbols and record delimiters are
      * bytes of \001 */
     pt = headstr;
     itree_traverse(headers) {
	  pt += sprintf(pt, "%u|%s\001", 
			itree_getkey(headers), (char *) itree_get(headers));
     }
     *pt = '\0';

     /* write the header dictionary record with a null terminator */
     length = strlen(headstr)+1;	/* include \0 */
     r = rs_gdbm_dbreplace(rs, RS_GDBM_HEADDICT, headstr, length);

     nfree(headstr);
     return r;
}


/*
 * Read the index for the ring with id 'ringid'.
 * This contains details of the records and their order in the ring, 
 * returning a TABLE (if successful) with the following columns
 *    seq      sequence number of datum
 *    time     date and time (int time_t format) datum was created
 *    hd_hash  hash index of the datum's header
 * Returns a TABLE if successful, which will be empty if no index 
 * exists for the ring. If there is a failure, NULL will be returned.
 */
TABLE rs_gdbm_read_index (RS_LLD lld, 	/* RS generic low level descriptor */
			  int ringid	/* ring id */)
{
     char *ringindex, indexname[RS_GDBM_INDEXKEYLEN];
     int length;
     TABLE index;
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return NULL;
     }

     /* make the ring index name of the form 'ri<ringid>' and read it */
     sprintf(indexname, "%s%d", RS_GDBM_INDEXNAME, ringid);
     ringindex = rs_gdbm_dbfetch(rs, indexname, &length);

     /* create table from ring index text */
     index = table_create_a(rs_ringidx_hds);
     if (ringindex) {
          table_scan(index, ringindex, "\t", TABLE_SINGLESEP, TABLE_NOCOLNAMES, 
		     TABLE_NORULER);
	  table_freeondestroy(index, ringindex);
     }

     /* return the table regardless of success. 
      * If the ring index was found, then the table will have some 
      * rows, otherwise it will not */
     return index;
}


/*
 * Write the passed TABLE representing a ring index to the GDBM datastore.
 * Returns 1 if successful or 0 if the operation has failed.
 */
int    rs_gdbm_write_index (RS_LLD lld, /* RS generic low level descriptor */
			    int ringid,	/* ring id */
			    TABLE index	/* index of samples within ring */)
{
     char *ringindex, indexname[RS_GDBM_INDEXKEYLEN];
     int length, r;
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     /* convert table to a string and write it to the GDBM */
     ringindex = table_outbody(index);
     if (ringindex) {
	  util_strrtrim(ringindex);	/* strip trailing \n */
	  length = strlen(ringindex)+1;	/* include \0 */
	  sprintf(indexname, "%s%d", RS_GDBM_INDEXNAME, ringid);
	  r = rs_gdbm_dbreplace(rs, indexname, ringindex, length);
     }

     nfree(ringindex);
     return r;
}


/*
 * Remove the index file from the GDBM file. Used as part of the ring
 * deletion process and should be used inside a write lock. 
 * Returns 1 for success or 0 for failure.
 */
int    rs_gdbm_rm_index     (RS_LLD lld, int ringid)
{
     RS_GDBMD rs;
     char indexname[RS_GDBM_INDEXKEYLEN];
     int r;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     /* delete the index record from the GDBM */
     sprintf(indexname, "%s%d", RS_GDBM_INDEXNAME, ringid);
     r = rs_gdbm_dbdelete(rs, indexname);

     return r;
}


/*
 * Add data blocks into the GDBM database and index them as a sequence.
 * The data blocks are in an ordered list with the values being of type
 * RS_DBLOCK and should be inserted sequentially starting from start_seq.
 * The ring is specified by 'ringid'.
 * Each RS_DBLOCK specifies the time and header of each sample and
 * the position implies the sequence.
 * Returns number of blocks inserted.
 */
int    rs_gdbm_append_dblock(RS_LLD lld,	/* low level descriptor */
			     int ringid,	/* ring id */
			     int start_seq, 	/* starting sequence */
			     ITREE *dblock	/* list of RS_DBLOCK */)
{
     int length, r, seq, num_written=0;
     RS_GDBMD rs;
     RS_DBLOCK d;
     char key[RS_GDBM_DATAKEYLEN];
     char *value;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     /* iterate over data to write */
     seq = start_seq;
     itree_traverse(dblock) {
	  /* compose the key and value pairs */
	  d = itree_get(dblock);
	  snprintf(key, RS_GDBM_DATAKEYLEN, "%s%d_%d", RS_GDBM_DATANAME, 
		   ringid, seq);
	  length = strlen(d->data);
	  value = xnmalloc(length + 25);	/* space for time & hd key */
	  length = snprintf(value, length+25, "%d|%u|%s", 
			    (unsigned int) d->time, d->hd_hashkey, d->data);
	  length++;	/* include \0 */

	  /* write the composed block of data */
	  r = rs_gdbm_dbreplace(rs, key, value, length);
	  if ( ! r )
	       elog_printf(ERROR, "couldn't write %s", key);
	  else
	       num_written++;

	  /* loop */
	  nfree(value);
	  seq++;
     }

     return num_written;
}


/*
 * Read a set of data blocks from a GDBM database, that are in
 * sequence and belong to the same ring.
 * By giving the ring and start sequence, and block count,
 * an ordered list is returned. This list contains at most 'count' 
 * records, with each element's key being the sequence number as an integer
 * and the value pointing to an nmalloc()ed RS_DBLOCK.
 * Returns an ITREE if successful (including empty data) in the format
 * above or NULL otherwise. 
 * Free the returned data with rs_free_dblock().
 */
ITREE *rs_gdbm_read_dblock(RS_LLD lld,	  /* low level descriptor */
			   int ringid,	  /* ring id */
			   int start_seq, /* starting sequence */
			   int nblocks	  /* number of data blocks */)
{
     RS_GDBMD rs;
     RS_DBLOCK d;
     ITREE *dlist;
     char key[RS_GDBM_DATAKEYLEN], *value;
     int length, i;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return NULL;
     }

     /* iterate over data to read */
     dlist = itree_create();
     for (i=0; i<nblocks; i++) {
	  /* compose the key and value pairs: rd<rindid>_<startseq> */
	  snprintf(key, RS_GDBM_DATAKEYLEN, "%s%d_%d", RS_GDBM_DATANAME, 
		   ringid, start_seq+i);

	  /* get the data */
	  value = rs_gdbm_dbfetch(rs, key, &length);
	  if ( ! value ) {
	       /* the ring is not yet written, has been expired or there
		* is a value mismatch somewhere. not a fatal problem, so 
		* we carry on */
	       elog_printf(DEBUG, "block does not exist: %s", key);
	       continue;
	  }

	  /* split into component parts, packing
	   * them into DBLOCK and thence into the list.
	   * for efficiency, don't xnstrdup but use the spaced returned by
	   * the .._dbfetch() function and use the private field to
	   * hold a reference to it so that mem can be released
	   * with rs_free_dblock() */
	  d = xnmalloc(sizeof(struct rs_data_block));
	  d->time = strtol(strtok(value, "|"), NULL, 10);
	  d->hd_hashkey = strtoul(strtok(NULL, "|"), NULL, 10);
	  d->data = strtok(NULL, "|");
	  d->__priv_alloc_mem = value;
	  itree_add(dlist, start_seq+i, d);
     }

     return dlist;
}



/*
 * Remove all the data blocks in the GDBM with ring set to 'ringid'
 * and have a sequence numbers between and including 'from_seq' and 'to_seq'.
 * Returns the number of blocks removed
 */
int   rs_gdbm_expire_dblock(RS_LLD lld,   /* low level descriptor */
			    int ringid,   /* ring id */
			    int from_seq, /* greater and equal */
			    int to_seq    /* less than and equal to */ )
{
     RS_GDBMD rs;
     char key[RS_GDBM_DATAKEYLEN];
     int seq, num_rm=0;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     /* cycle over the range of data blocks to remove */
     for (seq = from_seq; seq <= to_seq; seq++) {
	  snprintf(key, RS_GDBM_DATAKEYLEN, "%s%d_%d", RS_GDBM_DATANAME, 
		   ringid, seq);
	  if (rs_gdbm_dbdelete(rs, key))
	       num_rm++;
	  else
	       elog_printf(DEBUG, "couldn't delete %s", key);	       
     }

     return num_rm;
}


TREE  *rs_gdbm_read_substr  (RS_LLD lld, /* RS generic low level descriptor */
			     char *substr_key)
{
     return NULL;
}


/*
 * Read a single datum from a GDBM that must be locked for reading.
 * This mechanism is independent of ring, header or index structure
 * and just gets the datum by key value.
 * Returns the datum if successful and sets its length in
 * 'ret_length'. If unsuccessful, returns NULL and sets ret_length to -1.
 */
char  *rs_gdbm_read_value   (RS_LLD lld, /* RS generic low level descriptor */
			     char *key,	 /* key of datum */
			     int *ret_length /* output length of datum */ )
{
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return NULL;
     }

     return rs_gdbm_dbfetch(rs, key, ret_length);
}


/*
 * Write a single datum to a GDBM that must be locked for writing.
 * This mechanism is independent of ring, header or index structure
 * and just gets the datum by key value.
 * Returns 1 for successful or 0 for failure
 */
int    rs_gdbm_write_value (RS_LLD lld,	/* RS generic low level descriptor */
			    char *key,	/* key string */
			    char *value,/* contents of datum */
			    int length	/* length of datum */)
{
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     return rs_gdbm_dbreplace(rs, key, value, length);
}


/*
 * Checkpoint a GDBM file. Returns 1 for success or 0 for failure.
 */
int    rs_gdbm_checkpoint   (RS_LLD lld	/* RS generic low level descriptor */)
{
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     if (rs_gdbm_dbreorganise(rs))
	  return 0;	/* failure */
     else
	  return 1;	/* success */
}


/*
 * Return the size taken by the GDBM file in bytes or -1 if there is an error.
 */
int    rs_gdbm_footprint    (RS_LLD lld	/* RS generic low level descriptor */)
{
     struct stat buf;
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return -1;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return -1;
     }

     /* carry out the stat */
     if (stat(rs->name, &buf) == -1)
	  return -1;
     return(buf.st_size);
}


/*
 * Dump the GDBM database to elog using the DEBUG severity, one line 
 * per record, max 80 characters per line.
 * The database should be locked for reading.
 * Returns the number of records (and therefore lines) printed
 */
int rs_gdbm_dumpdb(RS_LLD lld)
{
     void *d;
     char *k, *ddump;
     int dlen, ln=0;
     RS_GDBMD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_gdbmd_from_lld(lld);
     if (rs->ref == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying GDBM not open");
	  return 0;
     }

     /* First datum */
     d = rs_gdbm_readfirst(rs, &k, &dlen);
     if ( ! d )
	  return 0;
     elog_startsend(DEBUG, "Contents of ringstore (GDBM) ----------\n");
     ln++;
     ddump = util_bintostr(65, d, dlen);
     elog_contprintf(DEBUG, "%14s %s\n", k, ddump);
     nfree(d);
     nfree(ddump);

     /* remining data */
     while (d != NULL) {
	  d = rs_gdbm_readnext(rs, &k, &dlen);
	  if (d == NULL)
	       break;
	  ddump = util_bintostr(65, d, dlen);
	  elog_contprintf(DEBUG, "%14s %s\n", k, ddump);
	  nfree(d);
	  nfree(ddump);
	  ln++;
     }

     elog_endsend(DEBUG, "-----------------------------------");

     return ln;
}



/*
 * Return pointers to the error status variables, that GDBM fills
 * in when it is able to
 */
void rs_gdbm_errstat(RS_LLD lld, int *errnum, char **errstr) {
     *errnum  = rs_gdbm_errno;
     *errstr = rs_gdbm_errstr[rs_gdbm_errno];
}

/* --------------- Private routines ----------------- */


RS_GDBMD rs_gdbmd_from_lld(RS_LLD lld	/* typeless low level data */)
{
     if (((RS_GDBMD)lld)->lld_type != RS_LLD_TYPE_GDBM) {
	  elog_die(FATAL, "type mismatch %d != RS_LLD_TYPE_GDBM (%d)",
		   (int *)lld, RS_LLD_TYPE_GDBM);
     }
     return (RS_GDBMD) lld;
}


/* Error handling routine when a database goes wrong */
void rs_gdbm_dberr()
{
     elog_safeprintf(ERROR, "GDBM error: %d - %s", gdbm_errno, 
		 gdbm_strerror(gdbm_errno));
}


/*
 * Private: Open GDBM using locks and timing loops and handle errors.
 * 'Where' is the name of the calling routine, 'rw' is the
 * locking mode (see rs_gdbm_lock for more information).
 * Returns 1 if successful or 0 for error
 */
int rs_gdbm_dbopen(RS_GDBMD rs, char *where, enum rs_db_lock rw)
{
     GDBM_FILE db;
     int i;
     struct timespec t;

#if 0
     /* the debug log below can be very annoying!! use with caution */
     elog_safeprintf(DEBUG, "called from %s mode %d name %s",
		 where, rw, rs->name);
#endif

     if (rs->ref)
          elog_printf(ERROR, "error DBM file %s already open", rs->name);

     /* loop to retry dbm repeatedly to get a lock */
     for (i=0; i < RS_GDBM_NTRYS; i++) {
          switch (rw) {
	  case RS_RDLOCK:	/* read */
	  case RS_RDLOCKNOW:
	       if (access(rs->name, F_OK) == -1)
		    return 0;
	       db = gdbm_open(rs->name, 0, GDBM_READER || GDBM_NOLOCK, 
			      rs->mode, rs_gdbm_dberr);
	       break;
	  case RS_WRLOCK:	/* write */
	  case RS_WRLOCKNOW:
	       db = gdbm_open(rs->name,0,GDBM_WRITER,rs->mode,rs_gdbm_dberr);
	       break;
	  case RS_CRLOCKNOW:	/* create */
	       db = gdbm_open(rs->name,0,GDBM_WRCREAT,rs->mode,rs_gdbm_dberr);
	       break;
	  default:
	       elog_safeprintf(ERROR, "%s unsupported action: %d",
			       where, rw);
	       return 0;
	       break;
	  }

	  /* got a lock and opened database */
	  if (db) {
	       rs->ref = db;
	       return 1;	/* success */
	  }

	  /* NOW! locks should fail now */
	  if (rw == RS_RDLOCKNOW || rw == RS_WRLOCKNOW || rw == RS_CRLOCKNOW)
	       return 0;	/* fail */

	  /* Allow lock failures to proceed with another try */
	  if (gdbm_errno != GDBM_CANT_BE_READER &&
	      gdbm_errno != GDBM_CANT_BE_WRITER)
	       break;

#if 0
	  /* log waiting */
	  fprintf(stderr,
		  "%s attempt %d on %s mode %d failed (%d: %s); retry"
		  where, i, fs->name, rw,
		  gdbm_errno, gdbm_strerror(gdbm_errno));
#endif

	  /* wait waittry nanoseconds */
	  t.tv_sec = 0;
	  t.tv_nsec = RS_GDBM_WAITTRY;
	  nanosleep(&t, NULL);
     }

     /* fatally failed to open gdbm file */
     elog_safeprintf(DIAG, 
		     "%s unable to open %s mode %d (err %d: %s)", 
		     where, rs->name, rw, 
		     gdbm_errno, gdbm_strerror(gdbm_errno));

     return 0;
}


/* Private: close an already opened DBM and clear the GDBM descriptor
 * in the RS_GDBMD structure */
void rs_gdbm_dbclose(RS_GDBMD rs)
{
#if 0
     /* the debug log below can be very annoying!! use with caution */
     elog_printf(DEBUG, "called file %s", rs->name);
#endif

     gdbm_close(rs->ref);
     rs->ref = NULL;
}


/* 
 *Private: fetch a datum from the GDBM using null terminated key, which
 * sets its length in ret_length and returns the value in a nmalloc()ed
 * buffer.
 * After use, free the buffer with nfree(). If there is an error, ret_length
 * will be set to -1 and NULL is returned.
 */
char *rs_gdbm_dbfetch(RS_GDBMD rs, char *key, int *ret_length)
{
     datum k, d;

     /* fetch data and handle error */
     k.dptr = key;
     k.dsize = strlen(key);
     d = gdbm_fetch(rs->ref, k);
     if (d.dptr == NULL) {
	  *ret_length = -1;
	  return NULL;
     }

     /* success: adopt and return data */
     *ret_length = d.dsize;
     nadopt(d.dptr);
     return d.dptr;
}


/* Private: replace data in a GDBM, overwriting previously stored values
 * Returns 1 for success, 0 for error */
int rs_gdbm_dbreplace(RS_GDBMD rs, char *key, char *value, int length)
{
     datum d, k;

     k.dptr = key;
     k.dsize = strlen(key);
     d.dptr = value;
     d.dsize = length;
     if (gdbm_store(rs->ref, k, d, GDBM_REPLACE))
	  return 0;	/* bad */
     else
	  return 1;	/* good */
}


/* Private: delete the data identified by key.
 * Returns 1 for success, 0 for error */
int rs_gdbm_dbdelete(RS_GDBMD rs, char *key)
{
     datum k;

     k.dptr = key;
     k.dsize = strlen(key);
     if (gdbm_delete(rs->ref, k))
	  return 0;	/* bad */
     else
	  return 1;	/* good */
}


/* Private: fetch the first datum from a GDBM and return it.
 * The caller must nfree() the dat after use */
char *rs_gdbm_dbfirstkey(RS_GDBMD rs)
{
     datum k;

     k = gdbm_firstkey(rs->ref);
     nadopt(k.dptr);

     return k.dptr;
}


/* Private: fetch the next datum from a GDBM given the key of the last datum.
 * Returns the datum */
char *rs_gdbm_dbnextkey(RS_GDBMD rs, char *lastkey)
{
     datum k, lastk;

     lastk.dptr = lastkey;
     lastk.dsize = strlen(lastkey);
     k = gdbm_nextkey(rs->ref, lastk);
     nadopt(k.dptr);
     return k.dptr;
}


/* Private: reoganise the GDBM */
int rs_gdbm_dbreorganise(RS_GDBMD rs)
{
     return gdbm_reorganize(rs->ref);
}


/*
 * Start a read traversal of the entire GDBM which should be 
 * locked for reading
 * Following a successful rs_gdbm_readfirst(), it should be followed by
 * rs_gdbm_readnext() for the next record or rs_gdbm_readend() to finish the
 * traversal and clear any storage.
 * Data returned by the rs_gdbm_read* family, should be freed by the caller 
 * when finished, but the keys passed back will be freed by successive
 * calls to rs_gdbm_readnext() or rs_gdbm_readend().
 * Returns the first data, with its size set in length and its key.
 * On failure of if the database is empty, NULL is returned, key is
 * set to NULL and length will be set to -1.
 * The traversal will skip the superblock record.
 */
char *rs_gdbm_readfirst(RS_GDBMD rs, char **key, int *length)
{
     char *k, *d;

     k = rs_gdbm_dbfirstkey(rs);
     if ( ! k ) {
	  *key = NULL;
	  *length = -1;
	  return NULL;
     }
     if ( ! strcmp(RS_GDBM_SUPERNAME, k) ) {
	  /* first record was superblock! do it again!! */
	  if (rs->lastkey)
	       nfree(rs->lastkey);
	  rs->lastkey = k;
	  k = rs_gdbm_dbnextkey(rs, rs->lastkey);
	  if ( ! k ) {
	       *key = NULL;
	       *length = -1;
	       return NULL;
	  }
     }

     /* store the fist key and get its data */
     if (rs->lastkey)
	  nfree(rs->lastkey);
     rs->lastkey = k;
     d = rs_gdbm_dbfetch(rs, rs->lastkey, length);

     /* return key and data */
     *key = k;
     return d;
}


/*
 * Return the next record in the GDBM.
 * Must be preceeded by a rs_gdbm_readnext() or rs_gdbm_readfirst().
 * Will not return the superblock record.
 * The caller should free the data when finished with nfree(), but leave 
 * the key alone as it will be freed by rs_gdbm_readnext() or 
 * rs_gdbm_readend();
 * Return NULL for error or if the database is exhausted.
 */
char *rs_gdbm_readnext(RS_GDBMD rs, char **key, int *length)
{
     char *d, *k;

     /* find the next key */
     do {
	  k = rs_gdbm_dbnextkey(rs, rs->lastkey);
	  if (rs->lastkey)
	       nfree(rs->lastkey);
	  rs->lastkey = k;
	  if ( ! k ) {
	       *key = NULL;
	       *length = -1;
	       return NULL;
	  }
     } while ( ! strcmp(RS_GDBM_SUPERNAME, k) );

     /* store the next key and get its data */
     d = rs_gdbm_dbfetch(rs, rs->lastkey, length);

     /* return the data */
     *key = k;
     return d;
}


/*
 * End the read traversal, clear up storage.
 */
void rs_gdbm_readend(RS_GDBMD rs)
{

     /* Free storage */
     if (rs->lastkey)
	  nfree(rs->lastkey);
     rs->lastkey = NULL;

     return;
}





#if TEST

char *rs_ringdir_hds[] = {"name", "id", "long", "about", "nslots", "dur", 
			  NULL};
char *rs_ringidx_hds[] = {"seq", "time", "hd_hash", NULL};

RS_SUPER rs_create_superblock()
{
     struct utsname uts;
     RS_SUPER super;
     time_t created;
     int r;
     char *domainname = "";
     struct tm *created_tm;

     r = uname(&uts);
     if (r < 0) {
	  /* error: unable to get data, so don't create */
	  elog_printf(ERROR, "unable to uname(). errno=%d %s",
		      errno, strerror(errno));
	  return 0;	/* bad */
     }
     time(&created);

#ifdef _GNU_SOURCE
     domainname = uts.domainname;
#endif

     /* Get timezone information, which is the number of seconds 
      * between GMT (UTC) and the local time.
      * We assume that the user running this process has a timezone 
      * that represents the true local time
      */
     created_tm = localtime(&created);

     super = xnmalloc(sizeof(struct rs_superblock));
     super->version = RS_SUPER_VERSION;
     super->created = created;
     super->os_name = xnstrdup(uts.sysname);
     super->os_release = xnstrdup(uts.release);
     super->os_version = xnstrdup(uts.version);
     super->hostname = xnstrdup(uts.nodename);
     super->domainname = xnstrdup(domainname);
     super->machine = xnstrdup(uts.machine);
     super->timezone = timezone;
     super->generation = 0;

     return super;
}


/*
 * Copy a superblock
 */
RS_SUPER rs_copy_superblock(RS_SUPER src)
{
     RS_SUPER dst;

     dst = xnmalloc(sizeof(struct rs_superblock));
     dst->version =     src->version;
     dst->created =     src->created;
     dst->os_name =     xnstrdup(src->os_name);
     dst->os_release =  xnstrdup(src->os_release);
     dst->os_version =  xnstrdup(src->os_version);
     dst->hostname =    xnstrdup(src->hostname);
     dst->domainname =  xnstrdup(src->domainname);
     dst->machine =     xnstrdup(src->machine);
     dst->timezone =    src->timezone;
     dst->generation =  src->generation;
     dst->ringcounter = src->ringcounter;

     return dst;
}


/*
 * Free an existing superblock
 */
void rs_free_superblock(RS_SUPER super)
{
     if (super == NULL)
	  return;

     nfree(super->os_name);
     nfree(super->os_release);
     nfree(super->os_version);
     nfree(super->hostname);
     nfree(super->domainname);
     nfree(super->machine);
     nfree(super);
}

/*
 * Free the space taken by a list of RS_DBLOCK when returned by 
 * rs_gdbm_read_dblock()
 */
void   rs_free_dblock    (ITREE *dlist)
{
     RS_DBLOCK d;

     itree_traverse(dlist) {
	  d = itree_get(dlist);
	  if (d->__priv_alloc_mem)
	       nfree(d->__priv_alloc_mem);
	  nfree(d);
     }
     itree_destroy(dlist);
}




#include <sys/time.h>
#include "rt_file.h"
#include "rt_std.h"
#define TESTRS1 "t.rs_gdbm.1.dat"
#define TEST_BUFLEN 50
#define TEST_ITER 100

int main()
{
     RS_GDBMD rs;
     int r, i;
     TABLE ringdir, ringdir2, index, index2;
     char *buf1, *buf2, *buf3;
     ITREE *headers, *headers2;
     ITREE *dlist, *dlist2;
     RS_DBLOCK dblock, dblock2;
     struct rs_data_block data1, data2, data3;

     /* initialise */
     route_init(NULL, 0);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     rs_gdbm_init();
     elog_init(0, "holstore test", NULL);
     fprintf(stderr, "expect diag messages, these are not errors "
	     "in themselves\n");

     /*
      * These tests are designed to check the correct working of the
      * interface abstraction, not the DB itself. We assume that that has
      * been fully tested and that it works!!
      */
     unlink(TESTRS1);

     /* test 1a: open (no create) */
     rs = rs_gdbm_open(TESTRS1, 0644, 0);
     if (rs != NULL) {
	  fprintf(stderr, "[1a] Shouldn't open rs_gdbm\n");
	  exit(1);
     }

     /* test 1b: open (create) and close */
     rs = rs_gdbm_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[1b] Unable to open rs_gdbm for writing\n");
	  exit(1);
     }
     rs_gdbm_close(rs);

     /* test 1c: open the created file, lock for reading and close */
     rs = rs_gdbm_open(TESTRS1, 0644, 0);
     if (rs == NULL) {
	  fprintf(stderr, "[1c] Unable to open rs_gdbm for reading\n");
	  exit(1);
     }
     if ( ! rs_gdbm_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[1c] Unable to lock rs_gdbm for reading\n");
	  exit(1);
     }
     rs_gdbm_unlock(rs);
     rs_gdbm_close(rs);

     /* test 1d: open, lock for writing and close */
     rs = rs_gdbm_open(TESTRS1, 0644, 0);
     if (rs == NULL) {
	  fprintf(stderr, "[1d] Unable to open rs_gdbm\n");
	  exit(1);
     }
     if ( ! rs_gdbm_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[1d] Unable to lock rs_gdbm for writing\n");
	  exit(1);
     }
     rs_gdbm_unlock(rs);	/* ...but close later */

     /* test 1e: initialisation checking */
     rs_gdbm_errstat(rs, &r, &buf1);
     if (r != 0) {
	  fprintf(stderr, "[0] errno should return 0\n");
	  exit(1);
     }
     if (strcmp(buf1, "unknown") != 0) {
	  fprintf(stderr, "[0] errstr should return 'unknown'\n");
	  exit(1);
     }
     rs_gdbm_close(rs);

     /* test 2: open, read and write ringdirs and close */
     rs = rs_gdbm_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[2] Unable to open rs_gdbm\n");
	  exit(1);
     }
     if ( ! rs_gdbm_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[2] Unable to lock rs_gdbm for reading\n");
	  exit(1);
     }
     ringdir = rs_gdbm_read_rings(rs);
     rs_gdbm_unlock(rs);
     if (ringdir == NULL) {
	  fprintf(stderr, "[2] no ringdir table returned\n");
	  exit(1);
     }
     if (table_nrows(ringdir) != 0) {
	  fprintf(stderr, "[2] ringdir shouldn't have rows!\n");
	  exit(1);
     }
     table_addemptyrow(ringdir);
     table_replacecurrentcell(ringdir, "name", "tom");
     table_replacecurrentcell(ringdir, "id", "0");
     table_replacecurrentcell(ringdir, "long", "thomas's ring !!");
     table_replacecurrentcell(ringdir, "about", "all about his rings?");
     table_replacecurrentcell(ringdir, "nslots", "30");
     table_replacecurrentcell(ringdir, "dur", "0");
     if ( ! rs_gdbm_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[2] Unable to lock rs_gdbm for writing\n");
	  exit(1);
     }
     if ( ! rs_gdbm_write_rings(rs, ringdir)) {
	  fprintf(stderr, "[2] unable to write ringdir\n");
	  exit(1);
     }
     rs_gdbm_unlock(rs);
     if ( ! rs_gdbm_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[2b] Unable to lock rs_gdbm for reading\n");
	  exit(1);
     }
     ringdir2 = rs_gdbm_read_rings(rs);
     rs_gdbm_unlock(rs);
     rs_gdbm_close(rs);
     buf1 = table_outtable(ringdir);
     buf2 = table_outtable(ringdir2);
     if (strcmp(buf1, buf2)) {
	  fprintf(stderr, "[2b] re-read table does not match\n");
	  exit(1);
     }
     nfree(buf1);
     nfree(buf2);
     table_destroy(ringdir);
     table_destroy(ringdir2);

     /* test 3: open, read and write headers and close */
     rs = rs_gdbm_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[3] Unable to open rs_gdbm\n");
	  exit(1);
     }
     if ( ! rs_gdbm_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[3] Unable to lock rs_gdbm for reading\n");
	  exit(1);
     }
     headers = rs_gdbm_read_headers(rs);
     rs_gdbm_unlock(rs);
     if (headers == NULL) {
	  fprintf(stderr, "[3] no header list returned\n");
	  exit(1);
     }
     if (itree_n(headers) != 0) {
	  fprintf(stderr, "[3] headers shouldn't have entries!\n");
	  exit(1);
     }
     itree_add(headers, 0, "tom");
     itree_add(headers, 1, "dick");
     itree_add(headers, 2, "harry");
     if ( ! rs_gdbm_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[3] Unable to lock rs_gdbm for writing\n");
	  exit(1);
     }
     if ( ! rs_gdbm_write_headers(rs, headers)) {
	  fprintf(stderr, "[3] unable to write headers\n");
	  exit(1);
     }
     rs_gdbm_unlock(rs);
     if ( ! rs_gdbm_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[3b] Unable to lock rs_gdbm for reading\n");
	  exit(1);
     }
     headers2 = rs_gdbm_read_headers(rs);
     rs_gdbm_unlock(rs);
     rs_gdbm_close(rs);
     if (itree_n(headers) != 3 || itree_n(headers2) != 3) {
	  fprintf(stderr, "[3b] re-read list have different sizes\n");
	  exit(1);
     }
     itree_first(headers); itree_first(headers2);
     if (strcmp(itree_get(headers), itree_get(headers2))) {
	  fprintf(stderr, "[2b] re-read list does not match: element 1\n");
	  exit(1);
     }
     itree_next(headers); itree_next(headers2);
     if (strcmp(itree_get(headers), itree_get(headers2))) {
	  fprintf(stderr, "[2b] re-read list does not match: element 2\n");
	  exit(1);
     }
     itree_next(headers); itree_next(headers2);
     if (strcmp(itree_get(headers), itree_get(headers2))) {
	  fprintf(stderr, "[2b] re-read list does not match: element 3\n");
	  exit(1);
     }
     itree_destroy(headers);
     itree_clearoutandfree(headers2);
     itree_destroy(headers2);

     /* test 4: open, read and write index and close */
     rs = rs_gdbm_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[4] Unable to open rs_gdbm\n");
	  exit(1);
     }
     if ( ! rs_gdbm_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[4] Unable to lock rs_gdbm for reading\n");
	  exit(1);
     }
     index = rs_gdbm_read_index(rs, 7); /* should return an empty index */
     rs_gdbm_unlock(rs);
     if (index == NULL) {
	  fprintf(stderr, "[4] no index table returned\n");
	  exit(1);
     }
     if (table_nrows(index) != 0) {
	  fprintf(stderr, "[4] index shouldn't have rows!\n");
	  exit(1);
     }
     table_addemptyrow(index);
     table_replacecurrentcell(index, "seq", "23");
     table_replacecurrentcell(index, "time", "98753388");
     table_replacecurrentcell(index, "hd_hash", "592264");
     if ( ! rs_gdbm_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[4] Unable to lock rs_gdbm for writing\n");
	  exit(1);
     }
     if ( ! rs_gdbm_write_index(rs, 7, index)) {
	  fprintf(stderr, "[4] unable to write index\n");
	  exit(1);
     }
     rs_gdbm_unlock(rs);
     if ( ! rs_gdbm_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[4b] Unable to lock rs_gdbm for reading\n");
	  exit(1);
     }
     index2 = rs_gdbm_read_index(rs, 7);
     rs_gdbm_unlock(rs);
     rs_gdbm_close(rs);
     buf1 = table_outtable(index);
     buf2 = table_outtable(index2);
     if (strcmp(buf1, buf2)) {
	  fprintf(stderr, "[4b] re-read table does not match\n");
	  exit(1);
     }
     nfree(buf1);
     nfree(buf2);
     table_destroy(index);
     table_destroy(index2);

     /* test 5: read, append and reread data to a ring */
     rs = rs_gdbm_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[5] Unable to open rs_gdbm\n");
	  exit(1);
     }
     if ( ! rs_gdbm_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[5] Unable to lock rs_gdbm for reading\n");
	  exit(1);
     }
     dlist = rs_gdbm_read_dblock(rs, 0, 7, 2);
     rs_gdbm_unlock(rs);
     if (dlist == NULL) {
	  fprintf(stderr, "[5] no data list returned\n");
	  exit(1);
     }
     if (itree_n(dlist) != 0) {
	  fprintf(stderr, "[5] data list shouldn't have entries!\n");
	  exit(1);
     }
     data1.time = time(NULL);
     data1.hd_hashkey = 6783365;
     data1.data = "tom";
     itree_append(dlist, &data1);
     data2.time = time(NULL);
     data2.hd_hashkey = 6783365;
     data2.data = "dick";
     itree_append(dlist, &data2);
     data3.time = time(NULL);
     data3.hd_hashkey = 6783365;
     data3.data = "harry";
     itree_append(dlist, &data3);
     if ( ! rs_gdbm_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[5] Unable to lock rs_gdbm for writing\n");
	  exit(1);
     }
     if ( ! rs_gdbm_append_dblock(rs, 0, 7, dlist)) {
	  fprintf(stderr, "[5] unable to write data blocks\n");
	  exit(1);
     }
     rs_gdbm_unlock(rs);
     if ( ! rs_gdbm_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[5b] Unable to lock rs_gdbm for reading\n");
	  exit(1);
     }
     dlist2 = rs_gdbm_read_dblock(rs, 0, 7, 5);
     if (itree_n(dlist2) != 3) {
	  fprintf(stderr, "[5b1] re-read list has wrong size\n");
	  exit(1);
     }
     rs_free_dblock(dlist2);
     dlist2 = rs_gdbm_read_dblock(rs, 0, 7, 3);
     rs_gdbm_unlock(rs);
     rs_gdbm_close(rs);
     if (itree_n(dlist) != 3 || itree_n(dlist2) != 3) {
	  fprintf(stderr, "[5b2] re-read list have different sizes\n");
	  exit(1);
     }
     itree_first(dlist);         itree_first(dlist2);
     dblock = itree_get(dlist);  dblock2 = itree_get(dlist2);
     if (dblock->time != dblock2->time) {
	  fprintf(stderr, "[5b] re-read list does not match: 1 time\n");
	  exit(1);
     }
     if (dblock->hd_hashkey != dblock2->hd_hashkey) {
	  fprintf(stderr, "[5b] re-read list does not match: 1 hd_hashkey\n");
	  exit(1);
     }
     if (strcmp(dblock->data, dblock2->data)) {
	  fprintf(stderr, "[5b] re-read list does not match: element 1\n");
	  exit(1);
     }
     itree_next(dlist);          itree_next(dlist2);
     dblock = itree_get(dlist);  dblock2 = itree_get(dlist2);
     if (dblock->time != dblock2->time) {
	  fprintf(stderr, "[5b] re-read list does not match: 1 time\n");
	  exit(1);
     }
     if (dblock->hd_hashkey != dblock2->hd_hashkey) {
	  fprintf(stderr, "[5b] re-read list does not match: 1 hd_hashkey\n");
	  exit(1);
     }
     if (strcmp(dblock->data, dblock2->data)) {
	  fprintf(stderr, "[5b] re-read list does not match: element 1\n");
	  exit(1);
     }
     itree_next(dlist);          itree_next(dlist2);
     dblock = itree_get(dlist);  dblock2 = itree_get(dlist2);
     if (dblock->time != dblock2->time) {
	  fprintf(stderr, "[5b] re-read list does not match: 1 time\n");
	  exit(1);
     }
     if (dblock->hd_hashkey != dblock2->hd_hashkey) {
	  fprintf(stderr, "[5b] re-read list does not match: 1 hd_hashkey\n");
	  exit(1);
     }
     if (strcmp(dblock->data, dblock2->data)) {
	  fprintf(stderr, "[5b] re-read list does not match: element 1\n");
	  exit(1);
     }
     rs_free_dblock(dlist2);
     itree_destroy(dlist);


     elog_fini();
     route_fini();

     printf("tests finished successfully\n");
     return 0;
}

#endif /* TEST */
