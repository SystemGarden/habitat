/*
 * Timeseries storage on top of holstore
 * Nigel Stuckey, January/February 1998
 * Based on original timeseries API of 1996-7
 *
 * Copyright System Garden Limitied 1996-2001, All rights reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nmalloc.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* Warning: dependency problem! We rely on route to log our error messgaes
 * which leads to a problem; see route.h for an explanation. Thus we want
 * to declare our include first. */ 
#include "holstore.h"
#include "timestore.h"
#include "route.h"
#include "elog.h"
#include "util.h"
#include "table.h"

/* Initialise table heading */
char *ts_mget_schema = "_seq\t_time\tvalue\n"
"sequence number\ttime when entry was stored\tvalue\tinfo\n"
"int\ttime_t\tstr\ttype\n"
"abs\tabs\tabs\tsense";

/* Initialise timestore class */
void ts_init()
{
     hol_init(0, 0);
}

/* Finalise timestore class */
void ts_fini()
{
     hol_fini();
}

/*
 * Open time series storage
 * Given the name of a holstore, attempt to open a timeseries ring inside it. 
 * Returns a reference to the ring (TS_RING) if successful, or NULL if 
 * the ring does not exist or there was some other failure.
 * ts_open() opens storage that should be closed with ts_close(). 
*/
TS_RING ts_open(char *holname,
		char *ringname, 
		char *password )
{
     struct ts_superblock *super;
     struct ts_ring *ring;
     HOLD h;

     /* open holstore and keep open until ts_close */
     h = hol_open(holname);
     if (!h) {
	  /* unable to open holstore */
          elog_printf(DEBUG, "unable to open holstore: %s", holname);
	  return NULL;
     }

     hol_begintrans(h, 'r');

     /* Create memory copy of ring if it exists and fill it */
     if ((super = ts_inreadsuper(h)) == NULL) {
	  /* Unable to open timestore */
	  hol_rollback(h);
	  hol_close(h);
	  elog_printf(DEBUG, "unable to open timestore in %s", holname);
	  return NULL;
     }

     /* read ring from disk and obtain a TS_RING structure */
     if ( (ring = ts_inreadring(h, super, ringname, password)) == NULL ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(h);
	  hol_close(h);
	  ts_infreesuper(super);
	  elog_printf(DEBUG, "unable to read timestore ring: %s,%s", holname, 
		      ringname);
	  return NULL;
     }

     hol_commit(h);
     return(ring);
}

/* Close the ring */
void ts_close(TS_RING ring) {
     if (!ring)
	  return;

     hol_close(ring->hol);
     ts_infreesuper(ring->super);
     ts_infreering(ring);
}

/*
 * Create a ring in a holstore.
 * Returns a reference to the ring created if successful, or NULL if
 * the ring already exists or there was an error of some sort.
 * Will create a timestore superblock inside the holstore.
 * ts_open() opens storage that should be closed with ts_close().
 */
TS_RING ts_create(char *holname, 
		  int mode,
		  char *ringname, 
		  char *description, 
		  char *password, 
		  int nslots )
{
     struct ts_superblock *super;
     struct ts_ring *ring;
     HOLD h;

     /* open holstore, then create if it does not exist */
     h = hol_open(holname);
     if (!h) {
	  /* unable to open holstore, try creating */
          h = hol_create(holname, mode);
	  if (!h) {
	       /* unable to open holstore */
	       elog_send(ERROR, 
			 "Unable to open holstore to create timestore");
	       return NULL;
	  }
     }

     hol_begintrans(h, 'w');

     /* Get a memory copy of the superblock */
     if ((super = ts_increatesuper(h)) == NULL) {
	  /* Unable to open timestore */
	  hol_rollback(h);
	  hol_close(h);
          elog_send(ERROR, "unable to create timestore");
	  return NULL;
     }

     /* Create the ring and obtain a TS_RING structure */
     if ( (ring = ts_increatering(h, super, ringname, description, password, 
				  nslots)) == NULL ) {
	  /* Unable to create new ring */
	  hol_rollback(h);
	  hol_close(h);
	  ts_infreesuper(super);
          elog_send(DEBUG, 
		    "unable to create ring in timestore");
	  return NULL;
     }

     /* increment ring count - as we have write lock since it was loaded, 
      * the superblock will be up to date */
     ring->super->nrings++;
     ts_inwritesuper(ring->hol, ring->super);

     hol_commit(h);
     return(ring);
}

/*
 * Remove the currently open ring.
 * This call implys a close, and will invalidate the passed ring.
 * Dont try to use it after this call.
 * Return 1 for success or 0 for failure
 */
int ts_rm(TS_RING ring)
{
     char ringhead[TS_MIDSTRLEN] /*, ringdatum[TS_MIDSTRLEN]*/ ;
     int r, i;

     if (!ring)
	  return 0;

     /* prepare header name */
     r = snprintf(ringhead, TS_MIDSTRLEN, "%s%s", TS_RINGSPACE, ring->name);
     if (r >= TS_MIDSTRLEN) {
	  elog_send(ERROR, "name too long for header");
	  return 0;
     }

     hol_begintrans(ring->hol, 'w');

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  return 0;
     }

     /*
      * remove all ring data by fetching the ring header and iterating
      * deletion from the oldest to the youngest
      */
     for (i = ring->oldest; i <= ring->youngest; i++) {
	  if (i == -1)		/* Empty ring */
	       break;

	  if ( ! ts_inrmdatum(ring, "ts_rm()", i) ) {
	       hol_rollback(ring->hol);
	       return 0;
	  }
     }

     /* remove ring header within a transaction */
     if ( ! hol_rm(ring->hol, ringhead) ) {
          elog_send(DEBUG, "ring does not exist");
	  return 0;
     }

     /* Update superblock with reduced number of rings */
     ts_infreesuper(ring->super);
     ring->super = ts_inreadsuper(ring->hol);
     ring->super->nrings--;
     ts_inwritesuper(ring->hol, ring->super);

     hol_commit(ring->hol);

     /* finish with db and this now invalid ring handle */
     hol_close(ring->hol);
     ts_infreesuper(ring->super);
     ts_infreering(ring);

     return 1;
}


/*
 * Put a block of data on the end of a valid ring.
 * If the ring has a finite size and has reached its maximum, the oldest
 * datum will be destructively removed before writing the new one.
 * The data may be binary and thus not be null terminated, but its length
 * is returned.
 * Returns sequence number for success or -1 for failure. 
 * If failed, the ring will remain open and the handle will still be valid.
 */
int ts_put(TS_RING ring, 	/* open ring */
	   void *block,		/* data block, \0 terminating not needed */
	   int length 		/* length of data */ )
{
     return ts_put_withtime(ring, block, length, time(NULL));
}

/*
 * As ts_put(), but takes a specified insertion time for the datum block
 */
int   ts_put_withtime(TS_RING ring, 	/* open ring */
		      void *block,	/* data block */
		      int length, 	/* length of data */ 
		      time_t instime	/* insertion time */ )
{
     char *bigblock;

     hol_begintrans(ring->hol, 'w');

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  return -1;
     }

     /* the smallest ring is 1, therefore we always save the new one */
     ring->youngest++;

     if (ring->oldest == -1) {
	  /* Empty ring, set the pointer correctly */
	  ring->youngest = ring->oldest = 0;
     } else {
	  /* if finite ring & we have reached the list length */
	  if (ring->nslots && (ring->oldest <= ring->youngest - ring->nslots))
	  {
	       /* delete the oldest datum */
	       if ( ! ts_inrmdatum(ring, "ts_put()", ring->oldest) ) {
		    hol_rollback(ring->hol);
		    return -1;
	       }
	       ring->oldest++;
	  }
     }

     /* write new datum and timestamp.
      * To do this, we nmalloc() more memory to fit the datum and
      * time in one contiguous block. Time will go at end of the
      * block, so memory may be released in a single nfree() when
      * returned by ts_get(). We get time and copy it to get round
      * memory alignment issues
      */

     bigblock = nmalloc(length + sizeof(time_t));
     if ( ! bigblock) {
	  elog_send(ERROR, "unable to nmalloc() enough");
	  hol_rollback(ring->hol);
	  return -1;
     }
     memcpy(bigblock, block, length);
     memcpy(bigblock + length, &instime, sizeof(time_t));
     /*time( (time_t *) bigblock+length );*/
     if ( ! ts_inwritedatum(ring, "ts_put()", ring->youngest, bigblock,
			     length+sizeof(time_t))) {
	  hol_rollback(ring->hol);
	  nfree(bigblock);
	  return -1;
     }

     /* save ring header */
     if ( ! ts_inwritering(ring) ) {
	  hol_rollback(ring->hol);
	  nfree(bigblock);
	  return -1;
     }

     hol_commit(ring->hol);
     nfree(bigblock);
     return ring->youngest;		/* success */
}


#if 0

WRITTEN BUT NOT YET USED OR TESTED

