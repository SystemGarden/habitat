/*
 * Span Store
 * Associates a contiguous run of timestore elements in a single
 * ring with a single information element that covers the series.
 *
 * Span is implemented on Holstore, and creates a single element per
 * ring using a key of the form
 *
 *    SPANSTORE_DATASPACE <ringname>
 *
 * Thus there is a one to one mapping with the timestore superblock.
 * Over time, this may be integrated into that superblock for fewer reads
 * in the holstore system overall.
 *
 * Inside the span datum is a table thus:-
 *
 *    <fromseq1> <toseq1> <fromdate1> <todate1> <headers1> \n
 *    <fromseq2> <toseq2> <fromdate2> <todate2> <headers2> \n
 *    <fromseq3> <toseq3> <fromdate3> <todate3> <headers3> \n
 *    ... etc
 *
 * which assoociates the data <headersN> with runs of sequencies.
 * Once neither <fromseq1> or <toseq1> are present in the timestore,
 * the span record may be deleted. Strictly, the <fromseqN> should be
 * updated when the sequence it refers to is removed; however, it won't
 * be a disaster.
 * <fromdateN> and <todateN> are copies of the timestamps from those
 * sequencies.
 * Although it is designed to refer to timestores, it does not need
 * need them to exist in order to operate.
 *
 * Nigel Stuckey, February & March 1999
 * Major overhall August 1999
 * Ammended November 1999
 * Copyright System Garden Ltd 1999-2001. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include "elog.h"
#include "itree.h"
#include "tree.h"
#include "holstore.h"
#include "timestore.h"
#include "spanstore.h"
#include "nmalloc.h"
#include "table.h"
#include "util.h"

char *spans_block_schema[]  = {SPANS_FROMCOL, SPANS_TOCOL, SPANS_FROMDTCOL, 
			       SPANS_TODTCOL, SPANS_DATACOL, NULL};
/*char *spans_lsspan_schema[] = {"from", "to", "header", NULL};*/
char *spans_allrings_cols[] = {"ring", "from", "time start", "to", 
			       "time end", "header", NULL};

/*
 * Reads the underlying block for the specifed timestore ring and
 * places the information into a table that is returned.
 * If the block does not exist, NULL is returned.
 */
SPANS spans_readblock(TS_RING ts	/* ring reference */ )
{
     /* need to be revised with new table */
     char key[SPANSTORE_KEYLEN];
     char *data;
     int datalen;
     TABLE tab;

     /* fetch the block */
     snprintf(key, SPANSTORE_KEYLEN, "%s%s", SPANSTORE_DATASPACE,
	      ts_name(ts));
     hol_begintrans(ts_holstore(ts), 'r');
     data = hol_get(ts_holstore(ts), key, &datalen);
     hol_endtrans(ts_holstore(ts));
     if ( ! data )
          return NULL;

     /* read into table */
     tab = table_create_a(spans_block_schema);
     table_scan(tab, data, "\t", TABLE_NOCOLNAMES, TABLE_NORULER);
     table_freeondestroy(tab, data);
     return tab;
}


/*
 * Writes a spanstore block using data from the given table
 * and associates it with a timestore.
 * Return 1 for success or 0 for error.
 */
int spans_writeblock(TS_RING ts,	/* ring reference */
		     SPANS tab		/* spanstore data */ )
{
     /* need to be revised with new table */
     char key[SPANSTORE_KEYLEN];
     char *data;
     int r;

     /* write the data */
     data = table_outbody(tab);
     if ( ! data )
	  return 0;

     /* write the block */
     snprintf(key, SPANSTORE_KEYLEN, "%s%s", SPANSTORE_DATASPACE,
	      ts_name(ts));
     hol_begintrans(ts_holstore(ts), 'w');
     r = hol_put(ts_holstore(ts), key, data, strlen(data)+1);
     hol_commit(ts_holstore(ts));
     nfree(data);

     return r;
}



/*
 * Create a new span in the provided table, which stores a string and
 * associates it between and including two sequencies, from and to, which
 * may be equal to indicate a span of one element.
 * The times of span limits should be included: time of first element in span
 * and the time of the last.
 * If no table or spans exist for the ring in question, then pass
 * an empty table and it will be formatted and the new span entered.
 * You will be prevented from overlapping existing spans.
 * Returning 1 for success or 0 for failure to write data
 * The table should be written back to the holstore using 
 * spans_writeblock() after this extension.
 * Data is copied internally by spanstore and freed on the destruction
 * of the span table.
 */

int spans_new(SPANS tab,	/* span table for ring */
	      int from,		/* new span starting sequence */
	      int to,		/* new span ending sequence */
	      time_t fromdt,	/* new span starting time */
	      time_t todt,	/* new span ending time */
	      char *data	/* new data */ )
{
     if (tab->ncols == 0) {
	  /* new table, so format columns */
	  table_addcol(tab, SPANS_FROMCOL,   NULL);
	  table_addcol(tab, SPANS_TOCOL,     NULL);
	  table_addcol(tab, SPANS_FROMDTCOL, NULL);
	  table_addcol(tab, SPANS_TODTCOL,   NULL);
	  table_addcol(tab, SPANS_DATACOL,   NULL);
     } else 
	  /* check overlaps for proposed new sequence */
	  if ( spans_overlap(tab, from, to) )
	       return 0;

     /* append a new row */
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, SPANS_FROMCOL,   util_i32toa(from  ));
     table_replacecurrentcell_alloc(tab, SPANS_TOCOL,     util_i32toa(to    ));
     table_replacecurrentcell_alloc(tab, SPANS_FROMDTCOL, util_u32toa(fromdt));
     table_replacecurrentcell_alloc(tab, SPANS_TODTCOL,   util_u32toa(todt  ));
     table_replacecurrentcell_alloc(tab, SPANS_DATACOL,   data);

     return 1;	/* success */	
}



/*
 * Get the details of the latest span from the span table [as returned by 
 * spans_read()].
 * The data will always be null terminated and may contain no newlines.
 * Do not attempt to free or directly modify the data.
 * Returns 1 if successful, from, fromdt, to, todt and data.
 * On failure or if there are no spans in the ring, returns 0.
 */
int spans_getlatest(SPANS tab,		/* span table for ring */
		    int *from,		/* return: span starting sequence */
		    int *to,		/* return: span ending sequence */
		    time_t *fromdt,	/* return: span starting time */
		    time_t *todt,	/* return: span ending time */
		    char **data		/* return: data */ )
{
     int rowkey=0, max=-1;

     /* find the data */
     table_traverse(tab) {
	  if (max < atoi( table_getcurrentcell(tab, SPANS_TOCOL) ) ) {
	       max = atoi( table_getcurrentcell(tab, SPANS_TOCOL) );
	       rowkey = table_getcurrentrowkey(tab);
	  }
     }

     /* report the data if found */
     if (max > -1) {
	  *from   =    atoi(table_getcell(tab,rowkey,SPANS_FROMCOL) );
	  *to     =    atoi(table_getcell(tab,rowkey,SPANS_TOCOL) );
	  *fromdt = strtoul(table_getcell(tab,rowkey,SPANS_FROMDTCOL),NULL,0);
	  *todt   = strtoul(table_getcell(tab,rowkey,SPANS_TODTCOL),  NULL,0);
	  *data   =         table_getcell(tab,rowkey,SPANS_DATACOL);
	  return 1;
     }

     return 0;

}

