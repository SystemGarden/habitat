/*
 * Ringstore low level storage using an abstracted interface
 * Berkley DB implementataion
 *
 * Nigel Stuckey, March 2011 using code from September 2001 and
 * January 1998 onwards
 * Copyright System Garden Limited 1998-2011. All rights reserved.
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
#include <libgen.h>
#include "nmalloc.h"
#include "elog.h"
#include "util.h"
#include "rs.h"
#include "rs_berk.h"

/* private functional prototypes */


const struct rs_lowlevel rs_berk_method = {
     rs_berk_init,          rs_berk_fini,          rs_berk_open,
     rs_berk_close,         rs_berk_exists,        rs_berk_lock,
     rs_berk_unlock,        rs_berk_read_super,    rs_berk_write_super,
     rs_berk_read_rings,    rs_berk_write_rings,   rs_berk_read_headers,
     rs_berk_write_headers, rs_berk_read_index,    rs_berk_write_index, 
     rs_berk_rm_index,      rs_berk_append_dblock, rs_berk_read_dblock, 
     rs_berk_expire_dblock, rs_berk_read_substr,   rs_berk_read_value,
     rs_berk_write_value,   rs_berk_checkpoint,    rs_berk_footprint,
     rs_berk_dumpdb,        rs_berk_errstat
};

/* statics */
int   rs_berk_isinit=0;			/* initialised */
int   rs_berk_errno=0;			/* unknown status */
char *rs_berk_errstr[] = {"unknown"};	/* unknown status */

/* initialise */
void   rs_berk_init         ()
{
     rs_berk_isinit=1;
}


/* finalise */
void   rs_berk_fini         ()
{
}



/*
 * Open a Berkley DB file to support the ringstore low level interface.
 * If create is set, call Berkley DB with the filename and the mode and create
 * if not already there. Otherwise, just attempt to open the file for
 * reading.
 * With Berkley DB, the file is opened when the route opens and stays open
 * until the route is closed. Locking and unlocking trigger transactions
 * primitives [rs_berk_lock(), rs_berk_unlock()].
 * Returns the low level descriptor if successful or NULL otherwise.
 */
RS_LLD rs_berk_open         (char *filepath,	/* path of Berkley DB file */
			     mode_t perm, 	/* unix file creation perms */
			     int create		/* 1=create */ )
{
     int r;
     u_int32_t db_flags, env_flags;
     DB_ENV *envp;
     DB *dbp;
     DB_TXN *txn;
     RS_BERKD rs;
     RS_SUPER superblock;
     char *filedir, *filename, *mem_filedir, *mem_filename;

     if (! rs_berk_isinit)
          elog_die(FATAL, "rs_berk unitialised");

     /* separate file path into name and dir */
     mem_filedir  = xnstrdup(filepath);
     mem_filename = xnstrdup(filepath);
     filedir  = dirname(mem_filedir);
     filename = dirname(mem_filename);

     /* create environment */
     envp = NULL;
     r = db_env_create(&envp, 0);
     if (r != 0) {
          elog_printf(ERROR, "Error creating environment handle: %s\n",
		      db_strerror(r));
	  goto rs_berk_open_err;
     }

     /* ask for transactions, locking, logging and a cache */
     env_flags = DB_CREATE |    /* Create the environment if it does 
				 * not already exist. */
                 DB_INIT_TXN  | /* Initialize transactions */
                 DB_INIT_LOCK | /* Initialize locking. */
                 DB_INIT_LOG  | /* Initialize logging */
                 DB_INIT_MPOOL; /* Initialize the in-memory cache. */

     /* open environment (in addition to creating) */
     r = envp->open(envp, filedir, env_flags, 0);
     if (r != 0) {
          elog_printf(ERROR, "Error opening environment: %s\n",
		      db_strerror(r));
	  goto rs_berk_open_err;
     }

     /* Create the DB handle */
     r = db_create(&dbp, envp, 0);
     if (r != 0) {
          elog_printf(ERROR, "Database creation failed %s (%d)", 
		      db_strerror(r), r);
	  goto rs_berk_open_err;
     }

     /* Open DB & possibly create the file */
     db_flags = DB_AUTO_COMMIT;
     if (create)
          db_flags |= DB_CREATE;
     r = dbp->open(dbp,        /* Pointer to the database */
		   NULL,       /* Txn pointer */
		   filepath,   /* File name */
		   NULL,       /* Logical db name */
		   DB_BTREE,   /* Database type (using btree) */
		   db_flags,   /* Open flags */
		   0);         /* File mode. Using defaults */
     if (r != 0) {
          elog_printf(ERROR, "Database open of '%s' failed (%s - %d)", 
		      filepath, db_strerror(r), r);
	  goto rs_berk_open_err;
     }

     /* Get the txn handle */
     txn = NULL;
     r = envp->txn_begin(envp, NULL, &txn, 0);
     if (r) {
          elog_printf(ERROR, "Database transaction on '%s' failed (%s - %d)", 
		      filename, db_strerror(r), r);
	  goto rs_berk_open_err;
     }

     /* TODO - open DB for read and attempt to read the superblock
      * this tests for an existing berkley DB file that belongs to
      * a different app. Magic number test */

     /* Check the file exists and if so, is it a valid ringstore format */
     superblock = rs_berk_read_super_fd(envp, dbp, txn);
     if ( ! superblock ) {
          /* its a new DB, so we need to create some standard furniture.
	   * create the superblock from the base class library routine 
	   * and write it out to the empty DB */
          superblock = rs_create_superblock();
	  if ( ! rs_berk_write_super_fd(envp, dbp, txn, superblock)) {
	       elog_printf(ERROR, "unable to write superblock to %s", 
			   filename);
	       txn->abort(txn);
	       goto rs_berk_open_err;
	  }
     }

     /* 
      * Commit the transaction. Note that the transaction handle
      * can no longer be used.
      */ 
     r = txn->commit(txn, 0);
     if (r) {
          elog_printf(ERROR, "Open DB transaction commit on %s failed "
		      "(%s - %d)", filename, db_strerror(r), r);
	  return 0;
     }

     /* The Berkley DB now contains a superblock.
      *	Create, complete and return the descriptor */
     rs = xnmalloc(sizeof(struct rs_berk_desc));
     rs->lld_type = RS_LLD_TYPE_BERK;	/* descriptor type */
     rs->name = xnstrdup(filename);	/* file name */
     rs->dir = xnstrdup(filedir);	/* dir name */
     rs->mode = perm;			/* file mode */
     rs->envp = envp;			/* working open environment pointer */
     rs->dbp = dbp;			/* working open DB descriptor */
     rs->txn = NULL;			/* no current transaction */
     rs->cursorp = NULL;		/* no current cursor */
     rs->super = superblock;		/* super block structure */
     rs->lock = RS_UNLOCK;		/* unlocked */

     nfree(mem_filedir);
     nfree(mem_filename);

     return rs;

 rs_berk_open_err:
     /* general shutdown when an error is encountered to release resources */

     /* close the database */
     if (dbp) {
          r = dbp->close(dbp, 0);
	  if (r)
	       elog_printf(ERROR, "Database close transaction commit failed "
			   "on %s (%s - %d)", filename, db_strerror(r), r);
     }

     /* close the environment */
     if (envp) {
          r = envp->close(envp, 0);
	  if (r)
	       elog_printf(ERROR, "Error closing environment: %s (%s - %d)",
			   filename, db_strerror(r), r);
     }
     
     nfree(mem_filedir);
     nfree(mem_filename);

     return NULL;
}


