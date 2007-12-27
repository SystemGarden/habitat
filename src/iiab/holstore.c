/*
 * Holstore.
 * Generic storage database.
 * This class implements an abstraction layer over various DBM style
 * access methods. Currently, Gnu GDBM and Mird are used.
 *
 * Data is stored as a block of binary data, whereas the key is assumed
 * to be a string and null terminated. Currently, the terminating nulls 
 * are also stored.
 *
 * Nigel Stuckey, January 1998.
 * Suppport added for Mird, April 2001
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
#include "nmalloc.h"
#include "elog.h"
#include "util.h"

#define _HOLSTORE_H_PRIVATE_
#include "holstore.h"

int   hol_ntrys;	/* number of attempts to get open DBM lock */
long  hol_waittry;	/* nanoseconds to wait between DBM attempts */
int   hol_isinit=0;	/* has holstore been initialised */

/* Initialise holstore class
 * If ntrys or waittry is 0, then an internal default is provided
 */
void hol_init(int ntrys,	/* number of attempts to get open DBM lock */
	      long waittry	/* nanoseconds to wait between DBM opens */)
{
     hol_isinit++;
     if ( (hol_ntrys = ntrys) == 0)
          hol_ntrys = HOLSTORE_NTRYS;
     if ( (hol_waittry = waittry) == 0)
          hol_waittry = HOLSTORE_WAITTRY;
}

void hol_fini() {
}

/*
 * Open a holstore database without creating it.
 * Returns a holstore handle or NULL if unsuccessful
 */
HOLD hol_open(char *name	/* name of database */ )
{
     HOLD h;
     datum d, k;
     char magic[HOLSTORE_MAGICLEN+1];

     if (! hol_isinit)
          elog_die(FATAL, "unitialised");

     /* prepare this class's reference */
     h = xnmalloc(sizeof(struct holstore_descriptor));
     h->name = xnstrdup(name);
     h->mode = 0;		/* dont need the mode when reading */
     h->ref = NULL;
     h->lastkey.dptr = NULL;	/* no traversal */
     h->trans = 0;		/* no transactions */
     h->inhibtrans = 0;
     h->version = -1;
#if DB_RDBM
     h->logname = util_strjoin(name, ".log", NULL);
#endif /* DB_RDBM */

     /* be cautious about taking over an existing database file */
     /* use test mode (t) which will read without wingeing */
     if (hol_dbopen(h, "hol_open()", 't')) {
          /* database file exists -- is it one of ours? */
	  k.dptr = HOLSTORE_SUPERNAME;
	  k.dsize = HOLSTORE_SUPERNLEN;
	  d = hol_dbfetch(h, k);
	  nadopt(d.dptr);	/* monitor this memory */
	  if (d.dptr == NULL) {
	       /* we have disturbed a existing database that is nothing  */
	       /* to do with holstore!!.  Leave now!! */
	       hol_dbclose(h);
#if DB_RDBM
	       nfree(h->logname);
#endif /* DB_RDBM */
	       nfree(h->name);
	       nfree(h);
	       elog_safeprintf(INFO, 
			   "cant find holstore superblock in %s", name);
	       return NULL;	/* failure */
	  }
	  hol_dbclose(h);
     } else {
          /* DBM does not exist  */
#if DB_RDBM
	  nfree(h->logname);
#endif /* DB_RDBM */
          nfree(h->name);
	  nfree(h);
	  return NULL;
     }

     /* existing holstore, check suberblock */
     sscanf(d.dptr, 
	    " %[^|] | %d | %ld | %[^|] | %[^|] | %[^|] | %[^|] | %[^|]", 
	    (char *) &magic, &h->version, (long *) &h->created, 
	    (char *) &h->sysbuf.sysname, (char *) &h->sysbuf.nodename, 
	    (char *) &h->sysbuf.release, (char *) &h->sysbuf.version, 
	    (char *) &h->sysbuf.machine);
     nfree(d.dptr);
     if (strcmp(magic, HOLSTORE_MAGIC)) {
          /* failed magic check: refuse to open database */
          elog_safeprintf(ERROR, "%s wrong magic: not a holstore",
		      name);
#if DB_RDBM
	  nfree(h->logname);
#endif /* DB_RDBM */
	  nfree(h->name);
	  nfree(h);
	  return NULL;
     }
     if (h->version != HOLSTORE_VERSION) {
       /* failed: refuse to open database */
          elog_safeprintf(ERROR, 
		      "hol_open() wrong version: %s is %d, want %d",
		      name, h->version, HOLSTORE_VERSION);
#if DB_RDBM
	  nfree(h->logname);
#endif /* DB_RDBM */
	  nfree(h->name);
	  nfree(h);
	  return NULL;
     }

     return h;
}

/*
 * Create a holstore database
 * If a holstore already exists, dont touch it but treat it as a success.
 * Returns a holstore handle if created or NULL if unsuccessful
 */
HOLD hol_create(char *name,	/* name of database */
		mode_t mode	/* permission mode of database file */ ) {
     HOLD h;
     datum d, k;
     char superblock[HOLSTORE_SUPERMAX];
     int r;

     if (! hol_isinit)
          elog_die(FATAL, "unitialised");

     /* prepare this class's reference */
     h = xnmalloc(sizeof(struct holstore_descriptor));
     h->name = xnstrdup(name);
     h->mode = mode;
     h->ref = NULL;
     h->lastkey.dptr = NULL;	/* no traversal */
     h->trans = 0;		/* no transactions */
     h->inhibtrans = 0;
     h->version = -1;
#if DB_RDBM
     h->logname = util_strjoin(name, ".log", NULL);
#endif /* DB_RDBM */

     /* open the underlying database, creating if necessary, 
      * check for superblock, write one if not there and close */
     if (!hol_dbopen(h, "hol_create()", 'c')) {
#if DB_RDBM
	  nfree(h->logname);
#endif /* DB_RDBM */
	  nfree(h->name);
	  nfree(h);
	  return 0;	/* failure */
     }
     k.dptr = HOLSTORE_SUPERNAME;
     k.dsize = HOLSTORE_SUPERNLEN;
     d = hol_dbfetch(h, k);
     nadopt(d.dptr);		/* monitor this memory */
     if (d.dptr == NULL) {
	  /* new holstore, need to create superblock */
	  r = uname(&h->sysbuf);
	  if (r < 0) {
	       /* error: allow to continue with null values */
	       elog_safeprintf(ERROR, 
			   "unable to uname(). errno=%d %s",
			    errno, strerror(errno));
	       memset(&h->sysbuf, 0, sizeof(struct utsname));
	  }
	  time(&h->created);
	  h->version = HOLSTORE_VERSION;
	  d.dsize = 1+sprintf(superblock, "%s|%d|%ld|%s|%s|%s|%s|%s", 
			      HOLSTORE_MAGIC, h->version, (long) h->created, 
			      h->sysbuf.sysname, h->sysbuf.nodename, 
			      h->sysbuf.release, h->sysbuf.version, 
			      h->sysbuf.machine);
	  d.dptr = superblock;
	  r = hol_dbreplace(h, k, d);
	  if (r) {
	       /* error: unable to store superblock */
	       elog_safeprintf(ERROR,"unable to store superblock");
	       hol_dbclose(h);
#if DB_RDBM
	       nfree(h->logname);
#endif /* DB_RDBM */
	       nfree(h->name);
	       nfree(h);
	       return NULL;
	  }
     } else {
          /* error: allow to continue with null values */
          elog_safeprintf(ERROR, 
		      "superblock already exists in %s", name);
	  hol_dbclose(h);
#if DB_RDBM
	  nfree(h->logname);
#endif /* DB_RDBM */
	  nfree(h->name);
	  nfree(h);
	  return 0;	/* failure */
     }

     hol_dbclose(h);

     return h;
}

