/* 
 * Ringstore
 * 
 * Provides flexible storage and quick access of time series data in 
 * database file. Designed for Habitat, implementing an low level
 * abstract interface and providing storage for TABLE data types
 *
 * Nigel Stuckey, 2001 & 2003
 *
 * Copyright System Garden Limited 2001-2003. All rights reserved.
 */

#include <time.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "tree.h"
#include "itree.h"
#include "nmalloc.h"
#include "table.h"
#include "tableset.h"
#include "util.h"
#include "hash.h"
#include "rs.h"

/*
 * Description of Ringstore
 *
 * This class stores tabular data over time in sequence.
 * Multiple rings can co-exist in a single file.
 *
 * The storage implements a set of persistant ring buffers in a single
 * disk file (or set of files), with limited or unlimited length
 * (can be a ringbuffer or a queue). If limited in length, old data is 
 * lost as new data `overwrites' its slot.
 *
 * Within each slot, data is input in rows of attributes (using TABLE 
 * data types) where the values share a common sample time. 
 * Multiple instances of the same data type (such as performance of 
 * multiple disks) are held in separate rows in the same sample and 
 * resolved by identifing unique keys.
 *
 * Unique sequencies are automatically allocated to resolve high frequency
 * data (time is only represented in seconds).
 *
 * The default behaviour of insertion may be changed by specifing
 * meta data in the TABLE columns on insertion to give greater flexability.
 * The API is stateful, like file access. You seek, read one or many 
 * records, etc and you close.
 *
 * IMPLEMENTATION
 *
 * The public interface and the high level implementation is provided
 * by this class. It calls a set of pluggable low level routines that 
 * actually store the information to disk and manage the indexes.
 * When an ringstore file is opened, a set of vectors are passed that
 * describe the low level methods.
 *
 * Ring details are kept in a single description table and will be
 * cached for fast access.
 * When table data is stored, the headers are removed and stored in a
 * dictionary indexed by a hash of the contents. The table data is 
 * then stored with the hash key of the header, the time and its key
 * is the ring id and sequence.
 * The time and sequence is stored in an index for fast retreaval
 * of data.
 *
 */

/* statics */
char *rs_ringdir_hds[] = {"name", "dur", "id", "long", "about", "nslots", 
			  NULL};
char *rs_ringidx_hds[] = {"seq", "time", "hd_hash", NULL};
char *rs_info_header_hds[] = {"key", "header", NULL};

/* globals */
TREE *rs_dblock_cache;		/* dblock cache: blocks indexed by their
				 * storage key */

/* private structures */
struct rs_priv_dblock_index {
     time_t time;
     int from, to;
};

/* private functional prototypes */
ITREE *rs_priv_table_to_dblock(TABLE tab, unsigned long hash);
TABLE  rs_priv_dblock_to_table(ITREE *db,RS ring,
			       char *(hash_lookup)(RS, unsigned long),
			       TABLE existing_tab, int musthave_seq,
			       int musthave_time, int musthave_dur);
int    rs_priv_load_index(RS ring, TABLE *index);
unsigned long rs_priv_header_to_hash(RS ring, char *header);
char * rs_priv_hash_to_header(RS ring, unsigned long hdhash);

/* ---- file and ring functions ---- */

/*
 * Initialise the the ringstore class
 */
void  rs_init()
{
     rs_dblock_cache =  tree_create();
}


/*
 * Finalise the ringstore class and shut it down
 */
void  rs_fini()
{
     tree_destroy (rs_dblock_cache);
}


/*
 * Open a ring within a ringstore. 
 * Method should by the type of ringstore to open. Currently, the only 
 * supported type is rs_gdbm_method.
 * If the ringstore file does not exist and flags contain RS_CREATE 
 * as part of its bitmask, then the file `filename' will be created
 * using the permissions contained in `filemode'.
 * Likewise, if the ring does not exist, that it will be created
 * with size of `nslots', long name of 'longname' and text `description' 
 * if RS_CREATE is specified .
 * If the file and ring exists, then filemode, description and nslots will
 * all be ignored.
 * If `nslots' is 0, then the ring will be unlimited in length.
 * If there is a regular timing to each element, then duration should be 
 * set to the interval in seconds or 0 if irregular. 
 * Unless specified by the route, the initial current position will be 
 * set to the oldest.
 * Returns a descriptor if successful or NULL for failure 
 * (see rs_errno() and rs_errstr()).
 */
RS    rs_open(RS_METHOD method	/* method vectors */,
	      char *filename	/* name of file */, 
	      int   filemode	/* permission mode of file */, 
	      char *ringname	/* name of ring within file */,
	      char *longname	/* long name of ring */,
	      char *description	/* text description */, 
	      int   nslots	/* length of ring or 0 for unlimited */,
	      int   duration	/* seconds between samples or 0 irregular */,
	      int   flags	/* mask of RS_CREATE, 0 otherwise */)
{
     struct rs_session *ring;
     RS_LLD lld;
     TABLE ringdir;
     int rowindex;
     RS_SUPER super;

     /* attempt to initialise the method and open the datastore */
     method->ll_init();
     lld = method->ll_open(filename, filemode, flags);

     /* return if failed */
     if ( ! lld )
	  return NULL;

     /* lock up your datastores for READING */
     if ( ! method->ll_lock(lld, RS_RDLOCK, "rs_open") ) {
	  /* if we can't lock at the begining of this process, 
	   * we should fail the entire call */
	  method->ll_close(lld);
	  return NULL;
     }

     /* allocate structures */
     ring = xnmalloc(sizeof(struct rs_session));
     ring->handle = lld;
     ring->method = method;

     /*
      * The database is ours!
      * Get the ring directory and create a new one if ours does not exist
      */
     ringdir = ring->method->ll_read_rings(ring->handle);
     rowindex = table_search2(ringdir, "name", ringname, 
			      "dur", util_i32toa(duration));
     if (rowindex == -1) {
	  if ((flags & RS_CREATE) == 0) {
	       /* can't create new ring */
	       ring->method->ll_close(ring->handle);
	       table_destroy(ringdir);
	       nfree(ring);
	       return NULL;
	  }
	  /* This is a new ring; escalate the lock to a write */
	  if ( ! method->ll_lock(ring->handle, RS_WRLOCK, "rs_open") ) {
	       elog_printf(ERROR, "unable to create ring; "
			   "it may work if you try again");
	       ring->method->ll_close(ring->handle);
	       table_destroy(ringdir);
	       nfree(ring);
	       return NULL;
	  }

	  /* Read super block, create new ring directory entry
	   * and write out */
	  super = ring->method->ll_read_super(ring->handle);
	  table_addemptyrow(ringdir);
	  table_replacecurrentcell_alloc(ringdir,"name", ringname);
	  table_replacecurrentcell_alloc(ringdir,"id",
					 util_i32toa(super->ringcounter++));
	  table_replacecurrentcell_alloc(ringdir,"long", longname);
	  table_replacecurrentcell_alloc(ringdir,"about", description);
	  table_replacecurrentcell_alloc(ringdir,"nslots",util_i32toa(nslots));
	  table_replacecurrentcell_alloc(ringdir,"dur", util_i32toa(duration));
	  rowindex = table_getcurrentrowkey(ringdir);
	  if ( ! ring->method->ll_write_rings(ring->handle, ringdir)) {
	       /* no damage done and we should close and return */
	       elog_printf(ERROR, "unable to write ringdir");
	       ring->method->ll_unlock(ring->handle);
	       ring->method->ll_close (ring->handle);
	       table_destroy(ringdir);
	       nfree(ring);
	       return NULL;
	  }

	  /* update the superblock and also write out */
	  super->generation++;
	  if ( ! ring->method->ll_write_super(ring->handle, super) ) {
	       /* we have written the ring dir but not been able to 
		* update the superblock. New rings will not have the
		* correct ids and the generation counter will be wrong.
		* The transaction is broken and we need to let people know */
	       elog_printf(FATAL, "unable to write superblock; "
			   "datastore needs repair");
	       ring->method->ll_write_value(ring->handle, "DAMAGED", 
					    "superbock", 10);
	       ring->method->ll_unlock(ring->handle);
	       ring->method->ll_close (ring->handle);
	       table_destroy(ringdir);
	       rs_free_superblock(super);
	       nfree(ring);
	       return NULL;
	  }
	  table_gotorow(ringdir, rowindex);
     } else {
	  /* just get an uptodate copy of the superblock */
	  super = ring->method->ll_read_super(ring->handle);
     }

     /* unlock */
     ring->method->ll_unlock(ring->handle);

     /* 
      * The datastore has our ring in it and we have a current ring
      * directory with details, whose currency is set to our row. 
      * Get various details store in the RS discriptor, clear up and return.
      * Note: the first and last sequences are fetched from the index
      * table is read [ll_read_index()]
      */
     ring->errnum     = NULL;
     ring->errstr     = "no error";
     ring->ringname   = xnstrdup(table_getcurrentcell(ringdir, "name"));
     ring->generation = super->generation;
     ring->ringid     = strtol(table_getcurrentcell(ringdir, "id"), 
			       (char**)NULL, 10);
     ring->nslots     = strtol(table_getcurrentcell(ringdir, "nslots"), 
			       (char**)NULL, 10);
     ring->youngest   = 0;	/* force update in rs_get() */
     ring->oldest     = 0;
     ring->current    = -1;
     ring->youngest_t    = ring->oldest_t    = 0;
     ring->youngest_hash = ring->oldest_hash = 0;
     ring->duration      = strtol(table_getcurrentcell(ringdir, "dur"), 
				  (char**)NULL, 10);
     ring->hdcache       = itree_create();
     table_destroy(ringdir);
     rs_free_superblock(super);

     return ring;
}


/*
 * Close an open ringstore descriptor
 */
void  rs_close(RS ring	/* ring descriptor */)
{
     if (!ring)
	  elog_die(FATAL, "NULL file handle");
     if (ring->ringid == -1)
	  elog_printf(ERROR, "using killed ring");

     ring->method->ll_close (ring->handle);
     xnfree(ring->ringname);
     itree_clearoutandfree(ring->hdcache);
     itree_destroy(ring->hdcache);
     xnfree(ring);
}


/*
 * Remove a ring from a ringstore file.
 * The ring should not be open, so in addition to (file,ring) we will 
 * need the ringstore method.
 * Returns 1 if successful or 0 for error.
 */
