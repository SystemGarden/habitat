/*
 * GHabitat Gtk GUI email class to send data via email
 *
 * Nigel Stuckey, February 2011
 * Copyright System Garden Ltd 2011. All rights reserved
 */

#include <gtk/gtk.h>
#include "main.h"
#include "uiemail.h"
#include "uiabout.h"


/* display edit_win, showing the ringname being exported and preparing
 * a preview of the data that will be sent */
G_MODULE_EXPORT void 
uiemail_init             (GtkButton       *button,
			  gpointer         user_data)
{
     uiabout_browse_help(HELP_DATA_EMAIL);
}


/* callback to email data from the email window */
G_MODULE_EXPORT void 
uiemail_on_send           (GtkButton       *button,
			   gpointer         user_data)
{
     GtkWidget *email_win;
     GtkWidget *email_ring_name;
     GtkWidget *email_format_value;
     GtkWidget *email_option_title_check;
     GtkWidget *email_option_info_check;
     GtkWidget *email_option_time_check;
     GtkWidget *email_option_seq_check;
     GtkWidget *email_to_value;
     GtkWidget *email_cc_value;
     GtkWidget *email_bcc_value;
     GtkWidget *email_subject_value;
     GtkWidget *email_message_text;

     /* get widgets */
     email_win = get_widget("email_win");
     email_ring_name = get_widget("email_ring_name");
     email_format_value = get_widget("email_format_value");
     email_option_title_check = get_widget("email_option_title_check");
     email_option_info_check = get_widget("email_option_info_check");
     email_option_time_check = get_widget("email_option_time_check");
     email_option_seq_check = get_widget("email_option_seq_check");
     email_to_value = get_widget("email_to_value");
     email_cc_value = get_widget("email_cc_value");
     email_bcc_value = get_widget("email_bcc_value");
     email_subject_value = get_widget("email_subject_value");
     email_message_text = get_widget("email_message_text");

#if 0

REACHED HERE

    GtkWidget *mtype = NULL;
    char *to, *cc, *subject, *mtypestr, *mailer, *cmd;
    int r, withtitle, withruler, withseq, withtime, docsv;
    char *buf;
    FILE *fs;

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

#endif
}


G_MODULE_EXPORT void 
uiemail_on_help             (GtkButton       *button,
			     gpointer         user_data)
{
     uiabout_browse_help(HELP_DATA_EMAIL);
}
