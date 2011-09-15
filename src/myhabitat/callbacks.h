/*
 * Callbacks include file for the myhabitat graphical visualisation tool.
 * Nigel Stuckey, December 2010
 * Copyright System Garden 1999-2010. All rights reserved.
 */

#ifndef _CALLBACKS_H_
#define _CALLBACKS_H_

#include <gtk/gtk.h>

void on_view_choices (GtkObject *object, gpointer user_data);
void on_view_toolbar (GtkObject *object, gpointer user_data);
void on_view_curves (GtkObject *object, gpointer user_data);
void on_edit_habrc (GtkObject *object, gpointer user_data);
void on_edit_jobs (GtkObject *object, gpointer user_data);
void on_edit_harvest (GtkObject *object, gpointer user_data);

#endif /* _CALLBACKS_H_ */
