/*
 * Table storage on top of timestore, spanstore and holstore
 *
 * Nigel Stuckey, April & August 1999
 * Copyright System Garden Limited 1996-2001, All rights reserved.
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "nmalloc.h"
#include "holstore.h"
#include "timestore.h"
#include "tablestore.h"
#include "spanstore.h"
#include "route.h"
#include "elog.h"
#include "util.h"

/* Initialise tablestore class */
void tab_init()
{
     ts_init();
}

void tab_fini()
{
     ts_fini();
}

/*
 * Open table storage ring.
 * Given the name of a holstore, attempt to open a timeseries ring inside it.
 * Register the headers as a schema for subsequent tab_put() calls.
 * Returns a reference to the tablestore ring (TAB_RING) if successful, 
 * or NULL if unable to find headers or not allowed to create them later.
 * Tab_open() opens storage that should be closed with tab_close(). 
 */
TAB_RING tab_open(char *holname,  /* filename of holstore containing tables */
		  char *ringname, /* name of table store ring */
		  char *password  /* password to ring (NULL for none) */ )
{
     TS_RING ts;
     TAB_RING tab;

     /* open timestore ring to store data */
     ts = ts_open(holname, ringname, password);
     if ( ! ts )
           return NULL;

     tab = tab_open_fromts(ts);

     return tab;
}


/*
 * Open table storage ring from a already open timestore ring
 * No error checking is done, but a tablestore structure (TAB_RING) is 
 * allocated to hold the open timestore structure and this is returned.
 * Use tab_close() to clear up, which will remove the timestore.
 */
TAB_RING tab_open_fromts(TS_RING ts)
{
     TAB_RING tab;

     /* create a descriptor for this instance */
     tab = xnmalloc( sizeof(struct tab_session) );
     tab->ts = ts;
     tab->schema = NULL;
     tab->ncols = 0;
     tab->from = tab->to = -1;		/* flag to start a new sequence */

     return tab;
}


/* Close the tablestore ring */
void tab_close(TAB_RING t) {
     if (!t)
	  return;

     ts_close(t->ts);
     if (t->schema) {
	  itree_clearoutandfree(t->schema);
	  itree_destroy(t->schema);
     }
     nfree(t);
}

/*
 * Create a ring in a holstore.
 * Returns a reference to the tablestore ring (TAB_RING) if successful, 
 * or NULL if unable to find headers or not allowed to create them later.
 * Tab_create() opens storage that should be closed with tab_close(). 
 */
TAB_RING tab_create(char *holname,/* filename of holstore containing tables */
		    int   mode,	  /* file permission mode for holstore */
		    char *tablename,	/* name of table store ring */
		    char *description,	/* description of ring */
		    char *password,	/* password to ring (NULL for none) */
		    int   nslots	/* number of slots in ring */ )
{
     TS_RING ts;
     TAB_RING tab;

     /* open timestore ring to store data */
     ts = ts_create(holname, mode, tablename, description, password, nslots);
     if ( ! ts )
           return NULL;

     /* create a descriptor for this instance */
     tab = xnmalloc( sizeof(struct tab_session) );
     tab->ts = ts;
     tab->schema = NULL;
     tab->ncols = 0;
     tab->from = tab->to = -1;		/* flag to start a new sequence */

     return tab;
}

/*
 * Remove the currently open ring and remove all data and header information.
 * This call implys a close, and will invalidate the passed ring.
 * Dont try to use it after this call.
 * Return 1 for success or 0 for failure
 */
int tab_rm(TAB_RING t)
{
     SPANS stab;

     /* start overall transaction - not implemented as currently too hard */

     stab = spans_readblock(t->ts);
     if (stab) {
	  if (spans_purge(stab, INT_MAX, (time_t) 0) == -1)
	       return 0;
	  spans_writeblock(t->ts, stab);
	  table_destroy(stab);
     }
     if ( ! ts_rm(t->ts) ) {
          return 0;
     }

     /* end overall transaction - not implemented as currently too hard */

     /* free structures */
     if (t->schema) {
	  itree_clearoutandfree(t->schema);
	  itree_destroy(t->schema);
     }
     nfree(t);

     return 1;
}


/*
 * Put a table of data on the end of a valid ring.
 * The table is checked against the current header for the open session
 * and if the same, the span is extended. If the header has changed, 
 * a new session will be started.
 * Binary data is not allowed in the table class.
 * If the ring has a finite size and has reached its maximum, the oldest
 * datum will be destructively removed before writing the new one.
 * If a header has not been declared for the session by the open or create
 * methods, default ones are allocated, which must be followed for the rest
 * of the session.
 * Returns the sequence number if successful or -1 for failure. 
 * If failed, the ring will remain open and the handle will be valid.
 * Failures include badly formatted tables.
 */
int tab_put(TAB_RING t,	 	/* open ring */
	    TABLE data		/* data in a table structure */ )
{
     return tab_put_withtime(t, data, time(NULL));
}


/*
 * As tab_put, but specifying the time for each datum
 */
int tab_put_withtime(TAB_RING t, 	/* open ring */
		     TABLE data, 	/* data in a table structure */
		     time_t instime	/* insertion time */ )
{
     int seq, r, oldseq, oldlen, lenhead, saveseq, newspan=0;
     char *hdtext, *infotext, *buf, *olddata;
     HOLD hol;
     TREE *hd;
     SPANS stab;
     time_t oldtime;

     /* initialise */
     hol = ts_holstore(t->ts);

     /* check for data and extract it */
     if (table_ncols(data) <= 0) {
          elog_send(ERROR, "no columns in data");
	  return -1;
     }
     if (table_nrows(data) <= 0) {
          elog_send(ERROR, "no rows in data");
	  return -1;
     }
     buf = table_outbody(data);

     /* check the columns: column number check only */
     if (t->from == -1 || t->ncols != table_ncols(data))
	  newspan++;		/* columns changed: start new span */

     /* begin overall transaction covering spanstore and timestore */
     if ( ! hol_begintrans(hol, 'w') ) {
          elog_send(ERROR, "unable to get transaction");
	  nfree(buf);
	  return -1;
     }
     hol_inhibittrans(hol);

     /* write the data away in the timestore */
     seq = ts_put_withtime(t->ts, buf, strlen(buf)+1, instime);
     if (seq == -1) {
          hol_allowtrans(hol);
	  hol_rollback(hol);
          return -1;
     }
     nfree(buf);

     /* read span table or start one if one does not exist */
     stab = spans_readblock(t->ts);
     if ( ! stab )
	  stab = spans_create();

     /* update the list of spans to include the header */
     if (newspan) {
	  /* new span - update TAB_RING */
	  if (t->schema)
	       itree_clearoutandfree(t->schema);
	  else
	       t->schema = itree_create();
	  hd = table_getheader(data);
	  tree_traverse(hd)
	       itree_append(t->schema, xnstrdup(tree_getkey(hd)));
	  t->ncols = tab_ncols(data);
          t->from = t->to = seq;

	  /* collect and prepare the column names + info */
	  hdtext = table_outheader(data);
	  infotext = table_outinfo(data);
	  lenhead = strlen(hdtext);
	  if (infotext) {
	       /* if has info lines, concatinate with interveaning \n */
	       buf = util_strjoin(hdtext, "\n", infotext, NULL);
	       nfree(hdtext);
	       nfree(infotext);
	       hdtext = buf;
	  }

	  /* create new span in span table */
	  r = spans_new(stab, t->from, t->to, instime, instime, hdtext);
	  nfree(hdtext);
	  if (r == 0)
	       elog_printf(ERROR, "unable to create new span "
			   "but data table was written (ring %s seq %d)", 
			   ts_name(t->ts), seq);
     } else {
	  /* extend existing span in table */
	  r = spans_extend(stab, t->from, t->to, seq, instime);
          t->to = seq;		/* update TAB_RING */
	  if (r == 0)
	       elog_printf(ERROR, "unable to update existing "
			   "span but data table was written (ring %s seq %d)", 
			   ts_name(t->ts), seq);
     }

     /* get the oldest datum to find its time: expensive operaton */
     saveseq = ts_lastread(t->ts);
     ts_jumpoldest(t->ts);
     olddata = ts_get(t->ts, &oldlen, &oldtime, &oldseq);
     nfree(olddata);
     ts_setjump(t->ts, saveseq);

     /* purge span headers that releate to data not present in the ring */
     spans_purge(stab, ts_oldest(t->ts), oldtime);
     if (t->from < ts_oldest(t->ts))
          t->from = ts_oldest(t->ts);

     /* write span block back to disk */
     r = spans_writeblock(t->ts, stab);
	  if (r == 0)
	       elog_printf(ERROR, "unable to write span black "
			   "but data table was written (ring %s seq %d)", 
			   ts_name(t->ts), seq);

     table_destroy(stab);

     /* release overall transaction */
     hol_allowtrans(hol);
     hol_commit(hol);

     if (r)
          return seq;
     else
          return -1;
}