int   rs_destroy(RS_METHOD method	/* method vectors */,
		 char *filename		/* name of file */,
		 char *ringname		/* name of ring */)
{
     RS_LLD lld;
     TABLE ringdir, ringindex;
     int rowindex, ringid, from_seq, to_seq;
     RS_SUPER super;

     /* open (filename,ringname) conventionally if it exists 
      * and lock for writing */
     method->ll_init();
     lld = method->ll_open(filename, 0, 0);
     if ( ! lld )
	  return 0;
     if ( ! method->ll_lock(lld, RS_WRLOCK, "rs_destroy") ) {
	  method->ll_close(lld);
	  return 0;
     }

     /* read the ring directory, find the ring id, remove the ring's row 
      * from the table and finallt write it back */
     ringdir = method->ll_read_rings(lld);
     if ( ! ringdir ) {
	  method->ll_unlock(lld);
	  method->ll_close(lld);
	  return 0;
     }
     rowindex = table_search(ringdir, "name", ringname);
     if (rowindex == -1) {
	  method->ll_unlock(lld);
	  method->ll_close(lld);
	  table_destroy(ringdir);
	  return 0;
     }
     ringid = strtol(table_getcurrentcell(ringdir, "id"), (char**)NULL, 10);
     table_rmcurrentrow(ringdir);
     if ( ! method->ll_write_rings(lld, ringdir) ) {
	  method->ll_unlock(lld);
	  method->ll_close(lld);
	  table_destroy(ringdir);
	  return 0;
     }

     /* Read superblock and increment the generation count & write back.
      * As the ring directory is changed, we have to alter the generation
      * count, which tells others to reread the current view of the rings */
     super = method->ll_read_super(lld);
     if ( ! super ) {
	  /* Can't read superblock, but the ring has been removed from
	   * the dir. Best to flag the error, mark the superblock as
	   * damaged and continue to clean up the datastore. */
	  elog_printf(ERROR, "unable to read superblock but ring "
		      "removed from dir; datastore needs repair");
	  method->ll_write_value(lld, "DAMAGED", "superbock", 10);
     } else {
	  super->generation++;
	  if ( ! method->ll_write_super(lld, super) ) {
	  /* Can't write superblock, but the ring has been removed from
	   * the dir. Best to flag the error, mark the superblock as
	   * damaged and continue to clean up the datastore. */
	       elog_printf(ERROR, "unable to write superblock but ring "
			   "removed from dir; datastore needs repair");
	       method->ll_write_value(lld, "DAMAGED", "superbock", 10);
	  }
     }

     /* read the ring's index into memory but delete from disk */
     ringindex = method->ll_read_index(lld, ringid);
     if ( ! method->ll_rm_index(lld, ringid) )
	  /* Can't remove the index, flag as error and continue */
	  elog_printf(DEBUG, "remove index failed");

     /* expire all the data elements from the ring's index table */
     if ( ! ringindex ) {
	  /* Can't remove the index, flag as error and continue */
	  elog_printf(ERROR, "unable to remove ring elements as "
		      "there is no index; datastore needs cleaning");
     } else {
	  /* purge all the ring's elements */
	  table_first(ringindex);
	  from_seq = strtol(table_getcurrentcell(ringindex, "seq"),
			    (char**)NULL, 10);
	  table_last(ringindex);
	  to_seq = strtol(table_getcurrentcell(ringindex, "seq"),
			  (char**)NULL, 10);
	  method->ll_expire_dblock(lld, ringid, from_seq, to_seq);
	  table_destroy(ringindex);
     }

     /* unlock, free data structures and return */
     method->ll_unlock(lld);

     return 1;	/* success */
}


/* ---- stateful record oriented transfer ---- */
/* Data transfer & operations that move a cursor in the open ring.
 * The cursor is the notion of the current reading sample.
 * There is no cursor for writes, instead they are always appended
 * to the ring (with the exception of the rs_replace() function) */

/*
 * Append data to a ring, and remove the oldest if capacity is reached.
 * The current reading position (the cursor) is unaffected.
 * If there is no sequence column, each row will be a different sequence.
 * If the column _time does not exist, then a _time column IS CREATED
 * IN YOUR INPUT DATA TABLE as a side effect and the current time is 
 * used for each sample.
 * As many discrete samples are appended as there are sequences in the table.
 * The number of samples/sequences placed on the head of the ring will 
 * be removed from the end, oldest first.
 * Returns 1 for success or 0 for error (see rs_errno() and rs_errstr()).
 */
int   rs_put(RS    ring	/* ring descriptor */, 
	     TABLE data	/* data contained in a table */)
{
     char *headtxt, *infotxt, *head_and_info;
     unsigned long hash;
     ITREE *dblock;
     int r, seq, old_oldest;
     TABLE index;
     RS_DBLOCK d;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }
     if (table_nrows(data) == 0)
	  return 1;	/* success -- no work to do */

     /* get write lock & load ring's index */
     if ( ! ring->method->ll_lock(ring->handle, RS_WRLOCK, "rs_put") ) {
	  rs_free_dblock(dblock);
	  nfree(headtxt);
	  return 0;
     }
     if ( ! rs_priv_load_index(ring, &index) ) {
	  rs_free_dblock(dblock);
	  nfree(headtxt);
	  return 0;
     }

     /* hunt the correct hash and generate a datablock list from the table */
     headtxt = table_outheader(data);
     infotxt = table_outinfo(data);
     if (infotxt) {
          head_and_info = util_strjoin(headtxt, "\n", infotxt, NULL);
	  hash    = rs_priv_header_to_hash(ring, head_and_info);
     } else {
	  hash    = rs_priv_header_to_hash(ring, headtxt);
     }
     dblock  = rs_priv_table_to_dblock(data, hash);
     nfree(headtxt);
     if (infotxt) {
          nfree(infotxt);
	  nfree(head_and_info);
     }

     /* store dblocks in cache ? */

     /* append the dblocks to the sequence of them on disk */
     if (table_nrows(index)) {
	  table_last(index);
	  seq = strtol(table_getcurrentcell(index, "seq"),
		       (char**)NULL, 10) + 1;
     } else {
	  seq = 0;
     }
     r = ring->method->ll_append_dblock(ring->handle, ring->ringid, seq, 
					dblock);


     /* append the details of the new dblocks to the ring index */
     itree_traverse(dblock) {
	  d = itree_get(dblock);
	  table_addemptyrow(index);
	  table_replacecurrentcell_alloc(index, "seq", util_i32toa(seq++));
	  table_replacecurrentcell_alloc(index, "time",util_i32toa(d->time));
	  table_replacecurrentcell_alloc(index, "hd_hash", 
					 util_u32toa(d->hd_hashkey));
     }

     /* calculate the new ring endpoints and expire data and index 
      * entries if needed */
elog_printf(DEBUG, "put -- o %d y %d c %d ==> ", ring->oldest, ring->youngest, 
       ring->current);
     ring->youngest = seq - 1;
     if (ring->oldest < 0)
	  ring->oldest = 0;
     if (ring->oldest <= ring->youngest - ring->nslots) {
	  /* -- need to expire -- */
	  old_oldest = ring->oldest;
	  ring->oldest = ring->youngest - ring->nslots + 1;

	  /* purge the index of any expired dblocks */
	  table_first(index);
	  while ( ! table_isbeyondend(index) ) {
	       if (strtol(table_getcurrentcell(index, "seq"),
			  (char**)NULL, 10) < ring->oldest)
		    table_rmcurrentrow(index);
	       else
		    table_next(index);
	  }

	  /* purge the ring of any expired dblocks */
	  r = ring->method->ll_expire_dblock(ring->handle, ring->ringid, 
					     old_oldest, ring->oldest-1);
     }
elog_printf(DEBUG, "o %d y %d c %d", ring->oldest, ring->youngest, 
       ring->current);

     /* store the updated index and header (and cache?) */
     r = ring->method->ll_write_index(ring->handle, ring->ringid, index);

     /* unlock */
     ring->method->ll_unlock(ring->handle);

     /* free and return */
     table_destroy(index);
     rs_free_dblock(dblock);
     return 1;	/* success */
}


/*
 * Get the data sample at the current reading position of the ring and 
 * advance the position to the next data set. 
 * Returns a TABLE on success or NULL if at the end of the ring and there is
 * no data to read or another failure (see rs_errno() and rs_errstr()).
 * Table will contain the same headers as inserted, unless musthave_meta 
 * true, in which case, _seq, _time and _dur are created as columns, 
 * representing the sequence, the insertion time and the ring's duration.
 */
TABLE rs_get(RS ring,		/* ring descriptor */
	     int musthave_meta	/* include _seq, _time & _dur */)
{
     TABLE index, data;
     ITREE *dblist;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }

elog_printf(DEBUG, "get -- o %d y %d c %d ==> ", 
	    ring->oldest, ring->youngest, ring->current);

     /* reset current pointer if we already know it to be out of bounds */
     if (ring->current < ring->oldest)
	  ring->current = ring->oldest;
     if (ring->current > ring->youngest+1)
	  ring->current = ring->youngest+1;

     /* using the premise that there is a high probability that the data 
      * we want has NOT been removed, try to access it directly from storage 
      * without without reading index, ring directory or the superblock.
      * Its faster!!
      */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_get") )
	  return NULL;
     dblist = ring->method->ll_read_dblock(ring->handle, ring->ringid, 
					   ring->current, 1);
     if (!dblist) {
	  ring->method->ll_unlock(ring->handle);
	  return NULL;
     }

     if (itree_empty(dblist)) {
          itree_destroy(dblist);

	  /* waiting for next sequence, don't bother to get the index
	   * for speed. just return NULL */
	  if (ring->current == ring->youngest+1) {
	       ring->method->ll_unlock(ring->handle);
elog_printf(DEBUG, "o %d y %d c %d (NULL returned)", ring->oldest, 
	    ring->youngest, ring->current);
	       return NULL;
	  }

	  /* our speculation was not luckey! bummer!
	   * the block has either been expired, the ring has changed in 
	   * size or removed altogether. We now need to check on the 
	   * indexes...
	   */

	  if ( ! rs_priv_load_index(ring, &index) ) {
	       elog_printf(DIAG, "ring %s has been removed", ring);
	       ring->method->ll_unlock(ring->handle);
	       ring->ringid = -1;	/* invalidate ring */
	       return NULL;
	  }

	  /* if the block was not there, we always move to the
	   * oldest in the sequence and re-get the block */
	  ring->current = ring->oldest;
	  table_destroy(index);
	  dblist = ring->method->ll_read_dblock(ring->handle, ring->ringid, 
						ring->current, 1);
	  if (!dblist || itree_empty(dblist)) {
	       ring->method->ll_unlock(ring->handle);
	       if (dblist)
		    rs_free_dblock(dblist);
elog_printf(DEBUG, "o %d y %d c %d (NULL returned)", ring->oldest, 
	    ring->youngest, ring->current);
	       return NULL;
	  }
     }

     /* increment count */
     if (ring->current > ring->youngest) {
	  /* data has been written by someone else. 
	   * change our ring pointers */
	  ring->youngest++;
	  if ((ring->youngest - ring->nslots)+1 > ring->oldest)
	       ring->oldest = (ring->youngest - ring->nslots)+1;
     }
     ring->current++;

elog_printf(DEBUG, "after: o %d y %d c %d", ring->oldest, 
	    ring->youngest, ring->current);

     /* construct table using the header and the data */
     if (musthave_meta) 
          data = rs_priv_dblock_to_table(dblist, ring, rs_priv_hash_to_header,
					 NULL, 1, 1, 1);
     else
          data = rs_priv_dblock_to_table(dblist, ring, rs_priv_hash_to_header,
					 NULL, 0, 0, 0);
     if (!data)
	  elog_printf(ERROR, "unable to reconstruct data");
     rs_free_dblock(dblist);
     ring->method->ll_unlock(ring->handle);

     return data;
}


/*
 * Replace the data table at the current reading position.
 * The old data is lost and the current position is not advanced.
 * The new data keeps the existing sequence and time.
 * Returns 1 for successs or 0 for failure (see rs_errno() and rs_errstr()).
 */
int   rs_replace(RS ring	/* ring descriptor */, 
		 TABLE data	/* data */)
{
     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }
     /* if _time is not provided, as a column in the table, add it now */

     /* get string representation of header and hash it */

     /* traverse the input table to construct a list of dblocks suitable 
      * for the low level interface */

     /* get write lock */

     /* write the dblocks to disk, replacing the existing data */

     /* if not in cache, write header and hash index to the store,
      * finally loading into our cache */

     return 0;	/* failure - to be implemented */
}


/*
 * Get multiple sets of data starting from the current read point
 * and extending by a maximum of nsequencies. All data is returned 
 * in a single table but can be distinguished by using the '_seq' column.
 * '_time' is also provided for insertion time.
 * The current read point is positioned to the slot beyond the 
 * last included in the returned TABLE.
 * Returns TABLE if successful or NULL on error, generially that
 * there is no data to read (see rs_errno() and rs_errstr()).
 * Table will always include _seq, _time and _dur.
 */
TABLE rs_mget_nseq(RS ring		/* ring descriptor */, 
		   int nsequences	/* slots contained in result */)
{
     TABLE data;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }

