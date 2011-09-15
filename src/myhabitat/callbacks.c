/*
 * Miscellaneous callbacks for MyHabitat
 * Nigel Stuckey, March 2010
 */
#include <gtk/gtk.h>
#include "main.h"
#include "uicollect.h"
#include "uiedit.h"
#include "callbacks.h"
#include "../iiab/iiab.h"
#include "../iiab/cf.h"
#include "../iiab/httpd.h"
#include "../iiab/util.h"
#include "../iiab/nmalloc.h"

/* callback to quit application, giving the opportunity to not quit if so 
 * configured */
G_MODULE_EXPORT void 
on_quit (GtkObject *object, gpointer user_data)
{
     int dont_ask_to_quit;

     dont_ask_to_quit = cf_getint(iiab_cf, DONTASKTOQUIT_CFNAME);
     if (dont_ask_to_quit == CF_UNDEF)
          dont_ask_to_quit = 0;	/* default is to ask */

     if (dont_ask_to_quit)
          gtk_main_quit();
     else
          show_window("quit_win");
}

/* callback to create a new toplevel window */
G_MODULE_EXPORT void 
on_new_window (GtkObject *object, gpointer user_data)
{
  g_print("on_new_window\n");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_choice_refresh_choice (GtkObject *object, gpointer user_data)
{
  g_print("on_choice_refresh_choice\n");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_choice_refresh_data (GtkObject *object, gpointer user_data)
{
  g_print("on_choice_refresh_data\n");
}


/* callback to ...? */
G_MODULE_EXPORT void 
on_choice_close (GtkObject *object, gpointer user_data)
{
  g_print("on_choice_close\n");
  show_window("close_win");
}


/* callback to show and hide the toolbars */
G_MODULE_EXPORT void 
on_view_toolbar (GtkObject *object, gpointer user_data)
{
     GtkWidget *ringview_w, *menu_w, *view_w;

     /* g_print("on_view_toolbar\n"); */

     ringview_w = get_widget("ringview_handlebox");
     view_w = get_widget("view_handlebox");
     menu_w = get_widget("m_view_toolbar");

     g_signal_handlers_block_by_func(G_OBJECT(object), 
				     G_CALLBACK(on_view_toolbar), NULL);

     if (GTK_WIDGET_VISIBLE(ringview_w)) {
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), FALSE);
  	  gtk_widget_hide(ringview_w);
  	  gtk_widget_hide(view_w);
     } else {
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), TRUE);
  	  gtk_widget_show(ringview_w);
  	  gtk_widget_show(view_w);
     }

     g_signal_handlers_unblock_by_func(G_OBJECT(object), 
				       G_CALLBACK(on_view_toolbar), NULL);
}


#if 0
NOT IMPLEMENTED YET

/* callback to show and hide the offsets columns pane */
G_MODULE_EXPORT void 
on_view_offsets (GtkObject *object, gpointer user_data)
{
  /* NOT YET TESTED */
     GtkWidget *offsets_w, *menu_w, *btn_w;

     offsets_w = get_widget("inst_list");
     menu_w = get_widget("m_view_metrics");
     btn_w = get_widget("view_metrics_btn");

     g_print("on_view_offsets\n");

     g_signal_handlers_block_by_func(G_OBJECT(object), 
				     G_CALLBACK(on_view_offsets), NULL);

     if (GTK_WIDGET_VISIBLE(offsets_w)) {
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), FALSE);
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(btn_w),
					    FALSE);
  	  gtk_widget_hide(offsets_w);
     } else {
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), TRUE);
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(btn_w),
					    TRUE);
	  gtk_widget_show(offsets_w);
     }

     g_signal_handlers_unblock_by_func(G_OBJECT(object), 
				       G_CALLBACK(on_view_offsets), NULL);
}
#endif