/*
 * Put a table of data on the end of a valid ring.
 * The data must be in the text representation of the table class.
 * The table is checked against the current header for the open session
 * and if the same, the span is extended. If the header has changed, 
 * a new session will be started.
 * Binary data is not allowed in the table class.
 * If the ring has a finite size and has reached its maximum, the oldest
 * datum will be destructively removed before writing the new one.
 * If a header has not been declared for the session by the open or create
 * methods, default ones are allocated, which must be followed for the rest
 * of the session.
 * Returns the sequence number if successful or -1 for failure. 
 * If failed, the ring will remain open and the handle will be valid.
 * Failures include badly formatted tables.
 * The input tabtext will not be altered.
 */
int tab_puttext(TAB_RING t,	 	/* open ring */
		const char *tabtext	/* string containing table */ )
{
     int r;
     TABLE tab;
     char *tmptext;

     /* initialise */
     tab = table_create();
     tmptext = xnstrdup(tabtext);
     table_freeondestroy(tab, tmptext);

     /* read table text with header */
     r = table_scan(tab, tmptext, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r == -1) {
	  elog_send(ERROR, "unable to scan data");
	  table_destroy(tab);
	  return -1;
     }

     /* do the work, clear up and return */
     r = tab_put(t, tab);
     table_destroy(tab);

     return r;
}

/*
 * Multiple get of raw data.
 * Sets the ITREE retlist with a list of ntsbuf structures, representing
 * the mulitple extraction of data. For details see ts_mget().
 * In addition to this functionality, this routine augments each ntsbuf with
 * additional span information, setting datatext in each structure with
 * the appropreate span information.
 * Use tab_mgetrawfree() to free the list or tab_mgetrawfree_leavedata()
 * to remove indexes but leave the data.
 * Returns the number of samples actually obtained if successful or -1 
 * for failure.
 */
int    tab_mgetraw(TAB_RING ring, int want, ITREE **retlist)
{
     int r, tsret, firstspanseq, lastspanseq;
     ntsbuf *nts;
     SPANS span;
     char *spandata, *spantext=NULL;
     time_t firstspantime, lastspantime;

     /* get the raw data from timestore */
     tsret = ts_mget(ring->ts, want, retlist);
     if (tsret <= 0)
	  return tsret;

     /* get span block once from disk */
     span = spans_readblock(ring->ts);
     if ( ! span )
	  return -1;

     /* iterate over the nutbuf list and set spans */
     lastspanseq = -1;
     itree_traverse(*retlist) {
	  nts = itree_get(*retlist);

	  /* get new span details if needed */
	  if (nts->seq > lastspanseq) {
	       r = spans_getseq(span, nts->seq, &firstspanseq, &lastspanseq,
				&firstspantime, &lastspantime, &spandata);
	       spantext = xnstrdup(spandata);
	       if ( ! r ) {
		    /* getting the span failed, ensure that we can try again
		     * next iteration */
		    spantext = NULL;
		    lastspanseq = -1;
	       }
	  }

	  /* augment the ntsbuf */
	  nts->spantext = spantext;
     }

     table_destroy(span);
     return tsret;
}


/* Remove the ntslist produced by tab_mgetraw() */
void   tab_mgetrawfree(ITREE *list)
{
     ntsbuf *nts;
     char *alreadyfreed = NULL;

     itree_traverse(list) {
	  /* get data and loop if we have already freed it */
	  nts = itree_get(list);
	  if (nts->spantext == alreadyfreed || nts->spantext == NULL)
	       continue;

	  /* free data and remember for next loops */
	  alreadyfreed = nts->spantext;
	  nfree(nts->spantext);
     }

     /* free the bulk of data and indexes */
     ts_mgetfree(list);
}

/* Free the indexes and ntsbuf summaries allocated by tab_mgetraw but
 * leave the data blocks intact. 
 * The spantext is also removed as part of the ntsbuf index.
 */
void   tab_mgetrawfree_leavedata(ITREE *list)
{
     ntsbuf *nts;
     char *alreadyfreed = NULL;

     if ( ! list)
	  return;

     itree_traverse(list) {
	  /* get data and loop if we have already freed it */
	  nts = itree_get(list);
	  if (nts->spantext == alreadyfreed || nts->spantext == NULL)
	       continue;

	  /* free data and remember for next loops */
	  alreadyfreed = nts->spantext;
	  nfree(nts->spantext);
     }

     /* free the bulk of data and indexes */
     ts_mgetfree_leavedata(list);
}



/*
 * Return the header string of the latest RECORDED table; if no tab_puts()
 * have been carried out, it will be the header of the most recent span 
 * on disk. If tab_put()s have been run in this tab open session, then it 
 * will return the current header string.
 * The returned string should be release with nfree() when you have finished
 * If there are no spans on disk or a failure has occured, NULL is returned
 */
char *tab_getheader_latest(TAB_RING t	/* open ring */ )
{
     int dummy, r;
     char *header, *dupheader;
     SPANS stab;
     time_t dummyt;

     stab = spans_readblock(t->ts);
     r = spans_getlatest(stab, &dummy, &dummy, &dummyt, &dummyt, &header);
     if (r == 0) {
	  table_destroy(stab);
	  return NULL;
     }

     dupheader = xnstrdup(header);
     table_destroy(stab);

     return dupheader;
}

/*
 * Return the oldest header string
 * The returned string should be released with nfree().
 * If there are no spans on disk or a failure has occured, NULL is returned
 */
char *tab_getheader_oldest(TAB_RING t	/* open ring */ )
{
     int dummy, r;
     time_t dummyt;
     char *header, *dupheader;
     SPANS stab;

     stab = spans_readblock(t->ts);
     r = spans_getoldest(stab, &dummy, &dummy, &dummyt, &dummyt, &header);
     if (r == 0) {
	  table_destroy(stab);
	  return NULL;
     }

     dupheader = xnstrdup(header);
     table_destroy(stab);

     return dupheader;
}

/* 
 * Return header string associated with seq. Release with nfree() after use.
 * If there is an error or the sequence does not exist, NULL is returned.
 */
char *tab_getheader_seq(TAB_RING t,	/* open ring */
			int seq		/* table sequence number */ )
{
     int dummy, r;
     time_t dummyt;
     char *header, *dupheader;
     SPANS stab;

     stab = spans_readblock(t->ts);
     r = spans_getseq(stab, seq, &dummy, &dummy, &dummyt, &dummyt, &header);
     if (r == 0) {
	  table_destroy(stab);
	  return NULL;
     }

     dupheader = xnstrdup(header);
     table_destroy(stab);

     return dupheader;
}

/* 
 * Jump before the first table in the youngest span, so that the next call to
 * tab_get() will return the first table of the span.
 * Returns the absolute sequence to which jumped and will be read next
 * (tab+lastread()+1) or -1 if the ring is empty or there was an error.
 */
int   tab_jump_youngestspan(TAB_RING t		/* open ring */ )
{
     int from, to, r;
     time_t from_t, to_t;
     char *data;
     SPANS stab;

     /* get the span bounds and jump to the starting point */
     stab = spans_readblock(t->ts);
     r = spans_getlatest(stab, &from, &to, &from_t, &to_t, &data);
     table_destroy(stab);
     if ( ! r )
          return -1;
     ts_setjump(t->ts, from-1);
     return ts_lastread(t->ts)+1;
}

/* 
 * Jump before the first table in the oldest span, so that the next call to
 * tab_get() will return the first table of the span.
 * Returns the absolute sequence to which jumped and will be read next
 * (tab+lastread()+1) or -1 if the ring is empty or there was an error.
 */
int   tab_jump_oldestspan(TAB_RING t		/* open ring */ )
{
     int from, to, r;
     time_t from_t, to_t;
     char *data;
     SPANS stab;

     /* get the span bounds and jump to the starting point */
     stab = spans_readblock(t->ts);
     r = spans_getoldest(stab, &from, &to, &from_t, &to_t, &data);
     table_destroy(stab);
     if ( ! r )
          return -1;
     ts_setjump(t->ts, from-1);
     return ts_lastread(t->ts)+1;
}

