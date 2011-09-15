/*
 * Habitat GUI data area
 *
 * Nigel Stuckey, July 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#ifndef _UIDATA_H_
#define _UIDATA_H_

#include <gtk/gtk.h>
#include "uivis.h"
#include "../iiab/tree.h"

void uidata_init();
void uidata_fini();
void uidata_on_choice_changed (GtkTreeView *tree, gpointer data);
void uidata_choice_change (GtkTreeSelection *selection);
void uidata_on_ring_changed (GtkToolButton *toolbutton, gpointer data);
void uidata_on_other_ring_pressed (GtkToolButton *toolbutton, gpointer data);
void uidata_on_other_ring_item_activated (GtkImageMenuItem *menubutton, 
					  gpointer data);
void uidata_ring_change (char *ringlabel);
void uidata_populate_info();
void uidata_illuminate_ring_btns(TREE *rings);
void uidata_remove_child_widget(GtkWidget* widget, gpointer data);
void uidata_illuminate_vis_btns(enum uivis_t vistype);
void uidata_illuminate_time();
void uidata_deilluminate_time();
void uidata_on_data_update (GtkObject *object, gpointer user_data);
void uidata_data_update();


#endif /* _UIDATA_H_ */
