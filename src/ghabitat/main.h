/*
 * main.c for the ghabitat graphical visualisation tool.
 * Nigel Stuckey
 * Copyright System Garden 1999-2001. All rights reserved.
 */

/* Nigel Stuckey, 18 Sept 1999 */

#include <gtk/gtk.h>
#include "gmcgraph.h"

#define CMDLN_OPTS           "s"
#define CMDLN_USAGE          "[-s]\n"\
"where -s          safe mode: don't autoload data"
#define AUTOCLOCKWORK_CFNAME "clockwork.auto"
#define DONTASKCLOCKWORK_CFNAME  "clockwork.dontask"
#define HELP_BUILT_PATH  "/help/"
#define HELP_DEV_PATH    "/../help/html/"
#define HELP_IMPORT      "import.html"
#define HELP_EXPORT      "export.html"
#define HELP_DATA_APP    "data_app.html"
#define HELP_DATA_EMAIL  "data_email.html"
#define HELP_DATA_SAVE   "data_save.html"
#define HELP_OPEN_HOST   "open_host.html"
#define HELP_OPEN_ROUTE  "open_route.html"
#define HELP_README      "../../README"
#define WEB_SYSGAR       "http://www.systemgarden.com/"
#define WEB_USAGE        "http://www.systemgarden.com/habitat/docs/user/"
#define WEB_HABITAT      "http://www.systemgarden.com/habitat/"
#define WEB_HARVEST      "http://www.systemgarden.com/harvest/"
#define MAN_BUILT_PATH   "/html/"
#define MAN_DEV_PATH     "/../html/"
#define MAN_GHABITAT     "man1/ghabitat.1.html"
#define MAN_CLOCKWORK    "man1/clockwork.1.html"
#define MAN_HABGET       "man1/habget.1.html"
#define MAN_HABPUT       "man1/habput.1.html"
#define MAN_CONFIG       "man5/config.5.html"

/* Widgets instantiated elsewhere that need to be global */
extern GtkWidget    *baseWindow;
extern GtkWidget    *import_window;
extern GtkWidget    *export_window;
extern GtkWidget    *data_app_window;
extern GtkWidget    *data_save_window;
extern GtkWidget    *data_email_window;
extern GtkWidget    *open_host_window;
extern GtkWidget    *open_route_window;
extern GtkWidget    *file_close_dialog;	/* close holstore dialog */
extern GtkWidget    *file_import_select;/* import file selection widget */
extern GtkWidget    *file_export_select;/* export file selection widget */
extern GtkWidget    *menugraph;	/* menu drop-down containing graph ops */
extern GtkWidget    *save_viewed_data;	/* menu item when dispaying data */
extern GtkWidget    *send_data_to_app;	/* menu item when dispaying data */
extern GtkWidget    *send_data_to_email; /* menu item when dispaying data */
extern GtkWidget    *tree;
extern GtkTooltips  *tooltips;
extern GtkWidget    *tableframe;
extern GtkWidget    *tablescroll;
extern GtkWidget    *panes;
extern GtkWidget    *graphframe;
extern GtkWidget    *graphpanes;
extern GMCGRAPH     *graph;	   /* gtkplot canvas */
extern GtkWidget    *ctlframe;	   /* frame containing graph control buttons */
extern GtkWidget    *listpanes;	   /* dividing panes between inst and attr */
extern GtkWidget    *instanceframe;/* frame containing instance widgets */
extern GtkWidget    *instanceview; /* viewport containing instance buttons */
extern GtkWidget    *attributeview;/* viewport containing graph attr buttons */
extern GtkWidget    *splash_view;
extern GtkWidget    *tableframe;/* frame containing table & curves */
extern GtkWidget    *edtreeframe;
extern GtkWidget    *edtree;	/* editable tree */
extern GtkWidget    *messagebar;/* for elog messages from iiab components */
extern GtkWidget    *progressbar;/* for gui specific messages */
extern GtkWidget    *about_window;

/* Widgets instantiated locally that need to be global */
extern GtkWidget    *file_open_window;
extern int          show_rulers;
extern int          show_axis;
extern int          view_histogram;
extern GdkCursor    *mouse_pointer_wait;
extern GdkCursor    *mouse_pointer_normal;

/*?*/
