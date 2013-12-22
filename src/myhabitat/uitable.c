/*
 * Habitat table UI code, converting a TABLE into GtkListStore model
 * which is the viewed by GtkTreeView.
 *
 * TO DO
 * There is almost certainly a better way of doing this, writing a
 * shim to convert the TABLE into a GtkTreeView model directly
 *
 * Nigel Stuckey, September 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#define _ISOC99_SOURCE
#define _XOPEN_SOURCE

#include <time.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "../iiab/util.h"
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"
#include "rcache.h"
#include "uilog.h"
#include "uitable.h"
#include "main.h"

int uitable_timecol=0;	/* index of _time col */

/*
 * Convert a TABLE into a GtkListStore, which is the model (omits headers, etc).
 * Filter out data outside the min and max viewing boundaries.
 * Updates UI progress bar from 50% to 80%
 */
GtkListStore *uitable_mkmodel(TABLE tab, time_t view_min, time_t view_max)
{
     GtkListStore *list;
     GtkTreeIter iter;
     ITREE *hdorder;
     TREE  *row;
     GType *coltypes;
     char  *cell;
     int i, col, ncols, nrows;
     time_t timestamp;

     /* create gtk list */
     if (!tab)
          return NULL;		/* no table, can't do anything */
     ncols = table_ncols(tab);
     nrows = table_nrows(tab);
     coltypes = nmalloc(sizeof(GType) * ncols);
     for (i=0; i<ncols; i++)
          coltypes[i] = G_TYPE_STRING;
     list = gtk_list_store_newv(ncols, coltypes);

     /* iterate over the rows, adding to the list one cell at a time */
     hdorder = tab->colorder;
     i = 0;
     table_traverse(tab) {
          if (i % 100 == 0)
	       uilog_setprogress("Arranging data", 0.5 + (0.3 * i / nrows), 1);
          row = table_getcurrentrow(tab);
	  gtk_list_store_prepend(list, &iter);
	  col=0;
	  itree_traverse(hdorder) {
	       cell = tree_find(row, (char *) itree_get(hdorder));
	       if (cell) {
		    /* _time col format change and remember its col num */
		    if (strncmp(itree_get(hdorder), "_time", 5) == 0) {
		         uitable_timecol = col;
			 timestamp = strtol(cell, (char**)NULL, 10);
			 if (timestamp < view_min || timestamp > view_max) {
			      /* out of range, remove from model */
			      gtk_list_store_remove(list, &iter);
			      goto endofrow;
			 }
		         gtk_list_store_set(list, &iter, col,
					    util_decdatetime(timestamp),
					    -1);
		    } else {
		         gtk_list_store_set(list, &iter, col, cell, -1);
		    }
	       }
	       col++;
	  }
     endofrow:
	  tree_destroy(row);
	  i++;
     }

     int nchildren = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list), NULL);
     /*g_print("created gtklist with %d columns %d rows\n", ncols, nchildren);*/

     elog_printf(INFO, "Showing %d data points, %d samples, %d attributes", 
		 nchildren * ncols, nchildren, ncols);

     /* return the Gtk list model */
     nfree(coltypes);

     return list;
}


/*
 * Free the GtkListStore created by uitable_mkmodel()
 */
void uitable_freemodel(GtkListStore *list)
{
     g_object_unref(list);
}


/*
 * Create a GtkTreeView from the List Store
 */
GtkTreeView * uitable_mkview(TABLE tab, GtkListStore *model)
{
     GtkTreeViewColumn *tvcol;
     GtkCellRenderer   *renderer;
     GtkWidget         *view;
     int                col;
     ITREE             *hdorder;
     char              *name, *key, *info, *keystr, *bigtip;

     if (!tab || !model)
          return NULL;		/* no table or model: can't do anything */

     /* Tree view and settings */
     view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
     gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), 1);
     g_object_set (view, "has-tooltip", TRUE, NULL);
     g_signal_connect (view, "query-tooltip",
		       G_CALLBACK (uitable_cb_query_tooltip), NULL);
#if 0
     g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (view)),
		       "changed", G_CALLBACK (selection_changed_cb), view);