/* 
 * Jump before the first table in the span containing sequence 'seq', 
 * so that the next call to tab_get() will return the first table of the span.
 * Returns the absolute sequence to which jumped and will be read next
 * (tab+lastread()+1) or -1 if the ring is empty or there was an error.
 */
int   tab_jump_seqspan(TAB_RING t,		/* open ring */
		       int seq			/* table sequence number */ )
{
     int from, to, r;
     time_t from_t, to_t;
     char *data;
     SPANS stab;

     /* get the span bounds and jump to the starting point */
     stab = spans_readblock(t->ts);
     r = spans_getseq(stab, seq, &from, &to, &from_t, &to_t, &data);
     table_destroy(stab);
     if ( ! r )
          return -1;
     ts_setjump(t->ts, from-1);
     return ts_lastread(t->ts)+1;
}


/*
 * Search through the current ring to find a datum that equals or is 
 * greater than `fromt' and position in front of it, such that the next call
 * to tab_get() will return the table. 
 * If `hintseq' is other than -1, jump to that before linear searching.
 */
void  tab_jumptime(TAB_RING ring, time_t fromt, int hintseq)
{
     time_t t;
     int s, l;

     /* jump to a reasonable point: if fromt is supplied start there, 
      * or start at the begining otherwise */
     if (fromt != -1)
	  tab_jump_seqspan(ring, hintseq);
     else
	  tab_jumpoldest(ring);

     /* search for the datum containing fromt using timestore routines
      * as they do not attempt to parse the data */
     s = -1;
     while (s < fromt)
	  ts_get(ring->ts, &l, &t, &s);

     /* back up to the datum wanted and parse it */
     tab_jump(ring, -1);
}


/*
 * Return the next table in the ring.
 * Returns a TABLE, the insertion time and its sequence number on
 * success or NULL for failure.
 */
TABLE  tab_get(TAB_RING t, time_t *retinstime, int *retseq)
{
     int retlength, from, to, r;
     time_t from_t, to_t;
     char *tabtext, *hdtext, *hdtextdup;
     TABLE tab;
     SPANS stab;

     /* get data and header */
     tabtext = tab_getraw(t, &retlength, retinstime, retseq);
     if (tabtext == NULL)
	  return NULL;
     stab = spans_readblock(t->ts);
     if (stab == NULL) {
          nfree(tabtext);
	  return NULL;
     }
	  
     r = spans_getseq(stab, *retseq, &from, &to, &from_t, &to_t, &hdtext);
     hdtextdup = xnstrdup(hdtext);
     table_destroy(stab);
     if ( ! r ) {
          nfree(tabtext);
	  return NULL;
     }

     /* convert to table */
     tab = table_create_s(hdtextdup);
     if (table_scan(tab, tabtext,"\t",TABLE_SINGLESEP, TABLE_NOCOLNAMES, 
		    TABLE_NORULER) == -1)
	  elog_printf(ERROR, "unable to scan table");
     table_freeondestroy(tab, tabtext);
     table_freeondestroy(tab, hdtextdup);

     return tab;
}



/*
 * All the tables elements in a single TAB_RING t, between the
 * sequencies `ufrom' and `uto' are concatinated together and 
 * returned in a single TABLE.
 * The sequence and time of each record's insertion is held in supplemental
 * columns names _seq and _time.
 * The header of the returned TABLE is a superset of the headers used by
 * the table elements.
 * The table should be deleted using table_destroy().
 * Returns NULL for error, empty list or if there were no tablestore.
 */
TABLE tab_mget_byseqs(TAB_RING t, int ufrom, int uto)
{
     TABLE otab;
     SPANS stab;
     int sseq, eseq;
     ITREE *data;
     char *header;
     int nrows, r;

     /* create output table */
     otab = table_create();
/*     table_addcol(otab, "_time", NULL);*/

     /* 
      * iterate over the spans that lie between and co-incide with the
      * ufrom-uto range.
      */
     stab = spans_readblock(t->ts);
     if (stab == NULL)
	  return NULL;

     table_traverse(stab) {
	  sseq   = atoi( table_getcurrentcell(stab, SPANS_FROMCOL) );
	  eseq   = atoi( table_getcurrentcell(stab, SPANS_TOCOL) );
	  header = table_getcurrentcell(stab, SPANS_DATACOL);
	  /*printf("span: %d-%d <<%s>>\n", sseq, eseq, header);*/

	  /* does span contain ufrom and to useqs completely? */
	  if ( ufrom >= sseq && ufrom < eseq &&
	       uto   <= eseq && uto   > sseq ) {
	       /* collect data from this span */
	       tab_setjump(t, ufrom-1);
	       r = tab_mgetraw(t, uto-ufrom+1, &data);
	       if (r != uto-ufrom+1)
		    elog_printf(DIAG, "mismatch of returned data [1]: "
				"%d != %d, results may not be correct", 
				r, uto-ufrom+1);
	       /* build table from single span and return */
	       nrows += tab_addtablefrom_tabnts(otab,xnstrdup(header),data);
	       table_destroy(stab);
	       tab_mgetrawfree_leavedata(data);
	       return otab;
	  }

	  /* does span contain ufrom seq? */
	  if ( ufrom >= sseq && ufrom <= eseq ) {
	       /* collect starting data from this span */
	       tab_setjump(t, ufrom-1);
	       r = tab_mgetraw(t, eseq-ufrom+1, &data);
	       if (r != eseq-ufrom+1)
		    elog_printf(DIAG, "mismatch of returned data [2]: "
				"%d != %d, results may not be correct", 
				r, sseq-ufrom+1);
	       /* build table from single span and continue */
	       nrows += tab_addtablefrom_tabnts(otab,xnstrdup(header),data);
	       tab_mgetrawfree_leavedata(data);
	       continue;
	  }

	  /* does span contain uto seq? */
	  if ( uto >= sseq && uto <= eseq ) {
	       /* collect starting data from this span */
	       tab_setjump(t, sseq-1);
	       r = tab_mgetraw(t, uto-sseq+1, &data);
	       if (r != uto-sseq+1)
		    elog_printf(DIAG, "mismatch of returned data [3]: "
				"%d != %d, results may not be correct", 
				r, uto-sseq+1);
	       /* build table from single span and continue */
	       nrows += tab_addtablefrom_tabnts(otab,xnstrdup(header),data);
	       tab_mgetrawfree_leavedata(data);
	       continue;
	  }

	  /* is span part of the body */
	  if ( ufrom < sseq && uto > eseq ) {
	       /* collect starting data from this span */
	       tab_setjump(t, sseq-1);
	       r = tab_mgetraw(t, eseq-sseq+1, &data);
	       if (r != eseq-sseq+1)
		    elog_printf(DIAG, "mismatch of returned data [4]: "
				"%d != %d, results may not be correct", 
				r, eseq-sseq+1);
	       /* build table from single span and continue */
	       nrows += tab_addtablefrom_tabnts(otab,xnstrdup(header),data);
	       tab_mgetrawfree_leavedata(data);
	       continue;
	  }
     }

     table_destroy(stab);
     return otab;
}





/*
 * All the tables in the span containing the specifed sequence are 
 * concatinated together and returned in a TABLE data type.
 * The sequence and time of each record's insertion is held in supplemental
 * columns names _seq and _time.
 * The table should be deleted using table_destroy().
 * Returns NULL for error or empty list.
 */
