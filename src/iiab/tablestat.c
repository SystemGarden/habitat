/*
 * Table statistics class
 * Nigel Stuckey, June 2004
 *
 * Copyright System Garden Ltd 1999-2004, all rights reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tablestat.h"
#include "elog.h"
#include "util.h"

/* 
 * Cascade samples sequences of data from sequence aware routes (such as 
 * ringstore (rs:) and SQL ringstore (sqlrs:)) to produce computed 
 * summaries of the fields.
 *
 * At the start of the run, the sampled route is opened at the begining
 * or after the latest sequence (if previously stored) and the route is 
 * held open at this point for subsequent runs.
 *
 * Each time the sample action is called it will catch up with all the 
 * intervening entries and write a summary to the output route. 
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
 *    CASCADE_AVG  - Calculate average of the sample run (or ave)
 *    CASCADE_MIN  - Find minimum number in the sample run
 *    CASCADE_MAX  - Find maximum number in the sample run
 *    CASCADE_SUM  - Calculate the sum of the corresponding figures
 *    CASCADE_LAST - Echo the last set of figures (same result as snap method)
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
 *   rate | add together the figures from the set defined by key+column,
 *        | then divide by the number of seconds between the first and
 *        | samples plus one of the sample durations (pick the first)
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
 * Initialise a cascade monitor route for sampling.
 * Returns the type CASCADE if successful of NULL otherwise.
 */
CASCADE *cascade_init(enum cascade_fn func,	/* cascading function */
		      char *monroute		/* p-url route to monitor */ )
{
     struct cascade_info *sampent;
     ROUTE input;

     /* check that we have a recognisable tablestore in the command */
     input = route_parse(monroute);
     if ( ! input ) {
	  elog_printf(ERROR, "unable to parse route format %s", 
		      monroute);
	  return NULL;
     }
     if (input->method != TABLESTORE) {
	  route_freeROUTE(input);
	  elog_printf(ERROR, "route %s is not tablestore, it should "
		      "begin with `tab:'", monroute);
	  return NULL;
     }

     /* save all the details in sample_tab */
     sampent = nmalloc( sizeof(struct cascade_info) );
     sampent->fn = func;
     sampent->monrt = input;
     sampent->monitor = NULL;

     elog_printf(DEBUG, "cascade type %d init on %s", func, monroute);

     return sampent;
}

/*
 * Sample the tablestore ring set up by cascade_init and described in CASCADE.
 * See the description above for how the thing works.
 * The computed table is sent to the output route and errors are sent to 
 * the error route. Returns 0 for success or 1 for failure.
 */
