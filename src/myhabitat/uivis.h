/*
 * Habitat GUI visualisation code
 *
 * Nigel Stuckey, August 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#ifndef _UIVIS_H_
#define _UIVIS_H_

#include <time.h>
#include <gtk/gtk.h>

/* Visualisation states */
enum uivis_t {
     UIVIS_SPLASH=0,
     UIVIS_WHATNEXT,
     UIVIS_INFO,
     UIVIS_TEXT,
     UIVIS_HTML,
     UIVIS_TABLE,
     UIVIS_CHART,
     UIVIS_NONE,
     UIVIS_EOL
};

/* GtkNotebook page: Display */
enum{
     UIVIS_NOTEBOOK_DISPLAY_SPLASH=0,
     UIVIS_NOTEBOOK_DISPLAY_MAIN,
     UIVIS_NOTEBOOK_DISPLAY_INFO,
     UIVIS_NOTEBOOK_DISPLAY_WHATNEXT,
     UIVIS_NOTEBOOK_DISPLAY_EOL
};

/* GtkNotebook page: Visualisation (inside display) */
enum{
     UIVIS_NOTEBOOK_VIS_TABLE=0,
     UIVIS_NOTEBOOK_VIS_CHART,
     UIVIS_NOTEBOOK_VIS_TEXT,
     UIVIS_NOTEBOOK_VIS_PROP,
     UIVIS_NOTEBOOK_VIS_HTML,
     UIVIS_NOTEBOOK_VIS_EOL
};


void uivis_init();
void uivis_fini();
void uivis_change_view(enum uivis_t);
void uivis_draw(char *route, TABLE (*dfunc)(time_t, time_t), 
		time_t view_oldest, time_t view_youngest);
void uivis_on_vis_changed (GtkToolButton *toolbutton, gpointer data);
void uivis_on_view_text (GtkObject *object, gpointer user_data);
void uivis_on_view_table (GtkObject *object, gpointer user_data);
void uivis_on_view_chart (GtkObject *object, gpointer user_data);

#endif /* _UIVIS_H_ */