/*
 * Get the details of the olodest span from the span table [as returned by 
 * spans_read()].
 * The data will always be null terminated and may contain no newlines.
 * No not attempt to free or directly modify the data.
 * Returns 1 if successful, from, fromdt, to, todt and data.
 * On failure or if there are no spans in the ring, returns 0.
 */
int spans_getoldest(SPANS tab,		/* span table for ring */
		    int *from,		/* return: span starting sequence */
		    int *to,		/* return: span ending sequence */
		    time_t *fromdt,	/* return: span starting time */
		    time_t *todt,	/* return: span ending time */
		    char **data		/* return: data */ )
{
     int rowkey=0, min=INT_MAX;

     /* find the data */
     table_traverse(tab) {
	  if (min > atoi( table_getcurrentcell(tab, SPANS_TOCOL) ) ) {
	       min = atoi( table_getcurrentcell(tab, SPANS_TOCOL) );
	       rowkey = table_getcurrentrowkey(tab);
	  }
     }

     /* report the data if found */
     if (min < INT_MAX) {
	  *from   =    atoi(table_getcell(tab,rowkey,SPANS_FROMCOL) );
	  *to     =    atoi(table_getcell(tab,rowkey,SPANS_TOCOL) );
	  *fromdt = strtoul(table_getcell(tab,rowkey,SPANS_FROMDTCOL),NULL,0);
	  *todt   = strtoul(table_getcell(tab,rowkey,SPANS_TODTCOL),  NULL,0);
	  *data   =         table_getcell(tab,rowkey,SPANS_DATACOL);
	  return 1;
     }

     return 0;

}


/*
 * Get the details of the span containing seq from the span table
 * [as returned by spans_read()].
 * The data will always be null terminated and may contain no newlines.
 * No not attempt to free or directly modify the data.
 * Returns 1 if successful, from, fromdt, to, todt and data.
 * On failure or if there are no spans in the ring, returns 0.
 */
int spans_getseq(SPANS tab,		/* span table for ring */
		 int seq,		/* sequence  to look for */
		 int *from,		/* return: span starting sequence */
		 int *to,		/* return: span ending sequence */
		 time_t *fromdt,	/* return: span starting time */
		 time_t *todt,		/* return: span ending time */
		 char **data		/* return: data */ )
{
     int found=0;

     /* find the data */
     table_traverse(tab) {
	  if (seq >= atoi( table_getcurrentcell(tab, SPANS_FROMCOL) ) &&
	      seq <= atoi( table_getcurrentcell(tab, SPANS_TOCOL) ) ) {
	       found++;
	       break;
	  }
     }

     /* report the data if found */
     if (found) {
	  *from   =    atoi(table_getcurrentcell(tab, SPANS_FROMCOL) );
	  *to     =    atoi(table_getcurrentcell(tab, SPANS_TOCOL) );
	  *fromdt = strtoul(table_getcurrentcell(tab, SPANS_FROMDTCOL),NULL,0);
	  *todt   = strtoul(table_getcurrentcell(tab, SPANS_TODTCOL),  NULL,0);
	  *data   =         table_getcurrentcell(tab, SPANS_DATACOL);
	  return 1;
     }

     return 0;
}


/*
 * Get the details of the span containing the time dt from the span table
 * [as returned by spans_read()] or 0 if no match was found.
 * If no span contains the time and findnearest!=0, then adjacent spans will
 * be returned. If findnearest == 1 or SPANS_HUNTPREV, details of the 
 * previous span are returned. If ==2 or SPANS_HUNTNEXT, then the next span 
 * is returned.
 * The data will always be null terminated and may contain newlines.
 * No not attempt to free or directly modify the data.
 * Returns 1 if successful, from, fromdt, to, todt and data.
 * On failure or if there are no spans in the ring, returns 0.
 */
int spans_gettime(SPANS tab,		/* span table for ring */
		  time_t dt,		/* time to look for */
		  int findnearest,	/* 0=exact, 1=prev, 2=next */
		  int *from,		/* return: span starting sequence */
		  int *to,		/* return: span ending sequence */
		  time_t *fromdt,	/* return: span starting time */
		  time_t *todt,		/* return: span ending time */
		  char **data		/* return: data */ )
{
     int found=0, nearestrowkey=-1;
     time_t sp_from, sp_to, nearest_t=0;

     /* initialise */
     if (findnearest == SPANS_HUNTPREV)
	  nearest_t = 0;
     else if (findnearest == SPANS_HUNTNEXT)
	  nearest_t = INT_MAX;

     
     /* find the data */
     table_traverse(tab) {
	  /* convert from string */
	  sp_from = strtoul(table_getcurrentcell(tab, SPANS_FROMDTCOL),NULL,0);
	  sp_to   = strtoul(table_getcurrentcell(tab, SPANS_TODTCOL),  NULL,0);

	  /* containing match, overriding all others */
	  if (dt >= sp_from && dt <= sp_to ) {
	       found++;
	       break;
	  }

	  /* record the nearest rowkey in case it is needed */
	  if (   (  findnearest == SPANS_HUNTPREV && 
		    dt > sp_to   && sp_from > nearest_t  ) ||
		 (  findnearest == SPANS_HUNTNEXT && 
		    dt < sp_from && sp_from < nearest_t    )   ) {
	       nearestrowkey = table_getcurrentrowkey(tab);
	       nearest_t = sp_from;
	  }
     }

     /* if not found an exact match but some are near, use that instead */
     if ( (!found) && nearestrowkey > -1) {
	  table_gotorow(tab, nearestrowkey);
	  found++;
     }

     /* report the data if found */
     if (found) {
	  *from   =    atoi(table_getcurrentcell(tab, SPANS_FROMCOL) );
	  *to     =    atoi(table_getcurrentcell(tab, SPANS_TOCOL) );
	  *fromdt = strtoul(table_getcurrentcell(tab, SPANS_FROMDTCOL),NULL,0);
	  *todt   = strtoul(table_getcurrentcell(tab, SPANS_TODTCOL),  NULL,0);
	  *data   =         table_getcurrentcell(tab, SPANS_DATACOL);
	  return 1;
     }

     return 0;
}



/*
 * Search the span table [as returned by spans_read()] for data matching 
 * the specified string.
 * Returns 1 if successful, from, fromdt, to and todt sequencies.
 * On failure or if there are no spans in the ring, returns 0.
 */
int spans_search(SPANS tab,		/* span table for ring */
		 char *data,		/* query data string */
		 int *from,		/* return: span starting sequence */
		 int *to,		/* return: span ending sequence */
		 time_t *fromdt,	/* return: span starting time */
		 time_t *todt		/* return: span ending time */ )
{
     int found=0;

     /* find the data */
     table_traverse(tab) {
	  if ( ! strcmp( data, table_getcurrentcell(tab, SPANS_DATACOL) ) ) {
	       found++;
	       break;
	  }
     }

     /* report the data if found */
     if (found) {
	  *from   =   atoi( table_getcurrentcell(tab, SPANS_FROMCOL) );
	  *to     =   atoi( table_getcurrentcell(tab, SPANS_TOCOL) );
	  *fromdt = strtoul(table_getcurrentcell(tab, SPANS_FROMDTCOL),NULL,0);
	  *todt   = strtoul(table_getcurrentcell(tab, SPANS_TODTCOL),  NULL,0);
	  return 1;
     }

     return 0;
}



