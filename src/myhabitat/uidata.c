/*
 * Habitat GUI data area
 *
 * Nigel Stuckey, July 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#define _GNU_SOURCE

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "../iiab/elog.h"
#include "../iiab/iiab.h"
#include "../iiab/util.h"
#include "../iiab/nmalloc.h"
#include "../iiab/tree.h"
#include "../iiab/rs.h"
#include "../iiab/rs_gdbm.h"
#include "../iiab/httpd.h"
#include "../iiab/rt_sqlrs.h"
#include "uidata.h"
#include "uitime.h"
#include "uilog.h"
#include "uichoice.h"
#include "uivis.h"
#include "uipref.h"
#include "fileroute.h"
#include "main.h"

/* file scope info on the visutalisation being displayed currently */
GtkTreeSelection *current_selection, *previous_selection;
GtkTreeIter    current_iter, previous_iter;
GtkTreeModel  *current_model;
GtkTreePath   *current_choice_path=NULL, *previous_choice_path=NULL;
gchar         *current_choice_label='\0', 
              *current_choice_purl='\0', 
              *current_ringname='\0';
gpointer       current_choice_getdatacb=NULL;
guint          current_timeout_id=0;
char          *uidata_ringpurl='\0';
TABLE        (*uidata_ringdatacb)(time_t, time_t) = NULL;
TABLE          current_info_tab = NULL;
FILEROUTE_TYPE current_choice_type = FILEROUTE_TYPE_UNKNOWN;
FILEROUTE_TYPE uidata_type = FILEROUTE_TYPE_UNKNOWN;


/* Build the initial choice tree and set up the associated variables */
void uidata_init() {
}

void uidata_fini() {
     uidata_stop_timed_update();

     if (current_info_tab)
          table_destroy(current_info_tab);
     if (uidata_ringpurl)
          nfree(uidata_ringpurl);
}


/* callback to handle a change in the choice tree list */
G_MODULE_EXPORT void 
uidata_on_choice_changed (GtkTreeView *tree, gpointer data)
{
     GtkTreeSelection *selection;

     selection = gtk_tree_view_get_selection(tree);

     uidata_choice_change(selection);
}


/*
 * Change the source being viewed, which changes the visualisation window
 * mode and reads a directory of routes from the new source, unless it is 
 * from a direct function (datacb) or is too simple to support rings.
 * It then initialises viewing buttons from the directory (as applicable),
 * and calls uidata_ring_change() with a default to draw the initial data.
 *
 * There is a chain of events from this point, deciding default data, 
 * setting its label, initialising the timeslider, which in turn loads the 
 * data into the cache and then draws the visualisation.
 *
 * Sets the following lexical globals:-
 *    current_choice_label     - Label of choice
 *    current_choice_purl      - PURL of choice if a ROUTE
 *    current_choice_getdatacb - function pointer if programmatic
 *    current_choice_type      - File type of choice
 *    current_info_tab         - ring directory or NULL if n/a or empty
 */
