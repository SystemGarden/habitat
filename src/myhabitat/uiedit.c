/*
 * MyHabitat Gtk GUI Harvest handling, implementing the callbacks and
 * functions for editing. This is a thin layer over a standard widget
 *
 * Nigel Stuckey, December 2010
 * Copyright System Garden Ltd 2004,2010. All rights reserved
 */

#include <time.h>
#include <string.h>
#include <gtk/gtk.h>
#include "../iiab/elog.h"
#include "../iiab/util.h"
#include "uiedit.h"
#include "uidialog.h"
#include "uilog.h"
#include "main.h"


/* Initialise UIEdit class in MyHarvest */
void uiedit_init() {
}

void uiedit_fini() {
}


/* Edit a file from a path */
GtkWidget * uiedit_load_file(char *filepath)
{
     return NULL;
}


/* Edit a text object from a p-url ROUTE */
void uiedit_load_route(char *purl, char *artifact)
{
     GtkWidget *edit_win, *edit_size_value, *edit_file_value, 
       *edit_artifact_name, *edit_textview, *edit_readonly_label;
     GtkTextBuffer *edit_textbuffer;
     char *buffer, *nlines_str;
     int length, nlines, readonly;
     PangoFontDescription *font_desc;

     g_print("uiedit_load_route\n");

     /* check for read only */
     edit_readonly_label = get_widget("edit_readonly_label");
     readonly = !route_access(purl, NULL, ROUTE_WRITEOK);
     if (readonly)
          gtk_widget_show(edit_readonly_label);
     else
          gtk_widget_hide(edit_readonly_label);

     /* check it exists at all */
     if ( ! route_access(purl, NULL, ROUTE_READOK))
          uilog_modal_alert("File does not yet exist", 
			    "The file %s does not yet exist and you will "
			    "receive an empty window",
			    purl);

     /* Attempt to read route */
     buffer = route_read(purl, NULL, &length);
     if ( ! buffer) {
          /* no route object or unable to read it */
          elog_printf(FATAL, "Unable to read %s", purl);
          return;
     }

     /* get UI refs */
     edit_win           = get_widget("edit_win");
     edit_textbuffer    = GTK_TEXT_BUFFER(gtk_builder_get_object
					  (gui_builder,"edit_textbuffer"));
     edit_textview      = get_widget("edit_textview");
     edit_size_value    = get_widget("edit_size_value");
     edit_file_value    = get_widget("edit_file_value");
     edit_artifact_name = get_widget("edit_artifact_name");

     /* set labels and buffers */
     gtk_text_buffer_set_text(edit_textbuffer, buffer, length);
     nlines = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(edit_textbuffer));
     nlines_str = util_i32toa(nlines);
     gtk_label_set_text(GTK_LABEL(edit_size_value), nlines_str);
     gtk_label_set_text(GTK_LABEL(edit_file_value), purl);
     gtk_label_set_text(GTK_LABEL(edit_artifact_name), artifact);

     /* set font */
     font_desc = pango_font_description_from_string ("monospace 8");
     gtk_widget_modify_font (edit_textview, font_desc);
     pango_font_description_free (font_desc);

     /* no modification */
     gtk_text_buffer_set_modified(edit_textbuffer, FALSE);

     show_window("edit_win");

     return;
}


/* Edit a text object from a NULL terminated memory buffer */
GtkWidget * uiedit_load_buffer(char *buffer)
{
     g_print("uiedit_buffer\n");
     return NULL;
}


GtkWidget * uiedit_set_read_only()
{
     return NULL;
}


/* callback for the edit revert button */
G_MODULE_EXPORT void 
uiedit_on_revert (GtkObject *object, gpointer user_data)
{
     GtkWidget *edit_file_value, *edit_artifact_name;
     GtkTextBuffer *edit_textbuffer;
     const gchar *filename, *artifactname;

     g_print("uiedit_on_revert\n");

     /* get UI fields */
     edit_file_value = get_widget("edit_file_value");
     edit_artifact_name = get_widget("edit_artifact_name");
     filename = gtk_label_get_text(GTK_LABEL(edit_file_value));
     artifactname = gtk_label_get_text(GTK_LABEL(edit_artifact_name));

     /* check for modified */
     edit_textbuffer = GTK_TEXT_BUFFER(gtk_builder_get_object
				       (gui_builder,"edit_textbuffer"));
     if (gtk_text_buffer_get_modified(edit_textbuffer)) {
          /* ask to lose edits */
          if ( ! uidialog_yes_or_no("edit_win", "Really Revert Configuration?",
				    "Do you really want to revert to the "
				    "current configuration and lose any edits "
				    "you have made?") )
	       return;
     }

     /* call to reread */
     uiedit_load_route((char *) filename, (char *) artifactname);

     return;
}


