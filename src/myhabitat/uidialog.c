/*
 * MyHabitat Gtk GUI Harvest dialogues, implementing specific dilogue 
 * (dialog) windows
 *
 * Nigel Stuckey, February 2011
 * Copyright System Garden Ltd 2004,2011. All rights reserved
 */

#include <gtk/gtk.h>
#include "../iiab/util.h"
#include "uidialog.h"
#include "main.h"

/*
 * Modal dialog with yes or no buttons. 
 * Varargs allows the secondary to have printf style varargs. 
 * Returns UIDIALOG_YES (True or 1) or UIDIALOG_NO (False or 0) 
 */
int uidialog_yes_or_no (char *parent_window_name,
			char *primary_text, 
			char *secondary_text, ... /* varagrs */) {

     GtkWidget *askw, *parent_win;

     parent_win = get_widget(parent_window_name);

     askw = gtk_message_dialog_new(GTK_WINDOW(parent_win),
				   GTK_DIALOG_MODAL | 
				   GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_QUESTION,
				   GTK_BUTTONS_YES_NO,
				   primary_text, NULL);

     va_list ap;
     va_start(ap, secondary_text);
     gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(askw),
						secondary_text, ap);
     va_end(ap);
 
     gint result = gtk_dialog_run(GTK_DIALOG(askw));
     gtk_widget_destroy(askw);

     switch (result) {
     case GTK_RESPONSE_YES:
          return UIDIALOG_YES;
     default:
          return UIDIALOG_NO;
     }

     return UIDIALOG_NO;
}

