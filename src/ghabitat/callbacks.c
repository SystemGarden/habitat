/*
 * Code genererated by glade for habitat
 * Copyright System Garden Ltd, 1999-2005. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <unistd.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <string.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#include "gtkaction.h"
#include "main.h"
#include "uichoice.h"
#include "ghchoice.h"
#include "misc.h"
#include "../iiab/elog.h"
#include "../iiab/table.h"
#include "../iiab/tableset.h"
#include "../iiab/meth.h"
#include "../iiab/conv.h"
#include "../iiab/util.h"
#include "../iiab/cf.h"
#include "../iiab/iiab.h"
#include "../iiab/httpd.h"

int log_popup_detailed_state=0;

gboolean
on_baseWindow_destroy_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data_save)
{
     gtk_main_quit();
     return FALSE;
}


gboolean
on_baseWindow_delete_event             (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
     gtk_main_quit();
     return FALSE;
}

void
on_open_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_setprogress("open file", 0.0, 0);
     gtk_widget_show (file_open_window);
}


void
on_import_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     TREE *history, *session;
     GList *hols = NULL, *hist = NULL;
     GtkWidget *import_file_combo;
     GtkWidget *import_file_types;
     GtkWidget *import_ringstore_combo;
     GtkWidget *import_ring_name;
     GtkWidget *import_progress;

     /* create import window and stuff with the latest information */
     import_window = create_import_window();

     /* fetch widget referencies */
     import_file_combo=lookup_widget(import_window, "import_file_combo");
     import_file_types=lookup_widget(import_window, "import_file_types");
     import_ringstore_combo = lookup_widget(import_window, 
					   "import_ringstore_combo");
     import_ring_name = lookup_widget(import_window, "import_ring_name");
     import_progress  = lookup_widget(import_window, "import_progress");

     /* fill import file pulldown with history list */
     history = ghchoice_get_myfiles_list();
     tree_traverse(history)
	  hist = g_list_append(hist, tree_getkey(history));
     if (hist) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(import_file_combo), hist);
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(import_file_combo)->entry), 
			     hist->data);
     }

     /* fill ringstore pulldown with session list */
     session = ghchoice_get_myfiles_load();
     tree_traverse(session)
	  hols = g_list_append(hols, tree_getkey(session));
     if (hols) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(import_ringstore_combo),hols);
	  gtk_entry_set_text (GTK_ENTRY (
	       GTK_COMBO (import_ringstore_combo)->entry), hols->data);
     }

     /* get rings for default ringstore, if present */
     if (hols)
	  on_import_ringstore_name_changed(
	       GTK_EDITABLE (GTK_COMBO (import_ringstore_combo)->entry), NULL);

     /* file a history of files */
     gtk_widget_show (import_window);

     g_list_free(hols);
     g_list_free(hist);
}


void
on_import_file_name_changed            (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
on_import_ringstore_name_changed        (GtkEditable     *editable,
                                        gpointer         user_data)
{

}


void
on_import_file_filesel_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
     file_import_select = create_file_import_select();
     gtk_widget_show (file_import_select);
}


void
on_import_ringstore_filesel_clicked     (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_import_action_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
#if 0
     /* process the import action button */
     GtkWidget *import_file_types;
     GtkWidget *import_file_name;
     GtkWidget *import_ringstore_name;
     GtkWidget *import_ring_name;
     GtkWidget *import_progress;
     GtkWidget *import_title_opt;
     GtkWidget *import_info_opt;
     GtkWidget *import_time_opt;
     GtkWidget *import_seq_opt;
     GtkWidget *import_host_opt;
     GtkWidget *import_ring_opt;
     GtkWidget *import_dur_opt;
     GtkWidget *mtype = NULL;
     char *file, *hol, *ring, *mtypestr, description[512];
     int r, withtitle, withruler, withseq, withtime, withhost, withring,
       withdur;

     import_file_types = lookup_widget(GTK_WIDGET(button), "import_file_types");
     import_file_name  = lookup_widget(GTK_WIDGET(button), "import_file_name");
     import_ringstore_name = lookup_widget(GTK_WIDGET(button), 
					  "import_ringstore_name");
     import_ring_name  = lookup_widget(GTK_WIDGET(button), "import_ring_name");
     import_progress   = lookup_widget(GTK_WIDGET(button), "import_progress");
     import_title_opt  = lookup_widget(GTK_WIDGET(button), "import_title_opt");
     import_info_opt   = lookup_widget(GTK_WIDGET(button), "import_info_opt");
     import_time_opt   = lookup_widget(GTK_WIDGET(button), "import_time_opt");
     import_seq_opt    = lookup_widget(GTK_WIDGET(button), "import_seq_opt");
     import_host_opt   = lookup_widget(GTK_WIDGET(button), "import_host_opt");
     import_ring_opt   = lookup_widget(GTK_WIDGET(button), "import_ring_opt");
     import_dur_opt    = lookup_widget(GTK_WIDGET(button), "import_dur_opt");

     /* check that the fields have been filled in correctly */
     file = gtk_entry_get_text( GTK_ENTRY(import_file_name) );
     if (file == NULL || *file == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(import_progress), 
					  "need an import file");
	  return;
     }
     if (access(file, R_OK)) {
	  gtk_progress_set_format_string( GTK_PROGRESS(import_progress), 
					  "unable to read import file");
	  return;
     }
     hol = gtk_entry_get_text( GTK_ENTRY(import_ringstore_name) );
     if (hol == NULL || *hol == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(import_progress), 
					  "need a ringstore");
	  return;
     }
     ring = gtk_entry_get_text( GTK_ENTRY(import_ring_name) );
     if (ring == NULL || *ring == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(import_progress), 
					  "need an ring");
	  return;
     }

     /* find the option selected */
     /* This is voodoo!!
      * The option is held in an GtkAccelLabel in a GtkMenuItem connected
      * to a GtkOptionMenu. Rather than tunnelling down the list, the 
      * GtkOptionMenu reparents the active label for a while, so all
      * we have to do is dig out the contents of GtkOptionMenu's bin (single
      * child) then cast down cast it to a lable and aquire the text.
      * Confused?? anyway, it appears to work.
      */
     if (GTK_BIN (import_file_types)->child == NULL) {
	  gtk_progress_set_format_string( GTK_PROGRESS(import_progress), 
					  "import file type not set");
	  return;
     } else {
	  mtype = GTK_BIN (import_file_types)->child;
	  gtk_label_get (GTK_LABEL (mtype), &mtypestr);
     }

     /* prepare description and options for load */
     snprintf(description, 512, "import of %s", file);
     withtitle = GTK_TOGGLE_BUTTON(import_title_opt)->active;
     withruler = GTK_TOGGLE_BUTTON(import_info_opt)->active;
     withtime  = GTK_TOGGLE_BUTTON(import_time_opt)->active;
     withseq   = GTK_TOGGLE_BUTTON(import_seq_opt)->active;
     withhost  = GTK_TOGGLE_BUTTON(import_host_opt)->active;
     withring  = GTK_TOGGLE_BUTTON(import_ring_opt)->active;
     withdur   = GTK_TOGGLE_BUTTON(import_dur_opt)->active;

     /* carry out the work */
     /* process specific import types */
     if (strncmp(mtypestr, "csv", 3) == 0) {
	  r = conv_file2ring(file, hol, 0644, ring, description, NULL, 
			     10, ",", withtitle, withruler, withtime, 
			     withseq, withhost, withring, withdur);
     } else if (strncmp(mtypestr, "tsv", 3) == 0) {
	  r = conv_file2ring(file, hol, 0644, ring, description, NULL, 
			     10, "\t", withtitle, withruler, withtime, 
			     withseq, withhost, withring, withdur);
     } else if (strncmp(mtypestr, "fha", 3) == 0) {
	  r = conv_file2ring(file, hol, 0644, ring, description, NULL, 
			     10, "\t", withtitle, withruler, withtime, 
			     withseq, withhost, withring, withdur);
     } else if (strncmp(mtypestr, "sar", 3) == 0) {
	  r = conv_solsar2tab(file, hol, ring, "", "");
     } else {
	  gtk_progress_set_format_string( GTK_PROGRESS(import_progress), 
					  "import file type not set");
	  return;
     }

     /* inform user of conclusion */
     if (r <= -1)
	  gtk_progress_set_format_string( GTK_PROGRESS(import_progress), 
					  "couldn't import file");
     else
	  gtk_progress_set_format_string( GTK_PROGRESS(import_progress), 
					  "file imported into ring");

     /* keep file for later */
     ghchoice_add_myfiles_list(file);
     ghchoice_add_myfiles_list(hol);
#endif
}


void
on_import_finished_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
     gtk_widget_hide (import_window);
     gtk_widget_destroy (import_window);
}


void
on_import_help_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
     gtkaction_browse_help(HELP_IMPORT);
}


void
on_file_import_okbutton_clicked        (GtkButton       *button,
                                        gpointer         user_data)
{
  char *fname;
  GtkWidget *import_file_name, *import_file_combo;
  /*GList *files;*/

  /* get file name selection and some widget pointers */
  fname = gtk_file_selection_get_filename( GTK_FILE_SELECTION(user_data) );
  import_file_name = lookup_widget(GTK_WIDGET(button), "import_file_name");
  import_file_combo = lookup_widget(GTK_WIDGET(button), "import_file_combo");

  /* set text field and pop-down list of combo box */
  gtk_entry_set_text(GTK_ENTRY(import_file_name), fname);
#if 0
MAKES IT COREDUMP AS NOT A LIS AT FIRST. 
NEED TO WORK OUT HOW TO APPEND PROPERLY
  files = g_list_append(GTK_COMBO(import_file_combo)->list, fname);
  gtk_combo_set_popdown_strings(GTK_COMBO(import_file_combo), files);
#endif
  
  /* remove the file selection box */
  gtk_widget_hide(GTK_WIDGET(user_data));
  gtk_widget_destroy(GTK_WIDGET(user_data));
  gtkaction_clearprogress();
}


void
on_file_import_cancelbutton_clicked    (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_hide(GTK_WIDGET(user_data));
  gtk_widget_destroy(GTK_WIDGET(user_data));
  gtkaction_clearprogress();
}


void
on_export_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     TREE *history, *session;
     GList *hols=NULL, *hist=NULL;
     GtkWidget *export_ringstore_combo;
     GtkWidget *export_ring_combo;
     GtkWidget *export_file_combo;

     /* create export window and stuff with the latest information */
     export_window = create_export_window();

     /* fetch widget referencies */
     export_ringstore_combo = lookup_widget(export_window, 
					   "export_ringstore_combo");
     export_ring_combo=lookup_widget(export_window, "export_ring_combo");
     export_file_combo=lookup_widget(export_window, "export_file_combo");

     /* fill ringstore pulldown with session list */
     session = ghchoice_get_myfiles_load();
     tree_traverse(session)
	  hols = g_list_append(hols, tree_getkey(session));
     if (hols) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(export_ringstore_combo),hols);
	  gtk_entry_set_text (GTK_ENTRY (
	       GTK_COMBO (export_ringstore_combo)->entry), hols->data);
     }

     /* get rings for default ringstore, if present */
     if (hols)
	  on_export_ringstore_name_changed(
	       GTK_EDITABLE( GTK_COMBO (export_ringstore_combo)->entry), NULL);

     /* fill output file pulldown with history list */
     history = ghchoice_get_myfiles_list();
     tree_traverse(history)
	  hist = g_list_append(hist, tree_getkey(history));
     if (hist) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(export_file_combo), hist);
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(export_file_combo)->entry), 
			     hist->data);
     }

     gtk_widget_show (export_window);

     g_list_free(hols);
     g_list_free(hist);
}


