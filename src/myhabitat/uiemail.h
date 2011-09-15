/*
 * GHabitat Gtk GUI email class to send data via email
 *
 * Nigel Stuckey, February 2011
 * Copyright System Garden Ltd 2011. All rights reserved
 */


#ifndef _UIEMAIL_H_
#define _UIEMAIL_H_

#include <gtk/gtk.h>

void uiconv_copy_on_source_location_set (GtkComboBox *object, gpointer user_data);
void uiconv_copy_on_source_file_set (GtkFileChooserButton *object,
				     gpointer user_data);
void uiconv_copy_on_source_name_set (GtkEntry *object, gpointer user_data);
void uiconv_copy_on_source_ring_set (GtkComboBox *object, gpointer user_data);
void uiconv_copy_on_source_clear (GtkButton *object, gpointer user_data);
void uiconv_copy_on_source_view (GtkButton *object, gpointer user_data);
void uiconv_copy_on_dest_location_set (GtkComboBox *object, gpointer user_data);
void uiconv_copy_on_dest_file_set (GtkFileChooserButton *object, gpointer user_data);
void uiconv_copy_on_dest_clear (GtkButton *object, gpointer user_data);
void uiconv_copy_on_copy (GtkButton *object, gpointer user_data);
void uiconv_copy_on_help (GtkButton *object, gpointer user_data);

#endif /* _UIEMAIL_H_ */