elog_printf(DEBUG, "mget %d -- o %d y %d c %d ==> ", 
	    nsequences, ring->oldest, ring->youngest, ring->current);

     /* reset current pointer if we already know it to be out of bounds */
     if (ring->current < ring->oldest)
	  ring->current = ring->oldest;
     if (ring->current > ring->youngest+1)
	  ring->current = ring->youngest+1;

     /* get the data by calling the stateless routine */
     data = rs_mget_range(ring, 
			  ring->current, ring->current+nsequences-1, 
			  -1, -1);

     if (data) {
          /* success -- increment counters */
          ring->current += nsequences;
	  if (ring->current > ring->youngest)
	       ring->current = ring->youngest+1;
     }

     return data;
}


/*
 * Get multiple sets of data starting from the current read point
 * and covering all data older that `last_t'. All data is returned 
 * in a single table but can be distinguished by using the '_seq' column.
 * '_time' is also provided for insertion time.
 * The current read point is positioned to the slot beyond the 
 * last included in the returned TABLE.
 * Returns TABLE if successful or NULL on error, generially that
 * there is no data to read (see rs_errno() and rs_errstr()).
 */
TABLE rs_mget_to_time(RS ring		/* ring descriptor */, 
		      time_t last_t	/* youngest time to read */)
{
     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }
     /* get a read lock on the store */

     /* find the key of the next and last dblock using the current
      * position in the ring's index and the last time specified 
      * by the caller */

     /* fetch data from storage using the bounding keys */

     /* make a unique list of header requirements from the data
      * and fetch them if not already cached */

     /* unlock storage */

     /* construct table using the headers and the data */

     return NULL;	/* failure - to be implemented */
}


/* ---- stateful record oriented positioning ---- */

/*
 * Set the sequence and insertion time of the data at the 
 * current read position in two supplied variables. 
 * Sequences run from 0 and if -1 is returned, 
 * then the ring is empty and is not considered an error.
 * Returns the sequence number; sequence and time may be NULL.
 */
int   rs_current (RS ring	/* ring descriptor */, 
		  int *sequence	/* RETURN: sequence number */, 
		  time_t *time	/* RETURN: insertion time */)
{
     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return -1;
     }

     *sequence = ring->current;
     *time     = 0;			/* to be implemented */
     return ring->current;
}


/*
 * Set the sequence and insertion time of the youngest data in the ring
 * in two supplied variables. 
 * The disk is checked to find the ring sequence boundaries
 * Sequences run from 0 and if -1 is returned then the ring is empty 
 * and is not considered an error.
 * Returns the sequence number; sequence and time may be NULL.
 */
int   rs_youngest (RS ring		/* ring descriptor */, 
		   int *sequence	/* RETURN: sequence number */, 
		   time_t *time		/* RETURN: insertion time */)
{
     int r;
     TABLE index;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return -1;
     }

     /* force an index load to make sure the data is accurate */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_youngest") )
	  return -1;
     r = rs_priv_load_index(ring, &index);
     ring->method->ll_unlock(ring->handle);
     if ( ! r ) {
	  elog_printf(DIAG, "ring %s has been removed", ring);
	  ring->ringid = -1;	/* invalidate ring */
	  return -1;
     }
     table_destroy(index);

     *sequence = ring->youngest;
     *time     = ring->youngest_t;
     return ring->youngest;
}


/*
 * Set the sequence and insertion time of the oldest data in the ring
 * in two supplied variables. 
 * The disk is checked to find the ring sequence boundaries
 * Sequences run from 0 and if -1 is returned, 
 * then the ring is empty and is not considered an error.
 * Returns the sequence number; sequence and time may be NULL.
 */
int   rs_oldest   (RS ring		/* ring descriptor */, 
		   int *sequence	/* RETURN: sequence number */, 
		   time_t *time		/* RETURN: insertion time */)
{
     TABLE index;
     int r;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return -1;
     }

     /* force an index load to make sure the data is accurate */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_youngest") )
	  return -1;
     r = rs_priv_load_index(ring, &index);
     ring->method->ll_unlock(ring->handle);
     if ( ! r ) {
	  elog_printf(DIAG, "ring %s has been removed", ring);
	  ring->ringid = -1;	/* invalidate ring */
	  return -1;
     }
     table_destroy(index);

     *sequence = ring->oldest;
     *time     = ring->oldest_t;
     return ring->oldest;
}


/*
 * Move the current reading position back nsequencies
 * The disk is checked to find the ring sequence boundaries
 * Returns the actual number of slots moved
 */
int   rs_rewind   (RS ring		/* ring descriptor */, 
		   int nsequences	/* move back these slots */)
{
     TABLE index;
     int r, oldseq;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     if (nsequences < 0)
	  return 0;
     if (ring->current < 0)
	  return 0;

     /* force an index load to make sure the data is accurate */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_youngest") )
	  return -1;
     r = rs_priv_load_index(ring, &index);
     ring->method->ll_unlock(ring->handle);
     if ( ! r ) {
	  elog_printf(DIAG, "ring %s has been removed", ring);
	  ring->ringid = -1;	/* invalidate ring */
	  return -1;
     }
     table_destroy(index);

     oldseq = ring->current;
     ring->current -= nsequences;
     if (ring->current < ring->oldest)
	  ring->current = ring->oldest;

     return oldseq - ring->current;
}


/*
 * Move the current reading position forward nsequencies
 * It is alowed to move to one position beyond current data, ready for new
 * data to come in.
 * Returns the actual number of slots moved
 */
int   rs_forward  (RS ring		/* ring deswcriptor*/, 
		   int nsequences	/* move forward these slots */)
{
     TABLE index;
     int r, oldseq;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     if (nsequences < 0)
	  return 0;
     if (ring->current < 0)
	  return 0;

     /* force an index load to make sure the data is accurate */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_youngest") )
	  return -1;
     r = rs_priv_load_index(ring, &index);
     ring->method->ll_unlock(ring->handle);
     if ( ! r ) {
	  elog_printf(DIAG, "ring %s has been removed", ring);
	  ring->ringid = -1;	/* invalidate ring */
	  return -1;
     }
     table_destroy(index);

     oldseq = ring->current;
     ring->current += nsequences;
     if (ring->current > ring->youngest+1)
	  ring->current = ring->youngest+1;

     return ring->current - oldseq;
}


/*
 * Set the current reading position to sequence, which is not valid
 * will set the position to be the oldest or the youngest available 
 * sequence.
 * Returns the sequence number if successful or -1 otherwise
 */
int   rs_goto_seq (RS ring	/* ring descriptor */, 
		   int sequence	/* goto this sequence */)
{
     TABLE index;
     int r;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     /* force an index load to make sure the data is accurate */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_goto_seq") )
	  return -1;
     r = rs_priv_load_index(ring, &index);
     ring->method->ll_unlock(ring->handle);
     if ( ! r ) {
	  elog_printf(DIAG, "ring %s has been removed", ring);
	  ring->ringid = -1;	/* invalidate ring */
	  return -1;
     }
     table_destroy(index);

     if (sequence < ring->oldest)
	  ring->current = ring->oldest;
     else if (sequence > ring->youngest+1)
	  ring->current = ring->youngest;
     else
	  ring->current = sequence;

     return ring->current;
}


/*
 * Set the current reading position to the data whose insertion time 
 * is on or before time.
 * Returns the sequence number is successful or -1 otherwise
 */
int   rs_goto_time(RS ring	/* ring descriptor */, 
		   time_t time	/* youngest data time */)
{
     TABLE index;
     int r;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return -1;
     }

     /* force an index load to make sure the data is accurate */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_goto_time") )
	  return -1;
     r = rs_priv_load_index(ring, &index);
     ring->method->ll_unlock(ring->handle);
     if ( ! r ) {
	  elog_printf(DIAG, "ring %s has been removed", ring);
	  ring->ringid = -1;	/* invalidate ring */
	  return -1;
     }
     table_destroy(index);

     /* seach for the time in the index, either for an exact match or 
      * for the first time that is greater than asked for. */
     table_traverse(index) {
             if (strtol(table_getcurrentcell(index, "time"),
			(char**)NULL, 10) > time)
	       break; 
     }
     return table_getcurrentrowkey(index);
}


/* ---- stateless column oriented reading ---- */

/*
 * Return a TABLE containing a range of samples bound by sequence and 
 * time values. To be extracted, a sample must be have a sequence that is
 * within an inclusive range of sequences AND have a time that is within
 * an inclusive range of times.
 * -1 is used as a wild card for any of the values, opening up that part 
 * of the range. 
 * Using [-1,-1] as the sequence range will extract on only the time range.
 * Using [-1,-1,-1,-1] to wildcard all ranges will return the entire ring.
 * NULL is returned is there is no data, free TABLEs with table_destroy(). 
 * The current reading point is unaffected by this function.
 * Table will always include _seq, _time and _dur.
 */
TABLE rs_mget_range (RS  ring		/* ring descriptor */, 
		     int from_seq	/* oldest/starting sequence */, 
		     int to_seq		/* youngest/ending sequence */, 
		     time_t from_time	/* oldest/starting time */, 
		     time_t to_time	/* oldest/ending time */ )
{
     TABLE index, data, myindex;
     TABSET myset;
     ITREE *dblist;
     int first, last;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }

     /* lock ring and read the index */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_mget_range") )
	  return NULL;
     if ( ! rs_priv_load_index(ring, &index) ) {
          elog_printf(DIAG, "ring %s has been removed", ring);
	  ring->method->ll_unlock(ring->handle);
	  ring->ringid = -1;	/* invalidate ring */
	  return NULL;
     }

     /* process the index, finding matching sequences first, then matching 
	times */
     myset = tableset_create(index);
     if (from_seq != -1)
          tableset_where(myset, "seq",  ge, util_u32toa(from_seq));
     if (from_time != -1)
          tableset_where(myset, "time", ge, util_u32toa(from_time));
     if (to_seq != -1)
          tableset_where(myset, "seq",  le, util_u32toa(to_seq));
     if (to_time != -1)
          tableset_where(myset, "time", le, util_u32toa(to_time));
     myindex = tableset_into(myset);

     /* now find the sequences that remain */
     if (table_nrows(myindex) < 1) {
          ring->method->ll_unlock(ring->handle);
          table_destroy(index);
	  table_destroy(myindex);
	  tableset_destroy(myset);
          return NULL;
     }
     table_first(myindex);
     first = strtol(table_getcurrentcell(myindex, "seq"), (char**)NULL, 10);
     table_last(myindex);
     last = strtol(table_getcurrentcell(myindex, "seq"), (char**)NULL, 10);
     table_destroy(index);
     table_destroy(myindex);
     tableset_destroy(myset);

     /* load the data bound by the sequences */
     dblist = ring->method->ll_read_dblock(ring->handle, ring->ringid, 
					   first, last-first+1);
     if (!dblist || itree_empty(dblist)) {
          ring->method->ll_unlock(ring->handle);
	  if (dblist)
	       rs_free_dblock(dblist);
elog_printf(DEBUG, "NULL returned");
          return NULL;
     }

     /* construct table using the header and the data */
     data = rs_priv_dblock_to_table(dblist, ring, rs_priv_hash_to_header, 
				    NULL, 1, 1, 1);
     if (!data)
	  elog_printf(ERROR, "unable to reconstruct data");
     rs_free_dblock(dblist);
     ring->method->ll_unlock(ring->handle);

     return data;
}




/*
 * Get named columns of data between and including the sequencies
 * supplied.  Column names should be listed as the keys in the TREE list,
 * values are not used.
 * Returns a table including the column names asked for and two
 * meta columns: '_seq' for sequence number of row and '_time' for 
 * insertion time.
 * Free the table with table_destroy().
 * The current record position is not affected.
 * NULL is returned for error
 */
TABLE  rs_mget_byseq (RS ring		/* ring descriptor */, 
		      int from_seq	/* oldest/starting sequence */, 
		      int to_seq	/* youngest/ending sequence */)
{
     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }
     /* save current position, move to the from_seq */

     /* call the rs_mget_byseqs() */

     /* restore current position */

     /* store blocks in cache */

     /* convert blocks to table */

     /* drop unwanted columns from table */

     return NULL;	/* failure - to be implemented */
}