/*
 * Multiple put. Appends the blocks itemised in list on to a valid ring.
 * As with ts_put(), The oldest data will be removed from the list if
 * the maximum is reached in a finite ring.
 * The data should be specified in an ordered list, using an ITREE
 * the data pointing to an ntsbuf structure. The ITREE index is not used
 * apart from its ordering function, carried out by the user.
 * The ntsbuf should contain buffer pointer, data length and insertion time
 * (buffer, len, instime) all set by the user.
 * The sequence number is generated automatically from the ordering of
 * the ITREE and the `seq' field is ignored.
 * Returns the first sequence number inserted; the last may be obtained 
 * ts_youngest(). An error will cause -1 to be returned.
 */
int   ts_mput(TS_RING ring,	/* open ring */
	      ITREE *list	/* list of data */)
{
     char datumname[TS_MIDSTRLEN];
     char *bigblock;
     time_t now;
     int ndata, i, skip, seq, firstseq;
     ntsbuf *nts;

     /* count */
     ndata = itree_n(list);

     hol_begintrans(ring->hol, 'w');

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  return -1;
     }

     /* quart into pint pot? */
     if (ring->nslots && ndata > ring->nslots) {
	  /* the data will not fit into the ring */
	  /* purge all existing data */
	  while (ring->oldest < ring->youngest) {
	       if ( ! ts_inrmdatum(ring, "", ring->oldest) ) {
		    hol_rollback(ring->hol);
		    return -1;
	       }
	       ring->oldest++;
	  }

	  /* work out the number of data to skip */
	  skip = ndata - ring->nslots;	/* data to skip */
	  ring->oldest += skip;		/* adjust oldest seq */
	  ring->youngest += ndata;	/* adjust youngest seq */
	  seq = ring->oldest;		/* set sequence counter */
     } else {
	  ring->youngest += ndata;	/* adjust youngest seq */

	  /* empty ring? */
	  if (ring->oldest == -1)
	       ring->oldest = 0;	/* set the oldest pointer */

	  /* if finite ring & we have reached the list length */
	  while ( ring->nslots && 
		  (ring->oldest <= ring->youngest - ring->nslots) )
	  {
	       /* delete the oldest datum */
	       if ( ! ts_inrmdatum(ring, "ts_mput()", ring->oldest) ) {
		    hol_rollback(ring->hol);
		    return -1;
	       }
	       ring->oldest++;
	  }
	  seq = ring->oldest;		/* set sequence counter */
     }

     /* iterate over list and write */
     firstseq = seq;
     itree_first(list);
     for (i=0; i<ndata; i++) {
	  /* skip first data if unable to fit */
	  if (skip) {
	       skip--;
	       continue;
	  }

	  nts = itree_get(list);

	  /* write new data and timestamps.
	   * To do this, we nmalloc() more memory to fit the datum and
	   * time in one contiguous block. Time will go at end of the
	   * block, so memory may be released in a single nfree() when
	   * returned by ts_get(). We get time and copy it to get round
	   * memory alignment issues
	   */
	  bigblock = xnmemdup(nts->buffer, nts->len + sizeof(time_t));
	  memcpy(bigblock + nts->len, &nts->instime, sizeof(time_t));
	  /*time( (time_t *) bigblock+length );*/
	  if ( ! ts_inwritedatum(ring, "", seq, bigblock, 
				 nts->len + sizeof(time_t))) {
	       hol_rollback(ring->hol);
	       nfree(bigblock);
	       return -1;
	  }

	  nfree(bigblock);
	  seq++;
     }

     /* save ring header */
     if ( ! ts_inwritering(ring) ) {
	  hol_rollback(ring->hol);
	  return -1;
     }

     hol_commit(ring->hol);
     return firstseq;		/* success */
}
#endif



/*
 * Get the oldest datum from a ring that we have not already encountered.
 * When a ring is first opened, the reader is set to the oldest record and
 * every time ts_get() is called, we advance to the next one. Eventually, 
 * we could reach the end, when a NULL would be returned; calling ts_get()
 * would continue to return NULL until a new datum was put in the end of 
 * the ring.
 * If the ring is finite, it is possible to loose data. This happens when 
 * data is removed from the ring faster that the ability of the reader
 * to run ts_get(). As a sanity check and to help in other efficiency 
 * measures, this call returns the internal sequence number of the 
 * returned datum. The sequencies start from 0 and are contiguous.
 * Returns the next unread datum as nmalloc()ed data (which you should 
 * free after use) or NULL if there is no more data to read.
 * NOTE: that the data returned is a buffer and is NOT null terminated; 
 * however, the buffer returned is always allocated as len+sizeof(int) bytes,
 * enough room to stick in a null at the end if strings are being used.
 */
void *ts_get(TS_RING ring, 		/* open ring */
	     int *retlength,		/* Returned datum/block length */
	     time_t *retinstime,	/* Returned insertion time */
	     int *retseq		/* Returned sequence number */ )
{
     char *bigblock;
     int len, seq;
     time_t then;

     hol_begintrans(ring->hol, 'r');

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unble to read ring header");
	  return NULL;
     }

     /* work out the sequence we want off the holstore */
     if (ring->youngest == -1) {
	  hol_rollback(ring->hol);
	  return NULL;			/* empty ring */
     }
     seq = ring->lastread+1;
     if (seq < ring->oldest)
	  seq = ring->oldest;
     if (seq > ring->youngest)
	  seq = ring->youngest;		/* btw this was a stupid value */

     if (seq == ring->lastread) {
	  hol_commit(ring->hol);
	  return NULL;			/* no available values */
     }

     /* Read that sequence.
      * The block includes the time stamp at the end, so that it can be
      * cleaned up with a single call to nfree() by the caller when 
      * they are finished with the value. */
     bigblock = ts_inreaddatum(ring, "ts_get()", seq, &len);
     if ( ! bigblock ) {
	  hol_rollback(ring->hol);
	  return NULL;
     }
     ring->lastread = seq;
     *retlength = len - sizeof(time_t);
     *retseq = seq;
     memcpy(&then, bigblock + len - sizeof(time_t), sizeof(time_t));
     *retinstime = then;

     hol_commit(ring->hol);
     return bigblock;		/* success */
}

/*
 * Multiple get.
 * Like ts_get() above, in that it returns the oldest unread data, but
 * where ts_get() returns a single datum, ts_mget() will return as many
 * data records as asked for.
 * The user specifies the quantity required in `want' and the number 
 * available is returned, which will be between 0 and `want'.
 * The data is returned in an ntsbuf structure and collected by ITREE 
 * structure, which will be indexed by sequence.
 * Note that the item `datatext' is unused by timestore and will  not be set.
 * Please use ts_mgetfree() to release all storage returned to you.
 * Returns the number of samples actually obtained (and returned in arrays)
 * if successful or -1 if there was a failure.
 */
int ts_mget(TS_RING ring,	/* Opened ring */
	    int want,		/* Number of data records wanted */
	    ITREE **retlist	/* Data returned as list of ntsbuf */ )
{
     int startseq, endseq, i, numseq;
     ntsbuf *dat;
     ITREE *list;

     /* check arguments */
     if (ring == NULL)
	  elog_die(FATAL, "called with ring == NULL");
     if (retlist == NULL)
	  elog_die(FATAL, "called with retlist == NULL");
     if (want <= 0) {
	  *retlist = NULL;
	  return 0;
     }

     hol_begintrans(ring->hol, 'r');

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  return -1;
     }

     /* work out the first unseen sequence we want off the holstore */
     if (ring->youngest == -1) {
	  hol_rollback(ring->hol);
	  return 0;			/* empty ring */
     }
     startseq = ring->lastread+1;
     if (startseq < ring->oldest)
	  startseq = ring->oldest;
     if (startseq > ring->youngest)
	  startseq = ring->youngest;	/* btw this was a stupid value */

     if (startseq == ring->lastread) {
	  hol_commit(ring->hol);
	  return 0;			/* no available values */
     }

     /* work out last sequence */
     endseq = startseq+want-1;		/* endseq will actually point to
					 * the final sequence */
     if (endseq > ring->youngest)
	  endseq = ring->youngest;

     /* allocate data in one chunk */
     numseq = endseq-startseq+1;
     dat = xnmalloc(numseq * sizeof(ntsbuf));
     list = itree_create();
     if ( ! list) {
          elog_send(ERROR, "unable to create list");
	  return -1;
     }

     /* Read the sequence.
      * The block includes the time stamp at the end, so that it can be
      * cleaned up with a single call to nfree() by the caller when 
      * they are finished with the value. */
     for (i=0; i < numseq; i++) {
	  dat[i].seq    = startseq + i;
	  dat[i].buffer = ts_inreaddatum(ring, "ts_mget()", dat[i].seq, 
					  &dat[i].len);
	  if ( ! dat[i].buffer)
	       continue;
	  dat[i].len   -= sizeof(time_t);
	  memcpy(&dat[i].instime, dat[i].buffer + dat[i].len, sizeof(time_t));
	  itree_add(list, dat[i].seq, &dat[i]);
     }

     /* finish reading */
     ring->lastread = endseq;
     hol_commit(ring->hol);

     /* return the values */
     *retlist = list;
     return numseq;
}


/* Free the data allocated by ts_mget() */
void ts_mgetfree(ITREE *dat)
{
     ntsbuf *buf;

     /* free the buffers */
     itree_traverse(dat) {
	  buf = itree_get(dat);
	  nfree(buf->buffer);
     }

     /* free the ntsbuf, which was declared as an array and so should 
      *	be freed as a whole array */
     itree_first(dat);
     nfree(itree_get(dat));

     /* Now free the tree and its elements */
     itree_destroy(dat);
}


