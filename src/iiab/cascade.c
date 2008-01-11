/*
 * Cascade sampling class.
 * Nigel Stuckey, November 1999.
 *
 * Copyright System Garden Ltd 1999-2001, all rights reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <values.h>
#include "cascade.h"
#include "elog.h"
#include "tableset.h"
#include "table.h"
#include "route.h"
#include "util.h"


/* 
 * Cascade samples sequences of data from tables or sequence aware 
 * ROUTES (such as ringstore (rs:) and SQL ringstore (sqlrs:)) and
 * aggregates the values to produce computed summaries of the fields.
 *
 * There are two imput methods: either directly submiting the whole
 * dataset of multiple samples or by giving a route that will contain
 * that information.
 *
 * A route is opened and positioned after the last sequence: work will
 * start from that point. The position is then remembered for the duration
 * of the session. Each time the sample action is called it will catch up 
 * with all the intervening entries and write a summary to the output route. 
 * If there are no entries since last time, nothing is generated. 
 * If there is 1 entry since last time, that entry is echoed.
 * For 2 or more entries, the calculations are carried out and a single
 * entry is output.
 *
 * For multi instance data, only records that correspond to matching
 * keys will be processed together. Eg, disk data for 'sd0a' will always
 * be compared to other 'sd0a' data.
 * 
 * The cascade function should be one of the following:-
 *
 *    CASCADE_AVG  - Calculate average of the sample run
 *    CASCADE_MIN  - Find minimum number in the sample run
 *    CASCADE_MAX  - Find maximum number in the sample run
 *    CASCADE_SUM  - Calculate the sum of the corresponding figures
 *    CASCADE_LAST - Echo the last set of figures (same result as snap method)
 *    CASCADE_FIRST- Echo the last set of figures (same result as snap method)
 *    CASCADE_DIFF - Echo the last set of figures (same result as snap method)
 *    CASCADE_RATE - Calculate mean rate, to get per sec figures. cf. avg
 *
 * Counters are not dealt with by cascade: the data is expected to
 * to be in a standard, absolute format.
 *
 * The algorithms are as follows:-
 *
 *   op   | algorithm
 *   -----+----------------------------------------------------------------
 *   avg  | add together figures from each sequence, with corresponding 
 *        | keys within a column. divide by the number of samples
 *   min  | find the lowest figure from the set defined by key+column
 *   max  | find the highest figure from the set defined by key+column
 *   sum  | add together the figures from the set defined by key+column
 *   last | the value of key,column from the last sequence in the set
 *   first| the value of key,column from the first sequence in the set
 *   diff | difference between the first and last values in set
 *   rate | difference between the first and last values in set, divided
 *        | by seconds the set covers
 *
 * The monitored route does not need to exist.
 *
 * Note: that avg will produce an average using the number of samples as
 * it divisor, which is not always desirable.
 * Rate, however, will divide the accumulated sum with the time 
 * difference+first duration in seconds, yeilding a per-second rate.
 * It is safe to use rate as a method for chains of samples.
 *
 * Returns -1 if there was an error, such as the input route not existing
 * or if the input route was not a tablestore.
 */

/*
 * Initialise a cascade session.
 * The route is opened now for monitoring, run cascade_sample() to
 * process changes from this point onwards.
 * Returns the type CASCADE if successful of NULL otherwise.
 */
CASCADE *cascade_init(enum cascade_fn func,	/* cascading function */
		      char *monroute		/* p-url route to monitor */ )
{
     struct cascade_info *session;
     int seq, size, r;
     time_t modt;

     r = route_stat(monroute, NULL, &seq, &size, &modt);

     /* save all the details in session structure, the sequence
      * is that of the last one right now */
     session = nmalloc( sizeof(struct cascade_info) );
     session->fn   = func;
     session->purl = xnstrdup(monroute);
     session->seq  = r ? seq+1 : 0;

     elog_printf(DEBUG, "cascade type %d init on %s from seq %d", 
		 func, monroute, seq+1);

     return session;
}


/*
 * End the monitoring of the tablestore and free its referencies
 * You will not be able to use the CASCADE reference after this call.
 */
void cascade_fini(CASCADE *session)
{
     nfree(session->purl);
     nfree(session);
}


/*
 * Sample the tablestore ring set up by cascade_init and described in CASCADE.
 * See the description above for how the thing works.
 * The computed table is sent to the output route and errors are sent to 
 * the error route. Returns 1 for success or 0 for failure.
 */
