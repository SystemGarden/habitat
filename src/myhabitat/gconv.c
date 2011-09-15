/*
 * Graph conversion functions
 *
 * Nigel Stuckey, September 1999, refactored October 2010
 * Copyright System Garden Limited 1999-2001, 2010. All rights reserved.
 */

#define _ISOC99_SOURCE

#include "gconv.h"
#include "../iiab/util.h"
#include "../iiab/table.h"
#include "../iiab/tableset.h"
#include <float.h>
#include <string.h>
#include <stdlib.h>

#if 0
/*
 * Convert the RESDAT structure into nmalloc()'ed float arrays ready for
 * plotting with the draw command. Returns the number of samples in the
 * arrays and sets the pointers addressed xvals and yvals to the x and y 
 * values as output. If there are no usable samples, 0 is returned.
 * If keycol and keyval are NULL, the data is assumed to have no key;
 * if they are set, however, then keyed data is extracted from the
 * table(s) within RESDAT
 * Count data is transformed into absolute values (difference over time) 
 * for plotting, where as absooute data is left alone. In this case, 
 * the first value is lost as it is used as a base.
 * Count and absolute types of data are 'rebased' depending on the 
 * g->start value (the only reason we need a UIGRAPH structure passed).
 * The main reason for this is to cope with loss of accuracy of a float 
 * at large values (time_t in the year 2000).
 * Absolute data is left alone but count data is processed for differencies
 * over time. The first sample of a ring is lost as it provides the base for
 * calculations on subsequent data.
 * Time should be identified using the column name _time. If it does not 
 * exist or there are values missing, then an attempt is made to create
 * a mock time based on one second intervals from the epoch.
 */