void
on_export_ringstore_filesel_clicked     (GtkButton       *button,
                                        gpointer         user_data)
{
#if 0
CREATE_RINGSTORE_EXPORT_SELECT NOT YET DEFINED
     GtkWidget *export_ringstore_select;

     export_ringstore_select = create_ringstore_export_select();
     gtk_widget_show (export_ringstore_select);
#endif
}


void
on_export_file_filesel_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
     file_export_select = create_file_export_select();
     gtk_widget_show (file_export_select);
}


void
on_export_action_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
#if 0
     /* process the export action button */
     GtkWidget *export_file_types;
     GtkWidget *export_file_name;
     GtkWidget *export_ringstore_name;
     GtkWidget *export_ring_name;
     GtkWidget *export_progress;
     GtkWidget *export_title_opt;
     GtkWidget *export_info_opt;
     GtkWidget *export_time_opt;
     GtkWidget *export_seq_opt;
     GtkWidget *export_host_opt;
     GtkWidget *export_ring_opt;
     GtkWidget *export_dur_opt;
     GtkWidget *mtype = NULL;
     char *file, *hol, *ring, *filedir, *eot, *mtypestr;
     int r, withtitle, withruler, withseq, withtime, withhost, withring,
       withdur;

     export_file_types = lookup_widget(GTK_WIDGET(button), "export_file_types");
     export_file_name  = lookup_widget(GTK_WIDGET(button), "export_file_name");
     export_ringstore_name = lookup_widget(GTK_WIDGET(button), 
					  "export_ringstore_name");
     export_ring_name  = lookup_widget(GTK_WIDGET(button), "export_ring_name");
     export_progress   = lookup_widget(GTK_WIDGET(button), "export_progress");
     export_title_opt  = lookup_widget(GTK_WIDGET(button), "export_title_opt");
     export_info_opt   = lookup_widget(GTK_WIDGET(button), "export_info_opt");
     export_time_opt   = lookup_widget(GTK_WIDGET(button), "export_time_opt");
     export_seq_opt    = lookup_widget(GTK_WIDGET(button), "export_seq_opt");
     export_host_opt   = lookup_widget(GTK_WIDGET(button), "export_host_opt");
     export_ring_opt   = lookup_widget(GTK_WIDGET(button), "export_ring_opt");
     export_dur_opt    = lookup_widget(GTK_WIDGET(button), "export_dur_opt");

     /* check that the fields have been filled in correctly */
     file = gtk_entry_get_text( GTK_ENTRY(export_file_name) );
     if (file == NULL || *file == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(export_progress), 
					  "Need an import file");
	  return;
     }

     /* get the base dir */
     eot = strrchr(file, '/');	/* bin dir */
     if (eot) {
	  filedir = xnmemdup(file, eot-file+1);
	  *(eot-file+filedir) = '\0';   /* remove last or trailing '/' */
     } else
	  filedir = xnstrdup(".");
     if (access(filedir, W_OK)) {
	  gtk_progress_set_format_string( GTK_PROGRESS(export_progress), 
					  "unable to write export file "
					  "directory");
	  return;
     }
     nfree(filedir);
     if ( ! access(file, F_OK)) {
	  /* want a popup TODO */
	  gtk_progress_set_format_string( GTK_PROGRESS(export_progress), 
					  "file exists");
	  return;
     }
     hol = gtk_entry_get_text( GTK_ENTRY(export_ringstore_name) );
     if (hol == NULL || *hol == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(export_progress), 
					  "need a ringstore");
	  return;
     }
     ring = gtk_entry_get_text( GTK_ENTRY(export_ring_name) );
     if (ring == NULL || *ring == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(export_progress), 
					  "need an ring");
	  return;
     }

     /* find the option selected */
     /* This is voodoo!!
      * The option is held in an GtkAccelLabel in a GtkMenuItem connected
      * to a GtkOptionMenu. Rather than tunnelling down the list, the 
      * GtkOptionMenu reparents the active label for a while, so all
      * we have to do is dig out the contents of GtkOptionMenu's bin (single
      * child) then cast down cast it to a lable and aquire the text.
      * Confused?? anyway, it appears to work.
      */
     if (GTK_BIN (export_file_types)->child == NULL) {
	  gtk_progress_set_format_string( GTK_PROGRESS(export_progress), 
					  "output file type not set");
	  return;
     } else {
	  mtype = GTK_BIN (export_file_types)->child;
	  gtk_label_get (GTK_LABEL (mtype), &mtypestr);
     }

     /* set up options for export */
     withtitle = GTK_TOGGLE_BUTTON(export_title_opt)->active;
     withruler = GTK_TOGGLE_BUTTON(export_info_opt)->active;
     withtime  = GTK_TOGGLE_BUTTON(export_time_opt)->active;
     withseq   = GTK_TOGGLE_BUTTON(export_seq_opt)->active;
     withhost  = GTK_TOGGLE_BUTTON(export_host_opt)->active;
     withring  = GTK_TOGGLE_BUTTON(export_ring_opt)->active;
     withdur   = GTK_TOGGLE_BUTTON(export_dur_opt)->active;

     /* carry out the work */
     /* process specific export types, using first a tablestore method
      * then the timestore if that fails */
     if (strncmp(mtypestr, "csv", 3) == 0) {
	  r = conv_ring2file(hol, ring, NULL, file, ',', withtitle, 
			     withruler, withtime, NULL, withseq, withhost, 
			     withring, withdur, -1, -1);
     } else if (strncmp(mtypestr, "tsv", 3) == 0) {
	  r = conv_ring2file(hol, ring, NULL, file, '\t', withtitle, 
			     withruler, withtime, NULL, withseq, withhost, 
			     withring, withdur, -1, -1);
     } else if (strncmp(mtypestr, "fha", 3) == 0) {
	  r = conv_ring2file(hol, ring, NULL, file, '\t', withtitle, 
			     withruler, withtime, NULL, withseq, withhost, 
			     withring, withdur, -1, -1);
     } else {
	  gtk_progress_set_format_string( GTK_PROGRESS(export_progress), 
					  "output file type not set");
	  return;
     }

     /* inform user of conclusion */
     if (r <= -1)
	  gtk_progress_set_format_string( GTK_PROGRESS(export_progress), 
					  "couldn't export file");
     else
	  gtk_progress_set_format_string( GTK_PROGRESS(export_progress), 
					  "ring exported");

     /* keep file for later */
     ghchoice_add_myfiles_list(file);
     ghchoice_add_myfiles_list(hol);
#endif
}


void
on_export_finished_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
     gtk_widget_hide (export_window);
     gtk_widget_destroy (export_window);
}


void
on_export_help_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
     gtkaction_browse_help(HELP_EXPORT);
}


void
on_file_export_okbutton_clicked        (GtkButton       *button,
                                        gpointer         user_data)
{
  char *fname;
  GtkWidget *export_file_name, *export_file_combo;
  GtkWidget *listitem;

  /* get file name selection and some widget pointers */
  fname = gtk_file_selection_get_filename (GTK_FILE_SELECTION (user_data));
  export_file_name = lookup_widget (GTK_WIDGET(button), "export_file_name");
  export_file_combo = lookup_widget (GTK_WIDGET(button), "export_file_combo");

  /* set text field and prepend to pop-down list of combo box */
  listitem = gtk_list_item_new_with_label ((gchar *) fname);
  gtk_widget_show (listitem);
  gtk_container_add (GTK_CONTAINER (GTK_COMBO (export_file_combo)->list), 
		     listitem);
  gtk_entry_set_text (GTK_ENTRY (export_file_name), fname);
  
  /* remove the file selection box */
  gtk_widget_hide(GTK_WIDGET(user_data));
  gtk_widget_destroy(GTK_WIDGET(user_data));
  gtkaction_clearprogress();
}


void
on_file_export_cancelbutton_clicked    (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_hide(GTK_WIDGET(user_data));
  gtk_widget_destroy(GTK_WIDGET(user_data));
  gtkaction_clearprogress();
}


/*
 * The holstore name has changed, so open it up and with the ring listing
 * change the pulldown in the ring field of the export form.
 */
void
on_export_ringstore_name_changed        (GtkEditable     *editable,
                                        gpointer         user_data)
{
#if 0
     GtkWidget *export_ringstore_name;
     GtkWidget *export_ring_combo;
     char *hol;
     HOLD hid;
     GList *rings=NULL;
     TREE *holrings;

     /* fetch widget referencies */
     export_ringstore_name = lookup_widget(GTK_WIDGET(editable),
					  "export_ringstore_name");
     export_ring_combo = lookup_widget(GTK_WIDGET(editable),"export_ring_combo");

     /* get ringstore selection */
     hol = gtk_entry_get_text( GTK_ENTRY(export_ringstore_name) );
     if (hol == NULL || *hol == '\0') {
	  rings = g_list_append(rings, "");
	  gtk_combo_set_popdown_strings (GTK_COMBO (export_ring_combo), rings);
	  g_list_free(rings);
	  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (export_ring_combo)->entry),
			      "");
	  return;
     }

     /* get rings from the timestore part of the holstore */
     hid = hol_open(hol);
     if (hid) {
	  holrings = ts_lsringshol(hid, "");
	  hol_close(hid);
	  if (holrings) {
	       tree_traverse(holrings)
		    rings = g_list_append(rings, tree_getkey(holrings));
	       
	       /* set rings in widget */
	       gtk_combo_set_popdown_strings (GTK_COMBO 
					      (export_ring_combo), rings);
	       gtk_entry_set_text (GTK_ENTRY (
		    GTK_COMBO (export_ring_combo)->entry), rings->data);

	       ts_freelsrings(holrings);
	       g_list_free(rings);
	  }
     } else {
	  elog_printf(DIAG, "can't open %s to read timestore rings", hol);
     }

#endif
}


void
on_export_file_name_changed            (GtkEditable     *editable,
                                        gpointer         user_data)
{

}

void
on_local_collect_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *w, *stop_bin, *stop_user, *stop_pid, *stop_run, *dontask_opt;
     char *key, *user, *tty, *datestr, pidstr[10];
     int pid, dontask;

     if ((pid = is_clockwork_running(&key, &user, &tty, &datestr))) {
          sprintf(pidstr, "%d", pid);;
          w = create_stop_clockwork_window();
	  gtk_widget_show (w);
	  stop_bin  = lookup_widget(w, "stop_clockwork_bin_val");
	  stop_user = lookup_widget(w, "stop_clockwork_user_val");
	  stop_pid  = lookup_widget(w, "stop_clockwork_pid_val");
	  stop_run  = lookup_widget(w, "stop_clockwork_runt_val");
	  gtk_label_set(GTK_LABEL(stop_bin), key);
	  gtk_label_set(GTK_LABEL(stop_user), user);
	  gtk_label_set(GTK_LABEL(stop_pid), pidstr);
	  gtk_label_set(GTK_LABEL(stop_run), datestr);
     } else {
          w = create_start_clockwork_window();
	  gtk_widget_show (w);
	  dontask_opt = lookup_widget(w, "start_clockwork_dontask_opt");
	  dontask = cf_getint(iiab_cf, DONTASKCLOCKWORK_CFNAME);
	  if (dontask != CF_UNDEF && dontask) {
	       gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(dontask_opt), 1);
	  }
     }
}


void
on_exit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_setprogress("Please wait", 0.0, 0);
     gtk_main_quit();
}


void
on_menugraph_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     /* called when the graph menu is pulled down. --event not needed */
}