/* Close and free up an existing rs_berk descritpor */
void   rs_berk_close        (RS_LLD lld	/* RS generic low level descriptor */)
{
     RS_BERKD rs;
     int r;

     /* param checking */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not opened before closing");
	  return;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->dbp == NULL) {
          elog_die(FATAL, "underlying Berkley DB not open");
	  return;
     }

     /* -- commit or rollback transaction -- */

     /* close the database */
     if (rs->dbp) {
          r = rs->dbp->close(rs->dbp, 0);
	  if (r)
	       elog_printf(ERROR, "Database close failed: %s (%d)",
			   db_strerror(r), r);
     }

     /* close the environment */
     if (rs->envp) {
          r = rs->envp->close(rs->envp, 0);
	  if (r)
	       elog_printf(ERROR, "Error closing environment: %s (%d)",
			   db_strerror(r), r);
     }

     rs = rs_berkd_from_lld(lld);
     nfree(rs->name);
     nfree(rs->dir);
     rs_free_superblock(rs->super);
     nfree(rs);
}



/* Checks to see if the filename is a RS_BERK file and can carry out the
 * what is required in 'todo'.
 * A return of 0 means yes, non-0 means no of which can indicate several 
 * states. 
 * 1=the file exists but is not a Berkley DB file, 
 * 2=the file does not exist, 
 * 3=the file exists but would be unable to carry out 'todo' */
int    rs_berk_exists       (char *filename, enum rs_db_writable todo)
{
     RS_SUPER superblock;

     /* Check the file exists and if so, is it a valid ringstore format */
     superblock = rs_berk_read_super_file(filename);
     if ( ! superblock ) {
	  if (access(filename, F_OK) == 0) {
	       /* a non-Berkley DB file exists, so we leave it alone */
	       elog_printf(DIAG, "%s exists but is not a Berkley DB file", filename);
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
	  elog_printf(DIAG, "Berkley DB %s exists but unable to write as asked", 
		      filename);
	  return 1;	/* bad */
     }

     return 0;	/* good */
}

/*
 * Lock the Berkley DB db for work and keep it locked until rs_berk_unlock()
 * is called. If successive calls are made, the locks will be converted
 * to the newest's request.
 * In Berkley DB, there is no difference between locking for read or write:
 * one just starts a transaction to be commited later. Unfortunately, one
 * issue with this is the occaisional dead lock, where one or more 
 * transactions will have to be aborted.
 * THIS IS CURRENTLY SILENTLY IGNORED -- TBD FIX
 * For compatibility, all ringstore locking args are accepted by silently 
 * ignored (RS_RDLOCK, RS_RWLOCK, RS_RDLOCKNOW and RS_WRLOCKNOW).
 * Returns 1 for success or 0 for failure
 */
int    rs_berk_lock(RS_LLD lld,		/* RS generic low level descriptor */
		    enum rs_db_lock rw, /* lock to take on db */
		    char *where		/* caller description for diag */)
{
     RS_BERKD rs;
     DB_TXN *txn;
     int r;

     /* param checking */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not opened before locking");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);

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

     /* Start a transaction */
     txn = NULL;
     r = rs->envp->txn_begin(rs->envp, NULL, &txn, 0);
     if (r) {
          elog_printf(ERROR, "Transaction begin failed: %s (%d)",
		      db_strerror(r), r);
	  return 0;
     }
     rs->txn = txn;

     return 1;	/* success */
}


/* Unlock the Berkley DB, which actually commits the transaction */
void   rs_berk_unlock       (RS_LLD lld	/* RS generic low level descriptor */)
{
     RS_BERKD rs;
     int r;

     /* param checking */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not opened before unlocking");
	  return;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return;
     }

     r = rs->txn->commit(rs->txn, 0);
     if (r)
          elog_printf(ERROR, "Transaction commit failed: %s (%d)",
		      db_strerror(r), r);

     rs->lock = RS_UNLOCK;
     rs->txn = NULL;
}