/* Free the data allocated by ts_mget() but leaving the data blocks */
void ts_mgetfree_leavedata(ITREE *dat)
{
     /* free the ntsbuf, which was declared as an array and so should 
      *	be freed as a whole array */
     itree_first(dat);
     nfree(itree_get(dat));

     /* Now free the tree and its elements */
     itree_destroy(dat);
}


/*
 * Multiple get returning a table.
 * Like ts_mget() above, but returns data in a TREE of columns, with the
 * key being the column name and value being an ITREE column list.
 * Each column list is an ordered column.
 * Please use table_deepdestroy() to release all storage returned to you.
 * Returns the table or NULL if there was a failure.
 */
TABLE ts_mget_t(TS_RING ring,	/* Opened ring */
		int want	/* Number of data records wanted */ )
{
     int startseq, endseq, i, j;
     TREE *row;
     void *buffer;
     int length;
     time_t instime;
     char seqstr[15], *seqcpy, timestr[12], *timecpy;
     TABLE rdata;
     char *schema;

     /* create return table */
     schema = xnstrdup(ts_mget_schema);
     rdata = table_create_s(schema);
     table_freeondestroy(rdata, schema);

     /* create insertion list */
     row = tree_create();
     tree_add(row, "_seq", NULL);
     tree_add(row, "_time", NULL);
     tree_add(row, "value", NULL);

     hol_begintrans(ring->hol, 'r');

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  table_destroy(rdata);
	  tree_destroy(row);
	  return NULL;
     }

     /* work out the first unseen sequence we want off the holstore */
     if (ring->youngest == -1) {
	  hol_rollback(ring->hol);
	  tree_destroy(row);
	  return rdata;			/* empty ring */
     }
     startseq = ring->lastread+1;
     if (startseq < ring->oldest)
	  startseq = ring->oldest;
     if (startseq > ring->youngest)
	  startseq = ring->youngest;	/* btw this was a stupid value */

     if (startseq == ring->lastread) {
	  hol_commit(ring->hol);
	  tree_destroy(row);
	  return rdata;			/* no available values */
     }

     /* work out last sequence */
     endseq = startseq+want-1;		/* endseq will actually point to
					 * the final sequence */
     if (endseq > ring->youngest)
	  endseq = ring->youngest;

     /* Read the sequence.
      * The block includes the time stamp at the end, so that it can be
      * cleaned up with a single call to nfree() by the caller when 
      * they are finished with the value. */
     for (i=startseq; i <= endseq; i++) {
          buffer = ts_inreaddatum(ring, "ts_mget_t()", i, &length);
	  if ( ! buffer )
	       continue;

	  /* sequence */
	  sprintf(seqstr, "%d", i);
	  seqcpy = xnstrdup(seqstr);
	  tree_find(row, "_seq");
	  tree_put(row, seqcpy);

	  /* time */
	  memcpy(&instime, buffer + length - sizeof(time_t), sizeof(time_t));
	  sprintf(timestr, "%lu", instime);
	  timecpy = xnstrdup(timestr);
	  tree_find(row, "_time");
	  tree_put(row, timecpy);

	  /* value: patch time part of buffer with a NULL and premature 
	   * \0 with \n so the whole value is displayed (especially for
	   * versionstore rings) */
#if 0
	  /* value (remove trailing null in data) */
	  bufstr = xnmalloc( length + 1024 );
	  strcstr(bufstr, length+1024, buffer, length-sizeof(time_t)-1);
#endif
#if 0
	  bufstr = util_bintostr(length+1024, buffer, length-sizeof(time_t)-1);
#endif
	  for (j=0; j<length; j++)
	       if ( ((char *) buffer)[j] == '\0')
		    ((char *) buffer)[j] = '\n';
	  * (char *) (buffer + length - sizeof(time_t)) = '\0';
	  tree_find(row, "value");
	  tree_put(row, buffer);

	  /* add row and clearup for next iteration */
	  table_addrow_noalloc(rdata, row);
	  table_freeondestroy(rdata, seqcpy);
	  table_freeondestroy(rdata, timecpy);
	  table_freeondestroy(rdata, buffer);
     }

     /* finish reading */
     tree_destroy(row);
     ring->lastread = endseq;
     hol_commit(ring->hol);

     /* return the values */
     return rdata;
}


/*
 * Replace the current datum, while not disturbing its position in the 
 * list, dictated by the sequence number, or the insertion date.
 * In the current format, a modification date is not maintained.
 * This function would normally be proceeded by ts_get(), in which case
 * you run the following: ts_get(); ts_jump(-1); ts_replace(), as
 * ts_get() advances the current datum.
 * Returns the sequence replaced or -1 if there was a problem.
 */
int ts_replace(TS_RING ring, 	/* open ring */
	       void *block,	/* data block, \0 terminating not needed */
	       int length 	/* length of data */ )
{
     char *bigblock, *newbigblock;
     int seq, len;

     hol_begintrans(ring->hol, 'w');

     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  return -1;
     }

     /* check the sequence is still available */
     if (ring->youngest == -1) {
	  hol_rollback(ring->hol);
          elog_printf(ERROR, "unable to replace anything in an empty ring");
	  return -1;			/* empty ring */
     }

     seq = ring->lastread+1;
     if (seq < ring->oldest || seq > ring->youngest) {
          hol_rollback(ring->hol);
          elog_printf(ERROR, "element %d is not in ring", 
		      seq);
	  return -1;			/* missing ring */

     }

     /* Read that sequence.
      * The block includes the time stamp at the end, so that it can be
      * cleaned up with a single call to nfree() by the caller when 
      * they are finished with the value. */
     bigblock = ts_inreaddatum(ring, "ts_get()", seq, &len);
     if ( ! bigblock ) {
	  hol_rollback(ring->hol);
          elog_printf(ERROR, "element %d not available to replace", seq);
	  return -1;
     }

     /* allocate new space and copy previous time over */
     newbigblock = xnmalloc(length + sizeof(time_t));
     memcpy(newbigblock, block, length);
     memcpy(newbigblock + length, bigblock + len - sizeof(time_t), 
	    sizeof(time_t));
     nfree(bigblock);

     /* write new datum and old timestamp */
     if ( ! ts_inwritedatum(ring, "ts_replace()", seq, newbigblock,
			     length+sizeof(time_t)) ) {
	  hol_rollback(ring->hol);
	  nfree(newbigblock);
	  return -1;
     }

     /* flush changes, update handle, clean up and return */
     hol_commit(ring->hol);
     nfree(newbigblock);
     ring->lastread = seq;
     return seq;
}


/*
 * Returns the sequence number of the last datum read.
 * If the ring is empty or nothing has been read, returns -1.
 */
int ts_lastread(TS_RING ring)
{
     return (ring->lastread);
}

/*
 * Returns the sequence number of the oldest datum sequence available
 * in the ring. If there are no entries in ring, returns -1.
 */
int ts_oldest(TS_RING ring)
{
     return (ring->oldest);
}

/*
 * Returns the sequence number of the youngest datum sequence available
 * in the ring. If there are no entries in ring, returns -1.
 */
int ts_youngest(TS_RING ring)
{
     return (ring->youngest);
}


/*
 * Change the next datum to be read by giving adding a relative quantity
 * to the current context's sequence number. If the value is +ve, you will
 * jump over unread data; if -ve you will jump back to read previous data.
 * A jump value of 0 will have no affect. If you jump beyond the youngest
 * available data, you will treated as though you have read all the data
 * and ts_get()s will yield nothing until more data is available.
 * If you jump back to before the oldest datum, the next ts_get() will 
 * return the oldest available datum. 
 * Returns the number of data actually jumped. If the list is empty, 
 * the jump will be ignored and a jump of 0 will be returned.
 * Warning: does not update from disk and consequently is a fast operation.
 * ts_jumpyoungest() and ts_jumpoldest() should be used instead.
 */
int ts_jump(TS_RING ring,	/* Current ring context */
	    int jump		/* relative amount to move */ )
{
     int from;

     if (ring->youngest == -1)
	  return 0;

     from = ring->lastread;
     ring->lastread += jump;

     if (ring->lastread > ring->youngest)
	  ring->lastread = ring->youngest;
     if (ring->lastread < ring->oldest-1)
	  ring->lastread = ring->oldest-1;

     return (ring->lastread - from);
}


/* 
 * Jump past youngest datum in the ring and checks the disk to ensure the
 * correct ring values.
 * Returns the number of data jumped
 */

int ts_jumpyoungest(TS_RING ring)
{
     int diff;

     /* update ring header in memory */
     hol_begintrans(ring->hol, 'r');
     if ( ! ts_inupdatering(ring) )
	  /* ring does not exist or password mismatch */
	  elog_printf(ERROR, "unable to read ring header %s,%s", 
		      ring->hol->name, ring->name);
     hol_endtrans(ring->hol);

     /* if unable to refresh for some reason, continue with the in memory 
      * copy */
     diff = ring->youngest - ring->lastread;
     ring->lastread = ring->youngest;

     return diff;
}


/* 
 * Jump to before the oldest datum in the ring
 * Returns the number of data jumped
 */