/* Close an already opened holstore */
void hol_close(HOLD h		/* holstore descriptor */ )
{
     if (h == NULL) {
	  elog_safeprintf(ERROR, "holstore not opened");
	  return;
     }

/* do we complain or do we end transactions and close off database? */
     if (h->trans)
	  elog_safeprintf(ERROR, "closed in mid transaction");

     if (h->ref != NULL) {
	  elog_safeprintf(ERROR, "db inconsistently open");
	  hol_dbclose(h);
     }

     nfree(h->name);
#if DB_RDBM
     nfree(h->logname);
#endif /* DB_RDBM */
     nfree(h);
}

/*
 * Write data to an open holstore and associate it with a key.
 * If data already exists with that key, it will be overwritten
 * Returns 1 for success, 0 for failure.
 */
int hol_put(HOLD h,		/* holstore descriptor */
	    char *key,		/* data key */
	    void *dat, 		/* data value */
	    int length		/* data length */ )
{
     int r;
     datum d, k;

     if (h == NULL) {
	  elog_safeprintf(ERROR, "holstore not opened");
	  return 0;
     }

     if (!h->trans && h->ref != NULL) {
	  elog_safeprintf(ERROR, "db inconsistently open");
	  return 0;
     }

     if (h->trans && h->ref == NULL) {
	  elog_safeprintf(ERROR, "db inconsistently closed");
	  return 0;
     }

     if (h->trans == 1) {
	  elog_safeprintf(ERROR, "called in read transaction");
	  return 0;
     }

     /* store data with the specified length, store key as a string WITH
      * its null byte. This makes extraction easier in hol_get() etc. */
     d.dptr  = dat;
     d.dsize = length;
     k.dptr  = key;
     k.dsize = strlen(key)+1;

     if (!h->trans)
	  if (!hol_dbopen(h, "hol_put()", 'w'))
	       return 0;	/* failure */

     r = hol_dbreplace(h, k, d);		/* store */

     if (!h->trans)
	  hol_dbclose(h);

     if (r)
	  r = 0;	/* Failure */
     else
	  r = 1;	/* Success */
     return r;
}

/*
 * Read data associcated with a key.
 * Returns the datum if successful or NULL on failure. Length will be set
 * to the size of the datum.
 * NB Please use nfree() to release the returned datum once you have 
 * finished with it.
 */
void *hol_get(HOLD h, 		/* holstore descriptor */
	      char *key, 	/* data key */
	      int *length	/* data length */ )
{
     datum d, k;

     if (h == NULL) {
	  elog_safeprintf(ERROR, "holstore not opened");
	  return NULL;
     }

     if (!h->trans && h->ref != NULL) {
	  elog_safeprintf(ERROR, "db inconsistently open");
	  return 0;
     }

     if (h->trans && h->ref == NULL) {
	  elog_safeprintf(ERROR, "db inconsistently closed");
	  return 0;
     }

     k.dptr  = key;
     k.dsize = strlen(key)+1;

     if (!h->trans)
	  if (!hol_dbopen(h, "hol_get()", 'r'))
	       return 0;	/* failure */

     d = hol_dbfetch(h, k);
     nadopt(d.dptr);	/* monitor this memory */

     if (!h->trans)
	  hol_dbclose(h);

     *length = d.dsize;

     return d.dptr;
}

/*
 * Remove datum from holstore associated with key.
 * Return 1 if successful or 0 on error.
 */
int hol_rm(HOLD h, 		/* holstore descriptor */
	   char *key		/* data key */ )
{
     int r;
     datum k;

     if (h == NULL) {
	  elog_safeprintf(ERROR, "holstore not opened");
	  return 0;
     }

     if (!h->trans && h->ref != NULL) {
	  elog_safeprintf(ERROR, "db inconsistently open");
	  return 0;
     }

     if (h->trans && h->ref == NULL) {
	  elog_safeprintf(ERROR, "db inconsistently closed");
	  return 0;
     }

     k.dptr  = key;
     k.dsize = strlen(key)+1;

     if (!h->trans)
	  if (!hol_dbopen(h, "hol_rm()", 'w'))
	       return 0;	/* failure */

     r = hol_dbdelete(h, k);

     if (!h->trans)
	  hol_dbclose(h);

     return r+1;
}

/*
 * Searches through the store looking for keys or values that match
 * the given regular expressions. The key or value may be NULL if you
 * do not want to search for it; it is equivolent to wild card `*'.
 * Keys are always strings, but the value or data is a void *.
 * Only request a value or data search if it makes sense and is a string.
 * Warning! this is a sequential search and may take a long period of time.
 * Returns a TREE of key value pairs or NULL if error. Data lengths are
 * not returned and thus the values in the tree are only usable if they 
 * are strings. All returned values have an additional null appended
 * to ensure the data is null terminated.
 * Hol_freesearch() is provided to clear up TREE structure after use.
 * Hol_search() must be used in a transaction.
 */
