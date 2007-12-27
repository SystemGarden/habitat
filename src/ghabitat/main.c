/*
 * main.c for the ghabitat graphical visualisation tool.
 * Nigel Stuckey
 * Copyright System Garden 1999-2001. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "interface.h"
#include "support.h"

#include "rt_gtkgui.h"
#include "uidata.h"
#include "uichoice.h"
#include "ghchoice.h"
#include "gtkaction.h"
#include "gmcgraph.h"
#include "main.h"
#include "../iiab/route.h"
#include "../iiab/elog.h"
#include "../iiab/holstore.h"
#include "../iiab/iiab.h"
#include "../iiab/util.h"

/* configuration defaults in two phases: the first just relies on
 * stderr, the second uses the GUI once enough facilities have been
 * set up. The GUI config can either be info and above or debug and above */
char *cfdefaults = "elog.all			none:\n"
		   "elog.above info		stderr:\n"
		   "nmalloc			0\n"
		   AUTOCLOCKWORK_CFNAME "	0\n"
		   GTKACTION_CF_CURVES "        pc_idle pc_nice pc_system "
                                              " pc_user pc_wait pc_work "
                                              " pc_used "
                                              " rx_pkts tx_pkts ";

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

/* Widgets instantiated elsewhere that need to be global */
GtkWidget    *baseWindow;
GtkWidget    *import_window;
GtkWidget    *export_window;
GtkWidget    *data_app_window;
GtkWidget    *data_save_window;
GtkWidget    *data_email_window;
GtkWidget    *open_host_window;
GtkWidget    *open_route_window;
GtkWidget    *file_close_dialog;	/* close holstore dialog */
GtkWidget    *file_import_select;	/* import file selection widget */
GtkWidget    *file_export_select;	/* export file selection widget */
GtkWidget    *menugraph;	/* menu drop-down containing graph ops */
GtkWidget    *menudata;		/* menu drop-down containing graph ops */
GtkWidget    *save_viewed_data;	/* menu item when dispaying data */
GtkWidget    *send_data_to_app;	/* menu item when dispaying data */
GtkWidget    *send_data_to_email; /* menu item when dispaying data */
GtkWidget    *tree;
GtkTooltips  *tooltips;
GtkWidget    *tableframe;
GtkWidget    *tablescroll;
GtkWidget    *panes;
GtkWidget    *graphframe;
GtkWidget    *graphpanes;
GMCGRAPH     *graph;		/* gtkplot canvas */
GtkWidget    *ctlframe;		/* frame containing graph control buttons */
GtkWidget    *listpanes;	/* dividing panes between inst and attrib */
GtkWidget    *instanceframe;	/* frame containing instance widgets */
GtkWidget    *instanceview;	/* viewport containing instance buttons */
GtkWidget    *attributeview;	/* viewport containing graph attr buttons */
GtkWidget    *splash_view;
GtkWidget    *tableframe;	/* frame containing table & curves */
GtkWidget    *edtreeframe;
GtkWidget    *edtree;		/* editable tree */
GtkWidget    *messagebar;	/* for elog messages from iiab components */
GtkWidget    *progressbar;	/* for gui specific short term progress */
GtkWidget    *about_window;

/* Widgets instantiated locally that need to be global */
GtkWidget    *file_open_window;
int          show_rulers;
int          show_axis;
int          view_histogram;
GdkCursor    *mouse_pointer_wait;
GdkCursor    *mouse_pointer_normal;

/* Misc */