int ts_jumpoldest(TS_RING ring)
{
     int diff;

     /* update ring header in memory */
     hol_begintrans(ring->hol, 'r');
     if ( ! ts_inupdatering(ring) )
	  /* ring does not exist or password mismatch */
	  elog_send(ERROR, "unable to read ring header");
     hol_endtrans(ring->hol);

     /* if unable to refresh for some reason, continue with the in memory 
      * copy */
     diff = ring->oldest - ring->lastread -1;
     ring->lastread = ring->oldest-1;

     return diff;
}


/*
 * Jump to the specified poisition in the ring.
 * The value given, if within the bounds of the ring, will be treated
 * as the last sequence read by the user. ts_get() will give you the next
 * datum, if it exists. If you jump to data older than held, the oldest will 
 * be returned; if younger than held, you will be set to the youngest and 
 * nothing will be returned until a new datum is stored.
 * Returns the number of data moved as a relative amount
 * Warning: does not update from disk and consequently is a fast operation.
 * ts_jumpyoungest() and ts_jumpoldest() should be used instead.
 */
int ts_setjump(TS_RING ring, int setjump)
{
     int from;

     if (ring->youngest == -1)
	  return 0;

     from = ring->lastread;
     ring->lastread = setjump;

     if (ring->lastread > ring->youngest)
	  ring->lastread = ring->youngest;
     if (ring->lastread < ring->oldest-1)
	  ring->lastread = ring->oldest-1;

     return (ring->lastread - from);
}


/*
 * Preallocate the same amount of space for unallocated elements in the ring.
 * Depending on the implementation of holstore, it may guarantee the success
 * of future ts_put()s if space is an issue and improve allocation speed.
 * On unbounded rings (with no fixed size), no work will be done and the
 * call is considered successful.
 * Returns 1 if successful, 0 otherwise.
 */
int ts_prealloc(TS_RING ring, int size)
{
     char *dataspace;
     int i, limit;

     dataspace = xnmalloc(size + sizeof(time_t));

     hol_begintrans(ring->hol, 'w');

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  nfree(dataspace);
	  return 0;
     }

     /* Write blank space for each unallocated dataum */
     limit = ring->oldest+ring->nslots;
     if (ring->oldest == -1)
	  limit++;
     for (i = ring->youngest+1; i < limit; i++) {
	  if ( ! ts_inwritedatum(ring, "ts_prealloc()", i, dataspace,
				  size+sizeof(time_t))) {
	       hol_rollback(ring->hol);
	       nfree(dataspace);
	       return 0;
	  }
     }

     /* save ring header */
     if ( ! ts_inwritering(ring) ) {
	  hol_rollback(ring->hol);
	  nfree(dataspace);
	  return 0;
     }

     hol_commit(ring->hol);
     nfree(dataspace);
     return 1;		/* success */
}


/*
 * Resize the ring to the number of elements specified.
 * If extending, slots are added at the youngest end of the ring to
 * take new data. If reducing, slots and data are removed from the oldest
 * part of the ring; the data will be unrecoverably lost.
 * Setting size to 0 will unbound the ring, removing any limit previously set.
 * Returns 1 for success or 0 for failure
 */
int ts_resize(TS_RING ring, int size)
{
     int i, newold;

     if (size < 0)
	  return 0;

     hol_begintrans(ring->hol, 'w');

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  return 0;
     }

     /* no change */
     if (size == ring->nslots) {
	  hol_rollback(ring->hol);
	  return 1;	/* no work, but still success */
     }

     /* increase in size, unbounded ring or empty */
     if (size > ring->nslots || size == 0 || ring->youngest == -1)
	  ring->nslots = size;

     /* size reduction */
     if (size < ring->nslots) {
	  /* work out the new boundaries, keeping as much data as possible */
	  newold = ring->youngest - size +1;
	  if (newold < ring->oldest)
	       newold = ring->oldest;

	  /* remove oldest data */
	  for (i=ring->oldest; i < newold; i++)
	       if ( ! ts_inrmdatum(ring, "ts_resize()", i) ) {
		    hol_rollback(ring->hol);
		    return 0;
	       }

	  ring->oldest = newold;
	  ring->nslots = size;
     }

     /* save ring header */
     if ( ! ts_inwritering(ring) ) {
	  hol_rollback(ring->hol);
	  return 0;
     }

     hol_commit(ring->hol);
     return 1;		/* success */
}


/*
 * Return some statistics about the ring. This will compare the current
 * state of the ring on disk with that held in memory.
 * Please free the storage from ret_description after use.
 * Returns 1 for success or 0 for failure 
 */
int ts_tell(TS_RING ring,		/* ring */
	    int *ret_nrings,		/* number of rings in timestore */
	    int *ret_nslots, 		/* number of slots */
	    int *ret_nread,		/* amount of data read */
	    int *ret_navailable,	/* allocated data available */
	    char **ret_description	/* ring description */ )
{
     if (hol_begintrans(ring->hol, 'r') == 0)
	  return 0;	/* lock not granted */

     /* update superblock in memory (we need nrings) */
     ts_infreesuper(ring->super);
     ring->super = ts_inreadsuper(ring->hol);

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  return 0;
     }

     /* assign return values */
     *ret_nrings = ring->super->nrings;
     *ret_nslots = ring->nslots;
     if (ring->oldest == -1) {
	  *ret_nread = *ret_navailable = 0;
     } else if (ring->lastread == -1) {
	  *ret_nread = 0;
	  *ret_navailable = ring->youngest - ring->oldest +1;
     } else {
	  *ret_nread = ring->lastread - ring->oldest +1;
	  *ret_navailable = ring->youngest - ring->lastread;
     }

     *ret_description = xnstrdup(ring->description);

     hol_commit(ring->hol);

     return 1;
}


/*
 * Return a list of rings contained in a TREE. The keys of the TREE are
 * the names of the rings, the data associacted with each key is the
 * actual ring definition data.
 * To return a list of all rings, ringpat should be "" (not NULL).
 * If a specific subset of rings is required, ringpat should contain
 * the regular expression of the namespace required.
 * Each key, data and the tree that contains them should be deleted;
 * A routine is provided to do that: ts_freelsrings().
 * The routine below works on HOLD handles and a macro is declared that
 * works with TS_RING types.
 * NULL is returned as error, other wise a tree is returned (which may 
 * be empty)
 */
TREE *ts_lsringshol(HOLD hol, char *ringpat)
{
     int tspatlen, nslen;
     TREE *rec;
     char tspattern[TS_MIDSTRLEN];

     if (ringpat == NULL)
	  elog_die(FATAL, "ringpat is NULL");

     /* prepare pattern name */
     tspatlen = snprintf(tspattern,TS_MIDSTRLEN, TS_RINGREMATCH, TS_RINGSPACE,
			 ringpat);
     if (tspatlen == TS_MIDSTRLEN) {
          elog_send(ERROR, "pattern too long");
	  return NULL;
     }

     /* search the holstore for matching TS superblocks */
     hol_begintrans(hol, 'r');
     rec = hol_search(hol, tspattern, NULL);
     hol_rollback(hol);

     /* strip the namespace off the records returned */
     nslen = strlen(TS_RINGSPACE);
     tree_traverse(rec)
          util_strdel(tree_getkey(rec), nslen);

     return rec;
}


/* 
 * purge all entries upto and including a specified sequence
 * Returns 1 if successful or 0 for failure
 */
int ts_purge(TS_RING ring,	/* ring */
	     int kill		/* delete entries <= kill */ )
{
     int i;

     hol_begintrans(ring->hol, 'w');

     /* update ring header in memory */
     if ( ! ts_inupdatering(ring) ) {
	  /* ring does not exist or password mismatch */
	  hol_rollback(ring->hol);
	  elog_send(DEBUG, "unable to read ring header");
	  return 0;
     }

     /* check killbefore is sensible */
     if (kill < ring->oldest || kill > ring->youngest) {
	  hol_rollback(ring->hol);
	  return 0;
     }

     /* remove purged data */
     for (i=ring->oldest; i <= kill; i++)
	  if ( ! ts_inrmdatum(ring, "ts_purge()", i) ) {
	       hol_rollback(ring->hol);
	       return 0;
	  }

     /* update header */
     ring->oldest = kill+1;
     if (ring->lastread < ring->oldest)
	  ring->lastread = -1;

     /* save ring header */
     if ( ! ts_inwritering(ring) ) {
	  hol_rollback(ring->hol);
	  return 0;
     }

     hol_commit(ring->hol);
     return 1;		/* success */
}


/* -------------------- Private routines  -------------------- */

/* 
 * Create a superblock on disk or read it if one is already there. 
 * Returns a superblock structure on success or NULL if unable to create 
 * the superblock. The superblock should be freed with ts_infreesuper().
 */
struct ts_superblock *ts_increatesuper(HOLD h)
{
     struct ts_superblock *sb;

     if ((sb = ts_inreadsuper(h)))
	  /* Superblock already exists */
	  return sb;

     /* create space for superblock */
     sb = nmalloc(sizeof(struct ts_superblock));
     if (!sb)
	  return NULL;

     /* assign initial values */
     sb->magic = TS_MAGICNUMBER;
     sb->version = TS_VERSIONNUMBER;
     sb->nrings = 0;
     sb->nalias = 0;
     sb->alias = NULL;

