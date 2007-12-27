/*
 * Gtk multicurve graph widget
 * This is a wrapper over the gtkplot for convenience
 *
 * Nigel Stuckey, September 1999
 * Copyright System Garden Limited 1999-2001. All rights reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gtkdatabox.h"
#include "gmcgraph.h"
#include "main.h"
#include "../iiab/itree.h"
#include "../iiab/timeline.h"
#include "../iiab/util.h"

char *gmcgraph_colours[] = { "red", "green", "orange", "purple", "cyan", 
			     "magenta", "LimeGreen", "gold", "maroon", 
			     "RosyBrown", "BlueViolet", "SpringGreen1", 
			     "IndianRed1", "DeepPink1", "DodgerBlue", 
			     "DarkSeaGreen", "goldenrod", "SaddleBrown", 
			     "coral", "DarkViolet", 
			     "VioletRed", "DeepSkyBlue4", "OliveDrab1", 
			     "OliveDrab4", 
			     /* second division */
			     "tan", "firebrick", 
			     /* also rans */
			     "SlateBlue", "Royalblue", "DarkGreen", 
			     "LawnGreen", "khaki", "plum1", "thistle1", 
			     "PaleGreen", "LimeGreen", "sienna", 
			     "DarkGoldenrod1", "yellow", "pink", 
			     "purple", "DarkOrange", "DarkSlateGray", 
			     "DarkSeaGreen",
			     NULL};

/*
 * Create a GtkDataBox canvas in the containing window supplied, and
 * initialise to standard settings 
 */
GMCGRAPH *gmcgraph_init(GtkWidget *base_window, GtkWidget *hpane)
{
     GMCGRAPH *g;

     /* initialise structure ready for graph creation */
     g = (GMCGRAPH *) xnmalloc(sizeof(struct gmcgraph_all));
     g->graphs = tree_create();
     g->start = 0;
     g->end = 0;
     g->curvecol = tree_create();
     g->colunused = itree_create();
     g->nextcol = 0;

     /* initialise the gui */
     g->container = gtk_vbox_new(TRUE, 1);
     gtk_widget_ref(g->container);
     gtk_object_set_data_full(GTK_OBJECT (base_window), "graphbox", 
			      g->container,
			      (GtkDestroyNotify) gtk_widget_unref);
     gtk_widget_show (g->container);
     gtk_paned_add1 (GTK_PANED (hpane), g->container);
     gtk_widget_set_name(g->container, "gmcgraph");

     return g;
}


/* Remove the GtkDataBox widget and clear out all references held to it */
void gmcgraph_fini(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);

	  gtk_widget_destroy(gs->widget);
	  tree_traverse(gs->curves) {
	       nfree(tree_get(gs->curves));
	       nfree(tree_getkey(gs->curves));
	  }
	  tree_destroy(gs->curves);

	  nfree(tree_getkey(g->graphs));
	  nfree(gs);
     }
     tree_destroy(g->graphs);
     tree_destroy(g->curvecol);
     itree_destroy(g->colunused);
     nfree(g);
}


/* Returns true if the curve has been drawn and its data is being held */
int gmcgraph_iscurvedrawn(GMCGRAPH *g, char *graph_name, char *curve_name)
{
     if (gmcgraph_lookupcurve(g, graph_name, curve_name))
	  return 1;
     else
	  return 0;
}


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
 * g->start value (the only reason we need a GMCGRAPH structure passed).
 * The main reason for this is to cope with loss of accuracy of a float 
 * at large values (time_t in the year 2000).
 * Absolute data is left alone but count data is processed for differencies
 * over time. The first sample of a ring is lost as it provides the base for
 * calculations on subsequent data.
 * Time should be identified using the column name _time. If it does not 
 * exist or there are values missing, then an attempt is made to create
 * a mock time based on one second intervals from the epoch.
 */
