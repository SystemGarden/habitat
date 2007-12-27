/* 
 * Ringstore
 * 
 * Provides flexible storage and quick access of time series data in 
 * database file. Designed for Habitat, providing storage for TABLE 
 * data types
 *
 * Nigel Stuckey, July 2001
 *
 * Copyright System Garden Limited 2001. All rights reserved.
 */

#include <time.h>
#include "tree.h"
#include "itree.h"
#include "nmalloc.h"
#include "table.h"
#include "ringstore.h"

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
 * Within each slot, data is input in rows of attributes (using TABLE 
 * data types) where the values share a common sample time. 
 * Multiple instances of the same data type (such as performance of 
 * multiple disks) are held in separate rows in the same sample and 
 * resolved by identifing unique keys.
 * Unique sequencies are automatically allocated to resolve high frequency
 * data (time is only represented in seconds).
 * The default behaviour of insertion may be changed by specifing
 * meta data in the TABLE columns on insertion to give greater flexability.
 * The API is stateful, like file access. You seek, read one or many 
 * records, etc and you close.
 *
 * IMPLEMENTATION
 *
 * It is implemented using Mird as the low level storage system
 * and is optimised for the retreival of values ordered over time 
 * of a single attribute of data.
 *
 * options, xbase is good but written in c++
 * mird is c but not multiuser (at the open close level only)
 * first base is in c++ and a bit wacky but ok
 * typhoon is c but no locking and is not actively supported
 * firstbase is v good but the library is huge but .a may make bins smaller
 * 
 */
/*
 * Implementation
 *
 * When an ringstore instance is created (RS is the handle data type), 
 * a single Mird database is used specified by the file name. A single 
 * identity table exists in each database containing data about the 
 * creating host, such as system type, timezone etc.
 *
 * A single ring index table takes the ring name and uses it as a string to 
 * find the ring's static information, such as ring length, description 
 * names, index names etc.
 *
 * The headers from the TABLE data types are separated from the data and
 * placed in a single table of header strings. the headers are the column 
 * names and info rows. The headers are indexed by a hash of the header 
 * string, forming an int data type. Mird supports this directly and 
 * will handle key clashes (I hope).
 *
 * The text representation of the data from the TABLE is inserted in the
 * the data table for that ring.
 *
 */

#include "mird.h"

void rs_priv_mird_error(enum elog_severity, int myerrno, char *filename, 
			char *ringname, MIRD_RES mr);
int   rs_errorno;
char *rs_errorstr[] = {
     "unable to initialiase ringstore",
     "unable to create ringstore",
     "unable to synchronise after ringstore creation",
     "unable to close after ringstore creation",
     "unable to open ringstore",
     "unable to reinitialiase ringstore after creation",
     NULL
};

/* ---- file and ring ---- */

/*
 * Initialise the the ringstore class
 */
void  rs_init()
{
}


/*
 * Finalise the ringstore class and shut it down
 */
void  rs_fini()
{
}


/*
 * Open a ring within a ringstore. 
 * If the ringstore file does not exist and flags contain RS_CREATE 
 * as part of its bitmask, then the file `filename' will be created
 * using the permissions contained in `filemode'.
 * Likewise, if the ring does not exist, that it will be created
 * with size of `nslots' and text `description' if RS_CREATE is specified 
 * If the file and ring exists, then filemode, description and nslots will
 * all be ignored.
 * If `nslots' is 0, then the ring will be unlimited in length.
 * Returns a descriptor if successful or NULL for failure 
 * (see rs_errno() and rs_errstr()).
 */