G_MODULE_EXPORT void uidata_choice_change (GtkTreeSelection *selection)
{
     guint vis;
     char infopurl[256];
     TREE *attribs = NULL;
 
     /* age the current selection, current->previous */
     previous_selection = current_selection;
     previous_iter      = current_iter;
     if (previous_choice_path)
          gtk_tree_path_free(previous_choice_path);
     previous_choice_path = current_choice_path;

     /* get selection and retrieve the choice entry */
     current_selection = selection;
     if (gtk_tree_selection_get_selected (current_selection, &current_model, 
					  &current_iter)) {
          /* check for double click using tree paths and do nothing if so */
          current_choice_path = gtk_tree_model_get_path(current_model, 
							&current_iter);
	  if (current_choice_path && previous_choice_path &&
	      gtk_tree_path_compare(current_choice_path, 
				    previous_choice_path) == 0) {
	       /* label has been clicked twice: exit without retrieving 
		  additional data */
	       /*g_print("repeat selection on choice tree\n");*/
	       return;
	  }

	  /* -- we now load a new choice -- */

	  /* stop data updates right now to make the following safe and
	   * avoid unnecessary work */
	  uidata_stop_timed_update();

	  /* free previous data as we fetch new data */
	  if (current_choice_label) {
	       g_free (current_choice_label);
	       current_choice_label = '\0';
	  }
	  if (current_choice_purl) {
	       g_free (current_choice_purl);
	       current_choice_purl = '\0';
	  }

	  /* extract data from the tree model */
          gtk_tree_model_get (current_model, &current_iter, 
			      UICHOICE_COL_NAME,      &current_choice_label,
			      UICHOICE_COL_PURL,      &current_choice_purl,
			      UICHOICE_COL_GETDATACB, &current_choice_getdatacb,
			      UICHOICE_COL_VISUALISE, &vis, 
			      UICHOICE_COL_TYPE,      &current_choice_type, 
			      -1);
     } else {
          /* unslected: do nothing */
          return;
     }

     uilog_setprogress("Loading data summary", 0.2, 0);

     /* change the visualisation mode */
     uivis_change_view(vis);

     /* If a PURL is defined, then use that route to obtain the ring TABLE */
     if (current_choice_purl) {
          /*g_print ("You selected the label %s vis %d purl %s\n", 
	    current_choice_label, vis, current_choice_purl);*/

          if (current_info_tab)
	       table_destroy(current_info_tab);

	  /* Check what the route can support and set visualisation
	   * buttons accordingly. If rings are supported, get the ring table,
	   * and set the common ring buttons */
	  if (current_choice_type == FILEROUTE_TYPE_TEXT ||
	      current_choice_type == FILEROUTE_TYPE_UNKNOWN) {

	       /* plain text */
	       current_info_tab = NULL;
#if 0
	       uidata_illuminate_ring_btns(NULL);
	       uidata_illuminate_vis_btns(UIVIS_TEXT);
#endif

	  } else if (current_choice_type == FILEROUTE_TYPE_TSV || 
		     current_choice_type == FILEROUTE_TYPE_CSV || 
		     current_choice_type == FILEROUTE_TYPE_PSV || 
		     current_choice_type == FILEROUTE_TYPE_SSV) {

	       /* various type of simple CSV and table */
	       current_info_tab = NULL;
#if 0
	       uidata_illuminate_ring_btns(NULL);
	       uidata_illuminate_vis_btns(UIVIS_TABLE);
#endif
	  } else {

	       /* everything else is assumed to be more complex and support
		* multiple rings -- ringstores.
		* Get ring information & stats from the route */
	       snprintf(infopurl, 256, "%s?clinfo", current_choice_purl);
	       current_info_tab = route_tread(infopurl, NULL);
	       if ( ! current_info_tab ) {
		    /* no data, so disable the attempted view and replace with
		     * a splash if non-local or flag it if non-local. 
		     * Perhaps something more intellegent later */
		    if (strcmp("local:", current_choice_purl) == 0) {
			/* Can read from local daemeon: probably not running */
			/* Do nothing for now, but maybe illuminate an 
			 * indicator later on TODO */
		        elog_printf(INFO, "Local data unavailable (%s)",
		            	    infopurl);
		    } else {
		        elog_printf(DIAG, "Unable to read %s as table", infopurl);
			uilog_modal_alert("Unable to Load Host", 
				          "The habitat file, peer or repository "
					  "either does not exist or is not "
					  "runing to provide us with data (%s)", 
					  current_choice_purl);
		    }
		    uidata_illuminate_ring_btns(NULL);
		    uidata_illuminate_vis_btns(UIVIS_NONE);
		    uivis_change_view(UIVIS_SPLASH);
		    goto uidata_on_choice_changed_clearup;
	       }

#if 0
	       char *ringdebug;
	       ringdebug = table_outtable(current_info_tab);
	       g_print ("---- Read table:-\n%s\n----\n", ringdebug);
	       nfree(ringdebug);
#endif

	       /* Extract the unique values from the name column as a TREE */
	       attribs = table_uniqcolvals(current_info_tab, "name", NULL);
	       if ( ! attribs || tree_n(attribs) <= 0) {
		    /* no data, so disable the attempted view and replace with
		     * a splash for now. Perhaps something more intellegent 
		     * later */
		    elog_printf(DIAG, "There are no rings in %s", infopurl);
		    uilog_modal_alert("No Data Stored", 
				 "The habitat file, peer or repository appears "
				 "to be empty. Make sure that data is being "
				 "collected and return once it has stored (%s)",
				 current_choice_purl);
		    uidata_illuminate_ring_btns(NULL);
		    uidata_illuminate_vis_btns(UIVIS_NONE);
		    uivis_change_view(UIVIS_SPLASH);
		    goto uidata_on_choice_changed_clearup;
	       }

	       /* more waiting */
	       uilog_setprogress("Loading ring data", 0.4, 0);

	       /* update buttons; no redraw as no data */
	       uidata_illuminate_ring_btns(attribs);
	       uidata_illuminate_vis_btns(vis);

	  }

	  /* carry on the rest of of the ring initialisation */
	  uidata_ring_change(NULL);

	  if (attribs)
	       tree_destroy(attribs);

     } else if (current_choice_getdatacb) {

          /* No PURL, use the GETDATACB function to get the TABLE */

          /* Debug */
          /*g_print ("You selected the label %s vis %d dynamic\n", 
	    current_choice_label, vis);*/

          /* No ring information currently implemented for GETDATACB */
	  if (current_info_tab)
	       table_destroy(current_info_tab);
	  current_info_tab = NULL;

	  /* more waiting */
	  uilog_setprogress("Loading default data", 0.4, 0);

	  /* update buttons; no redraw as no data */
	  uidata_illuminate_ring_btns(NULL);
	  uidata_illuminate_vis_btns(vis);

	  /* carry on the rest of of the ring initialisation */
          uidata_ring_change(NULL);

     } else {
          /* Debug */
          /*g_print ("You selected the label %s vis %d but no purl\n", 
	    current_choice_label, vis);*/
     }

     /* Clear up */
 uidata_on_choice_changed_clearup:
     uilog_clearprogress();
}


