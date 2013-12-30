/*
 * Gtk multicurve, multichart, time-based graph widget
 * This is a wrapper over the gtkdatabox to manage the drawing of named
 * charts and curves with default colours, inside a given Gtk container box.
 *
 * Has a single call to draw a named curve on a named chart. Everything is
 * created if not in existance, charts being stacked vertically and the 
 * state is held in a graphset. Calls are available to remove individual 
 * curves, charts, or curves sharing the same name over all charts.
 * Colour, style and zooming calls are also availabe.
 * Time (on the x-axis only) is supported by setting a timebase mapping call
 * using unix datetime for min and max.
 *
 * Nigel Stuckey, September 1999, October 2010
 * Copyright System Garden Limited 1999-2001. All rights reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtkdatabox.h>
#include <gtkdatabox_lines.h>
#include <gtkdatabox_bars.h>
#include <gtkdatabox_points.h>
#include <gtkdatabox_markers.h>
#include "graphdbox.h"
#include "main.h"
#include "callbacks.h"
#include "../iiab/itree.h"
#include "../iiab/timeline.h"
#include "../iiab/util.h"
#include "../iiab/cf.h"
#include "../iiab/iiab.h"
#include "../iiab/elog.h"

char *graphdbox_colours[] = { "red", "green", "orange", "purple", "cyan", 
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
 * Create our graph set implemented with GtkDataBox and associate with 
 * a empty GtkVBox to be used as a container which is provided by the caller.
 * The vbox is empty at the begining.
 * Multiple graph sets are supported as each has a unique handle.
 */
GRAPHDBOX *graphdbox_create(GtkVBox *box)
{
     GRAPHDBOX *g;

     /* initialise super structure ready for graph creation */
     g = (GRAPHDBOX *) xnmalloc(sizeof(struct graphdbox_all));
     g->graphs = tree_create();
     g->start = 0;
     g->end = 0;
     g->curvecol = tree_create();
     g->colunused = itree_create();
     g->nextcol = 0;
     g->container = box;

     /* set fail-safe configuration if missing or out of range */
     if ( ! cf_defined(iiab_cf, GRAPHDBOX_SHOWRULERS_CFNAME) )
          cf_putint(iiab_cf, GRAPHDBOX_SHOWRULERS_CFNAME, 1);

     if ( ! cf_defined(iiab_cf, GRAPHDBOX_SHOWAXIS_CFNAME) )
          cf_putint(iiab_cf, GRAPHDBOX_SHOWAXIS_CFNAME, 1);

     if ( ! cf_defined(iiab_cf, GRAPHDBOX_DRAWSTYLE_CFNAME) ||
	  cf_getint(iiab_cf, GRAPHDBOX_DRAWSTYLE_CFNAME) < 0 ||
	  cf_getint(iiab_cf, GRAPHDBOX_DRAWSTYLE_CFNAME) >= GRAPHDBOX_GTYPE_EOL )
          cf_putint(iiab_cf, GRAPHDBOX_DRAWSTYLE_CFNAME, GRAPHDBOX_THINLINE);

     return g;
}


/* Remove the GtkDataBox widget and clear out all references held to it */
void graphdbox_destroy(GRAPHDBOX *g)
{
     struct graphdbox_graph *gs;
     struct graphdbox_curve *curve;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);

	  gtk_widget_destroy(gs->gdbox);
	  tree_traverse(gs->curves) {
	       curve = tree_get(gs->curves);
	       nfree(curve->X);
	       nfree(curve->Y);
	       nfree(curve);
	       nfree(tree_getkey(gs->curves));
	  }
	  tree_destroy(gs->curves);
	  if (gs->timeline)
	       timeline_free(gs->timeline);

	  nfree(tree_getkey(g->graphs));
	  nfree(gs);
     }
     tree_destroy(g->graphs);
     tree_clearout(g->curvecol, tree_infreemem, NULL);
     tree_destroy(g->curvecol);
     itree_destroy(g->colunused);
     nfree(g);
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
 * text         String array of labels to plot at the X & Y points defined
 *              above. Can be NULL for no text.
 * overwrite    If true the previous curve is replaced with the new one.
 *              If false the new data will be appended to the old data
 *              and the combined set is drawn. overwrite=0 not implemented yet.
 * NB: only overwrite=1 is supported at present.
 * Returns the colour of the rendered curve or NULL if there was a problem, 
 * including insufficient values to plot.
 */
