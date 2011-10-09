/*
 * GHabitat Gtk GUI log handling, implementing an IIAB Route driver, primarily
 * used for elog messages to be added to the GUI, status bar and progress bar
 *
 * Nigel Stuckey, June 2004, May 2010
 * Copyright System Garden Ltd 2004,2010. All rights reserved
 */

#ifndef _UILOG_H_
#define _UILOG_H_

#include <gtk/gtk.h>


/* Column definitions for log message list */
enum {
     UILOG_COL_TIME=0,
     UILOG_COL_SEVERITY,
     UILOG_COL_TEXT,
     UILOG_COL_FUNCTION,
     UILOG_COL_FILE,
     UILOG_COL_LINE,
     UILOG_COL_BG,
     UILOG_COL_FG,
     UILOG_COL_EOL
};

void uilog_init();
void uilog_elog_raise(const char *errtext,int etlen);
void uilog_modal_alert(char *primary, char *secondary, ...);
gboolean uilog_clearstatus(gpointer data);
void uilog_setprogress  (char *text, double fraction, int showpercent);
void uilog_clearprogress();
void uilog_on_view_log_line  (GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *,
			      gpointer);
void uilog_on_collect_change (GtkObject *object, gpointer user_data);
void uilog_on_table_expert   (GtkObject *object, gpointer user_data);
void uilog_on_table_clear    (GtkObject *object, gpointer user_data);
void uilog_on_logline_expert (GtkObject *object, gpointer user_data);

#endif /* _UILOG_H_ */