RS    rs_open(char *filename	/* name of file */, 
	      int filemode	/* permission mode of file */, 
	      char *ringname	/* name of ring within file */,
	      char *description	/* text description */, 
	      int nslots	/* length of ring or 0 for unlimited */,
	      int flags		/* mask of RS_CREATE, 0 otherwise */)
{
     struct rs_session *ring;
     MIRD_RES mr;

     /* allocate structures */
     ring = xnmalloc(sizeof(struct rs_session));
     mr = mird_initialize(filename, &ring->db);
     if (mr) {
	  rs_priv_mird_error(ERROR, RS_ENOINIT, filename, ringname, mr);
	  mird_free_error(mr);
	  xnfree(ring);
	  return NULL;
     }

     if (flags | RS_CREATE) {
	  /* The caller wants to create the file if it does not exist;
	   * attempt creation of Mird file and RS supertable by using
	   * the following combination which should call open(2) with
	   * O_EXCL|O_CREAT as options */
	  ring->db->flags |= MIRD_EXCL;
	  ring->db->file_mode = filemode;
	  mr = mird_open(ring->db);
	  if (mr) {
	       if (mr->error_no == MIRDE_OPEN_CREATE) {
		    /* The file exists, we do not need to write anything 
		     * inside it. Proceed to opening... */
		    mird_free_error(mr);
		    goto rs_open_normal;
	       } else {
		    rs_priv_mird_error(ERROR, RS_ENOCREATE, filename, 
				       ringname, mr);
		    mird_free_error(mr);
		    xnfree(ring);
		    return NULL;
	       }
	  }

	  /*
	   * The empty database is ours!
	   * Write the supertable to the standard location containing
	   * information about the creating system
	   */

	  /* Close file and carry on in shared mode.
	   * If we can't sync, then there's not much we can do but carry on
	   * but if we can't close, then its best we stop. 
	   * Reinitialisation is required as rs_close() clears the 
	   * ring->db structure */
	  mr = mird_sync(ring->db);
	  if (mr) {
	       rs_priv_mird_error(ERROR, RS_ENOSYNC, filename, ringname, mr);
	       mird_free_error(mr);
	  }
	  mr = mird_close(ring->db);
	  if (mr) {
	       rs_priv_mird_error(ERROR, RS_ENOCLOSE, filename, ringname, mr);
	       mird_free_error(mr);
	       xnfree(ring);
	       return NULL;
	  }
	  mr = mird_initialize(filename, &ring->db);
	  if (mr) {
	       rs_priv_mird_error(ERROR, RS_ENOREINIT, filename, ringname, mr);
	       mird_free_error(mr);
	       xnfree(ring);
	       return NULL;
	  }
     }

     /* open an existing, prepared environment */
rs_open_normal:
     ring->db->flags |= MIRD_NOCREATE;
     ring->db->flags &= ~MIRD_EXCL;	/* remove exclusive flag */
     mr = mird_open(ring->db);
     if (mr) {
BIG PROBLEM -- NOT MULTI USER
	  rs_priv_mird_error(ERROR, RS_ENOOPEN, filename, ringname, mr);
	  xnfree(ring);
	  return NULL;
     }

     /* check the existance of our ring and create if not there */

     return ring;
}


/*
 * Close an open ringstore descriptor
 */
void  rs_close(RS ring	/* ring descriptor */)
{
     
     xnfree(ring);
}


/*
 * Remove a ring from a ringstore file.
 * Returns 1 if successful or 0 otherwise (see rs_errno() and rs_errstr()).
 */
int   rs_destroy(char *filename	/* name of file */,
		 char *ringname /* name of ring */)
{
     return 0;	/* failure - to be implemented */
}


/* ---- stateful record oriented transfer ---- */

/*
 * Append data to a ring, and remove the oldest if capacity is reached.
 * The current reading position is unaffected.
 * Returns 1 for success or 0 for error (see rs_errno() and rs_errstr()).
 */
int   rs_put(RS ring	/* ring descriptor */, 
	     TABLE data	/* data contained in a table */)
{
     return 0;	/* failure - to be implemented */
}


/*
 * Get the data table at the current reading position of the ring and 
 * advance the position to the next data slot. Returns a TABLE on 
 * success or NULL on failure (see rs_errno() and rs_errstr()).
 * Table will include '_seq' and '_ins' columns for sequence and
 * insertion time
 */