/*
 * Extend a span to new lengths by specifing the new end sequence number
 * and its time. 
 * Returns 1 for success or 0 if unable to find the original span. 
 * The table should be written back to the holstore using spans_writeblock() 
 * after this extension.
 */
int spans_extend(SPANS tab,	/* span table for ring */
		 int from,	/* span starting sequence */
		 int to,	/* current ending sequence */
		 int newto,	/* new ending sequence */
		 time_t newtodt	/* new ending time */ )
{
     int found=0;

     /* find the data */
     table_traverse(tab) {
	  if (from == atoi( table_getcurrentcell(tab, SPANS_FROMCOL) ) &&
	      to   == atoi( table_getcurrentcell(tab, SPANS_TOCOL) ) ) {
	       found++;
	       break;
	  }
     }

     /* patch the data */
     if (found) {
	  table_replacecurrentcell_alloc(tab,SPANS_TOCOL,  util_i32toa(newto));
	  table_replacecurrentcell_alloc(tab,SPANS_TODTCOL,util_u32toa(newtodt));
	  return 1;
     }

     return 0;
}

/*
 * Remove any spans that are before (less than) `oldestseq' and modify 
 * the `from' field of any span that straddles the sequence and make it 
 * equal to `oldestseq'. The time of the oldest sequence should also
 * be provided.
 * This is done inside the span table, which should then be written to 
 * holstore afterwards with table_writeblock().
 * Returns the number of spans affected, or -1 if there was an error.
 */
int spans_purge(SPANS tab,		/* span table for ring */
		int oldestseq,		/* new starting sequence */
		time_t oldestdt		/* new starting time */ )
{
     int n=0;

     /* traverse the list to find purged spans */
     table_traverse(tab) {
	  if ( atoi(table_getcurrentcell(tab, SPANS_TOCOL)) < oldestseq ) {
	       /* the span is toast */
	       table_rmcurrentrow(tab);
	       n++;
	  } else if (atoi(table_getcurrentcell(tab,SPANS_FROMCOL)) <oldestseq){
	       /* the span needs `from' to be adjusted */
	       table_replacecurrentcell_alloc(tab, SPANS_FROMCOL, 
					      util_i32toa(oldestseq));
	       table_replacecurrentcell_alloc(tab, SPANS_FROMDTCOL, 
					      util_u32toa(oldestdt));
	       n++;
	  }
     }

     return n;
}


/*
 * Returns a table of ring blocks available in the given holstore or
 * NULL otherwise. The table columns are named: ring, from, to, time start, 
 * time end and header.
 */
SPANS spans_readringblocks(HOLD hol)
{
     char spanname[SPANSTORE_KEYLEN];
     TREE *allrings;
     ITREE *thisring, *span;
     TABLE tab;

     /* get a list of all spans in this ring and their data */
     snprintf(spanname, SPANSTORE_KEYLEN, "%s*", SPANSTORE_DATASPACE);
     hol_begintrans(hol, 'r');
     allrings = hol_search(hol, spanname, NULL);
     hol_endtrans(hol);
     if ( ! allrings)
	  return NULL;

     tab = table_create_a(spans_allrings_cols);

     /* this is slightly tricky to do with table_scan()+others,
      * so we scan the text ourselves */
     tree_traverse( allrings ) {
       if (util_scantext(tree_get(allrings),"\t",UTIL_MULTISEP,&thisring) < 1)
	       continue;
	  itree_traverse(thisring) {
	       /* insert the individual span entries in the order they
		* appear in the row */
	       span = itree_get(thisring);
	       table_addemptyrow(tab);
	       table_replacecurrentcell(tab, "ring", tree_getkey(allrings)
					+ strlen(SPANSTORE_DATASPACE));
	       itree_first(span);
	       table_replacecurrentcell(tab, "from", itree_get(span));
	       itree_next(span);
	       table_replacecurrentcell(tab, "to", itree_get(span));
	       itree_next(span);
	       table_replacecurrentcell(tab, "time start", itree_get(span));
	       itree_next(span);
	       table_replacecurrentcell(tab, "time end", itree_get(span));
	       itree_next(span);
	       table_replacecurrentcell(tab, "header", itree_get(span));
	  }
	  util_scanfree(thisring);
	  table_freeondestroy(tab, tree_get(allrings));
	  table_freeondestroy(tab, tree_getkey(allrings));
     }

     /* dont use hol_freesearch() as we want to keep the key and data
      * allocations for the answer; just free the tree */
     tree_destroy(allrings);

     return tab;
}



/*
 * Return 1 if the proposed span overlaps existing spans or if there is 
 * a failure. If there is NOT an overlap return 0.
 */
int spans_overlap(SPANS tab,		/* table of a ring's spans */
		  int from,		/* proposed span starting sequence */
		  int to		/* proposed span ending sequence */ )
{
     int rowkey;
     char *tmp;

     table_traverse(tab) {
	  /* from overlaps */
	  if (from >= atoi(table_getcurrentcell(tab, SPANS_FROMCOL)) &&
	      from <= atoi(table_getcurrentcell(tab, SPANS_TOCOL  )) ) {

	       rowkey = table_getcurrentrowkey(tab);
	       tmp = table_printrow(tab, rowkey);
	       elog_printf(DEBUG, "span [%d,%d] overlaps (f) existing "
			   "span:-\n%s", from, to, tmp);
	       nfree(tmp);
	       return 1;
	  }

	  /* to overlaps */
	  if (to >= atoi(table_getcurrentcell(tab, SPANS_FROMCOL)) &&
	      to <= atoi(table_getcurrentcell(tab, SPANS_TOCOL  )) ) {

	       rowkey = table_getcurrentrowkey(tab);
	       tmp = table_printrow(tab, rowkey);
	       elog_printf(DEBUG, "span [%d,%d] overlaps (t) existing "
			   "span:-\n%s", from, to, tmp);
	       nfree(tmp);
	       return 1;
	  }


	  /* contains overlap */
	  if (atoi(table_getcurrentcell(tab, SPANS_FROMCOL)) >= from &&
	      atoi(table_getcurrentcell(tab, SPANS_TOCOL  )) <= to ) {

	       rowkey = table_getcurrentrowkey(tab);
	       tmp = table_printrow(tab, rowkey);
	       elog_printf(DEBUG, "span [%d,%d] overlaps (c) existing "
			   "span:-\n%s", from, to, tmp);
	       nfree(tmp);
	       return 1;
	  }
     }

     return 0;		/* no overlap */
}