TREE *hol_search(HOLD h, 		/* holstore descriptor */
		 char *key_regex, 	/**/
		 char *value_regex	/**/ )
{
     regex_t key_pattern, value_pattern;
     char errbuf[HOLSTORE_ERRBUFSZ];
     int r, match;
     TREE *rec;
     char *d;		/* all data considered strings for search */
     char *k;
     int dlen;

     if (h == NULL) {
	  elog_safeprintf(ERROR, "holstore not opened");
	  return NULL;
     }

     /* Compile regular expressions */
     if (key_regex)
	  if ((r = regcomp(&key_pattern, key_regex, REG_EXTENDED|REG_NOSUB))) {
	       regerror(r, &key_pattern, errbuf, HOLSTORE_ERRBUFSZ);
	       elog_safeprintf(ERROR, 
			   "problem with key pattern: %s, error is %s", 
			   key_pattern, errbuf);
	       return NULL;
	  }
     if (value_regex)
	  if ((r = regcomp(&value_pattern, value_regex, 
			  REG_EXTENDED|REG_NOSUB))) {
	       regerror(r, &value_pattern, errbuf, HOLSTORE_ERRBUFSZ);
	       elog_safeprintf(ERROR, 
			   "problem with value pattern: %s, error is %s", 
			   key_pattern, errbuf);
	       return NULL;
	  }

     rec = tree_create();

     /* first record */
     d = hol_readfirst(h, &k, &dlen);
     if (d == NULL) {
	  hol_readend(h);
	  /*hol_commit(h);*/
	  return rec;	/* Assume all was OK, but hol was just empty */
     }

     /* key/value combination logic */
     match = 0;
     if (key_regex)
	  if ( ! regexec(&key_pattern, k, 0, NULL,0))
	       match++;
     if (value_regex) {
	  /* when searching for data, we want to treat void * block 
	   * as string and extend the nmalloc()ed block by one to 
	   * append a null */
	  d = xnrealloc(d, dlen+1);
	  d[dlen] = '\0';
	  if ( ! regexec(&value_pattern, d, 0, NULL,0))
	       match++;
     }

     if (key_regex && value_regex && match == 2)
	  tree_add(rec, xnstrdup(k), d);
     else if (! key_regex && value_regex && match == 1)
	  tree_add(rec, xnstrdup(k), d);
     else if (key_regex && ! value_regex && match == 1) {
	  d = xnrealloc(d, dlen+1);
	  d[dlen] = '\0';
	  tree_add(rec, xnstrdup(k), d);
     } else if ( ! (key_regex || value_regex)) {
	  d = xnrealloc(d, dlen+1);
	  d[dlen] = '\0';
	  tree_add(rec, xnstrdup(k), d);
     } else
	  nfree(d);

     while (d != NULL) {
	  d = hol_readnext(h, &k, &dlen);
	  if (d == NULL)
	       break;

	  /* key/value combination logic */
	  match = 0;
	  if (key_regex)
	       if ( ! regexec(&key_pattern, k, 0, NULL,0))
		    match++;
	  if (value_regex) {
	       /* when searching for data, we want to treat void * block 
		* as string and extend the nmalloc()ed block by one to 
		* append a null */
	       d = xnrealloc(d, dlen+1);
	       d[dlen] = '\0';
	       if ( ! regexec(&value_pattern, d, 0, NULL,0))
		    match++;
	  }

	  if (key_regex && value_regex && match == 2)
	       tree_add(rec, xnstrdup(k), d);
	  else if (! key_regex && value_regex && match == 1)
	       tree_add(rec, xnstrdup(k), d);
	  else if (key_regex && ! value_regex && match == 1) {
	       d = xnrealloc(d, dlen+1);
	       d[dlen] = '\0';
	       tree_add(rec, xnstrdup(k), d);
	  } else if ( ! (key_regex || value_regex)) {
	       d = xnrealloc(d, dlen+1);
	       d[dlen] = '\0';
	       tree_add(rec, xnstrdup(k), d);
	  } else
	       nfree(d);
     }

     if (key_regex)
	  regfree(&key_pattern);
     if (value_regex)
	  regfree(&value_pattern);

     return (rec);
}

/* clear up tree list created by hol_search() above */
void hol_freesearch(TREE* list)
{
     tree_traverse(list) {
	  nfree(tree_get(list));
	  nfree(tree_getkey(list));
     }
     tree_destroy(list);
}


/*
 * Start a transaction using locking.
 * Everything between the begintrans and the commit will be safe from
 * other readers and writers.
 * Mode can be set to 'r' for read lock only or 'w' for a write lock.
 * Currently, rollback() is not implemented, so changes may not be undone.
 * Returns 1 for successful transaction start or 0 for problem, when no 
 * lock has been granted.
 */
int hol_begintrans(HOLD h, char mode)
{
     if (h == NULL) {
	  elog_safeprintf(ERROR, "holstore not opened");
	  return 0;
     }

     if (h->inhibtrans)
          return 1;		/* inhibit causes a success */

     if (h->trans) {
#if 0
          elog_safeprintf(WARNING,"already in %s transaction",
		      (h->trans == 1 ? "read" : "write") );
#else
	  fprintf(stderr, "hol_begintrans() already in %s transaction\n",
		  (h->trans == 1 ? "read" : "write") );
#endif
	  return 0;
     }

     if (h->ref != NULL) {
	  elog_safeprintf(WARNING, "db inconsistently open");
	  return 0;
     }

     switch (mode) {
     case 'w':
	  if (!hol_dbopen(h, "hol_begintrans()", 'w'))
	       return 0;	/* failure */
	  h->trans = 2;
	  break;
     case 'r':
	  if (!hol_dbopen(h, "hol_begintrans()", 'r'))
	       return 0;	/* failure */
	  h->trans = 1;
	  break;
     default:
          elog_safeprintf(DEBUG, "called with mode=%c", 
		      mode);
	  return 0;
     }

     return 1;
}

/*
 * Ends a reading transaction. It is an error to call this outside a 
 * read transaction [without a call to begintrans(..., 'r' ) first].
 * Returns 1 for success or 0 for an error, if it returns at all.
 * Some errors are programming ones and should cause core dumps to
 * fix the bugs.
 */
int hol_endtrans(HOLD h)
{
     if (h == NULL) {
          elog_die(FATAL, "holstore not opened");
	  return 0;
     }
     if (h->inhibtrans)
          return 1;		/* inhibit causes a success */
     if (h->ref == NULL) {
          elog_die(FATAL, "underlying db not open");
	  return 0;
     }
     if (h->trans != 1) {
          elog_die(FATAL, "not a read transation");
	  return 0;
     }

     return hol_commit(h);
}

/*
 * Backs out all changes in the database made during the current transaction.
 * There must be a current transaction open at the time this is called.
 * when this call is finished, the transaction will have come to an end.
 * Not yet implemented
 */
int hol_rollback(HOLD h)
{
     if (h == NULL) {
          elog_die(FATAL, "holstore not opened");
	  return 0;
     }
     if (h->inhibtrans)
          return 1;		/* inhibit causes a success */
     if (h->ref == NULL) {
          elog_die(FATAL, "underlying db not open");
	  return 0;
     }

#if 0
     elog_safeprintf(DEBUG, "rollback not yet implemented");
#endif

     return hol_commit(h);
}

/*
 * Finish the transaction, saving all changes made and removing the 
 * ability to roll back.
 */
int hol_commit(HOLD h)
{
     if (h == NULL) {
          elog_die(FATAL, "holstore not opened");
	  return 0;
     }
     if (h->inhibtrans)
          return 1;		/* inhibit causes a success */
     if (!h->trans) {
          elog_die(FATAL, "not in transaction");
	  return 0;
     }
     if (h->ref == NULL) {
          elog_die(FATAL, "db inconsistently closed");
	  return 0;
     }

     h->trans = 0;
     hol_dbclose(h);
     return 0;
}

/* Inhibit transaction calls. Following this call, subsequent
 * calls to hol_begintrans(), hol_endtrans(), hol_rollback(), hol_commit()
 * will be ignored.
 * Normal operation is resumed with a hol_allowtrans() call.
 * Further calls to hol_inhibittrans() will stack but no more action
 * will be taken, so calls to hol_inhibittrans() should be matched with
 * calls to hol_allowtrans().
 * Be VERY careful and use sparingly. Inhibition must be surrounded by 
 * a transaction, preferably of the write veriety.
 * Returns the number of inhibit calls in place before the call; thus 0
 * means this is the first in force.
 */