int cascade_sample(CASCADE *session,	/* cascade reference */
		   ROUTE output,	/* output route */
		   ROUTE error		/* error route */ )
{
     ROUTE rt;
     int r, seq, size;
     time_t modt;
     TABLE dataset, result;

     /* attempt to read from the current point to the last */
     rt = route_open(session->purl, NULL, NULL, 0);
     if ( ! rt )
          return 0;	/* monitored route does not exist (yet).
			 * so return successfully so we can try again 
			 * next time */
     dataset = route_seektread(rt, session->seq, -1);
     r = route_tell(rt, &seq, &size, &modt);
     route_close(rt);
     session->seq = seq+1;

     /* now carry out the aggregation on the table */
     result = cascade_aggregate(session->fn, dataset);

     /* save the results */
     if ( result ) {
          if ( ! route_twrite(output, result) )
	       elog_printf(ERROR, "unable to write result");
	  table_destroy(result);
     }
     if (dataset)
          table_destroy(dataset);
     
     return 1;
}



/*
 * Carry out aggregation on a complete data set held in a table
 * This is an alternative entry point to the class that does not need
 * the setting up of a session.
 * The table should identify keys, time, sequence and duration in the 
 * standard way as defined by FHA spec.
 * Returns a TABLE of results compliant to the FHA spec, _time will be
 * set to the last time of the dataset, _seq to 0. _dur is not set
 * The result is independent of dataset's memory allocation, sothe caller
 * needs to run table_destroy() to free its memory.
 * Returns NULL if there is an error, if dataset is NULL or if there
 * was insufficent data.
 */
