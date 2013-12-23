/*
 * Gtk multicurve graph widget
 * This is a wrapper over the gtkplot for convenience
 *
 * Nigel Stuckey, September 1999
 * Copyright System Garden Limited 1999-2001. All rights reserved.
 */

#define _ISOC99_SOURCE

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtkdatabox.h>
#include <gtkdatabox_lines.h>
#include <gtkdatabox_bars.h>
#include <gtkdatabox_points.h>
#include "graphdbox.h"
#include "uigraph.h"
#include "gconv.h"
#include "main.h"
#include "../iiab/itree.h"
#include "../iiab/cf.h"
#include "../iiab/iiab.h"

GRAPHDBOX *   uigraph_graphset;		/* The graphset from graphdbox; 
					 * we only need one set */

/* Singleton list of instances. Lists are held in the TREE's keys using
 * nmalloc()ed storage which must be managed by uigraph. */
TREE *        uigraph_inst_avail;       /* Available instances extracted from
					 * the current data's keys, set by 
					 * uigraph_inst_load(). List in keys */
TREE *        uigraph_inst_hint;	/* Instance selection hints, from 
					 * previous choices or prefs in .habrc.
					 * Used to draw the initial view, 
					 * matching data instance names.
					 * List held in keys */
TREE *        uigraph_inst_drawn;	/* Selected/drawn list of instances  */

/* Singleton list of curves. Lists are held in the TREE's keys using
 * nmalloc()ed storage which must be managed by uigraph. */
TREE *        uigraph_curves_hint;	/* Curve selection hints, used to
					 * draw initial graphs. Matching data
					 * curve names are selected, typically
					 * from .habrc persistance */
TREE *        uigraph_curves_drawn;	/* Selected/drawn list of curves */

/* Data extracted from current data table. nmalloc()ed storage which must 
 * be managed by uigraph */
char *        uigraph_keycol;		/* Current key column from data */
TABLE         uigraph_datatab;		/* Reference to data */
time_t        uigraph_oldest;		/* Zoom: oldest visible time */
time_t        uigraph_youngest;		/* Zoom: youngest visible time */

/*
 * Graph UI logic, taking GUI layout+events and calls the graphdbox class
 * to implement a multi graph, multi curve, time-based chart set.
 *
 * Initialise the structures for graph visualisation
 */ 
void uigraph_init() {
     ITREE *list;
     GtkWidget *box;

     box = get_widget("graph_vbox");
     uigraph_graphset     = graphdbox_create(GTK_VBOX(box));
     uigraph_inst_hint    = tree_create();
     uigraph_inst_avail   = tree_create();
     uigraph_inst_drawn   = tree_create();
     uigraph_curves_hint  = tree_create();
     uigraph_curves_drawn = tree_create();
     uigraph_datatab      = NULL;
     uigraph_oldest       = 0;
     uigraph_youngest     = 0;

     /* load the default curves from the config */
     if (cf_defined(iiab_cf, DEFAULT_CURVES_CFNAME)) {
          /* get the session information from the config */
          list = cf_getvec(iiab_cf, DEFAULT_CURVES_CFNAME);
	  if (list) 
	       /* list of choices */
	       uigraph_set_curve_hint(list);
	  else
	       /* single string */
	       /*tree_add(uigraph_curvesel, 
			xnstrdup(cf_getstr(iiab_cf, DEFAULT_CURVES_CFNAME)),
			NULL);*/
	       ;
     }
}


void uigraph_fini() {
     graphdbox_destroy(uigraph_graphset);
     tree_clearout(uigraph_inst_hint, tree_infreemem, NULL);
     tree_clearout(uigraph_curves_hint, tree_infreemem, NULL);
     tree_clearout(uigraph_inst_avail, tree_infreemem, NULL);
     tree_clearout(uigraph_curves_drawn, tree_infreemem, NULL);
     tree_clearout(uigraph_inst_drawn, tree_infreemem, NULL);
     tree_destroy (uigraph_inst_hint);
     tree_destroy (uigraph_inst_avail);
     tree_destroy (uigraph_inst_drawn);
     tree_destroy (uigraph_curves_hint);
     tree_destroy (uigraph_curves_drawn);
}