extern GtkTreeIter localparent;

/* Call back from an alarm event to update the visualisation pane to 
 * the local view. Do not change if there is a selection in place */
gint uidata_choice_change_to_local()
{
     GtkTreeStore *choicestore;
     GtkTreeView *choicetree;
     GtkTreeSelection *selection;
     GtkTreePath *path;
     GtkTreeIter iter;
     GtkTreeModel *model;

     /* get the selection object pointer */
     choicetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder,
						       "choice_tree"));
     selection = gtk_tree_view_get_selection(choicetree);
     if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
          /* there is a selection already, so don't attempt to 
	   * select the local */
          return;
     }

     /* get the path from tree and store */
     choicestore = GTK_TREE_STORE(gtk_builder_get_object(gui_builder,
							 "choice_treestore"));
     path = gtk_tree_model_get_path(GTK_TREE_MODEL(choicestore), 
				    &localparent);

     /* set the choice selection to the local view */
     gtk_tree_selection_select_path(selection, path);
     gtk_tree_path_free(path);

     /* fire the visualisation manually, I dont know which signal should 
      * be emitted */
     uidata_choice_change(selection);

     return FALSE;			/* cancel further updates */
}


/* callback to handle a change in the ring button list */
const gchar *uidata_ring_current_label="\0";
G_MODULE_EXPORT void 
uidata_on_ring_changed (GtkToolButton *toolbutton, gpointer data)
{
     const gchar *label;

     if ( ! gtk_toggle_tool_button_get_active(
				GTK_TOGGLE_TOOL_BUTTON(toolbutton)) )
          return;	/* button is up, do nothing */

     label = gtk_tool_button_get_label(GTK_TOOL_BUTTON(toolbutton));

     /*g_print("uidata_on_ring_changed() - button pressed is %s; next is "
       "forget data and call uidata_ring_change()\n", label);*/

     /* change the ring and display it */
     uitime_forget_data();
     uidata_ring_change((char *)label);
}


/* callback to handle the 'other' ring button, which triggers a menu */
G_MODULE_EXPORT void 
uidata_on_other_ring_pressed (GtkToolButton *toolbutton, gpointer data)
{
     GtkWidget *menu;

     if ( ! gtk_toggle_tool_button_get_active(
				GTK_TOGGLE_TOOL_BUTTON(toolbutton)) )
          return;	/* button is up, do nothing */


     /* get widgets */
     menu = get_widget("otherrings_menu");

     /* post the popup */
     gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0,
                    gtk_get_current_event_time());
}


/* callback to handle the menu item 'other' ring menu */
G_MODULE_EXPORT void 
uidata_on_other_ring_item_activated (GtkImageMenuItem *menubutton,
				     gpointer data)
{
     const gchar *label;

     label = gtk_menu_item_get_label(GTK_MENU_ITEM(menubutton));
     /*fprintf(stderr, "uidata_on_other_ring_item_activated() callback to %s\n", label);*/

     /* change the ring and display it */
     uitime_forget_data();
     uidata_ring_change((char *)label);
}


/* Change or update the ring data displayed in the visualisation.
 * If ringlabel is NULL, then use a default if data is a ROUTE.
 * Handles the button label to standard ringname translation ('CPU' as the 
 * human readble button label maps to 'sys' ring name, etc),
 * extracts out the time boundaries set by the GUI and initialises the time 
 * slider (which in turn (by Gtk events) loads data from the source and 
 * into the cache then draws the visulaisation).
 *
 * Depends on the following globals:-
 *    current_choice_purl  - The PURL of the ROUTE
 *    current_info_tab     - The directory of rings
 *    current_choice_type  - The choice's file type 
 * Sets the following globals:-
 *    uidata_ring_current_label  - ring label or "" as appropriate
 *    uidata_ringpurl            - PURL of data
 *    uidata_ringdatacb          - function pointer of data
 *    uidata_type                - data type of route
 */