/*
 * Get named columns of data between and including the times
 * supplied.  Column names should be listed as the keys in the TREE list,
 * values are not used.
 * Returns a table including the column names asked for and two
 * meta columns: '_seq' for sequence number of row and '_time' for 
 * sample time.
 * Free the table with table_destroy().
 * The current record position is not affected.
 * NULL is returned for error
 */
TABLE  rs_mget_bytime(RS ring		/* ring descriptor */, 
		      time_t from_t	/* oldest/starting time */,   
		      time_t to_t	/* youngest/ending time */)
{
     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }
     /* save current position, move to the from_seq */

     /* call the rs_mget_bytime() */

     /* restore current position */

     /* store blocks in cache */

     /* convert blocks to table */

     /* drop unwanted columns from table */

     return NULL;	/* failure - to be implemented */
}



/*
 * Get all the entries between two times in a single table from the 
 * rings that share the same name, consolidated over all durations.
 * Where there are overlaps for a time, the lower duration data takes 
 * precidence and will obscure higher rate data.
 */
TABLE  rs_mget_cons  (RS_METHOD method, char *filename, char *ringname, 
		      time_t from_t, time_t to_t)
{
     RS_LLD lld;
     TABLE outtab, ringtab, myrings, index, myindex;
     TABSET ring_tset, index_tset;
     char *hunt_from, *hunt_to;
     int id, seq_from, seq_to;
     ITREE *tablist, *dblocks;
     struct rs_session psuedo_ring;

     /* open file read only and grab a read lock */
     method->ll_init();
     lld = method->ll_open(filename, 0, 0);
     if ( ! lld )
	  return NULL;
     if ( ! method->ll_lock(lld, RS_RDLOCK, "rs_mget_cons") ) {
	  method->ll_close(lld);
	  return NULL;
     }

     /* create a partially completed rs_session structure that can be
      * used over the rings of all durations */
     psuedo_ring.handle   = lld;
     psuedo_ring.method   = method;
     psuedo_ring.ringname = ringname;
     psuedo_ring.duration = 0;
     psuedo_ring.hdcache  = itree_create();

     /* read ring directory */
     ringtab = method->ll_read_rings(lld);
     if (!ringtab) {
          method->ll_unlock(lld);
	  method->ll_close(lld);
	  return NULL;
     }

     /* make an ordered ascending list of (ring,dur) */
     ring_tset = tableset_create(ringtab);
     tableset_where(ring_tset, "name", eq, ringname);
     tableset_sortby(ring_tset, "dur", TABSET_SORT_NUM_ASC);
     myrings = tableset_into(ring_tset);

     /* start hunting for matching blocks */
     if (from_t == -1)
          hunt_from = xnstrdup("0");
     else
          hunt_from = xnstrdup(util_i32toa(from_t));
     if (to_t == -1)
          hunt_to   = xnstrdup(util_i32toa(INT_MAX));
     else
          hunt_to   = xnstrdup(util_i32toa(to_t));
     tablist   = itree_create();
     outtab    = table_create();
     table_traverse(myrings) {
          id = strtol(table_getcurrentcell(myrings, "id"), (char**)NULL, 10);
	  elog_printf(DEBUG, "hunting ring %s id %d dur %s from %s to %s", 
		      ringname, id, table_getcurrentcell(myrings, "dur"), 
		      hunt_from, hunt_to);
	  index = method->ll_read_index(lld, id);
	  if (!index)
	       continue;

	  /* select out the matching samples */
	  index_tset = tableset_create(index);
	  tableset_where(index_tset, "time", ge, hunt_from);
	  tableset_where(index_tset, "time", le, hunt_to);
	  myindex = tableset_into(index_tset);
	  if (table_nrows(myindex) == 0) {
	       tableset_destroy(index_tset);
	       table_destroy(index);
	       table_destroy(myindex);
	       continue;
	  }

	  /* reset hunt targets and fetch the actual samples */
	  table_first(myindex);
	  nfree(hunt_to);
	  hunt_to  = xnstrdup( util_i32toa( 
			strtol( table_getcurrentcell(myindex, "time"), 
				(char**)NULL, 10) - 1 ));
	  seq_from = strtol(table_getcurrentcell(myindex, "seq"),
			    (char**)NULL, 10);
	  /*elog_printf(DEBUG, "from %s  ", hunt_to);*/
	  table_last(myindex);
	  /*elog_printf(DEBUG, "to %s ", 
	    table_getcurrentcell(myindex, "time"));*/
	  seq_to   = strtol(table_getcurrentcell(myindex, "seq"),
			    (char**)NULL, 10);
	  dblocks = method->ll_read_dblock(lld,id,seq_from,seq_to-seq_from+1);

	  /* turn blocks into tables */
	  rs_priv_dblock_to_table(dblocks, &psuedo_ring, 
				  rs_priv_hash_to_header, outtab, 0, 1, 0);

	  elog_printf(DEBUG, "  --found seq %d-%d (%d), read %d blocks, "
		      "outtab nrows %d", seq_from, seq_to, seq_to-seq_from, 
		      itree_n(dblocks), table_nrows(outtab));

	  rs_free_dblock(dblocks);
	  tableset_destroy(index_tset);
	  table_destroy(index);
	  table_destroy(myindex);
     }

     /* get out the the file */
     method->ll_unlock(lld);
     method->ll_close(lld);

     /* clear up */
     tableset_destroy(ring_tset);
     table_destroy(ringtab);
     itree_destroy(psuedo_ring.hdcache);
     table_destroy(myrings);
     nfree(hunt_from);
     nfree(hunt_to);

     /* no data */
     if (table_nrows(outtab) == 0) {
	  table_destroy(outtab);
	  return NULL;
     }
	  
     /* create a sorted version */
     if ( ! table_sortnumeric(outtab, "_time", NULL))
	  elog_printf(ERROR, "unable to sort");

     return outtab;
}


/* ---- file and ring modification & information ---- */

/*
 * Changed the number of slots use in the ring.
 * newslots=0 will unlimit the ring to turn it into a queue; rs_purge()
 * should then be used to remove data from the tail of the queue.
 * Otherwise, ring will behave like a ring buffer and destroy data from
 * the tail (the oldest) when the number of slots is used up and new data
 * is inserted in the head.
 * If newslots is a reduction in slots, the data lost is minimised 
 * and prioritised, by removing unused data slots first then removing 
 * from the tail after that.
 * If newslots is an increase, then the slots will be used for new data.
 * Returns 1 for success or 0 for failure.
 */
int   rs_resize(RS ring		/* ring descriptor */, 
		int newslots	/* change the slots to this */)
{
     TABLE ringdir;
     int rowindex, r, newoldest;
     char *duration, *newslots_str;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     if (newslots < 0) {
	  elog_printf(ERROR, "number of new slots for ring %d,%d must be "
		      "positive", ring->ringname, duration);
	  return 0;
     }

     /* get write lock */
     if ( ! ring->method->ll_lock(ring->handle, RS_WRLOCK, "rs_resize") )
	  return 0;

     /* Read the ring directory, amend the ring and write out */
     duration = xnstrdup(util_i32toa(ring->duration));
     ringdir = ring->method->ll_read_rings(ring->handle);
     rowindex = table_search2(ringdir, 
			      "name", ring->ringname, 
			      "dur",  duration);
     if (rowindex == -1) {
	  elog_printf(ERROR, "ring %s,%s does not exist", 
		      ring->ringname, duration);
	  ring->method->ll_unlock(ring->handle);
	  table_destroy(ringdir);
	  nfree(duration);
	  return 0;
     }

     newslots_str = xnstrdup(util_i32toa(newslots));
     table_replacecurrentcell_alloc(ringdir,"nslots", newslots_str);
     r = ring->method->ll_write_rings(ring->handle, ringdir);

     /* unlock file and free ring table */
     ring->method->ll_unlock(ring->handle);
     table_destroy(ringdir);
     nfree(duration);
     nfree(newslots_str);

     if ( ! r ) {
	  elog_printf(ERROR, "unable to write ringdir, number of slots "
		      "will not be changed");
	  return 0;
     }

     newoldest = ring->youngest - newslots;
     if (newoldest > ring->oldest) {
	  /* down size ring distructively */
	  r = rs_purge(ring, newoldest - ring->oldest);
	  if ( ! r ) {
	       elog_printf(WARNING, "purging data from oldest %d data "
			   "failed but ring size shortened", 
			   newoldest - ring->oldest);
	  }
     }

     /* Change the in-memory details */
     ring->nslots = newslots;

     return 1;
}


/*
 * Remove nkill slots of the oldest data from the ring, leaving the
 * number of slots unchanged. The newly freed slots are added to the
 * already free and that total number is available for use without
 * losing further data from the ring buffer.
 * Returns the number of slots of data actually purged if successful or 
 * 0 for failure.
 */
int   rs_purge(RS ring		/* ring descriptor */, 
	       int nkill	/* number of slots to remove data */)
{
     int actual_data, actual_kill, purge_from, purge_to, removed, r;
     TABLE index, newindex;
     TABSET newset;

     if (nkill <= 0)
          return 0;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     /* lock ring and read the index for this ring */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_purge") )
	  return 0;
     if ( ! rs_priv_load_index(ring, &index) ) {
          elog_printf(DIAG, "ring %s has been removed", ring);
	  ring->method->ll_unlock(ring->handle);
	  ring->ringid = -1;	/* invalidate ring */
	  return 0;
     }

     /* the ring structure is uptodate, now carry out the calculations */
     actual_data = ring->youngest - ring->oldest + 1;
     actual_kill = (nkill > actual_data) ? actual_data : nkill;
     purge_from = ring->oldest;
     purge_to   = ring->oldest + actual_kill - 1;

     /* carry out the removal */
     removed = ring->method->ll_expire_dblock(ring->handle, ring->ringid, 
					      purge_from, purge_to);
     if (removed != actual_kill)
          elog_printf(ERROR, "discrepancy between removal quantities %d vs %d",
		      actual_kill, removed);

     /* removed purged rows from the index & save */
     newset = tableset_create(index);
     tableset_where(newset, "seq",  gt, util_u32toa(purge_to));
     newindex = tableset_into(newset);
     r = ring->method->ll_write_index(ring->handle, ring->ringid, newindex);

     /* unlock */
     ring->method->ll_unlock(ring->handle);

     /* change pointers and potentially reset sequences */
     ring->oldest = purge_to + 1;
     if (ring->oldest > ring->youngest)
          /* ring->oldest = ring->youngest = */ ring->current = -1;
     else if (ring->current < ring->oldest)
          ring->current = ring->oldest;
     if (table_nrows(newindex) > 0) {
          table_first(newindex);
	  ring->oldest_t    = strtol(table_getcurrentcell(newindex, "time"),
				     (char**)NULL, 10);
	  ring->oldest_hash = strtol(table_getcurrentcell(newindex, "hd_hash"),
				     (char**)NULL, 10);
	  table_last(newindex);
	  ring->youngest_t   = strtol(table_getcurrentcell(newindex, "time"),
				      (char**)NULL, 10);
	  ring->youngest_hash= strtol(table_getcurrentcell(newindex,"hd_hash"),
				      (char**)NULL, 10);
     } else {  
          ring->youngest_t    = ring->oldest_t    = 0;
	  ring->youngest_hash = ring->oldest_hash = 0;
     }

     table_destroy(index);
     table_destroy(newindex);
     tableset_destroy(newset);

     return actual_kill;
}


/*
 * Return status information about the open ring, passing data back 
 * by reference (so don't go freeing anything)!
 * The nread and nunread values relate to the current slot position
 * in the context of the entire ring; if you jumpped backwards, nunread
 * will increase and nread will decrease.
 * Returns 1 for success or 0 for failure.
 */
