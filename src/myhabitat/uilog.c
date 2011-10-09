/*
 * GHabitat Gtk GUI log handling, implementing an IIAB Route driver, primarily
 * used for elog messages to be added to the GUI, status bar and progress bar
 *
 * Nigel Stuckey, June 2004, May 2010
 * Copyright System Garden Ltd 2004,2010. All rights reserved
 */

#define _GNU_SOURCE

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "../iiab/nmalloc.h"
#include "../iiab/route.h"
#include "../iiab/elog.h"
#include "../iiab/util.h"
#include "uilog.h"
#include "main.h"


/* Initialise uilog GUI, building components not taken care of by Glade
 * and GtkBuilder. Thus, should be run after these components have run.
 */
void uilog_init() {
     GtkListStore *logstore;

     /* grab log store and clear it */
     logstore = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						      "log_liststore"));
     gtk_list_store_clear(logstore);

     GtkTreeView *log_table;
     GtkTreeViewColumn *func_col, *file_col, *line_col;

     log_table = GTK_TREE_VIEW(get_widget("log_table"));
     func_col = gtk_tree_view_get_column(log_table, UILOG_COL_FUNCTION);
     file_col = gtk_tree_view_get_column(log_table, UILOG_COL_FILE);
     line_col = gtk_tree_view_get_column(log_table, UILOG_COL_LINE);
     gtk_tree_view_column_set_visible(func_col, FALSE);
     gtk_tree_view_column_set_visible(file_col, FALSE);
     gtk_tree_view_column_set_visible(line_col, FALSE);
}


/* IIAB Route driver, built to parse elog format messages and write them 
 * to the GUI. Will also raise alert windows (potentially modal) on certain
 * severities. Only handles the elog format declared on initialisation */
void uilog_elog_raise(const char *errtext, 	/* text containing error */
		      int etlen			/* length of error text */ )
{
     char *errtext_dup, *tok_helper;
     char ecode, *esev, *efile, *efunc, *eline, *etext;
     time_t etime;
     guint contextid;
     GtkStatusbar *messagebar;
     GtkListStore *log_liststore;
     GtkTreeIter log_iter;

     /* in main(), we declared the message format to be:-
      *
      *       e|time|severity|file|function|line|text
      *
      * where e is the error character: d, i, w, e, f.
      */

     /* make a copy of the error text so we can patch it to buggery */
     errtext_dup =  xnstrdup(errtext);
     if (errtext_dup[etlen-1] == '\n')
          errtext_dup[etlen-1] = '\0';

     /* isolate the components of the error string */
     ecode = *strtok_r(errtext_dup, "|", &tok_helper);
     etime = strtol(strtok_r(NULL, "|", &tok_helper), (char**)NULL, 10);
     esev  = strtok_r(NULL, "|", &tok_helper);
     efile = strtok_r(NULL, "|", &tok_helper);
     efunc = strtok_r(NULL, "|", &tok_helper);
     eline = strtok_r(NULL, "|", &tok_helper);
     etext = strtok_r(NULL, "|", &tok_helper);

     /* place error text into the Gtk GUI
      * 1. Push it onto the status bar
      * 2. Append to log_liststore, the list data model
      * 3. If we have a Fatal message, generate a GUI popup
      * 4. Set a timeout to eventually blank the status bar */

     /* 1. Push message onto the status bar, context id is 'iiab' */
     messagebar = GTK_STATUSBAR(gtk_builder_get_object(gui_builder, 
						       "status_bar"));
     contextid = gtk_statusbar_get_context_id (messagebar, "iiab");
     gtk_statusbar_push(messagebar, contextid, etext);

     /* 2. Add to the log_liststore, the error log model */
     log_liststore = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
							   "log_liststore"));
     gtk_list_store_prepend(log_liststore, &log_iter);
     gtk_list_store_set(log_liststore, &log_iter,
			UILOG_COL_TIME,     util_decdatetime(etime),
			UILOG_COL_SEVERITY, esev,
			UILOG_COL_TEXT,     etext,
			UILOG_COL_FUNCTION, efunc,
			UILOG_COL_FILE,     efile,
			UILOG_COL_LINE,     eline,
			UILOG_COL_BG,       "red",
			UILOG_COL_FG,       "white",
			-1 );

     /* 3. If we have Fatal, we use this to generate a GUI popup in addition
      *    to the log */
     if (ecode == 'F')
          uilog_modal_alert("<big><b>Sorry</b></big>", etext);

     /* 4. Add time out to statusbar */
     g_timeout_add_seconds(7, uilog_clearstatus, NULL);

     /* free resources */
     nfree(errtext_dup);
}