TABLE rs_get(RS ring	/* ring descriptor */)
{
     return NULL;	/* failure - to be implemented */
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
     return 0;	/* failure - to be implemented */
}


/*
 * Get multiple sets of data starting from the current read point
 * and extending by a maximum of nsequencies. All data is returned 
 * in a single table but can be distinguished by using the '_seq' column.
 * '_ins' is also provided for insertion time.
 * The current read point is positioned to the slot beyond the 
 * last included in the returned TABLE.
 * Returns TABLE if successful or NULL on error, generially that
 * there is no data to read (see rs_errno() and rs_errstr()).
 */
TABLE rs_mget_byseqs(RS ring		/* ring descriptor */, 
		     int nsequences	/* slots contained in result */)
{
     return NULL;	/* failure - to be implemented */
}


/*
 * Get multiple sets of data starting from the current read point
 * and covering all data older that `last_t'. All data is returned 
 * in a single table but can be distinguished by using the '_seq' column.
 * '_ins' is also provided for insertion time.
 * The current read point is positioned to the slot beyond the 
 * last included in the returned TABLE.
 * Returns TABLE if successful or NULL on error, generially that
 * there is no data to read (see rs_errno() and rs_errstr()).
 */
TABLE rs_mget_bytime(RS ring		/* ring descriptor */, 
		     time_t last_t	/* youngest time to read */)
{
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
     return -1;	/* empty ring - to be implemented */
}


/*
 * Set the sequence and insertion time of the youngest data in the ring
 * in two supplied variables. 
 * Sequences run from 0 and if -1 is returned, 
 * then the ring is empty and is not considered an error.
 * Returns the sequence number; sequence and time may be NULL.
 */
int   rs_youngest (RS ring		/* ring descriptor */, 
		   int *sequence	/* RETURN: sequence number */, 
		   time_t *time		/* RETURN: insertion time */)
{
     return -1;	/* empty ring - to be implemented */
}


/*
 * Set the sequence and insertion time of the oldest data in the ring
 * in two supplied variables. 
 * Sequences run from 0 and if -1 is returned, 
 * then the ring is empty and is not considered an error.
 * Returns the sequence number; sequence and time may be NULL.
 */
int   rs_oldest   (RS ring		/* ring descriptor */, 
		   int *sequence	/* RETURN: sequence number */, 
		   time_t *time		/* RETURN: insertion time */)
{
     return -1;	/* empty ring - to be implemented */
}


/*
 * Move the current reading position back nsequencies
 * Returns the actual number of slots moved
 */
int   rs_rewind   (RS ring		/* ring descriptor */, 
		   int nsequencies	/* move back these slots */)
{
     return 0;	/* moved nowhere - to be implemented */
}


/*
 * Move the current reading position forward nsequencies
 * Returns the actual number of slots moved
 */
int   rs_forward  (RS ring		/* ring deswcriptor*/, 
		   int nsequencies	/* move forward these slots */)
{
     return 0;	/* moved nowhere - to be implemented */
}


/*
 * Set the current reading position to sequence
 * Returns the sequence number is successful or -1 otherwise
 */
int   rs_goto_seq (RS ring	/* ring descriptor */, 
		   int sequence	/* goto this sequence */)
{
     return -1;	/* failure - to be implemented */
}


/*
 * Set the current reading position to the data whose insertion time 
 * is on or before time.
 * Returns the sequence number is successful or -1 otherwise
 */
int   rs_goto_time(RS ring	/* ring descriptor */, 
		   time_t time	/* youngest data time */)
{
     return -1;	/* failure - to be implemented */
}


/* ---- stateless column oriented reading ---- */