/* Return a list of rings; free with macro spans_freels() */
TREE *spans_lsringshol(HOLD h)
{
     TREE *thol, *tspan;

     if ( ! hol_begintrans(h, 'r') )
	  elog_send(ERROR, "unable to get holstore lock for ring list");
     thol = hol_search(h, SPANSTORE_DATASPACE "*", NULL);
     hol_endtrans(h);

     /* make a new list with keys that have the spanstore dataspace 
      * prefix removed */
     if (thol) {
	  tspan = tree_create();
	  tree_traverse(thol) {
	       tree_add(tspan, xnstrdup(tree_getkey(thol) + 
					strlen(SPANSTORE_DATASPACE)),
			tree_get(thol));
	       nfree(tree_getkey(thol));
	  }
	  tree_destroy(thol);
     } else
	  tspan = NULL;

     return tspan;
}


/*
 * Return a list of name roots present in a ringblock as returned by
 * span_getringblocks().
 * There is a convention in tablestore naming which allows us to piece
 * together like data from several rings. The convention is:-
 * 
 *    r.<name><NNN>
 *
 * Rings must start with `r' to be considered for consolidation. <name> is 
 * the key that links like data together and <NNN> is the sample period
 * in seconds.
 * If there are no rings suitable for consolidation, NULL will be returned.
 * On success, an independent list is returned with ring names in the key 
 * and values set to NULL. It should be released with tree_clearoutandfree()
 * and tree_destroy().
 */
TREE *spans_getnameroots(SPANS ringblocks)
{
     TREE *nameroots;
     char *rname, name[100];
     int rootlen;

     nameroots = tree_create();
     table_traverse(ringblocks) {
	  /* get ring name and reject if too short or with the wrong prefix */
	  rname = table_getcurrentcell(ringblocks, "ring");
	  if (rname == NULL || strlen(rname) < 3)
	       continue;
	  if (strncmp(rname, "r.", 2))
	       continue;

	  /* size and copy root name, get the time bounderies */
	  rootlen = strcspn(rname+2, "0123456789");
	  strncpy(name, rname+2, rootlen);
	  name[rootlen] = '\0';

	  /* compare with list */
	  if (tree_find(nameroots, name) == TREE_NOVAL) {
	       /* new word root */
	       tree_add(nameroots, xnstrdup(name), NULL);
	  }
     }

     if (tree_empty(nameroots)) {
	  tree_destroy(nameroots);
	  nameroots = NULL;
     }

     return nameroots;
}



/*
 * Return a list of ring names that contain data which fall between
 * fromdt and todt and share a common name root, suitable for consolidation.
 * The information is based from ringblocks, as produced by 
 * spans_readringblocks().
 * There is a convention in tablestore naming which allows us to piece
 * together like data from several rings. The convention is:-
 * 
 *    r.<name><NNN>
 *
 * Rings must start with `r' to be considered for consolidation. <name> is 
 * the key that links like data together and <NNN> is the sample period
 * in seconds.
 * The time limits of the consolidated ring coverage are set in ret_begin 
 * and ret_end, which is storage provided by the caller; if they are set 
 * to NULL, then they will not be set.
 * The ring names are returned in the keys of the list and refer to data in
 * the ringblock SPANS table; data items are set to NULL. Therefore, 
 * ringblocks should exist during the lifetime of the list and the list
 * should be destroyed with tree_destroy().
 * Returns NULL if there are no matches.
 */
TREE *spans_getrings_byrootandtime(SPANS ringblocks, char *nameroot, 
				   time_t fromdt, time_t todt, 
				   time_t *ret_begin, time_t *ret_end)
{
     TREE *rings;
     char *rname, name[100];
     int rootlen;
     time_t oldest_t, newest_t;

     /* set returned boundary values */
     if (ret_begin)
	  *ret_begin = INT_MAX;
     if (ret_end)
	  *ret_end = 0;

     rings = tree_create();
     table_traverse(ringblocks) {
	  /* get ring name and reject if too short or with the wrong prefix */
	  rname = table_getcurrentcell(ringblocks, "ring");
	  if (rname == NULL || strlen(rname) < 3)
	       continue;
	  if (strncmp(rname, "r.", 2))
	       continue;

	  /* size and copy root name, only proceed if nameroot matches */
	  rootlen = strcspn(rname+2, "0123456789");
	  strncpy(name, rname+2, rootlen);
	  name[rootlen] = '\0';
	  if (strcmp(name, nameroot) != 0)
	       continue;

	  /* get the time bounderies */
	  oldest_t = (time_t) strtoul(
	       table_getcurrentcell(ringblocks, "time start"), NULL, 0);
	  newest_t = (time_t) strtoul(
	       table_getcurrentcell(ringblocks, "time end"), NULL, 0);

	  /* check for containment */
	  if ( ( todt   >= oldest_t && todt   <= newest_t ) ||
	       ( fromdt >= oldest_t && fromdt <= newest_t ) ||
	       ( fromdt <= oldest_t && todt   >= newest_t ) )
	       if (tree_find(rings, rname) == TREE_NOVAL)
		    tree_add(rings, rname, NULL);

	  /* check and set consolidated ring boundaries */
	  if (ret_begin && oldest_t < *ret_begin)
	       *ret_begin = oldest_t;
	  if (ret_end && newest_t > *ret_end)
	       *ret_end = newest_t;
     }

     if (tree_empty(rings)) {
	  tree_destroy(rings);
	  rings = NULL;
     }

     return rings;
}


/*
 * Return a list of span headers that contain data with in a sequence range
 * fromseq and toseq.
 * The information is based from ringblocks, as produced by 
 * spans_readringblocks().
 * The ring names are the keys of the returned list and refer to data in
 * the ringblock SPANS table; data in the list are set to NULL. Therefore, 
 * ringblocks should exist during the lifetime of the list and the list
 * should be destroyed with tree_destroy().
 * Returns NULL if there are no matches.
 */
TREE *spans_getheader_byseqrange(SPANS tab, int fromseq, int toseq)
{
     TREE *hds;
     int oldest_seq, newest_seq;

     hds = tree_create();
     table_traverse(tab) {
	  /* convert seq to int */
	  oldest_seq = atoi( table_getcurrentcell(tab, SPANS_FROMCOL) );
	  newest_seq = atoi( table_getcurrentcell(tab, SPANS_TOCOL) );

	  /* check for containment */
	  if ( ( toseq   >= oldest_seq && toseq   <= newest_seq ) ||
	       ( fromseq >= oldest_seq && fromseq <= newest_seq ) ||
	       ( fromseq <= oldest_seq && toseq   >= newest_seq ) )
	       if (tree_find(hds, table_getcurrentcell(tab, SPANS_DATACOL)) 
		                  == TREE_NOVAL)
		    tree_add(hds, table_getcurrentcell(tab, SPANS_DATACOL), 
			     NULL);
     }

     if (tree_empty(hds)) {
	  tree_destroy(hds);
	  hds = NULL;
     }

     return hds;
}



#if TEST

#include <unistd.h>
#include "nmalloc.h"

