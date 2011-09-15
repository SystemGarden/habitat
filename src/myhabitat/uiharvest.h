/*
 * MyHabitat Gtk GUI Harvest handling, implementing the callbacks and
 * connections for talking to the Harvest repository.
 *
 * Nigel Stuckey, July 2010
 * Copyright System Garden Ltd 2004,2010. All rights reserved
 */

#ifndef _UIHARVEST_H_
#define _UIHARVEST_H_

#include <gtk/gtk.h>

void harv_init();
void harv_fini();
void harv_populate_gui ();
void harv_on_enable (GtkObject *object, gpointer user_data);
void harv_on_send (GtkObject *object, gpointer user_data);
void harv_account_detail_visibility(int isvisible);
void harv_on_proxy_detail (GtkObject *object, gpointer user_data);
void harv_proxy_detail_visibility(int isvisible);
void harv_on_ok (GtkObject *object, gpointer user_data);
void harv_save_gui();
void harv_on_test (GtkObject *object, gpointer user_data);
void harv_on_help (GtkObject *object, gpointer user_data);
void harv_on_get_account (GtkObject *object, gpointer user_data);

#endif /* _UIHARVEST_H_ */