GdkColor *graphdbox_draw(GRAPHDBOX *g, char *graph_name, char *curve_name, 
			 int nvals, float *xvals, float *yvals, char **text,
			 int overwrite)
{
     static GdkColor colour;
     char *gname;
     struct graphdbox_graph *gs;
     struct graphdbox_curve *mycurve;
     int r;

#if 0
     g_print("-- graphdbox_draw() - graph: %s, curve: %s, nvals: %d\n",
	     graph_name == NULL ? "SINGLETON" : graph_name, curve_name, nvals);

     int i;
     for (i=0; i<nvals; i++) {
       g_print("    %4d: t=%s, v=%f\n", i, util_decdatetime((time_t)xvals[i]), 
	       yvals[i]);
     }
#endif

     /* assert!! */
     if (nvals < 2) {
	  elog_printf(DIAG, "Can't draw curve %s %s: only has %d value",
		      graph_name ? graph_name : "(default)", curve_name,
		      nvals);
	  return NULL;
     }

     /* choose a default graph name if asked */
     if (graph_name)
	  gname = graph_name;
     else
	  gname = GRAPHDBOX_DEFGRAPHNAME;

     /* find the existing graph structure or create a new one */
     gs = tree_find(g->graphs, gname);
     if (gs == TREE_NOVAL)
          gs = graphdbox_newgraph(g, gname, 
				  cf_getint(iiab_cf, 
					    GRAPHDBOX_DRAWSTYLE_CFNAME) );

     /* have I plotted this before? if so, get the curve structure */
     if ( (mycurve = tree_find(gs->curves, curve_name)) != TREE_NOVAL ) {
	  if (overwrite) {
	       /* remove existing curve from widget only [graphdbox_rmcurve()
		* removes from widget and list] */
	       /*g_print("recycling curve %s (index %d)\n", curve_name, 
		 mycurve->index);*/
	       r = gtk_databox_graph_remove(GTK_DATABOX(gs->gdbox), 
					    mycurve->dbgraph);
	       if (r)
		    elog_printf(ERROR, "Error removing curve with "
				"gtk_databox_graph_remove()");
	       if (mycurve->X)
		    nfree(mycurve->X);
	       if (mycurve->Y)
		    nfree(mycurve->Y);
	       mycurve->X = xvals;	/* new values */
	       mycurve->Y = yvals;
	       mycurve->nvals = nvals;
	  } else {
	       g_print("should not be here!!\n");
	       elog_printf(ERROR, "extending curves not yet supported");
	       if (mycurve->X)
		    nfree(mycurve->X);
	       if (mycurve->Y)
		    nfree(mycurve->Y);
	       mycurve->X = xvals;	/* should we store these vals?? */
	       mycurve->Y = yvals;
	       mycurve->nvals = nvals;
	  }
     } else {
	  /* -- draw a new curve -- */

	  /* allocate the curve structure */
	  mycurve = nmalloc(sizeof(struct graphdbox_curve));
	  mycurve->X = xvals;
	  mycurve->Y = yvals;
	  mycurve->nvals = nvals;
	  mycurve->style = gs->style;

	  /* add curve to tree */
	  tree_add(gs->curves, xnstrdup(curve_name), mycurve);
     }

     /* select a colour */
     graphdbox_usecolour(g, curve_name, &colour);

     /* create new curve in the requested style */
     /* TODO: the graphstyle should properly become curve style */
     switch (mycurve->style) {
     case GRAPHDBOX_THINLINE:
          mycurve->dbgraph = gtk_databox_lines_new(nvals, xvals, yvals, 
						   &colour, 1);
	  break;
     case GRAPHDBOX_MIDLINE:
          mycurve->dbgraph = gtk_databox_lines_new(nvals, xvals, yvals, 
						   &colour, 2);
	  break;
     case GRAPHDBOX_FATLINE:
          mycurve->dbgraph = gtk_databox_lines_new(nvals, xvals, yvals, 
						   &colour, 3);
	  break;
     case GRAPHDBOX_POINT:
          mycurve->dbgraph = gtk_databox_points_new(nvals, xvals, yvals, 
						    &colour, 3);
	  break;
     case GRAPHDBOX_BAR:
          mycurve->dbgraph = gtk_databox_bars_new(nvals, xvals, yvals, 
						  &colour, 1);
	  break;
     case GRAPHDBOX_TEXT:
          mycurve->dbgraph = gtk_databox_markers_new(nvals, xvals, yvals, 
						     &colour, 1, GTK_DATABOX_MARKERS_TRIANGLE);
	  /* TODO: apply text with a loop of 
	     gtk_databox_markers_set_label() */
	  break;
     default:
          elog_die(FATAL, "Unhandled switch case %d", mycurve->style);
     }
     if ( ! mycurve->dbgraph )
          elog_printf(ERROR, "Unable to create new curve %s with gtk_databox, "
		      "style %d", curve_name, mycurve->style);
		      

     /* draw the curve on selected graph (lines object on a databox object) */
     r = gtk_databox_graph_add(GTK_DATABOX(gs->gdbox), mycurve->dbgraph);
     if (r)
          elog_printf(ERROR, "Unable to add curve %s with "
		      "gtk_databox_graph_add(), style %d", curve_name, 
		      mycurve->style);

     graphdbox_updateaxis(gs); /* do I want this ?? should updateing the axis not be a different call? */

     /*gtk_databox_redraw(GTK_DATABOX(gs->gdbox));*/
     /*g_print("curve %s allocated index %d\n", curve_name, mycurve->index);*/

     return &colour;
}