int cascade_sample(CASCADE *sampent,	/* cascade reference */
		   ROUTE output,	/* output route */
		   ROUTE error		/* error route */ )
{
     TAB_RING ring;
     ITREE *d;
     TREE *baserow, *samplerow, *keyinfo;
     TABLE basetab, sampletab=NULL;
     ntsbuf *nts;
     int ntabs, isabs, r, nsamples=0;
     double basevalue, samplevalue;
     char *currentspan, *absstr, *typestr, *keycol, *keyvalue;
     time_t base_t=0, sample_t=0;

     /* check tablestore is open */
     if ( ! sampent->monitor ) {
	  ring = tab_open(sampent->monrt->name.tab.storename, 
			  sampent->monrt->name.tab.ringname, NULL);
	  if ( ! ring )
	       return 0;	/* monitored ring is not yet there so
				 * return successfully and try again on
				 * the next event */
	  /* position beyond youngest and save the ring reference */
	  tab_jumpyoungest(ring);
	  sampent->monitor = ring;
     }

     /* fetch new data since the last sample (without jumping, this is the
      * default behaviour of tablestore) */
     ntabs = tab_mgetraw(sampent->monitor, METH_BUILTIN_SAMPLE_NTABS, &d);
     if (ntabs < 0)
	  return ntabs;		/* return the error given to us */
     if (ntabs == 0)
	  return 0;		/* nothing to read */

     /* iterate over tables from mget */
     currentspan = NULL;
     basetab = NULL;
     keycol = NULL;
     itree_traverse(d) {
	  /* get next mget entry */
	  nts = itree_get(d);

	  if ( currentspan != nts->spantext ) {
	       /* we have just started or span has changed */

	       if (basetab) {
		    /* process completed span and emit before starting 
		     * a new one */
		    cascade_finalsample(sampent, output, error, 
					basetab, sampletab, nsamples, keycol,
					base_t, sample_t);
		    table_destroy(basetab);
		    basetab = NULL;
	       }

	       /* clear sample table of previous run */
	       if (sampletab) {
		    table_destroy(sampletab);
		    sampletab = NULL;
	       }

	       /* start new base table */
	       currentspan = nts->spantext;
	       basetab = table_create_s(currentspan);
	       table_scan(basetab, nts->buffer, "\t", TABLE_SINGLESEP, 
			  TABLE_NOCOLNAMES, TABLE_NORULER);
	       base_t = nts->instime;		/* first sample time */
	       nsamples = 1;
	  } else {
	       /* continuation of span */

	       /* clear sample table of previous run */
	       if (sampletab) {
		    table_destroy(sampletab);
		    sampletab = NULL;
	       }

	       /* set headers, info from the base table and scan sample 
		* into table. this does not contain any info lines as it
		* was created with table_create_t not _s */
	       sampletab = table_create_t( table_getcolorder(basetab) );
	       table_scan(sampletab, nts->buffer, "\t", TABLE_SINGLESEP, 
			  TABLE_NOCOLNAMES, TABLE_NORULER);
	       sample_t = nts->instime;
	       nsamples++;

	       /* find the key in the base table, if applicable */
	       keyinfo = table_getinforow(basetab, CASCADE_INFOKEYROW);
	       if (keyinfo) {
		    keycol = tree_search(keyinfo, "1", 2);  /* include NULL */
		    tree_destroy(keyinfo);
	       } else
		    keycol = NULL;

	       /* iterate over the rows of the sample table */
	       table_traverse(sampletab) {

		    /* get current row from sample */
		    samplerow = table_getcurrentrow(sampletab);

		    /* are we dealing with keys? */
		    if (keycol) {
			 /* extract key value from sample and search for the 
			  * corresponding row in the base table */
			 keyvalue = tree_find(samplerow, keycol);
			 if (keyvalue) {
			      r = table_search(basetab, keycol, keyvalue);
			 } else {
			      /* no key in sample, assume it is a new row */
			      r = -1;
			 }

			 /* get row from base table; if not there we assume
			  * that it is a new instance that popped-up and
			  * a new row should be created in the base table */
			 if (r == -1) {
			      table_addrow_alloc(basetab, samplerow);
			      tree_destroy(samplerow);
			      continue;		/* next sample row */
			 }
		    } else {
			 /* no keys - just get the first value from the base 
			  * table; it will not be called again */
			 table_first(basetab);
		    }
		    baserow = table_getcurrentrow(basetab);

		    /* cycle over base & process each column/cell within 
		     * the line */
		    tree_traverse(baserow) {
			 /* numeric or string? if string, leave it as the first
			  * sample text has already been captured and that is 
			  * all we can do */
			 typestr = table_getinfocell(basetab, "type", 
						    tree_getkey(baserow));
			 if (typestr && strcmp(typestr, "str") == 0) {
			      continue;
			 }

			 /* find the sense (abs or cnt) of each column */
			 absstr = table_getinfocell(basetab, "sense", 
						    tree_getkey(baserow));
			 if (absstr && strcmp("abs", absstr) == 0)
			      isabs = 1;	/* abs */
			 else
			      isabs = 0;	/* cnt */

			 /* now find the values */
			 basevalue = atof( tree_get(baserow) );
			 samplevalue = atof( tree_find(samplerow, 
						       tree_getkey(baserow)) );
			 /*fprintf(stderr, 
				  "col=%s  basevalue=%f  samplevalue=%f\n", 
				  tree_getkey(baserow), basevalue, 
				  samplevalue);*/

			 switch (sampent->fn) {
			 case CASCADE_AVG:
			      if ( ! isabs )
				   continue;	/* wait for last table */

			      /* add values for later averaging */
			      table_replacecurrentcell_alloc(
				   basetab, 
				   tree_getkey(baserow),
				   util_ftoa(basevalue + samplevalue) );
			      break;
			 case CASCADE_MIN:
			      if ( ! isabs )
				   continue;	/* to be done */

			      /* find min value */
			      if (util_min(basevalue, samplevalue) < basevalue)
			      table_replacecurrentcell_alloc(
				   basetab, 
				   tree_getkey(baserow),
				   util_ftoa(samplevalue));
			      break;
			 case CASCADE_MAX:
			      if ( ! isabs )
				   continue;	/* to be done */

			      /* find max value */
			      if (util_max(basevalue, samplevalue) > basevalue)
				   table_replacecurrentcell_alloc(
					basetab, 
					tree_getkey(baserow),
					util_ftoa(samplevalue));
			      break;
			 case CASCADE_SUM:
			      if ( ! isabs )
				   continue;	/* wait for last table */

			      /* add values */
			      table_replacecurrentcell_alloc(
				   basetab, 
				   tree_getkey(baserow), 
				   util_ftoa(basevalue + samplevalue) );
			      break;
			 case CASCADE_LAST:
			      /* abs and cnt: wait for last sample */
			      break;
			 case CASCADE_RATE:
			      if ( ! isabs )
				   continue;	/* wait for last table */

			      /* add values for later averaging */
			      table_replacecurrentcell_alloc(
				   basetab, 
				   tree_getkey(baserow),
				   util_ftoa(basevalue + samplevalue) );
			      break;
			 }
		    }

		    /* free working trees */
		    tree_destroy(samplerow);
		    tree_destroy(baserow);
	       }
	  }
     }

     /* final sample if inside a span */
     if (basetab && sampletab) {
	  /* last table in span */
	  cascade_finalsample(sampent, output, error, 
			      basetab, sampletab, nsamples, keycol, 
			      base_t, sample_t);
     }

     /* free working tables */
     if (basetab)
	  table_destroy(basetab);
     if (sampletab)
	  table_destroy(sampletab);
     tab_mgetrawfree(d);

     return 0;
}


