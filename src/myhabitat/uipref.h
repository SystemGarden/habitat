/*
 * MyHabitat Gtk GUI preference class to store and load select configuration
 * items iiab_cf.
 *
 * Nigel Stuckey, November 2011
 * Copyright System Garden Ltd 2011. All rights reserved
 */


#ifndef _UIPREF_H_
#define _UIPREF_H_

#include <gtk/gtk.h>

#define UIPREF_CFNAME "myhab."
#define UIPREF_CFKEY_FETCHQUANT UIPREF_CFNAME "fetchquant"
#define UIPREF_CFKEY_CACHESIZE  UIPREF_CFNAME "cachesize"
#define UIPREF_CFKEY_UPDATE     UIPREF_CFNAME "update"

void uipref_init();
void uipref_fini();
void uipref_on_fetchquant_set (GtkRadioButton *object, gpointer user_data);
void uipref_on_cachesize_set (GtkEntry *object, gpointer user_data);
void uipref_on_update_set (GtkEntry *object, gpointer user_data);

#endif /* _UIPREF_H_ */