/* Create a new graph. If there is no default name, set it to graph_name.
 * Uses the globals show_rulers and show_axis to determin whether an
 * axis or rulers are to be drawn and view_histogram to set the default
 * graph style to line or bar.
 * Returns the address of the graph structure, although this has already
 * been added to the GRAPHDBOX lists (g->graphs) */
struct graphdbox_graph *graphdbox_newgraph(GRAPHDBOX *g, char *graph_name, 
					   enum graphdbox_graphtype type)
{
     struct graphdbox_graph *gs;
     char *gname, *imgpath;
     GtkWidget *box, *table, *image, *event_box;

     /* allocate name */
     gname = xnstrdup(graph_name);

     /* set up timeline, used in horizontal ruler */
     timeline_setoffset(g->start);

     /* allocate and initialise graph structure */
     gs = xnmalloc(sizeof(struct graphdbox_graph));
     tree_add(g->graphs, gname, gs);
     gs->curves = tree_create();
     gs->style  = type;
     gs->minmax = 0.0;		/* 0.0 is special 'ignore me' value */
     gs->parent = g;
     gs->timeline = NULL;

     /* create graph (chart+scrollbars held in a table) with positions
      * and add to containing widget */
     gtk_databox_create_box_with_scrollbars_and_rulers_positioned(&box, &table, 
					  TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);
     gtk_databox_ruler_set_linear_label_format(
			gtk_databox_get_ruler_y(GTK_DATABOX(box)), "%%-%dg");
     gtk_databox_ruler_set_text_hoffset(
			gtk_databox_get_ruler_y(GTK_DATABOX(box)), -1);

     gs->gdbox  = GTK_WIDGET(box);
     gs->gtable = GTK_WIDGET(table);
     gtk_box_pack_start (GTK_BOX(g->container), GTK_WIDGET(table),TRUE,TRUE,0);

     /* attach corner buttons */
     imgpath = util_strjoin(iiab_dir_lib, "/", GRAPHDBOX_LEFT_IMG, NULL);
     image = gtk_image_new_from_file(imgpath);
     nfree(imgpath);
     event_box = gtk_event_box_new();
     gtk_container_add(GTK_CONTAINER(event_box), image);
     gtk_widget_set_tooltip_text(event_box, "Show or hide choice panel");
     g_signal_connect(G_OBJECT(event_box), "button-press-event", 
		      G_CALLBACK(on_view_choices), NULL);
     gtk_table_attach(GTK_TABLE(table), event_box, 0, 1, 0, 1,
		      GTK_FILL, GTK_FILL, 0, 0);

     imgpath = util_strjoin(iiab_dir_lib, "/", GRAPHDBOX_UP_IMG, NULL);
     image = gtk_image_new_from_file(imgpath);
     nfree(imgpath);
     event_box = gtk_event_box_new();
     gtk_container_add(GTK_CONTAINER(event_box), image);
     gtk_widget_set_tooltip_text(event_box, "Show or hide toolbar panel");
     g_signal_connect(G_OBJECT(event_box), "button-press-event", 
		      G_CALLBACK(on_view_toolbar), NULL);
     gtk_table_attach(GTK_TABLE(table), event_box, 2, 3, 0, 1,
		      GTK_FILL, GTK_FILL, 0, 0);

     imgpath = util_strjoin(iiab_dir_lib, "/", GRAPHDBOX_RIGHT_IMG, NULL);
     image = gtk_image_new_from_file(imgpath);
     nfree(imgpath);
     event_box = gtk_event_box_new();
     gtk_container_add(GTK_CONTAINER(event_box), image);
     gtk_widget_set_tooltip_text(event_box, "Show or hide curve panel");
     g_signal_connect(G_OBJECT(event_box), "button-press-event", 
		      G_CALLBACK(on_view_curves), NULL);
     gtk_table_attach(GTK_TABLE(table), event_box, 2, 3, 2, 3,
		      GTK_FILL, GTK_FILL, 0, 0);