void uidata_ring_change (char *ringlabel)
{
     char *vis_title, *from_txt, *to_txt;
     GtkWidget *w_vis_label;
     char ringpurl[256];
     time_t from_t, to_t;

     if (ringlabel) {
          /* We are given a ring name, but return if have it already */
          if (*uidata_ring_current_label &&
	      strcmp(uidata_ring_current_label, ringlabel) == 0)
	       return;
	  uidata_ring_current_label = ringlabel;
	  /*g_print("uidata_ring_change() - given ring %s\n", 
	    uidata_ring_current_label);*/
     } else {
          /* We are passed a NULL label (for default).
	   * If it is a CSV, a text file, or a function, then the ring 
	   * label will be "",
	   * Otherwise we use the currently selected button or a default
	   * if all else fails. */

          if (current_choice_type == FILEROUTE_TYPE_TSV  || 
	      current_choice_type == FILEROUTE_TYPE_CSV  || 
	      current_choice_type == FILEROUTE_TYPE_PSV  || 
	      current_choice_type == FILEROUTE_TYPE_SSV  ||
	      current_choice_type == FILEROUTE_TYPE_TEXT ||
	      uidata_ringdatacb != NULL ) {

		    /* data not from a ROUTE */
		    uidata_ring_current_label = "";
	  } else {

	       /* Button Grab for previous selection
		* METHOD: get the first item, treat as button, find 
		* its group, walk group list till end or we find the
		* active one. */
	       GtkWidget *w_ring_btn=NULL;

	       /* TO BE FIXED */
	       if (w_ring_btn) {
		    /* one of the buttons is pressed, use this to select 
		     * the ring */
#if 0
		    uidata_ring_current_label = gtk_button_get_label
		      (GTK_BUTTON(w_ring_btn) );
#else
		    uidata_ring_current_label = gtk_tool_button_get_label
		      (GTK_TOOL_BUTTON(w_ring_btn) );
#endif
	       } else {
		    /* can't find any pressed buttons, so we need a default */
		    if (current_choice_purl && *current_choice_purl) {
		         /* data from a ROUTE */
		         uidata_ring_current_label = "CPU";
			 /*g_print("uidata_ring_change() - given NULL, no "
				 "pressed buttons, defaulting to %s\n", 
				 uidata_ring_current_label);*/
		    } else {
		         /* At this point we have no idea!! */
		         uidata_ring_current_label = "";
		    }
	       }
	  }
     }

     /* derive the ring name from the click */
     if (strcmp(uidata_ring_current_label, "CPU") == 0) {
          current_ringname = "sys";
     } else if (strcmp(uidata_ring_current_label, "Storage") == 0) {
          current_ringname = "io";
     } else if (strcmp(uidata_ring_current_label, "Network") == 0) {
          current_ringname = "net";
     } else if (strcmp(uidata_ring_current_label, "Processes") == 0) {
          current_ringname = "ps";
     } else if (strcmp(uidata_ring_current_label, "Uptime") == 0) {
          current_ringname = "up";
     } else if (strcmp(uidata_ring_current_label, "Events") == 0) {
          current_ringname = "events";
     } else {
          /* not a standard label (ie Storage, CPU, etc) so we have to use 
	   * the label string as it as the ring name */
       if (uidata_ring_current_label && *uidata_ring_current_label)
	    current_ringname = (char *) uidata_ring_current_label;
       else
	    current_ringname = NULL;
     }

     /*fprintf(stderr, "uidata_ring_change() - current_label=%s, "
	     "current_ringname=%s\n",
	     uidata_ring_current_label ? uidata_ring_current_label : "(null)",
	     current_ringname);*/

     /* Only operate when there is an info table and a ringname */
     if (current_info_tab && current_ringname) {
          /* extract the times and information from the table & then update 
	   * the time slider with the default view */
          if (table_search(current_info_tab, "name", current_ringname) == -1) {
	       if (ringlabel) {
		    elog_printf(FATAL, "Please choose another ring as there "
				"was no data for %s (%s in stat table)", 
				current_ringname, uidata_ring_current_label);
		    goto  uidata_on_ring_changed_clearup;
	       } else {
		    /* Non standard ring as the default (sys) was not found.
		     * This is not an error, its just that we dont know what to 
		     * pick. The next best thing is to use a random ring from
		     * the source, such as the first row of the stat table */
		    table_first(current_info_tab);
		    current_ringname = table_getcurrentcell(current_info_tab, 
							    "long");
		    uidata_ring_current_label=table_getcurrentcell(
						    current_info_tab, "name");
	       }
	  }
	  from_txt = table_getcurrentcell(current_info_tab, "otime");
	  to_txt   = table_getcurrentcell(current_info_tab, "ytime");
	  if (!from_txt || !to_txt) {
	       elog_printf(ERROR, "Unable to find times for %s in stat table", 
			   uidata_ring_current_label);
	       goto  uidata_on_ring_changed_clearup;
	  }
	  from_t = atoi(from_txt);
	  to_t = atoi(to_txt);
     } else {
          /* If there is no table, then assume it is from a function, 
	   * a text file or a file based CSV of some type and set 
	   * dummy dates */
          to_t = time(NULL);
          from_t = to_t - 86400;
     }

     /* Compile the base ring url of the form
      *    [driver] : [host or file] , [ring]
      * and store in a global for later use by the various callbacks.
      * Each can add the additional p-url parts they need for their
      * specific function */
     if (current_choice_purl) {

	  /* compile route */
          if (current_choice_type == FILEROUTE_TYPE_TSV  || 
	      current_choice_type == FILEROUTE_TYPE_CSV  || 
	      current_choice_type == FILEROUTE_TYPE_PSV  || 
	      current_choice_type == FILEROUTE_TYPE_SSV  ||
	      current_choice_type == FILEROUTE_TYPE_TEXT ||
	      current_choice_type == FILEROUTE_TYPE_UNKNOWN ||
	      uidata_ringdatacb != NULL ) {

	       /* single level: file name but no ring */
	       strncpy(ringpurl, current_choice_purl, 256);
	  } else {
	       /* suports two level rings: host + "," + ring */
	       snprintf(ringpurl, 256, "%s,%s", current_choice_purl, 
			current_ringname);

	  }

          /*g_print ("You selected ring label=%s purl=%s ringname=%s "
		   "ringpurl=%s\n", 
		   uidata_ring_current_label, current_choice_purl, 
		   current_ringname, ringpurl);*/

	  /* save globally */
          if (uidata_ringpurl)
	       nfree(uidata_ringpurl);
	  uidata_ringpurl = xnstrdup(ringpurl);
	  uidata_ringdatacb = NULL;

     } else if (current_choice_getdatacb) {
          /* if we are going dynamic, clear current purl & set the 
	   * dyndata function */
	  if (uidata_ringpurl)
	       nfree(uidata_ringpurl);
	  uidata_ringpurl = '\0';
	  uidata_ringdatacb = current_choice_getdatacb;

          /*g_print ("You selected a direct function %p\n", 
	    uidata_ringdatacb);*/
     }

     /* common global */
     uidata_type = current_choice_type;

     /* set time (where needed) and draw */
#if 0
     if (current_choice_type == FILEROUTE_TYPE_TEXT ||
	 current_choice_type == FILEROUTE_TYPE_UNKNOWN) {
          /* no time, draw directly */
fprintf(stderr, "******type text\n");
          uivis_draw(uidata_ringpurl, uidata_ringdatacb, from_t, to_t);
     } else {
#endif
          /* set slider which causes the redraw */
          uitime_forget_data();
	  uitime_allow_slider_reload();
	  uitime_set_slider(from_t, to_t, -1);
#if 0
    }
#endif

 uidata_on_ring_changed_clearup:

     /* Assign the visualisation label (title) with the ring */
     if (uidata_ring_current_label && *uidata_ring_current_label)
          vis_title = util_strjoin("<b>", current_choice_label, " - ", 
				   uidata_ring_current_label, "</b>", NULL);
     else
          vis_title = util_strjoin("<b>", current_choice_label, "</b>", NULL);

     w_vis_label = get_widget("vis_label");
     gtk_label_set_markup(GTK_LABEL(w_vis_label), vis_title);
     nfree(vis_title);

     /* Clear up */
     uilog_clearprogress();

     /* Set up repeats */
     uidata_set_timed_update();
}