/* The glade GUI defines the following layout for the graph pane:-
 *                               (1)
 *       +------------------------+-------------------+
 *       |                        | inst_list         |
 *       |                        +-----(3)-----------+
 *       +---------(2)------------+ curves_list       |
 *       |                        |                   |
 *       +------------------------+-------------------+
 *
 * (1) is graph_divider
 * (2) is graph_vbox
 * (3) is graphctl_divider
 * Then there are two GtkTreeLists and their model storage
 *   inst_list   - inst_liststore   - Holds instances
 *   curves_list - curves_liststore - Holds the curves/attributes
 * The vbox is empty at the begining and contains only one pane, ready to
 * hold one graph; as additional graphs are added, more panes are greated 
 * to hold them
 */

/* Append items in the list to the instance hint list.
 * Anything in the list will be autoselected by string key, causing a 
 * graph to be displayed.
 * The keys contain the list elements, caller is responsible for releasing 
 * argument (inst) memory */
void uigraph_set_inst_hint(ITREE *inst)
{
     itree_traverse(inst) {
          if (tree_find(uigraph_inst_hint, itree_get(inst)) == TREE_NOVAL)
	       tree_add(uigraph_inst_hint, xnstrdup(itree_get(inst)), NULL);
     }
}

/* Append items in the list to the hint list.
 * Anything in the list will be autoselected by string key, causing curves to
 * be drawn in all the instances.
 * The keys contain the list elements, caller is responsible for releasing 
 * argument (inst) memory */
void uigraph_set_curve_hint(ITREE *curves)
{
     itree_traverse(curves) {
          if (tree_find(uigraph_curves_hint, itree_get(curves)) == TREE_NOVAL)
	       tree_add(uigraph_curves_hint,xnstrdup(itree_get(curves)),NULL);
     }
}

/* ---- primary public interfaces ---- */

/* Set the data to be used uigraph. 
 * Initialises the instance and curve lists and graphs, arranges for 
 * sensible defaults based on the hint lists.
 * To display, call uigraph_settimebase() to set displayed times, then
 * uigraph_draw_all_selected() to draw all the defaults.
 *
 * No data from tab is needed by the graphing code and its management 
 * stays with the caller */
void uigraph_data_load(TABLE tab)
{
     /* clear data before we go any further */
     uigraph_data_unload();

     /* assign new data */
     uigraph_datatab = tab;
     /*if (table_nrows(tab) == 1)
       elog_printf(INFO, "Unable to plot a single sample in a chart");*/
     uigraph_inst_load();
     uigraph_curve_load();
}

/* Set the graph timebase to display from oldest to youngest on the X axis
 * (pre zooming). A value of 0 in the data's _time column will correspond
 * to oldest and be displayed on the left hand edge */
void uigraph_set_timebase(time_t oldest, time_t youngest)
{
     uigraph_oldest = oldest;
     uigraph_youngest = youngest;
     graphdbox_settimebase(uigraph_graphset, oldest, youngest);
}

/* Update and redraw the existing data table with new points and preserve the
 * contextual state (zoom, positions, selections etc).
 * Whilst the data may change entirely, the instances and attributes are 
 * assumed to to be the same, which saves time by avoiding list updates
 * of data. No other functions need to be called to cause a redraw.
 *
 * No data from tab is needed by the graphing code and its management 
 * stays with the caller */
void uigraph_data_update_redraw(TABLE tab)
{
     /* assign updated data: we will expect appends and removals from the
      * front. We don't expect the columns to change */
     uigraph_datatab = tab;
 
#if 0
     /* Dont change the view or reset the time base, just walk over the
      * new instances and curves fo the new data, seeing if each is plotted
      * then replace the ones that are */
     /* TODO: REDRAW DRAW */
uigraph_drawcurve
     while instnace
       while curve
	 if (graphdbox_lookupcurve(g, graphname, curvename)) {
	       nvals = gconv_table2arrays(uigraph_graphset, uigraph_datatab, 
					  uigraph_oldest, uigraph_youngest,
					  colname, 
					  uigraph_keycol, instance,
					  &xvals, &yvals);
	       if (nvals <= 1)
		 return;

	       /* scale if needed */
	       if (scale != 1.0 || offset > 0.0)
		    for (i=0; i < nvals; i++)
			 yvals[i] = scale * yvals[i] + offset;

	       /* now draw the replacement and ignore colour */
	       colour = graphdbox_draw(uigraph_graphset, instance, colname, 
				       nvals, xvals, yvals, NULL, 1);

	       /* find new max from this curve */
	       if (possmax > max)
		    max = possmax;
	     }
     graphdbox_setallminmax(uigraph_graphset, max);
#endif

}