     imgpath = util_strjoin(iiab_dir_lib, "/", GRAPHDBOX_DOWN_IMG, NULL);
     image = gtk_image_new_from_file(imgpath);
     nfree(imgpath);
     event_box = gtk_event_box_new();
     gtk_container_add(GTK_CONTAINER(event_box), image);
     gtk_widget_set_tooltip_text(event_box, "Show or hide navigation panel");
     g_signal_connect(G_OBJECT(event_box), "button-press-event", 
		      G_CALLBACK(on_view_curves), NULL);
     gtk_table_attach(GTK_TABLE(table), event_box, 0, 1, 2, 3,
		      GTK_FILL, GTK_FILL, 0, 0);

     gtk_widget_show_all(GTK_WIDGET(g->container));

     return gs;
}



/* Returns true if the curve has been drawn and its data is being held */
int graphdbox_iscurvedrawn(GRAPHDBOX *g, char *graph_name, char *curve_name)
{
     if (graphdbox_lookupcurve(g, graph_name, curve_name))
	  return 1;
     else
	  return 0;
}


/* Return 1 if an individual graph has been zoomed or 0 otherwise */
int graphdbox_iszoomed(struct graphdbox_graph *gs) {
     GtkAdjustment *adjX, *adjY;

     adjX = gtk_databox_get_adjustment_x(GTK_DATABOX(gs->gdbox));
     adjY = gtk_databox_get_adjustment_y(GTK_DATABOX(gs->gdbox));

     if (gtk_adjustment_get_page_size(adjX) < 0.99 ||
	 gtk_adjustment_get_value(adjX)     > 0.01 ||
	 gtk_adjustment_get_page_size(adjY) < 0.99 ||
	 gtk_adjustment_get_value(adjY)     > 0.01)

	  return 1;
     else
	  return 0;
}

/*
 * Redraws the graph named graph_name to the screen. Used after
 * graphdbox_draw() for efficently drawing lots of curves then doing
 * a single update.
 */
void graphdbox_update(GRAPHDBOX *g, char *graph_name)
{
     char *gname;
     struct graphdbox_graph *gs;

     /* choose the graph name */
     if (graph_name)
	  gname = graph_name;
     else
	  gname = GRAPHDBOX_DEFGRAPHNAME;

     /* get the structure */
     gs = tree_find(g->graphs, gname);
     if (gs == TREE_NOVAL)
	  return;

     g_print("graphdbox_update() called, redraw disabled I dont know if I need ti any more\n");
     /* DO I NEED THIS ? gtk_databox_redraw(GTK_DATABOX(gs->gdbox));*/
}


/* set the timebase on the xaxis, such that the range min..max is shown
 * on the axis and arrange for it to be labeled suitably.
 * If min or max are -1 then do not change that value.
 */
void graphdbox_settimebase(GRAPHDBOX *g, time_t min, time_t max)
{
#if 0
     char min_str[UTIL_SHORTSTR], max_str[UTIL_SHORTSTR];

     strcpy(min_str, util_shortadaptdatetime(min));
     strcpy(max_str, util_shortadaptdatetime(max));

     elog_printf(DEBUG, "timebase from min %s (%ld) .. max %s (%ld)", 
		 min_str, min, max_str, max);
#endif

     if (min > -1)
	  g->start = min;
     if (max > -1)
	  g->end   = max;
}


/* Set the maximum value to be displayed on the y-axis of all the 
 * graphs currently in existence.
 * A value of 0.0 clears the effect
 */
void graphdbox_setallminmax(GRAPHDBOX *g, double value)
{
     struct graphdbox_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gs->minmax = value;
     }
}


/* update the graph range and set axis ticks but does not redraw; the
 * caller should call gtk_databox_redraw() when ready. Bypassed
 * if the graph is zoomed */