void
on_show_rulers_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *w;

     gmcgraph_showallrulers(graph);

     show_rulers = 1;
     w = lookup_widget(GTK_WIDGET(menuitem), "hide_rulers");
     gtk_widget_show(w);
     w = lookup_widget(GTK_WIDGET(menuitem), "show_rulers");
     gtk_widget_hide(w);
}


void
on_hide_rulers_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *w;

     gmcgraph_hideallrulers(graph);

     show_rulers = 0;
     w = lookup_widget(GTK_WIDGET(menuitem), "show_rulers");
     gtk_widget_show(w);
     w = lookup_widget(GTK_WIDGET(menuitem), "hide_rulers");
     gtk_widget_hide(w);
}


void
on_show_axis_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *w;

     gmcgraph_showallaxis(graph);

     show_axis = 1;
     w = lookup_widget(GTK_WIDGET(menuitem), "hide_axis");
     gtk_widget_show(w);
     w = lookup_widget(GTK_WIDGET(menuitem), "show_axis");
     gtk_widget_hide(w);
}


void
on_hide_axis_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *w;

     gmcgraph_hideallaxis(graph);

     show_axis = 0;
     w = lookup_widget(GTK_WIDGET(menuitem), "show_axis");
     gtk_widget_show(w);
     w = lookup_widget(GTK_WIDGET(menuitem), "hide_axis");
     gtk_widget_hide(w);
}


void
on_view_histogram_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *w;

     gmcgraph_allgraphstyle(graph, GMCGRAPH_BAR);

     view_histogram = 1;
     w = lookup_widget(GTK_WIDGET(menuitem), "view_curves");
     gtk_widget_show(w);
     w = lookup_widget(GTK_WIDGET(menuitem), "view_histogram");
     gtk_widget_hide(w);
}


void
on_view_curves_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *w;

     gmcgraph_allgraphstyle(graph, GMCGRAPH_THINLINE);

     view_histogram = 0;
     w = lookup_widget(GTK_WIDGET(menuitem), "view_curves");
     gtk_widget_hide(w);
     w = lookup_widget(GTK_WIDGET(menuitem), "view_histogram");
     gtk_widget_show(w);
}

void
on_set_curve_colour_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *w;

     w = create_curve_colour();
     gtk_widget_show(w);
}


void
on_about_habitat_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *w;
     char buf[30];

     about_window = create_about_dialog ();
     w = lookup_widget(about_window, "about_name");
     sprintf(buf, "Habitat\nVersion %s", VERSION);
     gtk_label_set_text(GTK_LABEL(w), buf);
     gtk_widget_show (about_window);
}


gboolean
on_stathistory_button_press_event      (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
     elog_printf(DEBUG, "button press event callback (3)");
#if 0
     GtkWidget *w;

     w = create_log_popup_menu();
     gtk_widget_show(w);
#endif
     return FALSE;
}


gboolean
on_stathistory_button_release_event    (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
     elog_printf(DEBUG, "button release event callback (2)");
     return FALSE;
}


void
on_Show_origin_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_Show_date_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_Show_time_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_Show_severity_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_sevColDebug_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_sevColInfo_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_sevColWarn_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_sevColErr_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_sevColFatal_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_about_button_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_hide(about_window);
  gtk_widget_destroy(about_window);
}


void
on_file_open_okbutton_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
  int r=0;
  char *fname;
  struct uichoice_node *filenode, *myfiles=NULL;
  GtkCTreeNode *myfiles_treeitem;

  /* move the furniture */
  gtkaction_setprogress("skimming file", 0.0, 0);
  gtk_widget_hide(file_open_window);

  /* find myfiles node */
  myfiles = uichoice_findlabel_all("my files");
  if (myfiles == NULL) {
       elog_printf(ERROR, "unable to find myfile node to attach");
       gtkaction_clearprogress();
       return;
  }

#if 0
  nodetree = uichoice_getnodetree();
  itree_traverse(nodetree) {
       myfiles = uichoice_findlabel(itree_get(nodetree), "my files");
       if (myfiles)
	    break;
  }
  if (myfiles == NULL) {
       elog_printf(ERROR, "unable to find myfile node to attach");
       gtkaction_clearprogress();
       return;
  }
#endif

  /* get filename & load */
  fname = gtk_file_selection_get_filename( GTK_FILE_SELECTION(user_data) );
  filenode = ghchoice_loadfile(fname, myfiles, &r);

  /* process return code from the load */
  switch (r) {
  case 1:
       /* file successfully added to choice tree, now the gui has to catch
	* up. Find the corresponding myfiles tree item, and if a sub tree has
	* not been created (no children) then create one.
	* Then make a new node corresponding to the new file and expose
	* the filenames in the tree */
       myfiles_treeitem = GTK_CTREE_NODE(*(GtkCTreeNode **) uichoice_getnodearg
					(myfiles, GTKACTION_GUIITEMKEY) );

       /*gtkaction_setprogress("drawing", 0.3, 0);*/
/*       subtree = GTK_TREE_ITEM_SUBTREE(myfiles_treeitem);
       if (subtree == 0) {
	    subtree = gtk_tree_new();
	    gtk_tree_item_set_subtree(GTK_TREE_ITEM(myfiles_treeitem), 
				      subtree);
       }
       gtkaction_makechoice(GTK_TREE(subtree), filenode, tooltips);
*/
       gtkaction_makechoice(myfiles_treeitem, filenode, tooltips);
       /*gtkaction_expandchoice(GTK_TREE(subtree), 1, tooltips);*/
       gtk_ctree_expand(GTK_CTREE(tree), myfiles_treeitem);
       break;
  case -1:
       elog_printf(ERROR, "Unable to read %s", fname);
       break;
  case -2:
       elog_printf(INFO, "%s has already been loaded", fname);
       break;
  case 0:
  default:
       elog_printf(ERROR, "Error loading %s", fname);
       break;
  }

  gtkaction_clearprogress();
}


void
on_file_open_cancelbutton_clicked      (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_hide(file_open_window);
  gtkaction_clearprogress();
}


void
on_save_graph_image_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_save_graph_setup_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

void
on_close_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *clist;
     TREE *hols;
     char *row[3];	/* row of 3 columns */
     struct uichoice_node *node;

     /* see if there are any open ringstores */
     hols = ghchoice_getloadedfiles();
     if (hols == NULL || tree_n(hols) == 0) {
	  elog_printf(INFO, "no open holstores to close!");
	  return;
     }

     /* create dialog */
     gtkaction_setprogress("close ringstore", 0.0, 0);
     file_close_dialog = create_file_close_dialog();

     /* stuff latest open files into GUI list */
     clist = lookup_widget(file_close_dialog, "file_close_list");
     tree_traverse(hols) {
	  /* prepare row */
	  row[0] = strrchr(tree_getkey(hols), '/');
	  if ( row[0] )
	       row[0]++;
	  else
	       row[0] = tree_getkey(hols);
	  node = tree_get(hols);
	  row[1] = strchr(node->info, '(');
	  if ( ! row[1] )
	       row[1] = node->info;
	  row[2] = tree_getkey(hols);
	  gtk_clist_append( GTK_CLIST(clist), row);
     }
     gtk_widget_show (file_close_dialog);
}


void
on_update_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_choice_update();
}


void
on_update_node_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     /* update the selected dynamic choice node */

     /* -- TODO: get selected node -- */

     /*gtkaction_node_update();*/
}


void
on_update_all_nodes_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     /* update all the dynamic choice nodes */
}


void
on_raw_data_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_write_table_contents_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


gboolean
on_tree_button_press_event             (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
     GtkWidget *popup, *label;
     gint row, col;
     GtkCTreeNode *treenode;
     struct uichoice_node *node;

     /* produce a popup when the right mouse button (number three) 
      * is pressed */
     if (event->type   == GDK_BUTTON_PRESS &&
	 event->button == 3) {

	  /* grab the uichoice node under the mouse pointer */
	  gtk_clist_get_selection_info(GTK_CLIST(widget), event->x, event->y,
				       &row, &col);
	  treenode = gtk_ctree_node_nth(GTK_CTREE(widget), row);
	  node = (struct uichoice_node *) 
	       gtk_ctree_node_get_row_data(GTK_CTREE(widget), treenode);

	  /* create the popup menu & assign a wm icon */ 
	  popup = create_choice_popup_menu();

	  /* make a title for the menu and place at the top */
	  label = gtk_menu_item_new_with_label (node->label);
	  gtk_widget_set_name (label, "choice_popup_label");
	  gtk_widget_ref (label);
	  gtk_object_set_data_full (GTK_OBJECT (popup), 
				    "choice_popup_label", label,
				    (GtkDestroyNotify) gtk_widget_unref);
	  gtk_widget_show (label);
	  gtk_menu_prepend(GTK_MENU(popup), label);

	  /* assign the node data to the tree, not the popup.
	   * The popup may not be in existance by the time we need the
	   * data. */
	  gtk_object_set_data (GTK_OBJECT (tree), "choice_popup_node",
			       (gpointer) node);

	  /* popup, but store the node data */
	  gtk_menu_popup (GTK_MENU (popup), NULL, NULL, NULL, NULL, 
			  event->button, event->time);

	  /* Tell calling code that we have handled this event; the buck
	   * stops here. */
	  return TRUE;
     }

    /* Tell calling code that we have not handled this event; pass it on. */
    return FALSE;
}


void
on_file_close_button_action_clicked    (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *clist;
     GList *sel;
     char *fname;
     struct uichoice_node *fnode, *node;
     GtkCTreeNode *treeitem=NULL;
/*     ITREE *top;*/

     /* get the list widget, find the selected row then the filename */
     clist = lookup_widget(GTK_WIDGET(button), "file_close_list");
     sel = GTK_CLIST(clist)->selection;
     if ( ! sel ) {
	  on_file_close_button_cancel_clicked(button, user_data);
	  return;
     }
     gtk_clist_get_text( GTK_CLIST(clist), GPOINTER_TO_INT(sel->data), 
			 2, &fname);
     if ( ! fname ) {
	  on_file_close_button_cancel_clicked(button, user_data);
	  return;
     }

     /* abort if the file is not loaded */
     if ( (fnode = tree_find(ghchoice_getloadedfiles(), fname))
	  						== TREE_NOVAL ) {
	  elog_printf(ERROR, "unable to remove %s; suggest restarting", fname);
     }

     /* see if the current active data is part of the unloading file */
     if (uichoice_isancestor(fnode, datapres_node)) {
	  /* it is!! force the selection of a neutral choice, such as
	   * the myfiles node, or the node's parent (even better). */
	  /*g_print("need to remove display\n");*/
	  node = fnode->parent;
	  gtkaction_choice_deselect();
     }

     /* double check displayed data */
     if (uichoice_isancestor(datapres_node, fnode)) {
	  g_print("warning: unsafe!, node not removed\n");
     }

     /* update gui, removing the file node and children */
     treeitem = GTK_CTREE_NODE(*(GtkCTreeNode **) uichoice_getnodearg
			       (fnode, GTKACTION_GUIITEMKEY) );
     /*gtkaction_deletechoice(GTK_CTREE(tree), treeitem);*/
     gtk_ctree_remove_node(GTK_CTREE(tree), treeitem);

     /* unload file, which removes the choice node */
     ghchoice_unloadfile(fname);

     on_file_close_button_cancel_clicked(button, user_data);
}


void
on_file_close_button_cancel_clicked    (GtkButton       *button,
                                        gpointer         user_data)
{
     gtk_widget_hide(file_close_dialog);
     gtk_widget_destroy(file_close_dialog);
     gtkaction_clearprogress();
}