/*
 * Return the column names (attributes) used by data between and
 * including the two supplied sequencies. The list returned contains the
 * names in the keys, values are undefined. Free the list with
 * "tree_clearout(t, tree_infreemem, NULL); tree_destroy(t)".
 * The current record position is not affected.
 * NULL is returned for error.
 */
TREE  *rs_colnames_byseqs(RS ring	/* ring descriptor */, 
			  int from_seq	/* oldest/starting sequence */, 
			  int to_seq	/* youngest/ending sequence */)
{
     return NULL;	/* failure - to be implemented */
}


/*
 * Return the column names (attributes) used by data between and
 * including the two supplied times. The list returned contains the
 * names in the keys, values are undefined. Free the list with
 * "tree_clearout(t, tree_infreemem, NULL); tree_destroy(t)".
 * The current record position is not affected.
 * NULL is returned for error.
 */
TREE  *rs_colnames_bytime(RS ring	/* ring descriptor */, 
			  int from_t	/* oldest/starting time */,   
			  int to_t	/* youngest/ending time */)
{
     return NULL;	/* failure - to be implemented */
}


/*
 * Get a single column of data between and including the sequencies
 * supplied.  The returned list has the sequence number as key
 * and the column data as value.
 * Free the list with "itree_clearoutandfree(); itree_destroy()".
 * The current record position is not affected.
 * NULL is returned for error
 */
ITREE *rs_getcol_byseq  (RS ring	/* ring descriptor */, 
			 char *colname	/* column/attribute name */,  
			 int from_seq	/* oldest/starting sequence */, 
			 int to_seq	/* youngest/ending sequence */)
{
     return NULL;	/* failure - to be implemented */
}


/*
 * Get a single column of data between and including the times
 * supplied.  The returned list has the time as key
 * and the column data as value.
 * The problem with this method is that if several samples occur 
 * at the same time, the order may not be maintained with in each
 * time quanta. If this is a problem, use rs_getcols_bytime()
 * instead, which will return a table.
 * Free the list with "itree_clearoutandfree(); itree_destroy()".
 * The current record position is not affected.
 * NULL is returned for error
 */
ITREE *rs_getcol_bytime (RS ring	/* ring descriptor */, 
			 char *colname	/* column/attribute name */,  
			 int from_t	/* oldest/starting time */,   
			 int to_t	/* youngest/ending time */)
{
     return NULL;	/* failure - to be implemented */
}


/*
 * Get named columns of data between and including the sequencies
 * supplied.  Column names should be listed as the keys in the TREE list,
 * values are not used.
 * Returns a table including the column names asked for and two
 * meta columns: '_seq' for sequence number of row and '_ins' for 
 * insertion time.
 * Free the table with table_destroy().
 * The current record position is not affected.
 * NULL is returned for error
 */
TABLE  rs_getcols_byseq (RS ring	/* ring descriptor */, 
			 TREE *colnames	/* col names as keys */, 
			 int from_seq	/* oldest/starting sequence */, 
			 int to_seq	/* youngest/ending sequence */)
{
     return NULL;	/* failure - to be implemented */
}


/*
 * Get named columns of data between and including the times
 * supplied.  Column names should be listed as the keys in the TREE list,
 * values are not used.
 * Returns a table including the column names asked for and two
 * meta columns: '_seq' for sequence number of row and '_ins' for 
 * insertion time.
 * Free the table with table_destroy().
 * The current record position is not affected.
 * NULL is returned for error
 */
TABLE  rs_getcols_bytime(RS ring	/* ring descriptor */, 
			 TREE *colnames	/* col names as keys */, 
			 int from_t	/* oldest/starting time */,   
			 int to_t	/* youngest/ending time */)
{
     return NULL;	/* failure - to be implemented */
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
     return 0;	/* failure - to be implemented */
}


/*
 * Remove nkill data from the oldest part of the ring, leaving the
 * number of slots unchanged. The same quantity of data can be 
 * added to the newest part of the ring with out destroying any
 * additional data as 'nkill' slots are free.
 * Returns the number of slots of data actually purged if successful or 
 * 0 for failure.
 */