void graphdbox_updateaxis(struct graphdbox_graph *gs)
{
     gfloat minx, maxx, miny, maxy;
     gfloat v_minx, v_maxx, v_miny, v_maxy;
     int r;

     /* if in a zoomed state, we need to suspend to axis update */
     if (graphdbox_iszoomed(gs))
	  return;

     if (gs->parent->end) {
	  /* timebase has been set: set the timeline for correct 
	   * translation from 0..max to gs->parent->start..gs->parent->end */
	  timeline_setoffset(gs->parent->start);

	  /*gtk_databox_rescale(GTK_DATABOX(gs->gdbox));*/
	  r = gtk_databox_calculate_extrema (GTK_DATABOX(gs->gdbox), 
					     &minx, &maxx, &miny, &maxy);
	  if (r == -2) {
	       /* no datasets so put a default in */
	       minx = miny = 0.0;
	       maxx = maxy = 1.0;
	  } else if (r == -1) {
	       elog_printf(ERROR, "No valid gtk_databox on which to "
			   "calculate extrema");
	       return;
	  }
          /*g_print("calc: min (%f,%f) max (%f,%f)  --  ", 
	    minx, miny, maxx, maxy);*/

	  /* override time and max y value as configured */
	  maxx = (gs->parent->end - gs->parent->start);
	  if (gs->minmax != 0.0) {
	       if (maxy < gs->minmax)
		    maxy = gs->minmax;
	  }
	  if (maxy < 1.0) 
	       maxy = 1.0;

	  /* Keep this state for the x and y rulers */
	  v_minx = minx; 
	  v_maxx = maxx; 
	  v_miny = miny; 
	  v_maxy = maxy;

	  /* We use a margin on 5% on the start and end times for a
	   * better appearence and to prevent the impression of clipping */
	  minx = - ( maxx * .03 );
	  miny = - ( maxy * .05 );
	  maxy = maxy * 1.05;
	  maxx = maxx * 1.03;

	  /* g_print("fudged: min (%f,%f) max (%f,%f)\n", 
	     minx, miny, maxx, maxy); */

	  gtk_databox_set_total_limits(GTK_DATABOX(gs->gdbox), minx, maxx,
					 maxy, miny);
     } else {
	  /* timebase not set */
          gtk_databox_auto_rescale(GTK_DATABOX(gs->gdbox), 0.05);
     }

     /* Set the custom x-axis to be a timeline */
     ITREE *timetics;
     GtkDataboxRuler *ruler;
     gchar **labels;
     gfloat *tvals;
     int count, i;

     ruler = gtk_databox_get_ruler_x(GTK_DATABOX(gs->gdbox));
     timetics = timeline_calc(v_minx, v_maxx, v_maxx - v_minx, 8);
     count = itree_n(timetics);
     labels = xnmalloc(count * sizeof(char *));
     tvals = xnmalloc(count * sizeof(gfloat));
     i = 0;
     itree_traverse(timetics) {
       tvals[i] = itree_getkey(timetics);
       if (((struct timeline_tick *)itree_get(timetics))->label) {
	 labels[i] = ((struct timeline_tick *)itree_get(timetics))->label;
       } else {
	 labels[i] = "";
       }
       i++;
     }
     gtk_databox_ruler_set_manual_ticks(ruler, tvals);
     gtk_databox_ruler_set_manual_tick_cnt(ruler, count);
     gtk_databox_ruler_set_manual_tick_labels(ruler, labels);
     if (gs->timeline)
       timeline_free(gs->timeline);
     gs->timeline = timetics;

#if 0
     elog_printf(DEBUG, "axis updates on %s to min (%.2f,%.2f) "
		 "max (%.2f,%.2f)",
		 "unknown", min.x, min.y, max.x, max.y);
#endif
}


/* update the graph range and set axis ticks of all graphs */
void graphdbox_updateallaxis(GRAPHDBOX *g)
{
     struct graphdbox_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  graphdbox_updateaxis(gs);
     }
}

/* remove the curve from the given graph */
void graphdbox_rmcurve(GRAPHDBOX *g, char *graph_name, char *curve_name)
{
     struct graphdbox_graph *gs;
     struct graphdbox_curve *mycurve;
     int r;

     /* find graph */
     gs = graphdbox_lookupgraph(g, graph_name);
     if ( ! gs )
	  return;

     /* find curve in graph */
     if ( (mycurve = tree_find(gs->curves, curve_name)) != TREE_NOVAL)
     {
	  /* remove curve from widget and our list of drawn curves */
	  r= gtk_databox_graph_remove(GTK_DATABOX(gs->gdbox), mycurve->dbgraph);
	  if (r)
	       elog_printf(ERROR, "Error removing curve with "
			   "gtk_databox_graph_remove()");
	  nfree(mycurve->X);
	  nfree(mycurve->Y);
	  nfree(mycurve);
	  nfree(tree_getkey(gs->curves));
	  tree_rm(gs->curves);

	  /* give back the curve colour */
	  graphdbox_recyclecolour(g, curve_name);

	  /* redraw whole graph (all remaining curves) */
	  graphdbox_updateaxis(gs);
	  /*	  gtk_databox_redraw(GTK_DATABOX(gs->gdbox));*/
     }
}