/* Clear the reference to the data, the instance and curve lists.
 * Further redraws will display blanks without setting new data. */
void uigraph_data_unload()
{
     uigraph_inst_unload();
     uigraph_curve_unload();
     uigraph_datatab = NULL;
}


/*
 * Draw the currently selected curves in the given instance, where data is 
 * available.
 * Reads the Gtk UI (curves_treestore tree model) as it contains scaling 
 * information and will update with the returned colour from graphdbox.
 * Load the instances with uigraph_inst_load() before calling, as they are 
 * needed to be identify.
 */
void uigraph_drawgraph(char *instance	/* graph name (instance) */ )
{
     float *xvals, *yvals, scale, offset, possmax, max=0.0;
     char *colname, *label;
     int nvals, i, active;
     GtkListStore *list;
     GtkTreeIter iter;
     GdkColor *colour;

     /* Iterate over the storage model, extracting offset, gradient, name 
      * and active status */
     list = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						  "curves_liststore") );
     if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter) == FALSE)
          return;
     do {
	  gtk_tree_model_get(GTK_TREE_MODEL (list),     &iter, 
			     UIGRAPH_CURVE_COLNAME,     &colname,
			     UIGRAPH_CURVE_LABEL,       &label,
			     UIGRAPH_CURVE_ACTIVE,      &active,
			     UIGRAPH_CURVE_SCALE,       &scale,
			     UIGRAPH_CURVE_OFFSET,      &offset,
			     UIGRAPH_CURVE_POSSIBLEMAX, &possmax,
			     -1);

	  /* if curve is active, get curve data and convert to doubles */
#if 0
     g_print("-- uigraph_drawgraph() - uigraph_oldest %d, uigraph_youngest %d\n"
             "diff %d, colname: %s, active: %d\n",
	     uigraph_oldest, uigraph_youngest, uigraph_youngest-uigraph_oldest,
     	     colname, active);
#endif
    
	  if (active) {
	       nvals = gconv_table2arrays(uigraph_graphset, uigraph_datatab, 
					  uigraph_oldest, uigraph_youngest,
					  colname, 
					  uigraph_keycol, instance,
					  &xvals, &yvals);
	       if (nvals <= 1)
		 return;

	       /* scale if needed */
	       if (scale != 1.0 || offset > 0.0)
		    for (i=0; i < nvals; i++)
			 yvals[i] = scale * yvals[i] + offset;

	       /* now draw */
	       colour = graphdbox_draw(uigraph_graphset, instance, colname, 
				       nvals, xvals, yvals, NULL, 1);

	       /* find new max from this curve */
	       if (possmax > max)
		    max = possmax;

	       /* patch colour as necessary */
	       gtk_list_store_set(list, &iter, 
		 		  UIGRAPH_CURVE_COLOUR, colour,
		 		  -1);
	  }

	  /* clear up */
	  g_free(colname);
	  g_free(label);
     } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter) == TRUE);

     /* find and set new max of all graphs */
     graphdbox_setallminmax(uigraph_graphset, max);
     graphdbox_updateallaxis(uigraph_graphset);
}


/*
 * Remove the named graphs from display; dont touch the instance list though
 */
void uigraph_rm_graph(char *instance)
{
     /* remove the graph */
     graphdbox_rmgraph(uigraph_graphset, instance);
}


/*
 * Remove all graphs from display. Leaves the container empty but the
 * curve and instance lists will remain. Items in the instance list will 
 * be deselected.
 */
void uigraph_rm_all_graphs()
{
     graphdbox_rmallgraphs(uigraph_graphset);
}


/*
 * Draw a curve in one or more graphs, scaling if required. 
 * Returns the colour assigned by graphdbox or NULL for error.
 */