/* 
 * Read the superblock from an opened, locked Berkley DB file and return
 * a superblock structure if successful or NULL otherwise.
 * Replaces the superblock copy in the descriptor as well, to keep 
 * it up-to-date.
 * Free superblock with rs_free_superblock().
 */
RS_SUPER rs_berk_read_super (RS_LLD lld	/* RS generic low level descriptor */)
{
     RS_BERKD rs;
     RS_SUPER super;

     /* param checking */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open to read superblock");
	  return NULL;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return NULL;
     }

     /* read latest superblock */
     super = rs_berk_read_super_fd(rs->envp, rs->dbp, rs->txn);
     if ( ! super )
	  return NULL;

     /* cache a copy of the updated superblock */
     rs_free_superblock(rs->super);
     rs->super = rs_copy_superblock(super);

     return super;
}


/* 
 * Read the superblock from an unopened Berkley DB file that is uninitialised
 * (or being investigated) by rs_berk and return a superblock structure 
 * if successful or NULL otherwise.
 * Use rs_free_superblock() to clear the structure after use.
 * The file will be open and closed in this call.
 */
RS_SUPER rs_berk_read_super_file (char *dbname	/* name of Berkley DB file */)
{
     RS_LLD db;
     RS_BERKD berk_db;
     RS_SUPER super;

     /* open the Berkley DB using normal open call */
     if (access(dbname, R_OK) == -1)
	  return NULL;
     db = rs_berk_open(dbname, 0x400, 0);
     if (!db) {
          elog_printf(DIAG, "unable to open %s as Berkley DB file", dbname);
	  return NULL;
     }

     /* read the superblock from the RS_BERKD structure */
     berk_db = rs_berkd_from_lld(db);
     super = rs_copy_superblock(berk_db->super);

     rs_berk_close(db);

     return super;
}

/* 
 * Read the superblock from an opened Berkley DB file that is uninitialised
 * (or being investigated) by rs_berk and return a superblock structure if 
 * successful or NULL otherwise.
 * Use rs_free_superblock() to clear the structure after use.
 * Assumes that the we are already within a transaction, auto or explicit.
 */
RS_SUPER rs_berk_read_super_fd   (DB_ENV *envp,	/* Berkley DB environment */
				  DB * dbp,	/* Berkley DB pointer */
				  DB_TXN * txn	/* Berkley DB transaction */)
{
     DBT d, k;
     RS_SUPER super;
     char *magic, *mydata;
     int r;

     /* attempt to read an existing ringstore superblock */
     memset(&k, 0, sizeof(k));
     memset(&d, 0, sizeof(d));
     k.data = RS_BERK_SUPERNAME;
     k.size = RS_BERK_SUPERNLEN;

     /* Perform the database read, assuming transactions are taken care 
      * of by the caller */
     r = dbp->get(dbp, txn, &k, &d, 0);
     if (r) {
          elog_printf(ERROR, "Superblock get failed: %s (%d)",
		      db_strerror(r), r);
	  return NULL;
     }

     /* check the magic string */
     mydata = nstrdup(d.data);
     magic = util_strtok_sc(mydata, "|");
     if (strcmp(magic, RS_BERK_MAGIC) != 0) {
          nfree(mydata);
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

     return super;
}


/*
 * Write the superblock to an opened, locked Berkley DB file and return 1
 * if successful or 0 for error.
 * If the write is successful, the copy in the descrptor is updated
 * with the new version.
 */
int    rs_berk_write_super(RS_LLD lld,    /* RS generic low level descriptor */
			   RS_SUPER super /* superblock */)
{
     RS_BERKD rs;
     int r;

     /* param checking */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open to write superblock");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return 0;
     }

     /* write and if successful, update the descriptor's superblock cache */
     r = rs_berk_write_super_fd(rs->envp, rs->dbp, rs->txn, super);
     if (r) {
	  rs_free_superblock(rs->super);
	  rs->super = rs_copy_superblock(super);
     }

     return r;
}


/*
 * Open the Berkley DB file for writing and store the given superblock.
 * It will not create the file and will return 1 for success or 
 * 0 for  failure.
 */
