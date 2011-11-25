/*
 * Habitat GUI visualisation code
 *
 * Nigel Stuckey, August 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */
#include <time.h>
#include <string.h>
#include "../iiab/util.h"
#include "uivis.h"
#include "rcache.h"
#include "uitable.h"
#include "uigraph.h"
#include "main.h"
#include "gconv.h"
#include "uilog.h"

/* globals to store the current and previous views */
enum uivis_t uivis_vis_mode, uivis_vis_oldmode;

/* global to remember the model */
GtkListStore *uivis_model=NULL;

/* initialise visualisations */
void uivis_init()
{
     uivis_change_view(UIVIS_SPLASH);
}
void uivis_fini() {
}

/*
 * Change the visualisation mode.
 * Called when a new node is picked from the choice tree.
 * Drawing comes later (see uivis_draw() below) and no data is fetched.
 */
void uivis_change_view(enum uivis_t vis)
{
     GtkNotebook *display, *visualisation;

     /* Work out the visualisation change, implemented as a notebook in Gtk.
      * 1. Splash when there is no data, 2. text data, 3. chart data */
     display = GTK_NOTEBOOK (get_widget("display_notebook"));
     visualisation = GTK_NOTEBOOK (get_widget("visualisation_notebook"));
     switch (vis) {
     case UIVIS_INFO:
          /* Information about the chosen node */
          gtk_notebook_set_current_page(display, UIVIS_NOTEBOOK_DISPLAY_INFO);
	  uidata_populate_info();
          break;
     case UIVIS_TEXT:
          /* Show the node contents as a text representation */
          gtk_notebook_set_current_page(display, UIVIS_NOTEBOOK_DISPLAY_MAIN);
          gtk_notebook_set_current_page(visualisation, UIVIS_NOTEBOOK_VIS_TEXT);
          break;
     case UIVIS_HTML:
          /* Show the node contents as an HTML representation */
          gtk_notebook_set_current_page(display, UIVIS_NOTEBOOK_DISPLAY_MAIN);
          gtk_notebook_set_current_page(visualisation, UIVIS_NOTEBOOK_VIS_HTML);
          break;
     case UIVIS_TABLE:
          /* Show the node contents as a table */
          gtk_notebook_set_current_page(display, UIVIS_NOTEBOOK_DISPLAY_MAIN);
          gtk_notebook_set_current_page(visualisation, UIVIS_NOTEBOOK_VIS_TABLE);
          break;
     case UIVIS_CHART:
          /* Show the node contents as a chart */
          gtk_notebook_set_current_page(display, UIVIS_NOTEBOOK_DISPLAY_MAIN);
	  gtk_notebook_set_current_page(visualisation, UIVIS_NOTEBOOK_VIS_CHART);
          break;
     case UIVIS_WHATNEXT:
          /* Show the 'what next?' screen */
          gtk_notebook_set_current_page(display, UIVIS_NOTEBOOK_DISPLAY_WHATNEXT);
	  break;
     case UIVIS_SPLASH:
     default:
          /* show the splash screen when asked and by default */
          gtk_notebook_set_current_page(display, UIVIS_NOTEBOOK_DISPLAY_SPLASH);
	  break;
     }

     /* remember old mode and new mode */
     uivis_vis_oldmode = uivis_vis_mode;
     uivis_vis_mode = vis;

     /* Handle the greying out of menu and button items */
     GtkWidget *w;
     w = get_widget("view_metrics_btn");  gtk_widget_set_sensitive(w, FALSE);
     w = get_widget("view_hzoom_btn");    gtk_widget_set_sensitive(w, FALSE);
     w = get_widget("view_vzoom_btn");    gtk_widget_set_sensitive(w, FALSE);
     w = get_widget("view_zoomout_btn");  gtk_widget_set_sensitive(w, FALSE);
     w = get_widget("view_zoomhome_btn"); gtk_widget_set_sensitive(w, FALSE);
     switch (vis) {
     case UIVIS_TEXT:
     case UIVIS_TABLE:
     case UIVIS_CHART:
          /* enable the data viewing menu options */
          w = get_widget("m_print");          gtk_widget_set_sensitive(w, TRUE);
          w = get_widget("m_print_preview");  gtk_widget_set_sensitive(w, TRUE);
          w = get_widget("m_view_chart");     gtk_widget_set_sensitive(w, TRUE);
          w = get_widget("m_view_table");     gtk_widget_set_sensitive(w, TRUE);
          w = get_widget("m_view_text");      gtk_widget_set_sensitive(w, TRUE);
          w = get_widget("m_zoom");           gtk_widget_set_sensitive(w, TRUE);
          w = get_widget("m_panels");         gtk_widget_set_sensitive(w, TRUE);
          w = get_widget("m_data_pulldown");  gtk_widget_set_sensitive(w, TRUE);
          break;
     case UIVIS_SPLASH:
     case UIVIS_WHATNEXT:
     case UIVIS_INFO:
     case UIVIS_HTML:
     default:
          /* disable the data viewing menu options */
          w = get_widget("m_print");         gtk_widget_set_sensitive(w, FALSE);
          w = get_widget("m_print_preview"); gtk_widget_set_sensitive(w, FALSE);
          w = get_widget("m_view_chart");    gtk_widget_set_sensitive(w, FALSE);
          w = get_widget("m_view_table");    gtk_widget_set_sensitive(w, FALSE);
          w = get_widget("m_view_text");     gtk_widget_set_sensitive(w, FALSE);
          w = get_widget("m_zoom");          gtk_widget_set_sensitive(w, FALSE);
          w = get_widget("m_panels");        gtk_widget_set_sensitive(w, FALSE);
          w = get_widget("m_data_pulldown"); gtk_widget_set_sensitive(w, FALSE);
	  break;
     }
     if (vis == UIVIS_CHART) {
          /* enable extra items if a chart */
          w = get_widget("view_metrics_btn"); gtk_widget_set_sensitive(w, TRUE);
	  w = get_widget("view_hzoom_btn");   gtk_widget_set_sensitive(w, TRUE);
	  w = get_widget("view_vzoom_btn");   gtk_widget_set_sensitive(w, TRUE);
	  w = get_widget("view_zoomout_btn"); gtk_widget_set_sensitive(w, TRUE);
	  w = get_widget("view_zoomhome_btn");gtk_widget_set_sensitive(w, TRUE);
     }
}