int   rs_stat(RS ring			/* ring descriptor */, 
	      int *ret_duration		/* RET: duration */,
	      int *ret_nslots		/* RET: number or slots in ring */, 
	      int *ret_oldest		/* RET: oldest sequence number */,
	      int *ret_oldest_t		/* RET: oldest time */,
	      unsigned long *ret_oldest_hash	/* RET: oldest header hash */,
	      int *ret_youngest		/* RET: youngest sequence number */,
	      int *ret_youngest_t	/* RET: youngest time */,
	      unsigned long *ret_youngest_hash	/* RET: youngest hd hash */,
	      int *ret_current		/* RET: current sequence number */
	      )

{
     TABLE index;
     int r;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     /* fetch up-to-date ring and ring directory structure */
     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_stat") )
	  return 0;
     r = rs_priv_load_index(ring, &index);
     ring->method->ll_unlock(ring->handle);
     if ( ! r ) {
	  elog_printf(DIAG, "ring %s has been removed", ring);
	  ring->ringid = -1;	/* invalidate ring */
	  return 0;
     }

     /* carry out assignments */
     *ret_duration      = ring->duration;
     *ret_nslots        = ring->nslots;
     *ret_oldest        = ring->oldest;
     *ret_oldest_t      = ring->oldest_t;
     *ret_oldest_hash   = ring->oldest_hash;
     *ret_youngest      = ring->youngest;
     *ret_youngest_t    = ring->youngest_t;
     *ret_youngest_hash = ring->youngest_hash;
     *ret_current       = ring->current;

     table_destroy(index);

     return (1);
}


/*
 * Return details of the rings in the file.
 * A TABLE is returned with the following columns
 *    name    name of the ring
 *    dur     duration of the ring
 *    nslots  number of slots in ring
 *    id      id of ring
 *    long    long name of ring
 *    about   description of ring
 * Returns NULL for an error.
 */
TABLE rs_lsrings(RS_METHOD method	/* method vectors */,
		 char *filename		/* file name of ringstore */)
{
     RS_LLD lld;
     TABLE tab;

     /* open file read only and grab a read lock */
     method->ll_init();
     lld = method->ll_open(filename, 0, 0);
     if ( ! lld )
	  return NULL;
     if ( ! method->ll_lock(lld, RS_RDLOCK, "rs_lsrings") ) {
	  method->ll_close(lld);
	  return NULL;
     }

     /* read ring directory */
     tab = method->ll_read_rings(lld);
     method->ll_unlock(lld);
     method->ll_close(lld);

     return tab;
}


/*
 * Return details of consolidated rings in the ringstore file. 
 * A TABLE is returned with the following columns
 *    name    name of the ring
 *    long    long name of ring
 *    about   description of ring
 * Returns NULL for an error.
 */
TABLE rs_lsconsrings(RS_METHOD method	/* method vectors */,
		     char *filename	/* file name of ringstore */)
{
     RS_LLD lld;
     TABLE tab;
     TREE *uniq;

     /* open file read only and grab a read lock */
     method->ll_init();
     lld = method->ll_open(filename, 0, 0);
     if ( ! lld )
	  return NULL;
     if ( ! method->ll_lock(lld, RS_RDLOCK, "rs_lsconsrings") ) {
	  method->ll_close(lld);
	  return NULL;
     }

     /* read ring directory */
     tab = method->ll_read_rings(lld);
     method->ll_unlock(lld);
     method->ll_close(lld);

     /* remove duplicate rows leaving just the first of each row name */
     uniq = tree_create();
     table_first(tab);
     while (!table_isbeyondend(tab)) {
          if (tree_find(uniq, table_getcurrentcell(tab, "name"))==TREE_NOVAL){
	       tree_add(uniq, table_getcurrentcell(tab, "name"), NULL);
	       table_next(tab);
	  } else {
	       table_rmcurrentrow(tab);
	  }
     }
     table_rmcol(tab, "dur");
     table_rmcol(tab, "id");
     table_rmcol(tab, "nslots");
     tree_destroy(uniq);

     return tab;
}



/*
 * Return long details of the rings in the file.
 * A TABLE is returned with the following columns
 *    name    name of the ring
 *    dur     duration of the ring
 *    id      id of ring
 *    long    long name of ring
 *    about   description of ring
 *    nslots  number of slots in ring
 *    oseq    oldest sequence
 *    otime   oldest time
 *    yseq    youngest sequence
 *    ytime   youngest time
 * Details are held in a table with columns including name, nslots, 
 * description.
 * Takes longer than lsrings, so consider using that instead.
 * Returns NULL for an error.
 */
TABLE rs_inforings(RS_METHOD method	/* method vectors */,
		   char *filename	/* file name of ringstore */)
{
     RS_LLD lld;
     TABLE rings, index;
     int ringid;

     /* open file read only and grab a read lock */
     method->ll_init();
     lld = method->ll_open(filename, 0, 0);
     if ( ! lld )
	  return NULL;
     if ( ! method->ll_lock(lld, RS_RDLOCK, "rs_inforings") ) {
	  method->ll_close(lld);
	  return NULL;
     }

     /* read ring directory and traverse each to get their index */
     rings = method->ll_read_rings(lld);
     if (rings) {
          table_addcol(rings, "oseq",  NULL);
          table_addcol(rings, "otime", NULL);
          table_addcol(rings, "yseq",  NULL);
          table_addcol(rings, "ytime", NULL);
          table_traverse(rings) {
	       ringid = strtol(table_getcurrentcell(rings, "id"),
			       (char**)NULL, 10);
	       index    = method->ll_read_index(lld, ringid);
	       if (index && table_nrows(index) > 0) {
		    table_first(index);
		    table_replacecurrentcell_alloc(rings, "oseq", 
					table_getcurrentcell(index, "seq"));
		    table_replacecurrentcell_alloc(rings, "otime", 
					table_getcurrentcell(index, "time"));
		    table_last(index);
		    table_replacecurrentcell_alloc(rings, "yseq", 
					table_getcurrentcell(index, "seq"));
		    table_replacecurrentcell_alloc(rings, "ytime", 
					table_getcurrentcell(index, "time"));
	       } else {
		    table_replacecurrentcell_alloc(rings, "oseq",  "-1");
		    table_replacecurrentcell_alloc(rings, "otime", "0");
		    table_replacecurrentcell_alloc(rings, "yseq",  "-1");
		    table_replacecurrentcell_alloc(rings, "ytime", "0");
	       }
	       if (index)
		    table_destroy(index);
	  }
     }

     method->ll_unlock(lld);
     method->ll_close(lld);

     return rings;
}



/*
 * Return details of consolidated rings in the ringstore file. 
 * A TABLE is returned with the following columns
 *    name    name of the ring
 *    long    long name of ring
 *    about   description of ring
 * Returns NULL for an error.
 */
TABLE rs_infoconsrings(RS_METHOD method	/* method vectors */,
		       char *filename	/* file name of ringstore */)
{
     RS_LLD lld;
     TABLE rings, index;
     TREE *oldest, *youngest;
     char *name;
     time_t otime, ytime;
     int ringid;

     /* open file read only and grab a read lock */
     method->ll_init();
     lld = method->ll_open(filename, 0, 0);
     if ( ! lld )
	  return NULL;
     if ( ! method->ll_lock(lld, RS_RDLOCK, "rs_lsconsrings") ) {
	  method->ll_close(lld);
	  return NULL;
     }

     /* read ring directory */
     rings = method->ll_read_rings(lld);
     if (!rings) {
	  method->ll_close(lld);
	  return NULL;
     }

     /* remove duplicate rows but collect the oldest and youngest times
      * of each, summerising ring data into a single entry per ring 
      * with its time boundaries */
     oldest   = tree_create();
     youngest = tree_create();
     table_first(rings);
     while (!table_isbeyondend(rings)) {
          /* fetch the index for the current ring and find the times of
	   * the oldest and youngest data */
          name   = table_getcurrentcell(rings, "name");
          ringid = strtol(table_getcurrentcell(rings, "id"),
			  (char**)NULL, 10);
	  index  = method->ll_read_index(lld, ringid);
	  if (index) {
	       if (table_nrows(index)) {
		    table_first(index);
		    otime = strtol(table_getcurrentcell(index, "time"),
				   (char**)NULL, 10);
		    table_last(index);
		    ytime = strtol(table_getcurrentcell(index, "time"),
				   (char**)NULL, 10);
	       } else {
		    otime = ytime = 0;
	       }
	       table_destroy(index);
	  } else {
	       otime = ytime = 0;
	  }

	  /* check against the ring list and save inside it */
	  if (tree_find(oldest, name) == TREE_NOVAL) {
	       tree_add(oldest,   name, (void *) otime);
	       tree_add(youngest, name, (void *) ytime);
	       table_next(rings);
          } else {
	       if (otime < (time_t) tree_get(oldest))
	            tree_put(oldest,   (void *) otime);
	       tree_find(youngest, name);
	       if (ytime > (time_t) tree_get(youngest))
	            tree_put(youngest, (void *) ytime);
	       table_rmcurrentrow(rings);
	  }
     }

     /* release file */
     method->ll_unlock(lld);
     method->ll_close(lld);

     /* remove unneeded columns, add new ones */
     table_rmcol(rings, "dur");
     table_rmcol(rings, "id");
     table_rmcol(rings, "nslots");
     table_addcol(rings, "otime", NULL);
     table_addcol(rings, "ytime", NULL);

     /* add in the summary of youngest and oldest times */
     table_traverse(rings) {
          name = table_getcurrentcell(rings, "name");
          tree_find(oldest,   name);
          tree_find(youngest, name);
	  table_replacecurrentcell_alloc(rings, "otime", 
				   util_i32toa((time_t) tree_get(oldest)));
	  table_replacecurrentcell_alloc(rings, "ytime", 
				   util_i32toa((time_t) tree_get(youngest)));
     }

     /* clean up and return */
     tree_destroy(oldest);
     tree_destroy(youngest);
     return rings;
}


/*
 * Return the file name of the currently opened ringstore.
 * Do not free or damage the string.
 * Returns NULL for error.
 */
char *rs_filename(RS ring	/* ring descriptor */)
{
     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }
     return NULL /*ring->handle->name*/;	/* failure - to be implemented */
}


/*
 * Return the ring name of the currently opened ringstore.
 * Do not free or damage the string.
 * Returns NULL for error.
 */
char *rs_ringname(RS ring	/* ring descriptor */)
{
     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }
     return ring->ringname;
}


/*
 * Return the amount of space taken up in storing this ringstore file
 * in bytes. Returns 0 for error.
 */
int   rs_footprint(RS ring	/* ring descriptor */)
{
     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }
     return 0;	/* failure - to be implemented */
}


/*
 * Returns the number of bytes left for the ring store to grow, inside 
 * its filesystem. Returns -1 for error.
 */
int   rs_remain(RS ring		/* ring descriptor */)
{
     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return -1;
     }
     return -1;	/* failure - to be implemented */
}


/*
 * Change the name of the current ring. 
 * Returns 1 for success or 0 for error.
 */
int   rs_change_ringname(RS ring, char *newname)
{
     TABLE ringdir;
     int rowindex, r;
     char *duration;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     /* get write lock */
     if ( ! ring->method->ll_lock(ring->handle, RS_WRLOCK, 
				  "rs_change_ringname") )
	  return 0;

     /* Read the ring directory, amend the ring and write out */
     duration = xnstrdup(util_i32toa(ring->duration));
     ringdir = ring->method->ll_read_rings(ring->handle);
     rowindex = table_search2(ringdir, 
			      "name", newname,
			      "dur",  duration);
     if (rowindex != -1) {
	  /* there is already a ring with the new name and duration. 
	   * refuse to rename */
	  elog_printf(ERROR, "can't over write an existing ring %s,%s", 
		      newname, duration);
	  nfree(duration);
	  ring->method->ll_unlock(ring->handle);
	  table_destroy(ringdir);
	  return 0;
     }
     rowindex = table_search2(ringdir, 
			      "name", ring->ringname, 
			      "dur",  duration);
     if (rowindex == -1) {
	  elog_printf(ERROR, "ring %s,%s does not exist", 
		      ring->ringname, duration);
	  ring->method->ll_unlock(ring->handle);
	  table_destroy(ringdir);
	  nfree(duration);
	  return 0;
     }
     table_replacecurrentcell_alloc(ringdir,"name", newname);
     r = ring->method->ll_write_rings(ring->handle, ringdir);

     /* unlock file and free ring table */
     ring->method->ll_unlock(ring->handle);
     table_destroy(ringdir);
     nfree(duration);

     if ( ! r ) {
	  elog_printf(ERROR, "unable to write ringdir, ring name will not "
		      "be changed");
	  return 0;
     }

     /* Change the in-memory details */
     nfree(ring->ringname);
     ring->ringname = xnstrdup(newname);

     return 1;
}