     /* Save to disk */
     if (ts_inwritesuper(h, sb))
	  return sb;
     else {
	  ts_infreesuper(sb);
	  return NULL;
     }
}

/*
 * read superlock from holstore into memory.
 * the returned superblock structure should freed with ts_infreesuper()
 */
struct ts_superblock *ts_inreadsuper(HOLD h)
{
     int sblen, r;
     char *sbtxt;
     struct ts_superblock *sb;

     /* get superblock text off holstore */
     sbtxt = hol_get(h, TS_SUPERNAME, &sblen);
     if (!sbtxt)
	  return NULL;

     /* parse fixed part of superblock text */
     sb = nmalloc(sizeof(struct ts_superblock));
     if (!sb) {
	  nfree(sbtxt);
	  return NULL;
     }
     r = sscanf(sbtxt, "%d %d %d %d", &sb->magic, &sb->version, &sb->nrings, 
	    &sb->nalias);
     nfree(sbtxt);
     if (r < 4) {
          elog_printf(ERROR, "superblock corrupted: %s", sbtxt);
	  return NULL;
     }

     /* get variable length part of superblock */
     /* NOT YET IMPLEMENTED */
     sb->alias = NULL;

     /* Check the magic number and version */
     if (sb->magic != TS_MAGICNUMBER) {
	  elog_printf(ERROR, "wrong magic number, found %d want %d", 
		      sb->magic, TS_MAGICNUMBER);
	  return NULL;
     }
     if (sb->version != TS_VERSIONNUMBER) {
	  elog_printf(ERROR, "wrong version, found %d want %d", sb->version, 
		      TS_VERSIONNUMBER);
	  return NULL;
     }

     return sb;
}

/* 
 * Write the superblock from memory to disk, but does not free the 
 * superblock structure.
 * Returns 1 for success, 0 for failure
 */
int ts_inwritesuper(HOLD h, struct ts_superblock *sb)
{
     int r, sblen;
     char sbtxt[TS_MAXSUPERLEN];

     /* create a textual representation of the superblock */
     sblen = snprintf(sbtxt, TS_MAXSUPERLEN, "%d %d %d %d", sb->magic, 
		      sb->version, sb->nrings, sb->nalias);
     if (sblen == TS_MAXSUPERLEN) {
          elog_send(ERROR, "internal overflow");
	  return 0;
     }
     /* RING ALIASES NOT YET IMPLEMENTED */

     /* write the superblock to holstore with \0 */
     r = hol_put(h, TS_SUPERNAME, sbtxt, sblen+1);

     return r;
}

/* Free the in-memory representation of the superblock */
void ts_infreesuper(struct ts_superblock *sb)
{
     nfree(sb);
}

/*
 * Create a new ring in the holstore, storing details in the provided
 * ring structure. Checks to see if a ring of the same name already exists.
 * Additional memory allocations are made and assigned to the passed 
 * structure. The TS_RING structure should be freed with ts_infreering().
 * Returns a TS_RING handle is successful, or NULL for a failure.
 */
TS_RING ts_increatering(HOLD hol,		/* holstore id */
			struct ts_superblock *super, /* superblock pointer */
			char *ringname,		/* name of ring */
			char *description,	/* string description */
			char *password,		/* password string; 
						 * NULL pointer for none */
			int nslots		/* number of slots in ring */ )
{
     char ringrec[TS_MIDSTRLEN], *ringexist;
     TS_RING ring;
     int len;

     /* check arguments */
     if (nslots < 0) {
          elog_send(ERROR, 
		    "slots should be 0 or above");
	  return 0;
     }
     if (ringname == NULL || *ringname == '\0') {
          elog_send(ERROR, "rings must have names");
	  return 0;
     }
     if (description == NULL) {
          elog_send(ERROR, 
		    "rings must have a description");
	  return 0;
     }

     /* see if the ring already exists */
     snprintf(ringrec, TS_MIDSTRLEN, "%s%s", TS_RINGSPACE, ringname);
     ringexist = hol_get(hol, ringrec, &len);
     if (ringexist) {
	  /* cant continue as ring already exists */
	  nfree(ringexist);
	  return 0;	/* failure - ring exist */
     }

     /* initialise structure */
     ring = xnmalloc(sizeof(struct ts_ring));
     ring->hol = hol;
     ring->super = super;
     ring->lastread = -1;
     ring->nslots = nslots;
     ring->oldest = -1;
     ring->youngest = -1;
     ring->name = xnstrdup(ringname);
     if (description)
	  ring->description = xnstrdup(description);
     else
	  ring->description = xnstrdup("");
     if (password)
	  ring->password = xnstrdup(password);
     else
	  ring->password = xnstrdup("");

     ts_inwritering(ring);
     return ring;
}

/* Search for a ring's record in an open holstore, which will be a textual 
 * representation of TS_RING. It contains the characteristics of the ring.
 * If the ring was found with the correct password, return a TS_RING 
 * or if unable to find the ring or there was an incorrect password, 
 * return NULL.
 * The TS_RING structure should be freed with ts_infreering().
 */
TS_RING ts_inreadring(HOLD hol, 
		      struct ts_superblock *super,
		      char *ringname, 
		      char *password)
{
     char ringrec[TS_MIDSTRLEN], txtname[TS_MIDSTRLEN], 
	  txtdescrip[TS_MIDSTRLEN], txtpassword[TS_MIDSTRLEN];
     char *ringtxt;
     TS_RING ring;
     int len, r;

     /* initialise */
     txtname[0] = txtdescrip[0] = txtpassword[0] = '\0';

     /* compose ring name and read in record */
     snprintf(ringrec, TS_MIDSTRLEN, "%s%s", TS_RINGSPACE, ringname);
     ringtxt = hol_get(hol, ringrec, &len);
     if (!ringtxt)
	  return NULL;	/* failure - ring does not exist */

     /* Create memory copy of ring if it exists and fill it */
     ring = xnmalloc(sizeof(struct ts_ring));
     ring->hol = hol;
     ring->super = super;
     ring->lastread = -1;

     /* scan record into structure */
     r = sscanf(ringtxt, "%d | %d | %d | %[^|] | %[^|] | %s", &ring->nslots, 
		&ring->oldest,&ring->youngest,txtname,txtdescrip,txtpassword);
     nfree(ringtxt);
     if (r < 5) {
          nfree(ring);
	  return NULL;	/* failure - did not read right number of parameters */
     }

     /* check password  */
     if (password || txtpassword[0]) {
	  if ( ! password) {
	       elog_send(DIAG, "no supplied password");
	       nfree(ring);
	       return NULL;
	  }
	  if (strcmp(password, txtpassword)) {
	       elog_send(DIAG, "password mismatch");
	       nfree(ring);
	       return NULL;	/* failure - password mismatch */
	  }
     }

     /* password checked OK  - finish assigning structure & return success */
     ring->name = xnstrdup(txtname);
     ring->description = xnstrdup(txtdescrip);
     ring->password = xnstrdup(txtpassword);
     return ring;
}

/*
 * Write ring to holstore. The memory allocations are not are not affected.
 * Returns 1 for success or 0 for failure.
 */
int ts_inwritering(struct ts_ring *ring) {
     char ringtxt[TS_LONGSTRLEN], ringname[TS_MIDSTRLEN];
     int ringlen, namelen;

     /* compose key */
     namelen = snprintf(ringname, TS_MIDSTRLEN, "%s%s", TS_RINGSPACE, 
			ring->name);
     if (namelen == TS_MIDSTRLEN) {
	  elog_send(ERROR, "key overflowed");
	  return 0;	/* failure - not enough room */
     }

     /* compose structure into record */
     ringlen = snprintf(ringtxt, TS_LONGSTRLEN, "%d|%d|%d|%s|%s|%s", 
			ring->nslots, ring->oldest, ring->youngest, 
			ring->name, ring->description, ring->password);
     if (ringlen == TS_LONGSTRLEN) {
	  elog_send(ERROR, "ring overflowed");
	  return 0;	/* failure - not enough room */
     }

     /* write structure away, including \0 */
     return(hol_put(ring->hol, ringname, ringtxt, ringlen+1));
}

/*
 * Update the TS_RING structure in memory with that held on disk
 * Returns 1 if update was successful or 0 for failure.
 * Failures could be if the ring has been deleted or the ring structure is
 * corrupted on disk.
 */
int ts_inupdatering(TS_RING ring)
{
     char ringrec[TS_MIDSTRLEN];
     char *ringtxt;
     int len, r;

     /* compose ring name and read in record */
     snprintf(ringrec, TS_MIDSTRLEN, "%s%s", TS_RINGSPACE, ring->name);
     ringtxt = hol_get(ring->hol, ringrec, &len);
     if (!ringtxt) {
          elog_printf(DIAG, "ring %s does not exist, it has probably "
		      "been deleted", ring->name);
	  return 0;	/* failure - ring does not exist */
     }

     /* scan record into structure
      * nslots, oldest and youngest could all change but
      * the name, description and password and not allowed to change
      * while on-line. This can be changed easily
      */
     r = sscanf(ringtxt, "%d | %d | %d | %*[^|] | %*[^|] | %*s", 
		&ring->nslots, &ring->oldest, &ring->youngest);
     nfree(ringtxt);
     if (r != 3) {
          elog_printf(ERROR, "ring %s has been corrupted, %d parameters read",
		      ring->name, r);
	  return 0;	/* failure - did not read right number of parameters */
     }

     return 1;
}