/*
 * Draw the data in the current visualisation mode [as set by 
 * uivis_change_view()]
 * Data is defined by the route name (key to cache) or the data function
 * and bounded by the two times.
 * If the route name exists the function will not be used; if the route name
 * is NULL, then data function will be called; if both are NULL, nothing 
 * is drawn.
 * Route data is found from the cache if it exists, if it does not then 
 * no redrawing takes place. If the data function returns NULL or is empty
 * then no redrawing takes place.
 * Currently causes the whole model to be recreated and new views to be made.
 * Called by timeslider or the visualisation buttons.
 */
void uivis_draw(char *route, TABLE (*dfunc)(time_t, time_t), 
		time_t view_oldest, time_t view_youngest)
{
     TABLE tabdata;
     GtkListStore  *model;
     GtkTreeView   *view;
     GtkNotebook   *vis_notebook;
     GtkTextBuffer *vis_textbuffer;
     GtkWidget     *vis_table_scroll, *old_view;
     char          *buffer;

     if ( ! route && ! dfunc)
          return;

     vis_notebook = GTK_NOTEBOOK(get_widget("visualisation_notebook"));
     vis_table_scroll = get_widget("vis_table_scroll");
     vis_textbuffer = GTK_TEXT_BUFFER(gtk_builder_get_object(gui_builder,
							     "vis_textbuffer"));

     /* Clear up current visutalisations */
     switch (uivis_vis_oldmode) {
     case UIVIS_INFO:
     case UIVIS_TEXT:
     case UIVIS_HTML:
     case UIVIS_TABLE:
     case UIVIS_CHART:
     case UIVIS_SPLASH:
     case UIVIS_WHATNEXT:
     default:
          break;
     }

     /* Source the data to visualise from route or function */
     if (route) {
          tabdata = rcache_find(route);
	  if (!tabdata) {
	       g_print("**unable to find data (route %s) in cache\n", route);
	       return;
	  }
     } else if (dfunc) {
          tabdata = (*dfunc)(view_oldest, view_youngest);
	  if (!tabdata) {
	       g_print("**data function has not given data\n");
	       return;
	  }
     }

     /* Visualisation specific handling. Each mode has its own model or
      * method of storing data, the instialisation and destruction of each 
      * is handled in each case. This is to allow for */
     switch (uivis_vis_mode) {
     case UIVIS_INFO:
          /* Information about the chosen node */

          model = NULL;

          /* Do nothing as yet */

          break;
     case UIVIS_TEXT:
          /* Show the node contents as a text representation */

          model = NULL;

	  /* As we ask for the data to be interpreted as a table, text
	   * will be help in column data. If it exists, just display 
	   * the single column. If not, then just print the table out as
	   * text and use that */
	  table_first(tabdata);
	  buffer = table_getcurrentcell(tabdata, "data");
	  if (buffer) {
	       gtk_text_buffer_set_text(vis_textbuffer, buffer, -1);
	  } else {
	       buffer = table_print(tabdata);
	       gtk_text_buffer_set_text(vis_textbuffer, buffer, -1);
	       nfree(buffer);
	  }

          break;
     case UIVIS_HTML:
          /* Show the node contents as an HTML representation */

          model = NULL;

          /* To be done */

          break;

     case UIVIS_TABLE:
          /* Show the node contents as a table */

          model = uitable_mkmodel(tabdata, view_oldest, view_youngest);
	  if (!model) {
	       g_print("**unable to create model (route %s)\n", route);
	       return;
	  }
	  view = uitable_mkview(tabdata, model);
	  if (!view) {
	       g_print("**unable to create view (route %s)\n", route);
	       uitable_freemodel(model);
	       return;
	  }

	  /* Replace the existing table with our new one */
	  old_view = gtk_bin_get_child( GTK_BIN(vis_table_scroll) );
	  if (old_view)
	       gtk_widget_destroy(old_view);
	  gtk_container_add(GTK_CONTAINER(vis_table_scroll), GTK_WIDGET(view));
	  gtk_widget_show_all(GTK_WIDGET(view)); 
	  break;

     case UIVIS_CHART:
          /* Show the node contents as a chart */
          /* Directly uses tabdata and my graphdbox class to manage the 
	   * data representation; a tree model is not needed as
	   * uigraph/graphdbox internally create float arrays that do 
	   * a similar thing .*/

       /* update -- if the same route or the same function and currently zoomed  (so need uigraph_is_zoomed()) then do uigraph_data_update_redraw() or uigraph_redraw_all_selelcted() or nothing at all */
          uigraph_rm_all_graphs();
	  uigraph_data_load(tabdata);
	  uigraph_set_timebase(view_oldest, view_youngest);
	  uigraph_draw_all_selected();

          model = NULL;
          break;
     case UIVIS_SPLASH:
     case UIVIS_WHATNEXT:
     default:
          /* show the splash screen when asked and by default */

          model = NULL;

          /* To be done */

	  break;
     }

     /* Store the widgets globally */
     if (uivis_model)
          uitable_freemodel(uivis_model);
     uivis_model = model;
}



