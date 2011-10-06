/*
 * GHabitat Gtk GUI about window
 *
 * Nigel Stuckey, July 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#include <gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>
#include "../iiab/iiab.h"
#include "../iiab/util.h"
#include "../iiab/nmalloc.h"
#include "main.h"
#include "uiabout.h"

/* browser list */
char *uiabout_browsers[] = {"firefox", "mozilla", "konqueror", "netscape", 
			    "opera", "safari", "chimera",  "chimera2",  
			    "lynx",
			    NULL};

/* Initialise about window in the GUI */
void uiabout_init() {
}

void uiabout_fini() {
}


/* callback to to the habitat wiki */
G_MODULE_EXPORT void 
uiabout_on_about (GtkObject *object, gpointer user_data)
{
     GtkAboutDialog *aboutwin;

     aboutwin = GTK_ABOUT_DIALOG(gtk_builder_get_object(gui_builder,
							"about_win"));
     gtk_about_dialog_set_version(aboutwin, VERSION);
     gint result = gtk_dialog_run (GTK_DIALOG(aboutwin));
     switch (result)
       {
       case GTK_RESPONSE_OK:
       case GTK_RESPONSE_CLOSE:
	 break;
       default:
	 break;
       }
     gtk_widget_hide(GTK_WIDGET(aboutwin));
}

/* callback to to the habitat wiki */
G_MODULE_EXPORT void 
on_support_wiki (GtkObject *object, gpointer user_data)
{
     uiabout_browse_web(WEB_WIKI);
}

/* callback to the systemgarden website */
G_MODULE_EXPORT void 
on_website (GtkObject *object, gpointer user_data)
{
     uiabout_browse_web(WEB_HABITAT);
}

/* callback to display the MyHabitat manual */
G_MODULE_EXPORT void 
on_manual (GtkObject *object, gpointer user_data)
{
     uiabout_browse_man(MAN_MYHABITAT);
}



/*
 * Launch a browser using the url given. Returns 1 if successful or 0 if 
 * failed, such as not finding the correct browser.
 */
int uiabout_browse_web(char *url)
{
     char **b, *pathenv, *match, cmd[PATH_MAX+1024];
     int r;

     /* find a valid and executable browser, in a determined order;
      * basically, the best first and the last resort trailing up
      * the rear */
     pathenv = getenv("PATH");
     for (b = uiabout_browsers; *b; b++) {
	  match = util_whichdir(*b, pathenv);
	  if (match) {
	       /* A match, but is it executable ? */
	       r = access(*b, X_OK);
	       if (r != 0) {
		    /* found and execute browser */
		    /*if (strcmp(*b, "netscape") == 0)
			 snprintf(cmd, PATH_MAX+1024, "%s -remote %s", 
				  match, url);
				  else*/
		    elog_printf(INFO, "Starting browser...");
		    snprintf(cmd, PATH_MAX+1024, "%s %s &", match, url);
		    r = system(cmd);
		    nfree(match);
		    if (r == -1) {
		         elog_printf(ERROR, "Unable to run browser (using %s)", 
				     cmd);
			 return 0;	/* fail - browser not worked */
		    } else {
			 return 1;	/* success */
		    }
	       }
	       nfree(match);
	  }
     }

     elog_printf(WARNING, "Unable to find a known browser in the PATH");
     return 0;		/* fail - run out of browsers */
}


/*
 * Search for a help file in standard locations and run a web browser
 * on the discovered location.
 * Returns 1 for success or 0 for failure: either no file or no browser.
 */
int uiabout_browse_help(char *helpfile)
{
     char *url, *file;
     int r;

     /* find the help file in the built location */
     file = util_strjoin(iiab_dir_lib, HELP_BUILT_PATH, helpfile, NULL);
     if (access(file, R_OK) != 0) {
	  /* no files in the built location, try the development place */
	  elog_printf(INFO, "Unable to show help %s", file);
	  nfree(file);
	  file = util_strjoin(iiab_dir_bin, HELP_DEV_PATH, helpfile,
			      NULL);
     }
     if (access(file, R_OK) != 0) {
	  /* no files in the dev location either; abort */
	  elog_printf(ERROR, "Unable to show help %s", file);
	  nfree(file);
	  return 0;	/* failure */
     }

     /* convert file into a url for the browser and display */
     url = util_strjoin("file://localhost", file, NULL);
     nfree(file);
     r = uiabout_browse_web(url);

     /* clear up and return */
     nfree(url);
     return r;
}


/*
 * Search for a man file in standard locations and run a web browser
 * on the discovered location.
 * Returns 1 for success or 0 for failure: either no file or no browser.
 */
int uiabout_browse_man(char *manpage)
{
     char *url, *file;
     int r;

     /* find the help file in the system location (for linux) */
     file = util_strjoin(iiab_dir_lib, MAN_BUILT_PATH, manpage, NULL);
     if (access(file, R_OK) != 0) {
	  /* no files in the built location, try the development place */
	  elog_printf(INFO, "Unable to show manpage in production location "
		      "%s (%s)", manpage, file);
	  nfree(file);
	  file = util_strjoin(iiab_dir_bin, MAN_DEV_PATH, manpage, NULL);
     }
     if (access(file, R_OK) != 0) {
	  /* no files in the dev location either; abort */
	  elog_printf(ERROR, "Unable to show manpage %s in development "
		      "locations (%s)", manpage, file);
	  nfree(file);
	  return 0;	/* failure */
     }

     /* convert file into a url for the browser and display */
     url = util_strjoin("file://", file, NULL);
     nfree(file);
     r = uiabout_browse_web(url);

     /* clear up and return */
     nfree(url);
     return r;
}