/* remove the curve from all the graphs */
void graphdbox_rmcurveallgraphs(GRAPHDBOX *g, char *curve_name)
{
     /* iterate over graph, removing the curve from each graph */
     tree_traverse(g->graphs) {
          graphdbox_rmcurve(g, tree_getkey(g->graphs), curve_name);
     }
}


/* remove all the curves on the specified graph */
void graphdbox_rmallcurves(GRAPHDBOX *g, char *graph_name)
{
     struct graphdbox_graph *gs;
     struct graphdbox_curve *mycurve;
     char *mycurvekey;
     int r;

     /* find graph */
     gs = graphdbox_lookupgraph(g, graph_name);
     if ( ! gs ) {
          elog_printf(ERROR, "graph name does not exist: %s", graph_name);
	  return;
     }

     /* clear the widget of curves (graph in gtk-speak) */
     r = gtk_databox_graph_remove_all(GTK_DATABOX(gs->gdbox));
     if (r)
          elog_printf(ERROR, "Error removing all curves with "
		      "gtk_databox_graph_remove_all()");

     /* clear curve storage, written in a way that does not clash with
      * the recycle code */
     while ( ! tree_empty(gs->curves) ) {
	  /* point to storage and delete list entry */
	  tree_first(gs->curves);
	  mycurve    = tree_get(gs->curves);
	  mycurvekey = tree_getkey(gs->curves);
	  tree_rm(gs->curves);
	  /* recycle colour */
	  graphdbox_recyclecolour(g, mycurvekey);
	  /* free storage */
	  nfree(mycurve->X);
	  nfree(mycurve->Y);
	  nfree(mycurve);
	  nfree(mycurvekey);
     }
}

/* remove the specified graph */
void graphdbox_rmgraph(GRAPHDBOX *g, char *graph_name)
{
     struct graphdbox_graph *gs;
     struct graphdbox_curve *mycurve;
     int r;

     /* find graph */
     gs = graphdbox_lookupgraph(g, graph_name);
     if ( ! gs )
	  return;

     /* clear the widget of all curves (graphs in gtkdbox-speak) */
     r = gtk_databox_graph_remove_all(GTK_DATABOX(gs->gdbox));
     if (r)
          elog_printf(ERROR, "Error removing curve with "
		      "gtk_databox_graph_remove_all()");

     /* remove the table containing databox, rulers and corner buttons */
     gtk_widget_destroy(gs->gtable);

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
      * in the collective GRAPHDBOX structure */
     if (tree_empty(g->graphs)) {
	  tree_clearout(g->curvecol, tree_infreemem, NULL);
	  itree_clearout(g->colunused, NULL);
	  g->nextcol = 0;
     }
}