#define TESTFILE1 "t.spanstore.1.dat"
#define TESTRING1 "sptest"
#define TESTRING2 "r.aaa1"
#define TESTRING3 "r.aaa2"
#define TESTRING4 "r.aaa60"
#define TESTRING5 "r.bbb60"
#define TESTRING6 "r.ccc60"
#define TESTRING7 "r.ccc5"
#define TESTRING8 "r.ccc6"
#define TESTRING9 "e.ccc7"
#define TESTRING10 "e.ccc8"
#define TESTRING11 "e.ccc9"

int main(int argc, char *argv[]) {
     rtinf err;
     TS_RING ts;
     int from, to, datlen, r;
     time_t fromdt, todt, now;
     char *data, str[1024];
     SPANS spans;
     TREE *ringroots, *ringlist;

     route_init("stderr", 0, NULL);
     err = route_open("stderr", NULL, NULL, 0);
     elog_init(err, 0, "spanstore test", NULL);
     ts_init();

     unlink(TESTFILE1);
     now = time(NULL);

     /* Test 1, the creation */
     ts = ts_create(TESTFILE1, 0644, TESTRING1, "spanstore test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL, "[1] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING1);
     spans = spans_readblock(ts);
     if ( spans != NULL )
          elog_die(FATAL, "[1] shouldn't be able to readblock()");
     spans = table_create();
     r = spans_new(spans, 0, 0, now, now, "skiing");
     if ( ! r )
          elog_die(FATAL, "[1] failed to create new span");
     r = spans_writeblock(ts, spans);
     if ( ! r )
          elog_die(FATAL, "[1] failed to write new block");
     /* whitebox testing of new span */
     sprintf(str, "%s%s", SPANSTORE_DATASPACE, TESTRING1);
     data = (char *) hol_get(ts_holstore(ts), str, &datlen);
     if ( ! data )
          elog_die(FATAL, "[1] failed to hol_get");
     r = sprintf(str, "0\t0\t%ld\t%ld\tskiing", now, now);
     if ( strncmp(data, str, r) != 0 )
          elog_die(FATAL, "[1] failed to match contents "
		   "(%s != %s)", data, str);
     if ( datlen != r+2 )
          elog_die(FATAL,"[1] contents length mismatch (%d != %d)", 
		   datlen, r+2);
     nfree(data);
     data = NULL;
     ts_close(ts);
     table_destroy(spans);

     /* test 2, open and fetch single record by latest and oldest */
     ts = ts_open(TESTFILE1, TESTRING1, NULL);
     if ( ! ts )
          elog_die(FATAL, "[2a] unable to open timestore %s,%s",
		   TESTFILE1, TESTRING1);
     spans = spans_readblock(ts);
     if ( spans == NULL )
          elog_die(FATAL, "[2a] can't readblock()");
     r = spans_getlatest(spans, &from, &to, &fromdt, &todt, &data);
     if ( ! r )
          elog_die(FATAL, "[2a] can't getlatest()");
     if (to != 0)
          elog_die(FATAL, "[2a] to != 0");
     if (from != 0)
          elog_die(FATAL, "[2a] from != 0");
     if ( strncmp(data, "skiing", 6) != 0 )
          elog_die(FATAL, "[2a] failed to match contents");
     if ( strlen(data) != 6 )
          elog_die(FATAL,"[2a] contents length mismatch (%d != 6)", 
		   strlen(data));
     r = spans_getoldest(spans, &from, &to, &fromdt, &todt, &data);
     if ( ! r )
          elog_die(FATAL, "[2b] can't getoldest()");
     if (to != 0)
          elog_die(FATAL, "[2b] to != 0");
     if (from != 0)
          elog_die(FATAL, "[2b] from != 0");
     if ( strncmp(data, "skiing", 6) != 0 )
          elog_die(FATAL, "[2b] failed to match contents");
     if ( strlen(data) != 6 )
          elog_die(FATAL,"[2b] contents length mismatch (%d != 6)", 
		   strlen(data));

     /* test 3, get the single record by sequence */
     r = spans_getseq(spans, 52, &from, &to, &fromdt, &todt, &data);
     if ( r )
          elog_die(FATAL, "[3] shouldn't be able to getseq(52)");
     r = spans_getseq(spans, 0, &from, &to, &fromdt, &todt, &data);
     if ( ! r )
          elog_die(FATAL, "[3] can't getseq(0)");
     if (to != 0)
          elog_die(FATAL, "[3] to != 0");
     if (from != 0)
          elog_die(FATAL, "[3] from != 0");
     if ( strncmp(data, "skiing", 6) != 0 )
          elog_die(FATAL, "[3] failed to match contents");
     if ( strlen(data) != 6 )
          elog_die(FATAL,"[3] contents length mismatch (%d != 6)", 
		   strlen(data));

     /* test 4, attempt to create a new span over the existing one */
     r = spans_new(spans, 0, 0, now, now, "downhill");
     if ( r )
          elog_die(FATAL, "[4a] overwrote existing span");
     r = spans_new(spans, 0, 30, now, now, "downhill");
     if ( r )
          elog_die(FATAL, "[4b] overwrote existing span");

     /* test 5, purge single span of length 1 */
     r = spans_purge(spans, 0, now);
     if ( r != 0 )
          elog_die(FATAL, "[5a] purge failed");
     r = spans_getlatest(spans, &from, &to, &fromdt, &todt, &data);
     if ( ! r )
          elog_die(FATAL, "[5a] span was purged");
     r = spans_purge(spans, 1, now);
     if ( r != 1 )
          elog_die(FATAL, "[5b] purge failed");
     r = spans_getlatest(spans, &from, &to, &fromdt, &todt, &data);
     if ( r )
          elog_die(FATAL, "[5b] span was not purged");

     /* test 6, extend the single span */
     r = spans_new(spans, 0, 0, now, now, "downhill");
     if ( ! r )
          elog_die(FATAL, "[6] failed to create new span");
     r = spans_extend(spans, 0, 0, 57, now);
     if ( ! r )
          elog_die(FATAL, "[6] failed to extend");
     /* write out the block */
     r = spans_writeblock(ts, spans);
     if ( ! r )
          elog_die(FATAL, "[6] failed to write new block");
     /* whitebox testing of new span */
     sprintf(str, "%s%s", SPANSTORE_DATASPACE, TESTRING1);
     data = (char *) hol_get(ts_holstore(ts), str, &datlen);
     if ( ! data )
          elog_die(FATAL, "[6] failed to hol_get");
     r = sprintf(str, "0\t57\t%ld\t%ld\tdownhill", now, now);
     if ( strncmp(data, str, r) != 0 )
          elog_die(FATAL, "[6] failed to match contents "
		   "(%s != %s)", data, str);
     if ( strlen(data) != r+1 )
          elog_die(FATAL,"[6] contents length mismatch (%d != %d)", 
		   strlen(data), r+2);
     nfree(data);
     data = NULL;

     /* test 7, purge start of single long span */
     r = spans_purge(spans, 10, now);
     if ( ! r )
          elog_die(FATAL, "[7] shouldn't fail purging nothing");
     r = spans_getseq(spans, 5, &from, &to, &fromdt, &todt, &data);
     if ( r )
          elog_die(FATAL, "[7] span was not purged");
     r = spans_getseq(spans, 14, &from, &to, &fromdt, &todt, &data);
     if ( ! r )
          elog_die(FATAL, "[7] can't getseq(14)");
     if (from != 10)
          elog_die(FATAL, "[7] from != 10");
     if (to != 57)
          elog_die(FATAL, "[7] to != 57");
     if ( strncmp(data, "downhill", 8) != 0 )
          elog_die(FATAL, "[7] failed to match contents");
     if ( strlen(data) != 8 )
          elog_die(FATAL,"[7] contents length mismatch (%d != 8)", 
		   strlen(data));

     /* test 8, purge whole of single long span */
     r = spans_purge(spans, 100, now);
     if ( ! r )
          elog_die(FATAL, "[8] shouldn't fail purging nothing");
     r = spans_getseq(spans, 30, &from, &to, &fromdt, &todt, &data);
     if ( r )
          elog_die(FATAL, "[8a] span was not purged");
     r = spans_getlatest(spans, &from, &to, &fromdt, &todt, &data);
     if ( r )
          elog_die(FATAL, "[8b] span was not purged");

     /* test 9, create several new spans */
     r = spans_new(spans, 0, 13, now, now, "skiing");
     if ( ! r )
          elog_die(FATAL, "[9a] failed to create new span");
     r = spans_new(spans, 14, 62, now, now, "downhill");
     if ( ! r )
          elog_die(FATAL, "[9b] failed to create new span");
     r = spans_new(spans, 63, 1210, now, now, "only");
     if ( ! r )
          elog_die(FATAL, "[9c] failed to create new span");
     r = spans_new(spans, 1211, 1211, now, now, "on");
     if ( ! r )
          elog_die(FATAL, "[9d] failed to create new span");
     r = spans_new(spans, 1212, 1212, now, now, "the");
     if ( ! r )
          elog_die(FATAL, "[9e] failed to create new span");
     r = spans_new(spans, 1213, 2001, now, now, "moon");
     if ( ! r )
          elog_die(FATAL, "[9f] failed to create new span");
     /* write out the block */
     r = spans_writeblock(ts, spans);
     if ( ! r )
          elog_die(FATAL, "[9] failed to write new block");
     /* whitebox testing of new spans */
     sprintf(str, "%s%s", SPANSTORE_DATASPACE, TESTRING1);
     data = (char *) hol_get(ts_holstore(ts), str, &datlen);
     if ( ! data )
          elog_die(FATAL, "[9a] failed to hol_get");
     r = sprintf(str, "0\t13\t%ld\t%ld\tskiing\n14\t62\t%ld\t%ld\tdownhill\n"
		 "63\t1210\t%ld\t%ld\tonly\n1211\t1211\t%ld\t%ld\ton\n"
		 "1212\t1212\t%ld\t%ld\tthe\n1213\t2001\t%ld\t%ld\tmoon\n", 
		 now, now, now, now, now, now, now, now, now, now, now, now);
     if ( strncmp(data, str, r) != 0 )
          elog_die(FATAL, "[9a] failed to match contents "
		   "(%s != %s)", data, str);
     if ( datlen != r+1 )
          elog_die(FATAL,"[9a] contents length mismatch "
		   "(%d != %d)", datlen, r+1);
     nfree(data);
     data = NULL;
     datlen = -1;
			/* blackbox test */
     r = spans_getoldest(spans, &from, &to, &fromdt, &todt, &data);
     if ( ! r )
          elog_die(FATAL, "[9g] can't getoldest()");
     if (to != 13)
          elog_die(FATAL, "[9g] to != 13");
     if (from != 0)
          elog_die(FATAL, "[9g] from != 0");
     if ( strncmp(data, "skiing", 6) != 0 )
          elog_die(FATAL, "[9g] failed to match contents");
     if ( strlen(data) != 6 )
          elog_die(FATAL,"[9g] contents length mismatch (%d != 6)", 
		   strlen(data));

     /* test 10, extend a span in the middle - at the moment this is allowed
      * although I can't think of a reason to allow it!! */
     r = spans_extend(spans, 1211, 1211, 1212, now);
     if ( ! r )
          elog_die(FATAL, "[10] failed to extend");
     /* write out the block */
     r = spans_writeblock(ts, spans);
     if ( ! r )
          elog_die(FATAL, "[10] failed to write new block");
     /* whitebox testing of new spans */
     sprintf(str, "%s%s", SPANSTORE_DATASPACE, TESTRING1);
     data = (char *) hol_get(ts_holstore(ts), str, &datlen);
     if ( ! data )
          elog_die(FATAL, "[10] failed to hol_get");

     r = sprintf(str, "0\t13\t%ld\t%ld\tskiing\n14\t62\t%ld\t%ld\tdownhill\n"
		 "63\t1210\t%ld\t%ld\tonly\n1211\t1212\t%ld\t%ld\ton\n"
		 "1212\t1212\t%ld\t%ld\tthe\n1213\t2001\t%ld\t%ld\tmoon\n", 
		 now, now, now, now, now, now, now, now, now, now, now, now);
     if ( strncmp(data, str, r) != 0 )
          elog_die(FATAL, "[10] failed to match contents "
		   "(%s != %s)", data, str);
     if ( datlen != r+1 )
          elog_die(FATAL,"[10] contents length mismatch "
		   "(%d != %d)", datlen, r+1);
     nfree(data);
     data = NULL;
     datlen = -1;

     /* test 11, search for a span body */
     from = to = -1;
     r = spans_search(spans, "moon", &from, &to, &fromdt, &todt);
     if ( ! r)
          elog_die(FATAL, "[11] failed to find");
     if (from != 1213)
          elog_die(FATAL, "[11] from != 1213");
     if (to != 2001)
          elog_die(FATAL, "[11] to != 2001");
     r = spans_search(spans, "frog", &from, &to, &fromdt, &todt);
     if (r)
          elog_die(FATAL, "[11] found something that we shouldn't have");
     spans_writeblock(ts, spans);
     table_destroy(spans);

#if 0
     /* test 12, return the spans available */
     spans = spans_readringblocks(ts_holstore(ts));
     if ( ! spans )
          elog_die(FATAL, "[12] no list returned");
     if (tree_find(list, TESTRING1) == TREE_NOVAL)
          elog_die(FATAL, "[12] list does not contain %s", 
		   TESTRING1);
     if (tree_n(list) != 1)
          elog_die(FATAL, "[12] list has %d elements", 
		   tree_n(list));
     spans_freels(list);
#endif

     ts_close(ts);

     /* test 13, test name roots using name conventions */
     					/* set up rings */
     ts = ts_create(TESTFILE1, 0644, TESTRING2, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13a] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING2);
     spans = spans_readblock(ts);
     if ( spans != NULL )
          elog_die(FATAL, "[13] readblock() shouldn't return"
		   " NULL");
     spans = table_create();
     r = spans_new(spans, 0, 1, 100, 200, "aaa");
     if ( ! r )
          elog_die(FATAL, "[13a] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
     if ( ! r )
          elog_die(FATAL, "[13a] failed to write new block");
     ts_close(ts);
     ts = ts_create(TESTFILE1, 0644, TESTRING3, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13b] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING3);
     spans = table_create();
     r = spans_new(spans, 2, 3, 300, 400, "bbb");
     if ( ! r )
          elog_die(FATAL, "[13b] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
     if ( ! r )
          elog_die(FATAL, "[13] failed to write new block");
     ts_close(ts);
     ts = ts_create(TESTFILE1, 0644, TESTRING4, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13c] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING4);
     spans = table_create();
     r = spans_new(spans, 4, 5, 500, 600, "ccc");
     if ( ! r )
          elog_die(FATAL, "[13c] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
     if ( ! r )
          elog_die(FATAL, "[13] failed to write new block");
     ts_close(ts);
     ts = ts_create(TESTFILE1, 0644, TESTRING5, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13d] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING5);
     spans = table_create();
     r = spans_new(spans, 6, 7, 700, 800, "ddd");
     if ( ! r )
          elog_die(FATAL, "[13d] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
     if ( ! r )
          elog_die(FATAL, "[13] failed to write new block");
     ts_close(ts);
     ts = ts_create(TESTFILE1, 0644, TESTRING6, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13e] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING6);
     spans = table_create();
     r = spans_new(spans, 8, 9, 900, 1000, "eee");
     if ( ! r )
          elog_die(FATAL, "[13e] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
     if ( ! r )
          elog_die(FATAL, "[13e] failed to write new block");
     ts_close(ts);
     ts = ts_create(TESTFILE1, 0644, TESTRING7, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13f] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING7);
     spans = table_create();
     r = spans_new(spans, 10, 11, 1100, 1200, "fff");
     if ( ! r )
          elog_die(FATAL, "[13f] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
     if ( ! r )
          elog_die(FATAL, "[13f] failed to write new block");
     ts_close(ts);
     ts = ts_create(TESTFILE1, 0644, TESTRING8, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13g] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING8);
     spans = table_create();
     r = spans_new(spans, 12, 13, 1300, 1400, "ggg");
     if ( ! r )
          elog_die(FATAL, "[13g] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
     if ( ! r )
          elog_die(FATAL, "[13g] failed to write new block");
     ts_close(ts);
     ts = ts_create(TESTFILE1, 0644, TESTRING9, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13g] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING9);
     spans = table_create();
     r = spans_new(spans, 14, 15, 1500, 1600, "hhh");
     if ( ! r )
          elog_die(FATAL, "[13g] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
     if ( ! r )
          elog_die(FATAL, "[13g] failed to write new block");
     ts_close(ts);
     ts = ts_create(TESTFILE1, 0644, TESTRING10, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13i] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING10);
     spans = table_create();
     r = spans_new(spans, 16, 17, 1700, 1800, "iii");
     if ( ! r )
          elog_die(FATAL, "[13i] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
     if ( ! r )
          elog_die(FATAL, "[13i] failed to write new block");
     ts_close(ts);
     ts = ts_create(TESTFILE1, 0644, TESTRING11, "ring test", NULL, 100);
     if ( ! ts )
          elog_die(FATAL,"[13j] unable to create timestore %s,%s",
		   TESTFILE1, TESTRING11);
     spans = table_create();
     r = spans_new(spans, 18, 19, 1900, 2000, "jjj");
     if ( ! r )
          elog_die(FATAL, "[13j] failed to create new span");
     r = spans_writeblock(ts, spans);
     table_destroy(spans);
						/* read blocks */
     spans = spans_readringblocks(ts_holstore(ts));
     if ( ! spans )
          elog_die(FATAL, "[13] no list returned");
						/* check data */
     ringroots = spans_getnameroots(spans);
     if (tree_n(ringroots) != 3)
	  elog_die(FATAL, "[13] wrong number of ringroots returned "
		   "%d != 3", tree_n(ringroots));
     if (tree_find(ringroots, "aaa") == TREE_NOVAL)
	  elog_die(FATAL, "[13] cant find aaa");
     if (tree_find(ringroots, "bbb") == TREE_NOVAL)
	  elog_die(FATAL, "[13] cant find bbb");
     if (tree_find(ringroots, "ccc") == TREE_NOVAL)
	  elog_die(FATAL, "[13] cant find ccc");

     tree_clearoutandfree(ringroots);
     tree_destroy(ringroots);
     table_destroy(spans);

     /* test 14, get rings by name: check time range return */
     spans = spans_readringblocks(ts_holstore(ts));
     if ( ! spans )
          elog_die(FATAL, "[14] no list returned");

     ringlist = spans_getrings_byrootandtime(spans, "aaa", 0, 5, &fromdt,
					     &todt);
     if (fromdt != 100)
	  elog_die(FATAL, "[14a1] from dt %d != 100", fromdt);
     if (todt != 600)
	  elog_die(FATAL, "[14a2] to dt %d != 600", todt);
     if (ringlist != NULL)
	  elog_die(FATAL, "[14a3] didnt return NULL");

     ringlist = spans_getrings_byrootandtime(spans, "bbb", 1, 2, &fromdt,
					     &todt);
     if (fromdt != 700)
	  elog_die(FATAL, "[14b1] from dt %d != 700", fromdt);
     if (todt != 800)
	  elog_die(FATAL, "[14b2] to dt %d != 800", todt);
     if (ringlist != NULL)
	  elog_die(FATAL, "[14b3] didnt return NULL");

     ringlist = spans_getrings_byrootandtime(spans, "ccc", 1, 2, &fromdt,
					     &todt);
     if (fromdt != 900)
	  elog_die(FATAL, "[14c1] from dt %d != 900", fromdt);
     if (todt != 1400)
	  elog_die(FATAL, "[14c2] to dt %d != 1400", todt);
     if (ringlist != NULL)
	  elog_die(FATAL, "[14c3] didnt return NULL");

     /* test 15, get rings by time: check time returns */
     ringlist = spans_getrings_byrootandtime(spans, "aaa", 100, 600, &fromdt,
					     &todt);
     if (fromdt != 100)
	  elog_die(FATAL, "[15a] from dt %d != 100", fromdt);
     if (todt != 600)
	  elog_die(FATAL, "[15a] to dt %d != 600", todt);
     if (tree_n(ringlist) != 3)
	  elog_die(FATAL, "[15a] list didnt return 3");
     if (tree_find(ringlist, TESTRING2) == TREE_NOVAL)
	  elog_die(FATAL, "[15a] cant find %s", TESTRING2);
     if (tree_find(ringlist, TESTRING3) == TREE_NOVAL)
	  elog_die(FATAL, "[15a] cant find %s", TESTRING3);
     if (tree_find(ringlist, TESTRING4) == TREE_NOVAL)
	  elog_die(FATAL, "[15a] cant find %s", TESTRING4);
     tree_destroy(ringlist);

     ringlist = spans_getrings_byrootandtime(spans, "aaa", 300, 500, &fromdt,
					     &todt);
     if (fromdt != 100)
	  elog_die(FATAL, "[15b] from dt %d != 100", fromdt);
     if (todt != 600)
	  elog_die(FATAL, "[15b] to dt %d != 600", todt);
     if (tree_n(ringlist) != 2)
	  elog_die(FATAL, "[15b] list didnt return 3");
     if (tree_find(ringlist, TESTRING3) == TREE_NOVAL)
	  elog_die(FATAL, "[15b] cant find %s", TESTRING3);
     if (tree_find(ringlist, TESTRING4) == TREE_NOVAL)
	  elog_die(FATAL, "[15b] cant find %s", TESTRING4);
     tree_destroy(ringlist);

     ringlist = spans_getrings_byrootandtime(spans, "aaa", 300, 500, &fromdt,
					     &todt);
     if (fromdt != 100)
	  elog_die(FATAL, "[15b] from dt %d != 100", fromdt);
     if (todt != 600)
	  elog_die(FATAL, "[15b] to dt %d != 600", todt);
     if (tree_n(ringlist) != 2)
	  elog_die(FATAL, "[15b] list didnt return 2");
     if (tree_find(ringlist, TESTRING3) == TREE_NOVAL)
	  elog_die(FATAL, "[15b] cant find %s", TESTRING3);
     if (tree_find(ringlist, TESTRING4) == TREE_NOVAL)
	  elog_die(FATAL, "[15b] cant find %s", TESTRING4);
     tree_destroy(ringlist);

     ringlist = spans_getrings_byrootandtime(spans, "bbb", 731, 732, &fromdt,
					     &todt);
     if (fromdt != 700)
	  elog_die(FATAL, "[15c] from dt %d != 700", fromdt);
     if (todt != 800)
	  elog_die(FATAL, "[15c] to dt %d != 800", todt);
     if (tree_n(ringlist) != 1)
	  elog_die(FATAL, "[15c] list didnt return 1");
     if (tree_find(ringlist, TESTRING5) == TREE_NOVAL)
	  elog_die(FATAL, "[15c] cant find %s", TESTRING3);
     tree_destroy(ringlist);

     ringlist = spans_getrings_byrootandtime(spans, "bbb", 801, 802, &fromdt,
					     &todt);
     if (fromdt != 700)
	  elog_die(FATAL, "[15d] from dt %d != 700", fromdt);
     if (todt != 800)
	  elog_die(FATAL, "[15d] to dt %d != 800", todt);
     if (ringlist != NULL)
	  elog_die(FATAL, "[15d] didnt return NULL");

     ringlist = spans_getrings_byrootandtime(spans, "ccc", 100, 600, &fromdt,
					     &todt);
     if (fromdt != 900)
	  elog_die(FATAL, "[15e] from dt %d != 900", fromdt);
     if (todt != 1400)
	  elog_die(FATAL, "[15e] to dt %d != 1400", todt);
     if (ringlist != NULL)
	  elog_die(FATAL, "[15e] didnt return NULL");

     ringlist = spans_getrings_byrootandtime(spans, "ccc", 999, 1300, &fromdt,
					     &todt);
     if (fromdt != 900)
	  elog_die(FATAL, "[15f] from dt %d != 900", fromdt);
     if (todt != 1400)
	  elog_die(FATAL, "[15f] to dt %d != 1400", todt);
     if (tree_n(ringlist) != 3)
	  elog_die(FATAL, "[15f] list didnt return 3");
     if (tree_find(ringlist, TESTRING6) == TREE_NOVAL)
	  elog_die(FATAL, "[15f] cant find %s", TESTRING6);
     if (tree_find(ringlist, TESTRING7) == TREE_NOVAL)
	  elog_die(FATAL, "[15f] cant find %s", TESTRING7);
     if (tree_find(ringlist, TESTRING8) == TREE_NOVAL)
	  elog_die(FATAL, "[15f] cant find %s", TESTRING8);
     tree_destroy(ringlist);

     ringlist = spans_getrings_byrootandtime(spans, "ccc", 999, 1300, &fromdt,
					     &todt);
     if (fromdt != 900)
	  elog_die(FATAL, "[15f] from dt %d != 900", fromdt);
     if (todt != 1400)
	  elog_die(FATAL, "[15f] to dt %d != 1400", todt);
     if (tree_n(ringlist) != 3)
	  elog_die(FATAL, "[15f] list didnt return 3");
     if (tree_find(ringlist, TESTRING6) == TREE_NOVAL)
	  elog_die(FATAL, "[15f] cant find %s", TESTRING6);
     if (tree_find(ringlist, TESTRING7) == TREE_NOVAL)
	  elog_die(FATAL, "[15f] cant find %s", TESTRING7);
     if (tree_find(ringlist, TESTRING8) == TREE_NOVAL)
	  elog_die(FATAL, "[15f] cant find %s", TESTRING8);
     tree_destroy(ringlist);

     ringlist = spans_getrings_byrootandtime(spans, "ccc", 1001, 1300, 
					     &fromdt, &todt);
     if (fromdt != 900)
	  elog_die(FATAL, "[15g] from dt %d != 900", fromdt);
     if (todt != 1400)
	  elog_die(FATAL, "[15g] to dt %d != 1400", todt);
     if (tree_n(ringlist) != 2)
	  elog_die(FATAL, "[15g] list didnt return 2");
     if (tree_find(ringlist, TESTRING7) == TREE_NOVAL)
	  elog_die(FATAL, "[15g] cant find %s", TESTRING7);
     if (tree_find(ringlist, TESTRING8) == TREE_NOVAL)
	  elog_die(FATAL, "[15g] cant find %s", TESTRING8);
     tree_destroy(ringlist);

     ringlist = spans_getrings_byrootandtime(spans, "ccc", 1001, 2000, 
					     &fromdt, &todt);
     if (fromdt != 900)
	  elog_die(FATAL, "[15h] from dt %d != 900", fromdt);
     if (todt != 1400)
	  elog_die(FATAL, "[15h] to dt %d != 1400", todt);
     if (tree_n(ringlist) != 2)
	  elog_die(FATAL, "[15h] list didnt return 2");
     if (tree_find(ringlist, TESTRING7) == TREE_NOVAL)
	  elog_die(FATAL, "[15h] cant find %s", TESTRING7);
     if (tree_find(ringlist, TESTRING8) == TREE_NOVAL)
	  elog_die(FATAL, "[15h] cant find %s", TESTRING8);
     tree_destroy(ringlist);

     ringlist = spans_getrings_byrootandtime(spans, "ccc", 1302, 2000, 
					     &fromdt, &todt);
     if (fromdt != 900)
	  elog_die(FATAL, "[15i] from dt %d != 900", fromdt);
     if (todt != 1400)
	  elog_die(FATAL, "[15i] to dt %d != 1400", todt);
     if (tree_n(ringlist) != 1)
	  elog_die(FATAL, "[15i] list didnt return 1");
     if (tree_find(ringlist, TESTRING8) == TREE_NOVAL)
	  elog_die(FATAL, "[15i] cant find %s", TESTRING8);
     tree_destroy(ringlist);

     ringlist = spans_getrings_byrootandtime(spans, "ddd", 1, 2000, &fromdt, 
					     &todt);
     if (ringlist != NULL)
	  elog_die(FATAL, "[15j] didnt return NULL");

     table_destroy(spans);

     ts_close(ts);

     elog_fini();
     route_close(err);
     route_fini();

     printf("tests finished successfully\n");
     exit(0);
}

#endif /* TEST */
