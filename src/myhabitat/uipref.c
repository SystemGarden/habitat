/*
 * MyHabitat Gtk GUI preference class to store and load select configuration
 * items iiab_cf.
 *
 * Nigel Stuckey, November 2011
 * Copyright System Garden Ltd 2011. All rights reserved
 */

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "../iiab/cf.h"
#include "../iiab/iiab.h"
#include "../iiab/util.h"
#include "main.h"
#include "uipref.h"
#include "uidata.h"

void uipref_init()
{
     int val;
     GtkWidget *w;

     /* Fetch quanity */
     val = cf_getint(iiab_cf, UIPREF_CFKEY_FETCHQUANT);
     if (val != CF_UNDEF) {
          /* match the right button */
          if (val == 3600)
	       w = get_widget("pref_fetchquant_1h_radio");
	  else if (val == 86400)
	       w = get_widget("pref_fetchquant_1d_radio");
	  else if (val == 604800)
	       w = get_widget("pref_fetchquant_1w_radio");
	  else if (val == 2592000)
	       w = get_widget("pref_fetchquant_1m_radio");

	  /* block sig, activate and unblock sig */
          g_signal_handlers_block_by_func(G_OBJECT(w), 
					  G_CALLBACK(uipref_on_fetchquant_set),
					  NULL);
	  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( w ), TRUE );
          g_signal_handlers_unblock_by_func(G_OBJECT(w), 
					  G_CALLBACK(uipref_on_fetchquant_set),
					  NULL);
     }

     /* Data set cache size value */
     val = cf_getint(iiab_cf, UIPREF_CFKEY_CACHESIZE);
     if (val == CF_UNDEF)
          val = 10;
     w = get_widget("pref_cachesize_entry");
     g_signal_handlers_block_by_func(G_OBJECT(w), 
				     G_CALLBACK(uipref_on_cachesize_set),
				     NULL);
     gtk_entry_set_text( GTK_ENTRY(w), util_i32toa(val) );
     g_signal_handlers_unblock_by_func(G_OBJECT(w), 
				       G_CALLBACK(uipref_on_cachesize_set),
				       NULL);


     /* Data update value */
     val = cf_getint(iiab_cf, UIPREF_CFKEY_UPDATE);
     if (val == CF_UNDEF)
          val = UIDATA_DEFAULT_UPDATE_TIME;
     w = get_widget("pref_update_entry");
     g_signal_handlers_block_by_func(G_OBJECT(w), 
				     G_CALLBACK(uipref_on_update_set),
				     NULL);
     gtk_entry_set_text( GTK_ENTRY(w), util_i32toa(val) );
     g_signal_handlers_unblock_by_func(G_OBJECT(w), 
				       G_CALLBACK(uipref_on_update_set),
				       NULL);
}


void uipref_fini()
{
}


G_MODULE_EXPORT void 
uipref_on_fetchquant_set (GtkRadioButton *object, gpointer user_data)
{
     const gchar *text;

     if ( ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(object) ) )
          return;

     text = gtk_button_get_label( GTK_BUTTON(object) );

     if (strcmp(text, "One Hour") == 0) {
          cf_putint(iiab_cf, UIPREF_CFKEY_FETCHQUANT, 3600);
     } else if (strcmp(text, "One Day") == 0) {
          cf_putint(iiab_cf, UIPREF_CFKEY_FETCHQUANT, 86400);
     } else if (strcmp(text, "One Week") == 0) {
          cf_putint(iiab_cf, UIPREF_CFKEY_FETCHQUANT, 604800);
     } else if (strcmp(text, "One Month") == 0) {
          cf_putint(iiab_cf, UIPREF_CFKEY_FETCHQUANT, 2592000);
     } else {
          elog_printf(ERROR, "Unrecognised button");
     }
}


G_MODULE_EXPORT void 
uipref_on_cachesize_set (GtkEntry *object, gpointer user_data)
{
     const gchar *text;
     int size;

     /* get text, turn into an int and limit to 1..100 */
     text = gtk_entry_get_text( GTK_ENTRY(object) );
     size = atoi(text);
     if (size < 1)
          size = 1;
     if (size > 100)
          size = 100;

     cf_putint(iiab_cf, UIPREF_CFKEY_CACHESIZE, size);
}


G_MODULE_EXPORT void 
uipref_on_update_set (GtkEntry *object, gpointer user_data)
{
     const gchar *text;
     int secs;

     /* get text, turn into an int and limit to 1..100 */
     text = gtk_entry_get_text( GTK_ENTRY(object) );
     secs = atoi(text);
     if (secs < 0)
          secs = 0;

     cf_putint(iiab_cf, UIPREF_CFKEY_UPDATE, secs);
}