/* free the sub allocations of the ring, but not the ring itself */
void ts_infreering(struct ts_ring *ring)
{
     nfree(ring->name);
     nfree(ring->description);
     nfree(ring->password);
     nfree(ring);
}


/*
 * Write datum to the holstore, keyed on the ring name and the element 
 * number in the arguments. No change is made to the passed structure.
 * Returns 1 for success or 0 for failure.
 */
int ts_inwritedatum(struct ts_ring *ring,	/* ring to contain element */
		    char *caller,		/* name of calling function */
		    int element,		/* index of new element */
		    void *block,		/* the block to write */
		    int length			/* length of block */ )
{
     int r;
     char datumname[TS_MIDSTRLEN];

     if (element < 0) {
          elog_printf(ERROR, "%s negative element: %d",
		      caller, element);
	  return 0;
     }

     /* create the datum name */
     r = snprintf(datumname, TS_MIDSTRLEN, "%s%s_%d", TS_DATASPACE, 
		  ring->name, element);
     if (r == TS_MIDSTRLEN) {
	  elog_printf(ERROR, "%s datum name too long: %s", caller, datumname);
	  return 0;
     }

     /* add record */
     if ( ! hol_put(ring->hol, datumname, block, length) ) {
	  elog_printf(ERROR, "%s unable to write datum: %s", caller, 
		      datumname);
	  return 0;
     }

     return 1;
}


/*
 * Read datum indexed by `element' and handle errors.
 * Returns nmalloc()ed datum block if successful with its size set in length
 * or NULL for failure. Please nfree() data after use. Cheers.
 */
void *ts_inreaddatum(struct ts_ring *ring,/* Ring that contains element */
		     char *caller,	/* name of calling function */
		     int element,	/* index of wanted element */ 
		     int *length	/* length of returned datum */ )
{
     int r;
     char datumname[TS_MIDSTRLEN];

     if (element < 0) {
	  elog_printf(ERROR, "%s negative element: %s", caller, element);
	  return NULL;
     }

     /* create the datum name */
     r = snprintf(datumname, TS_MIDSTRLEN, "%s%s_%d", TS_DATASPACE, 
		  ring->name, element);
     if (r == TS_MIDSTRLEN) {
	  elog_printf(ERROR, "%s datum name too long: %s", caller, datumname);
	  return NULL;
     }

     /* read record */
     return ( hol_get(ring->hol, datumname, length) );
}


/*
 * Remove datum indexed by `element' and handle errors. 
 * Returns 0 for failure, 1 for success.
 */
int ts_inrmdatum(struct ts_ring *ring,	/* Ring that contains element */
		 char *caller,		/* name of calling function */
		 int element		/* index of doomed element */ )
{
     int r;
     char datumname[TS_MIDSTRLEN];

     if (element < 0) {
	  elog_printf(ERROR, "%s negative element: %d", caller, element);
	  return 0;
     }

     /* create the datum name */
     r = snprintf(datumname, TS_MIDSTRLEN, "%s%s_%d", TS_DATASPACE, 
		  ring->name, element);
     if (r == TS_MIDSTRLEN) {
	  elog_printf(ERROR, "%s datum name too long: %s", caller, datumname);
	  return 0;
     }

     /* delete record */
     if ( ! hol_rm(ring->hol, datumname) ) {
	  elog_printf(ERROR, "%s datum does not exist: %s", caller, datumname);
	  return 0;
     }

     return 1;
}


#if TEST

#include <sys/time.h>
#define TEST_TS1 "t.ts.1.dat"
#define TEST_TSLOG1 "t.ts.1.dat.log"
#define TEST_RING1 "ring1"
#define TEST_RING2 "ring2"
#define TEST_RING3 "ring3"
#define TEST_RING4 "ring4"
#define TEST_RING5 "ring5"
#define TEST_BUFLEN 50
#define TEST_ITER 1000

