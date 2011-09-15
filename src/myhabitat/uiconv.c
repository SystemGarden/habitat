/*
 * GHabitat Gtk GUI conversion class
 *
 * Nigel Stuckey, February 2011
 * Copyright System Garden Ltd 2011. All rights reserved
 */

#include <gtk/gtk.h>
#include "main.h"
#include "uiconv.h"



/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_source_location_set (GtkComboBox *object, gpointer user_data)
{
}


/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_source_file_set (GtkFileChooserButton *object, 
				gpointer user_data)
{
}

/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_source_name_set (GtkEntry *object, gpointer user_data)
{
}


/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_source_ring_set (GtkComboBox *object, gpointer user_data)
{
}


/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_source_clear (GtkButton *object, gpointer user_data)
{
}


/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_source_view (GtkButton *object, gpointer user_data)
{
}


/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_dest_location_set (GtkComboBox *object, gpointer user_data)
{
}


/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_dest_file_set (GtkFileChooserButton *object, gpointer user_data)
{
}


/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_dest_clear (GtkButton *object, gpointer user_data)
{
}


/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_copy (GtkButton *object, gpointer user_data)
{
}


/* callback to ??? in copy window */
G_MODULE_EXPORT void 
uiconv_copy_on_help (GtkButton *object, gpointer user_data)
{
}




#if 0

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
MAKES IT COREDUMP AS NOT A LIST AT FIRST. 
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


#endif