/* display a modal alert message with headline and body text, both of which 
 * can contain markup. Waits for 'OK' to be clicked */
void uilog_modal_alert(char *primary, char *secondary, ... /* varargs */)
{
     GtkWidget *alert_win;
     va_list ap;
     char *text;

     alert_win = get_widget("alert_win");

     /* primary text */
     asprintf(&text, "<big><b>%s</b></big>", primary);
     gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(alert_win), text);
     free(text);

     /* secondary text */
     va_start(ap, secondary);
     vasprintf(&text, secondary, ap);
     gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(alert_win),
						text, NULL);
     free(text);
     va_end(ap);

     gtk_dialog_run (GTK_DIALOG (alert_win));
}


/* remove the text from the status bar */
gboolean uilog_clearstatus(gpointer data)
{
     guint contextid;
     GtkStatusbar *messagebar;

     messagebar = GTK_STATUSBAR(gtk_builder_get_object(gui_builder, 
						       "status_bar"));
     contextid = gtk_statusbar_get_context_id (messagebar, "iiab");
     gtk_statusbar_pop(messagebar, contextid);

     return 1;
}


/*
 * Set the text and percentage progress in the progress bar, used for
 * short term, non-logged status messages.
 * If text is NULL, no text changes take place but the percentage complete
 * value is used to update the progress bar.
 * If fraction is -1, then the completion bar is not updated, but the text is;
 *    the fraction should be between 0.0-1.0 (0%-100%). If below 0.0, then
 *    printed %'age would be 0%, above 100%, printed %'age will be 100% at most.
 * If showpercent is true, a % figure is appended to the text status with
 *    a %'age instead of a fraction
 */
void uilog_setprogress(char *text, double fraction, int showpercent)
{
     char ptext[100];
     GtkProgressBar *progressbar;
     GtkWidget *throbber;

     throbber = get_widget("throbber_img");
     progressbar = GTK_PROGRESS_BAR(gtk_builder_get_object(gui_builder, 
							   "progress_bar"));

     if (text) {
          if (showpercent) {
	       if (fraction >= 1.0)
		    snprintf(ptext, 100, "%s 100%%", text);
	       else if (fraction <= 0.0)
		      snprintf(ptext, 100, "%s 0%%", text);
	       else
		    snprintf(ptext, 100, "%s %.1f%%", text, fraction*100);
               gtk_progress_bar_set_text (progressbar, ptext);
          } else {
               gtk_progress_bar_set_text (progressbar, text);
	  }
     }

     if (fraction >= 0.0 && fraction <= 1.0)
          gtk_progress_bar_set_fraction (progressbar, fraction);

     if (fraction > 0.1)
	  gtk_widget_show(throbber);
     else
	  gtk_widget_hide(throbber);

     /* update pending widgets */
     while (gtk_events_pending())
          gtk_main_iteration();
}


/* Clear the current progress level and message */
void uilog_clearprogress()
{
     GtkProgressBar *progressbar;
     GtkWidget *throbber;

     throbber = get_widget("throbber_img");
     progressbar = GTK_PROGRESS_BAR(gtk_builder_get_object(gui_builder, 
							   "progress_bar"));
     gtk_progress_bar_set_text (progressbar, "");
     gtk_progress_bar_set_fraction (progressbar, 0.0);
     gtk_widget_hide(throbber);
}