/* 
 * Carry out the final sample on behalf for cascade_sample(). This may be
 * called at the end of a span or the end of the ring to be sampled.
 * Basetab should include the final sample (for abs values), sampletab is 
 * there to calculate cnt values.
 * Keycol is name of the key's column or NULL if it doesn't exist.
 */
void cascade_finalsample(CASCADE *sampent,	/* cascade reference */
			 ROUTE output,		/* output route */
			 ROUTE error,		/* error route */ 
			 TABLE basetab,		/* populated base table */
			 TABLE sampletab,	/* populated final sample */
			 int nsamples,		/* num of samples in basetab */
			 char *keycol,		/* key column name (or NULL) */
			 time_t base_t,		/* time of base sample */
			 time_t sample_t	/* time of final sample */)
{
     TREE *baserow, *samplerow;
     int isabs, r;
     double basevalue, samplevalue;
     char *outbuf, *absstr, *typestr, *keyvalue;

     /* iterate over the rows of the sample table */
     table_traverse(sampletab) {

	  /* get current row from sample */
	  samplerow = table_getcurrentrow(sampletab);

	  /* are we dealing with keys? */
	  if (keycol) {
	       /* extract key value from sample and search for the 
		* corresponding row in the base table */
	       keyvalue = tree_find(samplerow, keycol);
	       if (keyvalue)
		    r = table_search(basetab, keycol, keyvalue);
	       else
		    /* no key in sample, assume it is a new row */
		    r = -1;

	       /* get row from base table; if not there we assume
		* that it is a new instance that popped-up and
		* a new row should be created in the base table */
	       if (r == -1) {
		    table_addrow_alloc(basetab, samplerow);
		    tree_destroy(samplerow);
		    continue;		/* next sample row */
	       }
	  } else {
	       /* no keys - just get the first value from the base 
		* table; it will not be called again */
	       table_first(basetab);
	  }

	  baserow = table_getcurrentrow(basetab);

	  /* find the value and sense of each column */
	  itree_traverse(baserow) {
	       /* numeric or string? if string, leave it alone */
	       typestr = table_getinfocell(basetab, "type", 
					   tree_getkey(baserow));
	       if (typestr && strcmp(typestr, "str") == 0)
		    continue;

	       /* find the data sense from the base table */
	       absstr = table_getinfocell(basetab, "sense", 
					  tree_getkey(baserow));
	       if (absstr && strcmp("abs", absstr) == 0)
		    isabs = 1;	/* abs */
	       else
		    isabs = 0;	/* cnt */

	       /* load the rows into integers */
	       basevalue = atof( tree_get(baserow) );
	       samplevalue = atof( tree_find(samplerow, 
					     tree_getkey(baserow)) );

	       switch (sampent->fn) {
	       case CASCADE_AVG:
		    if (isabs) {
			 /* average accumulated samples */
			 basevalue = basevalue / nsamples;
		    } else {
			 /* mean the diff between first & last, producing an 
			  * absolute value */
			 basevalue = basevalue + 
			      (samplevalue-basevalue) / nsamples;
		    }
		    /* store */
		    table_replacecurrentcell_alloc( basetab, 
						    tree_getkey(baserow),
						    util_ftoa(basevalue) );
		    elog_printf(DEBUG, "cascade AVG col %s basevalue %f", 
				tree_getkey(baserow), basevalue);
		    break;
	       case CASCADE_MIN:
		    /* cnt version not yet implemented */
		    table_replaceinfocell(basetab, "sense", 
					  tree_getkey(baserow), "abs");
		    break;
	       case CASCADE_MAX:
		    /* cnt version not yet implemented */
		    table_replaceinfocell(basetab, "sense", 
					  tree_getkey(baserow), "abs");
		    break;
	       case CASCADE_SUM:
		    if (isabs)
			 continue;	/* abs already done */

		    /* take diff between first & last */
		    basevalue = samplevalue - basevalue;
		    table_replacecurrentcell_alloc( basetab, 
						    tree_getkey(baserow),
						    util_ftoa(basevalue) );
		    table_replaceinfocell(basetab, "sense", 
					  tree_getkey(baserow), "abs");
		    elog_printf(DEBUG, "cascade SUM col %s basevalue %d", 
				tree_getkey(baserow), basevalue);
		    break;
	       case CASCADE_LAST:
		    table_replacecurrentcell_alloc(basetab, 
						   tree_getkey(baserow),
						   util_ftoa(samplevalue) );
		    break;
	       case CASCADE_RATE:
		    if (isabs) {
			 /* average accumulated samples. 
			  * If they are absolute, then we assume per 
			  * second values anyway, so we only need to 
			  * average over the number of samples */
			 basevalue = basevalue / nsamples;
		    } else {
			 /* generate an abs rate between first & last.
			  * If the base value is <0, then assume that the
			  * counter has cycled or reset itself and the best
			  * we can do is offer current samplevalue.
			  * If there is no difference in the sample time, 
			  * store the value difference. This is to understate 
			  * the true answer... we need fractional times 
			  * for that!!
			  */
			 if (samplevalue - basevalue < 0)
			      basevalue = samplevalue;
			 else
			      basevalue = samplevalue - basevalue;
			 if (basevalue > 0 && sample_t != base_t)
			      basevalue = basevalue / (sample_t - base_t);

			 table_replaceinfocell(basetab, "sense", 
					       tree_getkey(baserow), "abs");
		    }
		    /* store */
		    table_replacecurrentcell_alloc( basetab, 
						    tree_getkey(baserow),
						    util_ftoa(basevalue) );
		    elog_printf(DEBUG, "cascade RATE col %s basevalue %f", 
				tree_getkey(baserow), basevalue);
		    break;
	       }
	  }

	  tree_destroy(samplerow);
	  tree_destroy(baserow);
     }

     /* write out and then free current base table */
     outbuf = table_outtable(basetab);
     route_raw(output, outbuf, strlen(outbuf));
     nfree(outbuf);
}