int    rs_berk_write_super_file  (char *dbname,	 /* name Berkley DB file */
				  mode_t perm,   /* file create permissions */
				  RS_SUPER super /* superblock */ )
{
     RS_LLD db;
     RS_BERKD rs;
     int r;

     /* open the Berkley DB using a direct low level call */
     if (access(dbname, R_OK) == -1)
	  return 0;
     db = rs_berk_open(dbname, perm, 1);
     if (!db) {
          elog_printf(DIAG, "unable to open %s as Berkley DB file", dbname);
	  return 0;
     }

     /* write the superblock */
     rs = rs_berkd_from_lld(db);
     r = rs_berk_write_super_fd(rs->envp, rs->dbp, rs->txn, super);
     rs_berk_close(db);

     return r;
}


/*
 * Write a superblock to an opened Berkley DB file that is not initialised
 * in the normal way (or is in the process of initialising). Therfore
 * it does not use the RS_GDBMD structure.
 * Return 1 for successfully written superblock or 0 for an error.
 */
int    rs_berk_write_super_fd (DB_ENV *envp,	/* Berkley DB environment */
			       DB * dbp,	/* Berkley DB pointer */
			       DB_TXN *txn,	/* Berkley DB transaction */
			       RS_SUPER super	/* superblock */)
{
     char superblock[RS_BERK_SUPERMAX];
     int r;
     DBT k, d;

     /* prep data and key */
     memset(&k, 0, sizeof(k));
     memset(&d, 0, sizeof(d));
     d.size = 1+sprintf(superblock, "%s|%d|%ld|%s|%s|%s|%s|%s|%s|%d|%d|%d", 
			RS_BERK_MAGIC, super->version, super->created,
			super->os_name, super->os_release, super->os_version,
			super->hostname, super->domainname, super->machine,
			super->timezone, super->generation, 
			super->ringcounter);
     d.data = superblock;
     k.data = RS_BERK_SUPERNAME;
     k.size = RS_BERK_SUPERNLEN;

     /*
      * Perform the database write. If this fails, abort the transaction.
      */
     r = dbp->put(dbp, txn, &k, &d, 0);
     if (r) {
          elog_printf(ERROR, "Superblock put failed: %s (%d)",
		      db_strerror(r), r);
	  return 0;
     }

     return 1;		/* good */
}


/*
 * Read the ring directory and return a table of existing rings in the
 * existing and locked Berkley DB. The table contains a row per ring, with
 * the following columns (if successful).
 *   name   ring name
 *   id     ring id, used for low level storing
 *   long   long name of ring
 *   about  description of ring
 *   size   size of ring in slots or 0 for unlinited
 *   dur    duration of samples
 * Returns NULL if there is an error, or the TABLE otherwise.
 */
TABLE rs_berk_read_rings   (RS_LLD lld	/* RS generic low level descriptor */)
{
     char *ringdir;
     int length;
     TABLE rings;
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return NULL;
     }

     /* read in the ring directory and parse */
     ringdir = rs_berk_dbfetch(rs, RS_BERK_RINGDIR, &length);

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
 * The Berkley DB should be open for writing. 
 * Returns 1 for success or 0 for failure.
 */
int    rs_berk_write_rings (RS_LLD lld, /* RS generic low level descriptor */
			    TABLE rings /* table of rings */)
{
     char *ringdir;
     int length, r;
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return 0;
     }

     /* convert table to a string and write it to the Berkley DB */
     ringdir = table_outbody(rings);
     if (! ringdir)
	  ringdir = xnstrdup("");
     length = strlen(ringdir)+1;	/* include \0 */
     r = rs_berk_dbreplace(rs, RS_BERK_RINGDIR, ringdir, length);
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
ITREE *rs_berk_read_headers (RS_LLD lld	/* RS generic low level descriptor */)
{
     char *headstr, *hd_val, *tok;
     int length;
     unsigned int hd_hash;
     ITREE *hds;
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return NULL;
     }

     /* read in the ring directory and parse */
     headstr = rs_berk_dbfetch(rs, RS_BERK_HEADDICT, &length);
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
 * in the Berkley DB datastore. The list should have the header hash as the key
 * and the header & info string as the value.
 * Returns 1 if successful or 0 if the operation has failed.
 */
int    rs_berk_write_headers (RS_LLD lld,    /* generic low level descriptor */
			      ITREE *headers /* list of header strings */)
{
     int length, r, sz = 0;
     char *headstr, *pt;
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
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
     r = rs_berk_dbreplace(rs, RS_BERK_HEADDICT, headstr, length);

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
TABLE rs_berk_read_index (RS_LLD lld, 	/* RS generic low level descriptor */
			  int ringid	/* ring id */)
{
     char *ringindex, indexname[RS_BERK_INDEXKEYLEN];
     int length;
     TABLE index;
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return NULL;
     }

     /* make the ring index name of the form 'ri<ringid>' and read it */
     sprintf(indexname, "%s%d", RS_BERK_INDEXNAME, ringid);
     ringindex = rs_berk_dbfetch(rs, indexname, &length);

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
 * Write the passed TABLE representing a ring index to the Berkley DB datastore.
 * Returns 1 if successful or 0 if the operation has failed.
 */
int    rs_berk_write_index (RS_LLD lld, /* RS generic low level descriptor */
			    int ringid,	/* ring id */
			    TABLE index	/* index of samples within ring */)
{
     char *ringindex, indexname[RS_BERK_INDEXKEYLEN];
     int length, r;
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return 0;
     }

     /* convert table to a string and write it to the Berkley DB */
     ringindex = table_outbody(index);
     if (ringindex) {
	  util_strrtrim(ringindex);	/* strip trailing \n */
	  length = strlen(ringindex)+1;	/* include \0 */
	  sprintf(indexname, "%s%d", RS_BERK_INDEXNAME, ringid);
	  r = rs_berk_dbreplace(rs, indexname, ringindex, length);
     }

     nfree(ringindex);
     return r;
}


