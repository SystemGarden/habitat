/*
 * GHabitat collection control and GUI elements for start, stop and status
 *
 * Nigel Stuckey, May 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "../iiab/elog.h"
#include "../iiab/iiab.h"
#include "../iiab/util.h"
#include "../iiab/nmalloc.h"
#include "uicollect.h"
#include "uilog.h"
#include "main.h"

/*
 * Check to see if a clockwork collection daemon is running and if not, 
 * whether the user would like to start it via a GUI. If it is running,
 * then a quiet confirmation is printed to using elog.
 * Config alters the behavior:-
 *   AUTOCLOCKWORK_CFNAME (clockwork.auto)
 *      If set, then start clockwork without asking the user; ignores other
 *      config options
 *   DONTASKCLOCKWORK_CFNAME (clockwork.dontask)
 *      If set, then don't start clockwork and don't ask; post an INFO 
 *      message to elog with details.
 */
void uicollect_askclockwork()
{
     int pid, dontask, autorun;
     char *key;
     GtkWidget *w;

     pid = uicollect_is_clockwork_running(&key, NULL, NULL, NULL, 1);
     if (pid != 0) {
          elog_printf(INFO, "Collecting local data with %s on pid %d", 
		      key, pid);
	  return;
     }

#if __APPLE_CC__
	  elog_printf(FATAL, 
		      "Local collection is not currently supported on Mac. "
		      "We hope to provide it shortly.\n\n"
		      "MyHabitat on Mac is able to browse data on other "
		      "Habitat peer computers or read data files (eg .fha, "
		      ".csv, .grs).\n\n"
		      "See www.systemgarden.com/habitat for more information "
		      "on future releases.");
	  return;	  
#endif

     autorun = cf_getint(iiab_cf, AUTOCLOCKWORK_CFNAME);
     if (autorun != CF_UNDEF && autorun != 0)
          uicollect_startclockwork();
     else {
          dontask = cf_getint(iiab_cf, DONTASKCLOCKWORK_CFNAME);
	  if (dontask != CF_UNDEF && dontask != 0) {
	       /* Don't ask, dont start */
	       elog_printf(INFO, "Local data not being collected "
			   "(not asking & not auto starting). "
			   "Choose 'Edit->Collection' from the menu or "
			   "click 'collect' button to change your mind");
	  } else {
	       /* Ask to start */
	       w = get_widget("start_clockwork_win");
	       gtk_window_present(GTK_WINDOW(w));
	  }
	  return;
     }
}


/* Start clockwork running */
void uicollect_startclockwork()
{
     int r;
     char *cmd;

     if (uicollect_is_clockwork_runable()) {
          /* start clockwork daemon using system () */
          cmd = util_strjoin(iiab_dir_bin, "/clockwork", NULL);
	  elog_printf(INFO, "Starting %s to collect local data", cmd);
	  r = system(cmd);
	  if (r == -1) {
	       elog_printf(FATAL, "Problem starting the collector - please "
			   "check installation is correct. (We tried %s.) ",
			   cmd);
	       nfree(cmd);
	       return;
	  }
	  nfree(cmd);

	  elog_printf(INFO, "Now collecting local data");
	  return;
     } else {
          elog_printf(FATAL, "Can't find collector - please check "
		      "installation is correct");
     }

     return;
}

/* Stop a clockwork process started by this client. */
void uicollect_stopclockwork()
{
     int r;
     char *cmd;

     /* stop clockwork daemon using system () */
     cmd = util_strjoin(iiab_dir_bin, "/killclock >/dev/null", NULL);
     elog_printf(INFO, "Stopping local data collection with %s", cmd);
     r = system(cmd);
     if (r == -1) {
          elog_printf(ERROR, "Unable to stop local data collection "
		      "(attempted %s)", cmd);
	  nfree(cmd);
	  return;
     }
     nfree(cmd);

     return;
}


/* Display the stop clockwork window after populating it with details */
void uicollect_showstopclockwork()
{
     int ipid;
     char *tname, *tuser, *ttty, *tstart, tpid[15];
     GtkWidget *win, *name, *user, *pid, *start;

     /* grab widgets */
     win   = get_widget("stop_clockwork_win");
     name  = get_widget("stopclock_name_entry");
     user  = get_widget("stopclock_user_entry");
     pid   = get_widget("stopclock_pid_entry");
     start = get_widget("stopclock_start_entry");

     /* get details */
     ipid = uicollect_is_clockwork_running(&tname, &tuser, &ttty, &tstart, 0);
     snprintf(tpid, 15, "%d", ipid);

     /* stick them into the fields */
     gtk_entry_set_text(GTK_ENTRY(name), tname);
     gtk_entry_set_text(GTK_ENTRY(user), tuser);
     gtk_entry_set_text(GTK_ENTRY(pid),  tpid);
     gtk_entry_set_text(GTK_ENTRY(start), tstart);

     /* finish */
     gtk_window_present(GTK_WINDOW(win));

     return;
}