/*
 * End the monitoring of the tablestore and free its referencies
 * You will not be able to use the CASCADE reference after this call.
 */
void cascade_fini(CASCADE *sampent)
{
     if (sampent->monitor)
	  tab_close(sampent->monitor);
     route_freeROUTE(sampent->monrt);
     nfree(sampent);
}


#if TEST

#include <unistd.h>

#define TS_SAMPFILE "t.cascade.dat"
#define TS_SAMPRING "wrongring"
#define TS_SAMPPURL "ts:" TS_SAMPFILE "," TS_SAMPRING
#define TAB_SAMPFILE "t.cascade.dat"
#define TAB_SAMPRING "rightring"
#define TAB_SAMPPURL "tab:" TAB_SAMPFILE "," TAB_SAMPRING
#define TAB_RESFILE "t.cascade.dat"
#define TAB_RESRING "results"
#define TAB_RESPURL "tab:" TAB_RESFILE "," TAB_RESRING
#define TAB_HEAD1 "col1\tcol2\tcol3\nabs\tcnt\tnop\tsense\n--\n"
#define TAB_BODY1 "1.00\t2.00\t3.00"
#define TAB_TABLE1 TAB_HEAD1 TAB_BODY1 "\n"
#define TAB_HEAD2 "col1\tcol2\tcol3\nabs\tabs\tabs\tsense\n--\n"
#define TAB_BODY2 "1.00\t0.00\t0.00"
#define TAB_TABLE2 TAB_HEAD2 TAB_BODY2 "\n"
#define TAB_HEAD3 "col1\tcol2\tcol3\nabs\tabs\tabs\tsense\n1\t-\t-\tkey\n--\n"
#define TAB_BODY3 "1.00\t10.00\t20.00\n2.00\t30.00\t40.00"
#define TAB_TABLE3 TAB_HEAD3 TAB_BODY3 "\n"
#define TAB_HEAD4 "col1\tcol2\tcol3\nabs\tabs\tabs\tsense\n1\t-\t-\tkey\n--\n"
#define TAB_BODY4 "1.00\t11.00\t22.00\n2.00\t31.00\t42.00"
#define TAB_TABLE4 TAB_HEAD4 TAB_BODY4 "\n"