int gmcgraph_resdat2arrays(GMCGRAPH *g,	  /* graph structure */
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
     char *prt;		/* debug variable */

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
			      lasttim = atoi(itree_get(timlst));
			 else
			      lasttim = mocktim;
			 isfirst = 0;
		    } else {
			 if ( itree_get(vallst) )
			      newval = atof(itree_get(vallst));
			 else
			      newval = 0.0;
			 if ( timlst && itree_get(timlst) )
			      newtim = atoi(itree_get(timlst));
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
			 itree_add(idx, atoi(itree_get(timlst)), &vals[i]);
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


/*
 * Draw a new curve (and maybe graph) or replace an existing one
 * using the values supplied in xvals and yvals. 
 * The parameters are:-
 * graph_name   If unknown, a new graph is created; if NULL a default is
 *              created and used.
 * curve_name   If unknown, a new curve is drawn and will be assigned a colour.
 *              If the curve exists, new data is drawn with the same colour 
 *              to replace or extend the existing curve. 
 *              All curves of the same name share the same colour 
 *              regardless of graph
 * nvals        Number of values
 * xvals, yvals X and Y Arrays of nmalloc()ed data points, which will be 
 *              adopted by this function for garbage collection. The 
 *              caller should take no further responsibility for this memory.
 * overwrite    If true the previous curve is replaced with the new one.
 *              If false the new data will be appended to the old data
 *              and the combined set is drawn. Not implemented yet.
 * NB: only overwrite=1 is supported at present.
 * Returns the colour of the rendered curve or NULL if there was a problem, 
 * including insufficient values to plot.
 */
GdkColor *gmcgraph_draw(GMCGRAPH *g, char *graph_name, char *curve_name, 
			int nvals, float *xvals, float *yvals, int overwrite)
{
     static GdkColor colour;
     char *gname;
     struct gmcgraph_graph *gs;
     struct gmcgraph_curve *mycurve, *fixcurve;
     int killindex;

     /* assert!! */
     if (nvals < 2) {
	  elog_printf(ERROR, "Can't draw curve %s %s: only has %d values",
		      graph_name ? graph_name : "(default)", curve_name,
		      nvals);
     }

     /* choose the graph name, defaulting if one is not given */
     if (graph_name)
	  gname = graph_name;
     else
	  gname = GMCGRAPH_DEFGRAPHNAME;

#if 0
     /* debug */
     for (int i=0; i<nvals; i++)
	  elog_printf(DEBUG, "passed data x=%f    y=%f\n", xvals[i], yvals[i]);
#endif

     /* get the existing graph or create a new one */
     gs = tree_find(g->graphs, gname);
     if (gs == TREE_NOVAL)
	  gs = gmcgraph_newgraph(g, gname);

#if 0
     /* debug */
     for (int i=0; i<nvals; i++)
	  elog_printf(DEBUG, "plotting   x=%f    y=%f\n", xvals[i], yvals[i]);
#endif

     /* have I plotted this before? if so, curve structure */
     if ( (mycurve = tree_find(gs->curves, curve_name)) != TREE_NOVAL ) {
	  if (overwrite) {
	       /* remove existing curve from widget only [gmcgraph_rmcurve()
		* removes from widget and list] */
	       /*g_print("recycling curve %s (index %d)\n", curve_name, 
		 mycurve->index);*/
	       gtk_databox_data_remove(GTK_DATABOX(gs->widget), 
				       mycurve->index);
	       nfree(mycurve->X);
	       nfree(mycurve->Y);
	       mycurve->X = xvals;
	       mycurve->Y = yvals;

	       /* Nasty!! When we delete data we have to reindex to be
		* in step with the order in the widget's linked list */
	       killindex = mycurve->index;
	       tree_traverse(gs->curves) {
		    fixcurve = tree_get(gs->curves);
		    if (fixcurve->index > killindex)
			 fixcurve->index--;		/* decrement */
	       }
	  } else {
	       g_print("should not be here!!\n");
	       elog_printf(ERROR, "extending curves not yet supported");
	       mycurve->X = xvals;	/* should we store these vals?? */
	       mycurve->Y = yvals;
	  }
     } else {
	  /* -- draw a new curve -- */

	  /* allocate the curve structure */
	  mycurve = nmalloc(sizeof(struct gmcgraph_curve));
	  mycurve->X = xvals;
	  mycurve->Y = yvals;
	  mycurve->style = gs->style;

	  /* add curve to tree */
	  tree_add(gs->curves, xnstrdup(curve_name), mycurve);
     }

     /* select a colour */
     gmcgraph_usecolour(g, curve_name, &colour);

     /* create new curve and draw on selected graph */
     switch (mycurve->style) {
     case GMCGRAPH_THINLINE:
	  mycurve->index = gtk_databox_data_add_x_y(GTK_DATABOX(gs->widget), 
						    nvals,xvals,yvals,colour,
						    GTK_DATABOX_LINES, 1);
	  break;
     case GMCGRAPH_MIDLINE:
	  mycurve->index = gtk_databox_data_add_x_y(GTK_DATABOX(gs->widget), 
						    nvals,xvals,yvals,colour,
						    GTK_DATABOX_LINES, 2);
	  break;
     case GMCGRAPH_FATLINE:
	  mycurve->index = gtk_databox_data_add_x_y(GTK_DATABOX(gs->widget), 
						    nvals,xvals,yvals,colour,
						    GTK_DATABOX_LINES, 3);
	  break;
     case GMCGRAPH_POINT:
	  mycurve->index = gtk_databox_data_add_x_y(GTK_DATABOX(gs->widget), 
						    nvals,xvals,yvals,colour,
						    GTK_DATABOX_POINTS, 3);
	  break;
     case GMCGRAPH_BAR:
	  mycurve->index = gtk_databox_data_add_x_y(GTK_DATABOX(gs->widget), 
						    nvals,xvals,yvals,colour,
						    GTK_DATABOX_BARS, 1);
	  break;
     }

     gmcgraph_updateaxis(gs);

     /*gtk_databox_redraw(GTK_DATABOX(gs->widget));*/
     /*g_print("curve %s allocated index %d\n", curve_name, mycurve->index);*/

     return &colour;
}


/* return 1 if the graph has been zoomed or 0 otherwise */
int gmcgraph_iszoomed(struct gmcgraph_graph *gs) {
     if (GTK_DATABOX(gs->widget)->adjX->page_size < 0.99 ||
	 GTK_DATABOX(gs->widget)->adjX->value     > 0.01 ||
	 GTK_DATABOX(gs->widget)->adjY->page_size < 0.99 ||
	 GTK_DATABOX(gs->widget)->adjY->value     > 0.01)

	  return 1;
     else
	  return 0;
}

/*
 * Redraws the graph named graph_name to the screen. Used after
 * gmcgraph_draw() for efficently drawing lots of curves then doing
 * a single update.
 */
void gmcgraph_update(GMCGRAPH *g, char *graph_name)
{
     char *gname;
     struct gmcgraph_graph *gs;

     /* choose the graph name */
     if (graph_name)
	  gname = graph_name;
     else
	  gname = GMCGRAPH_DEFGRAPHNAME;

     /* get the structure */
     gs = tree_find(g->graphs, gname);
     if (gs == TREE_NOVAL)
	  return;

     gtk_databox_redraw(GTK_DATABOX(gs->widget));
}


/* Create a new graph. If there is no default name, set it to graph_name.
 * Uses the globals show_rulers and show_axis to determin whether an
 * axis or rulers are to be drawn and view_histogram to set the default
 * graph style to line or bar.
 * Returns the address of the graph structure, although this has already
 * been added to the GMCGRAPH lists (g->graphs) */
struct gmcgraph_graph *gmcgraph_newgraph(GMCGRAPH *g, char *graph_name)
{
     struct gmcgraph_graph *gs;
     char *gname;

     /* allocate name */
     gname = xnstrdup(graph_name);

     /* set up timeline, used in horizontal ruler */
     timeline_setoffset(g->start);

     /* allocate and initialise graph structure */
     gs = xnmalloc(sizeof(struct gmcgraph_graph));
     tree_add(g->graphs, gname, gs);
     gs->curves = tree_create();
     if (view_histogram)
	  gs->style = GMCGRAPH_BAR;
     else
	  gs->style = GMCGRAPH_THINLINE;
     gs->minmax = 0.0;		/* special 'ignore me' value */
     gs->parent = g;

     /* create widget and add to containing widget.
      * show_rulers and show_axis are globals */
     gs->widget = gtk_databox_new();
     gtk_widget_ref (gs->widget);
     gtk_object_set_data_full(GTK_OBJECT(baseWindow), "databox", gs->widget,
			      (GtkDestroyNotify) gtk_widget_unref);
     gtk_box_pack_start (GTK_BOX(g->container), gs->widget, TRUE, TRUE, 0);
     gtk_widget_set_usize(gs->widget, -2, -2);
     if (show_rulers)
	  gtk_databox_show_rulers(GTK_DATABOX(gs->widget));
     else
	  gtk_databox_hide_rulers(GTK_DATABOX(gs->widget));
     if (show_axis)
	  gtk_databox_show_cross (GTK_DATABOX(gs->widget));
     else
	  gtk_databox_hide_cross (GTK_DATABOX(gs->widget));
     gtk_widget_show(gs->widget);

     return gs;
}

/* set the timebase on the xaxis, such that the range min..max is shown
 * on the axis and arrange for it to be labeled suitably.
 * If min or max are -1 then do not change that value.
 */
void gmcgraph_settimebase(GMCGRAPH *g, time_t min, time_t max)
{
     char min_str[UTIL_SHORTSTR], max_str[UTIL_SHORTSTR];

     strcpy(min_str, util_shortadaptdatetime(min));
     strcpy(max_str, util_shortadaptdatetime(max));
     elog_printf(DEBUG, "timebase from min %s (%ld) .. max %s (%ld)", 
		 min_str, min, max_str, max);
     if (min > -1)
	  g->start = min;
     if (max > -1)
	  g->end   = max;
}


/*
 * Set the timebase by the node arguments tstart and tend, if present.
 * If not present, the graph will be unaltered. Retuns 1 if set, 0 if not
 */
int gmcgraph_settimebasebynode(GMCGRAPH *g, TREE *nodeargs)
{
     time_t tstart, tend;
     int *tsecs;

     /* find the graph timebase range from node arguments */
     tsecs = tree_find(nodeargs, "tsecs");
     if ( tsecs == TREE_NOVAL )
          return 0;
     if ( tsecs == NULL )
          elog_die(FATAL, "tsecs node argument is NULL");
     tend = time(NULL);
     tstart = tend - *tsecs;

     /* convert to time and use them to set graph timebase */
     gmcgraph_settimebase(g, tstart, tend);
     return 1;
}


/* Set the maximum value to be displayed on the y-axis, providing that
 * no data point exceeds it. If a data point does exceed it, then the 
 * true maximum should be drawn, with a 5% margin of white space.
 * It should still be possible to zoom the display after this call.
 * A value of 0.0 clears the effect
 */
void gmcgraph_setallminmax(GMCGRAPH *g, float value)
{
     struct gmcgraph_graph *gs;
     elog_printf(DEBUG, "setting minmax value to %f (%f asked)", 
		 value * 1.05, value);
     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gs->minmax = value * 1.05;
     }
}


