/*
 * MyHabitat Gtk GUI editing.
 *
 * Nigel Stuckey, December 2010
 * Copyright System Garden Ltd 2004,2010. All rights reserved
 */

#ifndef _UIEDIT_H_
#define _UIEDIT_H_

#include <gtk/gtk.h>

void uiedit_init();
void uiedit_fini();
GtkWidget * uiedit_load_file(char *filepath);
void uiedit_load_route(char *purl, char *artifact);
GtkWidget * uiedit_load_buffer(char *buffer);
GtkWidget * uiedit_set_read_only();
void uiedit_on_revert (GtkObject *object, gpointer user_data);
void uiedit_on_save (GtkObject *object, gpointer user_data);
void uiedit_on_cancel (GtkObject *object, gpointer user_data);
void uiedit_on_modified (GtkObject *object, gpointer user_data);

#endif /* _UIEDIT_H_ */