/* callback to show and hide the curves pane */
G_MODULE_EXPORT void 
on_view_curves (GtkObject *object, gpointer user_data)
{
     GtkWidget *divider_w, *menu_w, *btn_w, *visnote_w;
     GtkAllocation vis_alloc;
     int position, vis_width;
     static int saved_div_position = 0;

     divider_w = get_widget("graph_divider");
     menu_w = get_widget("m_view_curves");
     btn_w = get_widget("view_metrics_btn");
     visnote_w = get_widget("visualisation_notebook");

     g_signal_handlers_block_by_func(G_OBJECT(object), 
				     G_CALLBACK(on_view_curves), NULL);

     position = gtk_paned_get_position(GTK_PANED(divider_w));
     gtk_widget_get_allocation(visnote_w, &vis_alloc);
     vis_width = vis_alloc.width;

     /* if the divider is set to have a very small gap to the right hand 
      * edge then it is assumed that the curves are hidden to all intents.
      * The gap is measured against the edge of the visualisation_notebook */
     if (position + 50 < vis_width) {
          /* hide by setting the position to 9999 */
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), FALSE);
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(btn_w),
					    FALSE);
	  gtk_paned_set_position(GTK_PANED(divider_w), 9999);
	  saved_div_position = position;
     } else {
          /* show curve list by restoring the previous divider position */
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), TRUE);
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(btn_w),
					    TRUE);
	  gtk_paned_set_position(GTK_PANED(divider_w), saved_div_position);
     }

     g_signal_handlers_unblock_by_func(G_OBJECT(object), 
				       G_CALLBACK(on_view_curves), NULL);
}