/* update the graph range and set axis ticks but does not redraw; the
 * caller should call gtk_databox_redraw() when ready */
void gmcgraph_updateaxis(struct gmcgraph_graph *gs)
{
     GtkDataboxValue min, max;

     /* if in a zoomed state, we need to suspend to axis update */
     if (gmcgraph_iszoomed(gs))
	  return;

     if (gs->parent->end) {
	  /* timebase has been set: set the timeline for correct 
	   * translation from 0..max to gs->parent->start..gs->parent->end */
	  timeline_setoffset(gs->parent->start);

	  /*we use a margin on 5% on the start and end times for a
	   * better appearence and to prevent the impression of clipping */
	  /*gtk_databox_rescale(GTK_DATABOX(gs->widget));*/
	  /*gtk_databox_data_get_extrema(GTK_DATABOX(gs->widget),&min,&max);*/
	  gtk_databox_data_calc_extrema (GTK_DATABOX(gs->widget), &min, &max);
          /*g_print("calc: min (%f,%f) max (%f,%f)  --  ", 
	    min.x, min.y, max.x, max.y);*/

	  max.x = (gs->parent->end - gs->parent->start) * 1.05;
	  max.y = (max.y < 1.0) ? 1.0 : max.y;
	  if (gs->minmax != 0.0)
	       max.y = (max.y < gs->minmax) ? gs->minmax : max.y;
	  min.x = - ( (gs->parent->end - gs->parent->start) * .05 );
	  min.y = - ( max.y * .05 );
	  /* g_print("fudged: min (%f,%f) max (%f,%f)\n", 
	     min.x, min.y, max.x, max.y);*/

	  gtk_databox_rescale_with_values(GTK_DATABOX(gs->widget), min, max);
     } else {
	  /* timebase not set */
	  gtk_databox_rescale(GTK_DATABOX(gs->widget));
     }
     elog_printf(DEBUG, "axis updates on %s to min (%.2f,%.2f) "
		 "max (%.2f,%.2f)",
		 "unknown", min.x, min.y, max.x, max.y);
}