int gconv_resdat2arrays(UIGRAPH *g,	  /* graph structure */
			RESDAT rdat,	  /* result structure */
			char *colname, /* column containing data */
			char *keycol,  /* column containing key */
			char *keyval,  /* value of key */
			float **xvals, /* output array of xvalues */
			float **yvals  /* output array of yvalues */ )
{
     TABLE tmptab;
     ITREE *vallst, *timlst, *idx, *collst, *dlst;
     char *sense;
     int iscnt=0, isfirst, nvals=0, i, j=0, clashnum=0;
     time_t newtim=0, lasttim=0, clashtim=0, mocktim=0;
     float *vals, lastval=0.0, newval=0.0, *clashval=NULL, clashsum=0.0;

     if (rdat.t == TRES_NONE)
	  return 0;	/* no data */

     /* convert a single table into a list so that we can process uniformly */
     dlst = itree_create();
     if (rdat.t == TRES_TABLE) {
	  itree_append(dlst, rdat.d.tab);
     } else {
	  itree_traverse(rdat.d.tablst) {
	       itree_append(dlst, itree_get(rdat.d.tablst));
	  }
     }

#if 0
     /* dump of tables before converting them */
     char *prt;		/* debug variable */
     itree_traverse(dlst) {
	  prt = table_print((TABLE) itree_get(dlst));
	  elog_printf(DEBUG, "table %d = %s", 
		      itree_getkey(dlst), prt);
	  nfree(prt);
     }
#endif

     /* if keys are set, extract data and replace in list */
     if (keycol && keyval) {
	  collst = itree_create();
	  itree_append(collst, "_time");
	  itree_append(collst, colname);
	  itree_traverse(dlst) {
	       tmptab = table_selectcolswithkey(itree_get(dlst),
						keycol, keyval, collst);
	       itree_put(dlst, tmptab);		/* replace */
	  }
	  itree_destroy(collst);
     }

     /* count samples and allocate space */
     itree_traverse(dlst)
	  if (itree_get(dlst))
	       nvals += table_nrows( itree_get(dlst) );
     if ( ! nvals )
          return 0;	/* no valid data to plot (it probably shrank) */
     vals   = xnmalloc(nvals * sizeof(gfloat));
     *xvals = xnmalloc(nvals * sizeof(gfloat));
     *yvals = xnmalloc(nvals * sizeof(gfloat));
     /*elog_printf(DEBUG, "converting %d values", nvals);*/

     /* extract values from RESDAT into a working array containg values,
      * indexed by a tree keyed by time */
     idx = itree_create();
     i = 0;
     itree_traverse(dlst) {
	  if ( ! itree_get(dlst))
	       continue;
	  /* is this count data? */
	  sense = table_getinfocell(itree_get(dlst),"sense",colname);
	  if (sense && strcmp(sense, "cnt") == 0)
	       iscnt = 1;
	  else
	       iscnt = 0;
	  /*elog_printf(DEBUG, "column %s is %s", colname, 
	    iscnt?"count":"absolute");*/

	  /* convert values into float and and load into idx list */
	  vallst = table_getcol(itree_get(dlst), colname);
	  if ( ! vallst )
	       continue;	/* curve not in this table */
	  timlst = table_getcol(itree_get(dlst), "_time");
	  isfirst  = 1;
	  if (iscnt) {
	       /* carry out a cnt to abs calculation and insert the
		* abs value into index list */
	       if (timlst)
		    itree_first(timlst);
	       itree_traverse(vallst) {
		    if (isfirst) {
			 if ( itree_get(vallst) )
			      lastval = atof(itree_get(vallst));
			 else
			      lastval = 0.0;
			 if ( timlst && itree_get(timlst) )
			      lasttim = strtol(itree_get(timlst), 
					       (char**)NULL, 10);
			 else
			      lasttim = mocktim;
			 isfirst = 0;
		    } else {
			 if ( itree_get(vallst) )
			      newval = atof(itree_get(vallst));
			 else
			      newval = 0.0;
			 if ( timlst && itree_get(timlst) )
			      newtim = strtol(itree_get(timlst),
					      (char**)NULL, 10);
			 else
			      newtim = mocktim;
			 if (newtim - lasttim < 0)
			      vals[i] = (newval - lastval);
			 else
			      vals[i] = (newval - lastval) / 
				        (newtim - lasttim); 
			 lastval = newval;
			 lasttim = newtim;
			 itree_add(idx, newtim, &vals[i]);
			 i++;
		    }
		    if (timlst)
			 itree_next(timlst);
		    else
			 mocktim++;
	       }
	  } else {
	       /* just convert types and insert into index list */
	       if (timlst)
		    itree_first(timlst);
	       itree_traverse(vallst) {
		    if ( itree_get(vallst) )
			 vals[i] = atof(itree_get(vallst));
		    else
			 vals[i] = 0.0;
		    if (timlst && itree_get(timlst))
		         itree_add(idx, strtol(itree_get(timlst),
					       (char**)NULL, 10), &vals[i]);
		    else
			 itree_add(idx, mocktim, &vals[i]);
		    if (timlst)
			 itree_next(timlst);
		    else 
			 mocktim++;
		    i++;
	       }
	  }
	  if (timlst)
	       itree_destroy(timlst);
	  itree_destroy(vallst);
     }

     /*elog_printf(DEBUG, "extracted %d values", itree_n(idx));*/

     /* traverse list looking for key clashes and average them.
      * These are multiple values for the same time point */
     clashtim = -1;
     itree_traverse(idx) {
	  if (itree_getkey(idx) == clashtim) {
	       /* key clash found: average out the values */
	       clashsum += *((float*)itree_get(idx));
	       clashnum++;
	       *clashval = clashsum / clashnum;
	       itree_rm(idx);
	  } else {
	       clashtim = itree_getkey(idx);
	       clashval = itree_get(idx);
	       clashsum = *((float*)itree_get(idx));
	       clashnum = 1;
	  }
     }

     /* copy out values and convert times into final float arrays */
     i = 0;
     itree_traverse(idx) {
	  (*xvals)[i] = (int) (itree_getkey(idx) - g->start);
	  (*yvals)[i] = *( (float *) itree_get(idx));
	  i++;
     }

#if 0
     /* debug dump of the data converted to floats */
     elog_startprintf(DEBUG, "values to graph: g->start=%d -> ", g->start);
     j = 0;
     itree_traverse(idx) {
	  elog_contprintf(DEBUG, "j=%d idx=%d val=%.2f "
			         "xvals[%d]=%.2f yvals[%d]=%.2f, ", 
			  j, itree_getkey(idx), *( (float *) itree_get(idx)),
			  j, (*xvals)[j], j, (*yvals)[j]);
	  j++;
     }
     elog_endprintf(DEBUG, "%d values", i);
#endif

     /* free working storage and return the number of pairs there are */
     if (keyval && keycol) {
	  itree_traverse(dlst)
	       table_destroy(itree_get(dlst));
     }
     itree_destroy(dlst);
     itree_destroy(idx);
     nfree(vals);
     return i;
}
#endif