GdkColor *uigraph_drawcurve(char *curve,	/* curve name */
			    double scale,	/* curve scale/magnitude */
			    double offset	/* y-axis offset */ )
{
     float *xvals, *yvals;
     int nvals, i;
     GdkColor *colour = NULL;

#if 0
     /* debugging code block to show the arguments and table contents */
     elog_printf(DEBUG, "drawing %s, %d elements, scale %f, offset %f", 
		 curve, table_nrows(uigraph_datatab), scale, offset);
#endif

     if (uigraph_inst_avail && tree_n(uigraph_inst_avail) > 0) {
	  /* multiple instances: multiple graphs */
	  tree_traverse(uigraph_inst_avail) {
	       if ( ! tree_present(uigraph_inst_hint, 
				   tree_getkey(uigraph_inst_avail)) )
		    continue; /* dont draw if instance not selected */

	       nvals = gconv_table2arrays(uigraph_graphset, uigraph_datatab, 
					  uigraph_oldest, uigraph_youngest,
					  curve, uigraph_keycol, 
					  tree_getkey(uigraph_inst_avail), 
					  &xvals, &yvals);

	       if (nvals <= 1)
		    return NULL; /* if no values, stop drawing altogether */

	       /* scale and offset value: y=mx+c */
	       if (scale != 1.0 || offset != 0.0)
		    for (i=0; i < nvals; i++)
			 yvals[i] = scale * yvals[i] + offset;

	       colour = graphdbox_draw(uigraph_graphset, 
				       tree_getkey(uigraph_inst_avail), 
				       curve, nvals, xvals, yvals, NULL,
				       1 /*overwrite*/ );
	  }
     } else {
	  /* single, default instance */
	  nvals = gconv_table2arrays(uigraph_graphset, uigraph_datatab, 
				     uigraph_oldest, uigraph_youngest,
				     curve, NULL, NULL,
				     &xvals, &yvals);
	  if (nvals <= 1)
	       return NULL; /* if no values, dont draw */

	  /* scale and offset value: y=mx+c */
	  if (scale != 1.0 || offset != 0.0)
	       for (i=0; i < nvals; i++)
		    yvals[i] = scale * yvals[i] + offset;

	  colour = graphdbox_draw(uigraph_graphset, NULL, curve, nvals,
				  xvals, yvals, NULL, 1 /*overwrite*/ );
	  
     }

     graphdbox_updateallaxis(uigraph_graphset);
     return colour;
}



/*
 * Draws everything that has been selected, typically used at startup.
 * Iterates over the selected instances and triggers uigraph_drawcurve()
 * on each. Positions to the first active curve in the list.
 */
void uigraph_draw_all_selected() {
     if (uigraph_inst_avail && tree_n(uigraph_inst_avail) > 0) {
	  /* multiple instances: multiple graphs */
	  tree_traverse(uigraph_inst_avail) {
	       /* uigraph_inst_hint hold historic names as well as 
		* current, uigraph_inst_avail are graphs that can currently 
		* potentially be selected. We need a union of both 
		* structures to work out what needs to be refreshed */
	       if (tree_find(uigraph_inst_hint, 
			     tree_getkey(uigraph_inst_avail)) == TREE_NOVAL)
		    continue;
	       uigraph_drawgraph(tree_getkey(uigraph_inst_hint));
	  }
     } else {
	  /* single, default instance */
          uigraph_drawgraph(NULL);
       /*	  uigraph_update(NULL);*/
     }

     uigraph_curve_scroll_to_active();

     /*graphdbox_dump(uigraph_graphset);*/ /* Debug */
}


/* callback for horizontal zoom-in button, which updates the graph */
G_MODULE_EXPORT void
uigraph_on_zoom_in_horiz (GtkButton      *button,
			  gpointer      user_data)
{
     graphdbox_allgraph_zoomin_x(uigraph_graphset, 3);
}

/* callback for vertical zoom-in button, which updates the graph */
G_MODULE_EXPORT void
uigraph_on_zoom_in_vert (GtkButton      *button,
			 gpointer      user_data)
{
     graphdbox_allgraph_zoomin_y(uigraph_graphset, 3);
}

/* callback for incremental zoom-out button, which updates the graph */
G_MODULE_EXPORT void
uigraph_on_zoom_out (GtkButton      *button,
		     gpointer      user_data)
{
     graphdbox_allgraph_zoomout(uigraph_graphset);
}