extern gchar *uidata_ringpurl;			/* current route */
TABLE (*uidata_ringdatacb)(time_t, time_t);	/* current data source */
extern time_t uitime_view_oldest;		/* current first time */
extern time_t uitime_view_youngest;		/* current second time */

/* Callback to handle a change in the visualisation button group.
 * The active button dictates the viewing frames/widgets to be 
 * shown and the current viewed data will be redrawn in the appropriate
 * visualisation.
 * No data will be requested from the cache; it uses the current data. */
G_MODULE_EXPORT void 
uivis_on_vis_changed (GtkToolButton *toolbutton, gpointer data)
{
     const gchar *label;
     enum uivis_t vis;
     GtkWidget *menu_w;

     if ( ! gtk_toggle_tool_button_get_active(
				GTK_TOGGLE_TOOL_BUTTON(toolbutton)) )
          return;	/* button is up, do nothing */

     label = gtk_tool_button_get_label(GTK_TOOL_BUTTON(toolbutton));

     g_print("uivis_on_vis_changed() button %s\n", label);

     /* prevent repeat event handlers as we have to trigger the 
      * associcated menu entry */
     g_signal_handlers_block_by_func(G_OBJECT(toolbutton), 
				     G_CALLBACK(uivis_on_vis_changed), NULL);

     /* find the button pressed (as this is a communal callback), hold the 
      * type and set the appropriate menu state. As this callback is
      * turned off, we shouldn't get loops of repreased actions */
     if (strcmp(label, "Text") == 0) {
          vis = UIVIS_TEXT;
	  menu_w = get_widget("m_view_text");
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), TRUE);
     } else if (strcmp(label, "Table") == 0) {
          vis = UIVIS_TABLE;
	  menu_w = get_widget("m_view_table");
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), TRUE);
     } else if (strcmp(label, "Chart") == 0) {
          vis = UIVIS_CHART;
	  menu_w = get_widget("m_view_chart");
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), TRUE);
     } else {
          /* default */
          vis = UIVIS_SPLASH;
     }

     /* and release */
     g_signal_handlers_unblock_by_func(G_OBJECT(toolbutton), 
				       G_CALLBACK(uivis_on_vis_changed), NULL);

     uilog_setprogress("Drawing data", 0.4, 0);

     uivis_change_view(vis);

     /* draw the data but do not attempt to fetch any more as the 
      * timeslider has not been touched. Use the global externs from 
      * uidata and uitime to specify the parameters */
     uivis_draw(uidata_ringpurl, uidata_ringdatacb, 
		uitime_view_oldest, uitime_view_youngest);

     uilog_clearprogress();
}