TABLE cascade_aggregate(enum cascade_fn func, 	/* aggregation function */
			TABLE dataset		/* multi-sample, multi-key
						 * dataset in a table */ )
{
     TREE *inforow, *keyvals, *databykey, *colnames;
     char *keycol, *colname, *tmpstr, *type;
     int duration, haskey=0;
     TABLE itab, result;
     TABSET tset;
     ITREE *col;
     double val, tmpval1, tmpval2;
     time_t t1, t2, tdiff;

     /* assert special cases */
     if ( ! dataset )
          return NULL;
     if (table_nrows(dataset) == 0)
          return NULL;
     if ( ! table_hascol(dataset, "_time")) {
          tmpstr = table_outheader(dataset);
          elog_printf(ERROR, "attempting to aggregate a table without _time "
		      "column (columns: %s)", tmpstr);
	  nfree(tmpstr);
          return NULL;
     }

     /* find any keys that might exist */
     inforow = table_getinforow(dataset, "key");
     if (inforow) {
          keycol = tree_search(inforow, "1", 2);
	  if (keycol) {
	       keyvals = table_uniqcolvals(dataset, keycol, NULL);
	       if (keyvals) {
		    /* separate the combined data set into ones of
		     * separate keys */
		    haskey++;
		    databykey = tree_create();
		    tset = tableset_create(dataset);
		    tree_traverse(keyvals) {
		         /* select out the data */
		         tableset_reset(tset);
			 tableset_where(tset, keycol, eq, 
					tree_getkey(keyvals));
			 itab = tableset_into(tset);
		         tree_add(databykey, tree_getkey(keyvals), itab);
		    }
		    tableset_destroy(tset);
	       }
	       tree_destroy(keyvals);
	  }
	  tree_destroy(inforow);
     }

     /* if there were no keys found, pretend that we have a single one */
     if ( ! haskey ) {
          databykey = tree_create();
	  tree_add(databykey, "nokey", dataset);
     }

     /* find the time span and duration of the dataset */
     table_first(dataset);
     if (table_hascol(dataset, "_dur"))
          duration = strtol(table_getcurrentcell(dataset, "_dur"), NULL, 10);
     else
          duration = 0;
     t1 = strtol(table_getcurrentcell(dataset, "_time"), NULL, 10);
     table_last(dataset);
     t2 = strtol(table_getcurrentcell(dataset, "_time"), NULL, 10);
     tdiff = t2-t1+duration;

     /* go over the keyed table and apply our operators to each column 
      * in turn */
     result = table_create_fromdonor(dataset);
     table_addcol(result, "_seq", NULL);	/* make col before make row */
     table_addcol(result, "_time", NULL);
     table_addcol(result, "_dur", NULL);
     tree_traverse(databykey) {
          table_addemptyrow(result);
          itab = tree_get(databykey);
	  colnames = table_getheader(itab);
	  tree_traverse(colnames) {
	       colname = tree_getkey(colnames);
	       if ( ! table_hascol(result, colname)) {
		     tmpstr = xnstrdup(colname);
		     table_addcol(result, tmpstr, NULL);
		     table_freeondestroy(result, tmpstr);
	       }
	       col = table_getcol(itab, colname);
	       type = table_getinfocell(itab, "type", colname);
	       if (type && strcmp(type, "str") == 0) {
		    /* string value: report the last one */
		    itree_last(col);
		    table_replacecurrentcell(result, colname, itree_get(col));
	       } else if (strcmp(colname, "_dur") == 0) {
		    /* _dur: use the last value, can treat as string */
		    itree_last(col);
		    table_replacecurrentcell(result, "_dur", itree_get(col));
	       } else if (strcmp(colname, "_seq") == 0) {
		    /* _seq: only one result is produced so must be 0 */
		    table_replacecurrentcell(result, "_seq", "0");
	       } else if (strcmp(colname, "_time") == 0) {
		    /* _time: use the last value, can treat as string */
		    itree_last(col);
		    table_replacecurrentcell(result, "_time", itree_get(col));
	       } else {
		    /* numeric value: treat as a float and report it */
		    switch (func) {
		    case CASCADE_AVG:
		         /* average of column's values */
		         val = 0.0;
			 itree_traverse(col)
			      val += atof( itree_get(col) );
			 val = val / itree_n(col);
			 break;
		    case CASCADE_MIN:
		         /* minimum of column's values */
		         val = MAXDOUBLE;
			 itree_traverse(col) {
			      tmpval1 = atof( itree_get(col) );
			      if (tmpval1 < val)
				   val = tmpval1;
			 }
			 break;
		    case CASCADE_MAX:
		         /* maximum of column's values */
		         val = MINDOUBLE;
			 itree_traverse(col) {
			      tmpval1 = atof( itree_get(col) );
			      if (tmpval1 > val)
				   val = tmpval1;
			 }
			 break;
		    case CASCADE_SUM:
		         /* sum of column's values */
		         val = 0.0;
			 itree_traverse(col)
			      val += atof( itree_get(col) );
			 break;
		    case CASCADE_LAST:
		         /* last value */
		         itree_last(col);
			 val = atof( itree_get(col) );
			 break;
		    case CASCADE_FIRST:
		         /* last value */
		         itree_first(col);
			 val = atof( itree_get(col) );
			 break;
		    case CASCADE_DIFF:
		         /* the difference in values of first and last */
		         itree_first(col);
			 tmpval1 = atof( itree_get(col) );
			 itree_last(col);
			 tmpval2 = atof( itree_get(col) );
			 val = tmpval2 - tmpval1;
			 break;
		    case CASCADE_RATE:
		         /* difference in values (as CASCADE_DIFF) then 
			  * divided by the number of seconds in the set  */
		         itree_first(col);
			 tmpval1 = atof( itree_get(col) );
			 itree_last(col);
			 tmpval2 = atof( itree_get(col) );
			 val = tmpval2 - tmpval1;
			 val = val / tdiff;
			 break;
		    }
		    /* save the floating point value */
		    table_replacecurrentcell_alloc(result, colname, 
						   util_ftoa(val));
	       }
	       itree_destroy(col);
	  }
	  /* make sure that there are values for the special columns */
	  if ( ! table_hascol(dataset, "_time"))
	       table_replacecurrentcell(result, "_time", 
					util_decdatetime(time(NULL)));
	  if ( ! table_hascol(dataset, "_seq"))
	       table_replacecurrentcell(result, "_seq", "0");
	  if ( ! table_hascol(dataset, "_dur"))
	       table_replacecurrentcell(result, "_dur", "0");
     }

     /* clear up */
     if (haskey) {
          tree_traverse(databykey) {
	       itab = tree_get(databykey);
	       table_destroy(itab);
	  }
     }
     tree_destroy(databykey);

     return result;
}





#if TEST

#include <unistd.h>
#include "rt_file.h"
#include "rt_std.h"
#include "rt_rs.h"

#define TAB_SING        "_time\tcol1\tcol2\tcol3\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n"
#define TAB_SINGINFO    "_time\tcol1\tcol2\tcol3\n" \
                        "\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n"
#define TAB_SINGINFOKEY "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\tthing1\n" \
                        "5\t10.00\t20.00\t30.00\tthing2\n" \
                        "5\t100.00\t200.00\t300.00\tthing3\n"
#define TAB_MULT        "_time\tcol1\tcol2\tcol3\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n" \
                        "10\t1.00\t2.00\t3.00\n" \
                        "15\t1.00\t2.00\t3.00\n"