int   rs_change_duration(RS ring, int newdur)
{
     TABLE ringdir;
     int rowindex, r;
     char *duration, *newduration;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     /* get write lock */
     if ( ! ring->method->ll_lock(ring->handle, RS_WRLOCK, 
				  "rs_change_duration") )
	  return 0;

     /* Read the ring directory, amend the ring and write out */
     duration = xnstrdup(util_i32toa(ring->duration));
     newduration = xnstrdup(util_i32toa(newdur));
     ringdir = ring->method->ll_read_rings(ring->handle);
     rowindex = table_search2(ringdir, 
			      "name", ring->ringname,
			      "dur",  newduration);
     if (rowindex != -1) {
	  /* there is already a ring with the name and new duration. 
	   * refuse to change duration */
	  elog_printf(ERROR, "can't over write an existing ring %s,%s", 
		      ring->ringname, newduration);
	  nfree(duration);
	  nfree(newduration);
	  ring->method->ll_unlock(ring->handle);
	  table_destroy(ringdir);
	  return 0;
     }
     rowindex = table_search2(ringdir, 
			      "name", ring->ringname, 
			      "dur",  duration);
     if (rowindex == -1) {
	  elog_printf(ERROR, "ring %s,%s does not exist", 
		      ring->ringname, duration);
	  ring->method->ll_unlock(ring->handle);
	  table_destroy(ringdir);
	  nfree(duration);
	  nfree(newduration);
	  return 0;
     }
     table_replacecurrentcell_alloc(ringdir,"dur", newduration);
     r = ring->method->ll_write_rings(ring->handle, ringdir);

     /* unlock file and free ring table */
     ring->method->ll_unlock(ring->handle);
     table_destroy(ringdir);
     nfree(duration);
     nfree(newduration);

     if ( ! r ) {
	  elog_printf(ERROR, "unable to write ringdir, duration will not "
		      "be changed");
	  return 0;
     }

     /* Change the in-memory details */
     ring->duration = newdur;

     return 1;
}


int   rs_change_longname(RS ring, char *newlong)
{
     TABLE ringdir;
     int rowindex, r;
     char *duration;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     /* get write lock */
     if ( ! ring->method->ll_lock(ring->handle, RS_WRLOCK, 
				  "rs_change_longname") )
	  return 0;

     /* Read the ring directory, amend the ring and write out */
     duration = xnstrdup(util_i32toa(ring->duration));
     ringdir = ring->method->ll_read_rings(ring->handle);
     rowindex = table_search2(ringdir, 
			      "name", ring->ringname, 
			      "dur",  duration);
     if (rowindex == -1) {
	  elog_printf(ERROR, "ring %s,%s does not exist", 
		      ring->ringname, duration);
	  ring->method->ll_unlock(ring->handle);
	  table_destroy(ringdir);
	  nfree(duration);
	  return 0;
     }
     table_replacecurrentcell_alloc(ringdir, "long", newlong);
     r = ring->method->ll_write_rings(ring->handle, ringdir);

     /* unlock file and free ring table */
     ring->method->ll_unlock(ring->handle);
     table_destroy(ringdir);
     nfree(duration);

     if ( ! r ) {
	  elog_printf(ERROR, "unable to write ringdir, long name will not "
		      "be changed");
	  return 0;
     }

     return 1;
}


int   rs_change_comment (RS ring, char *newcomment)
{
     TABLE ringdir;
     int rowindex, r;
     char *duration;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return 0;
     }

     /* get write lock */
     if ( ! ring->method->ll_lock(ring->handle, RS_WRLOCK, 
				  "rs_change_comment") )
	  return 0;

     /* Read the ring directory, amend the ring and write out */
     duration = xnstrdup(util_i32toa(ring->duration));
     ringdir = ring->method->ll_read_rings(ring->handle);
     rowindex = table_search2(ringdir, 
			      "name", ring->ringname, 
			      "dur",  duration);
     if (rowindex == -1) {
	  elog_printf(ERROR, "ring %s,%s does not exist", 
		      ring->ringname, duration);
	  ring->method->ll_unlock(ring->handle);
	  table_destroy(ringdir);
	  nfree(duration);
	  return 0;
     }
     table_replacecurrentcell_alloc(ringdir, "about", newcomment);
     r = ring->method->ll_write_rings(ring->handle, ringdir);

     /* unlock file and free ring table */
     ring->method->ll_unlock(ring->handle);
     table_destroy(ringdir);
     nfree(duration);

     if ( ! r ) {
	  elog_printf(ERROR, "unable to write ringdir, comment will not "
		      "be changed");
	  return 0;
     }

     return 1;
}




/*
 * Returns the most recent error code if there was a problem with
 * any of the ringstore calls
 */
int   rs_errno(RS ring		/* ring descriptor */)
{
     return *(ring->errnum);
}


/*
 * Returns a string associated with the most recent error, which 
 * corresponds to the error number returned by rs_errno()
 */
char *rs_errstr(RS ring	/* ring descriptor */)
{
     return ring->errstr;
}



/*
 * Return a superblock structure from the local machine & os settings or
 * send back NULL if not successful.
 */
RS_SUPER rs_create_superblock()
{
     struct utsname uts;
     RS_SUPER super;
     time_t created;
     int r;
     char *domainname = "";

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
     localtime(&created);

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
     super->ringcounter = 0;

     return super;
}


/*
 * Free an existing superblock memory structure
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
 * duplicate a superblock to nmalloc()ed structure
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



/* low level diagnostic information */

/* return a copy of the superblock structure, free with rs_free_superblock() */
RS_SUPER rs_info_super(RS_METHOD method	/* method vectors */,
		       char *filename	/* file name of ringstore */)
{
     RS_SUPER super;
     RS_LLD lld;

     /* open file read only and grab a read lock */
     method->ll_init();
     lld = method->ll_open(filename, 0, 0);
     if ( ! lld )
	  return NULL;
     if ( ! method->ll_lock(lld, RS_RDLOCK, "rs_info_super") ) {
          method->ll_close(lld);
	  return NULL;
     }
     super = method->ll_read_super(lld);
     method->ll_unlock(lld);
     method->ll_close(lld);

     return super;
}

TABLE rs_info_ring(RS ring)
{
     TABLE tab;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }

     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_info_ring") ) {
	  return NULL;
     }
     tab = ring->method->ll_read_rings(ring->handle);
     ring->method->ll_unlock(ring->handle);

     return tab;
}

TABLE rs_info_header(RS ring)
{
     TABLE tab;
     ITREE *newheaders;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }

     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_info_header")) {
	  return NULL;
     }

     /* update the header cache */
     newheaders = ring->method->ll_read_headers(ring->handle);
     ring->method->ll_unlock(ring->handle);
     if (newheaders) {
	  if (ring->hdcache) {
	       itree_clearoutandfree(ring->hdcache);
	       itree_destroy(ring->hdcache);
	  }
	  ring->hdcache = newheaders;
     }

     /* make a table from the list */
     tab = table_create_a(rs_info_header_hds);
     itree_traverse(ring->hdcache) {
	  table_addemptyrow(tab);
	  table_replacecurrentcell_alloc(tab, "key", 
				   util_u32toa(itree_getkey(ring->hdcache)));
	  table_replacecurrentcell_alloc(tab, "header", 
				   itree_get(ring->hdcache));
     }

     return tab;
}

TABLE rs_info_index(RS ring)
{
     TABLE tab=NULL;

     if (ring->ringid == -1) {
	  elog_printf(ERROR, "using killed ring");
	  return NULL;
     }

     if ( ! ring->method->ll_lock(ring->handle, RS_RDLOCK, "rs_info_index") ) {
	  return NULL;
     }
     rs_priv_load_index(ring, &tab);
     ring->method->ll_unlock(ring->handle);

     return tab;
}





/*
 * Take a set of data in TABLE form and produce a list of DBLOCKs.
 * The table must have an attribute named _seq or _time to define the
 * different sequences in a table, otherwise it is all treated as a 
 * single sample. Each block represents one sequence.
 * Hash should be the unique header key to apply for the whole of this table.
 * Returns an ITREE list of RS_DBLOCK if successful (including empty data).
 * Free the returned data with rs_free_dblock().
 */
ITREE *rs_priv_table_to_dblock(TABLE tab,	  /* table containing data */
			       unsigned long hash /* unique heaader key */)
{
     int ikey, i, hastime=0;
     RS_DBLOCK d;
     TABLE itab;
     TABSET tset;
     ITREE *dblocks;
     TREE *seqs, *times;
     char *str;

     /* initialise and find the hash value of the header */
     dblocks = itree_create();
     if (table_hascol(tab, "_time"))
	  hastime++;

     tset = tableset_create(tab);
     if (table_hascol(tab, "_seq")) {
          /* traverse the table by sequence (_seq) */
          seqs = table_uniqcolvals(tab, "_seq", NULL);
	  itree_traverse(seqs) {
	       /* select out the data */
	       tableset_reset(tset);
	       tableset_where(tset, "_seq", eq, tree_getkey(seqs));
	       ikey = strtol(tree_getkey(seqs), (char**)NULL, 10);
	       itab = tableset_into(tset);
	       table_first(itab);
	       d = xnmalloc(sizeof(struct rs_data_block));
	       if (hastime)
		    d->time = strtol(table_getcurrentcell(itab, "_time"),
				     (char**)NULL, 10);
	       else
		    d->time = time(NULL);	/* if no _time: use now */
	       table_rmcol(itab, "_seq");
	       table_rmcol(itab, "_time");
	       table_rmcol(itab, "_dur");
	       d->hd_hashkey = hash;
	       d->data = table_outbody(itab);
	       d->__priv_alloc_mem = d->data;
	       itree_add(dblocks, ikey, d);
	       table_destroy(itab);
	  }
	  tree_destroy(seqs);
     } else if (hastime) {
          /* traverse the table by time (_time) making each a 
	   * successive sequence */
          times = table_uniqcolvals(tab, "_time", NULL);
	  itree_traverse(times) {
	       /* select out the data */
	       ikey = strtol(tree_getkey(times), (char**)NULL, 10);
	       tableset_reset(tset);
	       tableset_where(tset, "_time", eq, tree_getkey(times));
	       tableset_excludet(tset, "_time _dur");
	       str = tableset_print(tset, TABSET_NOTPRETTY, TABSET_NONAMES,
				    TABSET_NOINFO, TABSET_WITHBODY);
	       d = xnmalloc(sizeof(struct rs_data_block));
	       d->time = ikey;
	       d->hd_hashkey = hash;
	       d->data = str;
	       d->__priv_alloc_mem = d->data;
	       itree_add(dblocks, ikey, d);
	       /*table_destroy(itab);*/
	  }
	  tree_destroy(times);
     } else {
          /* treat the data as a single sequence and use the current time */
          tableset_excludet(tset, "_dur");
	  itab = tableset_into(tset);
	  i = table_getcurrentrowkey(itab);
	  d = xnmalloc(sizeof(struct rs_data_block));
	  d->time = time(NULL);
	  d->hd_hashkey = hash;
	  d->data = table_outbody(itab);
	  d->__priv_alloc_mem = d->data;
	  itree_append(dblocks, d);
	  table_destroy(itab);
     }
     tableset_destroy(tset);

     /* clean up and return */
     return dblocks;
}