ROUTE err, out;

void test_cascade(enum cascade_fn mode, char *stage1, char *stage2);

int main(int argc1, char *argv[])
{
     route_init("stderr", 0, NULL);
     err = route_open("stderr", NULL, NULL, 0);
     out = route_open("stdout", NULL, NULL, 0);
     elog_init(err, 0, "cascade test", NULL);
     tab_init();

     /* run cascade with all the possible modes */
     test_cascade(CASCADE_AVG, TAB_TABLE1, TAB_TABLE1);
     test_cascade(CASCADE_RATE, TAB_TABLE2, TAB_TABLE2);
#if 0
     test_cascade(CASCADE_MIN);
     test_cascade(CASCADE_MAX);
     test_cascade(CASCADE_SUM);
     test_cascade(CASCADE_LAST);
#endif

     tab_fini();
     elog_fini();
     route_close(err);
     route_close(out);
     route_fini();
     printf("tests finished successfully\n");
     exit(0);
}


/* run though a series of tests for the current cascade configuration */
void test_cascade(enum cascade_fn mode, char *stage1, char *stage2)
{
     int r, i, resseq;
     TS_RING samplets;
     TAB_RING sampletabs;
     TAB_RING restab;
     CASCADE *cas;
     char *resbuf;
     time_t restime;
     ROUTE resrt;
     TABLE resintab;

     /* unlink previous storage */
     unlink(TS_SAMPFILE);
     unlink(TAB_SAMPFILE);
     unlink(TAB_RESFILE);

     /* create results output as a route */
     restab = tab_create(TAB_RESFILE, 0644, TAB_RESRING, 
			 "Output of testing results", NULL, 20);
     if ( ! restab )
	  elog_die(FATAL, "[0] Can't create tablestore ring");
     resrt = route_open(TAB_RESPURL, NULL, NULL, 0);
     if ( ! resrt )
	  elog_die(FATAL, "[0] Can't open result route");

     /* [1a] create a timestore ring */
     samplets = ts_create(TS_SAMPFILE, 0644, TS_SAMPRING, 
			  "Wrong sort of ring for testing ", NULL, 300);
     if ( ! samplets )
	  elog_die(FATAL, "[1a] can't create timestore ring");

     /* [1b] run cascade on the timestore */
     elog_send(ERROR, "[1b] expect an error below");
     cas = cascade_init(mode, TS_SAMPPURL);
     if (cas)
	  elog_die(FATAL, 
		   "[1b] shouldn't be able to start cascade on a timestore");
     ts_close(samplets);

     /* [2a] create test ring */
     sampletabs = tab_create(TAB_SAMPFILE, 0644, TAB_SAMPRING, 
			     "Test tablestore", NULL, 300);
     if ( ! sampletabs )
	  elog_die(FATAL, "[2a] can't create tablestore ring");

     /* [2b] run cascade on an empty ring */
     cas = cascade_init(mode, TAB_SAMPPURL);
     if ( ! cas)
	  elog_die(FATAL, "[2b] can't start cascade");

     /* [2c] sample when there is no change */
     r = cascade_sample(cas, out, err);
     if ( r )
	  elog_die(FATAL, "[2c] cascade sample failed");

     /* [2d] again sample when there is no change */
     r = cascade_sample(cas, out, err);
     if ( r )
	  elog_die(FATAL, "[2d] cascade sample failed");

     /* shutdown */
     cascade_fini(cas);
     tab_close(sampletabs);

     /* [3a] run cascade on a ring with a single table already there */
     sampletabs = tab_open(TAB_SAMPFILE, TAB_SAMPRING, NULL);
     if ( ! sampletabs )
	  elog_die(FATAL, "[3a] can't create tablestore ring");
     cas = cascade_init(mode, TAB_SAMPPURL);
     if ( ! cas)
	  elog_die(FATAL, "[3a] can't start cascade");

     /* [3b] sample when there is no change */
     r = cascade_sample(cas, out, err);
     if ( r )
	  elog_die(FATAL, "[3b] cascade sample failed");

     /* [3c] add one table and sample */
     r = tab_puttext(sampletabs, TAB_TABLE1);
     if ( r < 0 )
	  elog_die(FATAL, "[3c] add table failed");
     r = cascade_sample(cas, out, err);
     if ( r )
	  elog_die(FATAL, "[3c] cascade sample failed");

     /* shutdown */
     cascade_fini(cas);
     tab_close(sampletabs);

     /* [4a] run cascade on a ring with two tables already there */
     sampletabs = tab_open(TAB_SAMPFILE, TAB_SAMPRING, NULL);
     if ( ! sampletabs )
	  elog_die(FATAL, "[4a] can't create tablestore ring");
     cas = cascade_init(mode, TAB_SAMPPURL);
     if ( ! cas)
	  elog_die(FATAL, "[4a] can't start cascade");

     /* [4b] sample when there is no change */
     r = cascade_sample(cas, out, err);
     if ( r )
	  elog_die(FATAL, "[4b] cascade sample failed");

     /* [4c] add one table and sample */
     r = tab_puttext(sampletabs, TAB_TABLE1);
     if ( r < 0 )
	  elog_die(FATAL, "[4c] add table failed");
     r = cascade_sample(cas, out, err);
     if ( r )
	  elog_die(FATAL, "[4c] cascade sample failed");

     /* [4d] add another table and sample */
     r = tab_puttext(sampletabs, TAB_TABLE1);
     if ( r < 0 )
	  elog_die(FATAL, "[4d] add table failed");
     r = cascade_sample(cas, out, err);
     if ( r )
	  elog_die(FATAL, "[4d] cascade sample failed");

     /* [4e] add two tables and sample */
     r = tab_puttext(sampletabs, TAB_TABLE1);
     if ( r < 0 )
	  elog_die(FATAL, "[4e1] add table failed");
     r = tab_puttext(sampletabs, TAB_TABLE1);
     if ( r < 0 )
	  elog_die(FATAL, "[4e2] add table failed");
     r = cascade_sample(cas, resrt, err);
     if ( r )
	  elog_die(FATAL, "[4e] cascade sample failed");
     route_flush(resrt);
     resintab = tab_get(restab, &restime, &resseq);
     if ( ! resintab )
	  elog_die(FATAL, "[4e] unable to get result tab");
     resbuf = table_outtable(resintab);
     if (strcmp(resbuf, stage1))
	  elog_die(FATAL, "[4e] calculation failed:-\n"
		   "result=%s\nwanted=%s", resbuf, stage1);
     nfree(resbuf);
     table_destroy(resintab);

     /* shutdown */
     cascade_fini(cas);
     tab_close(sampletabs);

     /* [5a] run cascade on a ring with five tables already there */
     sampletabs = tab_open(TAB_SAMPFILE, TAB_SAMPRING, NULL);
     if ( ! sampletabs )
	  elog_die(FATAL, "[5a] can't create tablestore ring");
     cas = cascade_init(mode, TAB_SAMPPURL);
     if ( ! cas)
	  elog_die(FATAL, "[5a] can't start cascade");
     r = cascade_sample(cas, out, err);
     if ( r )
	  elog_die(FATAL, "[5a] cascade sample failed");

     /* [5b] add ten tables and sample */
     for (i=0; i<10; i++) {
	  r = tab_puttext(sampletabs, TAB_TABLE1);
	  if ( r < 0 )
	       elog_die(FATAL, "[5b%d] add table failed", i);
     }
     r = cascade_sample(cas, resrt, err);
     if ( r )
	  elog_die(FATAL, "[5b] cascade sample failed");
     route_flush(resrt);
     resintab = tab_get(restab, &restime, &resseq);
     if ( ! resintab )
	  elog_die(FATAL, "[5b] unable to get result tab");
     resbuf = table_outtable(resintab);
     if (strcmp(resbuf, stage2))
	  elog_die(FATAL, "[5b] calculation failed");
     nfree(resbuf);
     table_destroy(resintab);

     /* sample when there is no change */
     r = cascade_sample(cas, out, err);
     if ( r )
	  elog_die(FATAL, "t.cascade", 0, "[5b] cascade sample failed");

     /* add ten tables and sample */
     /* add another ten tables and sample */
     /* add 100 tables and sample */

     /* simple floating point */

     /* shutdown */
     cascade_fini(cas);
     tab_close(sampletabs);

     /* close off result ring */
     /* delete test files */
     tab_close(restab);
     route_close(resrt);
}

#endif