#define TAB_MULTINFO    "_time\tcol1\tcol2\tcol3\n" \
                        "\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n" \
                        "10\t1.00\t2.00\t3.00\n" \
                        "15\t1.00\t2.00\t3.00\n"
#define TAB_MULTINFOKEY "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\tthing1\n" \
                        "5\t10.00\t20.00\t30.00\tthing2\n" \
                        "5\t100.00\t200.00\t302.00\tthing3\n" \
                        "10\t1.00\t2.00\t3.00\tthing1\n" \
                        "10\t16.00\t23.00\t30.00\tthing2\n" \
                        "10\t108.00\t200.00\t304.00\tthing3\n" \
                        "15\t1.00\t2.00\t3.00\tthing1\n" \
                        "15\t18.00\t26.00\t30.00\tthing2\n" \
                        "15\t106.00\t200.00\t300.00\tthing3\n"
#define RES_AVGSING     "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n"
#define RES_AVGSINGKEY  "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\tthing1\n" \
                        "5\t10.00\t20.00\t30.00\tthing2\n" \
                        "5\t100.00\t200.00\t300.00\tthing3\n"
#define RES_AVGMULT     "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\n"
#define RES_AVGMULTKEY  "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\tthing1\n" \
                        "15\t14.67\t23.00\t30.00\tthing2\n" \
                        "15\t104.67\t200.00\t302.00\tthing3\n"
#define RES_MINSING     "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n"
#define RES_MINSINGKEY  "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\tthing1\n" \
                        "5\t10.00\t20.00\t30.00\tthing2\n" \
                        "5\t100.00\t200.00\t300.00\tthing3\n"
#define RES_MINMULT     "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\n"
#define RES_MINMULTKEY  "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\tthing1\n" \
                        "15\t10.00\t20.00\t30.00\tthing2\n" \
                        "15\t100.00\t200.00\t300.00\tthing3\n"
#define RES_MAXSING     "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n"
#define RES_MAXSINGKEY  "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\tthing1\n" \
                        "5\t10.00\t20.00\t30.00\tthing2\n" \
                        "5\t100.00\t200.00\t300.00\tthing3\n"
#define RES_MAXMULT     "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\n"
#define RES_MAXMULTKEY  "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\tthing1\n" \
                        "15\t18.00\t26.00\t30.00\tthing2\n" \
                        "15\t108.00\t200.00\t304.00\tthing3\n"
#define RES_SUMSING     "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n"
#define RES_SUMSINGKEY  "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\tthing1\n" \
                        "5\t10.00\t20.00\t30.00\tthing2\n" \
                        "5\t100.00\t200.00\t300.00\tthing3\n"
#define RES_SUMMULT     "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "15\t3.00\t6.00\t9.00\n"
#define RES_SUMMULTKEY  "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "15\t3.00\t6.00\t9.00\tthing1\n" \
                        "15\t44.00\t69.00\t90.00\tthing2\n" \
                        "15\t314.00\t600.00\t906.00\tthing3\n"
#define RES_FIRSTSING   "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n"
#define RES_FIRSTSINGKEY "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\tthing1\n" \
                        "5\t10.00\t20.00\t30.00\tthing2\n" \
                        "5\t100.00\t200.00\t300.00\tthing3\n"
#define RES_FIRSTMULT   "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\n"
#define RES_FIRSTMULTKEY "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\tthing1\n" \
                        "15\t10.00\t20.00\t30.00\tthing2\n" \
                        "15\t100.00\t200.00\t302.00\tthing3\n"
#define RES_LASTSING    "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\n"
#define RES_LASTSINGKEY "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "5\t1.00\t2.00\t3.00\tthing1\n" \
                        "5\t10.00\t20.00\t30.00\tthing2\n" \
                        "5\t100.00\t200.00\t300.00\tthing3\n"
#define RES_LASTMULT    "_time\tcol1\tcol2\tcol3\n" \
                        "\"\"\ttom\tdick\tharry\tinfo\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\n"
#define RES_LASTMULTKEY "_time\tcol1\tcol2\tcol3\tthing\n" \
                        "\"\"\ttom\tdick\tharry\tinst\tinfo\n" \
                        "-\t-\t-\t-\t1\tkey\n" \
                        "i32\t2dp\t2dp\t2dp\tstr\ttype\n" \
                        "--\n" \
                        "15\t1.00\t2.00\t3.00\tthing1\n" \
                        "15\t18.00\t26.00\t30.00\tthing2\n" \
                        "15\t106.00\t200.00\t300.00\tthing3\n"