/* Populate the information screen with the current choice */
void uidata_populate_info() 
{
     gchar *label, *purl, *help;
     GtkImage *infoimage;
     GtkLabel *infolabel, *infotext;
     GdkPixbuf *bigimage, *badge;

     /* get selection details */
     /*difference between a tree_model and a tree_store?*/
     gtk_tree_model_get (current_model, &current_iter, 
			 UICHOICE_COL_NAME,     &label,
			 UICHOICE_COL_HELP,     &help,
			 UICHOICE_COL_PURL,     &purl,
			 UICHOICE_COL_BIGIMAGE, &bigimage,
			 UICHOICE_COL_BADGE,    &badge,
			 -1);

     /* get the widget references */
     infoimage = GTK_IMAGE (get_widget("information_image"));
     infolabel = GTK_LABEL (get_widget("information_label"));
     infotext  = GTK_LABEL (get_widget("information_text"));

     /* assign items */
     gtk_image_set_from_pixbuf(infoimage, bigimage);
     gtk_label_set_text(infolabel, label);
     gtk_label_set_text(infotext, help);

     /* free up */
     g_free(label);
     g_free(purl);
     g_free(help);
}



/* Illuminate & set up the buttons for the appropriate rings. Pass the rings
 * in a TREE data type, which will get modified (entries deleted).
 * Pass an empty TREE list or NULL to deilluminate all buttons */