/* callback for the edit save button */
G_MODULE_EXPORT void 
uiedit_on_save (GtkObject *object, gpointer user_data)
{
     GtkWidget *edit_win, *edit_file_value;
     GtkTextBuffer *edit_textbuffer;
     GtkTextIter start, end;
     char *buffer, *purl;
     ROUTE file;
     int r;

     g_print("uiedit_on_save\n");

     /* get UI refs */
     edit_win        = get_widget("edit_win");
     edit_textbuffer = GTK_TEXT_BUFFER(gtk_builder_get_object
				       (gui_builder,"edit_textbuffer"));
     edit_file_value = get_widget("edit_file_value");

     /* check that the buffer has been modified and needs writing */
     if ( ! gtk_text_buffer_get_modified(edit_textbuffer) )
          return;

#if 0
     /* get labels and buffers */
     nlines = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(edit_textbuffer));
     nlines_str = util_i32toa(nlines);
     gtk_label_set_text(GTK_LABEL(edit_size_value), nlines_str);
#endif

     /* Attempt to open route */
     purl = gtk_label_get_text(GTK_LABEL(edit_file_value));
     file = route_open(purl, NULL, NULL, 1 /* its a file - no history */);
     if ( ! file ) {
          /* no route object or unable to read it */
          elog_printf(FATAL, "Unable to open %s for writing. "
		      "Check permissions", purl);
          return;
     }

     /* get buffer text */
     gtk_text_buffer_get_start_iter(edit_textbuffer, &start);
     gtk_text_buffer_get_end_iter(edit_textbuffer, &end);
     buffer = gtk_text_buffer_get_text(edit_textbuffer, &start, &end, FALSE);

     /* Attempt to write to route */
     r = route_write(file, buffer, strlen(buffer));
     route_close(file);
     g_free(buffer);
     if (r == -1) {
          /* no route object or unable to read it */
          elog_printf(FATAL, "Unable to write %s. Check permissions", purl);
          return;
     }

     /* clear modification */
     gtk_text_buffer_set_modified(edit_textbuffer, FALSE);

     gtk_widget_hide(edit_win);
}


/* callback for cancel button */
G_MODULE_EXPORT void 
uiedit_on_cancel (GtkObject *object, gpointer user_data)
{
     GtkWidget *edit_win;
     GtkTextBuffer *edit_textbuffer;

     g_print("uiedit_on_cancel\n");

     /* check for modified */
     edit_textbuffer = GTK_TEXT_BUFFER(gtk_builder_get_object
				       (gui_builder,"edit_textbuffer"));
     if (gtk_text_buffer_get_modified(edit_textbuffer)) {
          /* ask to lose edits */
          if ( ! uidialog_yes_or_no("edit_win", "Really Cancel Edits?",
				    "Do you really want to cancel and lose "
				    "any edits you have made?") )
	       return;       
     }

     edit_win = get_widget("edit_win");
     gtk_widget_hide(edit_win);
}


/* callback for modification signal */
G_MODULE_EXPORT void 
uiedit_on_modified (GtkObject *object, gpointer user_data)
{
     GtkWidget *edit_status_label;
     GtkTextBuffer *edit_textbuffer;

     g_print("uiedit_on_modified\n");

     edit_status_label = get_widget("edit_status_label");

     /* check for modified */
     edit_textbuffer = GTK_TEXT_BUFFER(gtk_builder_get_object
				       (gui_builder,"edit_textbuffer"));
     if (gtk_text_buffer_get_modified(edit_textbuffer)) {
          /* set modified flag */
          gtk_widget_show(edit_status_label);
     } else {
          /* reset modified flag */
          gtk_widget_hide(edit_status_label);
     }
}


/* callback on certain keyboard actions to recount lines */
G_MODULE_EXPORT void 
uiedit_on_recount_lines (GtkObject *object, gpointer user_data)
{
     GtkWidget *edit_size_value;
     GtkTextBuffer *edit_textbuffer;
     int nlines;
     char *nlines_str;

     g_print("uiedit_on_recount_lines\n");

     /* get UI refs */
     edit_textbuffer = GTK_TEXT_BUFFER(gtk_builder_get_object
				       (gui_builder,"edit_textbuffer"));
     edit_size_value = get_widget("edit_size_value");

     /* get labels and buffers */
     nlines = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(edit_textbuffer));
     nlines_str = util_i32toa(nlines);
     gtk_label_set_text(GTK_LABEL(edit_size_value), nlines_str);
}