#endif

     /* create gtk tree view */
     hdorder = tab->colorder;
     col = 0;
     itree_traverse(hdorder) {
          /* Create column and assign a name */
	  tvcol = gtk_tree_view_column_new();
	  name = table_getinfocell(tab, "name", itree_get(hdorder));
	  if (name && *name && strcmp(name, "0") != 0 
	      && strcmp(name, "-") != 0)

	       /* 'pretty' name */
	       gtk_tree_view_column_set_title(tvcol, name);
	  else
	       /* just the attribute name */
	       gtk_tree_view_column_set_title(tvcol, itree_get(hdorder));
	  gtk_tree_view_append_column(GTK_TREE_VIEW(view), tvcol);

	  /* Create a column tooltip from the TABLE 'info' info row */
          key  = table_getinfocell(tab, "key",  itree_get(hdorder));
          info = table_getinfocell(tab, "info", itree_get(hdorder));
	  if ( ! info)
	       info = "";
	  if (key == NULL)
	       keystr = "";
	  else if (*key == '1')
	       keystr = ",primary key";
	  else if (*key == '2')
	       keystr = ",secondary key";
	  else if (*key == '3')
	       keystr = ",tertiary key";
	  else
	       keystr = "";
	  if (*keystr) {
	       bigtip = util_strjoin(info, " (", keystr, ")", NULL);
	  } else {
	       bigtip = xnstrdup(info);
	  }

	  g_object_set (tvcol->button, "tooltip-text", bigtip, NULL);
	  nfree(bigtip);

	  /* Set a text renderer for the column */
	  renderer = gtk_cell_renderer_text_new();
	  gtk_tree_view_column_pack_start(tvcol, renderer, TRUE);
	  gtk_tree_view_column_add_attribute(tvcol, renderer, "text", col);
	  col++;
     }

     gtk_tree_selection_set_mode(gtk_tree_view_get_selection(
				GTK_TREE_VIEW(view)), GTK_SELECTION_NONE);

     /*g_print("created gtktreeview with %d columns\n", itree_n(hdorder));*/

     return GTK_TREE_VIEW(view);
}


/*
 * Free the GtkTreeView created by uitable_mkview()
 */
void uitable_freeview(GtkTreeView *view)
{
     g_object_unref(view);
}


/*
 * Callback for the tooltip cells on the tree view
 */
gboolean
uitable_cb_query_tooltip (GtkWidget  *widget,
			  gint        x,
			  gint        y,
			  gboolean    keyboard_tip,
			  GtkTooltip *tooltip,
			  gpointer    data)
{
     GtkTreeIter iter;
     GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
     GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
     GtkTreePath *path = NULL;
     GtkTreeViewColumn *column;
     gchar *samptime;
     gchar *pathstring, *tip=NULL;
     const gchar *title="";
     gint treex, treey;
     char buffer[512] = {0};
     char *timedist;
     struct tm tm;
     time_t celltime;

     /* get the tooltip details */
     if (!gtk_tree_view_get_tooltip_context (tree_view, &x, &y,
					     keyboard_tip,
					     &model, &path, &iter))
          return FALSE;

     /* find time data and the row number (path) */
     gtk_tree_model_get (model, &iter, uitable_timecol, &samptime, -1);
     pathstring = gtk_tree_path_to_string (path);

     /* find the column tooltip - if keyboard_tip==FALSE, then (x,y) are
      * converted to bin_window coords; else treat as a widget coords.
      * We want tree coords. */
     if (keyboard_tip)
          gtk_tree_view_convert_widget_to_tree_coords(tree_view, x, y, 
						      &treex, &treey);
     else
          gtk_tree_view_convert_widget_to_bin_window_coords(tree_view, x, y, 
							  &treex, &treey);

     /*gtk_tree_view_convert_bin_window_to_tree_coords(tree_view, x, y, 
       &treex, &treey);*/

     /* get the column, so I can get the title and the tip */
     if (gtk_tree_view_get_path_at_pos(tree_view, treex, treey, NULL, 
				       &column, NULL, NULL)) {

          title = gtk_tree_view_column_get_title(column);
	  if (strncmp(title, "_time", 5) == 0) {

	       /* convert the time string into time_t to print a more 
		* helpful better tool tip */
	       if (strptime(samptime, "%d-%b-%y %I:%M:%S %p", &tm) == NULL) {
		    g_snprintf (buffer, 511, "<b>Sample time</b>");
	       } else {
		    celltime = mktime(&tm);
		    timedist = util_approxtimedist(time(NULL), celltime);
		    g_snprintf (buffer, 511, "<b>Sample time</b>\n%s ago",
				timedist);
		    nfree(timedist);
	       }
	  }
	  g_object_get (column->button, "tooltip-text", &tip, NULL);
     } else {
          g_snprintf (buffer, 511, "Row does not exist (path %s)", pathstring);
     }

     /* compose tooltip string */
     if ( ! buffer[0] )
#if 0
          g_snprintf (buffer, 511, "<b>%s</b>\n%s at %s (path %s)", 
		      tip?tip:"no tip", title, samptime, pathstring);
#else
          g_snprintf (buffer, 511, "<b>%s</b>\n%s at %s", 
		      tip?tip:"no tip", title, samptime);
#endif
     gtk_tooltip_set_markup (tooltip, buffer);

     gtk_tree_view_set_tooltip_row (tree_view, tooltip, path);

     g_free (samptime);
     if (tip)
          g_free (tip);

     gtk_tree_path_free (path);
     g_free (pathstring);

     return TRUE;
}