/* update the graph range and set axis ticks */
void gmcgraph_updateallaxis(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gmcgraph_updateaxis(gs);
     }
}

/* remove the curve from the given graph */
void gmcgraph_rmcurve(GMCGRAPH *g, char *graph_name, char *curve_name)
{
     struct gmcgraph_graph *gs;
     struct gmcgraph_curve *mycurve;
     int killindex;

     /* find graph */
     gs = gmcgraph_lookupgraph(g, graph_name);
     if ( ! gs )
	  return;

     /* find curve in graph */
     if ( (mycurve = tree_find(gs->curves, curve_name)) != TREE_NOVAL)
     {
	  /* remove curve from widget and our list of drawn curves */
	  killindex = mycurve->index;
	  gtk_databox_data_remove(GTK_DATABOX(gs->widget), killindex);
	  gmcgraph_updateaxis(gs);
	  gtk_databox_redraw(GTK_DATABOX(gs->widget));
	  nfree(mycurve->X);
	  nfree(mycurve->Y);
	  /* unreference mycurve->style ? */
	  nfree(mycurve);
	  nfree(tree_getkey(gs->curves));
	  tree_rm(gs->curves);

	  /* Nasty!! When we delete data we have to reindex */
	  tree_traverse(gs->curves) {
	       mycurve = tree_get(gs->curves);
	       if (mycurve->index > killindex)
		    mycurve->index--;		/* decrement */
	  }

	  /* give back the curve colour */
	  gmcgraph_recyclecolour(g, curve_name);
     }
}