int main()
{
     TS_RING ring1, ring2, ring3, ring4, ring5;
     int r, i, len1, len2, len3, seq1, seq2, seq3;
     char *dat1, *dat2, *dat3;
     time_t time1, time2, time3;
     char datbuf1[TEST_BUFLEN];
     TREE *lst1;
     ITREE *lst2;
     ntsbuf *mgetdat;
     int nrings, nslots, nread, navail;
     TABLE tab1;

     route_init(NULL, 0);
     elog_init(0, "timestore test", NULL);
     ts_init(err, 1);

     unlink(TEST_TS1);
     unlink(TEST_TSLOG1);

     /* test 1: creating and opening rings, checking all the failure modes */
     route_printf(err, "[1] expect an error --> ");
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (ring1)
	  route_die(err, "[1] Shouldn't have opened ring\n");
     ring1 = ts_create(TEST_TS1, 0644, TEST_RING1, "This is the first test",
			NULL, 10);
     if (!ring1)
	  route_die(err, "[1] Unable to create ring\n");
     dat1 = hol_get(ring1->hol, TS_SUPERNAME, &len1);
     if (!dat1)
	  route_die(err, "[1] Timestore superblock not created\n");
     nfree(dat1);
      route_printf(err, "[1] expect another error --> ");
     ring2 = ts_create(TEST_TS1, 0644, TEST_RING1, "This is the first test",
			NULL, 10);
     if (ring2)
	  route_die(err, "[1] Shouldn't be able to create a second time\n");
     ts_close(ring1);
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[1] Unable to open existing ring\n");
     ring2 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (!ring2)
	  route_die(err, "[1] Unable to again open existing ring\n");
     ts_close(ring2);
     ts_close(ring1);

     /* test 2: removing rings */
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[2] Unable to open existing ring\n");
     if ( ! ts_rm(ring1))
	  route_die(err, "[2] Unable to remove existing ring\n");
     route_printf(err, "[2] expect a further error --> ");
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (ring1)
	  route_die(err, "[2] Shouldn't have opened removed ring\n");

     /* test 3: rings at quantity */
     ring1 = ts_create(TEST_TS1, 0644, TEST_RING1, "This is the first test",
			NULL, 10);
     if (!ring1)
	  route_die(err, "[3] Unable to create ring 1\n");
     ring2 = ts_create(TEST_TS1, 0644, TEST_RING2, "This is the second test",
			NULL, 0);
     if (!ring2)
	  route_die(err, "[3] Unable to create ring 2\n");
     ring3 = ts_create(TEST_TS1, 0644, TEST_RING3, "This is the third test",
			NULL, 100);
     if (!ring3)
	  route_die(err, "[3] Unable to create ring 3\n");
     ring4 = ts_create(TEST_TS1, 0644, TEST_RING4, "This is the forth test",
			NULL, 100);
     if (!ring4)
	  route_die(err, "[3] Unable to create ring 4\n");
     ring5 = ts_create(TEST_TS1, 0644, TEST_RING5, "This is the fifth test",
			NULL, 17);
     if (!ring5)
	  route_die(err, "[3] Unable to create ring 5\n");
     /* now delete them all */
     if ( ! ts_rm(ring1) )
	  route_die(err, "[3] Unable to delete ring 1\n");
     if ( ! ts_rm(ring2) )
	  route_die(err, "[3] Unable to delete ring 2\n");
     if ( ! ts_rm(ring3) )
	  route_die(err, "[3] Unable to delete ring 3\n");
     if ( ! ts_rm(ring4) )
	  route_die(err, "[3] Unable to delete ring 4\n");
     if ( ! ts_rm(ring5) )
	  route_die(err, "[3] Unable to delete ring 5\n");

     /* test 4: put and get data on a ring */
     ring1 = ts_create(TEST_TS1, 0644, TEST_RING1, "4th test: get/set rings",
			NULL, 10);
     if (!ring1)
	  route_die(err, "[4] Unable to create ring\n");
     if (ts_put(ring1, "element 1", 10) == -1 )
	  route_die(err, "[4] Unable to put element 1\n");
     dat1 = ts_get(ring1, &len1, &time1, &seq1);
     if ( ! dat1)
	  route_die(err, "[4] Unable to get element 1\n");
     if (strncmp(dat1, "element 1", len1))
	  route_die(err, "[4] Element 1 incorrectly read\n");
     nfree(dat1);
     if (ts_put(ring1, "element 2", 10) == -1)
	  route_die(err, "[4] Unable to put element 2\n");
     dat1 = ts_get(ring1, &len1, &time1, &seq1);
     if ( ! dat1)
	  route_die(err, "[4] Unable to get element 2\n");
     if (strncmp(dat1, "element 2", len1))
	  route_die(err, "[4] Element 2 incorrectly read\n");
     nfree(dat1);
     ts_close(ring1);
		/* open and read again */
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[4] Unable to reopen existing ring\n");
     dat1 = ts_get(ring1, &len1, &time1, &seq1);
     if ( ! dat1)
	  route_die(err, "[4] Unable to reget element 1\n");
     if (strncmp(dat1, "element 1", len1))
	  route_die(err, 
		    "[4] Reobtained element 1 incorrectly read (got  %s)\n", 
		    dat1);
     nfree(dat1);
     dat1 = ts_get(ring1, &len1, &time1, &seq1);
     if ( ! dat1)
	  route_die(err, "[4] Unable to reget element 2\n");
     if (strncmp(dat1, "element 2", len1))
	  route_die(err, "[4] Reobtained element 2 incorrectly read\n");
     nfree(dat1);
     ts_close(ring1);

     /* test 5: put and get several to test ring rollover */
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[5] Unable to reopen existing ring\n");
     dat1 = ts_get(ring1, &len1, &time1, &seq1);
     if ( ! dat1)
	  route_die(err, "[5] Unable to reget element 1\n");
     if (strncmp(dat1, "element 1", len1))
	  route_die(err, 
		    "[4] Reobtained element 1 incorrectly read (got  %s)\n", 
		    dat1);
     nfree(dat1);
     dat1 = ts_get(ring1, &len1, &time1, &seq1);
     if ( ! dat1)
	  route_die(err, "[5] Unable to reget element 2\n");
     if (strncmp(dat1, "element 2", len1))
	  route_die(err, "[5] Reobtained element 2 incorrectly read\n");
     nfree(dat1);

     for (i=3; i<15; i++) {
	  /* write 12 elements */
	  r = sprintf(datbuf1, "element %d", i);
	  if (ts_put(ring1, datbuf1, r+1) == -1)
	       route_die(err, "[5] Unable to put element %d\n", i);
     }
     for (i=5; i<15; i++) {
	  /* read last 10 elements */
	  r = sprintf(datbuf1, "element %d", i);
	  dat1 = ts_get(ring1, &len1, &time1, &seq1);
	  if ( ! dat1)
	       route_die(err, "[5] Unable to get element %d\n", i);
	  if (r +1 != len1)
	       route_die(err, "[5] Incorrect length of element %d\n", i);
	  if (strncmp(dat1, datbuf1, len1))
	       route_die(err, "[5] element %d incorrectly read\n", i);
	  nfree(dat1);
	  if (time1 < time(NULL) - 5 || time1 > time(NULL))
	       route_die(err, "[5] element %d wrong time %d %d\n", i, time1, 
			 time(NULL));
     }
     ts_close(ring1);

     /* test 6: mget */
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[6] Unable to reopen existing ring\n");
     r = ts_mget(ring1, 20, &lst2);
     if (r != 10)
	  route_die(err, "[6] %d returned, should be 10\n", r);
     itree_traverse(lst2) {
	  /* check the 10 elements returned by mget */
	  mgetdat = itree_get(lst2);
	  r = sprintf(datbuf1, "element %d", itree_getkey(lst2)+1);
	  if (r +1 != mgetdat->len)
	       route_die(err, "[6] Incorrect length %d, should be %d\n", 
			 mgetdat->len, r+1);
	  if (mgetdat->instime < time(NULL) - 5 || 
	      mgetdat->instime > time(NULL))
	       route_die(err, "[6] sequence %d wrong time %d, should be %d\n",
			 itree_getkey(lst2), mgetdat->instime, time(NULL));
	  if (mgetdat->seq != itree_getkey(lst2))
	       route_die(err, "[6] Incorrect sequence %d, should be %d\n", 
			 mgetdat->seq, itree_getkey(lst2));
	  if (strncmp(mgetdat->buffer, datbuf1, mgetdat->len))
	       route_die(err, "[6] sequence %d incorrectly read\n", 
			 mgetdat->seq);
     }
     ts_mgetfree(lst2);
     ts_close(ring1);

     /* test 7: jump */
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[7] Unable to reopen existing ring\n");
     if (ts_lastread(ring1) != -1)
	  route_die(err, "[7] lastread not -1 at begining\n");
     ts_jumpoldest(ring1);
     if (ts_lastread(ring1) != 3)
	  route_die(err, "[7] lastread %d not 3 at oldest\n",
		    ts_lastread(ring1));
     ts_jumpyoungest(ring1);
     if (ts_lastread(ring1) != 13)
	  route_die(err, "[7] lastread %d not 13 at youngest\n", 
		    ts_lastread(ring1));
     ts_jump(ring1, 4);
     if (ts_lastread(ring1) != 13)
	  route_die(err, "[7] lastread %d not 13 after overjump 1\n", 
		    ts_lastread(ring1));
     ts_setjump(ring1, 11);
     if (ts_lastread(ring1) != 11)
	  route_die(err, "[7] lastread %d not 11 after set 1\n", 
		    ts_lastread(ring1));
     ts_jump(ring1, 4);
     if (ts_lastread(ring1) != 13)
	  route_die(err, "[7] lastread %d not 13 after overjump 2\n", 
		    ts_lastread(ring1));
     ts_jump(ring1, -40);
     if (ts_lastread(ring1) != 3)
	  route_die(err, "[7] lastread %d not 3 after underjump 1\n", 
		    ts_lastread(ring1));
     ts_close(ring1);

     /* test 8 multiple rings */
     ring2 = ts_create(TEST_TS1, 0644, TEST_RING2, "Second ring", NULL, 100);
     ring3 = ts_create(TEST_TS1, 0644, TEST_RING3, "Third ring", NULL, 12);
     ring4 = ts_create(TEST_TS1, 0644, TEST_RING4, "Forth ring", NULL, 32);
     if ( ! (ring2 || ring3 || ring4))
	  route_die(err, "[8] Unable to create rings 2-4\n");
     for (i=0; i<100; i++) {
	  r = sprintf(datbuf1, "element %d", i);
	  if (ts_put(ring2, datbuf1, r+1) == -1)
	       route_die(err, "[8] Unable to put element %d on ring2\n", i);
     }
     for (i=0; i<12; i++) {
	  r = sprintf(datbuf1, "element %d", i);
	  if (ts_put(ring3, datbuf1, r+1) == -1)
	       route_die(err, "[8] Unable to put element %d on ring3\n", i);
     }
     for (i=0; i<32; i++) {
	  r = sprintf(datbuf1, "element %d", i);
	  if (ts_put(ring4, datbuf1, r+1) == -1)
	       route_die(err, "[8] Unable to put element %d on ring4\n", i);
     }
     ts_jumpoldest(ring2);
     ts_jumpoldest(ring3);
     ts_jumpoldest(ring4);
     if ((r = ts_jumpyoungest(ring2)) != 100)
	  route_die(err, "[8] wrong young value ring2 %d [seq %d] not 100\n", 
		    r, ts_lastread(ring2));
     if ((r = ts_jumpyoungest(ring3)) != 12)
	  route_die(err, "[8] wrong young value ring3 %d [seq %d] not 12\n",
		    r, ts_lastread(ring3));
     if ((r = ts_jumpyoungest(ring4)) != 32)
	  route_die(err, "[8] wrong young value ring4 %d [seq %d] not 32\n",
		    r, ts_lastread(ring4));
     ts_setjump(ring3, 4);
     ts_setjump(ring2, 45);
     ts_setjump(ring4, 24);
     if ((dat1 = ts_get(ring2, &len1, &time1, &seq1)) == NULL)
	  route_die(err, "[8] Cant get 45 record\n");
     if ((dat2 = ts_get(ring3, &len2, &time2, &seq2)) == NULL)
	  route_die(err, "[8] Cant get 4 record\n");
     if ((dat3 = ts_get(ring4, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[8] Cant get 24 record\n");
     if (seq1 != 46)
	  route_die(err, "[8] ring2 %d != 46\n", seq1);
     if (seq2 != 5)
	  route_die(err, "[8] ring3 %d != 5\n", seq2);
     if (seq3 != 25)
	  route_die(err, "[8] ring4 %d != 25\n", seq3);
     if (strcmp(dat1, "element 46"))
	  route_die(err, "[8] ring2 text not the same: %s\n", dat1);
     if (strcmp(dat2, "element 5"))
	  route_die(err, "[8] ring3 text not the same: %s\n", dat2);
     if (strcmp(dat3, "element 25"))
	  route_die(err, "[8] ring4 text not the same: %s\n", dat3);
     nfree(dat1);
     nfree(dat2);
     nfree(dat3);

     /* test 9: stats */
     				/* current state of ring 4 */
     if ( ! ts_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9a] unable to stat\n");
     if (nrings != 4)
	  route_die(err, "[9a] should be 4 rings, not %d\n", nrings);
     if (nslots != 32)
	  route_die(err, "[9a] should be 32 slots, not %d\n", nslots);
     if (nread != 26)
	  route_die(err, "[9a] should be 26 read, not %d\n", nread);
     if (navail != 6)
	  route_die(err, "[9a] should be 6 available, not %d\n", navail);
     if (strcmp(dat1, "Forth ring"))
	  route_die(err, "[9a] should be 'Fourth ring', not %s\n", dat1);
     nfree(dat1);
				/* begining of ring 4 */
     r = ts_jumpoldest(ring4);
     if (r != -26)
	  route_die(err, "[9b] should jumpoldest -25, not %d\n", r);
     if ( ! ts_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9b] unable to stat\n");
     if (nrings != 4)
	  route_die(err, "[9b] should be 4 rings, not %d\n", nrings);
     if (nslots != 32)
	  route_die(err, "[9b] should be 32 slots, not %d\n", nslots);
     if (nread != 0)
	  route_die(err, "[9b] should be 0 read, not %d\n", nread);
     if (navail != 32)
	  route_die(err, "[9b] should be 32 available, not %d\n", navail);
     nfree(dat1);
				/* one in to ring 4 */
     if ((dat3 = ts_get(ring4, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[9c] Cant get 1st record\n");
     if ( ! ts_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9c] unable to stat\n");
     if (nrings != 4)
	  route_die(err, "[9c] should be 4 rings, not %d\n", nrings);
     if (nslots != 32)
	  route_die(err, "[9c] should be 32 slots, not %d\n", nslots);
     if (nread != 1)
	  route_die(err, "[9c] should be 1 read, not %d\n", nread);
     if (navail != 31)
	  route_die(err, "[9c] should be 31 available, not %d\n", navail);
     nfree(dat3);
     nfree(dat1);
				/* two in to ring 4 */
     if ((dat3 = ts_get(ring4, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[9d] Cant get 2nd record\n");
     if ( ! ts_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9d] unable to stat\n");
     if (nrings != 4)
	  route_die(err, "[9d] should be 4 rings, not %d\n", nrings);
     if (nslots != 32)
	  route_die(err, "[9d] should be 32 slots, not %d\n", nslots);
     if (nread != 2)
	  route_die(err, "[9d] should be 2 read, not %d\n", nread);
     if (navail != 30)
	  route_die(err, "[9d] should be 30 available, not %d\n", navail);
     nfree(dat3);
     nfree(dat1);
				/* end of ring 4 */
     r = ts_jumpyoungest(ring4);
     if (r != 30)
	  route_die(err, "[9e] should jumpyoungest +30, not %d\n", r);
     if ( ! ts_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9e] unable to stat\n");
     if (nrings != 4)
	  route_die(err, "[9e] should be 4 rings, not %d\n", nrings);
     if (nslots != 32)
	  route_die(err, "[9e] should be 32 slots, not %d\n", nslots);
     if (nread != 32)
	  route_die(err, "[9e] should be 32 read, not %d\n", nread);
     if (navail != 0)
	  route_die(err, "[9e] should be 0 available, not %d\n", navail);
     nfree(dat1);
				/* one back in ring 4 */
     r = ts_jump(ring4, -1);
     if (r != -1)
	  route_die(err, "[9f] should jump -1, not %d\n", r);
     if ( ! ts_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9f] unable to stat\n");
     if (nrings != 4)
	  route_die(err, "[9f] should be 4 rings, not %d\n", nrings);
     if (nslots != 32)
	  route_die(err, "[9f] should be 32 slots, not %d\n", nslots);
     if (nread != 31)
	  route_die(err, "[9f] should be 31 read, not %d\n", nread);
     if (navail != 1)
	  route_die(err, "[9f] should be 1 available, not %d\n", navail);
     nfree(dat1);
				/* two back in ring 4 */
     r = ts_jump(ring4, -1);
     if (r != -1)
	  route_die(err, "[9g] should jump -1, not %d\n", r);
     if ( ! ts_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9g] unable to stat\n");
     if (nrings != 4)
	  route_die(err, "[9g] should be 4 rings, not %d\n", nrings);
     if (nslots != 32)
	  route_die(err, "[9g] should be 32 slots, not %d\n", nslots);
     if (nread != 30)
	  route_die(err, "[9g] should be 30 read, not %d\n", nread);
     if (navail != 2)
	  route_die(err, "[9g] should be 2 available, not %d\n", navail);
     nfree(dat1);
			     /* new, empty ring */
     ring5 = ts_create(TEST_TS1, 0644, TEST_RING5, "Fifth ring", NULL, 5);
     if ( ! ring5)
	  route_die(err, "[9h] unable to create fifth ring\n");
     if ( ! ts_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9h] unable to stat\n");
     if (nrings != 5)
	  route_die(err, "[9h] should be 5 rings, not %d\n", nrings);
     if (nslots != 5)
	  route_die(err, "[9h] should be 5 slots, not %d\n", nslots);
     if (nread != 0)
	  route_die(err, "[9h] should be 0 read, not %d\n", nread);
     if (navail != 0)
	  route_die(err, "[9h] should be 0 available, not %d\n", navail);
     nfree(dat1);
			     /* single record ring */
     if (ts_put(ring5, "bollocks", strlen("bollocks")+1) == -1 )
	  route_die(err, "[9i] Unable to put element 1 on ring5\n");
     if ( ! ts_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9i] unable to stat\n");
     if (nslots != 5)
	  route_die(err, "[9i] should be 5 slots, not %d\n", nslots);
     if (nread != 0)
	  route_die(err, "[9i] should be 0 read, not %d\n", nread);
     if (navail != 1)
	  route_die(err, "[9i] should be 1 available, not %d\n", navail);
     nfree(dat1);
			     /* double record ring */
     if (ts_put(ring5, "battersea", strlen("battersea")+1) == -1 )
	  route_die(err, "[9j] Unable to put element 2 on ring5\n");
     if ( ! ts_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9j] unable to stat\n");
     if (nslots != 5)
	  route_die(err, "[9j] should be 5 slots, not %d\n", nslots);
     if (nread != 0)
	  route_die(err, "[9j] should be 0 read, not %d\n", nread);
     if (navail != 2)
	  route_die(err, "[9j] should be 2 available, not %d\n", navail);
     nfree(dat1);
     				/* read first record */
     if ((dat3 = ts_get(ring5, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[9k] Cant get 1st record\n");
     if ( ! ts_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9k] unable to stat\n");
     if (nslots != 5)
	  route_die(err, "[9k] should be 5 slots, not %d\n", nslots);
     if (nread != 1)
	  route_die(err, "[9k] should be 1 read, not %d\n", nread);
     if (navail != 1)
	  route_die(err, "[9k] should be 1 available, not %d\n", navail);
     nfree(dat1);
     nfree(dat3);
     				/* read first record */
     if ((dat3 = ts_get(ring5, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[9l] Cant get 2nd record\n");
     if ( ! ts_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9l] unable to stat\n");
     if (nslots != 5)
	  route_die(err, "[9l] should be 5 slots, not %d\n", nslots);
     if (nread != 2)
	  route_die(err, "[9l] should be 2 read, not %d\n", nread);
     if (navail != 0)
	  route_die(err, "[9l] should be 0 available, not %d\n", navail);
     nfree(dat1);
     nfree(dat3);

     ts_close(ring2);
     ts_close(ring3);
     ts_close(ring4);
     ts_close(ring5);

     /* test 10: prealloc */

     /* test 11: resize */

     /* test 12: lsrings */
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     lst1 = ts_lsrings(ring1);
#if 0
     tree_traverse(lst1)
	  route_printf(err, "%s    %s\n", tree_getkey(lst1), tree_get(lst1));
#endif
     if (tree_find(lst1, TEST_RING1) == TREE_NOVAL)
	  route_die(err, "[12] Unable to find %s\n", TEST_RING1);
     if (tree_find(lst1, TEST_RING2) == TREE_NOVAL)
	  route_die(err, "[12] Unable to find %s\n", TEST_RING2);
     if (tree_find(lst1, TEST_RING3) == TREE_NOVAL)
	  route_die(err, "[12] Unable to find %s\n", TEST_RING3);
     if (tree_find(lst1, TEST_RING4) == TREE_NOVAL)
	  route_die(err, "[12]  Unable to find %s\n", TEST_RING4);
     ts_freelsrings(lst1);
     ts_close(ring1);

     /* test 13: replace an element */

     /* test 14: mget with table */
     ring1 = ts_open(TEST_TS1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[14] Unable to reopen existing ring\n");
     tab1 = ts_mget_t(ring1, 20);
     if ( (r = table_nrows(tab1)) != 10)
	  route_die(err, "[14] %d returned, should be 10\n", r);
#if 0
     table_traverse(tab1) {
	  /* check the 10 elements returned by mget */
	  r = sprintf(datbuf1, "element %d", 
		      atoi(table_getcurrentcell(tab1, "_seq")) +1);
	  if (r != strlen(table_getcurrentcell(tab1, "Value")))
	       route_die(err, "[14-%d] Incorrect length %d, should be %d\n", 
			 table_getcurrentrowkey(tab1),
			 strlen(table_getcurrentcell(tab1, "Value")), r);
#if 0
	  if (mgetdat->instime < time(NULL) - 5 || 
	      mgetdat->instime > time(NULL))
	       route_die(err, "[6] sequence %d wrong time %d, should be %d\n",
			 itree_getkey(lst2), mgetdat->instime, time(NULL));
	  if (mgetdat->seq != itree_getkey(lst2))
	       route_die(err, "[6] Incorrect sequence %d, should be %d\n", 
			 mgetdat->seq, itree_getkey(lst2));
	  if (strncmp(mgetdat->buffer, datbuf1, mgetdat->len))
	       route_die(err, "[6] sequence %d incorrectly read\n", 
			 mgetdat->seq);
#endif
     }
#endif
     table_destroy(tab1);
     ts_close(ring1);

     /* finalise */
     elog_fini();
     route_close(err);
     route_fini();
     unlink(TEST_TS1);

     printf("tests finished successfully\n");
     exit(0);
}

#endif /* TEST */