int hol_inhibittrans(HOLD  h)
{
     if (h->inhibtrans)
          elog_safeprintf(DEBUG, 
		      "transactions already inhibited, now %d deep", 
		      h->inhibtrans+1);
     return ++h->inhibtrans;
}

/*
 * Decrement the inhibition counter by 1 and allow normal transactions 
 * to continue if the counter == 0.
 */
int hol_allowtrans(HOLD  h)
{
     if (h->inhibtrans <= 0)
          elog_die(ERROR, "no inhibitions");
     return --h->inhibtrans;
}

/*
 * Start a read traversal of the entire holstore.
 * Read traversal must take place in a read or write transaction.
 * Following a successful hol_readfirst(), it should be followed by
 * hol_readnext() for the next record or hol_readend() to finish the
 * traversal and clear any storage.
 * Data returned by the hol_read* family, should be freed by the caller 
 * when finished, but the keys passed back will be freed by successive
 * calls to hol_readnext() or hol_readend().
 * Returns the first data, with its size set in length and its key.
 * On failure of if the database is empty, NULL is returned, key is
 * set to NULL and length will be set to -1.
 * The traversal will skip the superblock record.
 */
void *hol_readfirst(HOLD h, char **key, int *length)
{
     datum d, k;

     if (h == NULL) {
          elog_safeprintf(ERROR, "holstore not opened");
	  *key = NULL;
	  *length = -1;
	  return NULL;
     }

     if (!h->trans) {
          elog_safeprintf(ERROR, "must be in a transaction");
	  *key = NULL;
	  *length = -1;
	  return NULL;
     }

     if (h->ref == NULL) {
          elog_safeprintf(ERROR, "db inconsistently closed");
	  *key = NULL;
	  *length = -1;
	  return NULL;
     }

     k = hol_dbfirstkey(h);
     nadopt(k.dptr);		/* monitor this memory */
     if (k.dptr == NULL) {
	  hol_dbclose(h);
	  *key = NULL;
	  *length = -1;
	  return NULL;
     }
     if (!strncmp(HOLSTORE_SUPERNAME, k.dptr, HOLSTORE_SUPERNLEN) && 
	 k.dsize == HOLSTORE_SUPERNLEN) {
	  /* first record was superblock! do it again!! */
	  if (h->lastkey.dptr != NULL) {
	       nfree(h->lastkey.dptr);
	       h->lastkey.dptr = NULL;
	  }
	  h->lastkey = k;
	  k = hol_dbnextkey(h, h->lastkey);
	  nadopt(k.dptr);	/* monitor this memory */
	  if (k.dptr == NULL) {
	       *key = NULL;
	       *length = -1;
	       return NULL;
	  }
     }

     /* store the fist key and get its data */
     if (h->lastkey.dptr != NULL) {
          nfree(h->lastkey.dptr);
	  h->lastkey.dptr = NULL;
     }
     h->lastkey = k;
     d = hol_dbfetch(h, h->lastkey);
     nadopt(d.dptr);		/* monitor this memory */

     /* return the data */
     /*h->trans = 1;*/		/* transaction begun */
     *key = k.dptr;
     *length = d.dsize;
     return d.dptr;
}

/*
 * Return the next record in the holstore.
 * Must be preceeded by a hol_readnext or hol_readfirst.
 * Will not return the superblock record.
 * The caller should free the data when finished with nfree(), but leave 
 * the key alone as it will be freed by hol_readnext() or hol_readend();
 * Return NULL for error or if the database is exhausted.
 */
void *hol_readnext(HOLD h, char **key, int *length)
{
     datum d, k;

     if (h == NULL) {
          elog_safeprintf(ERROR, "holstore not opened");
	  *key = NULL;
	  *length = -1;
	  return NULL;
     }

     if (!h->trans) {
          elog_safeprintf(ERROR, "not in transaction");
	  *key = NULL;
	  *length = -1;
	  return NULL;
     }

     if (h->ref == NULL) {
          elog_safeprintf(ERROR, "db inconsistently closed");
	  *key = NULL;
	  *length = -1;
	  return NULL;
     }

     if (h->lastkey.dptr == NULL) {
          elog_safeprintf(ERROR, "reached the last record");
	  *key = NULL;
	  *length = -1;
	  return NULL;
     }
     /* find the next key */
     do {
	  k = hol_dbnextkey(h, h->lastkey);
	  nadopt(k.dptr);		/* monitor this memory */
	  if (h->lastkey.dptr != NULL) {
	       nfree(h->lastkey.dptr);
	       h->lastkey.dptr = NULL;
	  }
	  if (k.dptr == NULL) {
	       *key = NULL;
	       *length = -1;
	       return NULL;
	  }
	  h->lastkey = k;
     } while (!strncmp(HOLSTORE_SUPERNAME, k.dptr, HOLSTORE_SUPERNLEN) && 
	      k.dsize == HOLSTORE_SUPERNLEN);

     /* store the next key and get its data */
     d = hol_dbfetch(h, h->lastkey);
     nadopt(d.dptr);		/* monitor this memory */

     /* return the data */
     *key = k.dptr;
     *length = d.dsize;
     return d.dptr;
}

/*
 * End the read traversal, clear up storage and free locks
 */
void hol_readend(HOLD h)
{
     if (h == NULL) {
          elog_safeprintf(ERROR, "holstore not opened");
	  return;
     }

     if (!h->trans) {
          elog_safeprintf(ERROR, "not in a transaction");
	  return;
     }

     if (h->ref == NULL) {
          elog_safeprintf(ERROR, "inconsistently closed");
	  return;
     }

     /* Free storage */
     if (h->lastkey.dptr != NULL) {
	  nfree(h->lastkey.dptr);	/* not nfree() as DBM provided it */
	  h->lastkey.dptr = NULL;
     }
     return;
}

/*
 * Checkpoint the holsore, removing logs and the ability to rollback
 */
void  hol_checkpoint(HOLD h) {
     if (h == NULL) {
          elog_safeprintf(ERROR, "holstore not opened");
	  return;
     }

     if (h->trans) {
          elog_safeprintf(ERROR, "in a transaction");
	  return;
     }

     if (h->ref != NULL) {
          elog_safeprintf(ERROR, "inconsistently open");
	  return;
     }

     /* With DBM API, this means reorganise */
     if ( ! hol_dbopen(h, "hol_checkpoint()", 'w') )
          return;
     hol_dbreorganise(h);
     hol_dbclose(h);
}


/*
 * Print the database to the screen, one line per record.
 * Traverses the database to read records, using a transaction if
 * one is not available.
 * Returns the number of records (and therefore lines) printed
 */