/* hide the curve from view */
void gmcgraph_hidecurve(GMCGRAPH *g, char *graph_name, char *curve_name)
{
     struct gmcgraph_graph *gs;
     struct gmcgraph_curve *mycurve;

     /* find graph */
     gs = gmcgraph_lookupgraph(g, graph_name);
     if ( ! gs )
	  return;

     /* find curve in graph */
     if ( (mycurve = tree_find(gs->curves, curve_name)) != TREE_NOVAL)
     {
	  gtk_databox_set_data_type(GTK_DATABOX(gs->widget), mycurve->index, 
				    GTK_DATABOX_NOT_DISPLAYED, 0);
     }
}

/* make an existing curve visible */
void gmcgraph_showcurve(GMCGRAPH *g, char *graph_name, char *curve_name)
{
     struct gmcgraph_graph *gs;
     struct gmcgraph_curve *mycurve;

     /* find graph */
     gs = gmcgraph_lookupgraph(g, graph_name);
     if ( ! gs )
	  return;

     /* find curve in graph */
     if ( (mycurve = tree_find(gs->curves, curve_name)) != TREE_NOVAL)
     {
	  /* create new curve and draw on selected graph */
	  switch (mycurve->style) {
	  case GMCGRAPH_THINLINE:
	       gtk_databox_set_data_type(GTK_DATABOX(gs->widget), 
					 mycurve->index, GTK_DATABOX_LINES, 1);
	       break;
	  case GMCGRAPH_FATLINE:
	       gtk_databox_set_data_type(GTK_DATABOX(gs->widget), 
					 mycurve->index, GTK_DATABOX_LINES, 3);
	       break;
	  case GMCGRAPH_POINT:
	       gtk_databox_set_data_type(GTK_DATABOX(gs->widget), 
					 mycurve->index, GTK_DATABOX_POINTS,3);
	       break;
	  case GMCGRAPH_BAR:
	       gtk_databox_set_data_type(GTK_DATABOX(gs->widget), 
					 mycurve->index, GTK_DATABOX_BARS, 1);
	       break;
	  }
     }
}