#if 0
void
on_edtree_selection_changed            (GtkTree         *tree,
                                        gpointer         user_data)
{
  gtk_tree_unselect_child( GTK_TREE(edtree), 
			   GTK_WIDGET (GTK_TREE_SELECTION(edtree)->data) );
}
#endif


void
on_ctl_zoomin_x_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
     gmcgraph_allgraph_zoomin_x(graph);
}


void
on_ctl_zoomout_x_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
     gmcgraph_allgraph_zoomout_x(graph);
}


void
on_ctl_zoomin_y_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
     gmcgraph_allgraph_zoomin_y(graph);
}


void
on_ctl_zoomout_y_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
     gmcgraph_allgraph_zoomout_y(graph);
}


void
on_ctl_morewidgets_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *lesswidgets;

     lesswidgets = lookup_widget(GTK_WIDGET(button), "ctl_lesswidgets");
     gtkaction_graphattr_morewidgets(GTK_TABLE(datapres_widget), 
				     datapres_data);
     gtk_widget_hide(GTK_WIDGET(button));
     gtk_widget_show(lesswidgets);
}


void
on_ctl_lesswidgets_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *morewidgets;

     morewidgets = lookup_widget(GTK_WIDGET(button), "ctl_morewidgets");
     gtkaction_graphattr_lesswidgets(GTK_TABLE(datapres_widget));
     gtk_widget_hide(GTK_WIDGET(button));
     gtk_widget_show(morewidgets);
}


gboolean
on_edtree_button_press_event           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
     GtkWidget *item;

#if 0
     printf("on_edtree_button_press_event()\n");
     gtk_signal_emit_stop_by_name( GTK_OBJECT(widget), "button_press_event");
#endif

     /* expand the child which brought the event */
     item = gtk_get_event_widget ((GdkEvent*) event);

     while (item && !GTK_IS_TREE_ITEM (item))
	  item = item->parent;
  
     if (!item || (item->parent != widget))
	  return TRUE;

     if (GTK_TREE_ITEM(item)->subtree)
	  gtk_tree_item_expand(GTK_TREE_ITEM(item));

     return TRUE;
}


void
on_tree_select_row                     (GtkCTree        *ctree,
                                        GList           *node,
                                        gint             column,
                                        gpointer         user_data)
{
     struct uichoice_node *uic;

     /* for now, make the select expand as well as displaying the node */
     gtk_ctree_expand(ctree, GTK_CTREE_NODE(node));

     /* get uichoice_node reference from gui GtkCTree node */
     uic = gtk_ctree_node_get_row_data(GTK_CTREE(tree), GTK_CTREE_NODE(node));
     if (uic == NULL) {
	  elog_printf(ERROR, "unable to get uichoice node");
	  return;
     }

     gtkaction_choice_select(GTK_CTREE_NODE(node), uic);
}


void
on_tree_expand                         (GtkCTree        *ctree,
                                        GList           *node,
                                        gpointer         user_data)
{
     gtkaction_expandchoice(GTK_CTREE_NODE(node), 1, tooltips);
}

void
on_save_viewed_data_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     data_save_window = create_data_save_window();
     gtk_widget_show (data_save_window);
}


void
on_send_data_to_application_activate   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     data_app_window = create_data_app_window();
     gtk_widget_show (data_app_window);
}


void
on_data_app_action_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
     /* process the export action button */
     GtkWidget *data_app_file_types;
     GtkWidget *data_app_cmd;
     GtkWidget *data_app_progress;
     GtkWidget *data_app_title_opt;
     GtkWidget *data_app_info_opt;
     GtkWidget *data_app_time_opt;
     GtkWidget *data_app_seq_opt;
     GtkWidget *mtype = NULL;
     char *cmd, *mtypestr;
     int r, fd, withtitle, withruler, withseq, withtime, docsv;
     char *buf, tmpfilename[20], full_cmd[1024];

     data_app_file_types=lookup_widget(GTK_WIDGET(button),"data_app_file_types");
     data_app_cmd       =lookup_widget(GTK_WIDGET(button),"data_app_cmd");
     data_app_progress  =lookup_widget(GTK_WIDGET(button),"data_app_progress");
     data_app_title_opt =lookup_widget(GTK_WIDGET(button),"data_app_title_opt");
     data_app_info_opt  =lookup_widget(GTK_WIDGET(button),"data_app_info_opt");
     data_app_time_opt  =lookup_widget(GTK_WIDGET(button),"data_app_time_opt");
     data_app_seq_opt   =lookup_widget(GTK_WIDGET(button),"data_app_seq_opt");

     /* find the output type selected */
     /* This is voodoo!!
      * The option is held in an GtkAccelLabel in a GtkMenuItem connected
      * to a GtkOptionMenu. Rather than tunnelling down the list, the 
      * GtkOptionMenu reparents the active label for a while, so all
      * we have to do is dig out the contents of GtkOptionMenu's bin (single
      * child) then cast down cast it to a lable and aquire the text.
      * Confused?? anyway, it appears to work.
      */
     if (GTK_BIN (data_app_file_types)->child == NULL) {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_app_progress), 
					  "output file type not set");
	  return;
     } else {
	  mtype = GTK_BIN (data_app_file_types)->child;
	  gtk_label_get (GTK_LABEL (mtype), &mtypestr);
     }

     /* set up options for export */
     withtitle = GTK_TOGGLE_BUTTON(data_app_title_opt)->active;
     withruler = GTK_TOGGLE_BUTTON(data_app_info_opt)->active;
     withtime  = GTK_TOGGLE_BUTTON(data_app_time_opt)->active;
     withseq   = GTK_TOGGLE_BUTTON(data_app_seq_opt)->active;

     /* check that the fields have been filled in correctly */
     cmd = gtk_entry_get_text( GTK_ENTRY(data_app_cmd) );
     if (cmd == NULL || *cmd == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_app_progress), 
					  "Need a command line");
	  return;
     }

     /* process specific export types, using first a tablestore method
      * then the timestore if that fails */
     if (strncmp(mtypestr, "csv", 3) == 0) {
          docsv = 1;
     } else if (strncmp(mtypestr, "tsv", 3) == 0) {
          docsv = 0;
     } else if (strncmp(mtypestr, "fha", 3) == 0) {
          docsv = 0;
     } else {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_app_progress), 
					  "output data format not set");
	  return;
     }

     /* carry out the work */

     /* convert RESDAT into text */
     buf = gtkaction_resdat2text(datapres_data, withtime, withseq, 
				 withtitle, withruler, docsv);
     if (buf == NULL || *buf == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_app_progress), 
					  "strangely, no text data to send!");
	  return;
     }

     /* write the data to a temp place */
     strcpy(tmpfilename, "iiabXXXXXX");
     fd = mkstemp(tmpfilename);
     if (fd == -1) {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_app_progress), 
					  "unable to create temp file");
	  nfree(buf);
	  return;
     }
     r = write(fd, buf, strlen(buf));
     close(fd);
     if (r == -1) {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_app_progress), 
					  "unable to write temp file");
	  nfree(buf);
	  return;
     }

     /* construct the command line with the expanded file name
      * and a trailing command that will remove the temp file
      * make it all run in the background as a sub shell so
      * the commands run in sequence & exit without our 
      * program being involved */
     sprintf(full_cmd, "(%s; rm %s)&", cmd, tmpfilename);
     util_strsub(full_cmd, "%f", tmpfilename);

     /* execute */
     r = system(full_cmd);
     if (r <= 0)
	  gtk_progress_set_format_string( GTK_PROGRESS(data_app_progress), 
					  "couldn't write all data");
     else
	  gtk_progress_set_format_string( GTK_PROGRESS(data_app_progress), 
					  "data sent to application");

#if 0
     /* keep file for later */
     uichoice_add_myfiles_list(file);
     uichoice_add_myfiles_list(hol);
#endif 
}


void
on_data_app_finished_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
     gtk_widget_hide (data_app_window);
     gtk_widget_destroy (data_app_window);
}


void
on_data_app_help_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
     gtkaction_browse_help(HELP_DATA_APP);
}


void
on_send_data_to_email_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     data_email_window = create_data_email_window();
     gtk_widget_show (data_email_window);
}



void
on_data_email_action_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
     /* process the export action button */
     GtkWidget *data_email_file_type;
     GtkWidget *data_email_to;
     GtkWidget *data_email_cc;
     GtkWidget *data_email_subject;
     GtkWidget *data_email_progress;
     GtkWidget *data_email_title_opt;
     GtkWidget *data_email_info_opt;
     GtkWidget *data_email_time_opt;
     GtkWidget *data_email_seq_opt;
     GtkWidget *mtype = NULL;
     char *to, *cc, *subject, *mtypestr, *mailer, *cmd;
     int r, withtitle, withruler, withseq, withtime, docsv;
     char *buf;
     FILE *fs;

     data_email_file_type = lookup_widget(GTK_WIDGET(button), "data_email_file_type");
     data_email_to        = lookup_widget(GTK_WIDGET(button), "data_email_to");
     data_email_cc        = lookup_widget(GTK_WIDGET(button), "data_email_cc");
     data_email_subject   = lookup_widget(GTK_WIDGET(button), "data_email_subject");
     data_email_progress  = lookup_widget(GTK_WIDGET(button), "data_email_progress");
     data_email_title_opt = lookup_widget(GTK_WIDGET(button), "data_email_title_opt");
     data_email_info_opt  = lookup_widget(GTK_WIDGET(button), "data_email_info_opt");
     data_email_time_opt  = lookup_widget(GTK_WIDGET(button), "data_email_time_opt");
     data_email_seq_opt   = lookup_widget(GTK_WIDGET(button), "data_email_seq_opt");

     /* find the output type selected */
     /* This is voodoo!!
      * The option is held in an GtkAccelLabel in a GtkMenuItem connected
      * to a GtkOptionMenu. Rather than tunnelling down the list, the 
      * GtkOptionMenu reparents the active label for a while, so all
      * we have to do is dig out the contents of GtkOptionMenu's bin (single
      * child) then cast down cast it to a lable and aquire the text.
      * Confused?? anyway, it appears to work.
      */
     if (GTK_BIN (data_email_file_type)->child == NULL) {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_email_progress), 
					  "output file type not set");
	  return;
     } else {
	  mtype = GTK_BIN (data_email_file_type)->child;
	  gtk_label_get (GTK_LABEL (mtype), &mtypestr);
     }

     /* set up options for export */
     withtitle = GTK_TOGGLE_BUTTON(data_email_title_opt)->active;
     withruler = GTK_TOGGLE_BUTTON(data_email_info_opt)->active;
     withtime  = GTK_TOGGLE_BUTTON(data_email_time_opt)->active;
     withseq   = GTK_TOGGLE_BUTTON(data_email_seq_opt)->active;

     /* check that the fields have been filled in correctly */
     to = gtk_entry_get_text( GTK_ENTRY(data_email_to) );
     if (to == NULL || *to == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_email_progress), 
					  "Need a `to' e-mail address");
	  return;
     }
     cc = gtk_entry_get_text( GTK_ENTRY(data_email_cc) );
     subject = gtk_entry_get_text( GTK_ENTRY(data_email_subject) );

     /* check to see if mail or sendmail is there */
     if (access("/bin/mail", X_OK) == 0)
          mailer = "/bin/mail";
     else if (access("/usr/bin/Mail", X_OK) == 0)
          mailer = "/usr/bin/mail";
     else if (access("/usr/sbin/sendmail", X_OK) == 0)
          mailer = "/usr/sbin/sendmail";
     else {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_email_progress), 
					  "Can't find a suitable e-mail client");
	  return;
     }

     /* process specific export types */
     if (strncmp(mtypestr, "csv", 3) == 0) {
          docsv = 1;
     } else if (strncmp(mtypestr, "tsv", 3) == 0) {
          docsv = 0;
     } else if (strncmp(mtypestr, "fha", 3) == 0) {
          docsv = 0;
     } else {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_email_progress), 
					  "output data format not set");
	  return;
     }

     /* carry out the work */

     /* convert RESDAT into text */
     buf = gtkaction_resdat2text(datapres_data, withtime, withseq, withtitle, withruler, 
				 docsv);
     if (buf == NULL || *buf == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_email_progress), 
					  "strangely, no text data to send!");
	  return;
     }

     /* send the data on its way */
     cmd = util_strjoin(mailer, " -s \"", subject, "\" -c \"", cc, "\" \"", 
			to, "\"", NULL);
     fs = popen(cmd, "w");
     if (fs == NULL) {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_email_progress), 
					  "unable to run e-mail command");
	  nfree(buf);
	  return;
     }
     r = fwrite(buf, strlen(buf), 1, fs);
     pclose(fs);
     nfree(buf);

     /* inform user of conclusion */
     if (r <= 0)
	  gtk_progress_set_format_string( GTK_PROGRESS(data_email_progress), 
					  "unable to e-mail data");
     else
	  gtk_progress_set_format_string( GTK_PROGRESS(data_email_progress), 
					  "data sent by e-mail");

