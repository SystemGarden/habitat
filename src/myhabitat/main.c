/*
 * main.c for the myhabitat graphical visualisation tool written in Gtk2.
 * Nigel Stuckey
 * Copyright System Garden 2009. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include <unistd.h>
#include "rt_gtkgui.h"
#include "main.h"
#include "uichoice.h"
#include "uiabout.h"
#include "graphdbox.h"
#include "uigraph.h"
#include "rcache.h"
#include "uilog.h"
#include "uicollect.h"
#include "uiharvest.h"
#include "uiedit.h"
#include "../iiab/route.h"
#include "../iiab/elog.h"
#include "../iiab/iiab.h"
#include "../iiab/util.h"

/* configuration defaults in two phases: the first just relies on
 * stderr, the second uses the GUI once enough facilities have been
 * set up. The GUI config can either be info and above or debug and above */
char *cfdefaults = "elog.all			 none:\n"
		   "elog.above info		 stderr:\n"
		   "nmalloc			 0\n"
		   "jobs			 file:%l/default.jobs\n"
		   AUTOCLOCKWORK_CFNAME "	 0\n"
		   DEFAULT_CURVES_CFNAME "       pc_idle pc_nice pc_system "
                                               " pc_user pc_wait pc_work "
                                               " pc_used cpu cpupeak "
                                               " rx_pkts tx_pkts\n"
		   DEFAULT_INST_CFNAME "         eth0 eth1 eth2\n"
		   GRAPHDBOX_SHOWRULERS_CFNAME " 1\n"
		   GRAPHDBOX_SHOWAXIS_CFNAME "   1\n"
		   GRAPHDBOX_DRAWSTYLE_CFNAME "  line\n";

char *cfdefaults2i = 
	"elog.allformat		%7$c|%4$d|%5$s|%12$s|%13$s|%14$d|%17$s\n"
	"elog.all               none:\n"
	"elog.above info	gtkgui:\n";

char *cfdefaults2d = 
	"elog.allformat		%7$c|%4$d|%5$s|%12$s|%13$s|%14$d|%17$s\n"
	"elog.all               none:\n"
	"elog.above diag	gtkgui:\n";

/* gtk customisation files */
char *stylefiles[] = {
			 NULL};


/* Widgets instantiated locally that need to be global */
GtkBuilder   *gui_builder;