#define RS_SAMPFILE "t.cascade.rs"
#define RS_SAMPRING "sample"
#define RS_SAMPPURL "rs:" RS_SAMPFILE "," RS_SAMPRING ",0"
#define RS_RESFILE  "t.cascade.rs"
#define RS_RESRING  "result"
#define RS_RESPURL  "rs:" RS_RESFILE "," RS_RESRING ",0"

ROUTE err, out;

void test_cascade(enum cascade_fn mode, 
		  char *mode_label,
		  char *tab_sing, 
		  char *tab_singinfo,
		  char *tab_singinfokey,
		  char *tab_mult, 
		  char *tab_multinfo,
		  char *tab_multinfokey,
		  char *result_sing,
		  char *result_singkey,
		  char *result_mult,
		  char *result_multkey);

int main(int argc1, char *argv[])
{
     route_init(NULL, 0);
     route_register(&rt_filea_method);
     route_register(&rt_fileov_method);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     route_register(&rt_rs_method);
     if ( ! elog_init(1, "cascade test", NULL))
	  elog_die(FATAL, "didn't initialise elog\n");
     out = route_open("stdout", NULL, NULL, 0);
     err = route_open("stderr", NULL, NULL, 0);
     rs_init();

     /* run cascade with all the possible modes */
     test_cascade(CASCADE_AVG, "avg", 
		  TAB_SING, TAB_SINGINFO, TAB_SINGINFOKEY,
		  TAB_MULT, TAB_MULTINFO, TAB_MULTINFOKEY,
		  RES_AVGSING, RES_AVGSINGKEY, RES_AVGMULT, RES_AVGMULTKEY);
     test_cascade(CASCADE_MIN, "min", 
		  TAB_SING, TAB_SINGINFO, TAB_SINGINFOKEY,
		  TAB_MULT, TAB_MULTINFO, TAB_MULTINFOKEY,
		  RES_MINSING, RES_MINSINGKEY, RES_MINMULT, RES_MINMULTKEY);
     test_cascade(CASCADE_MAX, "max", 
		  TAB_SING, TAB_SINGINFO, TAB_SINGINFOKEY,
		  TAB_MULT, TAB_MULTINFO, TAB_MULTINFOKEY,
		  RES_MAXSING, RES_MAXSINGKEY, RES_MAXMULT, RES_MAXMULTKEY);
     test_cascade(CASCADE_SUM, "sum", 
		  TAB_SING, TAB_SINGINFO, TAB_SINGINFOKEY,
		  TAB_MULT, TAB_MULTINFO, TAB_MULTINFOKEY,
		  RES_SUMSING, RES_SUMSINGKEY, RES_SUMMULT, RES_SUMMULTKEY);
     test_cascade(CASCADE_FIRST, "first", 
		  TAB_SING, TAB_SINGINFO, TAB_SINGINFOKEY,
		  TAB_MULT, TAB_MULTINFO, TAB_MULTINFOKEY,
		  RES_FIRSTSING, RES_FIRSTSINGKEY, RES_FIRSTMULT, 
		  RES_FIRSTMULTKEY);
     test_cascade(CASCADE_LAST, "last", 
		  TAB_SING, TAB_SINGINFO, TAB_SINGINFOKEY,
		  TAB_MULT, TAB_MULTINFO, TAB_MULTINFOKEY,
		  RES_LASTSING, RES_LASTSINGKEY, RES_LASTMULT, 
		  RES_LASTMULTKEY);

     rs_fini();
     elog_fini();
     route_close(err);
     route_close(out);
     route_fini();
     printf("tests finished successfully\n");
     exit(0);
}