void uidata_illuminate_ring_btns(TREE *rings) 
{
     GtkWidget *sys_btn, *io_btn, *net_btn, *ps_btn, *event_btn, 
       *up_btn, *other_btn;
     GtkWidget *menu, *menuitem, *image;
     char *imagepath;

     /* Grab widgets */
     sys_btn   = get_widget("ringview_perf_btn");
     io_btn    = get_widget("ringview_io_btn");
     net_btn   = get_widget("ringview_net_btn");
     ps_btn    = get_widget("ringview_ps_btn");
     event_btn = get_widget("ringview_events_btn");
     up_btn    = get_widget("ringview_uptime_btn");
     other_btn = get_widget("ringview_other_btn");
     menu      = get_widget("otherrings_menu");

     /* Use the values to illuminate the buttons */
     if (rings && tree_find(rings, "sys") != TREE_NOVAL) {
          /* sys ring containing CPU */
          tree_rm(rings);
	  gtk_widget_set_sensitive(sys_btn, 1);
     } else {
	  gtk_widget_set_sensitive(sys_btn, 0);
     }

     if (rings && tree_find(rings, "io") != TREE_NOVAL) {
          tree_rm(rings);
	  gtk_widget_set_sensitive(io_btn, 1);
     } else {
	  gtk_widget_set_sensitive(io_btn, 0);
     }
     
     if (rings && tree_find(rings, "net") != TREE_NOVAL) {
          tree_rm(rings);
	  gtk_widget_set_sensitive(net_btn, 1);
     } else {
	  gtk_widget_set_sensitive(net_btn, 0);
     }

     if (rings && tree_find(rings, "up") != TREE_NOVAL) {
          tree_rm(rings);
	  gtk_widget_set_sensitive(up_btn, 1);
     } else {
	  gtk_widget_set_sensitive(up_btn, 0);
     }

     if (rings && tree_find(rings, "ps") != TREE_NOVAL) {
          tree_rm(rings);
	  gtk_widget_set_sensitive(ps_btn, 1);
     } else {
	  gtk_widget_set_sensitive(ps_btn, 0);
     }

     if (rings && tree_find(rings, "event") != TREE_NOVAL) {
          tree_rm(rings);
	  gtk_widget_set_sensitive(event_btn, 1);
     } else {
	  gtk_widget_set_sensitive(event_btn, 0);
     }

     /* other rings, not covered by the standard buttons */
     if (rings && tree_n(rings) > 0) {
	  gtk_widget_set_sensitive(other_btn, 1);

	  /* clear previous menu items */
	  gtk_container_foreach(GTK_CONTAINER(menu), 
				uidata_remove_child_widget, NULL);

	  while (tree_n(rings) > 0) {
	       tree_first(rings);

	       /* create menu item */
	       menuitem = gtk_image_menu_item_new_with_label
		 (tree_getkey(rings));
	       gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	       /* place icon on menuitem */
	       imagepath = util_strjoin(iiab_dir_lib, "/", 
					UICHOICE_ICON_RINGSTORE, NULL);
	       image = gtk_image_new_from_file(imagepath);
	       nfree(imagepath);
	       gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
					     image);
	       /*gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(menuitem), 1);*//* force image to show on menu */

	       /* description */
	       /*gtk_widget_set_tooltip_text(menuitem, "Test Test Test");*/

	       /* this is causing a problem to do with the "clicked" signal.
		* signal_id > 0 from g_signal_connect_closure_by_id().
		* disabled for now
 	       gtk_signal_connect(GTK_OBJECT (menuitem), "clicked", 
				  GTK_SIGNAL_FUNC (uidata_on_other_ring_item_activated),
				  NULL);
	       */
	       gtk_signal_connect(GTK_OBJECT (menuitem), "activate", 
				  GTK_SIGNAL_FUNC (uidata_on_other_ring_item_activated),
				  NULL);
	       tree_rm(rings);
	  }
	  gtk_widget_show_all(menu);
     } else {
	  gtk_widget_set_sensitive(other_btn, 0);
     }

}