#if 0
     /* keep file for later */
     uichoice_add_myfiles_list(file);
     uichoice_add_myfiles_list(hol);
#endif 
}


void
on_data_email_finished_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
     gtk_widget_hide (data_email_window);
     gtk_widget_destroy (data_email_window);
}


void
on_data_email_help_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
     gtkaction_browse_help(HELP_DATA_EMAIL);
}


void
on_data_save_filesel_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *data_save_file_select;

     data_save_file_select = create_file_data_save_select();
     gtk_widget_show (data_save_file_select);
}


void
on_data_save_action_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
     /* process the export action button */
     GtkWidget *data_save_file_types;
     GtkWidget *data_save_file;
     GtkWidget *data_save_progress;
     GtkWidget *data_save_title_opt;
     GtkWidget *data_save_info_opt;
     GtkWidget *data_save_time_opt;
     GtkWidget *data_save_seq_opt;
     GtkWidget *mtype = NULL;
     char *file, *mtypestr;
     int r, withtitle, withruler, withseq, withtime, docsv;
     char *buf;
     FILE *fs;

     data_save_file_types = lookup_widget(GTK_WIDGET(button), "data_save_file_types");
     data_save_file       = lookup_widget(GTK_WIDGET(button), "data_save_file");
     data_save_progress   = lookup_widget(GTK_WIDGET(button), "data_save_progress");
     data_save_title_opt  = lookup_widget(GTK_WIDGET(button), "data_save_title_opt");
     data_save_info_opt   = lookup_widget(GTK_WIDGET(button), "data_save_info_opt");
     data_save_time_opt   = lookup_widget(GTK_WIDGET(button), "data_save_time_opt");
     data_save_seq_opt    = lookup_widget(GTK_WIDGET(button), "data_save_seq_opt");

     /* find the output type selected */
     /* This is voodoo!!
      * The option is held in an GtkAccelLabel in a GtkMenuItem connected
      * to a GtkOptionMenu. Rather than tunnelling down the list, the 
      * GtkOptionMenu reparents the active label for a while, so all
      * we have to do is dig out the contents of GtkOptionMenu's bin (single
      * child) then cast down cast it to a lable and aquire the text.
      * Confused?? anyway, it appears to work.
      */
     if (GTK_BIN (data_save_file_types)->child == NULL) {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_save_progress), 
					  "output file type not set");
	  return;
     } else {
	  mtype = GTK_BIN (data_save_file_types)->child;
	  gtk_label_get (GTK_LABEL (mtype), &mtypestr);
     }

     /* set up options for export */
     withtitle = GTK_TOGGLE_BUTTON(data_save_title_opt)->active;
     withruler = GTK_TOGGLE_BUTTON(data_save_info_opt)->active;
     withtime  = GTK_TOGGLE_BUTTON(data_save_time_opt)->active;
     withseq   = GTK_TOGGLE_BUTTON(data_save_seq_opt)->active;

     /* check that the fields have been filled in correctly */
     file = gtk_entry_get_text( GTK_ENTRY(data_save_file) );
     if (file == NULL || *file == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_save_progress), 
					  "Need a file name");
	  return;
     }

     /* process specific export types, */
     if (strncmp(mtypestr, "csv", 3) == 0) {
          docsv = 1;
     } else if (strncmp(mtypestr, "tsv", 3) == 0) {
          docsv = 0;
     } else if (strncmp(mtypestr, "fha", 3) == 0) {
          docsv = 0;
     } else {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_save_progress), 
					  "output file type not set");
	  return;
     }

     /* carry out the work */

     /* convert RESDAT into text */
     buf = gtkaction_resdat2text(datapres_data, withtime, withseq, withtitle, 
				 withruler, docsv);
     if (buf == NULL || *buf == '\0') {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_save_progress), 
					  "strangely, no text data to save!");
	  return;
     }

     /* save the data in a file */
     fs = fopen(file, "w");
     if (fs == NULL) {
	  gtk_progress_set_format_string( GTK_PROGRESS(data_save_progress), 
					  "unable to write to file");
	  nfree(buf);
	  return;
     }
     r = fwrite(buf, strlen(buf), 1, fs);
     fclose(fs);
     nfree(buf);

     /* inform user of conclusion */
     if (r <= 0)
	  gtk_progress_set_format_string( GTK_PROGRESS(data_save_progress), 
					  "couldn't write all data");
     else
	  gtk_progress_set_format_string( GTK_PROGRESS(data_save_progress), 
					  "file written");

#if 0
     /* keep file for later */
     uichoice_add_myfiles_list(file);
     uichoice_add_myfiles_list(hol);
#endif 
}


void
on_data_save_finished_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
     gtk_widget_hide (data_save_window);
     gtk_widget_destroy (data_save_window);
}


void
on_data_save_help_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
     gtkaction_browse_help(HELP_DATA_SAVE);
}


void
on_file_data_save_okbutton_clicked     (GtkButton       *button,
                                        gpointer         user_data)
{
  char *fname;
  GtkWidget *data_save_file_name, *data_save_file_combo;
  /*GList *files;*/

  /* get file name selection and some widget pointers */
  fname = gtk_file_selection_get_filename( GTK_FILE_SELECTION(user_data) );
  data_save_file_name = lookup_widget(GTK_WIDGET(button), "data_save_file");
  data_save_file_combo = lookup_widget(GTK_WIDGET(button), "data_save_combo");

  /* set text field and pop-down list of combo box */
  if (fname)
    gtk_entry_set_text(GTK_ENTRY(data_save_file_name), fname);
#if 0
MAKES IT COREDUMP AS NOT A LIS AT FIRST. 
NEED TO WORK OUT HOW TO APPEND PROPERLY
  files = g_list_append(GTK_COMBO(data_save_file_combo)->list, fname);
  gtk_combo_set_popdown_strings(GTK_COMBO(data_save_file_combo), files);
#endif
  
  /* remove the file selection box */
  gtk_widget_hide(GTK_WIDGET(user_data));
  gtk_widget_destroy(GTK_WIDGET(user_data));
}


void
on_file_data_save_cancelbutton_clicked (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_hide(GTK_WIDGET(user_data));
  gtk_widget_destroy(GTK_WIDGET(user_data));
}


void
on_host_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     TREE *history;
     GList *hist = NULL;
     GtkWidget *open_host_combo;

     open_host_window = create_open_host_window();
     open_host_combo = lookup_widget(open_host_window, "open_host_combo");

     /* fill host list with 'my hosts' history */
     history = ghchoice_get_myhosts_list();
     tree_traverse(history)
	  hist = g_list_append(hist, tree_getkey(history));
     if (hist) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(open_host_combo), hist);
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(open_host_combo)->entry), "");
     }

     gtk_widget_show(open_host_window);

     g_list_free(hist);
}


void
on_route_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     TREE *history;
     GList *hist = NULL;
     GtkWidget *open_route_combo;

     open_route_window = create_open_route_window();
     open_route_combo = lookup_widget(open_route_window, "open_route_combo");

     /* fill host list with 'my hosts' history */
     history = ghchoice_get_myhosts_list();
     tree_traverse(history)
	  hist = g_list_append(hist, tree_get(history));
     if (hist) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(open_route_combo), hist);
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(open_route_combo)->entry),"");
     }

     gtk_widget_show(open_route_window);

     g_list_free(hist);
}


void
on_repository_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *repos_prop_window;

     repos_prop_window = create_repos_prop_window();
     gtk_widget_show(repos_prop_window);
}


void
on_open_host_open_btn_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
     int r=0;
     char *hostname, purl[128];
     struct uichoice_node *hostnode, *myhosts=NULL;
     GtkCTreeNode *myhosts_treeitem;
     GtkWidget *open_host_name, *open_host_source_repository;
     int from_repository;

     /* move the furniture */
     gtkaction_setprogress("searching for host", 0.0, 0);
     gtk_widget_hide (open_host_window);

     /* find myhosts node */
     myhosts = uichoice_findlabel_all("my hosts");
     if (myhosts == NULL) {
	  elog_printf(ERROR, "unable to find myhost node to attach");
	  gtkaction_clearprogress();
	  return;
     }

     /* get route name */
     open_host_name = lookup_widget(GTK_WIDGET(button), "open_host_name");
     hostname = gtk_entry_get_text( GTK_ENTRY(open_host_name) );

     /* find the host mode: repository or direct and load the 
      * correct address */
     open_host_source_repository = lookup_widget(GTK_WIDGET(button), 
						 "open_host_source_repository");
     from_repository = GTK_TOGGLE_BUTTON (open_host_source_repository)->active;
     if (from_repository)
	  snprintf(purl, 128, "sqlrs:%s", hostname);
     else
	  snprintf(purl, 128, "http://%s:%d/localtsv/", hostname, 
		   HTTPD_PORT_HTTP);
     hostnode = ghchoice_loadroute(purl, hostname, myhosts, &r);

     /* process return code from the load */
     switch (r) {
     case 1:
	  /* route successfully added to choice tree, now the gui has 
	   * to catch up. 
	   * Find the corresponding myhosts tree item, and if a sub tree has
	   * not been created (no children) then create one.
	   * Then make a new node corresponding to the new host and expose
	   * the filenames in the tree */
	  myhosts_treeitem = GTK_CTREE_NODE(*(GtkCTreeNode **) 
					    uichoice_getnodearg
					    (myhosts, GTKACTION_GUIITEMKEY) );

	  gtkaction_makechoice(myhosts_treeitem, hostnode, tooltips);
	  gtk_ctree_expand(GTK_CTREE(tree), myhosts_treeitem);
	  break;
     case -1:
          elog_printf(ERROR, "Unable to read %s %s", hostname, 
		      from_repository ? "from repository" : "directly");
	  break;
     case -2:
	  elog_printf(WARNING, "%s has already been loaded", hostname);
	  break;
     case 0:
     default:
          elog_printf(ERROR, "Failed to read %s %s", hostname, 
		      from_repository ? "from repository" : "directly");
	  break;
     }

     gtk_widget_destroy (open_host_window);
     gtkaction_clearprogress();
}