/* remove all the curves on the specified graph */
void gmcgraph_rmallcurves(GMCGRAPH *g, char *graph_name)
{
     struct gmcgraph_graph *gs;
     struct gmcgraph_curve *mycurve;
     char *mycurvekey;

     /* find graph */
     gs = gmcgraph_lookupgraph(g, graph_name);
     if ( ! gs )
	  return;

     /* clear the widget of curves */
     gtk_databox_data_remove_all(GTK_DATABOX(gs->widget));

     /* clear curve storage, written in a way that does not clash with
      * the recycle code */
     while ( ! tree_empty(gs->curves) ) {
	  /* point to storage and delete list entry */
	  tree_first(gs->curves);
	  mycurve    = tree_get(gs->curves);
	  mycurvekey = tree_getkey(gs->curves);
	  tree_rm(gs->curves);
	  /* recycle colour */
	  gmcgraph_recyclecolour(g, mycurvekey);
	  /* free storage */
	  nfree(mycurve->X);
	  nfree(mycurve->Y);
	  nfree(mycurve);
	  nfree(mycurvekey);
     }
}

/* remove the specified graph */
void gmcgraph_rmgraph(GMCGRAPH *g, char *graph_name)
{
     struct gmcgraph_graph *gs;
     struct gmcgraph_curve *mycurve;

     /* find graph */
     gs = gmcgraph_lookupgraph(g, graph_name);
     if ( ! gs )
	  return;

     /* clear the widget of curves */
     gtk_databox_data_remove_all(GTK_DATABOX(gs->widget));
     gtk_widget_destroy(gs->widget);

     /* clear curve storage */
     while ( ! tree_empty(gs->curves) ) {
	  tree_first(gs->curves);
	  mycurve = tree_get(gs->curves);
	  nfree(mycurve->X);
	  nfree(mycurve->Y);
	  nfree(mycurve);
	  nfree(tree_getkey(gs->curves));
	  tree_rm(gs->curves);
     }

     /* delete the graph structure */
     tree_destroy(gs->curves);
     nfree(gs);

     /* remove graph references */
     nfree(tree_getkey(g->graphs));
     tree_rm(g->graphs);

     /* remove the colour allocations and reset if there are no more graphs 
      * in the collective GMCGRAPH structure */
     if (tree_empty(g->graphs)) {
	  tree_clearout(g->curvecol, tree_infreemem, NULL);
	  itree_clearout(g->colunused, NULL);
	  g->nextcol = 0;
     }
}

/* remove all graphs (and curves) but leave the gmcgraph structure standing */
void gmcgraph_rmallgraphs(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;
     struct gmcgraph_curve *mycurve;

     while ( ! tree_empty(g->graphs) ) {
	  tree_first(g->graphs);
	  gs = tree_get(g->graphs);

	  gtk_widget_destroy(gs->widget);
	  /* clear curve storage */
	  while ( ! tree_empty(gs->curves) ) {
	       tree_first(gs->curves);
	       mycurve = tree_get(gs->curves);
	       nfree(mycurve->X);
	       nfree(mycurve->Y);
	       nfree(mycurve);
	       nfree(tree_getkey(gs->curves));
	       tree_rm(gs->curves);
	  }
	  tree_destroy(gs->curves);

	  nfree(tree_getkey(g->graphs));
	  nfree(gs);
	  tree_rm(g->graphs);
     }

     /* remove the colour allocations and reset */
     tree_clearout(g->curvecol, tree_infreemem, NULL);
     itree_clearout(g->colunused, NULL);
     g->nextcol = 0;
}

/* looks up the named graph, returning NULL if no graph exists or
 * the pointer to the graph structure otherwise */
struct gmcgraph_graph *gmcgraph_lookupgraph(GMCGRAPH *g, char *graph_name)
{
     char *gname;

     /* choose the graph name */
     if (graph_name)
	  gname = graph_name;
     else
	  gname = GMCGRAPH_DEFGRAPHNAME;

     if ( ! gname)
	  return NULL;		/* no graphs created */

     /* find the graph */
     if (tree_find(g->graphs, gname) == TREE_NOVAL)
	  return NULL;		/* no such graph name */
     else
	  return tree_get(g->graphs);
}

/* looks up the named graph, returning NULL if no graph and curve exists or
 * a pointer to struct gmcgraph_curve if found */