/* A callback to gtk_container_foreach to remove all children */
void uidata_remove_child_widget(GtkWidget* widget, gpointer data) {
     GtkWidget* parent = gtk_widget_get_parent(widget);
     gtk_container_remove(GTK_CONTAINER(parent), widget);
}



/* Illuminate & set up the visualisation buttons. 
 * UIVIS_CHART allows chart, table and text.
 * UIVIS_TABLE allows table and text
 * UIVIS_TEXT allows text only
 * UIVIS_NONE or NULL allows nothing
 */
void uidata_illuminate_vis_btns(enum uivis_t vistype)
{
     GtkWidget *text_btn, *table_btn, *chart_btn;

     /* Grab widgets */
     text_btn  = get_widget("ringview_text_btn");
     table_btn = get_widget("ringview_table_btn");
     chart_btn = get_widget("ringview_chart_btn");

     /* Do not generate signal (and this draw) from an illumination routine */
     g_signal_handlers_block_by_func(G_OBJECT(text_btn), 
				     G_CALLBACK(uivis_on_vis_changed), NULL);
     g_signal_handlers_block_by_func(G_OBJECT(table_btn), 
				     G_CALLBACK(uivis_on_vis_changed), NULL);
     g_signal_handlers_block_by_func(G_OBJECT(chart_btn), 
				     G_CALLBACK(uivis_on_vis_changed), NULL);

     gtk_widget_set_sensitive(text_btn, 1);
     if (!vistype) {
	  gtk_widget_set_sensitive(chart_btn, 0);
	  gtk_widget_set_sensitive(table_btn, 0);
	  gtk_widget_set_sensitive(text_btn, 0);
     } else if (vistype == UIVIS_CHART) {
	  gtk_widget_set_sensitive(chart_btn, 1);
	  gtk_widget_set_sensitive(table_btn, 1);
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(chart_btn),
					    1);
     } else if (vistype == UIVIS_TABLE) {
	  gtk_widget_set_sensitive(chart_btn, 0);
	  gtk_widget_set_sensitive(table_btn, 1);
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(table_btn),
					    1);
     } else {
	  gtk_widget_set_sensitive(chart_btn, 0);
	  gtk_widget_set_sensitive(table_btn, 0);
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(text_btn),
					    1);
     }

     /* and release */
     g_signal_handlers_unblock_by_func(G_OBJECT(text_btn), 
				       G_CALLBACK(uivis_on_vis_changed), NULL);
     g_signal_handlers_unblock_by_func(G_OBJECT(table_btn), 
				       G_CALLBACK(uivis_on_vis_changed), NULL);
     g_signal_handlers_unblock_by_func(G_OBJECT(chart_btn), 
				       G_CALLBACK(uivis_on_vis_changed), NULL);
}


/* Illuminate the time slider and associated labels */
void uidata_illuminate_time() {
     GtkWidget *ts_slide, *ts_min, *ts_max, *ts_current;

     ts_slide   = get_widget("view_timescale_slide");
     ts_min     = get_widget("view_timescale_min");
     ts_max     = get_widget("view_timescale_max");
     ts_current = get_widget("view_timescale_current");

     gtk_widget_set_sensitive(ts_slide,   1);
     gtk_widget_set_sensitive(ts_min,     1);
     gtk_widget_set_sensitive(ts_max,     1);
     gtk_widget_set_sensitive(ts_current, 1);
}


/* Remove the illumination of the time slider and associated labels */
void uidata_deilluminate_time() {
     GtkWidget *ts_slide, *ts_min, *ts_max, *ts_current;

     ts_slide   = get_widget("view_timescale_slide");
     ts_min     = get_widget("view_timescale_min");
     ts_max     = get_widget("view_timescale_max");
     ts_current = get_widget("view_timescale_current");

     gtk_widget_set_sensitive(ts_slide,   0);
     gtk_widget_set_sensitive(ts_min,     0);
     gtk_widget_set_sensitive(ts_max,     0);
     gtk_widget_set_sensitive(ts_current, 0);
}


/* Set the next timed update of the current ring, removing any existing
 * one that be in effect. If no ring is current, the net effect is to 
 * unset the timer. if thereis no 'dur' in the column, a default of
 * 30 seconds is chosen */
