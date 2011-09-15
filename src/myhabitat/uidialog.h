/*
 * MyHabitat Gtk GUI Harvest dialogues, implementing specific dilogue 
 * (dialog) windows
 *
 * Nigel Stuckey, February 2011
 * Copyright System Garden Ltd 2004,2011. All rights reserved
 */

#ifndef _UIDIALOG_H_
#define _UIDIALOG_H_

#include <gtk/gtk.h>

#define UIDIALOG_YES 1
#define UIDIALOG_NO  0

int uidialog_yes_or_no (char *parent_window_name,
			char *primary_text, 
			char *secondary_text, ... /* varagrs */);

#endif /* _UIDIALOG_H_ */