/*
 * Convert a TABLE structure into nmalloc()'ed float arrays ready for
 * plotting with the draw command. Returns the number of samples in the
 * arrays and sets the pointers addressed xvals and yvals to the x and y 
 * values as output. If there are no usable samples, 0 is returned.
 * There must be a _time column and values lying between oldest_t and 
 * youngest_t will be converted.
 * If keycol and keyval are NULL, the data is assumed to have no key;
 * if they are set, however, then keyed data is extracted from the
 * table(s) within RESDAT
 * Count data is transformed into absolute values (difference over time) 
 * for plotting, where as absooute data is left alone. In this case, 
 * the first value is lost as it is used as a base.
 * Count and absolute types of data are 'rebased' depending on the 
 * g->start value (the only reason we need a UIGRAPH structure passed).
 * The main reason for this is to cope with loss of accuracy of a float 
 * at large values (time_t in the year 2000).
 * Absolute data is left alone but count data is processed for differencies
 * over time. The first sample of a ring is lost as it provides the base for
 * calculations on subsequent data.
 * Time should be identified using the column name _time. If it does not 
 * exist or there are values missing, then an attempt is made to create
 * a mock time based on one second intervals from the epoch.
 */
int gconv_table2arrays(GRAPHDBOX *g,	/* graph structure */
		       TABLE intab,	/* input data */
		       time_t oldest_t,	/* oldest data to be converted */
		       time_t youngest_t, /* youngest data to be converted */
		       char *colname,	/* column containing data */
		       char *keycol,	/* column containing key */
		       char *keyval,	/* value of key */
		       float **xvals,	/* output array of xvalues */
		       float **yvals 	/* output array of yvalues */ )
{
     TABLE tab;
     ITREE *vallst, *timlst, *collst;
     char *sense;
     int iscnt=0, isfirst, nvals=0, i;
     time_t newtim=0, lasttim=0, mocktim=0;
     float lastval=0.0, newval=0.0;
     TABSET tabsub;

     /* create a subset (tableset from table) based on time */
     collst = itree_create();
     itree_append(collst, "_time");
     itree_append(collst, colname);

     tabsub = tableset_create(intab);
     tableset_select(tabsub, collst);
     tableset_where(tabsub, "_time", ge, util_u32toa(oldest_t));
     tableset_where(tabsub, "_time", le, util_u32toa(youngest_t));

     /* if keys are set, then set additional 'where' clauses to filter 
      * out non-key rows */
     if (keycol && keyval) {
	  tableset_where(tabsub, keycol, eq, keyval);
     }

     /* produce a derived table to use for extraction */
     tab = tableset_into(tabsub);	/* supposed not to fail */
     nvals = table_nrows(tab);
     if ( ! nvals ) {
          itree_destroy(collst);
	  tableset_destroy(tabsub);
	  table_destroy(tab);
          return 0;	/* no valid data to plot (it probably shrank) */  
     }

     /* allocate space */
     *xvals = xnmalloc(nvals * sizeof(gfloat));
     *yvals = xnmalloc(nvals * sizeof(gfloat));
     /*elog_printf(DEBUG, "converting %d values", nvals);*/

     /* Is this count data? 
      * Count data are integer counters that increment only, so we should
      * plot the increment */
     sense = table_getinfocell(tab, "sense", colname);
     if (sense && strcmp(sense, "cnt") == 0)
          iscnt = 1;
     else
          iscnt = 0;
     /*elog_printf(DEBUG, "column %s is %s", colname, 
                   iscnt?"count":"absolute");*/

     /* extract column of values as strings */
     vallst = table_getcol(tab, colname);
     if ( ! vallst ) {
	  /* curve not in this table */
          itree_destroy(collst);
	  tableset_destroy(tabsub);
	  table_destroy(tab);
          return 0;
     }

     /* extract the time column to correspond to the value column */
     timlst = table_getcol(tab, "_time");
     isfirst = 1;	/* first interation is special */
     i = 0;
     if (iscnt) {
          /* Counter - incremental data counter carry out a cnt to abs 
	   * conversion */

          /* increment over the two columns */
          if (timlst)
	       itree_first(timlst);
	  itree_traverse(vallst) {
	       if (isfirst) {
		    /* first value of a count series - remember but dont use */
		    if ( itree_get(vallst) )
		      lastval = atof((char *)itree_get(vallst));
		    else
		         lastval = 0.0;
		    if ( timlst && itree_get(timlst) )
		         lasttim = strtol((char *)itree_get(timlst), 
					  (char**)NULL, 10);
		    else
		         lasttim = mocktim;
		    isfirst = 0;	/* not first any more */
	       } else {
		    /* second value and onwards of a count series - use */
		    if ( itree_get(vallst) )
		         newval = atof(itree_get(vallst));
		    else
		         newval = 0.0;
		    if ( timlst && itree_get(timlst) )
		         newtim = strtol((char *)itree_get(timlst), 
					 (char**)NULL, 10);
		    else
		         newtim = mocktim;
		    if (newtim < lasttim)
		         /* time went backwards: save the difference in value */
		         (*yvals)[i] = newval - lastval;
		    else if (newtim == lasttim) {
		         /* repeat time: drop sample */
		         elog_printf(WARNING, "found a duplicate point for "
				     "the same time (%s)", 
				     util_decdatetime(newtim));
			 continue;
		    } else
		         /* average per second */
		         (*yvals)[i] = (newval - lastval) / (newtim - lasttim); 
		    lastval = newval;	/* new becomes old */
		    lasttim = newtim;
		    (*xvals)[i] = (float) (newtim - g->start);
		    i++;	/* sample used */
	       }
	       if (timlst)
		    itree_next(timlst);
	       else
		    mocktim++;
	  }
     } else {
          /* abs - absolute value */
          if (timlst)
	       itree_first(timlst);
	  itree_traverse(vallst) {
	       /* convert value */
	       if ( itree_get(vallst) )
		    (*yvals)[i] = strtof((char*)itree_get(vallst),
					 (char**)NULL);
	       else
		    (*yvals)[i] = 0.0;	/* missing value goes 0.0 :-( */
	       /* convert time */
	       if (timlst && itree_get(timlst))
		    (*xvals)[i] = strtof((char *)itree_get(timlst), 
					 (char**)NULL) - g->start;
	       else
		    (*xvals)[i] = mocktim;
	       /* iterate */
	       if (timlst)
		    itree_next(timlst);
	       else 
		    mocktim++;
	       i++;
	  }
     }

     /* clear up lists */
     if (timlst)
          itree_destroy(timlst);
     itree_destroy(vallst);

     /*elog_printf(DEBUG, "extracted %d values", itree_n(idx));*/

#if 0
     /* debug dump of the data converted to floats */
     elog_startprintf(DEBUG, "values to graph: g->start=%d -> ", g->start);
     for (int j=0; j<nvals; j++) {
	  elog_contprintf(DEBUG, "j=%d "
			         "xvals[%d]=%.2f yvals[%d]=%.2f, ", 
			  j, j, (*xvals)[j], j, (*yvals)[j]);
	  j++;
     }
     elog_endprintf(DEBUG, "%d values", i);
#endif

     /* free working storage and return the number of pairs there are */
     itree_destroy(collst);
     tableset_destroy(tabsub);
     table_destroy(tab);
     return i;
}