int   rs_purge(RS ring		/* ring descriptor */, 
	       int nkill	/* number of slots to remove data */)
{
     return 0;	/* failure - to be implemented */
}


/*
 * Set information about the open ring, passing data back by reference.
 * The nread and nunread values relate to the current slot position
 * in the context of the entire ring; if you jumpped backwards, nunread
 * will increase and nread will decrease.
 * Returns 1 for success or 0 for failure.
 */
int   rs_tell(RS ring			/* ring descriptor */, 
	      char **ret_ringname	/* RET: ring name */,
	      char **ret_filename	/* RET: file name */,
	      int *ret_nrings		/* RET: number of rings in file */, 
	      int *ret_nslots		/* RET: number or slots in ring */, 
	      int *ret_nread		/* RET: number of read slots */, 
	      int *ret_nunread		/* RET: number of unread slots */, 
	      char **ret_description	/* RET: text description of ring */)
{
     return 0;	/* failure - to be implemented */
}


/*
 * Return details of the rings in the file. Details are held in a table
 * with columns including name, nslots, description, 
 * Returns NULL for an error.
 */
TABLE rs_lsrings(char *filename	/* file name of ringstore */)
{
     return NULL;	/* failure - to be implemented */
}


/*
 * Return the file name of the currently opened ringstore.
 * Returns NULL for error.
 */
char *rs_filename(RS ring	/* ring descriptor */)
{
     return NULL;	/* failure - to be implemented */
}


/*
 * Return the ring name of the currently opened ringstore.
 * Returns NULL for error.
 */
char *rs_ringname(RS ring	/* ring descriptor */)
{
     return NULL;	/* failure - to be implemented */
}


/*
 * Return the amount of space taken up in storing this ringstore file
 * in bytes. Returns 0 for error.
 */
int   rs_footprint(RS ring	/* ring descriptor */)
{
     return 0;	/* failure - to be implemented */
}


/*
 * Returns the number of bytes left for the ring store to grow, inside 
 * its filesystem. Returns -1 for error.
 */
int   rs_remain(RS ring	/* ring descriptor */)
{
     return -1;	/* failure - to be implemented */
}


/*
 * Returns the most recent error code if there was a problem with
 * any of the ringstore calls
 */
int   rs_errno()
{
     return rs_errorno;
}


/*
 * Returns the string associacted with the error number.
 * The string is static/a constant and may not be freed.
 */
char *rs_errstr(int errno)
{
     return rs_errorstr[rs_errorno];
}



/* Internal helper method to handle mird errors, raising them to elog */
void rs_priv_mird_error(enum elog_severity sev, 
			int myerrno,
			char *filename, 
			char *ringname, 
			MIRD_RES mr)
{
     char *err_text;

     rs_errorno = myerrno;
     mird_describe_error(mr, &err_text);
     elog_printf(sev, "%s (%s,%s) %s", 
		 rs_errorstr[rs_errorno], filename, ringname, err_text);
     mird_free(err_text);
}


#if TEST
#define RSFILE1 "ringstore.1.rs"
#define RSRING1 "ring1"

int main() {
     RS rs1;

     route_init(NULL, 0);
     elog_init(0, "holstore test", NULL);
     rs_init();

     /* clear up before we start */
     unlink(RSFILE1);

     /* open and shut */
     rs1 = rs_open(RSFILE1, 0644, RSRING1, "Initial test ring", 5, RS_CREATE);
     if (!rs1)
	  elog_die(FATAL, "", 0, "[1] Can't create ringstore");
     rs_close(rs1);
     rs1 = rs_open(RSFILE1, 0644, RSRING1, "Initial test ring", 5, RS_CREATE);
     if (!rs1)
	  elog_die(FATAL, "", 0, "[1] Can't open ringstore");
     rs_close(rs1);

     rs_fini();
}

#endif /* TEST */
