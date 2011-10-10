/*
 * main.c for the myhabitat graphical visualisation tool.
 * Nigel Stuckey
 * Copyright System Garden 1999-2010. All rights reserved.
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <gtk/gtk.h>

#define CMDLN_OPTS           "s"
#define CMDLN_USAGE          "[-s]\n"\
"where -s          safe mode: don't autoload data"
#define AUTOCLOCKWORK_CFNAME     "clockwork.auto"
#define DONTASKCLOCKWORK_CFNAME  "clockwork.dontask"
#define DEFAULT_CURVES_CFNAME    "default.curves"
#define DEFAULT_INST_CFNAME      "default.inst"
#define DONTASKTOQUIT_CFNAME     "quit.dontask"
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
#define WEB_SYSGAR       "http://www.systemgarden.com"
#define WEB_USAGE        "http://www.systemgarden.com/habitat/docs/user"
#define WEB_HABITAT      "http://www.systemgarden.com/habitat"
#define WEB_HARVEST      "http://www.systemgarden.com/harvest"
#define WEB_WIKI         "http://wiki.systemgarden.com/index.php?title=Habitat"
#define MAN_BUILT_PATH   "/html/"
#define MAN_DEV_PATH     "/../html/"
#define MAN_MYHABITAT    "man1/myhabitat.1.html"
#define MAN_CLOCKWORK    "man1/clockwork.1.html"
#define MAN_HABGET       "man1/habget.1.html"
#define MAN_HABPUT       "man1/habput.1.html"
#define MAN_CONFIG       "man5/config.5.html"

/* Macros */
#define get_widget(name)   GTK_WIDGET(gtk_builder_get_object(gui_builder,name))
#define show_widget(name)  gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(gui_builder,name)))
#define hide_widget(name)  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui_builder,name)))
#define show_window(name)  gtk_window_present(GTK_WINDOW(gtk_builder_get_object(gui_builder,name)))

/* Widgets instantiated elsewhere that need to be global */
extern GtkBuilder   *gui_builder;

/* Types */
void complete_gui(void);

/* A nasty hack for GTK to work out window widths from the alloction. 
 * The method for doing so changes in Gtk 3.0 */
#if ! GTK_CHECK_VERSION( 2, 18, 0 )
  void gtk_widget_get_allocation(GtkWidget *, GtkAllocation *);
  #define gtk_widget_get_allocation( wi, al ) \
    if( ( al ) ) \
      if( GTK_IS_WIDGET( ( wi ) ) ) \
        *( al ) = GTK_WIDGET( ( wi ) )->allocation
#endif

#endif /* _MAIN_H_ */