/* callback for zoom-out to home button, which updates the graph */
G_MODULE_EXPORT void
uigraph_on_zoom_out_home (GtkButton      *button,
			  gpointer      user_data)
{
     graphdbox_allgraph_zoomout_home(uigraph_graphset);
}


/* Return 1 if the graph has been zoomed or 0 otherwise */
int uigraph_iszoomed()
{
    struct graphdbox_graph *firstgraph;
    
    /* We just look at the first graph as a proxy for the rest */
    tree_first(uigraph_graphset->graphs);
    firstgraph = tree_get(uigraph_graphset->graphs);
    return graphdbox_iszoomed(firstgraph);
}


/* ---- generally private functions ---- */

/* Add instances from the data table into the instance model.
 * Initialises uigraph_keycol lexical var needed by uigraph_curve_load.
 * If there are zero or one instances, then the instance pane will be hidden
 * and the divider moved to 0. If there are two or more instaces, then the
 * panes are shown and the divered is positioned.
*/
void uigraph_inst_load() {
     /* Instances are defined by info row cells in a TABLE being numeric,
      * '1' (in ascii) being the primary key, '2' secondary, etc.
      * Adds data as a set of widgets into the data instance model
      * and sets uigraph_inst_poss */

     TREE *hd, *colvals;
     char *keyval=NULL;

     /* Find the primary key column and record it in uigraph_keycol (char *).
      * Then compile the instances and store in the list uigraph_inst_avail (TREE).
      */
     hd = table_getheader(uigraph_datatab);
     colvals = tree_create();
     tree_traverse(hd) {
       keyval = table_getinfocell(uigraph_datatab, "key", tree_getkey(hd));
	  if (keyval && *keyval == '1') {
	       /* found the primary key column, now get the
		* unique key values which represent the 
		* instances on the GUI */
	       uigraph_keycol = tree_getkey(hd);
	       table_uniqcolvals(uigraph_datatab, uigraph_keycol, &colvals);

	       /* duplicate with own managed memory */
	       tree_traverse(colvals)
		    if ( ! tree_present(uigraph_inst_avail, 
					tree_getkey(colvals)) )
		         tree_add(uigraph_inst_avail, 
				  xnstrdup(tree_getkey(colvals)), NULL);
	       break;	/* only key the first match */
	  }
     }
     tree_destroy(colvals);

     if (tree_n(uigraph_inst_avail) >= 1) {

	  /* --- we have keys: multi-instance data --- */
          /* Add the instances to the Gtk list model of the 
	   * instance treelist */
          GtkListStore *list;
	  GtkTreeIter iter;
	  gboolean active;

	  list = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						       "inst_liststore") );

	  tree_traverse(uigraph_inst_avail) {
	       /* set active if selected, and THEN setup the callback signal.
		* We only want INSTANCES to draw graphs as they will
		* draw on all selected graphs anyway. If we draw here there
		* would be a loop! */
	       if (tree_present(uigraph_inst_drawn, 
				tree_getkey(uigraph_inst_avail))) {
		    active = 1;
	       } else if (tree_present(uigraph_inst_hint, 
				       tree_getkey(uigraph_inst_avail))) {
		    tree_add(uigraph_inst_drawn, 
			     xnstrdup(tree_getkey(uigraph_inst_avail)), NULL);
		    active = 1;
	       } else {
		    active = 0;
	       }

	       /* assign to list model */
	       gtk_list_store_append(list, &iter);
	       gtk_list_store_set(list, &iter, 
			UIGRAPH_INST_INSTNAME, tree_getkey(uigraph_inst_avail),
			UIGRAPH_INST_LABEL,    tree_getkey(uigraph_inst_avail),
			UIGRAPH_INST_TOOLTIP,  tree_getkey(uigraph_inst_avail),
			UIGRAPH_INST_ACTIVE,   active,
				  -1);
	  }

	  /* If nothing has been selected, then pick the first suitable column
	   * from the instance list as a default */
	  if ( tree_n(uigraph_inst_drawn) == 0 ) {
	       GtkTreePath *path;
	       char *label;

	       /* pick the first row in the list model */
	       path = gtk_tree_path_new_from_string ("1");
	       if ( ! gtk_tree_model_get_iter (GTK_TREE_MODEL (list), &iter, 
					       path) ) {
		    elog_die(FATAL, "unable to get iterator");
	       }
	       gtk_list_store_set(list, &iter, 
				  UIGRAPH_INST_ACTIVE, 1,
				  -1);
	       gtk_tree_model_get(GTK_TREE_MODEL(list), &iter,
				  UIGRAPH_INST_LABEL, &label,
				  UIGRAPH_INST_ACTIVE, &active,
				  -1);
	       gtk_tree_path_free (path);

	       /* add new graph name to drawn list and hint for next time */
	       if ( ! tree_present(uigraph_inst_hint, label) )
		    tree_add(uigraph_inst_hint, xnstrdup(label), NULL);
	       tree_add(uigraph_inst_drawn, xnstrdup(label), NULL);

	       /* clear up */
	       /*gtk_tree_path_free (path);*/
	       g_free(label);
	  }
     }

     /* show & position or hide the instance pane */
     GtkWidget *inst_pane, *attr_pane, *ctl_divider;
     inst_pane = get_widget("inst_list");
     attr_pane = get_widget("curves_list");
     ctl_divider = get_widget("graphctl_divider");

     if (tree_n(uigraph_inst_avail) >= 1) {
          /* 2 or more instances - show and position */
          /* TODO GET WIDTH SO WE CAN MEASURE FROM THE RIGHT */
	  gtk_paned_set_position(GTK_PANED(ctl_divider), 100);
     } else {
          /* 0 or 1 instances - hide */
	  gtk_paned_set_position(GTK_PANED(ctl_divider), 0);
     }
}