/*
 * Remove the index file from the Berkley DB file. Used as part of the ring
 * deletion process and should be used inside a write lock. 
 * Returns 1 for success or 0 for failure.
 */
int    rs_berk_rm_index     (RS_LLD lld, int ringid)
{
     RS_BERKD rs;
     char indexname[RS_BERK_INDEXKEYLEN];
     int r;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return 0;
     }

     /* delete the index record from the Berkley DB */
     sprintf(indexname, "%s%d", RS_BERK_INDEXNAME, ringid);
     r = rs_berk_dbdelete(rs, indexname);

     return r;
}


/*
 * Add data blocks into the Berkley DB database and index them as a sequence.
 * The data blocks are in an ordered list with the values being of type
 * RS_DBLOCK and should be inserted sequentially starting from start_seq.
 * The ring is specified by 'ringid'.
 * Each RS_DBLOCK specifies the time and header of each sample and
 * the position implies the sequence.
 * Returns number of blocks inserted.
 */
int    rs_berk_append_dblock(RS_LLD lld,	/* low level descriptor */
			     int ringid,	/* ring id */
			     int start_seq, 	/* starting sequence */
			     ITREE *dblock	/* list of RS_DBLOCK */)
{
     int length, r, seq, num_written=0;
     RS_BERKD rs;
     RS_DBLOCK d;
     char key[RS_BERK_DATAKEYLEN];
     char *value;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return 0;
     }

     /* iterate over data to write */
     seq = start_seq;
     itree_traverse(dblock) {
	  /* compose the key and value pairs */
	  d = itree_get(dblock);
	  snprintf(key, RS_BERK_DATAKEYLEN, "%s%d_%d", RS_BERK_DATANAME, 
		   ringid, seq);
	  length = strlen(d->data);
	  value = xnmalloc(length + 25);	/* space for time & hd key */
	  length = snprintf(value, length+25, "%ld|%lu|%s", 
			    d->time, d->hd_hashkey, d->data);
	  length++;	/* include \0 */

	  /* write the composed block of data */
	  r = rs_berk_dbreplace(rs, key, value, length);
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
 * Read a set of data blocks from a Berkley DB database, that are in
 * sequence and belong to the same ring.
 * By giving the ring and start sequence, and block count,
 * an ordered list is returned. This list contains at most 'count' 
 * records, with each element's key being the sequence number as an integer
 * and the value pointing to an nmalloc()ed RS_DBLOCK.
 * Returns an ITREE if successful (including empty data) in the format
 * above or NULL otherwise. 
 * Free the returned data with rs_free_dblock().
 */
ITREE *rs_berk_read_dblock(RS_LLD lld,	  /* low level descriptor */
			   int ringid,	  /* ring id */
			   int start_seq, /* starting sequence */
			   int nblocks	  /* number of data blocks */)
{
     RS_BERKD rs;
     RS_DBLOCK d;
     ITREE *dlist;
     char key[RS_BERK_DATAKEYLEN], *value;
     int length, i;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return NULL;
     }

     /* iterate over data to read */
     dlist = itree_create();
     for (i=0; i<nblocks; i++) {
	  /* compose the key and value pairs: rd<rindid>_<startseq> */
	  snprintf(key, RS_BERK_DATAKEYLEN, "%s%d_%d", RS_BERK_DATANAME, 
		   ringid, start_seq+i);

	  /* get the data */
	  value = rs_berk_dbfetch(rs, key, &length);
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
 * Remove all the data blocks in the Berkley DB with ring set to 'ringid'
 * and have a sequence numbers between and including 'from_seq' and 'to_seq'.
 * Returns the number of blocks removed
 */
int   rs_berk_expire_dblock(RS_LLD lld,   /* low level descriptor */
			    int ringid,   /* ring id */
			    int from_seq, /* greater and equal */
			    int to_seq    /* less than and equal to */ )
{
     RS_BERKD rs;
     char key[RS_BERK_DATAKEYLEN];
     int seq, num_rm=0;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return 0;
     }

     /* cycle over the range of data blocks to remove */
     for (seq = from_seq; seq <= to_seq; seq++) {
	  snprintf(key, RS_BERK_DATAKEYLEN, "%s%d_%d", RS_BERK_DATANAME, 
		   ringid, seq);
	  if (rs_berk_dbdelete(rs, key))
	       num_rm++;
	  else
	       elog_printf(DEBUG, "couldn't delete %s", key);	       
     }

     return num_rm;
}


TREE  *rs_berk_read_substr  (RS_LLD lld, /* RS generic low level descriptor */
			     char *substr_key)
{
     return NULL;
}


/*
 * Read a single datum from a Berkley DB that must be locked for reading.
 * This mechanism is independent of ring, header or index structure
 * and just gets the datum by key value.
 * Returns the datum if successful and sets its length in
 * 'ret_length'. If unsuccessful, returns NULL and sets ret_length to -1.
 */
char  *rs_berk_read_value   (RS_LLD lld, /* RS generic low level descriptor */
			     char *key,	 /* key of datum */
			     int *ret_length /* output length of datum */ )
{
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return NULL;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return NULL;
     }

     return rs_berk_dbfetch(rs, key, ret_length);
}