/* run though a series of tests for the current cascade configuration */
void test_cascade(enum cascade_fn mode, 
		  char *mode_label,
		  char *tab_sing, 
		  char *tab_singinfo,
		  char *tab_singinfokey,
		  char *tab_mult, 
		  char *tab_multinfo,
		  char *tab_multinfokey,
		  char *result_sing,
		  char *result_singkey,
		  char *result_mult,
		  char *result_multkey)
{
     int r, resseq, resoff;
     ROUTE resrt, samprt;
     CASCADE *cas;
     char *buf1, *resbuf1, *wantbuf1;
     TABLE tab1, restab1, wanttab1;
     time_t modt;

     /* [1] run cascade aggregation on empty tables */
     tab1 = table_create();
     restab1 = cascade_aggregate(mode, tab1);
     if (restab1)
	  elog_die(FATAL, "[1] should return NULL when aggregating an "
		   "empty table");

     /* [2] aggregate a single sample table, no info, no key */
     buf1 = xnstrdup(tab_sing);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     wanttab1 = table_create();
     wantbuf1 = xnstrdup(result_sing);
     table_scan(wanttab1, wantbuf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(wanttab1, wantbuf1);
     table_rminfo(wanttab1, "info");	/* remove info line */
     restab1 = cascade_aggregate(mode, tab1);
     if ( ! restab1)
	  elog_die(FATAL, "[2a] can't aggregate table: "
		   "mode %s, single sample, no info, no key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1     = table_outtable(tab1);
     resbuf1  = table_outtable(restab1);
     wantbuf1 = table_outtable(wanttab1);
     if (strcmp(wantbuf1, resbuf1))
          elog_die(FATAL, "[2b] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, wantbuf1);
     nfree(buf1);
     nfree(resbuf1);
     nfree(wantbuf1);
     table_destroy(tab1);
     table_destroy(restab1);
     table_destroy(wanttab1);

     /* [3] aggregate a single sample table, with info but no key */
     tab1 = table_create();
     buf1 = xnstrdup(tab_singinfo);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     restab1 = cascade_aggregate(mode, tab1);
     if ( ! restab1)
	  elog_die(FATAL, "[3a] can't aggregate table: "
		   "mode %s, single sample, with info, no key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1    = table_outtable(tab1);
     resbuf1 = table_outtable(restab1);
     if (strcmp(result_sing, resbuf1))
          elog_die(FATAL, "[3b] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, result_sing);
     nfree(buf1);
     nfree(resbuf1);
     table_destroy(tab1);
     table_destroy(restab1);

     /* [4] aggregate a single sample table, with info and key */
     tab1 = table_create();
     buf1 = xnstrdup(tab_singinfokey);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     restab1 = cascade_aggregate(mode, tab1);
     if ( ! restab1)
	  elog_die(FATAL, "[4a] can't aggregate table: "
		   "mode %s, single sample, with info and key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1    = table_outtable(tab1);
     resbuf1 = table_outtable(restab1);
     if (strcmp(result_singkey, resbuf1))
          elog_die(FATAL, "[4b] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, result_singkey);
     nfree(buf1);
     nfree(resbuf1);
     table_destroy(tab1);
     table_destroy(restab1);

     /* [5] aggregate a multi sample table, no info, no key */
     tab1 = table_create();
     buf1 = xnstrdup(tab_mult);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     wanttab1 = table_create();
     wantbuf1 = xnstrdup(result_mult);
     table_scan(wanttab1, wantbuf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(wanttab1, wantbuf1);
     table_rminfo(wanttab1, "info");	/* remove info line */
     restab1 = cascade_aggregate(mode, tab1);
     if ( ! restab1)
	  elog_die(FATAL, "[5a] can't aggregate table: "
		   "mode %s, multi sample, no info, no key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1     = table_outtable(tab1);
     resbuf1  = table_outtable(restab1);
     wantbuf1 = table_outtable(wanttab1);
     if (strcmp(wantbuf1, resbuf1))
          elog_die(FATAL, "[5b] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, wantbuf1);
     nfree(buf1);
     nfree(resbuf1);
     nfree(wantbuf1);
     table_destroy(tab1);
     table_destroy(restab1);
     table_destroy(wanttab1);

     /* [6] aggregate a multi sample table, with info but no key */
     tab1 = table_create();
     buf1 = xnstrdup(tab_multinfo);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     restab1 = cascade_aggregate(mode, tab1);
     if ( ! restab1)
	  elog_die(FATAL, "[6a] can't aggregate table: "
		   "mode %s, multi sample, with info, no key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1    = table_outtable(tab1);
     resbuf1 = table_outtable(restab1);
     if (strcmp(result_mult, resbuf1))
          elog_die(FATAL, "[6b] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, result_mult);
     nfree(buf1);
     nfree(resbuf1);
     table_destroy(tab1);
     table_destroy(restab1);

     /* [7] aggregate a multi sample table, with info and key */
     tab1 = table_create();
     buf1 = xnstrdup(tab_multinfokey);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     restab1 = cascade_aggregate(mode, tab1);
     if ( ! restab1)
	  elog_die(FATAL, "[7a] can't aggregate table: "
		   "mode %s, multi sample, with info and key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1    = table_outtable(tab1);
     resbuf1 = table_outtable(restab1);
     if (strcmp(result_multkey, resbuf1))
          elog_die(FATAL, "[7b] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, result_multkey);
     nfree(buf1);
     nfree(resbuf1);
     table_destroy(tab1);
     table_destroy(restab1);


     /*
      * And now the route based methods.
      * We run through the same tests using routes to store the samples
      * also the results
      */

     /* unlink previous storage */
     unlink(RS_SAMPFILE);
     unlink(RS_RESFILE);

     /* [8] create sample and result routes */
     resrt = route_open(RS_RESPURL, "Output of testing results", NULL, 20);
     if ( ! resrt )
	  elog_die(FATAL, "[8a] Can't open result route");

     samprt = route_open(RS_SAMPPURL, "Samples under test", NULL, 20);
     if ( ! samprt )
	  elog_die(FATAL, "[8b] Can't open result route");

     /* [9] run cascade on an empty ring and sample several times 
      * where there is no change */
     cas = cascade_init(mode, RS_SAMPPURL);
     if ( ! cas)
	  elog_die(FATAL, "[9a] can't start cascade");
     r = cascade_sample(cas, out, err);
     if ( !r )
	  elog_die(FATAL, "[9b] cascade sample failed");
     r = cascade_sample(cas, out, err);
     if ( !r )
	  elog_die(FATAL, "[9c] cascade sample failed");

     /* [10] add tab_sing to table and sample */
     tab1 = table_create();
     buf1 = xnstrdup(tab_sing);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     wanttab1 = table_create();
     wantbuf1 = xnstrdup(result_sing);
     table_scan(wanttab1, wantbuf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(wanttab1, wantbuf1);
     table_rminfo(wanttab1, "info");	/* remove info line */
     r = route_twrite(samprt, tab1);
     if ( r < 0 )
	  elog_die(FATAL, "[10a] add table failed");
     r = cascade_sample(cas, resrt, err);
     if ( !r )
          elog_die(FATAL, "[10b] cascade sample failed, mode %s", mode_label);
     r = route_tell(resrt, &resseq, &resoff, &modt);
     if ( !r )
          elog_die(FATAL, "[10c] can't route_tell(), mode %s", mode_label);
     restab1 = route_seektread(resrt, resseq, 0);
     if ( ! restab1)
	  elog_die(FATAL, "[10d] can't read result ring: "
		   "mode %s, single sample, no info, no key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1     = table_outtable(tab1);
     resbuf1  = table_outtable(restab1);
     wantbuf1 = table_outtable(wanttab1);
     if (strcmp(wantbuf1, resbuf1))
          elog_die(FATAL, "[10e] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, wantbuf1);
     nfree(buf1);
     nfree(resbuf1);
     nfree(wantbuf1);
     table_destroy(tab1);
     table_destroy(restab1);
     table_destroy(wanttab1);

     /* [11] add tab_singinfo to table and sample */
     tab1 = table_create();
     buf1 = xnstrdup(tab_singinfo);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     r = route_twrite(samprt, tab1);
     if ( r < 0 )
	  elog_die(FATAL, "[11a] add table failed");
     r = cascade_sample(cas, resrt, err);
     if ( !r )
          elog_die(FATAL, "[11b] cascade sample failed, mode %s", mode_label);
     r = route_tell(resrt, &resseq, &resoff, &modt);
     if ( !r )
          elog_die(FATAL, "[11c] can't route_tell(), mode %s", mode_label);
     restab1 = route_seektread(resrt, resseq, 0);
     if ( ! restab1)
	  elog_die(FATAL, "[11d] can't read result ring: "
		   "mode %s, single sample, with info, no key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1     = table_outtable(tab1);
     resbuf1  = table_outtable(restab1);
     if (strcmp(result_sing, resbuf1))
          elog_die(FATAL, "[11e] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, result_sing);
     nfree(buf1);
     nfree(resbuf1);
     table_destroy(tab1);
     table_destroy(restab1);

     /* [12] add tab_singinfokey to table and sample */
     tab1 = table_create();
     buf1 = xnstrdup(tab_singinfokey);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     r = route_twrite(samprt, tab1);
     if ( r < 0 )
	  elog_die(FATAL, "[12a] add table failed");
     r = cascade_sample(cas, resrt, err);
     if ( !r )
          elog_die(FATAL, "[12b] cascade sample failed, mode %s", mode_label);
     r = route_tell(resrt, &resseq, &resoff, &modt);
     if ( !r )
          elog_die(FATAL, "[12c] can't route_tell(), mode %s", mode_label);
     restab1 = route_seektread(resrt, resseq, 0);
     if ( ! restab1)
	  elog_die(FATAL, "[12d] can't read result ring: "
		   "mode %s, single sample, with info and key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1     = table_outtable(tab1);
     resbuf1  = table_outtable(restab1);
     if (strcmp(result_singkey, resbuf1))
          elog_die(FATAL, "[12e] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, result_sing);
     nfree(buf1);
     nfree(resbuf1);
     table_destroy(tab1);
     table_destroy(restab1);


     /* [13] add tab_mult to table and sample */
     tab1 = table_create();
     buf1 = xnstrdup(tab_mult);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     wanttab1 = table_create();
     wantbuf1 = xnstrdup(result_mult);
     table_scan(wanttab1, wantbuf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(wanttab1, wantbuf1);
     table_rminfo(wanttab1, "info");	/* remove info line */
     r = route_twrite(samprt, tab1);
     if ( r < 0 )
	  elog_die(FATAL, "[13a] add table failed");
     r = cascade_sample(cas, resrt, err);
     if ( !r )
          elog_die(FATAL, "[13b] cascade sample failed, mode %s", mode_label);
     r = route_tell(resrt, &resseq, &resoff, &modt);
     if ( !r )
          elog_die(FATAL, "[13c] can't route_tell(), mode %s", mode_label);
     restab1 = route_seektread(resrt, resseq, 0);
     if ( ! restab1)
	  elog_die(FATAL, "[13d] can't read result ring: "
		   "mode %s, multi sample, no info, no key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1     = table_outtable(tab1);
     resbuf1  = table_outtable(restab1);
     wantbuf1 = table_outtable(wanttab1);
     if (strcmp(wantbuf1, resbuf1))
          elog_die(FATAL, "[13e] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, wantbuf1);
     nfree(buf1);
     nfree(resbuf1);
     nfree(wantbuf1);
     table_destroy(tab1);
     table_destroy(restab1);
     table_destroy(wanttab1);

     /* [14] add tab_multinfo to table and sample */
     tab1 = table_create();
     buf1 = xnstrdup(tab_multinfo);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     r = route_twrite(samprt, tab1);
     if ( r < 0 )
	  elog_die(FATAL, "[14a] add table failed");
     r = cascade_sample(cas, resrt, err);
     if ( !r )
          elog_die(FATAL, "[14b] cascade sample failed, mode %s", mode_label);
     r = route_tell(resrt, &resseq, &resoff, &modt);
     if ( !r )
          elog_die(FATAL, "[14c] can't route_tell(), mode %s", mode_label);
     restab1 = route_seektread(resrt, resseq, 0);
     if ( ! restab1)
	  elog_die(FATAL, "[14d] can't read result ring: "
		   "mode %s, multi sample, with info, no key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     buf1     = table_outtable(tab1);
     resbuf1  = table_outtable(restab1);
     if (strcmp(result_mult, resbuf1))
          elog_die(FATAL, "[14e] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, result_mult);
     nfree(buf1);
     nfree(resbuf1);
     table_destroy(tab1);
     table_destroy(restab1);

     /* [15] add tab_multinfokey to table and sample */
     tab1 = table_create();
     buf1 = xnstrdup(tab_multinfokey);
     table_scan(tab1, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab1, buf1);
     r = route_twrite(samprt, tab1);
     if ( r < 0 )
	  elog_die(FATAL, "[15a] add table failed");
     r = cascade_sample(cas, resrt, err);
     if ( !r )
          elog_die(FATAL, "[15b] cascade sample failed, mode %s", mode_label);
     r = route_tell(resrt, &resseq, &resoff, &modt);
     if ( !r )
          elog_die(FATAL, "[15c] can't route_tell(), mode %s", mode_label);
     restab1 = route_seektread(resrt, resseq, 0);
     if ( ! restab1)
	  elog_die(FATAL, "[15d] can't read result ring: "
		   "mode %s, multi sample, with info and key", mode_label);
     table_rmcol(restab1, "_dur");
     table_rmcol(restab1, "_seq");
     table_replaceinfocell(restab1, "type", "_time", "i32");
     table_replaceinfocell(restab1, "key", "_time", "-");
     buf1     = table_outtable(tab1);
     resbuf1  = table_outtable(restab1);
     if (strcmp(result_multkey, resbuf1))
          elog_die(FATAL, "[15e] aggregation failed, mode %s:-\n"
		   "--- in ---\n%s\n--- out ---\n%s\n--- want ---\n%s", 
		   mode_label, buf1, resbuf1, result_multkey);
     nfree(buf1);
     nfree(resbuf1);
     table_destroy(tab1);
     table_destroy(restab1);

     /* shutdown */
     cascade_fini(cas);
     route_close(resrt);
     route_close(samprt);
}

#endif