TABLE tab_getspanbyseq(TAB_RING t, int containseq)
{
     int from, to, i, r;	/* span parameters */
     time_t from_t, to_t;	/* span time parameters */
     char *hdtext;		/* header text */
     ITREE *dlist;		/* list of data */
     int ndata;			/* amount of data */
     ntsbuf *rec;		/* record of data, time and sequence */
     ITREE *hd;			/* header list */
     ITREE *lotshd;		/* header buffer and index */
     TABLE tab;			/* extraction result */
     ITREE *orderrow;		/* column ordered row arrays */
     ITREE *lotsrow;		/* column ordered row buffer and index */
     TREE *namedrow;		/* column named index row array */
     char *seqstr;		/* string representation of sequence */
     char *insstr;		/* string representation of insert time */
     SPANS stab;
     char *infoname;		/* info line name */

     /* Find details of the span containing the sequence */
     stab = spans_readblock(t->ts);
     r = spans_getseq(stab, containseq, &from, &to, &from_t, &to_t, &hdtext);
     hdtext = xnstrdup(hdtext);
     table_destroy(stab);
     if ( ! r ) {
	  nfree(hdtext);
          return NULL;
     }

     /* split header string into an array ordered by column.
      * the first line is the header, the remaining ones are the info lines.
      * potentially many header lines could be produced, but we only 
      * want the first one */
     util_scantext(hdtext, "\t", UTIL_MULTISEP, &lotshd);
     itree_first(lotshd);
     hd = itree_get(lotshd);

     /* create table with the first header line */
     tab = table_create_t(hd);
     if ( ! tab ) {
          util_scanfree(lotshd);
	  nfree(hdtext);
	  return NULL;
     }

     /* create named row list from header */
     namedrow = tree_create();
     tree_traverse(hd)
          tree_add(namedrow, itree_get(hd), NULL);

     /* load up info lines */
     itree_next(lotshd);
     while ( ! itree_isbeyondend(lotshd) ) {
	  /* get the scanned line, save the name and remove it */
	  hd = itree_get(lotshd);
	  itree_last(hd);
	  infoname = itree_get(hd);
	  itree_rm(hd);

	  /* add the remaining data as info columns */
	  table_addinfo_it(tab, infoname, hd);

	  itree_next(lotshd);
     }

     /* set up header list again */
     itree_first(lotshd);
     hd = itree_get(lotshd);

     /* add _seq and _time as columns at the end of the table
      * and the named row to hold the meta information sequence number
      * and date time inserted */
     table_addcol(tab, "_seq", NULL);
     table_addcol(tab, "_time", NULL);
     tree_add(namedrow, "_seq", NULL);
     tree_add(namedrow, "_time", NULL);

     /* extract data and parse, before adding to the TABLE type */
     tab_setjump(t, from-1);		/* position ourselves */
     i=from;
     while (i <= to) {
          /* get a block of table entries for efficiency */
          ndata = tab_mgetraw(t, util_min(to-from+1, TAB_MAXMGETSZ), &dlist);
          if (ndata == -1)
	       break;

	  /* process each table in turn */
	  itree_traverse(dlist) {
	      i++;
	      rec = itree_get(dlist);

	      if ( ! rec->buffer )
		   continue;

	      /* create string representatons of meta information */
	      seqstr = xnstrdup(util_i32toa(rec->seq));
	      insstr = xnstrdup(util_i32toa(rec->instime));
	      table_freeondestroy(tab, seqstr);
	      table_freeondestroy(tab, insstr);

	      /* register with the table for freeing */
	      table_freeondestroy(tab, rec->buffer);

	      /* scan table entry */
	      util_scantext(rec->buffer, "\t", UTIL_MULTISEP, &lotsrow);

	      /* convert from position indexed itree to column named tree 
	       * by joining ordered row with ordered header */
	      itree_traverse(lotsrow) {
		   orderrow = itree_get(lotsrow);
		   /* add columns */
		   itree_traverse(orderrow) {
		        itree_find(hd, itree_getkey(orderrow));
			tree_find(namedrow, itree_get(hd));
			tree_put(namedrow, itree_get(orderrow));
		   }
		   /* add meta information */
		   tree_find(namedrow, "_seq");
		   tree_put(namedrow, seqstr);
		   tree_find(namedrow, "_time");
		   tree_put(namedrow, insstr);
		   /* add row to table */
		   table_addrow_noalloc(tab, namedrow);
	      }
	      util_scanfree(lotsrow);
	  }
	  tab_mgetrawfree_leavedata(dlist);
     }

     /* clear up and return */
     tree_destroy(namedrow);
     util_scanfree(lotshd);
     table_freeondestroy(tab, hdtext);

     return tab;
}




/*
 * Get consolidated data by time.
 * The tables in the ring between the times `from' and `to' are returned in a 
 * list provided by the caller. The flattened tables entries in common spans 
 * are concatinated together into single TABLEs. The tables are added to the
 * ITREE supplied by the caller as data, the key being the start time
 * of the span.
 * The time of each record's insertion is held in the supplemental
 * column _time.
 * Returns the number of rows collected or -1 for error (such as the
 * name not being structured with a time period eg. r.aaa999).
 */
int tab_getconsbytime(ITREE *olst,	/* list into which to add data */
		      TAB_RING t, 	/* timestore ring */
		      time_t from, 	/* get data after or including from */
		      time_t to 	/* get data before or including to */ )
{
     int fromseq, toseq;	/* bounding sequencies */
     int period;		/* period between samples in seconds */
     char *periodstr;		/* period in string form */
     SPANS stab;		/* span block */
     time_t stime, etime;	/* span times */
     int sseq, eseq;		/* span sequencies */
     char *header;		/* span data (the headers) */
     int r;			/* return code */
     ITREE *data;		/* data list */
     int nrows=0;		/* accumulated number of rows */
     TABLE tab;			/* working table handle */

     /* calculate time period from ring name  */
     periodstr = t->ts->name + strcspn(t->ts->name, "0123456789");
     if (periodstr == t->ts->name) {
	  elog_printf(ERROR, "unable time find ring period");
	  return -1;
     }
     if (periodstr == NULL)
	  return -1;		/* name not structured with period */
     period = atoi(periodstr);
     if (period == 0)
	  return -1;		/* name not structured with period */

#if 0
     printf("from %lu to %lu period %d\n", from, to, period);
#endif

     /* attempt to deduce the sequence numbers from the time by hunting
      * through the available spans, then calculating numbers off that.
      * It should work most of the time, but won't when synthetically
      * created and the span time are incorrect */
     stab = spans_readblock(t->ts);

     /* find span containing or greater than the time `from' */
     r = spans_gettime(stab, from, SPANS_HUNTNEXT, 
		       &sseq, &eseq, &stime, &etime, &header);
     if (r == 0) {
	  table_destroy(stab);
	  return -1;		/* no spans found */
     }

#if 0
     printf("DEBUG - spans table\n%s--found sseq %d eseq %d stime %lu ", 
	    "etime %lu\n", table_print(stab), sseq, eseq, stime, etime);
#endif

     /* get starting sequence */
     fromseq = sseq + (from - stime) / period;
     if (fromseq < 0)
	  fromseq = 0;

     /* find span containing or less than time `to' */
     r = spans_gettime(stab, to, SPANS_HUNTPREV, 
		       &sseq, &eseq, &stime, &etime, &header);
     if (r == 0) {
	  table_destroy(stab);
	  return -1;		/* no spans found */
     }

     /* get ending sequence */
     toseq = sseq + (to - stime) / period;
/*printf("DEBUF - fromseq %d toseq %d\n", fromseq, toseq);*/

#if 0
     /* prepare the output table */
     if (tree_find(otab->data, "_time") == TREE_NOVAL)
	  table_addcol(otab, "_time", NULL);
#endif

     /* traverse the span to build the table */
     table_traverse(stab) {
	  /* convert the data */
	  sseq  = atoi(   table_getcurrentcell(stab, SPANS_FROMCOL) );
	  eseq  = atoi(   table_getcurrentcell(stab, SPANS_TOCOL) );
	  stime = strtoul(table_getcurrentcell(stab, SPANS_FROMDTCOL),NULL,0);
	  etime = strtoul(table_getcurrentcell(stab, SPANS_TODTCOL),  NULL,0);
	  header=         table_getcurrentcell(stab, SPANS_DATACOL);
#if 0
	  printf("sseq %d eseq %d stime %lu etime %lu\n", sseq, eseq, stime, 
		 etime);
#endif

	  /* does one span contain from and to times completely? */
	  if ( from >= stime && from < etime &&
	       to   <= etime && to   > stime ) {
	       /* collect data from this span */
#if 0
	       printf("using this span from %d for %d contain all\n", 
		      fromseq-1, toseq-fromseq+1);
#endif
	       tab_setjump(t, fromseq-1);
	       r = tab_mgetraw(t, toseq-fromseq+1, &data);
	       if (r != toseq-fromseq+1)
		    elog_printf(DIAG, "mismatch of returned data [1]: "
				"%d != %d (t: %d <= %d <= %d s:%d <= %d <= %d)"
				", results may not be correct",
				r, toseq-fromseq+1,
				stime, from, etime, sseq, fromseq, eseq);
	       /* build table from single span and return */
	       tab = table_create();
	       nrows += tab_addtablefrom_tabnts(tab,xnstrdup(header),data);
	       itree_add(olst, from, tab);
	       table_destroy(stab);
	       tab_mgetrawfree_leavedata(data);
	       return nrows;
	  }

	  /* does span contain from time only? */
	  if ( from >= stime && from <= etime ) {
	       /* collect starting data from this span */
#if 0
	       printf("using this span from %d for %d contain from\n", 
		      fromseq-1, eseq-fromseq+1);
#endif
	       tab_setjump(t, fromseq-1);
	       r = tab_mgetraw(t, eseq-fromseq+1, &data);
	       if (r != eseq-fromseq+1)
		    elog_printf(DIAG, "mismatch of returned data [2]: %d != %d"
				" (t: %d <= %d <= %d s:%d <= %d <= %d), "
				"result may not be correct",
				r, eseq-fromseq+1, 
				stime, from, etime, sseq, fromseq, eseq);
	       /* build table from single span and continue */
	       tab = table_create();
	       nrows += tab_addtablefrom_tabnts(tab,xnstrdup(header),data);
	       itree_add(olst, from, tab);
	       tab_mgetrawfree_leavedata(data);
	       continue;
	  }

	  /* does span contain to time only? */
	  if ( to >= stime && to <= etime ) {
	       /* collect starting data from this span */
#if 0
	       printf("using this span from %d for %d contain from\n", 
		      sseq-1, toseq-sseq+1);
#endif
	       tab_setjump(t, sseq-1);
	       r = tab_mgetraw(t, toseq-sseq+1, &data);
	       if (r != toseq-sseq+1)
		    elog_printf(ERROR, "mismatch of returned data [3]: %d != "
				"%d (t: %d <= %d <= %d s:%d <= %d <= %d), "
				"result may not be correct",
				r, toseq-sseq+1, 
				stime, from, etime, sseq, fromseq, eseq);
	       /* build table from single span and continue */
	       tab = table_create();
	       nrows += tab_addtablefrom_tabnts(tab,xnstrdup(header),data);
	       itree_add(olst, stime, tab);
	       tab_mgetrawfree_leavedata(data);
	       continue;
	  }

	  /* is span part of the body of the request? */
	  if ( from < stime && to > etime ) {
	       /* collect starting data from this span */
#if 0
	       printf("using this span from %d for %d contain from\n", 
		      sseq-1, eseq-sseq+1);
#endif
	       tab_setjump(t, sseq-1);
	       r = tab_mgetraw(t, eseq-sseq+1, &data);
	       if (r != eseq-sseq+1)
		    elog_printf(DIAG, "mismatch of returned data [4]: %d != "
				"%d (t: %d <= %d <= %d s:%d <= %d <= %d), "
				"result may not be correct",
				r, eseq-sseq+1,
				stime, from, etime, sseq, fromseq, eseq);

	       /* build table from single span and continue */
	       tab = table_create();
	       nrows += tab_addtablefrom_tabnts(tab,xnstrdup(header),data);
	       itree_add(olst, stime, tab);
	       tab_mgetrawfree_leavedata(data);
	       continue;
	  }
     }

     table_destroy(stab);
     return nrows;
}