void
on_open_host_finished_btn_clicked      (GtkButton       *button,
                                        gpointer         user_data)
{
     gtk_widget_hide (open_host_window);
     gtk_widget_destroy (open_host_window);
}


void
on_open_host_help_btn_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
     gtkaction_browse_help(HELP_OPEN_HOST);
}



void
on_open_route_open_btn_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
     int r=0;
     char *purl;
     struct uichoice_node *hostnode, *myhosts=NULL;
     GtkCTreeNode *myhosts_treeitem;
     GtkWidget *open_route_purl;

     /* move the furniture */
     gtkaction_setprogress("searching for route", 0.0, 0);
     gtk_widget_hide (open_route_window);

     /* find myhosts node */
     myhosts = uichoice_findlabel_all("my hosts");
     if (myhosts == NULL) {
	  elog_printf(ERROR, "unable to find myhost node to attach");
	  gtkaction_clearprogress();
	  return;
     }

     /* get route name & load */
     open_route_purl = lookup_widget(GTK_WIDGET(button), "open_route_purl");
     purl = gtk_entry_get_text( GTK_ENTRY(open_route_purl) );
     hostnode = ghchoice_loadroute(purl, purl, myhosts, &r);

     /* process return code from the load */
     switch (r) {
     case 1:
	  /* route successfully added to choice tree, now the gui has 
	   * to catch up. 
	   * Find the corresponding myhosts tree item, and if a sub tree has
	   * not been created (no children) then create one.
	   * Then make a new node corresponding to the new host and expose
	   * the filenames in the tree */
	  myhosts_treeitem = GTK_CTREE_NODE(*(GtkCTreeNode **) 
					    uichoice_getnodearg
					    (myhosts, GTKACTION_GUIITEMKEY) );

	  gtkaction_makechoice(myhosts_treeitem, hostnode, tooltips);
	  gtk_ctree_expand(GTK_CTREE(tree), myhosts_treeitem);
	  break;
     case -1:
	  elog_printf(ERROR, "Unable to read %s", purl);
	  break;
     case -2:
	  elog_printf(WARNING, "%s has already been loaded", purl);
	  break;
     case 0:
     default:
	  elog_printf(ERROR, "Error loading %s", purl);
	  break;
     }

     gtk_widget_destroy (open_route_window);
     gtkaction_clearprogress();
}


void
on_open_route_finished_btn_clicked     (GtkButton       *button,
                                        gpointer         user_data)
{
     gtk_widget_hide (open_route_window);
     gtk_widget_destroy (open_route_window);
}


void
on_open_route_help_btn_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
     gtkaction_browse_help(HELP_OPEN_ROUTE);
}


void
on_readme_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_help(HELP_README);
}


void
on_about_harvest_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_sign_up_to_harvest_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_start_clockwork_once_button_clicked (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *w, *w_view, *w_dontask;
     GtkCTreeNode *myfiles_treeitem;
     int view, dontask, r;
     struct uichoice_node *filenode, *myfiles=NULL;
     char *template, filepath[256];

     /* read widget refs and find the values */
     w        =lookup_widget(GTK_WIDGET(button),"start_clockwork_window");
     w_view   =lookup_widget(GTK_WIDGET(button),"start_clockwork_view_opt");
     w_dontask=lookup_widget(GTK_WIDGET(button),"start_clockwork_dontask_opt");
     view     = GTK_TOGGLE_BUTTON(w_view)->active;
     dontask  = GTK_TOGGLE_BUTTON(w_dontask)->active;

     if (view) {
          /* add the local file to the choice tree; it will be 
	   * automatically saved on exit */
          myfiles = uichoice_findlabel_all("my files");
	  if (!myfiles) 
	       elog_printf(WARNING, "unable to find myfile node to attach");

	  /* construct conventional filepath */
	  template = util_strjoin(iiab_dir_var, "/%h.ts", NULL);
	  route_expand(filepath, template, "NOJOB", 0);
	  nfree(template);
	  filenode = ghchoice_loadfile(filepath, myfiles, &r);

	  /* process return code from the load */
	  switch (r) {
	  case 1:
	       /* success - now draw on GUI */
	       myfiles_treeitem = GTK_CTREE_NODE(*(GtkCTreeNode **) 
						uichoice_getnodearg
					(myfiles, GTKACTION_GUIITEMKEY) );
	       gtkaction_makechoice(myfiles_treeitem, filenode, tooltips);
	       gtk_ctree_expand(GTK_CTREE(tree), myfiles_treeitem);
	       break;
	  case -1:
	       elog_printf(ERROR, "Unable to read %s", filepath);
	       break;
	  case -2:
	       elog_printf(INFO, "%s has already been loaded", filepath);
	       break;
	  case 0:
	  default:
	       elog_printf(ERROR, "Error loading %s", filepath);
	       break;
	  }
     }

     if (dontask) {
          /* specificly save the 'dont ask' state to the config file */
          cf_putint(iiab_cf, DONTASKCLOCKWORK_CFNAME, -1);
          iiab_usercfsave (iiab_cf, DONTASKCLOCKWORK_CFNAME);
     } else if (cf_defined(iiab_cf, DONTASKCLOCKWORK_CFNAME) &&
		cf_getint (iiab_cf, DONTASKCLOCKWORK_CFNAME) != 0) {
          /* save an 'ask' state, if one has already been set */
          cf_putint(iiab_cf, DONTASKCLOCKWORK_CFNAME, 0);
	  iiab_usercfsave (iiab_cf, DONTASKCLOCKWORK_CFNAME);
     }

     gtk_widget_hide(w);
     gtk_widget_destroy(w);
     gtkaction_startclockwork();
}


void
on_start_clockwork_always_button_clicked
                                        (GtkButton       *button,
                                        gpointer         user_data)
{
     /* call the 'start once' cb as this behaviour is identical*/
     on_start_clockwork_once_button_clicked (button, user_data);

     /* now set auto clockwork start */
     cf_putint(iiab_cf, AUTOCLOCKWORK_CFNAME, -1);
     iiab_usercfsave(iiab_cf, AUTOCLOCKWORK_CFNAME);
}


void
on_start_clockwork_dont_button_clicked (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *w, *w_dontask;
     int dontask;

     w = lookup_widget(GTK_WIDGET(button), "start_clockwork_window");
     w_dontask=lookup_widget(GTK_WIDGET(button),"start_clockwork_dontask_opt");
     dontask  = GTK_TOGGLE_BUTTON(w_dontask)->active;

     gtk_widget_hide(w);
     gtk_widget_destroy(w);

     if (dontask) {
          /* specificly save the 'dont ask' state to the config file */
          cf_putint(iiab_cf, DONTASKCLOCKWORK_CFNAME, -1);
          iiab_usercfsave (iiab_cf, DONTASKCLOCKWORK_CFNAME);
     } else if (cf_defined(iiab_cf, DONTASKCLOCKWORK_CFNAME) &&
		cf_getint (iiab_cf, DONTASKCLOCKWORK_CFNAME) != 0) {
          /* save an 'ask' state, if one has already been set */
          cf_putint(iiab_cf, DONTASKCLOCKWORK_CFNAME, 0);
	  iiab_usercfsave (iiab_cf, DONTASKCLOCKWORK_CFNAME);
     }
}


void
on_stop_clockwork_now_button_clicked   (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *w;

     w = lookup_widget(GTK_WIDGET(button), "stop_clockwork_window");
     gtk_widget_hide(w);
     gtk_widget_destroy(w);
     gtkaction_stopclockwork();
}


void
on_stop_clockwork_noauto_button_clicked
                                        (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *w;

     /* now set auto clockwork start */
     cf_putint(iiab_cf, AUTOCLOCKWORK_CFNAME, 0);
     iiab_usercfsave(iiab_cf, AUTOCLOCKWORK_CFNAME);

     /* close the window */
     w = lookup_widget(GTK_WIDGET(button), "stop_clockwork_window");
     gtk_widget_hide(w);
     gtk_widget_destroy(w);
}


void
on_stop_clockwork_continue_button_clicked
                                        (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *w;

     w = lookup_widget(GTK_WIDGET(button), "stop_clockwork_window");
     gtk_widget_hide(w);
     gtk_widget_destroy(w);
}


void
on_alert_ok_button_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *w;

     w = lookup_widget(GTK_WIDGET(button), "alert_window");
     gtk_widget_hide(w);
     gtk_widget_destroy(w);
}



void
on_logging_level_normal_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     elog_setsevpurl(DEBUG, "none:");
     elog_setsevpurl(DIAG,  "none:");
}


void
on_logging_level_high_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     elog_setsevpurl(DEBUG, "none:");
     elog_setsevpurl(DIAG,  "gtkgui:");
}


void
on_logging_level_higher_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     elog_setsevpurl(DEBUG, "gtkgui:");
     elog_setsevpurl(DIAG,  "gtkgui:");
}


void
on_manual_pages_ghabitat_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_man(MAN_GHABITAT);
}


void
on_manual_pages_clockwork_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_man(MAN_CLOCKWORK);
}


void
on_manual_pages_habget_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_man(MAN_HABGET);
}


void
on_manual_pages_habput_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_man(MAN_HABPUT);
}


void
on_manual_pages_configuration_activate (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_man(MAN_CONFIG);
}

void
on_user_manual_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_web(WEB_USAGE);
}


void
on_web_system_garden_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_web(WEB_SYSGAR);
}


void
on_web_habitat_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_web(WEB_HABITAT);
}


void
on_web_harvest_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     gtkaction_browse_web(WEB_HARVEST);
}