void uidata_set_timed_update()
{
     char *dur_txt;
     int dur, defdur, cfdur;

     uidata_stop_timed_update();

     /* find default: confgured first, then emergency default */
     cfdur = cf_getint(iiab_cf, UIPREF_CFKEY_UPDATE);
     if (cfdur == CF_UNDEF || cfdur < 1)
          defdur = UIDATA_DEFAULT_UPDATE_TIME;
     else
          defdur = cfdur;

     /* extract the interval or default to one and set up the glib timeout  */
     if (current_info_tab && current_ringname) {
          if (table_search(current_info_tab, "name", current_ringname) != -1) {
	       dur_txt = table_getcurrentcell(current_info_tab, "dur");
	       if (dur_txt)
		    dur = atoi(dur_txt);
	       else
		    dur = defdur;

	       /* If duration is irregular, use a default update */
	       if (dur == 0)
		    dur = defdur;
	  } else {
	       /* no duration column, use a default refresh */
	       dur = defdur;
	  }
	  current_timeout_id = g_timeout_add_seconds(dur, 
						     uidata_on_timed_update, 
						     NULL);
     }
}


/* Remove any existing update */
void uidata_stop_timed_update() {
     /* Remove any existing timer */
     if (current_timeout_id) {
          g_source_remove(current_timeout_id);
	  current_timeout_id = 0;
     }
}


/* Call back from an alarm event to update data in the visualisation pane */
gint uidata_on_timed_update()
{
     if (current_choice_purl && current_info_tab && current_ringname) {
          uidata_data_update();
	  return TRUE; 			/* carry on till the next interval */
     } else {
          current_timeout_id = 0;	/* clear the global reference */
          return FALSE;			/* cancel further updates */
     }
}


/* callback to update data in visualisation pane */
G_MODULE_EXPORT void 
uidata_on_data_update (GtkObject *object, gpointer user_data)
{
     uidata_data_update();
}


/* update the data view based on the current ring, but downloading with 
 * new data and updating the visualisation as necessary */
void uidata_data_update()
{
     TABLE infotab;
     char infopurl[256];
     char *from_txt, *to_txt;
     time_t from_t, to_t;

     /* Get new info table */
     if (current_choice_purl) {
          /* Update from a ROUTE with a purl address */
          uilog_setprogress("Updating data summary", 0.2, 0);

          /* poll the route source for the latest info table, store it and 
	   * then update the current ring */
	  snprintf(infopurl, 256, "%s?clinfo", current_choice_purl);
	  infotab = route_tread(infopurl, NULL);
	  if ( ! infotab ) {
	       elog_printf(INFO, "Can't reach data source to update (%s)",
			   infopurl);
	       uilog_clearprogress();
	       return;	/* leave with the existing display intact */
	  }
 
	  /* overwrite old info table with new */
	  if (current_info_tab)
	       table_destroy(current_info_tab);
	  current_info_tab = infotab;

	  /* check ring still exists and get the latest time */
          if (table_search(current_info_tab, "name", current_ringname) == -1) {
	       /* ring is not there anymore. 
		* Log this but leave the display as is */
	       elog_printf(ERROR, "Displayed data %s does not exist "
			   "any more: unable to update data (ring %s, "
			   "purl %s)", uidata_ring_current_label,
			   current_ringname, current_choice_purl);
	       uilog_clearprogress();
	       return;	/* leave with the existing display intact */
	  }

	  /* extract current times */
	  from_txt = table_getcurrentcell(current_info_tab, "otime");
	  to_txt   = table_getcurrentcell(current_info_tab, "ytime");
	  if (!from_txt || !to_txt) {
	       elog_printf(ERROR, "Unable to find times for %s in stat table", 
			   uidata_ring_current_label);
	       uilog_clearprogress();
	       return;	/* leave with the existing display intact */
	  }
	  from_t = atoi(from_txt);
	  to_t = atoi(to_txt);

     } else if (current_choice_getdatacb) {
          /* No PURL, use the GETDATACB function to get the whole TABLE */
	  to_t = time(NULL);
          from_t = to_t - 86400;
     } else {
          elog_printf(ERROR, "Data source not set, neither purl nor "
		      "function based");
          uilog_clearprogress();
	  return;	/* leave with the existing display intact */
     }

     /* more waiting */
     uilog_setprogress("Loading latest data", 0.4, 0);

     /* update the slider with the latest details, which will cause 
      * everything to get redrawn. Owing to the cache, only the new 
      * data will actually get downloaded.
      * The ring purl does not need to be changed */
     uitime_set_slider(from_t, to_t, -1);

     uilog_clearprogress();
}