/*
 * Add a table of data represented by a list of raw nts records from 
 * tablestore to an existing TABLE data entity.
 * The column names of the list are represented with a parsable string 
 * `header'. The buffer element of each ntsbuf struct in the list `ndata'
 * is a scannable string representation of a row. Each scanned token
 * of the row is a cell that corresponds with the header row/column names.
 * The header may be obtained from span data and can include info lines, 
 * the ntsbuf list from tab_mgetraw().
 * The header string and the buffer of each ntsbuf struct will be added 
 * to the garbage list of the table for clearing up; the caller is freed
 * of responsibility for the freeing of header and should clearup ndata
 * with the call: tab_mgetrawfree_leavedata().
 * Existing columns in `t' that are not indexed in `header' will be set 
 * to NULL for all rows added.
 * Returns the number of rows added or -1 for an error.
 */
int tab_addtablefrom_tabnts(TABLE t, char *header, ITREE *ndata)
{
     ntsbuf *rec;		/* nts data structure */
     ITREE *dataorder;		/* ordered list of columns */
     int nrows=0;
     char *seqstr, *insstr;	/* meta info strings */
     ITREE *orderrow;		/* column ordered row arrays */
     ITREE *lotsrow;		/* column ordered row buffer and index */
     ITREE *lotshd;		/* list of header rows */
     ITREE *hd;			/* list of header cells */
     char *infoname;		/* name of info row */

     if (t == NULL || header == NULL || ndata == NULL)
	  return 0;

     /* split header string into an array ordered by column.
      * the first line is the header, the remaining ones are the info lines.
      * potentially many header lines could be produced, but we only 
      * want the first one */
     util_scantext(header, "\t", UTIL_MULTISEP, &lotshd);
     itree_first(lotshd);
     dataorder = itree_get(lotshd);

     /* compile data order and add additional column names from string */
     table_freeondestroy(t, header);
     itree_traverse(dataorder)
	  if (tree_find(t->data, itree_get(dataorder)) == TREE_NOVAL)
	       table_addcol(t, itree_get(dataorder), NULL);

     /* load up info lines */
     itree_next(lotshd);
     while ( ! itree_isbeyondend(lotshd) ) {
	  /* get the scanned info line, save the info name from the end
	   * and remove the word, leaving just the vlaues */
	  hd = itree_get(lotshd);
	  itree_last(hd);
	  infoname = itree_get(hd);
	  itree_rm(hd);

	  /* if the info row does not exist: add it */
	  if (tree_find(t->infolookup, infoname) == TREE_NOVAL)
	       table_addemptyinfo(t, infoname);

	  /* fill in info cells where none exists before; don't overwrite
	   * existing values */
	  itree_first(hd);
	  itree_traverse(dataorder) {
	       if (table_getinfocell(t, infoname, 
				     itree_get(dataorder)) == NULL) {
		    table_replaceinfocell(t, infoname, itree_get(dataorder), 
					  itree_get(hd));
	       }
	       itree_next(hd);
	  }
	  itree_next(lotshd);
     }

     /* add insertion time (_time) into table, sequence is meaningless
      * when rings are consolidated */
     if (tree_find(t->data, "_time") == TREE_NOVAL)
	  table_addcol(t, "_time", NULL);

     /* traverse the ntsbuf list */
     itree_traverse(ndata) {
	  rec = itree_get(ndata);

	  /* create string representatons of meta information */
	  seqstr = xnstrdup(util_i32toa(rec->seq));
	  insstr = xnstrdup(util_i32toa(rec->instime));
	  table_freeondestroy(t, seqstr);
	  table_freeondestroy(t, insstr);

	  /* register data with the table for freeing */
	  table_freeondestroy(t, rec->buffer);

	  /* scan raw table entry */
	  util_scantext(rec->buffer, "\t", UTIL_MULTISEP, &lotsrow);

	  itree_traverse(lotsrow) {
	       orderrow = itree_get(lotsrow);

	       /* add columns */
	       table_addemptyrow(t);
	       nrows++;
	       itree_first(orderrow);
	       itree_traverse(dataorder) {
		    /* add partial/sparse row to table */
		    table_replacecurrentcell(t, itree_get(dataorder), 
					     itree_get(orderrow));
		    itree_next(orderrow);
	       }

	       /* stamp with time */
	       table_replacecurrentcell(t, "_time", insstr);
	  }

	  util_scanfree(lotsrow);
     }

     return nrows;
}





#if TEST

#include <sys/time.h>
#include <stdio.h>