/* Empty the instance list from its Gtk list model and the associated widgets */
void uigraph_inst_unload()
{
     /* empty gtk list */
     GtkListStore *list;
     list = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						  "inst_liststore") );
     gtk_list_store_clear(list);

     /* remove items from data and drawn tree, freeing key data */
     tree_clearout(uigraph_inst_drawn, tree_infreemem, NULL);
     tree_clearout(uigraph_inst_avail, tree_infreemem, NULL);
}



/*
 * Callback when an instance button has been clicked, which will cause a
 * new graph to be displayed or an existing one to be removed
 */
G_MODULE_EXPORT void
uigraph_on_inst_toggled (GtkCellRendererToggle *widget,
			 gchar                 *path_string,
			 gpointer               user_data)
{
     int active;
     gchar *label;
     GtkTreeIter iter;
     GtkTreePath *path;
     GtkListStore *list;

     /* bring in the name and active state from the model */
     list = GTK_LIST_STORE(gtk_builder_get_object(gui_builder, 
						  "inst_liststore"));
     path = gtk_tree_path_new_from_string(path_string);
     gtk_tree_model_get_iter (GTK_TREE_MODEL(list), &iter, path);
     gtk_tree_model_get(GTK_TREE_MODEL(list), &iter, 
			UIGRAPH_INST_LABEL,   &label,
			UIGRAPH_INST_ACTIVE,  &active,
			-1);

     if (!active) {
	  /* add new graph name to list */
	  tree_add(uigraph_inst_hint, xnstrdup(label), NULL);
	  tree_add(uigraph_inst_drawn, xnstrdup(label), NULL);

	  /* draw new graph */
	  uigraph_drawgraph(label);

	  gtk_list_store_set(list, &iter, 
			     UIGRAPH_INST_ACTIVE, 1,
			     -1);
     } else {
	  /* remove graph name from lists: current and long term */
          if (tree_find(uigraph_inst_hint, (char *) label) != TREE_NOVAL) {
	       nfree(tree_getkey(uigraph_inst_hint));
	       tree_rm(uigraph_inst_hint);
	  }

	  if (tree_find(uigraph_inst_drawn, (char *) label) != TREE_NOVAL) {
	       nfree(tree_getkey(uigraph_inst_drawn));
	       tree_rm(uigraph_inst_drawn);
	  }

	  /* remove instance graph */
	  uigraph_rm_graph(label);

	  gtk_list_store_set(list, &iter, 
			     UIGRAPH_INST_ACTIVE, 0,
			     -1);
     }

     /* clear up */
     gtk_tree_path_free (path);
     g_free(label);
}