struct gmcgraph_curve *gmcgraph_lookupcurve(GMCGRAPH *g, char *graph_name, 
					    char *curve_name)
{
     char *gname;
     struct gmcgraph_graph *gs;

     /* choose the graph name */
     if (graph_name)
	  gname = graph_name;
     else
	  gname = GMCGRAPH_DEFGRAPHNAME;

     if ( ! gname)
	  return NULL;		/* no graphs created */

     /* find the graph */
     if (tree_find(g->graphs, gname) == TREE_NOVAL)
	  return NULL;		/* no such graph name */
     else {
	  gs = tree_get(g->graphs);
	  if (tree_find(gs->curves, curve_name) == TREE_NOVAL)
	       return NULL;	/* no such curve name */
	  else
	       return tree_get(gs->curves);	/* found curve */
     }
}

void gmcgraph_scale(GMCGRAPH *g, char *curve_name, float *scale_factor)
{
}

void gmcgraph_offset(GMCGRAPH *g, char *graph_name, char *curve_name, 
		     int offset, float factor)
{
}

void gmcgraph_regcurvedb(GMCGRAPH *g, char *curve_name, void (*clickcb)())
{
}

void gmcgraph_multiaxis(GMCGRAPH *g)
{
}

void gmcgraph_singleaxis(GMCGRAPH *g)
{
}

void gmcgraph_getcurveorder(GMCGRAPH *g)
{
}

void gmcgraph_setcurveorder(GMCGRAPH *g)
{
}

void gmcgraph_curvestyle(GMCGRAPH *g, char *curve_name, int fillarea)
{
}

/* return the major tick interval */
gfloat gmcgraph_majticks(gfloat max)
{
     return 5.0;
}

/* return the minor tick interval */
gfloat gmcgraph_minticks(gfloat max)
{
     return 1.0;
}

/* Hide the axis of all graphs */
void gmcgraph_hideallaxis(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_hide_cross( GTK_DATABOX(gs->widget) );
	  gtk_databox_redraw( GTK_DATABOX(gs->widget) );
     }
}

/* Show the axis of all graphs */
void gmcgraph_showallaxis(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_show_cross( GTK_DATABOX(gs->widget) );
	  gtk_databox_redraw( GTK_DATABOX(gs->widget) );
     }
}

/* Hide the rulers of all graphs */
void gmcgraph_hideallrulers(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_hide_rulers( GTK_DATABOX(gs->widget) );
	  gtk_databox_redraw( GTK_DATABOX(gs->widget) );
     }
}

/* Show the rulers of all graphs */
void gmcgraph_showallrulers(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_show_rulers( GTK_DATABOX(gs->widget) );
	  gtk_databox_redraw( GTK_DATABOX(gs->widget) );
     }
}


/* Change all graphs to the given style type */
void gmcgraph_allgraphstyle(GMCGRAPH *g, enum gmcgraph_graphtype style)
{
     struct gmcgraph_graph *gs;
     struct gmcgraph_curve *curve;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gs->style = style;
	  tree_traverse(gs->curves) {
	       curve = tree_get(gs->curves);
	       switch (style) {
	       case GMCGRAPH_THINLINE:
		    gtk_databox_set_data_type( GTK_DATABOX(gs->widget), 
					       curve->index,
					       GTK_DATABOX_LINES, 1 );
		    break;
	       case GMCGRAPH_FATLINE:
		    gtk_databox_set_data_type( GTK_DATABOX(gs->widget), 
					       curve->index,
					       GTK_DATABOX_LINES, 3 );
		    break;
	       case GMCGRAPH_POINT:
		    gtk_databox_set_data_type( GTK_DATABOX(gs->widget), 
					       curve->index,
					       GTK_DATABOX_POINTS, 3 );
		    break;
	       case GMCGRAPH_BAR:
		    gtk_databox_set_data_type( GTK_DATABOX(gs->widget), 
					       curve->index,
					       GTK_DATABOX_BARS, 3 );
		    break;
	       }
	  }
	  gtk_databox_redraw( GTK_DATABOX(gs->widget) );
     }
}

/* Change specified graph to the given style type */
void gmcgraph_graphstyle(GMCGRAPH *g, char *graph_name, 
			 enum gmcgraph_graphtype style)
{
}


/* Zoom in to the middle third on the graph's x-axis, leaving the 
 * y-axis alone */
