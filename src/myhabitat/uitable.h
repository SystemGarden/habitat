/*
 * Habitat table UI code, converting a TABLE into GtkListStore model
 * which is the viewed by GtkTreeView.
 *
 * Nigel Stuckey, September 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#ifndef _UITABLE_H_
#define _UITABLE_H_

#define _GNU_SOURCE

#include "../iiab/table.h"
#include <gtk/gtk.h>

GtkListStore *uitable_mkmodel  (TABLE tab, time_t view_min, time_t view_max);
void          uitable_freemodel(GtkListStore *);
GtkTreeView * uitable_mkview   (TABLE tab, GtkListStore *model);
void          uitable_freeview (GtkTreeView *view);
static gboolean uitable_cb_query_tooltip (GtkWidget  *widget,
					  gint        x,
					  gint        y,
					  gboolean    keyboard_tip,
					  GtkTooltip *tooltip,
					  gpointer    data);

#endif /* _UITABLE_H_ */