/* callback to view an individual log entry, called from the log tables */
G_MODULE_EXPORT void 
uilog_on_view_log_line (GtkTreeView        *treeview,
			GtkTreePath        *path,
			GtkTreeViewColumn  *col,
			gpointer            userdata)
{
     GtkTreeModel *model;
     GtkTreeIter   iter;

     /* get model from list */
     model = gtk_tree_view_get_model(treeview);

     /* get activated data line from model */
     if (gtk_tree_model_get_iter(model, &iter, path)) {
          gchar *time, *severity, *message, *function, *file, *line;
          GtkWidget *timew, *severityw, *functionw, *filew, *linew;
	  GtkWidget *logline_win, *messagew;

	  /* grab the data from the model's activated line */
	  gtk_tree_model_get(model, &iter, 
			     UILOG_COL_TIME,     &time,
			     UILOG_COL_SEVERITY, &severity,
			     UILOG_COL_TEXT,     &message,
			     UILOG_COL_FUNCTION, &function,
			     UILOG_COL_FILE,     &file,
			     UILOG_COL_LINE,     &line,
			     -1);

	  /* grab the logline's value widgets*/
	  timew     = get_widget("logline_timestamp_value");
	  severityw = get_widget("logline_severity_value");
	  functionw = get_widget("logline_function_value");
	  filew     = get_widget("logline_file_value");
	  linew     = get_widget("logline_line_value");
	  messagew  = get_widget("logline_message_value");

	  /* now set them with the activated data, overwriting old vals */
	  gtk_label_set_text(GTK_LABEL(timew),     time);
	  gtk_label_set_text(GTK_LABEL(severityw), severity);
	  gtk_label_set_text(GTK_LABEL(functionw), function);
	  gtk_label_set_text(GTK_LABEL(filew),     file);
	  gtk_label_set_text(GTK_LABEL(linew),     line);
	  gtk_label_set_text(GTK_LABEL(messagew),  message);

	  /* clear up */
	  g_free(time);
	  g_free(severity);
	  g_free(message);
	  g_free(function);
	  g_free(file);
	  g_free(line);

	  /* finally, show the data */
	  logline_win = get_widget("logline_win");
	  gtk_window_present(GTK_WINDOW(logline_win));
     }
}


/* callback to view an individual log entry, called from the log tables */
G_MODULE_EXPORT void 
uilog_on_collect_change  (GtkObject *object, gpointer user_data)
{
     const gchar *label;

     if ( ! gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(object)) )
          return;	/* button is up, do nothing */

     label = gtk_button_get_label(GTK_BUTTON(object));
     if (strcmp((char *)label, "Normal") == 0) {
          /* Normal message mode collects INFO and above */
	  elog_setsevpurl(DEBUG, "none:");
	  elog_setsevpurl(DIAG,  "none:");
	  elog_printf(INFO, "Collecting normal messages");
     } else if (strcmp(label, "Diagnostic") == 0) {
          /* Diag message mode collects DIAG and above */
	  elog_setsevpurl(DEBUG, "none:");
	  elog_setsevpurl(DIAG,  "gtkgui:");
	  elog_printf(INFO, "Collecting diagnostic messages");
     } else if (strcmp(label, "Debug") == 0) {
          /* Normal message mode collects INFO and above */
	  elog_setsevpurl(DEBUG, "gtkgui:");
	  elog_setsevpurl(DIAG,  "gtkgui:");
	  elog_printf(INFO, "Collecting debug messages");
     }
}


/* Add or remove the three expert location columns in the log table */
G_MODULE_EXPORT void 
uilog_on_table_expert (GtkObject *object, gpointer user_data)
{
     GtkTreeView *log_table;
     GtkTreeViewColumn *func_col, *file_col, *line_col;
     int vis=0;		/* default is invisible */

     if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(object)) )
          vis=1;	/* button is off, make visible */

     log_table = GTK_TREE_VIEW(get_widget("log_table"));
     func_col = gtk_tree_view_get_column(log_table, UILOG_COL_FUNCTION);
     file_col = gtk_tree_view_get_column(log_table, UILOG_COL_FILE);
     line_col = gtk_tree_view_get_column(log_table, UILOG_COL_LINE);
     gtk_tree_view_column_set_visible(func_col, vis);
     gtk_tree_view_column_set_visible(file_col, vis);
     gtk_tree_view_column_set_visible(line_col, vis);
}


/* Clear the current log messages in the store */
G_MODULE_EXPORT void 
uilog_on_table_clear (GtkObject *object, gpointer user_data)
{
     GtkListStore *logstore;

     /* grab log store and clear it */
     logstore = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						      "log_liststore"));
     gtk_list_store_clear(logstore);
}


/* Add or remove the three expert location fields in the log line window */
G_MODULE_EXPORT void 
uilog_on_logline_expert (GtkObject *object, gpointer user_data)
{
     GtkWidget *logline_expert_frame;

     logline_expert_frame = get_widget("logline_expert_frame");

     if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(object)) ) {
          gtk_widget_show(logline_expert_frame);
     } else {
          gtk_widget_hide(logline_expert_frame);
     }
}