/*
 * If clockwork is running on this machine, this routine will return 
 * its PID, which will be above 0.
 * If clockwork is not running, 0 will be returned.
 * Checks to see if the pid is actually running, not just relying
 * on the pid lock. If the giveerror flag is set, then an elog error
 * and a modal alert is raised.
 * Other user interface parts are changed as a result (repository_status_btn 
 * and whatnext).
 * If user, tty or datestr are non-NULL, then they are set as a return
 * (see iiab_getlockpid()).
 */
int uicollect_is_clockwork_running(char **key, char **user, char **tty, 
				   char **datestr, int giveerror)
{
     int pid;

     if (key)
          *key = "clockwork";
     pid = iiab_getlockpid("clockwork", user, tty, datestr);

     /* check that clockwork is actually running */
     if (pid) {
          if ( ! iiab_ispidrunning(pid) ) {
	       if (giveerror) {
		    elog_printf(ERROR, "The collector has crashed and the "
				"debris will be cleaned up by re-runnng it "
				"(was %s on pid %d started by %s at %s.", 
				"clockwork", pid, user?*user:"(unknown)", 
				datestr?*datestr:"(unknown)");
		    uilog_modal_alert("<big><b>Local Collector has "
				      "Crashed</b></big>", 
				      "The local data collector had crashed "
				      "sometime in the past and will need to "
				      "be restarted");
	       }
	       pid = 0;
	  }
     }

     /* change UI based on pid result */
     if (pid)
          uicollect_set_status_collecting();
     else
          uicollect_set_status_not_collecting();

     return pid;
}


/* check if clockwork is availble to run. Returns 1 for yes, 0 for no */
int uicollect_is_clockwork_runable()
{
     int noclockwork;
     char *path;

     /* Look in the primary location: $HAB/bin/clockwork */
     path = util_strjoin(iiab_dir_bin, "/clockwork", NULL);
     elog_printf(INFO, "Looking for %s to collect local data", path);
     noclockwork = access(path, R_OK | X_OK);
     nfree(path);
     if (noclockwork)
	  return 0;
     else
	  return 1;
}



/* Update UI components to show that local collection is not running */
void uicollect_set_status_not_collecting()
{
  GtkWidget *repository_status_btn, *whatnext_label, *whatnext_local_vbox,
    *whatnext_start_btn, *harvest_flower_16_img;

     /* Grab the whatnext widgets */
     repository_status_btn = get_widget("repository_status_btn");
     harvest_flower_16_img = get_widget("harvest_flower_16_img");
     whatnext_label        = get_widget("whatnext_label");
     whatnext_local_vbox   = get_widget("whatnext_local_vbox");
     whatnext_start_btn    = get_widget("whatnext_start_btn");

     /* change repository button and status */
     /*     gtk_button_set_image(repository_status_btn, harvest_flower_16_img);*/

     /* whatnext display change */
     gtk_widget_show(whatnext_local_vbox);
     gtk_label_set_markup(GTK_LABEL(whatnext_label), "<big><b>Not Collecting Local Data</b></big>\n\nStart collection now or connect to remote sources");

}


/* Update UI components to show that local collecting is running */
void uicollect_set_status_collecting()
{
  GtkWidget *repository_status_btn, *whatnext_label, *whatnext_local_vbox,
    *whatnext_start_btn, *harvest_flower_16_img;

     /* Grab the whatnext widgets */
     repository_status_btn = get_widget("repository_status_btn");
     harvest_flower_16_img = get_widget("harvest_flower_16_img");
     whatnext_label        = get_widget("whatnext_label");
     whatnext_local_vbox   = get_widget("whatnext_local_vbox");
     whatnext_start_btn    = get_widget("whatnext_start_btn");

     /* change repository button and status */
     /*     gtk_button_set_image(repository_status_btn, harvest_flower_16_img);*/

     /* whatnext display change */
     gtk_widget_hide(whatnext_local_vbox);
     gtk_label_set_markup(GTK_LABEL(whatnext_label), "<big><b>Add More Data Sources</b></big>\n\nLocal data is being collected as 'This Host:'\nAdd more sources with the buttons below");

}




/* amend the 'what next' display to show the right summary */
void uicollect_update_whatnext()
{
     GtkWidget *whatnext_label, *whatnext_local_vbox;

     /* Grab the whatnext widgets */
     whatnext_label      = get_widget("whatnext_label");
     whatnext_local_vbox = get_widget("whatnext_local_vbox");

     /* Is clockwork running? If not, set the start request message */

     /* Clockwork running; is there any local data to show? if not, find when 
      * it will be ready and set up a countdown */

}