/* 
 * Create a graph attribute selection list by adding to the attribute 
 * list model. Each line should draw a curve on the graph and will 
 * contain a button with callback to trigger.
 * uigraph_inst_load() should be called first to set up the graph list 
 * (if any) for data instances and the graph GRAPHDBOX widget graph should 
 * have been initialised before calling.
 *
 * The list uigraph_curves_hint is checked for previously 
 * selected curves and the appropriate lines are set to `on'.
 * If the list is empty, a default curve is chosen, set to `on',
 * and this creates an entry in uigraph_curves_hint.
 * Curves that have been selected are drawn on the graphs named in 
 * union of uigraph_inst_hint AND uigraph_inst_avail (selected current 
 * instances which are set up by gtkaction_mkgraphinst)
 * or the default graph if single instance.
 */
void uigraph_curve_load()
{
     GtkListStore *list;
     GtkTreeIter iter;
     char *info, *tooltip, *label;
     TREE *hd;
     gboolean active;
     double max;
     int r;

     list = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						  "curves_liststore") );
     hd = table_getheader(uigraph_datatab);
     tree_traverse(hd) {
          /* skip cols that are keys if defined */
          if (uigraph_keycol && 
	      strcmp(uigraph_keycol, tree_getkey(hd)) == 0) {
		    continue;
	  }

          /* skip cols that have names that begin with '_' */
	  if (*tree_getkey(hd) == '_') {
	       continue;
	  }

	  /* compile the tooltip */
	  info = table_getinfocell(uigraph_datatab, "info", tree_getkey(hd));
	  if (info)
	       tooltip = info;
	  else
	       tooltip = "";

	  /* get the max */
	  info = table_getinfocell(uigraph_datatab, "max", tree_getkey(hd));
	  if (info)
	       max = strtof(info, (char **)NULL);
	  else
	       max = 0.0;

	  /* find the label, which is the column name unless there is
	   * a 'name' info cell for this column */
	  label = table_getinfocell(uigraph_datatab, "name", tree_getkey(hd));
	  if (label && *label && strcmp(label, "0") != 0 
	      && strcmp(label, "-") != 0) {
	       /* leave label alone, we have it correctly */
	  } else {
	       /* label is column nmae */
	       label = tree_getkey(hd);
	  }

	  /* set active if selected and THEN setup the callback signal.
	   * We only want ATTRIBUTES to draw curves as they will
	   * draw on all selected graphs anyway. If we draw here there
	   * would be a loop! */
	  if (tree_present(uigraph_curves_drawn, tree_getkey(hd))) {
	       active = 1;
	  } else if (tree_present(uigraph_curves_hint, tree_getkey(hd))) {
	       tree_add(uigraph_curves_drawn, xnstrdup(tree_getkey(hd)), NULL);
	       active = 1;
	  } else {
	       active = 0;
	  }

	  /* assign to list model: label, tooltip, id */
	  gtk_list_store_append(list, &iter);
	  gtk_list_store_set(list, &iter, 
			     UIGRAPH_CURVE_TOOLTIP,     tooltip,
			     UIGRAPH_CURVE_COLNAME,     tree_getkey(hd),
			     UIGRAPH_CURVE_ACTIVE,      active,
			     UIGRAPH_CURVE_LABEL,       label,
			     UIGRAPH_CURVE_SCALE,       1.0,
			     UIGRAPH_CURVE_OFFSET,      0.0,
			     UIGRAPH_CURVE_POSSIBLEMAX, max,
			     -1);
     }

     /* If nothing has been selected, then pick the first suitable column
      * from the curve list as a default */
     if ( tree_n(uigraph_curves_drawn) == 0 ) {
          GtkTreePath *path;

          /* pick the first row in the list model */
          path = gtk_tree_path_new_from_string ("1");
	  r = gtk_tree_model_get_iter (GTK_TREE_MODEL (list), &iter, path);
	  if (r) {
	       gtk_list_store_set(list, &iter, 
				  UIGRAPH_CURVE_ACTIVE, 1,
				  -1);
	  } else {
	    /*		 WHY WHY elog_die(FATAL, "unable to get iterator");*/
	  }
	  gtk_tree_path_free (path);
     }
}