void
on_log_popup_button_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *log_popup_window;
     GtkWidget *log_popup_table;
     GtkWidget *log_popup_coloured;
     GtkWidget *log_popup_detailed;
     GtkWidget *log_popup_sev_entry;
     GtkWidget *log_popup_collect_entry;
     int coloured;
     enum elog_severity sev;
     char *sevtext, *sevroute;

     if (gtkaction_log_popup_available()) {
	  elog_printf(INFO, "log window already visible");
	  return;
     }

     /* create window and find widgets */
     log_popup_window    = create_log_popup_window();
     log_popup_table     = lookup_widget(log_popup_window, "log_popup_table");
     log_popup_coloured  = lookup_widget(log_popup_window, 
					 "log_popup_coloured");
     log_popup_detailed  = lookup_widget(log_popup_window, 
					 "log_popup_detailed");
     log_popup_sev_entry = lookup_widget(log_popup_window, 
					 "log_popup_sev_entry");
     log_popup_collect_entry = lookup_widget(log_popup_window, 
					 "log_popup_collect_entry");

     /* set collect severity combo from elog's routes */
     sevroute = elog_getpurl(DEBUG);
     if (strcmp(sevroute, "gtkgui:") == 0) {
	  /* if DEBUG==gtkgui:, assume higher */
	  sevtext = "Higher (Debug)";
     } else {
	  sevroute = elog_getpurl(DIAG);
	  if (strcmp(sevroute, "gtkgui:") == 0) {
	       /* if DIAG==gtkgui: and DEBUG!=gtkgui:, assume high */
	       sevtext = "High (Diagnostic)";
	  } else {
	       /* if DIAG!=gtkgui: and DEBUG!=gtkgui:, assume normal */
	       sevtext = "Normal (Information)";
	  }
     }
     gtk_signal_handler_block_by_func(GTK_OBJECT(log_popup_collect_entry),
		      GTK_SIGNAL_FUNC (on_log_popup_collect_entry_changed),
                      NULL);
     gtk_entry_set_text(GTK_ENTRY(log_popup_collect_entry), sevtext);
     gtk_signal_handler_unblock_by_func(GTK_OBJECT(log_popup_collect_entry),
		      GTK_SIGNAL_FUNC (on_log_popup_collect_entry_changed),
                      NULL);

     /* draw the log text table and notify gtkaction of its appearence 
	for live updates */
     gtkaction_log_popup_draw(log_popup_table, NOELOG, -1);
     gtkaction_log_popup_created(log_popup_table);

     /* get severity and colouring states from gtkaction and set the 
	severity view combo (pulldown) */
     gtkaction_log_popup_state(&sev, &coloured);
     if (sev != NOELOG && sev != DEBUG) {
	  switch (sev) {
	  case FATAL:
	       sevtext = "Fatal only";
	       break;
	  case ERROR:
	       sevtext = "Error +";
	       break;
	  case WARNING:
	       sevtext = "Warning +";
	       break;
	  case INFO:
	       sevtext = "Information +";
	       break;
	  case DIAG:
	       sevtext = "Diagnostic +";
	       break;
	  default:
	       elog_printf(ERROR, "don't know the severity %d", sev);
	       break;
	  }
	  gtk_signal_handler_block_by_func(GTK_OBJECT(log_popup_sev_entry),
		      GTK_SIGNAL_FUNC (on_log_popup_sev_entry_changed),
                      NULL);
	  gtk_entry_set_text(GTK_ENTRY(log_popup_sev_entry), sevtext);
	  gtk_signal_handler_unblock_by_func(GTK_OBJECT(log_popup_sev_entry),
		      GTK_SIGNAL_FUNC (on_log_popup_sev_entry_changed),
                      NULL);
     }

     /* Set coloured button using state from gtkaction (see above) */
     if (coloured) {
	  gtk_signal_handler_block_by_func(GTK_OBJECT(log_popup_coloured),
		      GTK_SIGNAL_FUNC (on_log_popup_coloured_toggled),
                      NULL);
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(log_popup_coloured),
				       TRUE);
	  gtk_signal_handler_unblock_by_func(GTK_OBJECT(log_popup_coloured),
		      GTK_SIGNAL_FUNC (on_log_popup_coloured_toggled),
                      NULL);
     }

     /* set detailed button from local static state; 
      * if detail is off, hide the extra columns */
     if (log_popup_detailed_state) {
	  gtk_signal_handler_block_by_func(GTK_OBJECT(log_popup_detailed),
		      GTK_SIGNAL_FUNC (on_log_popup_detailed_toggled),
                      NULL);
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(log_popup_detailed),
				       TRUE);
	  gtk_signal_handler_unblock_by_func(GTK_OBJECT(log_popup_detailed),
		      GTK_SIGNAL_FUNC (on_log_popup_detailed_toggled),
                      NULL);
     } else {
	  gtk_clist_set_column_visibility(GTK_CLIST(log_popup_table), 3, 0);
	  gtk_clist_set_column_visibility(GTK_CLIST(log_popup_table), 4, 0);
	  gtk_clist_set_column_visibility(GTK_CLIST(log_popup_table), 5, 0);
     }

     /* now show the results of our labours and attach a window manager icon */
     gtk_widget_show(log_popup_window);
     gtkaction_anypopup_setwmicon(log_popup_window);
}


void
on_log_popup_collect_entry_changed     (GtkEditable     *editable,
                                        gpointer         user_data)
{
     GtkWidget *log_popup_table;
     char *s;

     /* find the setting of the combo entry */
     log_popup_table = lookup_widget(GTK_WIDGET(editable), 
				     "log_popup_table");
     s = gtk_entry_get_text(GTK_ENTRY(editable));

     /* work out the code that represents the setting */
     switch (*s) {
     case 'N':
	  /* Normal */
	  elog_setsevpurl(DEBUG, "none:");
	  elog_setsevpurl(DIAG,  "none:");
	  break;
     case 'H':
	  if (s[4] == 'e') {
	       /* Higher */
	       elog_setsevpurl(DEBUG, "gtkgui:");
	       elog_setsevpurl(DIAG,  "gtkgui:");
	  } else {
	       /* High */
	       elog_setsevpurl(DEBUG, "none:");
	       elog_setsevpurl(DIAG,  "gtkgui:");
	  }
	  break;
     default:
	  elog_printf(ERROR, "don't know the severity %s", s);
	  break;
     }
}


void
on_log_popup_sev_entry_changed         (GtkEditable     *editable,
                                        gpointer         user_data)
{
     GtkWidget *log_popup_table;
     char *s;
     enum elog_severity sev;

     /* find the setting of the combo entry */
     log_popup_table = lookup_widget(GTK_WIDGET(editable), 
				     "log_popup_table");
     s = gtk_entry_get_text(GTK_ENTRY(editable));

     /* work out the code that represents the setting */
     sev = NOELOG;
     switch (*s) {
     case 'F':
	  sev = FATAL;
	  break;
     case 'E':
	  sev = ERROR;
	  break;
     case 'W':
	  sev = WARNING;
	  break;
     case 'I':
	  sev = INFO;
	  break;
     case 'D':
	  sev = DIAG;
	  break;
     case 'A':
	  sev = DEBUG;
	  break;
     default:
	  elog_printf(ERROR, "don't know the severity %s", s);
	  break;
     }

     gtkaction_log_popup_draw(log_popup_table, sev, -1);
}


void
on_log_popup_coloured_toggled          (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
     GtkWidget *log_popup_table;
     GtkWidget *w_coloured;
     int coloured;

     log_popup_table = lookup_widget(GTK_WIDGET(togglebutton), 
				     "log_popup_table");
     w_coloured      = lookup_widget(GTK_WIDGET(togglebutton),
				     "log_popup_coloured");
     coloured        = GTK_TOGGLE_BUTTON(w_coloured)->active;

     /* redraw log */
     gtkaction_log_popup_draw(log_popup_table, NOELOG, coloured);
}


void
on_log_popup_detailed_toggled          (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
     GtkWidget *log_popup_table;
     GtkWidget *w_detailed;

     /* find the setting of the detailed switch */
     log_popup_table = lookup_widget(GTK_WIDGET(togglebutton), 
				     "log_popup_table");
     w_detailed      = lookup_widget(GTK_WIDGET(togglebutton), 
				     "log_popup_detailed");
     log_popup_detailed_state  = GTK_TOGGLE_BUTTON(w_detailed)->active;

     /* detail on -- give columns width, off -- remove width */
     if (log_popup_detailed_state) {
	  gtk_clist_set_column_visibility(GTK_CLIST(log_popup_table), 3, 1);
	  gtk_clist_set_column_visibility(GTK_CLIST(log_popup_table), 4, 1);
	  gtk_clist_set_column_visibility(GTK_CLIST(log_popup_table), 5, 1);
     } else {
	  gtk_clist_set_column_visibility(GTK_CLIST(log_popup_table), 3, 0);
	  gtk_clist_set_column_visibility(GTK_CLIST(log_popup_table), 4, 0);
	  gtk_clist_set_column_visibility(GTK_CLIST(log_popup_table), 5, 0);
     }

     /* The window has to be visible already; do not need to redraw */
}


void
on_log_popup_ok_button_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *log_popup_window;

     log_popup_window = lookup_widget(GTK_WIDGET(button),"log_popup_window");
     gtkaction_log_popup_destroyed();
     gtk_widget_hide (log_popup_window);
     gtk_widget_destroy (log_popup_window);
}


/* view logs triggered from menu */
void
on_view_logs_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     on_log_popup_button_clicked(NULL, NULL);
}


void
on_zoom_in_horizontally_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     on_ctl_zoomin_x_clicked(NULL, NULL);
}


void
on_zoom_in_vertically_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     on_ctl_zoomin_y_clicked(NULL, NULL);
}


void
on_zoom_out_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     on_ctl_zoomout_y_clicked(NULL, NULL);
}


/* update the tree branch containg the right-click selected node */
void
on_choice_update_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     struct uichoice_node *node;

     /* find the node structure to which we are referring (tree is global 
      * and has the data attached to it) */
     node = (struct uichoice_node *) gtk_object_get_data (GTK_OBJECT (tree), 
							  "choice_popup_node");
     if (node == NULL) {
	  elog_printf(ERROR, "NULL choice node");
	  return;
     }

     /* at a later date, walk up the tree to update the whole route */

     gtkaction_node_update(node);
}


void
on_choice_remove_activate    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     on_close_activate(menuitem, user_data);
}


void
on_choice_add_file_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     on_open_activate(menuitem, user_data);
}


void
on_choice_add_host_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     on_host_activate(menuitem, user_data);
}


void
on_choice_add_route_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     on_route_activate(menuitem, user_data);
}


