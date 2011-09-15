/*
 * GHabitat Gtk GUI about window
 *
 * Nigel Stuckey, July 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#ifndef _UIABOUT_H_
#define _UIABOUT_H_

void uiabout_init();
void uiabout_fini();
void on_support_wiki (GtkObject *object, gpointer user_data);
void on_website (GtkObject *object, gpointer user_data);
void on_manual (GtkObject *object, gpointer user_data);
int  uiabout_browse_web(char *url);
int  uiabout_browse_help(char *helpfile);
int  uiabout_browse_man(char *manpage);

#endif /* _UIABOUT_H_ */