/* Scroll the curve list to the first active row */
void uigraph_curve_scroll_to_active()
{
     GtkListStore *list;
     GtkTreeView *view;
     GtkTreePath *path;
     GtkTreeIter iter;
     int active;

     /* get refs to the choice liststore and its view */
     list = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						  "curves_liststore") );
     view = (GtkTreeView *) get_widget("curves_list");

     /* get first active item in the choice list */
     if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter) == FALSE)
          return;
     do {
	  gtk_tree_model_get(GTK_TREE_MODEL (list),     &iter, 
			     UIGRAPH_CURVE_ACTIVE,      &active,
			     -1);

	  /* if curve is active, select */
	  if (active) {
	       /* get path from storage row as described by iter */
	       path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &iter);

	       /* now scroll */
	       gtk_tree_view_scroll_to_cell(view, path, NULL, TRUE, 0.05, 0.0);

	       /* we only want the first, so clean up and return */
	       gtk_tree_path_free(path);
	       return;
	  }
     } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter) == TRUE);

     return;
}


/* Empty the instance list from its Gtk list model and the associated widgets */
void uigraph_curve_unload()
{
     /* empty gtk list, leaving it intact */
     GtkListStore *list;
     list = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						  "curves_liststore") );
     gtk_list_store_clear(list);

     /* remove items from drawn tree and free key storage, leaving hint 
      * tree for next time */
     tree_clearout(uigraph_curves_drawn, tree_infreemem, NULL);
}

/*
 * Callback when a curve button has been clicked, which will cause a
 * new curve to be drawn or an existing one to be removed
 */
G_MODULE_EXPORT void
uigraph_on_curve_toggled (GtkCellRendererToggle *widget,
			  gchar                 *path_string,
			  gpointer               user_data)
{
     int active;
     gchar *label, *colname;
     GtkTreePath *path;
     GtkListStore *list;
     GtkTreeIter iter;
     GdkColor *colour;
     int possmax;

     /* bring in the name and active state from the model */
     list = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						  "curves_liststore"));
     path = gtk_tree_path_new_from_string(path_string);
     gtk_tree_model_get_iter (GTK_TREE_MODEL (list), &iter, path);
     gtk_tree_model_get(GTK_TREE_MODEL (list), &iter, 
			UIGRAPH_CURVE_COLNAME, &colname,
			UIGRAPH_CURVE_LABEL, &label,
			UIGRAPH_CURVE_ACTIVE, &active,
			UIGRAPH_CURVE_POSSIBLEMAX, &possmax,
			-1);

     if (!active) {
	  /* add new curve name to lists: current and long term */
	  tree_add(uigraph_curves_hint, xnstrdup(colname), NULL);
	  tree_add(uigraph_curves_drawn, xnstrdup(colname), NULL);

	  /* draw new curve */
	  if ( ! uigraph_datatab )
	       elog_die(FATAL, "data table has not been set");
	  colour = uigraph_drawcurve(colname, 1.0, 0.0);
	  graphdbox_setallminmax(uigraph_graphset, possmax);

	  gtk_list_store_set(list, &iter, 
			     UIGRAPH_CURVE_ACTIVE, 1,
			     UIGRAPH_CURVE_COLOUR, colour,
			     -1);

     } else {
	  /* undraw curve */
	  graphdbox_rmcurveallgraphs(uigraph_graphset, colname);

	  gtk_list_store_set(list, &iter, 
			     UIGRAPH_CURVE_ACTIVE, 0,
			     UIGRAPH_CURVE_COLOUR, NULL,
			     -1);

	  /* remove curve name from lists: current and long term */
          if (tree_find(uigraph_curves_hint, colname) != TREE_NOVAL) {
	       nfree(tree_getkey(uigraph_curves_hint));
	       tree_rm(uigraph_curves_hint);
	  }

	  if (tree_find(uigraph_curves_drawn, colname) != TREE_NOVAL) {
	       nfree(tree_getkey(uigraph_curves_drawn));
	       tree_rm(uigraph_curves_drawn);
	  }
     }

     /* clear up */
     gtk_tree_path_free (path);
     g_free(colname);
     g_free(label);
}