/* callback to show and hide the choice pane */
G_MODULE_EXPORT void 
on_view_choices (GtkObject *object, gpointer user_data)
{
     GtkWidget *scroll_w, *menu_w, *btn_w;

     scroll_w = get_widget("choice_scrollwin");
     menu_w = get_widget("m_view_choices");
     btn_w = get_widget("view_choice_btn");

     g_signal_handlers_block_by_func(G_OBJECT(object), 
				     G_CALLBACK(on_view_choices), NULL);

     if (GTK_WIDGET_VISIBLE(scroll_w)) {
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), FALSE);
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(btn_w),
					    FALSE);
  	  gtk_widget_hide(scroll_w);
     } else {
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_w), TRUE);
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(btn_w),
					    TRUE);
	  gtk_widget_show(scroll_w);
     }

     g_signal_handlers_unblock_by_func(G_OBJECT(object), 
				       G_CALLBACK(on_view_choices), NULL);
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_view_pattern_events (GtkObject *object, gpointer user_data)
{
  g_print("on_view_pattern_events\n");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_view_client_logs (GtkObject *object, gpointer user_data)
{
  g_print("on_view_client_logs\n");
  show_window("log_win");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_view_raw_data (GtkObject *object, gpointer user_data)
{
  g_print("on_view_raw_data\n");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_view_replication_logs (GtkObject *object, gpointer user_data)
{
  g_print("on_view_replication_logs\n");
}

/* callback to show clockwork status and start it or stop it */
G_MODULE_EXPORT void 
on_collect_status (GtkObject *object, gpointer user_data)
{
  if ( uicollect_is_clockwork_running(NULL, NULL, NULL, NULL, 0) )
          uicollect_showstopclockwork();
     else
          uicollect_askclockwork();
}


/* show the clockwork collector details in the stopclock window */
G_MODULE_EXPORT void 
on_stopclock_show_details (GtkObject *object, gpointer user_data)
{
     GtkWidget *w;

     w = get_widget("stopclock_detail_grid");

     if (GTK_WIDGET_VISIBLE(w))
          gtk_widget_hide(w);
     else
          gtk_widget_show(w);
}


/* callback to stop clockwork */
G_MODULE_EXPORT void 
on_stop_collect (GtkObject *object, gpointer user_data)
{
     hide_widget("stop_clockwork_win");
     uicollect_stopclockwork();
}


/* callback to stop clockwork */
G_MODULE_EXPORT void 
on_start_collect (GtkObject *object, gpointer user_data)
{
     hide_widget("start_clockwork_win");
     uicollect_startclockwork();
}


/* callback to ...? */
G_MODULE_EXPORT void 
on_view_collection_logs (GtkObject *object, gpointer user_data)
{
  g_print("on_view_collection_logs\n");
}


/* callback to ...? */
G_MODULE_EXPORT void 
on_choice_prop (GtkObject *object, gpointer user_data)
{
  g_print("on_choice_prop\n");
  show_window("property_win");
}

/* callback to edit the habrc files, the personal configuration */
G_MODULE_EXPORT void 
on_edit_habrc (GtkObject *object, gpointer user_data)
{
     char *usercf;

     /* edit the object refered to as 'c' in the config */
     usercf = cf_getstr(iiab_cf, "c");
     uiedit_load_route(usercf, "User Configuration");
}


/* callback to edit the current job table used by clockwork */
G_MODULE_EXPORT void 
on_edit_jobs (GtkObject *object, gpointer user_data)
{
     TABLE clockcf=NULL;
     char *clockpurl, *jobpurl, jobpurl_t[1000];
     int r;

     /* Check clockwork is running to see if we can read its configuration */
     if (uicollect_is_clockwork_running(NULL, NULL, NULL, NULL, 1)) {

          /* Get the latest configuration from the local running clockwork */
          clockpurl = util_strjoin("http://localhost:", HTTPD_PORT_HTTP_STR,
				   "/cftsv", NULL);
	  clockcf = route_tread(clockpurl, NULL);
	  if ( ! clockcf ) {
	       elog_printf(DIAG, "Unable to read clockwork configuration "
			   "(%s), although it is running; possibly security "
			   "is an issue", clockpurl);
	       elog_printf(FATAL, "<big><b>Unable to Load Collector "
			   "Configuration</b></big>\n"
			   "The collector is running but the configuration "
			   "can't be read. Check Habitat's security "
			   "configuration");
	       nfree(clockpurl);
	       return;
	  }
	  nfree(clockpurl);

	  r = table_search(clockcf, "name", "jobs");
	  if (r == -1) {
	       elog_printf(DIAG, "Clockwork configuration read but 'jobs' "
			   "declaration is missing");
	       elog_printf(FATAL, "<big><b>Unable to Load Collector "
			   "Configuration</b></big>\n"
			   "The collector does not have a configured job "
			   "table. Please check your configuration");
	       table_destroy(clockcf);
	       return;
	  }

	  jobpurl = table_getcurrentcell(clockcf, "value");
     } else {

          /* Clockwork is not running, so find the job table from the 
	   * current configuration (the jobs directive). This has a flaw as 
	   * clockwork may be started manually with a job switch (-j or -J) */
          jobpurl = cf_getstr(iiab_cf, "jobs");
	  if ( ! jobpurl) {
	       elog_printf(FATAL, "Unable to load collection jobs, as there "
			   "was no configuration directive.\n\n"
			   "Please specify -j, -J or set the directive `jobs' "
			   "in the configuration file to the route containing "
			   "a job table. \n\n"
			   "For example, `jobs=file:/etc/clockwork.jobs' "
			   "will look for the file /etc/clockwork.jobs");
	       return;
	  }
     }

     /* read the job table */
     r = route_expand(jobpurl_t, jobpurl, "NOJOB", 0);
     if (r == -1 || jobpurl_t[0] == '\0') {
          elog_printf(FATAL, "Unable to load collection jobs, as there are "
		      "no valid configuration directives in the table %s/%s. "
		      "Please specify -j, -J or set the directive `jobs' in "
		      "the configuration file to the route containing a job "
		      "table. For example, `jobs=file:/etc/clockwork.jobs' "
		      "will look for the file /etc/clockwork.jobs", 
		      jobpurl, jobpurl_t);
	  return;
     }

     uiedit_load_route(jobpurl_t, "Collection Jobs");

     if (clockcf)
          table_destroy(clockcf);
}


/* callback to edit harvest */
G_MODULE_EXPORT void 
on_edit_harvest (GtkObject *object, gpointer user_data)
{
     show_window("harvest_win");
}


/* callback to check for updates */
G_MODULE_EXPORT void 
on_check_for_updates (GtkObject *object, gpointer user_data)
{
     g_print("on_check_for_updates\n");
}


#if 0

/* callback to open a file */
G_MODULE_EXPORT void 
on_open_file (GtkObject *object, gpointer user_data)
{
  g_print("on_open_file\n");
  show_window("filechooser_win");
}

/* callback to close files or routes */
G_MODULE_EXPORT void 
on_close_file (GtkObject *object, gpointer user_data)
{
  g_print("on_close_file\n");
  show_window("quit_win");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_print (GtkObject *object, gpointer user_data)
{
  g_print("on_print\n");
  show_window("print_win");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_print_preview (GtkObject *object, gpointer user_data)
{
  g_print("on_print_preview_activate\n");
  show_window("preview_win");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_copy_ring (GtkObject *object, gpointer user_data)
{
  g_print("on_copy_ring\n");
  show_window("copy_win");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_close_ring (GtkObject *object, gpointer user_data)
{
  g_print("on_close_ring\n");
  show_window("close_win");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_edit_properties (GtkObject *object, gpointer user_data)
{
  g_print("on_edit_properties\n");
  show_window("property_win");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_panels (GtkObject *object, gpointer user_data)
{
  g_print("on_panels\n");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_data_bounds (GtkObject *object, gpointer user_data)
{
  g_print("on_data_bounds\n");
}

/* callback to ...? */
G_MODULE_EXPORT void 
on_about (GtkObject *object, gpointer user_data)
{
  g_print("on_about\n");
}

#endif