/*
 * Take a list of RS_DBLOCKs and create a single TABLE, which may 
 * contain multiple sequences.
 * The pair (ring, hash_lookup) are required to divine the header string
 * and thus create the table.
 * If existing_tab is passed, new data will be appended to that table
 * and additional columns created as needs be and the table is returned.
 * If existing_tab is NULL, then a new table will be created and returned.
 * NULL is returned for an error.
 */
TABLE rs_priv_dblock_to_table(ITREE *db,	/* list of dblocks */
			      RS ring,		/* open ring */
			      char *(hash_lookup)(RS, unsigned long),
			      TABLE existing_tab, /* append to table */
			      int musthave_seq,	/* must have sequence col */
			      int musthave_time,/* must have time col */
			      int musthave_dur	/* must have duration col*/ )
{
     RS_DBLOCK dblock;
     TABLE tab, loadtab=NULL;
     char *hd, *duphd, *data;
     TREE *row;
     int hasseq, hastime, hasdur, rowkey;

     if (existing_tab)
	  tab = existing_tab;
     else
	  tab = table_create();
     
     /* if we must have meta data in final table, make sure we have 
      * the right columns */
     if (musthave_seq)
          table_addcol(tab, "_seq", NULL);
     if (musthave_time)
          table_addcol(tab, "_time", NULL);
     if (musthave_dur)
          table_addcol(tab, "_dur", NULL);

     itree_traverse(db) {
          /* fetch header from each dblock's hash key and hold in the 
	   * header of the loading table */
          dblock = itree_get(db);
	  hd = hash_lookup(ring, dblock->hd_hashkey);
	  if (!hd) {
	       if (!existing_tab)
		    table_destroy(tab);
	       return NULL;
	  }
	  duphd  = xnstrdup(hd);
	  loadtab = table_create_s(duphd);
	  table_freeondestroy(loadtab, duphd);

	  /* add columns from loading table in to collection table
	   * and save the schema and loading table as a template */
	  table_addtable(tab, loadtab, 1);
#if 0
	  itree_add(schema, dblock->hd_hashkey, loadtab);
#endif

	  /* cache the state of the special cols to make slightly faster */
	  hasseq = hastime = hasdur = 0;
	  if (table_hascol(tab, "_seq"))
	       hasseq++;
	  if (table_hascol(tab, "_time"))
	       hastime++;
	  if (table_hascol(tab, "_dur"))
	       hasdur++;

	  /* drop _time, _seq and _dur from loading table and scan 
	   * dblock's data into it. DBLOCK data areas have these 
	   * columns removed and placed into other parts of the 
	   * DBLOCK structure */
	  table_rmcol(loadtab, "_dur");
	  table_rmcol(loadtab, "_time");
	  table_rmcol(loadtab, "_seq");
	  data = xnstrdup(dblock->data);
	  table_freeondestroy(loadtab, data);
	  table_scan(loadtab, data, RS_VALSEP, TABLE_SINGLESEP, 
		     TABLE_NOCOLNAMES, TABLE_NORULER);

	  /* append to collection table, adding in _dur, _time, 
	   * and _seq values if needs be */
	  table_traverse(loadtab) {
	       row = table_getcurrentrow(loadtab);
	       rowkey = table_addrow_alloc(tab, row);
	       if (hasseq)
		    table_replacecell_alloc(tab, rowkey, "_seq", 
					    util_i32toa(itree_getkey(db)));
	       if (hastime)
		    table_replacecell_alloc(tab, rowkey, "_time", 
					    util_i32toa(dblock->time));
	       if (hasdur)
		    table_replacecell_alloc(tab, rowkey, "_dur", 
					    util_i32toa(ring->duration));
	       tree_destroy(row);
	  }

	  /* clear up */
	  table_destroy(loadtab);
     }

     return tab;
}



/*
 * Free the space taken by a list of RS_DBLOCK when returned by 
 * ll_read_dblock()
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


/*
 * Validate the ring exists, load its index and update the ring buffer
 * sequence pointers in the ring descriptor. Requires a read or write lock.
 * Returns 1 for success or 0 for failure (like the ring does not 
 * exist anymore)
 */
int    rs_priv_load_index(RS ring, TABLE *index)
{
     TABLE it;

     /* read ring index from store and record the first and last sequences 
      * in the ring descriptor */
     it = ring->method->ll_read_index(ring->handle, ring->ringid);
     if ( ! it )
	  return 0;	/* failure */
     if (table_nrows(it)) {
	  table_first(it);
	  ring->oldest      = strtol(table_getcurrentcell(it, "seq"),
				     (char**)NULL, 10);
	  ring->oldest_t    = strtol(table_getcurrentcell(it, "time"), 
				     (char**)NULL, 10);
	  ring->oldest_hash = strtol(table_getcurrentcell(it, "hd_hash"), 
				     (char**)NULL, 10);
	  table_last(it);
	  ring->youngest      = strtol(table_getcurrentcell(it, "seq"),
				       (char**)NULL, 10);
	  ring->youngest_t    = strtol(table_getcurrentcell(it, "time"), 
				       (char**)NULL, 10);
	  ring->youngest_hash = strtol(table_getcurrentcell(it, "hd_hash"),
				       (char**)NULL, 10);
     } else {
	  ring->oldest      = ring->youngest      = -1;
	  ring->oldest_t    = ring->youngest_t    = -1;
	  ring->oldest_hash = ring->youngest_hash = 0;
     }
     if (ring->current < ring->oldest)
	  ring->current      = ring->oldest;

     *index = it;
     return 1;		/* success */
}


/*
 * Cache aware storage helper function. Given a header string, its 
 * corresponding unique value is returned and the association will have 
 * been stored persistantly.
 * Column headers for _seq, _time and _dur are kept so the subtially of
 * info lines can be maintained. However they will be treated specially 
 * in the body processing routines.
 * Resolves any hash value clashes that may occur and is cache aware.
 * Must be called within a write lock
 */
unsigned long rs_priv_header_to_hash(RS ring, char *header)
{
     unsigned long hash;
     ITREE *newheaders;
     int r;

     /* search for header */
     hash = hash_str(header);
     while (itree_find(ring->hdcache, hash) != ITREE_NOVAL) {
	  if (strcmp(itree_get(ring->hdcache), header) == 0) {
	       return hash;
	  }
	  hash++;
     }

     /* association not in cache, read headers from store and try again */
     newheaders = ring->method->ll_read_headers(ring->handle);
     if (newheaders) {
	  if (ring->hdcache) {
	       itree_clearoutandfree(ring->hdcache);
	       itree_destroy(ring->hdcache);
	  }
	  ring->hdcache = newheaders;

	  hash = hash_str(header);
	  while (itree_find(ring->hdcache, hash) != ITREE_NOVAL) {
	       if (strcmp(itree_get(ring->hdcache), header) == 0) {
		    return hash;
	       }
	       hash++;
	  }
     }

     /* new association, write to cache and then write cache to store.
      * hash should be unique due to the while loop above */
     itree_add(ring->hdcache, hash, xnstrdup(header));
     r = ring->method->ll_write_headers(ring->handle, ring->hdcache);
     if (!r)
	  elog_printf(ERROR,  0, 
		      "unable to write headers, store may become unsafe");

     return hash;
}


/*
 * Return the header string given its corresponding unique value.
 * Is cache aware and should be called within a read lock in case it needs
 * to read from storage
 * Returns NULL if the value has not been stored.
 */
char * rs_priv_hash_to_header(RS ring, unsigned long hdhash)
{
     ITREE *newheaders;

     if (itree_find(ring->hdcache, hdhash) == ITREE_NOVAL) {
	  /* fetch a new set of headers from disk */
	  newheaders = ring->method->ll_read_headers(ring->handle);
	  if (newheaders) {
	       if (ring->hdcache) {
		    itree_clearoutandfree(ring->hdcache);
		    itree_destroy(ring->hdcache);
	       }
	       ring->hdcache = newheaders;	  
	  } else {
	       return NULL;
	  }

	  if (itree_find(ring->hdcache, hdhash) == ITREE_NOVAL)
	       return NULL;
     }

     return itree_get(ring->hdcache);
}



#if TEST
#include "rs_gdbm.h"		/* relies on this sample implementation */
#include "route.h"
#include "rt_std.h"

#define RSFILE1  "t.rs.1.rs"
#define RSRING1  "ring1"
#define RSRINGL1 "ring 1 long name"
#define RSHEADER1 "tom\tdick\tharry"
#define RSTEXT1 RSHEADER1 "\n--\n1\t2\t3"
#define RSTEXT2 "tom\tdick\tharry\t_seq\n--\n1\t2\t3\t0\n2\t3\t4\t1\n3\t4\t5\t2\n"
#define RSTEXT3 "tom\tdick\tharry\t_seq\n--\n1\t2\t3\t0\n2\t3\t4\t0\n3\t4\t5\t0\n"
#define RSTEXT4 "tom\tdick\tharry\n--\n1\t2\t3\n2\t3\t4\n3\t4\t5\n"
#define RSHEADER5 "tom\001dick\001harry"
#define RSTEXT5 RSHEADER5 "\n--\n1\0012\0013"