int
main (int argc, char *argv[])
{
     GtkWidget  *window;
     char       *gui_file, *rc_file;
     GError     *error = NULL;

     /* Parse the arguments, so all the gtk specific parameters are 
      * taken out of the command line  */
     gtk_parse_args(&argc, &argv);

     /* initialise the habitat library, config table and load the defaults */
     iiab_start(CMDLN_OPTS, argc, argv, CMDLN_USAGE, cfdefaults);

     /* Locations run-time bits of GTK project */
     gui_file = util_strjoin(iiab_dir_lib, "/myhabitat.glade", NULL);
     rc_file = util_strjoin(iiab_dir_lib, "/myhabitat.rc", NULL);

     if (access(gui_file, R_OK)) {
          elog_die(FATAL, "Unable to find MyHabitat's support files (gui) and "
		   "unable to continue. Please repair the installation before "
		   "continuing. (Looked in %s)", gui_file);
     }

     if (access(rc_file, R_OK)) {
          elog_die(FATAL, "Unable to find MyHabitat's support files (rc) and "
		   "unable to continue. Please repair the installation before "
		   "continuing. (Looked in %s)", rc_file);
     }

     /* gtk style */
     gtk_rc_add_default_file(rc_file);

     /* Initialise Gtk properly */
     gtk_init(&argc, &argv);

     /* build base GUI using GtkBuilder Object */
     gui_builder = gtk_builder_new();
     if ( ! gtk_builder_add_from_file(gui_builder, gui_file, &error) ) {
          elog_printf(ERROR, "Gtk Builder Error: %s", error->message);
	  g_error_free(error);
	  return (1);
     }

     /* Connect signals */
     gtk_builder_connect_signals( gui_builder, NULL );

     /* Complete the base GUI with finishing touches */
     complete_gui();
     nfree(gui_file);
     nfree(rc_file);

     /* Start telling us to wait */
     uilog_init();
     uilog_setprogress("Starting up...", 0.2, 0);

     /* start bits of the myhabitat (this) app */
     uipref_init();
     rcache_init();
     uichoice_init();
     uivis_init();
     uidata_init();
     uigraph_init();
     harv_init();
     uiabout_init();
     uiedit_init();

     /* Show window. All other widgets are automatically shown by GtkBuilder */
     window = get_widget("myhabitat_win");
     gtk_widget_show( window );

     /* set up waiting mouse pointer */
/*      mouse_pointer_wait = gdk_cursor_new(GDK_WATCH); */
/*      mouse_pointer_normal = gdk_cursor_new(GDK_TOP_LEFT_ARROW); */

     /* reconfigure elog now enough facilities are available */
     route_register(&rt_gtkgui_method);
     if ( ! cf_defined(iiab_cf, "D") ) {
	  /* -D is debug, send logs to stderr for safety */
	  if (cf_defined(iiab_cf, "d"))
	       /* -d is diagnostic, display logs in GUI */
	       cf_scantext(iiab_cf, NULL, cfdefaults2d, CF_OVERWRITE);
	  else
	       /* default start-up */
	       cf_scantext(iiab_cf, NULL, cfdefaults2i, CF_OVERWRITE);
	  elog_configure(iiab_cf);
     }
     /*elog_printf(ERROR, "this is a test ");*/

     if (cf_defined(iiab_cf, "s") ) {
          /* In safe start mode, dont start anything or load anything */
	  uilog_setprogress("Safe start", 0.0, 0);
     } else {
          /* collect local data before loading choices */
	  uilog_setprogress("Asking about collection...", 0.3, 0);
          uicollect_askclockwork();

	  /* configure uichoice, specifically load previous routes */
	  uilog_setprogress("Loading my choices...", 0.6, 0);
	  uichoice_configure(iiab_cf);

	  uilog_setprogress("Welcome to Habitat...", 0.8, 0);
     }

     /* expand the choice tree now it is fully populated */
     uichoice_init_expand();
     uilog_setprogress("Welcome to Habitat", 0.0, 0);

     /* allow five seconds to look at the splash screen, then change the 
      * view if the local clockwork is running */
     g_timeout_add_seconds(5, uidata_choice_change_to_local, NULL);

     /* Start main loop */
     gtk_main();

     /* Shutting down, save all settings */
     uilog_setprogress("Shutting down...", 0.2, 0);
     uichoice_cfsave(iiab_cf);
     uilog_setprogress("Shutting down...", 0.4, 0);
     iiab_usercfsave(iiab_cf, UICHOICE_CF_MYFILES_LOAD);
     iiab_usercfsave(iiab_cf, UICHOICE_CF_MYFILES_HIST);
     iiab_usercfsave(iiab_cf, UICHOICE_CF_MYHOSTS_LOAD);
     iiab_usercfsave(iiab_cf, UICHOICE_CF_MYHOSTS_HIST);
     uilog_setprogress("Shutting down...", 0.6, 0);

     uiedit_fini();
     uiabout_fini();
     harv_fini();
     uigraph_fini();
     uidata_fini();
     uivis_fini();
     uichoice_fini();
     rcache_fini();
     uipref_fini();

     iiab_stop();

     /* Destroy builder, since we don't need it anymore */
     g_object_unref( G_OBJECT( gui_builder ) );

     return 0;
}

/* callback to kill the app */
G_MODULE_EXPORT void 
on_myhabitat_win_destroy (GtkObject *object, gpointer user_data)
{
    gtk_main_quit ();
}


void complete_gui()
{
     GtkWidget *colour_this;
     GdkColor colour;

     /* Colour the background of the splash screen white. 
      * Chosen over style method */
     colour_this = get_widget("splash_eventbox");
     colour.red   = 65535;
     colour.green = 65535;
     colour.blue  = 65535;
     gtk_widget_modify_bg(colour_this, GTK_STATE_NORMAL, &colour);

     colour_this = get_widget("whatnext_eventbox");
     gtk_widget_modify_bg(colour_this, GTK_STATE_NORMAL, &colour);

     colour_this = get_widget("about_win");
     gtk_widget_modify_bg(colour_this, GTK_STATE_NORMAL, &colour);

     /* Disable repository buttons, which essentially disables it to us 
      * for the 2.0 Alpha series. It is planned to add Repositories in Beta */
     hide_widget("repository_status_btn");
     hide_widget("m_edit_harvest");
     hide_widget("m_edit_repository");
     /* Disable developer buttons */
     hide_widget("m_dev_pulldown");
     /* Disable preference tabs */
     hide_widget("pref_set2_scroll");
     hide_widget("pref_set3_scroll");

#if __APPLE_CC__
     /* Disable button based on platform -- Mac can't collect yet */
     hide_widget("clockwork_status_btn");
#endif
}