/* callback to show text visualisation widget */
G_MODULE_EXPORT void 
uivis_on_view_text (GtkObject *object, gpointer user_data)
{
     GtkWidget *btn_w;

     if ( ! gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM(object)) )
          return; /* don't want deactivated */

     g_print("on_view_text\n");

     /* We don't do anything directly, instead we activate the associcated
      * toolbar button which will trigger the action */
     btn_w = get_widget("ringview_text_btn");
     gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(btn_w), TRUE);
}

/* callback to show table visualisation widget */
G_MODULE_EXPORT void 
uivis_on_view_table (GtkObject *object, gpointer user_data)
{
     GtkWidget *btn_w;

     if ( ! gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM(object)) )
          return; /* don't want deactivated */

     g_print("on_view_table\n");

     /* We don't do anything directly, instead we activate the associcated
      * toolbar button which will trigger the action */
     btn_w = get_widget("ringview_table_btn");
     gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(btn_w), TRUE);
}

/* callback to show chart visualisation widget */
G_MODULE_EXPORT void 
uivis_on_view_chart (GtkObject *object, gpointer user_data)
{
     GtkWidget *btn_w;

     if ( ! gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM(object)) )
          return; /* don't want deactivated */

     g_print("on_view_chart\n");

     /* We don't do anything directly, instead we activate the associcated
      * toolbar button which will trigger the action */
     btn_w = get_widget("ringview_chart_btn");
     gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(btn_w), TRUE);
}