int hol_contents(HOLD h)
{
     void *d;
     char *k, *ddump /*ddump[66]*/;
     int inTrans;
     int dlen, ln=0;

     if (h == NULL) {
          elog_safeprintf(ERROR, "holstore not opened");
	  return 0;
     }

     if (!(inTrans=h->trans))
	  hol_begintrans(h, 'r');
     d = hol_readfirst(h, &k, &dlen);
     if (d == NULL) {
	  hol_readend(h);
	  hol_commit(h);
	  return 0;
     }
     elog_startsend(DEBUG, "Contents of holstore ----------\n");
     ln++;
#if 0
     strcstr(ddump, 65, d, dlen);
     ddump[64] = '\0';		/* strcstr does not terminate on max */
#endif
     ddump = util_bintostr(65, d, dlen);
     elog_contprintf(DEBUG, "%14s %s\n", k, ddump);
     nfree(d);
     nfree(ddump);

     while (d != NULL) {
	  d = hol_readnext(h, &k, &dlen);
	  if (d == NULL)
	       break;
#if 0
	  strcstr(ddump, 65, d, dlen);
	  ddump[64] = '\0';	/* strcstr does not terminate on max */
#endif
	  ddump = util_bintostr(65, d, dlen);
	  elog_contprintf(DEBUG, "%14s %s\n", k, ddump);
	  nfree(d);
	  nfree(ddump);
	  ln++;
     }

     hol_readend(h);
     if (!inTrans)
	  hol_commit(h);

     elog_endsend(DEBUG, "-----------------------------------");

     return ln;
}


/* Return the size of the holstore or -1 for error */
int hol_footprint(HOLD h)
{
	struct stat buf;
	if (stat(h->name, &buf) == -1)
		return -1;
	return(buf.st_size);
}

/* Return the available space into which holstore can grow or -1 for error */
int   hol_remain(HOLD h)
{
#if linux
     struct statfs buf;
     if (statfs(h->name, &buf))
	  return -1;
#else
     struct statvfs buf;
     if (statvfs(h->name, &buf))
	  return -1;
#endif
     return (buf.f_bavail * buf.f_bsize);
}


/* Change the superblock values to the ones given in the function.
 * If an item is not to be replaced, use NULL for char * and -1 for
 * time_t or int.
 */
int   hol_setsuper(HOLD h, char *platform, char *host, char *os, 
		   time_t created, int version)
{
     datum d, k;
     char superblock[HOLSTORE_SUPERMAX];
     int r;

     /* carry out saftey checks */
     if (h == NULL) {
	  elog_safeprintf(ERROR, "holstore not opened");
	  return 0;
     }

     if (!h->trans && h->ref != NULL) {
	  elog_safeprintf(ERROR, "db inconsistently open");
	  return 0;
     }

     if (h->trans && h->ref == NULL) {
	  elog_safeprintf(ERROR, "db inconsistently closed");
	  return 0;
     }

     if (h->trans == 1) {
	  elog_safeprintf(ERROR, "called in read transaction");
	  return 0;
     }

     /* patch the structure */
     if (platform) {
	  strncpy(h->sysbuf.machine, platform, SYS_NMLN);
	  h->sysbuf.machine[SYS_NMLN] = '\0';
     }
     if (host) {
	  strncpy(h->sysbuf.nodename, host, SYS_NMLN);
	  h->sysbuf.nodename[SYS_NMLN] = '\0';
     }
     if (os) {
	  strncpy(h->sysbuf.sysname, os, SYS_NMLN);
	  h->sysbuf.sysname[SYS_NMLN] = '\0';
     }
     if (created != -1)
	  h->created = created;
     if (version != -1)
	  h->version = version;

     /* prepare the data */
     k.dptr = HOLSTORE_SUPERNAME;
     k.dsize = HOLSTORE_SUPERNLEN;
     d.dsize = 1+sprintf(superblock, "%s|%d|%ld|%s|%s|%s|%s|%s", 
			 HOLSTORE_MAGIC, h->version, (long) h->created, 
			 h->sysbuf.sysname, h->sysbuf.nodename, 
			 h->sysbuf.release, h->sysbuf.version, 
			 h->sysbuf.machine);
     d.dptr = superblock;

     /* store the patched superblock */
     if (!h->trans)
	  if (!hol_dbopen(h, "hol_setsuper()", 'w'))
	       return 0;	/* failure */

     r = hol_dbreplace(h, k, d);
     hol_dbclose(h);

     if (r) {
	  /* error: unable to store superblock */
	  elog_safeprintf(ERROR, "unable to store superblock");
	  return 0;
     }


     return 1;
}




/* --------------- Private routines ----------------- */

/* Error handling routine when a database goes wrong */
void hol_dberr()
{
#if DB_GDBM
     elog_safeprintf(ERROR, "DBM error: %d - %s", gdbm_errno, 
		 gdbm_strerror(gdbm_errno));
#elif DB_RDBM
#else
#endif	/* DB_GDBM */ 
}

/*
 * Private: Open DBM and handle erors.
 * 'where' is the name of the calling routine, 'readwrite' is the
 * mode. r=read only, t=read test (no message generated), w=write,
 * c=create.
 * Returns 1 if successful or 0 for error
 */