/* remove all graphs (and curves) but leave the uigraph structure standing */
void graphdbox_rmallgraphs(GRAPHDBOX *g)
{
     struct graphdbox_graph *gs;
     struct graphdbox_curve *mycurve;

     while ( ! tree_empty(g->graphs) ) {
          /* strategy is to clearup the first graph in the list 
	   * each iteration */
	  tree_first(g->graphs);
	  gs = tree_get(g->graphs);

	  /* remove the table containing databox, rulers and corner buttons */
	  gtk_widget_destroy(gs->gtable);

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

	  /* delete the current entry */
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
struct graphdbox_graph *graphdbox_lookupgraph(GRAPHDBOX *g, char *graph_name)
{
     char *gname;

     /* choose the graph name */
     if (graph_name)
	  gname = graph_name;
     else
	  gname = GRAPHDBOX_DEFGRAPHNAME;

     if ( ! gname)
	  return NULL;		/* no graphs created */

     /* find the graph */
     if (tree_find(g->graphs, gname) == TREE_NOVAL)
	  return NULL;		/* no such graph name */
     else
	  return tree_get(g->graphs);
}

/* looks up the named graph, returning NULL if no graph and curve exists or
 * a pointer to struct graphdbox_curve if found */
struct graphdbox_curve *graphdbox_lookupcurve(GRAPHDBOX *g, char *graph_name, 
					    char *curve_name)
{
     char *gname;
     struct graphdbox_graph *gs;

     /* choose the graph name */
     if (graph_name)
	  gname = graph_name;
     else
	  gname = GRAPHDBOX_DEFGRAPHNAME;

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


/* Zoom in to the middle of the graph's x-axis, leaving the 
 * y-axis alone. Zoomin factor is +ve magnification, typically 3.0 */
void graphdbox_allgraph_zoomin_x(GRAPHDBOX *g, double zoomin)
{
     struct graphdbox_graph *gs;
     gfloat left, right, top, bottom, width, offset;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_get_visible_limits(GTK_DATABOX(gs->gdbox), 
					 &left, &right, &top, &bottom);
	  width = right - left;
	  offset = (width - ( width / zoomin )) / 2;
	  gtk_databox_set_visible_limits(GTK_DATABOX(gs->gdbox), 
					 left + offset,
					 right - offset, 
					 top, 
					 bottom);
#if 0
	  elog_printf(DEBUG, "X zoom: factor=%f L=%2f R=%2f width=%2f "
		      "offset=%2f new_L=%2f new_R=%2f", 
		      zoomin, left, right, width, offset, 
		      left+offset, right-offset);
#endif
     }
}



/* Zoom in to the middle of the graph's y-axis, leaving the 
 * x-axis alone. Zoomin factor is +ve magnification, typically 3.0 */
void graphdbox_allgraph_zoomin_y(GRAPHDBOX *g, double zoomin)
{
     struct graphdbox_graph *gs;
     gfloat left, right, top, bottom, height, offset;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_get_visible_limits(GTK_DATABOX(gs->gdbox), 
					 &left, &right, &top, &bottom);
	  height = top - bottom;
	  offset = (height - ( height / zoomin )) / 2;
	  gtk_databox_set_visible_limits(GTK_DATABOX(gs->gdbox), 
					 left,
					 right,
					 top - offset, 
					 bottom + offset);
#if 0
	  elog_printf(DEBUG, "Y zoom: factor=%f T=%2f B=%2f height=%2f "
		      "offset=%2f new_T=%2f new_B=%2f", 
		      zoomin, top, bottom, height, offset,
		      top-offset, bottom+offset);
#endif
     }
}


/* Partially zoom out of all graphs */
void graphdbox_allgraph_zoomout(GRAPHDBOX *g)
{
     struct graphdbox_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_zoom_out( GTK_DATABOX(gs->gdbox) );
     }
}



/* Partially zoom out of all graphs */
void graphdbox_allgraph_zoomout_home(GRAPHDBOX *g)
{
     struct graphdbox_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_zoom_home( GTK_DATABOX(gs->gdbox) );
     }
}


/* return the major tick interval */
gdouble graphdbox_majticks(gdouble max)
{
     return 5.0;
}

/* return the minor tick interval */
gdouble graphdbox_minticks(gdouble max)
{
     return 1.0;
}

#if 0
/* Hide the axis of all graphs */
void graphdbox_hideallaxis(GRAPHDBOX *g)
{
     struct graphdbox_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_hide_cross( GTK_DATABOX(gs->gdbox) );
	  gtk_databox_redraw( GTK_DATABOX(gs->gdbox) );
     }
}

/* Show the axis of all graphs */
void graphdbox_showallaxis(GRAPHDBOX *g)
{
     struct graphdbox_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_show_cross( GTK_DATABOX(gs->gdbox) );
	  gtk_databox_redraw( GTK_DATABOX(gs->gdbox) );
     }
}

/* Hide the rulers of all graphs */
void graphdbox_hideallrulers(GRAPHDBOX *g)
{
     struct graphdbox_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_hide_rulers( GTK_DATABOX(gs->gdbox) );
	  gtk_databox_redraw( GTK_DATABOX(gs->gdbox) );
     }
}

/* Show the rulers of all graphs */
void graphdbox_showallrulers(GRAPHDBOX *g)
{
     struct graphdbox_graph *gs;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gtk_databox_show_rulers( GTK_DATABOX(gs->gdbox) );
	  gtk_databox_redraw( GTK_DATABOX(gs->gdbox) );
     }
}
#endif

#if 0
/* Change all graphs to the given style type */
void graphdbox_allgraphstyle(GRAPHDBOX *g, enum graphdbox_graphtype style)
{
     struct graphdbox_graph *gs;
     struct graphdbox_curve *curve;

     tree_traverse(g->graphs) {
	  gs = tree_get(g->graphs);
	  gs->style = style;
	  tree_traverse(gs->curves) {
	       curve = tree_get(gs->curves);
	       switch (style) {
	       case GRAPHDBOX_THINLINE:
		    gtk_databox_set_data_type( GTK_DATABOX(gs->gdbox), 
					       curve->index,
					       GTK_DATABOX_LINES, 1 );
		    break;
	       case GRAPHDBOX_FATLINE:
		    gtk_databox_set_data_type( GTK_DATABOX(gs->gdbox), 
					       curve->index,
					       GTK_DATABOX_LINES, 3 );
		    break;
	       case GRAPHDBOX_POINT:
		    gtk_databox_set_data_type( GTK_DATABOX(gs->gdbox), 
					       curve->index,
					       GTK_DATABOX_POINTS, 3 );
		    break;
	       case GRAPHDBOX_BAR:
		    gtk_databox_set_data_type( GTK_DATABOX(gs->gdbox), 
					       curve->index,
					       GTK_DATABOX_BARS, 3 );
		    break;
	       }
	  }
	  gtk_databox_redraw( GTK_DATABOX(gs->gdbox) );
     }
}
#endif

