/*
 * Habitat GUI time widgets
 *
 * Nigel Stuckey, August 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#ifndef _UITIME_H_
#define _UITIME_H_

#include <time.h>
#include <gtk/gtk.h>

#define UITIME_INITIAL_RANGE 86400 /* 1 days */ /*604800*/ /* 7 days */

void uitime_forget_data();
void uitime_set_slider(time_t from_t, time_t to_t, time_t openage_t);
gchar* uitime_on_slider_format_value (GtkScale *scale, gdouble value);
void uitime_on_slider_value_changed (GtkRange *slider, gpointer user_data);
void uitime_prevent_slider_reload();
void uitime_allow_slider_reload();
void uitime_slider_change(time_t slider_from, time_t slider_to);
void uitime_on_bounds_win (GtkObject *object, gpointer user_data);
void uitime_on_bounds_set (GtkObject *object, gpointer user_data);
void uitime_on_data_everything (GtkObject *object, gpointer user_data);

#endif /* _UITIME_H_ */