int hol_dbopen(HOLD h, char *where, char readwrite)
{
#if DB_GDBM	/* ------------------------------------------------------ */
     GDBM_FILE db;
     int i;
     struct timespec t;

#if 0
     /* the debug log below can be very annoying!! use with caution */
     elog_safeprintf(DEBUG, "called from %s mode %c name %s",
		 where, readwrite, h->name);
#endif

     if (h->ref)
          elog_safeprintf(ERROR, 
		      "error DBM file %s already open", h->name);

     /* loop to retry dbm repeatedly to get a lock */
     for (i=0; i < hol_ntrys; i++) {
          switch (readwrite) {
	  case 'r':	/* read */
	  case 't':	/* test */
	       if (access(h->name, F_OK) == -1)
		    return 0;
	       db = gdbm_open(h->name, 0, GDBM_READER || GDBM_NOLOCK, 
			      h->mode, hol_dberr);
	       break;
	  case 'w':	/* write */
	       db = gdbm_open(h->name, 0, GDBM_WRITER, h->mode, hol_dberr);
	       break;
	  case 'c':	/* create */
	       db = gdbm_open(h->name, 0, GDBM_WRCREAT, h->mode, hol_dberr);
	       break;
	  default:
	       elog_safeprintf(ERROR, 
			   "%s unsupported action: %c", where, readwrite);
	       return 0;
	       break;
	  }

	  /* got a lock and opened database */
	  if (db) {
	       h->ref = db;
	       return 1;
	  }

	  /* Allow lock failures to proceed with another try */
	  if (gdbm_errno != GDBM_CANT_BE_READER &&
	      gdbm_errno != GDBM_CANT_BE_WRITER)
	       break;

#if 0
	  /* log waiting */
	  /*route_printf(INFO, "hol_dbopen()", 0, */
	  fprintf(stderr,
		      "%s attempt %d on %s mode %c failed (%d: %s); retry",
		      where, i, h->name, readwrite, gdbm_errno,
		      gdbm_strerror(gdbm_errno));
#endif

	  /* wait waittry nanoseconds */
	  t.tv_sec = 0;
	  t.tv_nsec = hol_waittry;
	  nanosleep(&t, NULL);
     }

     /* fatally failed to open gdbm file */
     if (readwrite != 't')
	  elog_safeprintf(DIAG, 
		      "%s unable to open %s mode %c (%d: %s)", where, h->name,
		      readwrite, gdbm_errno, gdbm_strerror(gdbm_errno));
#elif DB_RDBM	/* ------------------------------------------------------ */
     int i, r;
     struct timespec t;

#if 0
     /* the debug log below can be very annoying!! use with caution */
     elog_safeprintf(DEBUG, "called from %s mode %c name %s",
		 where, readwrite, h->name);
#endif

     if (h->ref)
          elog_safeprintf(ERROR, 
		      "error DBM file %s already open", h->name);

     /* loop to retry dbm repeatedly to get a lock */
     for (i=0; i < hol_ntrys; i++) {
	  /* rdb has not yet implemented the difference between read and
	   * write locking, prefering to use a server approach instead.
	   * It would not be hard to write, so I'm going to keep the
	   * switch code here just in case.
	   */
          switch (readwrite) {
	  case 'r':	/* read */
	  case 't':	/* test */
	       if (access(h->name, R_OK) && access(h->logname, R_OK)) {
		    r = -10;		/* call would fail */
		    break;
	       }
	  case 'w':	/* write */
	  case 'c':	/* create */
	       r = rdb_open(&h->db, h->name, h->logname, h->mode, 1021);
	       break;
	  default:
	       elog_safeprintf(ERROR, 
			   "%s unsupported action: %c", where, readwrite);
	       return 0;
	       break;
	  }

	  /* got a lock and opened database */
	  if (r == 0) {
	       h->ref = &h->db;
	       return 1;
	  }

	  /* Allow lock failures to proceed with another try */
	  if (r != -2)
	       break;

#if 0
	  /*route_printf(INFO, "hol_dbopen()", 0, */
	  fprintf(stderr,
		      "%s attempt %d on %s mode %c failed (%d); retry",
		      where, i, h->name, readwrite, r);
#endif

	  /* wait waittry nanoseconds */
	  t.tv_sec = 0;
	  t.tv_nsec = hol_waittry;
	  nanosleep(&t, NULL);
     }

     /* fatally failed to open gdbm file */
     if (readwrite != 't')
	  elog_safeprintf(ERROR, 
		      "%s attempt %d on %s mode %c failed (%d)",
		      where, i, h->name, readwrite, r);
#else
#endif	/* DB_GDBM */ 

     return 0;
}

/* Private: close an already opened DBM */
void hol_dbclose(HOLD h)
{
#if DB_GDBM
#if 0
     /* the debug log below can be very annoying!! use with caution */
     elog_safeprintf(DEBUG, "called file %s", h->name);
#endif

     gdbm_close(h->ref);
     h->ref = NULL;
#elif DB_RDBM
     int r;

     r = rdb_close(h->ref);
     if (r != 0)
	  elog_safeprintf(ERROR, "close did not work file %s", h->name);
     h->ref = NULL;
#else
#endif	/* DB_GDBM */ 
}


/* Private: fetch data from a DBM and return the datum */
datum hol_dbfetch(HOLD h, datum key)
{
#if DB_GDBM
     return gdbm_fetch(h->ref, key);
#elif DB_RDBM
     datum d;

     d = rdb_fetch(h->ref, key);
     if (d.dptr)
	  d.dptr = xmemdup(d.dptr, d.dsize);	/* non-adopted but checked */
     return d;
#else
#endif	/* DB_GDBM */ 
}


/* Private: replace data in a DBM, overwriting previously stored values
 * Returns 0 for success, non-0 for error */
int hol_dbreplace(HOLD h, datum key, datum value)
{
#if DB_GDBM
     return gdbm_store(h->ref, key, value, GDBM_REPLACE);
#elif DB_RDBM
     return rdb_store(h->ref, key, value, 1 /*overwrite*/);
#else
#endif	/* DB_GDBM */ 
}


/* Private: delete the data identified by key.
 * Returns 0 for success, -1 for error */
int hol_dbdelete(HOLD h, datum key)
{
#if DB_GDBM
     return gdbm_delete(h->ref, key);
#elif DB_RDBM
     return rdb_delete(h->ref, key);
#else
#endif	/* DB_GDBM */ 
}


/* Private: fetch the first datum from a DBM and return it */
datum hol_dbfirstkey(HOLD h)
{
#if DB_GDBM
     return gdbm_firstkey(h->ref);
#elif DB_RDBM
     datum d;

     d = rdb_firstkey(h->ref);
     if (d.dptr)
	  d.dptr = xmemdup(d.dptr, d.dsize);	/* non-adopted but checked */
     return d;
#else
#endif	/* DB_GDBM */ 
}


/* Private: fetch the next datum from a DBM given the key of the last datum.
 * Returns the datum */
datum hol_dbnextkey(HOLD h, datum lastkey)
{
#if DB_GDBM
     return gdbm_nextkey(h->ref, lastkey);
#elif DB_RDBM
     datum d;

     d = rdb_nextkey(h->ref, lastkey);
     if (d.dptr)
	  d.dptr = xmemdup(d.dptr, d.dsize);	/* non-adopted but checked */
     return d;
#else
#endif	/* DB_GDBM */ 
}


/* Private: reoganise the DBM */
int hol_dbreorganise(HOLD h)
{
#if DB_GDBM
     return gdbm_reorganize(h->ref);
#elif DB_RDBM
     return rdb_reorganize(h->ref);
#else
#endif	/* DB_GDBM */ 
}



#if TEST

#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#define TESTHOL1 "t.hol.1.dat"
#define TESTLOG1 "t.hol.1.dat.log"
#define TEST_BUFLEN 50
#define TEST_ITER 100