int main() {
     RS rs1;
     TABLE tab1,  tab2,  tab3,  tab4,  tab5;
     char *buf1, *buf2, *buf3, *buf4, *buf5;
     int r;
     ITREE *dblock1;
     struct rs_data_block *a_dblock;
     unsigned long hash1;

     route_init(NULL, 0);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     elog_init(0, "holstore test", NULL);
     rs_init();

     /* clear up before we start */
     unlink(RSFILE1);

     /* test 1a: open a non-existant file */
     rs1 = rs_open(&rs_gdbm_method, RSFILE1, 0644, "should_fail", RSRINGL1,
		   "Initial test ring", 5, 5, 0);
     if (rs1)
	  elog_die(FATAL, 
		   "[1a] shouldn't open bad ringstore name");

     /* test 1b: open and shut */
     rs1 = rs_open(&rs_gdbm_method, RSFILE1, 0644, RSRING1, RSRINGL1,
		   "Initial test ring", 5, 5, RS_CREATE);
     if (!rs1)
	  elog_die(FATAL, "[1b] Can't create ringstore");
     rs_close(rs1);
     rs1 = rs_open(&rs_gdbm_method, RSFILE1, 0644, RSRING1, RSRINGL1,
		   "Initial test ring", 5, 5, RS_CREATE);
     if (!rs1)
	  elog_die(FATAL, "[1b] Can't open ringstore with create flag");
     rs_close(rs1);

     /* test 1c: open non-existant ring */
     rs1 = rs_open(&rs_gdbm_method, RSFILE1, 0644, "should_fail", RSRINGL1,
		   "Initial test ring", 5, 5, 0);
     if (rs1)
	  elog_die(FATAL, 
		   "[1c] shouldn't open bad ringstore name");

     /* test 1d: reopen and shut */
     rs1 = rs_open(&rs_gdbm_method, RSFILE1, 0644, RSRING1, RSRINGL1,
		   "Initial test ring", 5, 5, 0);
     if (!rs1)
	  elog_die(FATAL, "[1d] Can't open ringstore");
     rs_close(rs1);


     /* test 2: clear box! check implementation of the private routine to 
      * convert between data tables and DBLOCKS. */
     tab1 = table_create();
     buf1 = xnstrdup(RSTEXT1);
     r = table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     dblock1 = rs_priv_table_to_dblock(tab1, 999);
     if (itree_n(dblock1) != 1)
	  elog_die(FATAL, "[2a] bad number of dblocks %d != 1",
		   itree_n(dblock1));

     itree_first(dblock1);
     a_dblock = itree_get(dblock1);
     if (strcmp(a_dblock->data, "1\t2\t3\n") != 0)
	  elog_die(FATAL, "[2a] bad data block: %s", a_dblock->data);

     table_destroy(tab1);
     nfree(buf1);
     rs_free_dblock(dblock1);

     tab1 = table_create();
     buf1 = xnstrdup(RSTEXT2);
     r = table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     dblock1 = rs_priv_table_to_dblock(tab1, 999);
     if (itree_n(dblock1) != 3)
	  elog_die(FATAL, "[2b] bad number of dblocks %d != 3",
		   itree_n(dblock1));

     itree_first(dblock1);
     a_dblock = itree_get(dblock1);
     if (strcmp(a_dblock->data, "1\t2\t3\n") != 0)
	  elog_die(FATAL, "[2b] bad data block 1: %s", a_dblock->data);
     itree_next(dblock1);
     a_dblock = itree_get(dblock1);
     if (strcmp(a_dblock->data, "2\t3\t4\n") != 0)
	  elog_die(FATAL, "[2b] bad data block 2: %s", a_dblock->data);
     itree_next(dblock1);
     a_dblock = itree_get(dblock1);
     if (strcmp(a_dblock->data, "3\t4\t5\n") != 0)
	  elog_die(FATAL, "[2b] bad data block 3: %s", a_dblock->data);

     table_destroy(tab1);
     nfree(buf1);
     rs_free_dblock(dblock1);

     tab1 = table_create();
     buf1 = xnstrdup(RSTEXT3);
     r = table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     dblock1 = rs_priv_table_to_dblock(tab1, 999);
     if (itree_n(dblock1) != 1)
	  elog_die(FATAL, "[2c] bad number of dblocks %d != 1",
		   itree_n(dblock1));

     itree_first(dblock1);
     a_dblock = itree_get(dblock1);
     if (strcmp(a_dblock->data, "1\t2\t3\n2\t3\t4\n3\t4\t5\n") != 0)
	  elog_die(FATAL, "[2c] bad data block: %s", a_dblock->data);

     table_destroy(tab1);
     nfree(buf1);
     rs_free_dblock(dblock1);

     tab1 = table_create();
     buf1 = xnstrdup(RSTEXT4);
     r = table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     dblock1 = rs_priv_table_to_dblock(tab1, 999);
     if (itree_n(dblock1) != 1)
	  elog_die(FATAL, "[2d] bad number of dblocks %d != 1",
		   itree_n(dblock1));

     itree_first(dblock1);
     a_dblock = itree_get(dblock1);
     if (strcmp(a_dblock->data, "1\t2\t3\n2\t3\t4\n3\t4\t5\n") != 0)
	  elog_die(FATAL, "[2d] bad data block: %s", a_dblock->data);

     table_destroy(tab1);
     nfree(buf1);
     rs_free_dblock(dblock1);

     /* test 3: clear box! check implementation of the private routine to 
      * convert between DBLOCKS and data tables. 
      * white box testing, needs access to file locking, not normally given
      */
     rs1 = rs_open(&rs_gdbm_method, RSFILE1, 0644, RSRING1, RSRINGL1,
		   "Initial test ring", 5, 5, RS_CREATE);
     if (!rs1)
	  elog_die(FATAL, "[3] Can't open ringstore");
     rs1->method->ll_lock(rs1->handle, RS_RDLOCK, "main");
     tab1 = table_create();
     buf1 = xnstrdup(RSTEXT1);
     r = table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     hash1 = rs_priv_header_to_hash(rs1, RSHEADER1);
     dblock1 = rs_priv_table_to_dblock(tab1, hash1);
     tab2 = rs_priv_dblock_to_table(dblock1, rs1, rs_priv_hash_to_header, 
				    NULL, 0, 0, 0);
     table_rmcol(tab2, "_seq");
     table_rmcol(tab2, "_time");
     buf2 = table_outtable(tab2);
     if (strncmp(RSTEXT1, buf2, sizeof(RSTEXT1)-1) != 0)
	  elog_die(FATAL, 
		   "[3] tables do not match:-\nBUF1\n%s\nBUF2\n%s", 
		   RSTEXT1, buf2);
     rs1->method->ll_unlock(rs1->handle);
     rs_close(rs1);

     nfree(buf1);
     nfree(buf2);
     table_destroy(tab1);
     table_destroy(tab2);
     rs_free_dblock(dblock1);


     /* test 4: put a piece of data on the ring and read it back */
     tab1 = table_create();
     buf1 = xnstrdup(RSTEXT1);
     r = table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[4] does not scan 1 line");

     rs1 = rs_open(&rs_gdbm_method, RSFILE1, 0644, RSRING1, RSRINGL1,
		   "Initial test ring", 5, 5, RS_CREATE);
     if (!rs1)
	  elog_die(FATAL, "[4] Can't create ringstore");
     if (!rs_put(rs1, tab1))
	  elog_die(FATAL, "[4] Didn't save table");
     tab2 = rs_get(rs1, 0);
     rs_close(rs1);
     table_rmcol(tab2, "_seq");
     table_rmcol(tab2, "_time");
     buf2 = table_outtable(tab2);
     if (strncmp(RSTEXT1, buf2, sizeof(RSTEXT1)-1) != 0)
	  elog_die(FATAL, 
		   "[4] tables do not match:-\nBUF1\n%s\nBUF2\n%s", 
		   RSTEXT1, buf2);

     nfree(buf1);
     nfree(buf2);
     table_destroy(tab1);
     table_destroy(tab2);

     /* test 5: put sets of data on the ring and read them back */
     tab1 = table_create();
     buf1 = xnstrdup(RSTEXT1);
     r = table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[5a1] does not scan 1 line");
     tab2 = table_create();
     buf2 = xnstrdup(RSTEXT2);
     r = table_scan(tab2, buf2, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 3)
	  elog_die(FATAL, "[5a2] does not scan 3 lines");
     tab3 = table_create();
     buf3 = xnstrdup(RSTEXT3);
     r = table_scan(tab3, buf3, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 3)
	  elog_die(FATAL, "[5a3] does not scan 3 lines");
     tab4 = table_create();
     buf4 = xnstrdup(RSTEXT4);
     r = table_scan(tab4, buf4, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 3)
	  elog_die(FATAL, "[5a4] does not scan 3 line");

     /* open file */
     rs1 = rs_open(&rs_gdbm_method, RSFILE1, 0644, RSRING1, RSRINGL1,
		   "Initial test ring", 5, 5, RS_CREATE);
     if (!rs1)
	  elog_die(FATAL, "[5b] Can't create ringstore");
     if (!rs_put(rs1, tab1))
	  elog_die(FATAL, "[5b] Didn't save table");
     /* should be two sequences in a 5 slot ring: read them back */
     tab5 = rs_get(rs1, 0);
     if (!tab5)
	  elog_die(FATAL, "[5b1] nothing returned by rs_get()");
     table_rmcol(tab5, "_time");
     table_rmcol(tab5, "_seq");
     buf5 = table_outtable(tab5);
     if (strncmp(RSTEXT1, buf5, sizeof(RSTEXT1)-1) != 0)
	  elog_die(FATAL, 
		   "[5b1] tables do not match:-\nEXPECT\n%s\nREAD\n%s", 
		   RSTEXT1, buf5);
     nfree(buf5);
     table_destroy(tab5);
     tab5 = rs_get(rs1, 0);
     if (!tab5)
	  elog_die(FATAL, "[5b2] nothing returned by rs_get()");
     table_rmcol(tab5, "_time");
     table_rmcol(tab5, "_seq");
     buf5 = table_outtable(tab5);
     if (strncmp(RSTEXT1, buf5, sizeof(RSTEXT1)-1) != 0)
	  elog_die(FATAL, 
		   "[5b2] tables do not match:-\nEXPECT\n%s\nREAD\n%s", 
		   RSTEXT1, buf5);
     nfree(buf5);
     table_destroy(tab5);
     tab5 = rs_get(rs1, 0);
     if (tab5)
	  elog_die(FATAL, "[5b3] should be at end of ring");

     /* write once (three sequence table) read three */
     if (!rs_put(rs1, tab2))
	  elog_die(FATAL, "[5c] Didn't save table");
     tab5 = rs_get(rs1, 0);
     if (!tab5)
	  elog_die(FATAL, "[5c1] nothing returned by rs_get()");
     table_rmcol(tab5, "_time");
     table_rmcol(tab5, "_seq");
     buf5 = table_outtable(tab5);
     if (strncmp(RSTEXT1, buf5, sizeof(RSTEXT1)-1) != 0)
	  elog_die(FATAL, 
		   "[5c1] tables do not match:-\nEXPECT\n%s\nREAD\n%s", 
		   RSTEXT1, buf5);
     nfree(buf5);
     table_destroy(tab5);
     tab5 = rs_get(rs1, 0);
     if (!tab5)
	  elog_die(FATAL, "[5c2] nothing returned by rs_get()");
     table_rmcol(tab5, "_time");
     table_rmcol(tab5, "_seq");
     buf5 = table_outtable(tab5);
     if (strncmp("tom\tdick\tharry\n--\n2\t3\t4", buf5, 23) != 0)
	  elog_die(FATAL, 
		   "[5c2] tables do not match:-\nEXPECT\n%s\nREAD\n%s", 
		   "tom\tdick\tharry\n--\n2\t3\t4", buf5);
     nfree(buf5);
     table_destroy(tab5);
     tab5 = rs_get(rs1, 0);
     if (!tab5)
	  elog_die(FATAL, "[5c3] nothing returned by rs_get()");
     table_rmcol(tab5, "_time");
     table_rmcol(tab5, "_seq");
     buf5 = table_outtable(tab5);
     if (strncmp("tom\tdick\tharry\n--\n3\t4\t5", buf5, 23) != 0)
	  elog_die(FATAL, 
		   "[5c3] tables do not match:-\nEXPECT\n%s\nREAD\n%s", 
		   "tom\tdick\tharry\n--\n3\t4\t5", buf5);
     nfree(buf5);
     table_destroy(tab5);
     tab5 = rs_get(rs1, 0);
     if (tab5)
	  elog_die(FATAL, "[5c4] should be at end of ring");

     /* write twice (single seqences), read twice */
     if (!rs_put(rs1, tab3))
	  elog_die(FATAL, "[5d1] Didn't save table");
     if (!rs_put(rs1, tab4))
	  elog_die(FATAL, "[5d2] Didn't save table");
     tab5 = rs_get(rs1, 0);
     if (!tab5)
	  elog_die(FATAL, "[5d3] nothing returned by rs_get()");
     table_rmcol(tab5, "_time");
     table_rmcol(tab5, "_seq");
     buf5 = table_outtable(tab5);
     if (strncmp("tom\tdick\tharry\n--\n1\t2\t3\n2\t3\t4\n3\t4\t5", buf5, 26) != 0)
	  elog_die(FATAL, 
		   "[5d3] tables do not match:-\nEXPECT\n%s\nREAD\n%s", 
		   "tom\tdick\tharry\n--\n1\t2\t3\n2\t3\t4\n3\t4\t5", buf5);
     nfree(buf5);
     table_destroy(tab5);
     tab5 = rs_get(rs1, 0);
     if (!tab5)
	  elog_die(FATAL, "[5d4] nothing returned by rs_get()");
     table_rmcol(tab5, "_time");
     table_rmcol(tab5, "_seq");
     buf5 = table_outtable(tab5);
     if (strncmp(RSTEXT4, buf5, sizeof(RSTEXT4)-1) != 0)
	  elog_die(FATAL, 
		   "[5d4] tables do not match:-\nEXPECT\n%s\nREAD\n%s", 
		   RSTEXT4, buf5);

     rs_close(rs1);
     nfree(buf1);
     nfree(buf2);
     nfree(buf3);
     nfree(buf4);
     nfree(buf5);
     table_destroy(tab1);
     table_destroy(tab2);
     table_destroy(tab3);
     table_destroy(tab4);
     table_destroy(tab5);

     elog_printf(INFO, "all tests successfully completed");

     rs_fini();
     elog_fini();
     route_fini();
     exit(0);
}

#endif /* TEST */