int
main (int argc, char *argv[])
{
     char *default_file;
     ITREE *top;
     GtkCTreeNode *guitop;

     /* initialise config table and load the defaults */
     iiab_start(CMDLN_OPTS, argc, argv, CMDLN_USAGE, cfdefaults);
     uichoice_init(iiab_cf);
     ghchoice_init(iiab_cf);
     uidata_init(iiab_cf);

     /* add style locations */
     default_file = util_strjoin(iiab_dir_lib, "/ghabitat.rc", NULL);
     gtk_rc_add_default_file(default_file);		/* production */
     gtk_rc_add_default_file("ghabitat.rc");		/* development */

     /* initialise gui toolkit */
     gtk_set_locale ();
     gtk_init (&argc, &argv);

     /* point to where the pixmaps mmay be found during runtime */
     add_pixmap_directory ("pixmaps");
     add_pixmap_directory ("../pixmaps");

#if 0
     s = gtk_rc_get_default_files();
     while (*s) {
	  printf("rc file: %s\n", *s);
	  s++;
     }
#endif

     /*
      * The following code was added by Glade to create one of each component
      * (except popup menus), just so that you see something after building
      * the project. Delete any components that you don't want shown initially.
      */
     baseWindow = create_baseWindow ();
     gtk_widget_show (baseWindow);
     file_open_window = create_file_open_select ();

     /* discover referencies to globally used widgets for speed */
     menugraph    = lookup_widget(baseWindow, "menugraph");
     save_viewed_data  = lookup_widget(baseWindow, "save_viewed_data");
     send_data_to_app  = lookup_widget(baseWindow, "send_data_to_application");
     send_data_to_email= lookup_widget(baseWindow, "send_data_to_email");
     tree         = lookup_widget(baseWindow, "tree");
     tooltips     = GTK_TOOLTIPS(lookup_widget(baseWindow, "tooltips"));
     tableframe   = lookup_widget(baseWindow, "tableframe");
     tablescroll  = lookup_widget(baseWindow, "tablescroll");
     panes        = lookup_widget(baseWindow, "panes");
     graphframe   = lookup_widget(baseWindow, "graphframe");
     graphpanes   = lookup_widget(baseWindow, "graphpanes");
     /*graph        = lookup_widget(baseWindow, "graph");*/
     ctlframe     = lookup_widget(baseWindow, "ctlframe");
     listpanes    = lookup_widget(baseWindow, "listpanes");
     instanceframe= lookup_widget(baseWindow, "instanceframe");
     instanceview = lookup_widget(baseWindow, "instanceview");
     attributeview= lookup_widget(baseWindow, "attributeview");
     splash_view  = lookup_widget(baseWindow, "splash_view");
     tableframe   = lookup_widget(baseWindow, "tableframe");
     edtreeframe  = lookup_widget(baseWindow, "edtreeframe");
     edtree       = lookup_widget(baseWindow, "edtree");
     messagebar   = lookup_widget(baseWindow, "messagebar");
     progressbar  = lookup_widget(baseWindow, "progressbar");
     show_rulers  = 1;
     show_axis    = 1;
     view_histogram = 0;

     /* set up waiting mouse pointer */
     mouse_pointer_wait = gdk_cursor_new(GDK_WATCH);
     mouse_pointer_normal = gdk_cursor_new(GDK_TOP_LEFT_ARROW);

     /* initialise & configure gtkaction and prepare the choice tree */
     gtkaction_init();
     gtkaction_setprogress("starting up...", 0.2, 0);
     gtkaction_configure(iiab_cf);
     top = uichoice_gettopnodes();
     itree_traverse(top) {
	  guitop = gtkaction_makechoice(NULL, itree_get(top), tooltips);
	  gtkaction_expandchoice(guitop, GTKACTION_NTREELEV, tooltips);
	  gtk_ctree_expand_to_depth(GTK_CTREE(tree), guitop, 
				    GTKACTION_NTREELEV+1);
	  /*gtk_ctree_expand_to_depth(GTK_CTREE(tree), guitop, 
	    GTKACTION_NTREELEV);*/
	  /*gtkaction_expandlist(guitop, GTKACTION_NTREELEV, tooltips);*/
     }

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

     if (cf_defined(iiab_cf, "s") ) {
	  gtkaction_setprogress("safe start", 0.0, 0);
     } else {
	  /* configure uichoice, specifically load previous routes */
	  gtkaction_setprogress("loading my files...", 0.3, 0);
	  ghchoice_configure(iiab_cf);
	  gtkaction_choice_sync(GTK_CTREE(tree), "my files");
	  gtkaction_setprogress("loading my hosts...", 0.4, 0);
	  gtkaction_choice_sync(GTK_CTREE(tree), "my hosts");
	  gtkaction_setprogress("loading repository...", 0.5, 0);
	  gtkaction_choice_sync(GTK_CTREE(tree), "repository");

	  /* do something useful: provide an initial data display */
	  gtkaction_setprogress("welcome to habitat...", 0.8, 0);
	  gtkaction_gotochoice(ghchoice_initialview(), 0);
	  gtkaction_setprogress("welcome to habitat", 0.0, 0);
     }

     /* collect local data */
     gtkaction_askclockwork();

     gtkaction_choice_update_start();

     gtk_main ();

     nfree(default_file);
     gtkaction_choice_update_stop();
     gtkaction_setprogress("shutting down...", 0.2, 0);
     gmcgraph_fini(graph);	/* initialisation in create_baseWindow() */
     gtkaction_setprogress("shutting down...", 0.4, 0);
     gtkaction_fini();
     ghchoice_cfsave(iiab_cf);
     iiab_usercfsave(iiab_cf, GHCHOICE_CF_MYFILES_LOAD);
     iiab_usercfsave(iiab_cf, GHCHOICE_CF_MYFILES_LIST);
     iiab_usercfsave(iiab_cf, GHCHOICE_CF_MYHOSTS_LOAD);
     iiab_usercfsave(iiab_cf, GHCHOICE_CF_MYHOSTS_LIST);
     gtkaction_setprogress("shutting down...", 0.6, 0);
     ghchoice_fini();
     uichoice_fini();
     uidata_fini();
     iiab_stop();

     return 0;
}