void gmcgraph_allgraph_zoomin_x(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;
     gint width, height;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gdk_window_get_size (gs->widget->window, &width, &height);
	  GTK_DATABOX(gs->widget)->marked.x = width/3;
	  GTK_DATABOX(gs->widget)->marked.y = 0;
	  GTK_DATABOX(gs->widget)->select.x = width/3*2;
	  GTK_DATABOX(gs->widget)->select.y = height;
	  gtk_databox_zoom_to_selection (gs->widget, GTK_DATABOX(gs->widget));
	  elog_printf(DEBUG, 
		      "X zoom: h=%d w=%d, marked=(%d,%d) select=(%d,%d)", 
		      height, width, width/3, 0, width/3*2, height);
     }
}

/* Zoom in to the middle third on the graph's y-axis, leaving the 
 * x-axis alone */
void gmcgraph_allgraph_zoomin_y(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;
     gint width, height;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gdk_window_get_size (gs->widget->window, &width, &height);
	  GTK_DATABOX(gs->widget)->marked.x = 0;
	  GTK_DATABOX(gs->widget)->marked.y = height/3;
	  GTK_DATABOX(gs->widget)->select.x = width;
	  GTK_DATABOX(gs->widget)->select.y = height/3*2;
	  gtk_databox_zoom_to_selection (gs->widget, GTK_DATABOX(gs->widget));
	  elog_printf(DEBUG, 
		      "Y zoom: h=%d w=%d, marked=(%d,%d) select=(%d,%d)", 
		      height, width, width/3, 0, width/3*2, height);
     }
}

/* Partially zoom out of all graphs */
void gmcgraph_allgraph_zoomout_x(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_zoom_out( gs->widget, GTK_DATABOX(gs->widget) );
     }
}


/* Partially zoom out of all graphs */
void gmcgraph_allgraph_zoomout_y(GMCGRAPH *g)
{
     struct gmcgraph_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_zoom_out( gs->widget, GTK_DATABOX(gs->widget) );
     }
}


/*
 * Given a curve name, allocate a new colour if new or return a previously
 * used one if used before.
 */
void gmcgraph_usecolour(GMCGRAPH *g, char *curvename, GdkColor *col)
{
     char *colname;
     int colindex;

     colindex = (int) tree_find(g->curvecol, curvename);
     if ((void *) colindex == TREE_NOVAL) {
	  /* new curve, allocate a new colour */
	  if (itree_n(g->colunused) > 0) {
	       /* allocate one from the unused pile */
	       itree_first(g->colunused);
	       colindex = itree_getkey(g->colunused);
	       colname = gmcgraph_colours[colindex];
	       itree_rm(g->colunused);
	  } else if (gmcgraph_colours[g->nextcol] != NULL) {
	       /* allocate a 'virgin' colour */
	       colindex = g->nextcol++;
	       colname = gmcgraph_colours[colindex];
	  } else {
	       /* we've run out of virgins !!! */
	       colindex = -1;
	       colname = "black";
	  }

	  tree_add(g->curvecol, xnstrdup(curvename), (void *) colindex);
     } else {
	  /* existing curve */
	  if (colindex == -1)
	       colname = "black";
	  else
	       colname = gmcgraph_colours[colindex];
     }

     /* parse into a GdkColor structre */
     gdk_color_parse( colname, col );
}



/*
 * Given a curve name, check the GMCGRAPH structure to see if the curve
 * is used in any graphs. If not, then recycle the colour associated 
 * with the curve for later use [with gmcgraph_usecolour()].
 * Returns nothing.
 */
void gmcgraph_recyclecolour(GMCGRAPH *g, char *curvename)
{
     struct gmcgraph_graph *gs;
     int colindex;

     colindex = (int) tree_find(g->curvecol, curvename);
     if ((void *) colindex == TREE_NOVAL)
	  return;	/* curve name not valid */

     /* check if the curve is used */
     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  tree_traverse(gs->curves) {
	       if (strcmp(tree_getkey(gs->curves), curvename) == 0) {
		    return;	/* curve name still exists */
	       }
	  }
     }

     /* curve does not exist in any graph, recycle the colour */
     xnfree(tree_getkey(g->curvecol));
     tree_rm(g->curvecol);
     itree_add(g->colunused, colindex, NULL);
}