void
on_choice_properties_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkWidget *prop_window;
     GtkWidget *prop_name_value;
     GtkWidget *prop_info_value;
     GtkWidget *prop_help_value;
     GtkWidget *prop_node_interval_val;
     GtkWidget *prop_node_built_val;
     GtkWidget *prop_node_refresh_val;
     GtkWidget *prop_data_interval_val;
     GtkWidget *prop_data_built_val;
     GtkWidget *prop_data_refresh_val;
     GtkWidget *prop_args_inherit;
     GtkWidget *prop_args_table;
     GtkWidget *prop_simple_button;
     GtkWidget *prop_node_frame;
     struct uichoice_node *node;
     int inherit;
     char *row[2];	/* row of 2 cols */
     TREE *nodeargs;
     char *str;		/* general purpose pointer */

     /* create window and hook up the various values */
     prop_window = create_choice_prop_window();
     prop_name_value       = lookup_widget(prop_window, 
					   "choice_prop_name_value");
     prop_info_value       = lookup_widget(prop_window, 
					   "choice_prop_info_value");
     prop_help_value       = lookup_widget(prop_window, 
					   "choice_prop_help_value");
     prop_node_interval_val= lookup_widget(prop_window, 
					   "choice_prop_node_interval_val");
     prop_node_built_val   = lookup_widget(prop_window, 
					   "choice_prop_node_built_val");
     prop_node_refresh_val = lookup_widget(prop_window, 
					   "choice_prop_node_refresh_val");
     prop_data_interval_val= lookup_widget(prop_window, 
					   "choice_prop_data_interval_val");
     prop_data_built_val   = lookup_widget(prop_window, 
					   "choice_prop_data_built_val");
     prop_data_refresh_val = lookup_widget(prop_window, 
					   "choice_prop_data_refresh_val");
     prop_args_inherit     = lookup_widget(prop_window, 
					   "choice_prop_args_inherit_check");
     prop_args_table       = lookup_widget(prop_window, 
					   "choice_prop_args_table");
     prop_simple_button    = lookup_widget(prop_window, 
					   "choice_prop_simple_button");
     prop_node_frame       = lookup_widget(prop_window, 
					   "choice_prop_node_frame");

     /* hide the expert button, as we alwasy start off as simple */
     gtk_widget_hide(prop_simple_button);

     /* find state */
     inherit = GTK_TOGGLE_BUTTON(prop_args_inherit)->active;

     /* find the node structure to which we are referring on tree 
      * (which is global) and has the data attached to it for the 
      * duration of the popup. Save the node to this prop window so 
      * callbacks will be able to access the node when the popup node 
      * is no longer valid */
     node = (struct uichoice_node *) 
	  gtk_object_get_data (GTK_OBJECT (tree), "choice_popup_node");
     gtk_object_set_data (GTK_OBJECT(prop_window), "choice_prop_node", 
			  (gpointer) node);

     if (node == NULL) {
	  elog_printf(ERROR, "NULL choice node");
	  return;
     }

     /* assign values to node and data fields */
     str = uichoice_nodepath(node, "->");
     gtk_label_set_text( GTK_LABEL(prop_name_value), str);
     nfree(str);
     gtk_label_set_text( GTK_LABEL(prop_info_value), node->info);
     gtk_label_set_text( GTK_LABEL(prop_help_value), node->help);
     if (node->is_dynamic) {
	  if (node->dyntimeout) {
	       gtk_label_set_text( GTK_LABEL(prop_node_interval_val), 
				   util_i32toa(node->dyntimeout));
	       if (node->dyntime)
		    gtk_label_set_text( 
			 GTK_LABEL(prop_node_refresh_val), 
			 util_shortadaptdatetime(node->dyntime + 
						 node->dyntimeout) );
	       else
		    gtk_label_set_text( GTK_LABEL(prop_node_refresh_val), 
					"(not applicable)");
	  } else {
	       gtk_label_set_text( GTK_LABEL(prop_node_interval_val), 
				   "(not set)");
	       gtk_label_set_text( GTK_LABEL(prop_node_refresh_val), 
				   "(no refresh)");
	  }
	  if (node->dyntime)
	       gtk_label_set_text( GTK_LABEL(prop_node_built_val), 
				   util_shortadaptdatetime(node->dyntime));
	  else
	       gtk_label_set_text( GTK_LABEL(prop_node_built_val), 
				   "(not yet created)");
     } else {
	  gtk_widget_hide(prop_node_frame);
     }
     if (node->datatimeout) {
	  gtk_label_set_text( GTK_LABEL(prop_data_interval_val), 
			      util_i32toa(node->datatimeout));
	  if (node->datatime)
	       gtk_label_set_text( GTK_LABEL(prop_data_refresh_val), 
				   util_shortadaptdatetime(node->datatime + 
							   node->datatimeout));
	  else
	       gtk_label_set_text( GTK_LABEL(prop_data_refresh_val), 
				   "(not applicable)");
     } else {
	  gtk_label_set_text( GTK_LABEL(prop_data_interval_val), 
			      "(not set)");
	  gtk_label_set_text( GTK_LABEL(prop_data_refresh_val), 
			      "(no refresh)");
     }
     if (node->datatime)
	  gtk_label_set_text( GTK_LABEL(prop_data_built_val), 
			      util_shortadaptdatetime(node->datatime));
     else
	  gtk_label_set_text( GTK_LABEL(prop_data_built_val), 
			      "(not yet built)");

     /* walk the local or inherited arguments to build the arg list, missing 
      * out ones that are known to be pointers (like gtktreeitem) */
     if (inherit) {
	  nodeargs = tree_create();
	  uichoice_getinheritedargs(node, nodeargs);
     } else {
	  nodeargs = node->nodeargs;
     }
     tree_traverse(nodeargs) {
	  row[0] = tree_getkey(nodeargs);
	  row[1] = tree_get(nodeargs);
	  if ( ! row[1] )
	       row[1] = "(null)";
	  else if ( ! *row[1] )
	       row[1] = "(empty)";
	  else if ( ! util_is_str_printable(row[1]))
	       row[1] = util_u32toa( *((int *) tree_get(nodeargs)));
	  gtk_clist_append (GTK_CLIST(prop_args_table), row);
     }
     if (inherit)
	  tree_destroy(nodeargs);

     gtk_widget_show(prop_window);
     gtkaction_anypopup_setwmicon(prop_window);
}


void
on_choice_prop_expert_button_clicked   (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *prop_window;
     GtkWidget *prop_name_value;
     GtkWidget *prop_node_interval_val;
     GtkWidget *prop_node_built_prompt;
     GtkWidget *prop_node_built_val;
     GtkWidget *prop_node_refresh_prompt;
     GtkWidget *prop_node_refresh_val;
     GtkWidget *prop_data_interval_val;
     GtkWidget *prop_data_built_prompt;
     GtkWidget *prop_data_built_val;
     GtkWidget *prop_data_refresh_prompt;
     GtkWidget *prop_data_refresh_val;
     GtkWidget *prop_args_frame;
     GtkWidget *prop_expert_button;
     GtkWidget *prop_simple_button;
     struct uichoice_node *node;

     /* create window and hook up the various values */
     prop_window             =lookup_widget(GTK_WIDGET(button),
					    "choice_prop_window");
     prop_node_built_prompt  =lookup_widget(prop_window, 
					    "choice_prop_node_built_prompt");
     prop_node_built_val     =lookup_widget(prop_window, 
					    "choice_prop_node_built_val");
     prop_node_refresh_prompt=lookup_widget(prop_window, 
					    "choice_prop_node_refresh_prompt");
     prop_node_refresh_val   =lookup_widget(prop_window, 
					    "choice_prop_node_refresh_val");
     prop_data_built_prompt  =lookup_widget(prop_window, 
					    "choice_prop_data_built_prompt");
     prop_data_built_val     =lookup_widget(prop_window, 
					    "choice_prop_data_built_val");
     prop_data_refresh_val   =lookup_widget(prop_window, 
					    "choice_prop_data_refresh_val");
     prop_data_refresh_prompt=lookup_widget(prop_window, 
					    "choice_prop_data_refresh_prompt");
     prop_args_frame         =lookup_widget(prop_window, 
					    "choice_prop_args_frame");
     prop_expert_button      =lookup_widget(prop_window, 
					    "choice_prop_expert_button");
     prop_simple_button      =lookup_widget(prop_window, 
					    "choice_prop_simple_button");

     /* swap buttons around */
     gtk_widget_hide(prop_expert_button);
     gtk_widget_show(prop_simple_button);

     /* find the node structure to which we are referring (tree is global 
      * and has the data attached to it) */
     node = (struct uichoice_node *) gtk_object_get_data (GTK_OBJECT (tree), 
							  "choice_popup_node");
     if (node == NULL) {
	  elog_printf(ERROR, "NULL choice node");
	  return;
     }

     /* make the advanced stuff visible */
     if (node->is_dynamic) {
	  gtk_widget_show (prop_node_built_prompt);
	  gtk_widget_show (prop_node_built_val);
	  gtk_widget_show (prop_node_refresh_prompt);
	  gtk_widget_show (prop_node_refresh_val);
     }
     gtk_widget_show (prop_data_built_prompt);
     gtk_widget_show (prop_data_built_val);
     gtk_widget_show (prop_data_refresh_prompt);
     gtk_widget_show (prop_data_refresh_val);
     gtk_widget_show (prop_args_frame);
}


void
on_choice_prop_simple_button_clicked   (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *prop_window;
     GtkWidget *prop_name_value;
     GtkWidget *prop_node_interval_val;
     GtkWidget *prop_node_built_prompt;
     GtkWidget *prop_node_built_val;
     GtkWidget *prop_node_refresh_prompt;
     GtkWidget *prop_node_refresh_val;
     GtkWidget *prop_data_interval_val;
     GtkWidget *prop_data_built_prompt;
     GtkWidget *prop_data_built_val;
     GtkWidget *prop_data_refresh_prompt;
     GtkWidget *prop_data_refresh_val;
     GtkWidget *prop_args_frame;
     GtkWidget *prop_expert_button;
     GtkWidget *prop_simple_button;

     /* create window and hook up the various values */
     prop_window             =lookup_widget(GTK_WIDGET(button),
					    "choice_prop_window");
     prop_node_built_prompt  =lookup_widget(prop_window, 
					    "choice_prop_node_built_prompt");
     prop_node_built_val     =lookup_widget(prop_window, 
					    "choice_prop_node_built_val");
     prop_node_refresh_prompt=lookup_widget(prop_window, 
					    "choice_prop_node_refresh_prompt");
     prop_node_refresh_val   =lookup_widget(prop_window, 
					    "choice_prop_node_refresh_val");
     prop_data_built_prompt  =lookup_widget(prop_window, 
					    "choice_prop_data_built_prompt");
     prop_data_built_val     =lookup_widget(prop_window, 
					    "choice_prop_data_built_val");
     prop_data_refresh_val   =lookup_widget(prop_window, 
					    "choice_prop_data_refresh_val");
     prop_data_refresh_prompt=lookup_widget(prop_window, 
					    "choice_prop_data_refresh_prompt");
     prop_args_frame         =lookup_widget(prop_window, 
					    "choice_prop_args_frame");
     prop_expert_button      =lookup_widget(prop_window, 
					    "choice_prop_expert_button");
     prop_simple_button      =lookup_widget(prop_window, 
					    "choice_prop_simple_button");

     /* swap buttons around */
     gtk_widget_show(prop_expert_button);
     gtk_widget_hide(prop_simple_button);

     /* make the advanced stuff invisible */
     gtk_widget_hide (prop_node_built_prompt);
     gtk_widget_hide (prop_node_built_val);
     gtk_widget_hide (prop_node_refresh_prompt);
     gtk_widget_hide (prop_node_refresh_val);
     gtk_widget_hide (prop_data_built_prompt);
     gtk_widget_hide (prop_data_built_val);
     gtk_widget_hide (prop_data_refresh_prompt);
     gtk_widget_hide (prop_data_refresh_val);
     gtk_widget_hide (prop_args_frame);
}


void
on_choice_prop_close_button_clicked    (GtkButton       *button,
                                        gpointer         user_data)
{
     GtkWidget *choice_prop_window;

     choice_prop_window = lookup_widget(GTK_WIDGET(button),
					"choice_prop_window");
     gtk_widget_hide (choice_prop_window);
     gtk_widget_destroy (choice_prop_window);
}


void
on_choice_prop_args_inherit_check_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
     GtkWidget *prop_args_inherit;
     GtkWidget *prop_args_table;
     GtkWidget *prop_window;
     struct uichoice_node *node;
     int inherit;
     char *row[2];	/* row of 2 cols */
     TREE *nodeargs;

     /* get widgets */
     prop_window  = lookup_widget(GTK_WIDGET(togglebutton), 
					"choice_prop_window");
     prop_args_inherit  = lookup_widget(GTK_WIDGET(togglebutton), 
					"choice_prop_args_inherit_check");
     prop_args_table = lookup_widget(GTK_WIDGET(togglebutton), 
				     "choice_prop_args_table");
     /* find state & node */
     inherit = GTK_TOGGLE_BUTTON(prop_args_inherit)->active;
     node = (struct uichoice_node *) 
	  gtk_object_get_data (GTK_OBJECT (prop_window), "choice_prop_node");
     if (node == NULL) {
	  elog_printf(ERROR, "NULL choice node");
	  return;
     }

     /* walk the local or inherited arguments to build the arg list, missing 
      * out ones that are known to be pointers (like gtktreeitem) */
     gtk_clist_clear(GTK_CLIST(prop_args_table));
     if (inherit) {
	  nodeargs = tree_create();
	  uichoice_getinheritedargs(node, nodeargs);
     } else {
	  nodeargs = node->nodeargs;
     }
     tree_traverse(nodeargs) {
	  row[0] = tree_getkey(nodeargs);
	  row[1] = tree_get(nodeargs);
	  if ( ! row[1] )
	       row[1] = "(null)";
	  else if ( ! *row[1] )
	       row[1] = "(empty)";
	  else if ( ! util_is_str_printable(row[1]))
	       row[1] = util_u32toa( *((int *) tree_get(nodeargs)));
	  gtk_clist_append (GTK_CLIST(prop_args_table), row);
     }
     if (inherit)
	  tree_destroy(nodeargs);
}


void
on_repos_save_action_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_repos_save_reset_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_repos_cancel_finished_clicked       (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_repos_enable_button_toggled         (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

}