/*
 * Write a single datum to a Berkley DB that must be locked for writing.
 * This mechanism is independent of ring, header or index structure
 * and just gets the datum by key value.
 * Returns 1 for successful or 0 for failure
 */
int    rs_berk_write_value (RS_LLD lld,	/* RS generic low level descriptor */
			    char *key,	/* key string */
			    char *value,/* contents of datum */
			    int length	/* length of datum */)
{
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return 0;
     }

     return rs_berk_dbreplace(rs, key, value, length);
}


/*
 * Checkpoint a Berkley DB file. Returns 1 for success or 0 for failure.
 */
int    rs_berk_checkpoint   (RS_LLD lld	/* RS generic low level descriptor */)
{
     RS_BERKD rs;
     int r;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return 0;
     }

     r = rs_berk_dbreorganise(rs);
     if (r)
	  return 1;	/* success */
     else
	  return 0;	/* failure */
}


/*
 * Return the size taken by the Berkley DB file in bytes or -1 if there is 
 * an error.
 */
int    rs_berk_footprint    (RS_LLD lld	/* RS generic low level descriptor */)
{
     struct stat buf;
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return -1;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return -1;
     }

     /* carry out the stat */
     if (stat(rs->name, &buf) == -1)
	  return -1;
     return(buf.st_size);
}


/*
 * Dump the Berkley DB database to elog using the DEBUG severity, one line 
 * per record, max 80 characters per line.
 * The database should be locked for reading.
 * Returns the number of records (and therefore lines) printed
 */