/* Change specified graph to the given style type */
void graphdbox_graphstyle(GRAPHDBOX *g, char *graph_name, 
			 enum graphdbox_graphtype style)
{
}


/*
 * Given a curve name, allocate a new colour if new or return a previously
 * used one if used before.
 */
void graphdbox_usecolour(GRAPHDBOX *g, char *curvename, GdkColor *col)
{
     char *colname;
     int colindex;

     colindex = (int) (long) tree_find(g->curvecol, curvename);
     if ((void *) (long) colindex == TREE_NOVAL) {
	  /* new curve, allocate a new colour */
	  if (itree_n(g->colunused) > 0) {
	       /* allocate one from the unused pile */
	       itree_first(g->colunused);
	       colindex = itree_getkey(g->colunused);
	       colname = graphdbox_colours[colindex];
	       itree_rm(g->colunused);
	  } else if (graphdbox_colours[g->nextcol] != NULL) {
	       /* allocate a 'virgin' colour */
	       colindex = g->nextcol++;
	       colname = graphdbox_colours[colindex];
	  } else {
	       /* we've run out of virgins !!! */
	       colindex = -1;
	       colname = "black";
	  }

	  tree_add(g->curvecol, xnstrdup(curvename), (void *) (long) colindex);
     } else {
	  /* existing curve */
	  if (colindex == -1)
	       colname = "black";
	  else
	       colname = graphdbox_colours[colindex];
     }

     /* parse into a GdkColor structre */
     gdk_color_parse( colname, col );
}



/*
 * Given a curve name, check the GRAPHDBOX structure to see if the curve
 * is used in any graphs. If not, then recycle the colour associated 
 * with the curve for later use [with graphdbox_usecolour()].
 * Returns nothing.
 */
void graphdbox_recyclecolour(GRAPHDBOX *g, char *curvename)
{
     struct graphdbox_graph *gs;
     int colindex;

     colindex = (int) (long) tree_find(g->curvecol, curvename);
     if ((void *) (long) colindex == TREE_NOVAL)
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

/*
 * Dump the entire contents of the graph data
 */
void graphdbox_dump(GRAPHDBOX *g)
{
    /* General summary */
    g_print("graphdbox_dump() - %d graphs: ", tree_n(g->graphs)); 
    tree_traverse(g->graphs)
	g_printf("%s ", tree_getkey(g->graphs)); 
    g_print("\n    Timebase: %s (%lu) ", util_decdatetime(g->start),
            g->start);
    g_print("to %s (%lu) diff %lu\n", util_decdatetime (g->end), g->end, 
            g->end - g->start);

    /* Detail */
    tree_traverse(g->graphs) {
	struct graphdbox_graph *graph;
	graph = (struct graphdbox_graph *) tree_get(g->graphs);
	g_printf("    Graph: %s, Curves: %d, Style %d, Minmax: %f\n", 
	         tree_getkey (g->graphs), tree_n(graph->curves),
	         graph->style, graph->minmax);
	tree_traverse(graph->curves) {
	    struct graphdbox_curve *curve;
	    int i;
	    curve = (struct graphdbox_curve *) tree_get(graph->curves);
	    g_printf("     %s,%s ", tree_getkey(g->graphs), tree_getkey(graph->curves));
	    g_printf("Colour: %d, NValues: %d -- ", curve->colour, curve->nvals);
	    for (i=0; i < curve->nvals; i++) 
		g_printf("(%d:%f,%f) ", i, curve->X[i], curve->Y[i]);
	    g_printf("\n");
	}
    }

    /* Colours */
    g_print("    Colours %d: ", tree_n(g->curvecol)); 
    tree_traverse(g->curvecol)
	g_printf("%s ", tree_getkey(g->curvecol)); 
    g_print("\n    Recycled colours %d: ", itree_n(g->colunused)); 
    itree_traverse(g->colunused)
	g_printf("%s ", itree_get(g->colunused)); 
    g_print(". Next colour %d\n", g->nextcol); 
}