int main()
{
     HOLD hd;
     int r, i, len1, len2, len3;
     char *dat1, *dat2, *dat3, *key1; 
     char datbuf1[TEST_BUFLEN], keybuf1[TEST_BUFLEN];
     struct timeval tv1, tv2;
     struct timezone tz;
     double t1, t2;
     TREE *list1;

     route_init(NULL, 0);
     elog_init(0, "holstore test", NULL);
     hol_init(0, 0);

     /*
      * These tests are designed to check the correct working of the
      * interface abstraction, not the DB itself. We assume that that has
      * been fully tested and that it works!!
      */
     unlink(TESTHOL1);
     unlink(TESTLOG1);

     /* test 1: open and close */
     hd = hol_create(TESTHOL1, 0644);
     if (hd == NULL) {
	  fprintf(stderr, "[1] Unable to open holstore\n");
	  exit(1);
     }
     hol_close(hd);

     /* test 2: open, write and close */
     hd = hol_open(TESTHOL1);
     if (hd == NULL) {
	  fprintf(stderr, "[2] Unable to open holstore\n");
	  exit(1);
     }
     r = hol_put(hd, "nigel", "Hello, my name is nigel", 24);
     if (r == 0) {
	  fprintf(stderr, "[2] Unable to write to holstore\n");
	  exit(1);
     }
     hol_close(hd);

     /* test 3: open, read and close */
     hd = hol_open(TESTHOL1);
     if (hd == NULL) {
	  fprintf(stderr, "[3] Unable to open holstore\n");
	  exit(1);
     }
     dat1 = hol_get(hd, "nigel", &len1);
     if (dat1 == NULL) {
	  fprintf(stderr, "[3] Unable to read from holstore\n");
	  exit(1);
     }
     if (strcmp(dat1, "Hello, my name is nigel")) {
	  fprintf(stderr, "[3] Data does not compare\n");
	  exit(1);
     }
     if (len1 != strlen(dat1)+1) {	/* include null */
	  fprintf(stderr, "[3] Data lengths are not the same\n");
	  exit(1);
     }
     hol_close(hd);

     /* test 4: open delete and close */
     hd = hol_open(TESTHOL1);
     if (hd == NULL) {
	  fprintf(stderr, "[4] Unable to open holstore\n");
	  exit(1);
     }
     r = hol_rm(hd, "nigel");
     if (r == 0) {
	  fprintf(stderr, "[4] Unable to delete from holstore\n");
	  exit(1);
     }
     if (strcmp(dat1, "Hello, my name is nigel")) {
	  fprintf(stderr, "[4] Data does not compare\n");
	  exit(1);
     }
     nfree(dat1);
     hol_close(hd);

     /* test 5: open, get nothing and close */
     hd = hol_open(TESTHOL1);
     if (hd == NULL) {
	  fprintf(stderr, "[5] Unable to open holstore\n");
	  exit(1);
     }
     dat1 = hol_get(hd, "nigel", &len1);
     if (dat1 != NULL) {
	  fprintf(stderr, "[5] Shouldnt have read from holstore\n");
	  exit(1);
     }
     hol_close(hd);

     /* test 6: print the superblock */
     hd = hol_open(TESTHOL1);
     if (hd == NULL) {
	  fprintf(stderr, "[6] Unable to open holstore\n");
	  exit(1);
     }
     printf("Holstore file %s, created at %ld on %s using %s (%s)\n", 
	    TESTHOL1, hol_created(hd), hol_host(hd), hol_os(hd), 
	    hol_platform(hd));
     hol_close(hd);

     /* test 7: traverse the database */
     hd = hol_open(TESTHOL1);
     if (hd == NULL) {
	  fprintf(stderr, "[7] Unable to open holstore\n");
	  exit(1);
     }
     hol_put(hd, "rec1", "first record", 13);
     hol_put(hd, "rec2", "second record", 14);
     hol_begintrans(hd, 'r');
     dat1 = hol_readfirst(hd, &key1, &len1);
     if (dat1 == NULL)
	  fprintf(stderr, "[7] Unable to traverse first record\n");
     else {
	  printf("sequence 1 - %s\n", dat1);
	  nfree(dat1);
	  dat1 = hol_readfirst(hd, &key1, &len1);
	  if (dat1 == NULL) {
	       fprintf(stderr, "[7] Unable to restart traversal\n");
	  } else {
	       nfree(dat1);
	       dat1 = hol_readnext(hd, &key1, &len1);
	       if (dat1 == NULL)
		    fprintf(stderr, "[7] Unable to traverse to second rec\n");
	       else {
		    printf("sequence 2 - %s\n", dat1);
		    nfree(dat1);
		    dat1 = hol_readnext(hd, &key1, &len1);
		    if (dat1 != NULL) {
		         nfree(dat1);
			 fprintf(stderr, "[7] traversal not ending\n");
		    }
	       }
	  }
     }
     hol_readend(hd);
     hol_commit(hd);
     hol_close(hd);

     /* test 8: dump the database */
     hd = hol_open(TESTHOL1);
     if (hd == NULL) {
	  fprintf(stderr, "[8] Unable to open holstore\n");
	  exit(1);
     }
     hol_contents(hd);
     hol_close(hd);

     /* test 9: carry out a write lock trasaction */
     hd = hol_open(TESTHOL1);
     if (hd == NULL) {
	  fprintf(stderr, "[9] Unable to open holstore\n");
	  exit(1);
     }

     hol_begintrans(hd, 'w');
     dat1 = hol_get(hd, "rec1", &len1);
     dat2 = hol_get(hd, "rec2", &len2);
     if (dat1 == NULL || dat2 == NULL || len1 <= 0 || len2 <= 0)
	  fprintf(stderr, "[9] Problem getting existing records\n");

     r = hol_put(hd, "rec1", "This is a replacement value", 28);
     if (!r)
	  fprintf(stderr, "[9] Problem putting new rec1\n");

     dat3 = hol_get(hd, "rec1", &len3);
     if (dat1 == dat3 || !strcmp(dat1, dat3) || len1 == len3)
	  fprintf(stderr, "[9] old rec1 == new rec1!!\n");
     nfree(dat3);

     r = hol_put(hd, "rec2", "I am different from the second", 31);
     if (!r)
	  fprintf(stderr, "[9] Problem putting new rec2\n");

     dat3 = hol_get(hd, "rec2", &len3);
     if (dat2 == dat3 || !strcmp(dat2, dat3) || len2 == len3)
	  fprintf(stderr, "[9] old rec2 == new rec2!!\n");

     nfree(dat1);
     nfree(dat2);
     nfree(dat3);
     hol_commit(hd);
     hol_close(hd);

     /* test 10: speed test */
     hd = hol_open(TESTHOL1);
     if (hd == NULL) {
	  fprintf(stderr, "[10] Unable to open holstore\n");
	  exit(1);
     }
     /* test 10a: write new records out of transaction */
     gettimeofday(&tv1, &tz);
     t1 = (double) tv1.tv_usec / 1000000 + tv1.tv_sec;
     for (i=0; i<TEST_ITER; i++) {
	  sprintf(keybuf1, "key %d", i);
	  sprintf(datbuf1, "data %d", i);
	  r = hol_put(hd, keybuf1, datbuf1, strlen(datbuf1+1));
	  if (!r) {
	       fprintf(stderr, "[10a] failed to put %d\n", i);
	       exit(1);
	  }
     }
     gettimeofday(&tv2, &tz);
     t2 = (double) tv2.tv_usec / 1000000 + tv2.tv_sec;
     printf("%d new writes out of transaction: took %f\n", TEST_ITER, t2-t1);

     /* test 10b: read records out of transaction */
     gettimeofday(&tv1, &tz);
     t1 = (double) tv1.tv_usec / 1000000 + tv1.tv_sec;
     for (i=0; i<TEST_ITER; i++) {
	  sprintf(keybuf1, "key %d", i);
	  /*	  sprintf(datbuf1, "data %d", i);*/ /* check it */
	  dat1 = hol_get(hd, keybuf1, &len1);
	  if (dat1 == NULL) {
	       fprintf(stderr, "[10b] failed to get %d\n", i);
	       exit(1);
	  } else
	       nfree(dat1);
     }
     gettimeofday(&tv2, &tz);
     t2 = (double) tv2.tv_usec / 1000000 + tv2.tv_sec;
     printf("%d reads out of transaction: took %f\n", TEST_ITER, t2-t1);

     /* test 10c: update existing records out of transaction */
     gettimeofday(&tv1, &tz);
     t1 = (double) tv1.tv_usec / 1000000 + tv1.tv_sec;
     for (i=0; i<TEST_ITER; i++) {
	  sprintf(keybuf1, "key %d", i);
	  sprintf(datbuf1, "data %d", i);
	  r = hol_put(hd, keybuf1, datbuf1, strlen(datbuf1+1));
	  if (!r) {
	       fprintf(stderr, "[10c] failed to put %d\n", i);
	       exit(1);
	  }
     }
     gettimeofday(&tv2, &tz);
     t2 = (double) tv2.tv_usec / 1000000 + tv2.tv_sec;
     printf("%d updates out of transaction: took %f\n", TEST_ITER, t2-t1);

     /* test 10e: remove records outside transaction */
     gettimeofday(&tv1, &tz);
     t1 = (double) tv1.tv_usec / 1000000 + tv1.tv_sec;
     for (i=0; i<TEST_ITER; i++) {
	  sprintf(keybuf1, "key %d", i);
	  /*	  sprintf(datbuf1, "data %d", i);*/ /* check it */
	  r = hol_rm(hd, keybuf1);
	  if (!r) {
	       fprintf(stderr, "[10e] failed to rm %d\n", i);
	       exit(1);
	  }
     }
     gettimeofday(&tv2, &tz);
     t2 = (double) tv2.tv_usec / 1000000 + tv2.tv_sec;
     printf("%d removes out of transaction: took %f\n", TEST_ITER, t2-t1);

     /* test 10f: write records inside transaction */
     gettimeofday(&tv1, &tz);
     t1 = (double) tv1.tv_usec / 1000000 + tv1.tv_sec;
     hol_begintrans(hd, 'w');
     for (i=0; i<TEST_ITER; i++) {
	  sprintf(keybuf1, "key %d", i);
	  sprintf(datbuf1, "data %d", i);
	  r = hol_put(hd, keybuf1, datbuf1, strlen(datbuf1+1));
	  if (!r) {
	       fprintf(stderr, "[10f] failed to put %d\n", i);
	       exit(1);
	  }
     }
     hol_commit(hd);
     gettimeofday(&tv2, &tz);
     t2 = (double) tv2.tv_usec / 1000000 + tv2.tv_sec;
     printf("%d writes inside transaction: took %f\n", TEST_ITER, t2-t1);

     /* test 10g: read records inside transaction */
     gettimeofday(&tv1, &tz);
     t1 = (double) tv1.tv_usec / 1000000 + tv1.tv_sec;
     hol_begintrans(hd, 'w');
     for (i=0; i<TEST_ITER; i++) {
	  sprintf(keybuf1, "key %d", i);
	  /*	  sprintf(datbuf1, "data %d", i);*/ /* check it */
	  dat1 = hol_get(hd, keybuf1, &len1);
	  if (dat1 == NULL) {
	       fprintf(stderr, "[10g] failed to get %d\n", i);
	       exit(1);
	  } else
	       nfree(dat1);
     }
     hol_commit(hd);
     gettimeofday(&tv2, &tz);
     t2 = (double) tv2.tv_usec / 1000000 + tv2.tv_sec;
     printf("%d reads inside transaction: took %f\n", TEST_ITER, t2-t1);

     /* test 10h: traverse records inside read transaction */
     gettimeofday(&tv1, &tz);
     t1 = (double) tv1.tv_usec / 1000000 + tv1.tv_sec;
     hol_begintrans(hd, 'r');
     dat1 = hol_readfirst(hd, &key1, &len1);
     if (dat1 == NULL) {
	  fprintf(stderr, "[10h] Unable to traverse first record\n");
	  exit(1);
     }
     do {
	  nfree(dat1);
	  dat1 = hol_readnext(hd, &key1, &len1);
     } while (dat1 != NULL);
     hol_readend(hd);
     hol_commit(hd);
     gettimeofday(&tv2, &tz);
     t2 = (double) tv2.tv_usec / 1000000 + tv2.tv_sec;
     printf("%d traversal inside read transaction: took %f\n", TEST_ITER, t2-t1);

     /* test 10i: search for records inside transaction */
     hol_begintrans(hd, 'r');
     list1 = hol_search(hd, "key 7$", NULL);
     hol_commit(hd);
     if (list1 == NULL) {
	  fprintf(stderr, "[10i] no list when finding key 7\n");
	  exit(1);
     }
     if (tree_empty(list1)) {
	  fprintf(stderr, "[10i] empty list when finding key 7\n");
	  exit(1);
     }
     printf("search list ----- (expect key 7)\n");
     tree_traverse(list1)
	  printf("%s   %s\n", tree_getkey(list1), (char *) tree_get(list1));
     printf("end of list -----\n");
     hol_freesearch(list1);

     /* test 10j: update existing records inside transaction */
     gettimeofday(&tv1, &tz);
     t1 = (double) tv1.tv_usec / 1000000 + tv1.tv_sec;
     hol_begintrans(hd, 'w');
     for (i=0; i<TEST_ITER; i++) {
	  sprintf(keybuf1, "key %d", i);
	  sprintf(datbuf1, "data %d", i);
	  r = hol_put(hd, keybuf1, datbuf1, strlen(datbuf1+1));
	  if (!r) {
	       fprintf(stderr, "[10j] failed to put %d\n", i);
	       exit(1);
	  }
     }
     gettimeofday(&tv2, &tz);
     hol_commit(hd);
     t2 = (double) tv2.tv_usec / 1000000 + tv2.tv_sec;
     printf("%d updates inside transaction: took %f\n", TEST_ITER, t2-t1);

     /* test 10k: remove records inside transaction */
     gettimeofday(&tv1, &tz);
     t1 = (double) tv1.tv_usec / 1000000 + tv1.tv_sec;
     hol_begintrans(hd, 'w');
     for (i=0; i<TEST_ITER; i++) {
	  sprintf(keybuf1, "key %d", i);
	  /*	  sprintf(datbuf1, "data %d", i);*/ /* check it */
	  r = hol_rm(hd, keybuf1);
	  if (!r) {
	       fprintf(stderr, "[10k] failed to rm %d\n", i);
	       exit(1);
	  }
     }
     hol_commit(hd);
     gettimeofday(&tv2, &tz);
     t2 = (double) tv2.tv_usec / 1000000 + tv2.tv_sec;
     printf("%d removes inside transaction: took %f\n", TEST_ITER, t2-t1);

     /* test 11: checkpoint */
     hol_checkpoint(hd);

     hol_close(hd);

     elog_fini();
     route_close(err);
     route_fini();

     printf("tests finished successfully\n");
     return 0;
}

#endif /* TEST */