int rs_berk_dumpdb(RS_LLD lld)
{
     void *d;
     char *k, *ddump;
     int dlen, ln=0;
     RS_BERKD rs;

     /* param checking and conversion */
     if (lld == NULL) {
	  elog_printf(ERROR, "ringstore not open");
	  return 0;
     }
     rs = rs_berkd_from_lld(lld);
     if (rs->envp == NULL || rs->dbp == NULL || rs->lock == RS_UNLOCK) {
          elog_die(FATAL, "underlying Berkley DB not open/locked");
	  return 0;
     }

     /* First datum */
     d = rs_berk_readfirst(rs, &k, &dlen);
     if ( ! d )
	  return 0;
     elog_startsend(DEBUG, "Contents of ringstore (Berkley DB) ----------\n");
     ln++;
     ddump = util_bintostr(65, d, dlen);
     elog_contprintf(DEBUG, "%14s %s\n", k, ddump);
     nfree(d);
     nfree(ddump);

     /* remining data */
     while (d != NULL) {
	  d = rs_berk_readnext(rs, &k, &dlen);
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
 * Return pointers to the error status variables, that Berkley DB fills
 * in when it is able to
 */
void rs_berk_errstat(RS_LLD lld, int *errnum, char **errstr) {
     *errnum = errno;
     *errstr = db_strerror(errno);
}


/* --------------- Private routines ----------------- */


RS_BERKD rs_berkd_from_lld(RS_LLD lld	/* typeless low level data */)
{
     if (((RS_BERKD)lld)->lld_type != RS_LLD_TYPE_BERK) {
	  elog_die(FATAL, "type mismatch %d != RS_LLD_TYPE_BERK (%d)",
		   (int *)lld, RS_LLD_TYPE_BERK);
     }
     return (RS_BERKD) lld;
}


/* Error handling routine when a database goes wrong */
void rs_berk_dberr()
{
     elog_safeprintf(ERROR, "Berkley DB error: %d - %s", rs_berk_errno, 
		     db_strerror(rs_berk_errno));
}


/* 
 * Private: fetch a datum from the Berkley DB using null terminated key, which
 * sets its length in ret_length and returns the value in a nmalloc()ed
 * buffer.
 * After use, free the buffer with nfree(). If there is an error, ret_length
 * will be set to -1 and NULL is returned.
 */
char *rs_berk_dbfetch(RS_BERKD rs, char *key, int *ret_length)
{
     DBT k, d;
     int r;

     /* fetch data and handle error */
     memset(&k, 0, sizeof(k));
     memset(&d, 0, sizeof(d));
     k.data = key;
     k.size = strlen(key);
     r = rs->dbp->get(rs->dbp, rs->txn, &k, &d, 0);
     if (r) {
	  *ret_length = -1;
	  return NULL;
     }

     /* success: adopt and return data */
     *ret_length = d.size;
     return nstrdup(d.data);
}


/* Private: replace data in a Berkley DB, overwriting previously stored values
 * Returns 1 for success, 0 for error */
int rs_berk_dbreplace(RS_BERKD rs, char *key, char *value, int length)
{
     DBT d, k;

     memset(&k, 0, sizeof(k));
     memset(&d, 0, sizeof(d));
     k.data = key;
     k.size = strlen(key);
     d.data = value;
     d.size = length;
     if (rs->dbp->put(rs->dbp, rs->txn, &k, &d, 0))
	  return 0;	/* bad */
     else
	  return 1;	/* good */
}


/* Private: delete the data identified by key.
 * Returns 1 for success, 0 for error */
int rs_berk_dbdelete(RS_BERKD rs, char *key)
{
     DBT k;

     k.data = key;
     k.size = strlen(key);
     if (rs->dbp->del(rs->dbp, rs->txn, &k, 0))
	  return 0;	/* bad */
     else
	  return 1;	/* good */
}


/* Private: reoganise the Berkley DB. In Berkley, this reorganises the DB 
 * and returns free space to the file system */
int rs_berk_dbreorganise(RS_BERKD rs)
{
     return rs->dbp->compact(rs->dbp, rs->txn, NULL, NULL, NULL, 
			     DB_FREE_SPACE, NULL);
}


/*
 * Start a read traversal of the entire Berkley DB which should be 
 * locked for reading
 * Following a successful rs_berk_readfirst(), it should be followed by
 * rs_berk_readnext() for the next record or rs_berk_readend() to finish the
 * traversal and clear any storage.
 * Both key and data returned by the rs_berk_read* family, should be 
 * nfree()ed by the caller when finished.
 * Returns the first data, with its size set in length and its key.
 * On failure of if the database is empty, NULL is returned, key is
 * set to NULL and length will be set to -1.
 * The traversal will skip the superblock record.
 */
char *rs_berk_readfirst(RS_BERKD rs, char **key, int *length)
{
     DBT k, d;
     DBC *cursorp;
     int r;

     /* first time we do this, start the cursor */
     r = rs->dbp->cursor(rs->dbp, rs->txn, &cursorp, 0);
     if (r) {
          elog_printf(ERROR, "Cursor open failed: %s (%d)",
		      db_strerror(r), r);
	  *key = NULL;
	  *length = -1;
          return NULL;
     }
     rs->cursorp = cursorp;	/* store cursor */

     /* Now get the first data in the cursor */
     memset(&k, 0, sizeof(k));
     memset(&d, 0, sizeof(d));
     r = rs->cursorp->get(rs->cursorp, &k, &d, DB_FIRST);
     if (r) {
          if (r != DB_NOTFOUND)
	       elog_printf(ERROR, "Cursor read failed: %s (%d)",
			   db_strerror(r), r);
	  *key = NULL;
	  *length = -1;
          return NULL;
     }

     if ( ! strcmp(RS_BERK_SUPERNAME, k.data) ) {
	  /* first record was superblock! do it again!! */
          r = rs->cursorp->get(rs->cursorp, &k, &d, DB_NEXT);
	  if (r) {
	       if (r != DB_NOTFOUND)
		    elog_printf(ERROR, "Cursor read failed (2): %s (%d)",
				db_strerror(r), r);
	       *key = NULL;
	       *length = -1;
	       return NULL;
	  }
     }

     /* return key, data and data length */
     *key = nstrdup(k.data);
     *length = d.size;
     return nstrdup(d.data);
}


/*
 * Return the next record in the Berkley DB.
 * Must be preceeded by a rs_berk_readnext() or rs_berk_readfirst().
 * Will not return the superblock record.
 * The caller should free the data when finished with nfree(), but leave 
 * the key alone as it will be freed by rs_berk_readnext() or 
 * rs_berk_readend();
 * Return NULL for error or if the database is exhausted.
 */
char *rs_berk_readnext(RS_BERKD rs, char **key, int *length)
{
     DBT k, d;
     int r;

     /* Now get the first data in the cursor */
     memset(&k, 0, sizeof(k));
     memset(&d, 0, sizeof(d));

     /* find the next key */
     do {
          r = rs->cursorp->get(rs->cursorp, &k, &d, DB_NEXT);
	  if (r) {
	       if (r != DB_NOTFOUND)
		    elog_printf(ERROR, "Cursor read failed (2): %s (%d)",
				db_strerror(r), r);
	       *key = NULL;
	       *length = -1;
	       return NULL;
	  }
     } while ( ! strcmp(RS_BERK_SUPERNAME, k.data) );

     /* return key, data and data length */
     *key = nstrdup(k.data);
     *length = d.size;
     return nstrdup(d.data);
}


/*
 * End the read traversal, clear up storage.
 */
void rs_berk_readend(RS_BERKD rs)
{
     int r;

     /* End cursor */
     r = rs->cursorp->close(rs->cursorp);
     if (r)
          elog_printf(ERROR, "Cursor close failed (2): %s (%d)",
		      db_strerror(r), r);
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
 * Copy a superblock. Returns an nmalloc()ed superblock which should be freed
 * with rs_free_superblock
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
 * rs_berk_read_dblock()
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
#define TESTRS1 "t.rs_berk.1.dat"
#define TEST_BUFLEN 50
#define TEST_ITER 100

int main()
{
     RS_BERKD rs;
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
     rs_berk_init();
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
     rs = rs_berk_open(TESTRS1, 0644, 0);
     if (rs != NULL) {
	  fprintf(stderr, "[1a] Shouldn't open rs_berk\n");
	  exit(1);
     }

     /* test 1b: open (create) and close */
     rs = rs_berk_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[1b] Unable to open rs_berk for writing\n");
	  exit(1);
     }
     rs_berk_close(rs);

     /* test 1c: open the created file, lock for reading and close */
     rs = rs_berk_open(TESTRS1, 0644, 0);
     if (rs == NULL) {
	  fprintf(stderr, "[1c] Unable to open rs_berk for reading\n");
	  exit(1);
     }
     if ( ! rs_berk_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[1c] Unable to lock rs_berk for reading\n");
	  exit(1);
     }
     rs_berk_unlock(rs);
     rs_berk_close(rs);

     /* test 1d: open, lock for writing and close */
     rs = rs_berk_open(TESTRS1, 0644, 0);
     if (rs == NULL) {
	  fprintf(stderr, "[1d] Unable to open rs_berk\n");
	  exit(1);
     }
     if ( ! rs_berk_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[1d] Unable to lock rs_berk for writing\n");
	  exit(1);
     }
     rs_berk_unlock(rs);	/* ...but close later */

     /* test 1e: initialisation checking */
     rs_berk_errstat(rs, &r, &buf1);
     if (r != 0) {
	  fprintf(stderr, "[0] errno should return 0\n");
	  exit(1);
     }
     if (strcmp(buf1, "unknown") != 0) {
	  fprintf(stderr, "[0] errstr should return 'unknown'\n");
	  exit(1);
     }
     rs_berk_close(rs);

     /* test 2: open, read and write ringdirs and close */
     rs = rs_berk_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[2] Unable to open rs_berk\n");
	  exit(1);
     }
     if ( ! rs_berk_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[2] Unable to lock rs_berk for reading\n");
	  exit(1);
     }
     ringdir = rs_berk_read_rings(rs);
     rs_berk_unlock(rs);
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
     if ( ! rs_berk_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[2] Unable to lock rs_berk for writing\n");
	  exit(1);
     }
     if ( ! rs_berk_write_rings(rs, ringdir)) {
	  fprintf(stderr, "[2] unable to write ringdir\n");
	  exit(1);
     }
     rs_berk_unlock(rs);
     if ( ! rs_berk_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[2b] Unable to lock rs_berk for reading\n");
	  exit(1);
     }
     ringdir2 = rs_berk_read_rings(rs);
     rs_berk_unlock(rs);
     rs_berk_close(rs);
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
     rs = rs_berk_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[3] Unable to open rs_berk\n");
	  exit(1);
     }
     if ( ! rs_berk_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[3] Unable to lock rs_berk for reading\n");
	  exit(1);
     }
     headers = rs_berk_read_headers(rs);
     rs_berk_unlock(rs);
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
     if ( ! rs_berk_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[3] Unable to lock rs_berk for writing\n");
	  exit(1);
     }
     if ( ! rs_berk_write_headers(rs, headers)) {
	  fprintf(stderr, "[3] unable to write headers\n");
	  exit(1);
     }
     rs_berk_unlock(rs);
     if ( ! rs_berk_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[3b] Unable to lock rs_berk for reading\n");
	  exit(1);
     }
     headers2 = rs_berk_read_headers(rs);
     rs_berk_unlock(rs);
     rs_berk_close(rs);
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
     rs = rs_berk_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[4] Unable to open rs_berk\n");
	  exit(1);
     }
     if ( ! rs_berk_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[4] Unable to lock rs_berk for reading\n");
	  exit(1);
     }
     index = rs_berk_read_index(rs, 7); /* should return an empty index */
     rs_berk_unlock(rs);
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
     if ( ! rs_berk_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[4] Unable to lock rs_berk for writing\n");
	  exit(1);
     }
     if ( ! rs_berk_write_index(rs, 7, index)) {
	  fprintf(stderr, "[4] unable to write index\n");
	  exit(1);
     }
     rs_berk_unlock(rs);
     if ( ! rs_berk_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[4b] Unable to lock rs_berk for reading\n");
	  exit(1);
     }
     index2 = rs_berk_read_index(rs, 7);
     rs_berk_unlock(rs);
     rs_berk_close(rs);
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
     rs = rs_berk_open(TESTRS1, 0644, 1);
     if (rs == NULL) {
	  fprintf(stderr, "[5] Unable to open rs_berk\n");
	  exit(1);
     }
     if ( ! rs_berk_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[5] Unable to lock rs_berk for reading\n");
	  exit(1);
     }
     dlist = rs_berk_read_dblock(rs, 0, 7, 2);
     rs_berk_unlock(rs);
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
     if ( ! rs_berk_lock(rs, RS_WRLOCK, "test")) {
	  fprintf(stderr, "[5] Unable to lock rs_berk for writing\n");
	  exit(1);
     }
     if ( ! rs_berk_append_dblock(rs, 0, 7, dlist)) {
	  fprintf(stderr, "[5] unable to write data blocks\n");
	  exit(1);
     }
     rs_berk_unlock(rs);
     if ( ! rs_berk_lock(rs, RS_RDLOCK, "test")) {
	  fprintf(stderr, "[5b] Unable to lock rs_berk for reading\n");
	  exit(1);
     }
     dlist2 = rs_berk_read_dblock(rs, 0, 7, 5);
     if (itree_n(dlist2) != 3) {
	  fprintf(stderr, "[5b1] re-read list has wrong size\n");
	  exit(1);
     }
     rs_free_dblock(dlist2);
     dlist2 = rs_berk_read_dblock(rs, 0, 7, 3);
     rs_berk_unlock(rs);
     rs_berk_close(rs);
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