#define TEST_TABFILE1 "t.tab.1.dat"
#define TEST_RING1 "ring1"
#define TEST_RING2 "ring2"
#define TEST_RING3 "ring3"
#define TEST_RING4 "ring4"
#define TEST_RING5 "ring5"
#define TEST_RING6 "ring6"
#define TEST_BUFLEN 50
#define TEST_ITER 1000
#define TEST_HEAD1 "c1\tc2\tc3\tc4"
#define TEST_HEAD1a "0\t0\t%ld\t%ld\t\"c1\tc2\tc3\tc4\002int\tint\tstr\tstr\ttype\"\n"
#define TEST_HEAD1b "0\t1\t%ld\t%ld\t\"c1\tc2\tc3\tc4\002int\tint\tstr\tstr\ttype\"\n"
#define TEST_HEAD2 "c1\tc2\tc3"
#define TEST_HEAD2a "4\t13\t%ld\t%ld\t\"c1\tc2\tc3\"\n"
#define TEST_HEAD2b "0\t99\t%d\t%d\t\"c1\tc2\tc3\"\n"
#define TEST_HEAD2c "0\t11\t%d\t%d\t\"c1\tc2\tc3\"\n"
#define TEST_HEAD2d "0\t31\t%d\t%d\t\"c1\tc2\tc3\"\n"
#define TEST_HEAD2e "0\t1\t%d\t%d\t\"c1\tc2\tc3\"\n"
#define TEST_HEAD3 "sharon karren marrion\nint int str type"
#define TEST_HEAD4 "mary mungo and midge\tHector's house\tTeletubbies\tThe three bears"
#define TEST_TAB1 "c1\tc2\tc3\tc4\nint\tint\tstr\tstr\ttype\n---\t---\t---\t---\nmary\thector\ttinky winky\tdaddy bear\nmungo\tzaza\tdipsy\tmummy\nmige\tkiki\tla la\tbaby"
#define TEST_TAB2 "c1\tc2\tc3\tc4\nint\tint\tstr\tstr\ttype\n---\t---\t---\t---\nfamily 1\tfamily2\tfamily3\t\"family\t4\""
#define TEST_TAB3 "c1\tc2\tc3\n--\t--\t--\none\tfour\tseven\ntwo\tfive\teight\nthree\tsix\tnine\n\n"
int main()
{
     TAB_RING ring1, ring2, ring3, ring4, ring5;
     rtinf err;
     int r, r2, i, len1, len2, len3, seq1, seq2, seq3;
     char *dat1, *dat2, *dat3, *tmp;
     time_t time1, time2, time3, now;
     char datbuf1[TEST_BUFLEN], str[200];
     TREE *lst1;
     ITREE *lst2;
     ntsbuf *mgetdat;
     int nrings, nslots, nread, navail;
     TABLE tab1, tab2;

     route_init("stderr", 0, NULL);
     err = route_open("stderr", NULL, NULL, 0);
     elog_init(err, 0, "tablestore test", NULL);
     tab_init(err, 1);

     unlink(TEST_TABFILE1);
     now = time(NULL);

     /* test 1: creating and opening rings, checking all the failure modes */
     route_printf(err, "[1] expect an error --> ");
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     if (ring1)
	  route_die(err, "[1] Shouldn't have opened ring\n");
     ring1 = tab_create(TEST_TABFILE1, 0644, TEST_RING1, 
			"This is the first test", NULL, 10);
     if (!ring1)
	  route_die(err, "[1] Unable to create ring\n");
     dat1 = hol_get(ring1->ts->hol, TS_SUPERNAME, &len1);
     if (!dat1)
	  route_die(err, "[1] Timestore superblock not created\n");
     nfree(dat1);
     route_printf(err, "[1] expect another error --> ");
     ring2 = tab_create(TEST_TABFILE1, 0644, TEST_RING1, 
			"This is the first test", NULL, 10);
     if (ring2)
	  route_die(err, "[1] Shouldn't be able to create a second time\n");
     tab_close(ring1);
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[1] Unable to open existing ring\n");
     ring2 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     if (!ring2)
	  route_die(err, "[1] Unable to again open existing ring\n");
     tab_close(ring2);
     tab_close(ring1);

     /* test 2: removing rings */
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[2] Unable to open existing ring\n");
     if ( ! tab_rm(ring1))
	  route_die(err, "[2] Unable to remove existing ring\n");
     route_printf(err, "[2] expect a further error --> ");
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     if (ring1)
	  route_die(err, "[2] Shouldn't have opened removed ring\n");

     /* test 3: rings at quantity */
     ring1 = tab_create(TEST_TABFILE1, 0644, TEST_RING1, 
			"This is the first test", NULL, 10);
     if (!ring1)
	  route_die(err, "[3] Unable to create ring 1\n");
     ring2 = tab_create(TEST_TABFILE1, 0644, TEST_RING2, 
			"This is the second test", NULL, 0);
     if (!ring2)
	  route_die(err, "[3] Unable to create ring 2\n");
     ring3 = tab_create(TEST_TABFILE1, 0644, TEST_RING3, 
			"This is the third test", NULL, 100);
     if (!ring3)
	  route_die(err, "[3] Unable to create ring 3\n");
     ring4 = tab_create(TEST_TABFILE1, 0644, TEST_RING4,
			"This is the forth test", NULL, 100);
     if (!ring4)
	  route_die(err, "[3] Unable to create ring 4\n");
     ring5 = tab_create(TEST_TABFILE1, 0644, TEST_RING5,
			"This is the fifth test", NULL, 17);
     if (!ring5)
	  route_die(err, "[3] Unable to create ring 5\n");
     /* now delete them all */
     if ( ! tab_rm(ring1) )
	  route_die(err, "[3] Unable to delete ring 1\n");
     if ( ! tab_rm(ring2) )
	  route_die(err, "[3] Unable to delete ring 2\n");
     if ( ! tab_rm(ring3) )
	  route_die(err, "[3] Unable to delete ring 3\n");
     if ( ! tab_rm(ring4) )
	  route_die(err, "[3] Unable to delete ring 4\n");
     if ( ! tab_rm(ring5) )
	  route_die(err, "[3] Unable to delete ring 5\n");

     /* test 4: put and get data on a ring */
     ring1 = tab_create(TEST_TABFILE1, 0644, TEST_RING1,
			"4th test: get/set rings", NULL, 10);
     if (!ring1)
	  route_die(err, "[4] Unable to create ring\n");
     if ( tab_puttext(ring1, TEST_TAB1) == -1)
	  route_die(err, "[4] Unable to put table 1\n");
     tab1 = tab_get(ring1, &time1, &seq1);
     if ( ! tab1)
	  route_die(err, "[4] Unable to get table 1\n");
     tab2 = table_create();
     tmp = xnstrdup(TEST_TAB1);
     table_scan(tab2, tmp, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab2, tmp);
     dat1 = table_print(tab1);
     dat2 = table_print(tab2);
     if (strcmp(dat1, dat2))
	  route_die(err, "[4] table 1 incorrectly read\n");
     nfree(dat1);
     nfree(dat2);
     table_destroy(tab1);
     table_destroy(tab2);
     dat1 = hol_get(ring1->ts->hol, SPANSTORE_DATASPACE "ring1", &len1);
     if ( ! dat1)
	  route_die(err, "[4] Unable to get span of table 1\n");
     r = sprintf(str, TEST_HEAD1a, time1, time1);
     if (strncmp(dat1, str, len1))
	  route_die(err, "[4] Span of table 1 incorrectly read\n");
     if (r+1 != len1)
	  route_die(err, "[4] Length of table 1 span mismatch (%d != %d)\n",
		    strlen(TEST_HEAD1a)+1, len1);
     nfree(dat1);
     if ( tab_puttext(ring1, TEST_TAB2) == -1 )
	  route_die(err, "[4] Unable to put table 2\n");
     tab1 = tab_get(ring1, &time1, &seq1);
     if ( ! tab1)
	  route_die(err, "[4] Unable to get table 2\n");
     tab2 = table_create();
     tmp = xnstrdup(TEST_TAB2);
     table_scan(tab2, tmp, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab2, tmp);
     dat1 = table_print(tab1);
     dat2 = table_print(tab2);
     if (strcmp(dat1, dat2))
	  route_die(err, "[4] table 2 incorrectly read\n");
     nfree(dat1);
     nfree(dat2);
     table_destroy(tab1);
     table_destroy(tab2);
     dat1 = hol_get(ring1->ts->hol, SPANSTORE_DATASPACE "ring1", &len1);
     if ( ! dat1)
	  route_die(err, "[4] Unable to get span of tables 1+2\n");
     r = sprintf(str, TEST_HEAD1b, time1, time1);
     if (strncmp(dat1, str, len1))
	  route_die(err, "[4] Span of tables 1+2 incorrectly read\n");
     if (r+1 != len1)
	  route_die(err, "[4] Length of tables 1+2 span mismatch\n");
     nfree(dat1);
     tab_close(ring1);

		/* open and read again */
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[4b] Unable to reopen existing ring\n");
     tab1 = tab_get(ring1, &time1, &seq1);
     if ( ! tab1)
	  route_die(err, "[4b] Unable to reget table 1\n");
     tab2 = table_create();
     tmp = xnstrdup(TEST_TAB1);
     table_scan(tab2, tmp, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab2, tmp);
     dat1 = table_print(tab1);
     dat2 = table_print(tab2);
     if (strcmp(dat1, dat2))
	  route_die(err, "[4b] reobtained table 1 incorrectly read (got %s)\n",
		    dat1);
     nfree(dat1);
     nfree(dat2);
     table_destroy(tab1);
     table_destroy(tab2);
     tab1 = tab_get(ring1, &time1, &seq1);
     if ( ! tab1)
	  route_die(err, "[4b] unable to reget table 2\n");
     tab2 = table_create();
     tmp = xnstrdup(TEST_TAB2);
     table_scan(tab2, tmp, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab2, tmp);
     dat1 = table_print(tab1);
     dat2 = table_print(tab2);
     if (strcmp(dat1, dat2))
	  route_die(err, "[4b] reobtained table 2 incorrectly read (got %s)\n",
		    dat1);
     nfree(dat1);
     nfree(dat2);
     table_destroy(tab1);
     table_destroy(tab2);
     tab_close(ring1);

     /* test 5: put and get several to test ring rollover */
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     now = time(NULL);
     for (i=3; i<15; i++) {
	  /* write 12 tables */
	  r = sprintf(datbuf1, "c1\tc2\tc3\n--\t--\t--\ntable%d\ttable%d\ttable%d",
		      i, i, i);
	  if ( (r2 = tab_puttext(ring1, datbuf1)) == -1 )
	       route_die(err, "[5] Unable to put table %d\n", i);
	  if (r2 != i-1)
	       route_die(err,"[5] tab_put() returns %d for table %d, should "
			 "be %d\n", r2, i, i-1);
     }
     /* a couple should be lost, leaving a ring that runs from seq 4-13 */
     dat1 = hol_get(ring1->ts->hol,SPANSTORE_DATASPACE "ring1", &len1);
     if ( ! dat1)
	  route_die(err, "[5] Unable to get span of tables\n");
     r = sprintf(str, TEST_HEAD2a, now, now);
     if (strncmp(dat1, str, len1)) {
	  /* try again in case we are caught on a time boundary */
	  r = sprintf(str, TEST_HEAD2a, now, now+1);
	  if (strncmp(dat1, str, len1)) {
	       /* and again... */
	       r = sprintf(str, TEST_HEAD2a, now+1, now+1);
	       if (strncmp(dat1, str, len1))
		    route_die(err, "[5] Span incorrectly read\n");
	  }
     }
     if (r+1 != len1)
	  route_die(err, "[5] Length of span mismatch\n");
     nfree(dat1);
     for (i=5; i<15; i++) {
	  /* read last 10 tables */
	  r = sprintf(datbuf1, "table%d\ttable%d\ttable%d\n", i, i, i);
	  dat1 = tab_getraw(ring1, &len1, &time1, &seq1);
	  if ( ! dat1)
	       route_die(err, "[5] Unable to get table %d\n", i);
	  if (r +1 != len1)
	       route_die(err, "[5] Incorrect length of table %d\n", i);
	  if (strncmp(dat1, datbuf1, len1))
	       route_die(err, "[5] table %d incorrectly read\n", i);
	  nfree(dat1);
	  if (time1 < time(NULL) - 5 || time1 > time(NULL))
	       route_die(err, "[5] table %d wrong time %d %d\n", i, time1, 
			 time(NULL));
     }
     tab_close(ring1);

     /* test 6: mget */
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[6] Unable to reopen existing ring\n");
     r = tab_mgetraw(ring1, 20, &lst2);
     if (r != 10)
	  route_die(err, "[6] %d returned, should be 10\n", r);
     itree_traverse(lst2) {
	  /* check the 10 tables returned by mget */
	  mgetdat = itree_get(lst2);
	  r = sprintf(datbuf1, "table%d\ttable%d\ttable%d\n",
		      itree_getkey(lst2)+1, itree_getkey(lst2)+1, 
		      itree_getkey(lst2)+1);
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
	  if (strcmp(mgetdat->spantext, TEST_HEAD2))
	       route_die(err, "[6] sequence %d span header incorrect\n", 
			 mgetdat->seq);
     }
     tab_mgetrawfree(lst2);
     tab_close(ring1);

     /* test 7: jump */
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[7] Unable to reopen existing ring\n");
     if (tab_lastread(ring1) != -1)
	  route_die(err, "[7] lastread not -1 at begining\n");
     tab_jumpoldest(ring1);
     if (tab_lastread(ring1) != 3)
	  route_die(err, "[7] lastread %d not 3 at oldest\n",
		    tab_lastread(ring1));
     tab_jumpyoungest(ring1);
     if (tab_lastread(ring1) != 13)
	  route_die(err, "[7] lastread %d not 13 at youngest\n", 
		    tab_lastread(ring1));
     tab_jump(ring1, 4);
     if (tab_lastread(ring1) != 13)
	  route_die(err, "[7] lastread %d not 13 after overjump 1\n", 
		    tab_lastread(ring1));
     tab_setjump(ring1, 11);
     if (tab_lastread(ring1) != 11)
	  route_die(err, "[7] lastread %d not 11 after set 1\n", 
		    tab_lastread(ring1));
     tab_jump(ring1, 4);
     if (tab_lastread(ring1) != 13)
	  route_die(err, "[7] lastread %d not 13 after overjump 2\n", 
		    tab_lastread(ring1));
     tab_jump(ring1, -40);
     if (tab_lastread(ring1) != 3)
	  route_die(err, "[7] lastread %d not 3 after underjump 1\n", 
		    tab_lastread(ring1));
     tab_close(ring1);

     /* test 8 multiple rings */
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     if (!ring1)
	  route_die(err, "[8] Unable to reopen existing ring\n");
     ring2 = tab_create(TEST_TABFILE1, 0644, TEST_RING2, "Second ring", 
			NULL, 100);
     ring3 = tab_create(TEST_TABFILE1, 0644, TEST_RING3, "Third ring", 
			NULL, 12);
     ring4 = tab_create(TEST_TABFILE1, 0644, TEST_RING4, "Forth ring", 
			NULL, 32);
     if ( ! (ring2 || ring3 || ring4))
	  route_die(err, "[8] Unable to create rings 2-4\n");
     for (i=0; i<100; i++) {
	  r = sprintf(datbuf1, "c1\tc2\tc3\n--\t--\t--\ntable%d\ttable%d\t"
		      "table%d", i, i, i);
	  if (tab_puttext(ring2, datbuf1) == -1)
	       route_die(err, "[8] Unable to put table %d on ring2\n", i);
     }
     for (i=0; i<12; i++) {
	  r = sprintf(datbuf1, "c1\tc2\tc3\n--\t--\t--\ntable%d\ttable%d\t"
		      "table%d", i, i, i);
	  if (tab_puttext(ring3, datbuf1) == -1)
	       route_die(err, "[8] Unable to put table %d on ring3\n", i);
     }
     for (i=0; i<32; i++) {
	  r = sprintf(datbuf1, "c1\tc2\tc3\n--\t--\t--\ntable%d\ttable%d\t"
		      "table%d", i, i, i);
	  if (tab_puttext(ring4, datbuf1) == -1)
	       route_die(err, "[8] Unable to put table %d on ring4\n", i);
     }
     tab_jumpoldest(ring2);
     tab_jumpoldest(ring3);
     tab_jumpoldest(ring4);
     if ((r = tab_jumpyoungest(ring2)) != 100)
	  route_die(err, "[8] wrong young value ring2 %d [seq %d] not 100\n", 
		    r, tab_lastread(ring2));
     if ((r = tab_jumpyoungest(ring3)) != 12)
	  route_die(err, "[8] wrong young value ring3 %d [seq %d] not 12\n",
		    r, tab_lastread(ring3));
     if ((r = tab_jumpyoungest(ring4)) != 32)
	  route_die(err, "[8] wrong young value ring4 %d [seq %d] not 32\n",
		    r, tab_lastread(ring4));
     tab_setjump(ring3, 4);
     tab_setjump(ring2, 45);
     tab_setjump(ring4, 24);
     if ((dat1 = tab_getraw(ring2, &len1, &time1, &seq1)) == NULL)
	  route_die(err, "[8] Cant get 45 record\n");
     if ((dat2 = tab_getraw(ring3, &len2, &time2, &seq2)) == NULL)
	  route_die(err, "[8] Cant get 4 record\n");
     if ((dat3 = tab_getraw(ring4, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[8] Cant get 24 record\n");
     if (seq1 != 46)
	  route_die(err, "[8] ring2 %d != 46\n", seq1);
     if (seq2 != 5)
	  route_die(err, "[8] ring3 %d != 5\n", seq2);
     if (seq3 != 25)
	  route_die(err, "[8] ring4 %d != 25\n", seq3);
     if (strcmp(dat1, "table46\ttable46\ttable46\n"))
	  route_die(err, "[8] ring2 text not the same: %s\n", dat1);
     if (strcmp(dat2, "table5\ttable5\ttable5\n"))
	  route_die(err, "[8] ring3 text not the same: %s\n", dat2);
     if (strcmp(dat3, "table25\ttable25\ttable25\n"))
	  route_die(err, "[8] ring4 text not the same: %s\n", dat3);
     nfree(dat1);
     nfree(dat2);
     nfree(dat3);

     /* test 9: stats */
     				/* current state of ring 4 */
     if ( ! tab_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
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
     r = tab_jumpoldest(ring4);
     if (r != -26)
	  route_die(err, "[9b] should jumpoldest -25, not %d\n", r);
     if ( ! tab_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
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
     if ((dat3 = tab_getraw(ring4, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[9c] Cant get 1st record\n");
     if ( ! tab_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
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
     if ((dat3 = tab_getraw(ring4, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[9d] Cant get 2nd record\n");
     if ( ! tab_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
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
     r = tab_jumpyoungest(ring4);
     if (r != 30)
	  route_die(err, "[9e] should jumpyoungest +30, not %d\n", r);
     if ( ! tab_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
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
     r = tab_jump(ring4, -1);
     if (r != -1)
	  route_die(err, "[9f] should jump -1, not %d\n", r);
     if ( ! tab_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
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
     r = tab_jump(ring4, -1);
     if (r != -1)
	  route_die(err, "[9g] should jump -1, not %d\n", r);
     if ( ! tab_tell(ring4, &nrings, &nslots, &nread, &navail, &dat1))
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
     ring5 = tab_create(TEST_TABFILE1,0644,TEST_RING5, "Fifth ring", NULL, 5);
     if ( ! ring5)
	  route_die(err, "[9h] unable to create fifth ring\n");
     if ( ! tab_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
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
     if (tab_puttext(ring5, "c1\tc2\tc3\n--\t--\t--\nbollocks\tbum\t"
		     "tiddle") == -1)
	  route_die(err, "[9i] Unable to put table 1 on ring5\n");
     if ( ! tab_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9i] unable to stat\n");
     if (nslots != 5)
	  route_die(err, "[9i] should be 5 slots, not %d\n", nslots);
     if (nread != 0)
	  route_die(err, "[9i] should be 0 read, not %d\n", nread);
     if (navail != 1)
	  route_die(err, "[9i] should be 1 available, not %d\n", navail);
     nfree(dat1);
			     /* double record ring */
     if (tab_puttext(ring5, "c1\tc2\tc3\n--\t--\t--\nbattersea\tchelsea\t"
		     "worthing-by-sea") == -1)
	  route_die(err, "[9j] Unable to put table 2 on ring5\n");
     if ( ! tab_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9j] unable to stat\n");
     if (nslots != 5)
	  route_die(err, "[9j] should be 5 slots, not %d\n", nslots);
     if (nread != 0)
	  route_die(err, "[9j] should be 0 read, not %d\n", nread);
     if (navail != 2)
	  route_die(err, "[9j] should be 2 available, not %d\n", navail);
     nfree(dat1);
     				/* read first record */
     if ((dat3 = tab_getraw(ring5, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[9k] Cant get 1st record\n");
     if ( ! tab_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
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
     if ((dat3 = tab_getraw(ring5, &len3, &time3, &seq3)) == NULL)
	  route_die(err, "[9l] Cant get 2nd record\n");
     if ( ! tab_tell(ring5, &nrings, &nslots, &nread, &navail, &dat1))
	  route_die(err, "[9l] unable to stat\n");
     if (nslots != 5)
	  route_die(err, "[9l] should be 5 slots, not %d\n", nslots);
     if (nread != 2)
	  route_die(err, "[9l] should be 2 read, not %d\n", nread);
     if (navail != 0)
	  route_die(err, "[9l] should be 0 available, not %d\n", navail);
     nfree(dat1);
     nfree(dat3);

     /* test 10: check of resulting spans */
     dat1 = hol_get(ring1->ts->hol,SPANSTORE_DATASPACE "ring1", &len1);
     if ( ! dat1)
	  route_die(err, "[10r1] Unable to get span of tables\n");
     sscanf(dat1, "%d %d", &seq1, &seq2);
     if (seq1 != 4 || seq2 != 13)
	  route_die(err, "[10r1] Length of span mismatch\n");
     nfree(dat1);
     dat1 = hol_get(ring2->ts->hol,SPANSTORE_DATASPACE "ring2", &len1);
     if ( ! dat1)
	  route_die(err, "[10r2] Unable to get span of tables\n");
     sscanf(dat1, "%d %d", &seq1, &seq2);
     if (seq1 != 0 || seq2 != 99)
	  route_die(err, "[10r2] Length of span mismatch\n");
     nfree(dat1);
     dat1 = hol_get(ring3->ts->hol,SPANSTORE_DATASPACE "ring3", &len1);
     if ( ! dat1)
	  route_die(err, "[10r3] Unable to get span of tables\n");
     sscanf(dat1, "%d %d", &seq1, &seq2);
     if (seq1 != 0 || seq2 != 11)
	  route_die(err, "[10r3] Length of span mismatch\n");
     nfree(dat1);
     dat1 = hol_get(ring4->ts->hol,SPANSTORE_DATASPACE "ring4", &len1);
     if ( ! dat1)
	  route_die(err, "[10r4] Unable to get span of tables\n");
     sscanf(dat1, "%d %d", &seq1, &seq2);
     if (seq1 != 0 || seq2 != 31)
	  route_die(err, "[10r4] Length of span mismatch\n");
     nfree(dat1);
     dat1 = hol_get(ring5->ts->hol,SPANSTORE_DATASPACE "ring5", &len1);
     if ( ! dat1)
	  route_die(err, "[10r5] Unable to get span of tables\n");
     sscanf(dat1, "%d %d", &seq1, &seq2);
     if (seq1 != 0 || seq2 != 1)
	  route_die(err, "[10r5] Length of span mismatch\n");
     nfree(dat1);

     tab_close(ring1);
     tab_close(ring2);
     tab_close(ring3);
     tab_close(ring4);
     tab_close(ring5);

     /* test 11: prealloc */

     /* test 12: resize */

     /* test 13: lsrings */
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     lst1 = tab_lsrings(ring1);
#if 0
     tree_traverse(lst1)
	  route_printf(err,"[13] %s    %s\n",tree_getkey(lst1),tree_get(lst1));
#endif
     if (tree_find(lst1, TEST_RING1) == TREE_NOVAL)
	  route_die(err, "[13a] Unable to find %s\n", TEST_RING1);
     if (tree_find(lst1, TEST_RING2) == TREE_NOVAL)
	  route_die(err, "[13b] Unable to find %s\n", TEST_RING2);
     if (tree_find(lst1, TEST_RING3) == TREE_NOVAL)
	  route_die(err, "[13c] Unable to find %s\n", TEST_RING3);
     if (tree_find(lst1, TEST_RING4) == TREE_NOVAL)
	  route_die(err, "[13d] Unable to find %s\n", TEST_RING4);
     tab_freelsrings(lst1);
     tab_close(ring1);

     /* test 14: check headers */
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     dat1 = tab_getheader_latest(ring1);
     if ( ! dat1 )
          route_die(err, "[14a] null returned\n");
     if (strcmp(dat1, TEST_HEAD2))
          route_die(err, "[14a] wrong header returned: %s (havn't written anything yet)\n", dat1);
     nfree(dat1);
     if (tab_puttext(ring1, "c1\tc2\tc3\tc4\n--\t--\t--\t--\n"
		     "try\tthis\ton\tbaby")==-1)
          route_die(err, "[14b] cant put\n");
     dat1 = tab_getheader_latest(ring1);
     if ( ! dat1 )
          route_die(err, "[14b] null returned\n");
     if (strcmp(dat1, TEST_HEAD1))
          route_die(err, "[14b] wrong header returned: %s\n", dat1);
     nfree(dat1);
     dat1 = tab_getheader_seq(ring1, 7);
     if ( ! dat1 )
          route_die(err, "[14c] null returned\n");
     if (strcmp(dat1, TEST_HEAD2))
          route_die(err, "[14c] wrong header returned: %s\n", dat1);
     nfree(dat1);
     dat1 = tab_getheader_seq(ring1, 14);
     if ( ! dat1 )
          route_die(err, "[14d] null returned\n");
     if (strcmp(dat1, TEST_HEAD1))
          route_die(err, "[14d] wrong header returned: %s\n", dat1);
     nfree(dat1);
     tab_close(ring1);

     /* test 15: span jumping */
     ring1 = tab_open(TEST_TABFILE1, TEST_RING1, NULL);
     r = tab_jump_youngestspan(ring1);
     if (r != 14)
          route_die(err, "[15] seq (%d) not at latest span\n", r);
     r = tab_jump_seqspan(ring1, 7);
     if (r != 5)
          route_die(err, "[15] seq (%d) not at specified span\n", r);
     tab_close(ring1);

     /* finalise */
     tab_fini();
     elog_fini();
     route_close(err);
     route_fini();
     printf("tests finished successfully\n");
     exit(0);
}

#endif /* TEST */
