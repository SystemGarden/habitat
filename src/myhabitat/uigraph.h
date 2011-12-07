/*
 * Gtk multicurve graph widget
 * This is a wrapper over the gtkplot for convenience
 *
 * Nigel Stuckey, September 1999, October 2010
 * Copyright System Garden Limited 1999-2001. All rights reserved.
 */

#ifndef _UIGRAPH_H_
#define _UIGRAPH_H_

#include <gtk/gtk.h>
#include <gtkdatabox.h>
#include "../iiab/itree.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"
#include "uidata.h"

/* instance column names */
enum uigraph_inst_cols {
     UIGRAPH_INST_ICON=0,
     UIGRAPH_INST_INSTNAME,
     UIGRAPH_INST_LABEL,
     UIGRAPH_INST_TOOLTIP,
     UIGRAPH_INST_ACTIVE,
     UIGRAPH_INST_EOL
};

/* attribute column names or curves */
enum uigraph_curve_cols {
     UIGRAPH_CURVE_ICON=0,
     UIGRAPH_CURVE_SPARKLINE,
     UIGRAPH_CURVE_TOOLTIP,
     UIGRAPH_CURVE_COLNAME,
     UIGRAPH_CURVE_ACTIVE,
     UIGRAPH_CURVE_LABEL,
     UIGRAPH_CURVE_COLOUR,
     UIGRAPH_CURVE_SCALE,
     UIGRAPH_CURVE_OFFSET,
     UIGRAPH_CURVE_POSSIBLEMAX,
     UIGRAPH_CURVE_EOL
};

/* establish and set up stateful data */
void uigraph_init();
void uigraph_fini();
void uigraph_set_inst_hint(ITREE *);
void uigraph_set_curve_hint(ITREE *);
void uigraph_data_load(TABLE tab);
void uigraph_set_timebase(time_t oldest, time_t youngest);
void uigraph_data_update_redraw(TABLE tab);
void uigraph_data_unload();

void uigraph_drawgraph(char *instance	/* graph name (instance) */ );
void uigraph_rm_graph(char *instance);
void uigraph_rm_all_graphs();
GdkColor *uigraph_drawcurve(char *curve,	/* curve name */
			    float scale,	/* curve scale/magnitude */
			    float offset	/* y-axis offset */ );
void uigraph_draw_all_selected();

void uigraph_on_zoom_in_horiz (GtkButton *button, gpointer user_data);
void uigraph_on_zoom_in_vert  (GtkButton *button, gpointer user_data);
void uigraph_on_zoom_out      (GtkButton *button, gpointer user_data);
void uigraph_on_zoom_out_home (GtkButton *button, gpointer user_data);
int  uigraph_iszoomed();

void uigraph_inst_load();
void uigraph_inst_unload();
void uigraph_on_inst_toggled(GtkCellRendererToggle *widget, 
			     gchar *path_string, gpointer user_data);

void uigraph_curve_load();
void uigraph_curve_scroll_to_active();
void uigraph_curve_unload();
void uigraph_on_curve_toggled(GtkCellRendererToggle *widget, 
			      gchar *path_string, gpointer user_data);

#endif /* _UIGRAPH_H_ */
