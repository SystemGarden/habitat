/*
 * Habitat GUI choice tree
 * Presents a set of choices in the form of a tree, which when selected
 * cause its visualisation to change
 *
 * How it works:
 * Choice groups (the top levels) are hardcoded into 
 *    HABITAT, FILES, HOSTS and REPOSITORY
 * Nodes are categorised into each and will be inserted as relevant.
 *
 * Each node can represent a number of concepts as choice. Clicking on
 * them will cause an action defined in that node, these are:-
 * 1. ROUTE, in which case it will have a p-url address (UICHOICE_COL_PURL)
 *    and a maximum visualisation (UICHOICE_COL_VISUALISE). It will be
 *    assumed that the p-url will respond to standard route queries 
 *    for ring tables, etc (?info, ?cinfo, ?linfo, ?lcinfo) and that data
 *    is time series responing to time queries (?t=)
 * 2. Internal function, in which case an address of a function will be defined
 *    (UICHOICE_COL_GETDATACB) that returns a table to display data
 * 3. Dynamic choice, where the choice itself and its children can be updated
 *    with new data. The choice is a function address (UICHOICE_COL_GETDYNCB)
 *    that directly manipulates the choice tree model. The node can be dynamic
 *    (UICHOICE_COL_ISDYNAMIC), in which case the function to update the
 *    choice model will be called periodically (UICHOICE_COL_DYNTIME)
 *    or static in which case the node finction is called once to populate.
 * All data can be updated regularly (UICHOICE_COL_DATATIME), in which case
 * ROUTES will be queried for updates only by the cache (rcache) (0 is
 * no update). 
 *
 * Each node can be invisible (UICHOICE_COL_ISVISIBLE), have a visible
 * label with markup (UICHOICE_COL_LABEL), a plain name (UICHOICE_COL_NAME),
 * icons (UICHOICE_COL_IMAGE, UICHOICE_COL_BIGIMAGE, UICHOICE_COL_BADGE) and
 * help (UICHOICE_COL_HELP, UICHOICE_COL_TOOLTIP).
 *
 * Standard data fields are provided to help: filenames (UICHOICE_COL_FNAME)
 * and working data is used to keep track of data state 
 * (UICHOICE_COL_DATATIMOUT, UICHOICE_COL_DYNTIMEOUT, UICHOICE_COL_AVAILFROM, 
 * UICHOICE_COL_AVAILTO)
 *
 * When clicked, the node is is queried for visualisation
 * type which is set (uivis_*), index is fetched (uidata_*), a slice of
 * data is downloaded (uitime_*), cached (rcache_*) and drawn (uitable_*, 
 * uigraph_*).
 *
 * Nigel Stuckey, May, June 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#define _GNU_SOURCE

#include <time.h>
#include <string.h>
#include <stdio.h>
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
#include "uichoice.h"
#include "uivis.h"
#include "uilog.h"
#include "uidialog.h"
#include "uipref.h"
#include "dyndata.h"
#include "fileroute.h"
#include "main.h"

/* filescope references to choice tree model (choicestore) and key branches 
 * (GtkTreeIter), cached in the file scope for speed */
GtkTreeStore *choicestore;
GtkTreeIter habparent, localparent, fileparent, hostparent, reposparent;
GtkTreeIter harvparent;

/* filescope progress structure */
struct uichoice_progress {
     int nchildren;
     int visited;
} visitor_progress;

/* file and repository lookups */
TREE *uichoice_fnames;			/* active file list or URLs for hosts */
TREE *uichoice_repnames;		/* active repository list */

/* file and route lists */
TREE *uichoice_myfiles_load;		/* open files being displayed in 
					 * choice tree. Removed from list
					 * when file is closed */
TREE *uichoice_myfiles_hist;		/* all observed file names for 
					 * history in combo boxes */
TREE *uichoice_myhosts_load;		/* open hosts being displayed in 
					 * choice tree. Removed from list
					 * when host is closed */
TREE *uichoice_myhosts_hist;		/* all observed hostnames for 
					 * history in combo boxes */

/* link to collect the ringpurl value */
extern char *uidata_ringpurl;


/* Build the initial choice tree and set up the associated variables
 *
 * Choice groups (the top levels) are hardcoded into 
 *    HABITAT, FILES, HOSTS and REPOSITORY
 * All other choices are added later.
 */
void uichoice_init() {
     GdkPixbuf *icon, *bigicon;
     char *purl, *label;

     /* Initialise filter for the choice tree. Dependency is:-
      *   choice_tree -> choice_treefilter -> choice_treestore
      *   (view)         (filter)             (model / store) */
     GtkTreeModelFilter *filter;
     filter = GTK_TREE_MODEL_FILTER(gtk_builder_get_object(gui_builder,
							 "choice_treefilter"));
     gtk_tree_model_filter_set_visible_column(filter, UICHOICE_COL_ISVISIBLE);

     /* create session lists */
     uichoice_myfiles_load = tree_create();	/* loaded files */
     uichoice_myfiles_hist = tree_create();	/* file history */
     uichoice_myhosts_load = tree_create();	/* loaded files */
     uichoice_myhosts_hist = tree_create();	/* file history */
     uichoice_fnames       = tree_create();	/* file+host purl list */
     uichoice_repnames     = tree_create();	/* reposit -> rep node list */

     /* Habitat root */
     choicestore = GTK_TREE_STORE(gtk_builder_get_object(gui_builder,
							 "choice_treestore"));

     icon = uichoice_load_pixbuf(UICHOICE_ICON_HABITAT);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_HABITAT);
     gtk_tree_store_append(choicestore, &habparent, NULL);
     gtk_tree_store_set(choicestore, &habparent, 
			UICHOICE_COL_LABEL,     "<b>HABITAT</b>",
			UICHOICE_COL_NAME,      "Habitat",
			UICHOICE_COL_IMAGE,     icon,
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_WHATNEXT,
			-1);

     icon    = uichoice_load_pixbuf(UICHOICE_ICON_THISHOST);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_THISHOST);
     label   = util_strjoin("This Host: ", util_hostname(), NULL);
#if 0
     purl    = util_strjoin("http://localhost:", HTTPD_PORT_HTTP_STR, 
			    "/localtsv/", NULL);
#else
     purl    = util_strjoin("local:", NULL);
#endif
     gtk_tree_store_append(choicestore, &localparent, &habparent);
     gtk_tree_store_set(choicestore, &localparent, 
			UICHOICE_COL_LABEL,     label,
			UICHOICE_COL_NAME,      label,
			UICHOICE_COL_TOOLTIP,   "Data collected from this host",
			UICHOICE_COL_IMAGE,     icon,
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_CHART,
			UICHOICE_COL_PURL,      purl,
			UICHOICE_COL_TYPE,      FILEROUTE_TYPE_RS,
			-1);
     nfree(label); nfree(purl);

     icon = uichoice_load_pixbuf(UICHOICE_ICON_MYFILES);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_MYFILES);
     gtk_tree_store_append(choicestore, &fileparent, NULL);
     gtk_tree_store_set(choicestore, &fileparent, 
			UICHOICE_COL_LABEL,     "<b>FILES</b>",
			UICHOICE_COL_NAME,      "Files",
			UICHOICE_COL_TOOLTIP,   "Data held in files",
			UICHOICE_COL_IMAGE,     icon,
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_ISVISIBLE, 0,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);

     icon = uichoice_load_pixbuf(UICHOICE_ICON_MYHOSTS);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_MYHOSTS);
     gtk_tree_store_append(choicestore, &hostparent, NULL);
     gtk_tree_store_set(choicestore, &hostparent, 
			UICHOICE_COL_LABEL,     "<b>HOSTS</b>",
			UICHOICE_COL_NAME,      "Hosts",
			UICHOICE_COL_TOOLTIP,   "Data from other hosts "
			                        "running Habitat ",
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_ISVISIBLE, 0,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);

     icon = uichoice_load_pixbuf(UICHOICE_ICON_REPOS);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_REPOS);
     gtk_tree_store_append(choicestore, &reposparent, NULL);
     gtk_tree_store_set(choicestore, &reposparent, 
			UICHOICE_COL_LABEL,     "<b>REPOSITORY</b>",
			UICHOICE_COL_NAME,      "Repository",
			UICHOICE_COL_TOOLTIP,   "Data from a Harvest repository",
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_ISVISIBLE, 0,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);

     icon = uichoice_load_pixbuf(UICHOICE_ICON_HUNTER);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_HUNTER);
     gtk_tree_store_append(choicestore, &harvparent, &reposparent);
     gtk_tree_store_set(choicestore, &harvparent, 
			UICHOICE_COL_LABEL,     "Hunter",
			UICHOICE_COL_NAME,      "Hunter",
			UICHOICE_COL_TOOLTIP,   "Hunter - repository, monitoring and management",
			UICHOICE_COL_IMAGE,     icon,
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_ISVISIBLE, 0,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);
}


/* deallocate structures created */
void uichoice_fini()
{
     /* remove nodes from tree, freeing the keys */
     tree_clearout(uichoice_fnames,   tree_infreemem, NULL);
     tree_clearout(uichoice_repnames, tree_infreemem, NULL);

     /* remove remaining trees */
     tree_destroy(uichoice_fnames);
     tree_destroy(uichoice_repnames);

     /* remove session lists: dup'ed keys */
     tree_clearout       (uichoice_myfiles_load, tree_infreemem, NULL);
     tree_clearout       (uichoice_myfiles_hist, tree_infreemem, NULL);
     tree_destroy        (uichoice_myfiles_load);
     tree_destroy        (uichoice_myfiles_hist);
     tree_clearoutandfree(uichoice_myhosts_load);
     tree_clearoutandfree(uichoice_myhosts_hist);
     tree_destroy        (uichoice_myhosts_load);
     tree_destroy        (uichoice_myhosts_hist);
}


/* expand the choice tree on initialisation */
/* not certain that this should be a sparate function -NS */
void uichoice_init_expand() {
     GtkTreeView *choicetree;

     choicetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder,
						       "choice_tree"));

     /* Expand whole tree after this initialisation */
#if 0
     GtkTreePath *path;

     path = gtk_tree_path_new_from_string("0");
     gtk_tree_view_expand_row(path, 1);
     gtk_tree_path_free(path);
#endif
     gtk_tree_view_expand_all(choicetree);
}


/* Load a pixbuf from a file */
GdkPixbuf *uichoice_load_pixbuf(char *pbname) {
     GError *error = NULL;
     GdkPixbuf *pb;
     char *pbpath;

     pbpath = util_strjoin(iiab_dir_lib, "/", pbname, NULL);
     pb = gdk_pixbuf_new_from_file(pbpath, &error);
     if (error) {
          g_warning ("uichoice_load_pixbuf() - Could not load icon: %s\n", 
		     error->message);
	  g_error_free(error);
	  error = NULL;
     }
     nfree(pbpath);

     return pb;
}


/* Add local configuration to choice tree and view it */
G_MODULE_EXPORT void 
uichoice_on_config(GtkMenuItem *object, gpointer user_data) {
     uichoice_mknode_thishost_config(TRUE, TRUE);
}


/* Create the node HABITAT->This Host->Configuration in the choice tree.
 * Optionally show it and run it*/
void uichoice_mknode_thishost_config(int show, int run) {
     GtkTreeIter cfg;
     GdkPixbuf *icon, *bigicon;
     GtkTreeView *choicetree;
     GtkTreePath *path;

     /* check if already drawn */

     icon = uichoice_load_pixbuf(UICHOICE_ICON_LOGS);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_LOGS);
     gtk_tree_store_append(choicestore, &cfg, &localparent);
     gtk_tree_store_set(choicestore, &cfg, 
			UICHOICE_COL_LABEL,   "Configuration",
			UICHOICE_COL_NAME,    "Parsed Configuration",
			UICHOICE_COL_TOOLTIP, "Parsed configuration in MyHabitat",
			UICHOICE_COL_IMAGE, icon,
			UICHOICE_COL_BIGIMAGE, bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_GETDATACB, dyndata_config,
			UICHOICE_COL_VISUALISE, UIVIS_TABLE,
			-1);

     /* get treeview & path for other actions */
     choicetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, 
						       "choice_tree"));
     path = gtk_tree_model_get_path(GTK_TREE_MODEL(choicestore), &cfg);

     if (show)
	  gtk_tree_view_expand_to_path(choicetree, path);       

     if (run)
          gtk_tree_view_set_cursor(choicetree, path, NULL, FALSE);

     /* Clear up */
     gtk_tree_path_free(path);
}


/* Add local collection replication log to choice tree and view it */
G_MODULE_EXPORT void 
uichoice_on_replication_log(GtkMenuItem *object, gpointer user_data) {
     char *purl;

     /* localhost replication ring */
#if 0
     purl    = util_strjoin("http://localhost:", HTTPD_PORT_HTTP_STR, 
			    "/localtsv/rep", NULL);
#else
     purl    = util_strjoin("local:rep,0", NULL);
#endif
     uichoice_mknode_thishost_collector(purl, 
					"Replication Log",
					"Local Collection Replication Log",
					"Replication log from local collector",
					TRUE, TRUE);
     nfree(purl);
}


/* Add local collection replication log to choice tree and view it */
G_MODULE_EXPORT void 
uichoice_on_collection_log(GtkMenuItem *object, gpointer user_data) {
     char *purl;

     /* localhost replication ring */
#if 0
     purl    = util_strjoin("http://localhost:", HTTPD_PORT_HTTP_STR, 
			    "/localtsv/log", NULL);
#else
     purl    = util_strjoin("local:log,0", NULL);
#endif
     uichoice_mknode_thishost_collector(purl,
					 "Collection Log",
					 "Local Collection Log",
					 "Log from local collector",
					 TRUE, TRUE);
     nfree(purl);
}


/* Add local collection event log to choice tree and view it */
G_MODULE_EXPORT void 
uichoice_on_event_log(GtkMenuItem *object, gpointer user_data) {
     char *purl;

     /* localhost replication ring */
#if 0
     purl    = util_strjoin("http://localhost:", HTTPD_PORT_HTTP_STR, 
			    "/localtsv/patact", NULL);
#else
     purl    = util_strjoin("local:patact,0", NULL);
#endif
     uichoice_mknode_thishost_collector(purl,
					 "Event Log",
					 "Local Collection Event Log",
					 "Event log from local collector",
					 TRUE, TRUE);
     nfree(purl);
}


/* Create the node HABITAT->This Host (name)->NEWNODE in the choice tree.
 * Use purl as data source, label for onscreen text, name+tooltip to
 * compile the help. The flag show expands the tree path down to
 * the new label, run selects the node as though it had been clicked,
 * causing the purl to be shown */
void uichoice_mknode_thishost_collector(char *purl,	/* Data source */
					char *label, 	/* Visible label */
					char *name,	/* Full name */
					char *tooltip,	/* Label's tooltip */
					int show,	/* Show tree path */
					int run		/* Autoclick */) {
     GtkTreeIter log;
     GdkPixbuf *icon, *bigicon;
     GtkTreeView *choicetree;
     GtkTreePath *path;
     char *existing_label;

     /* need to check if the same node has been created before
      * by walking across the local choice node */
     if (gtk_tree_model_iter_children(GTK_TREE_MODEL(choicestore), 
				      &log, &localparent) == FALSE) {
          elog_printf(FATAL, "Unable to find children of local parent");
	  return;
     }
     do {
          gtk_tree_model_get(GTK_TREE_MODEL (choicestore), &log, 
			     UICHOICE_COL_LABEL,           &existing_label,
			     -1);
	  if (strcmp(label, existing_label) == 0) {
	       /* the same label is in the local branch of the choice tree */
	       g_free(existing_label);
	       return;
	  }
	  g_free(existing_label);
     } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(choicestore), 
				       &log));

     /* create node */
     icon = uichoice_load_pixbuf(UICHOICE_ICON_LOGS);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_LOGS);
     gtk_tree_store_append(choicestore, &log, &localparent);
     gtk_tree_store_set(choicestore, &log, 
			UICHOICE_COL_LABEL,     label,
			UICHOICE_COL_NAME,      name,
			UICHOICE_COL_TOOLTIP,   tooltip,
			UICHOICE_COL_IMAGE,     icon,
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_PURL,      purl,
			UICHOICE_COL_VISUALISE, UIVIS_TABLE,
			-1);

     /* get treeview & path for other actions */
     choicetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder, 
						       "choice_tree"));
     path = gtk_tree_model_get_path(GTK_TREE_MODEL(choicestore), &log);

     if (show)
	  gtk_tree_view_expand_to_path(choicetree, path);       

     if (run)
          gtk_tree_view_set_cursor(choicetree, path, NULL, FALSE);

     /* Clear up */
     gtk_tree_path_free(path);
}



/* Add more items to the 'this host' node in the choice tree */
G_MODULE_EXPORT void 
uichoice_on_more_local(GtkCheckMenuItem *object, gpointer user_data) {
     GtkTreeView *choicetree;
     GtkTreePath *path;

     /* Check the current state of 'more local'. Do we need to display 
      * more local choice nodes or do we need to remove them? */
     if ( gtk_check_menu_item_get_active(object) ) {
          /* add extra local nodes to choice tree */
          uichoice_mknode_thishost_extra();

	  /* Expand 'this host' node in the view */
	  choicetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder,
							    "choice_tree"));
	  path = gtk_tree_model_get_path(GTK_TREE_MODEL(choicestore), 
					 &localparent);
	  gtk_tree_view_expand_row(choicetree, path, 1);
	  gtk_tree_path_free(path);
     } else {
          uichoice_rmnode_thishost_extra();
     }
}


/* Create additional nodes and attach them to 'this host' node in the 
 * choice tree */
void uichoice_mknode_thishost_extra() {
     GtkTreeIter thishost, logs;
     GdkPixbuf *icon, *bigicon;

     /* Add to the 'thishost' node */
#if 0
     icon = uichoice_load_pixbuf(UICHOICE_ICON_UPTIME);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_UPTIME);
     gtk_tree_store_append(choicestore, &thishost, &localparent);
     gtk_tree_store_set(choicestore, &thishost, 
			UICHOICE_COL_LABEL,    "Uptime",
			UICHOICE_COL_NAME,     "Uptime",
			UICHOICE_COL_TOOLTIP,  "Time this host has been up and running",
			UICHOICE_COL_IMAGE,     icon,
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);
     icon = uichoice_load_pixbuf(UICHOICE_ICON_PERF);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_PERF);
     gtk_tree_store_append(choicestore, &thishost, &localparent);
     gtk_tree_store_set(choicestore, &thishost, 
			UICHOICE_COL_LABEL,   "Performance",
			UICHOICE_COL_NAME,    "Performance",
			UICHOICE_COL_TOOLTIP, "Performance statistics",
			UICHOICE_COL_IMAGE, icon,
			UICHOICE_COL_BIGIMAGE, bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);
     icon = uichoice_load_pixbuf(UICHOICE_ICON_EVENTS);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_EVENTS);
     gtk_tree_store_append(choicestore, &thishost, &localparent);
     gtk_tree_store_set(choicestore, &thishost, 
			UICHOICE_COL_LABEL,   "Events",
			UICHOICE_COL_NAME,    "Events",
			UICHOICE_COL_TOOLTIP, "Events on this host",
			UICHOICE_COL_IMAGE, icon,
			UICHOICE_COL_BIGIMAGE, bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);
#endif
     icon = uichoice_load_pixbuf(UICHOICE_ICON_LOGS);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_LOGS);
     gtk_tree_store_append(choicestore, &thishost, &localparent);
     gtk_tree_store_set(choicestore, &thishost, 
			UICHOICE_COL_LABEL,   "Agent Logs",
			UICHOICE_COL_NAME,    "Agent Logs",
			UICHOICE_COL_TOOLTIP, "Log messages from the collection agent on this host",
			UICHOICE_COL_IMAGE, icon,
			UICHOICE_COL_BIGIMAGE, bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);
     icon = uichoice_load_pixbuf(UICHOICE_ICON_REP);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_REP);
     gtk_tree_store_append(choicestore, &logs, &thishost);
     gtk_tree_store_set(choicestore, &logs, 
			UICHOICE_COL_LABEL,   "Replication",
			UICHOICE_COL_NAME,    "Replication",
			UICHOICE_COL_TOOLTIP, "Replication messages from this host",
			UICHOICE_COL_IMAGE, icon,
			UICHOICE_COL_BIGIMAGE, bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);
     icon = uichoice_load_pixbuf(UICHOICE_ICON_JOBS);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_JOBS);
     gtk_tree_store_append(choicestore, &logs, &thishost);
     gtk_tree_store_set(choicestore, &logs, 
			UICHOICE_COL_LABEL,   "Jobs",
			UICHOICE_COL_NAME,    "Jobs",
			UICHOICE_COL_TOOLTIP, "Job table for this host",
			UICHOICE_COL_IMAGE, icon,
			UICHOICE_COL_BIGIMAGE, bigicon,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_INFO,
			-1);
}


/* Remove the additional nodes under the 'this host' node in the choice tree */
void uichoice_rmnode_thishost_extra() {
     GtkTreeIter iter;

     /* remove additional local nodes - iterate over children of the
      * local node (localparent) removing all nodes */
     if ( gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(choicestore), &iter,
					&localparent, 0) == TRUE ) {
          while (gtk_tree_store_remove(choicestore, &iter) == TRUE)
	       ;
     }
}



/* callback to open files and load into choice */
G_MODULE_EXPORT void 
uichoice_on_file_open  (GtkButton *object, gpointer user_data)
{
     char *fname, *fmtname;
     GtkWidget *filechooser_win, *filechooser_format;

     /* grab widgets */
     filechooser_win = get_widget("filechooser_win");
     filechooser_format = get_widget("filechooser_format_combo");

     /* update the interface: query and hide the filechooser, set status */
     uilog_setprogress("Skimming file", 0.0, 0);
     fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (filechooser_win));
     fmtname = gtk_combo_box_get_active_text(GTK_COMBO_BOX
					     (filechooser_format));
     gtk_widget_hide(filechooser_win);

     /* attempt to load the file and free */
     uichoice_loadfile(fname, fmtname);
     g_free(fname);
     uilog_clearprogress();
}


/*
 * Open a file containing performance data and load it into the choice tree
 * under 'my files'.
 *    fname - name of file
 *    format - format hint in text
 * Files can be in several formats: RS, CSV, TSV, SSV, TEXT etc. The format
 * string starts with the format name as a hint and the remainder will 
 * be ignored. If not provided (signified by NULL) an attempt will be made 
 * to work it our automatically.
 * Errors will be displayed directly to user.
 */
void uichoice_loadfile(char *fname, char *format)
{
     GtkTreeIter newfile;
     GdkPixbuf *icon, *bigicon;
     GtkListStore *closestore;
     RS_SUPER super;
     char fullinfo[1024], *iconname, *bigiconname, *shortname=NULL, *purl=NULL;
     int canchart=0, r;
     TABLE ringsinfo=NULL;
     FILEROUTE_TYPE ftype;
     char *myformat;

     /* check if we have already read this file */
     if ( tree_find(uichoice_fnames, fname) != TREE_NOVAL ) {
          elog_printf(INFO, "File %s has already been loaded", fname);
          uilog_modal_alert("File Already Loaded", 
			    "The file %s has already been loaded", fname);
	  return;
     }

     /* check read access */
     if ( access( fname, R_OK ) ) {
          elog_printf(FATAL, "Unable to load %s. Please check that the file "
		      "is readable and that the file has not been moved",
		      fname);
	  return;
     }

     /* If no format hint given, try to work it out from the file extension */
     if ( format )
          myformat = format;
     else
          myformat = util_fileext(fname);

     /* Scan for format string */
     if (myformat) {
          if (strcasestr(myformat, "grs") ||
	      strcasestr(myformat, "rs") )
	       ftype = FILEROUTE_TYPE_GRS;
	  else if (strcasestr(myformat, "csv"))
	       ftype = FILEROUTE_TYPE_CSV;
	  else if (strcasestr(myformat, "tsv"))
	       ftype = FILEROUTE_TYPE_TSV;
	  else if (strcasestr(myformat, "ssv"))
	       ftype = FILEROUTE_TYPE_SSV;
	  else if (strcasestr(myformat, "psv"))
	       ftype = FILEROUTE_TYPE_PSV;
	  else if (strcasestr(myformat, "txt"))
	       ftype = FILEROUTE_TYPE_TEXT;
	  else
	       ftype = FILEROUTE_TYPE_UNKNOWN;
     } else {
          ftype = FILEROUTE_TYPE_UNKNOWN;
     }

     /* Now we try to open the file given the format type */
     shortname = util_basename(fname);
     if (ftype == FILEROUTE_TYPE_GRS) {
          /* GDBM ringstore type. Gather information from it using ringtore
	   * specific routines [rs_info_super()] rather than using ROUTEs
	   * as it is stateless and will give more info */
	  super = rs_info_super(&rs_gdbm_method, fname);
	  if (super) {
	       /* if successfully read an RS file, make an appropriate 
		* info string */
	       snprintf(fullinfo, 1024, 
			"%s (ringstore v%d, OS %s %s %s %s, on %s, created %s)",
			fname, super->version,
			super->os_name, super->os_release, super->os_version,
			super->machine, super->machine,
			util_decdatetime(super->created));

	       rs_free_superblock(super);

	       /* populate the table with other meta and directoy information */
	       ringsinfo = rs_inforings(&rs_gdbm_method, fname);

	       canchart=1;
	       purl = util_strjoin("grs:", fname, NULL);
	       iconname = UICHOICE_ICON_CHART;
	       bigiconname = UICHOICE_BIGICON_CHART;
	  } else {
	       /* failed to read the ringstore */
	       r = uidialog_yes_or_no("Unable to Load Ringstore File",
				  "Unable to load ringstore file. Read as "
				  "text instead?",
				  "The file %s could not be recognised as a "
				  "ringstore format and its structure can not "
				  "be read. Please check its format and "
				  "manually select the file type in the "
				  "'file open' window\n"
				  "Do you want to read the file as plain "
				  "text?\n",
				  fname);
	       if (r == UIDIALOG_YES)
		    ftype = FILEROUTE_TYPE_TEXT;
	       else
		    return;
	  }
     }
     if (ftype == FILEROUTE_TYPE_TSV || 
	 ftype == FILEROUTE_TYPE_CSV || 
	 ftype == FILEROUTE_TYPE_PSV || 
	 ftype == FILEROUTE_TYPE_SSV) {
          /* Fat Headed Array Type */
	  canchart=1;
	  purl = util_strjoin("file:", fname, NULL);
	  iconname = UICHOICE_ICON_CSV;
	  bigiconname = UICHOICE_BIGICON_CSV;
     }
     if (ftype == FILEROUTE_TYPE_TEXT) {
          /* Plain text type */
	  canchart=0;
	  purl = util_strjoin("file:", fname, NULL);
	  iconname = UICHOICE_ICON_TEXT;
	  bigiconname = UICHOICE_BIGICON_TEXT;
     }
     if (ftype == FILEROUTE_TYPE_UNKNOWN) {
          /* Unknown file type */
	  canchart=0;
	  purl = util_strjoin("file:", fname, NULL);
	  iconname = UICHOICE_ICON_TEXT;
	  bigiconname = UICHOICE_BIGICON_TEXT;
          /*elog_printf(FATAL, "Unable to load %s as a ringstore.\n"
		      "Please check the format of the file and manually "
		      "select the file type in the file open window",
		      fname);
		      return;*/
     }

     /* add to file lists: referenced file to node, session & history */
     tree_add(uichoice_fnames, xnstrdup(fname), NULL);
     uichoice_add_myfiles_load(fname);
     uichoice_add_myfiles_hist(fname);

     /* Add to the close list store model */
     closestore = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
							"close_liststore"));
     icon = uichoice_load_pixbuf(iconname);
     gtk_list_store_append(closestore, &newfile);
     gtk_list_store_set(closestore, &newfile, 
			UICHOICE_CLOSE_COL_NAME,     shortname,
			UICHOICE_CLOSE_COL_DETAILS,  fname,
			UICHOICE_CLOSE_COL_TOOLTIP,  fullinfo,
			UICHOICE_CLOSE_COL_ICON,     icon,
			UICHOICE_CLOSE_COL_PURL,     purl,
			UICHOICE_CLOSE_COL_FNAME,    fname,
			-1);

     /* Add the filename to the 'myfiles' node */
     gtk_tree_store_set(choicestore, &fileparent, 
			UICHOICE_COL_ISVISIBLE,   1,
			-1);
     icon = uichoice_load_pixbuf(iconname);
     bigicon = uichoice_load_pixbuf(bigiconname);
     gtk_tree_store_append(choicestore, &newfile, &fileparent);
     gtk_tree_store_set(choicestore, &newfile, 
			UICHOICE_COL_LABEL,     shortname,
			UICHOICE_COL_NAME,      shortname,
			UICHOICE_COL_TOOLTIP,   fname,
			UICHOICE_COL_IMAGE,     icon,
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_HELP,      fullinfo,
			UICHOICE_COL_ISDYNAMIC, 0,
			UICHOICE_COL_PURL,      purl,
			UICHOICE_COL_FNAME,     fname,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, canchart ? 
			         UIVIS_CHART : UIVIS_TEXT,
			UICHOICE_COL_TYPE,      ftype,
			-1);

     /* Always expand the file node */
     GtkTreeView *choicetree;
     GtkTreePath *path;

     choicetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder,
						       "choice_tree"));
     path = gtk_tree_path_new_from_string("0:1");
     gtk_tree_view_expand_row(choicetree, path, 0);
     gtk_tree_path_free(path);

     /* clear up */
     if (ringsinfo)
          table_destroy(ringsinfo);
     if (purl)
          nfree(purl);
}


/* callback to add a peer or repository host into choice */
G_MODULE_EXPORT void 
uichoice_on_host_add  (GtkButton *object, gpointer user_data)
{
     int from_repos=0;
     GtkWidget *connect_win, *hostname_entry, *connect_source_repos;
     char *purl, *hostname=NULL;

     /* grab widget refs */
     connect_win          = get_widget("connect_win");
     hostname_entry       = get_widget("connect_hostname_entry");
     connect_source_repos = get_widget("connect_source_repos");

     /* update the interface: query and hide the filechooser, set status */
     uilog_setprogress("contacting remote", 0.0, 0);

     /* extract data from the interface & hide window */
     if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						  connect_source_repos)) )
          from_repos++;
     hostname = (char *) gtk_entry_get_text(GTK_ENTRY(hostname_entry));
     gtk_widget_hide(connect_win);

     /* attempt to load the file and free */
     if (from_repos) {
          /* load from repository, purl format 'sqlrs:[hostname]' */
          purl = util_strjoin("sqlrs:", hostname, NULL);
	  uichoice_loadhost(purl, hostname);
     } else {
          /* load from peer habitat, purl format 
	   * 'http://[hostname]:[port]/localtsv/' */
          purl = util_strjoin("http://", hostname, ":", HTTPD_PORT_HTTP_STR, 
			      "/localtsv/", NULL);
          uichoice_loadhost(purl, hostname);
     }

     /* clear and tidy up */
     nfree(purl);
     uilog_clearprogress();
}


/*
 * Open a host using a route and make a description summary from its meta 
 * information.
 * The route should refer to the top most component of the specification, 
 * for instance 'sqlrs:myhost' or 'grs:/path/to/rs_file' or
 * 'http://host[:port]/path/to/tab/fmt/server'.
 */
void uichoice_loadhost(char *purl,		/* route spec p-url */
		       char *label		/* visible label */)
{
     GtkTreeIter newhost;
     GdkPixbuf *icon, *bigicon;
     char infopurl[256], *shortname=NULL, fullinfo[1024], *hostinfo, 
       *iconname, *bigiconname;
     TABLE tab;
     int i, infolen=0;
     TREE *row;
     GtkListStore *closestore;

     /* check if we have already read this route (rather than host) */
     if ( tree_find(uichoice_fnames, purl) != TREE_NOVAL ) {
          elog_printf(INFO, "Host (route %s) has already been loaded", purl);
          elog_printf(FATAL, "<big><b>Host '%s' Already Loaded</b></big>\n\n", 
		      label);
	  return;
     }

     /* NEED TO CHANGE THIS TO MAKE IT MORE SIMPLE. ALL I NEED TO DO IS
      * TO FIND A LITTLE BIT OF INOFMRATION ABOUT THIS HOST, LIKE A 
      * DESCRIPTION AND MAKE IT AVILABLE AS INFOMRATION */

     /* Read in the status of the host by appending '?info' to the p-url
      * seeing if scannable data is returned. The name at least should 
      * come back */
     snprintf(infopurl, 256, "%s?info", purl);
     tab = route_tread(infopurl, NULL);
     if ( ! tab ) {
          elog_printf(DIAG, "Unable to read %s as table", infopurl);
          elog_printf(FATAL, "<big><b>Unable to Load Host <i>%s</i></b></big>\n"
		      "The habitat peer or repository is uncontactable, "
		      "not listening or has ceased to exist",
		      label);
	  return;
     }

     /* The table should be a single row as it refers to a single machine, and
      * should have several columns, expected to be:-
      *    about, dur, id, long, name, nslots, oseq, otime, yseq, ytime
      * The column name 'host name' should contain the hostname which we 
      * should grab, then use the other rows for information. */
     table_first(tab);
     row = table_getcurrentrow(tab);
     tree_traverse(row) {
       /*	  if (strcmp(tree_getkey(row), "name") == 0)
		  shortname = tree_get(row);*/
          shortname = label;
	  infolen += snprintf(fullinfo+infolen, 1024-infolen, "%s: %s ", 
			      tree_getkey(row), (char *) tree_get(row));
     }
     tree_destroy(row);

     /* check the host name exists and use a default otherwise */
     if (shortname == NULL)
	  shortname = label;


     /* create hostinfo, which is a purl to the a host information table
      * Create this by lopping off the trailing file element from purl
      * when delimited by '/'. Purl is assumed to be of the form:-
      *     hostinfo/killdir/
      * where the trailing slash is optional. 'killdir' is removed
      * hostinfo is produced in the form:-
      *     hostinfo/info    for peer habitat access
      * or  hostinfo?linfo    for repsository access
      */
     hostinfo = xnstrdup(purl);
     i = strlen(hostinfo) - 1;
     if (hostinfo[i] == '/')			/* strip trailing '/' */
	  hostinfo[i--] = '\0';
     while (i >= 0 && hostinfo[i] != '/')	/* find dir separator */
	  i--;
     if (i < 0) {
	  /* no suitable separating slash, so its a host request to the 
	   * repository (sqlrs:host) and requires an '?info' appended */
          i = strlen(hostinfo);
	  hostinfo = xnrealloc(hostinfo, i+6);
	  strcpy(&hostinfo[i], "?linfo");
	  iconname = UICHOICE_ICON_REPOSHOST;		/* repos attach */
	  bigiconname = UICHOICE_BIGICON_REPOSHOST;	/* repos attach */
     } else {
	  /* its a single host, direct request to another habitat instance
	   * so append 'info' */
	  hostinfo[++i] = '\0';
	  hostinfo = xnrealloc( hostinfo, strlen(hostinfo) + 10 );
	  strcpy(&hostinfo[i], "linfo");	/* turns into '/info' */
	  iconname = UICHOICE_ICON_PEERHOST;		/* peer attach */
	  bigiconname = UICHOICE_BIGICON_PEERHOST;	/* peer attach */
     }

     /* get hostinfo */
     /* ---TO BE DONE--- */

     /* add to file lists: referenced file to node, session & history */
     tree_add(uichoice_fnames, xnstrdup(purl), NULL);
     uichoice_add_myhosts_load(shortname, purl);
     uichoice_add_myhosts_hist(shortname, purl);

     /* Add to the close list store model */
     closestore = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
							"close_liststore"));
     icon = uichoice_load_pixbuf(iconname);
     gtk_list_store_append(closestore, &newhost);
     gtk_list_store_set(closestore, &newhost, 
			UICHOICE_CLOSE_COL_NAME,     shortname,
			UICHOICE_CLOSE_COL_DETAILS,  hostinfo,
			UICHOICE_CLOSE_COL_TOOLTIP,  fullinfo,
			UICHOICE_CLOSE_COL_ICON,     icon,
			UICHOICE_CLOSE_COL_PURL,     purl,
			-1);

     /* Add the hostname to the 'HOSTS' node */
     gtk_tree_store_set(choicestore, &hostparent, 
			UICHOICE_COL_ISVISIBLE,   1,
			-1);
     icon = uichoice_load_pixbuf(iconname);
     bigicon = uichoice_load_pixbuf(bigiconname);
     gtk_tree_store_append(choicestore, &newhost, &hostparent);
     gtk_tree_store_set(choicestore, &newhost, 
			UICHOICE_COL_LABEL,     shortname,
			UICHOICE_COL_NAME,      shortname,
			UICHOICE_COL_TOOLTIP,   fullinfo,
			UICHOICE_COL_IMAGE,     icon,
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_HELP,      fullinfo,
			UICHOICE_COL_ISDYNAMIC, 0,
			UICHOICE_COL_PURL,      purl,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_CHART,
			UICHOICE_COL_TYPE,      FILEROUTE_TYPE_RS,
			-1);

     /* Always expand the hosts node */
     GtkTreeView *choicetree;
     GtkTreePath *path;

     choicetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder,
						       "choice_tree"));
     path = gtk_tree_path_new_from_string("0:2");
     gtk_tree_view_expand_row(choicetree, path, 0);
     gtk_tree_path_free(path);

     /* free and return */
     table_destroy(tab);
     nfree(hostinfo);
}


/*
 * Close a data source using file name and purl pair.
 * Common closing routine for a set of different callbacks.
 * If it has a file name, it is treated as a file otherwise it is a 
 * host from a peer or repsoitory.
 * If neither are present, then the close iwndow is presented and 
 * this routine returns.
 */
void uichoice_source_close(char *fname, char *purl)
{
     char *choicepurl, *choicefile, *choicelabel;
     GtkTreeIter iter;

     /* if displaying choice in data vis, move to a spashscreen.
      * Use the most significant part of the purl as ring will have
      * been appended to uidata_ringpurl */
     if (purl && uidata_ringpurl &&
	 strncmp(purl, uidata_ringpurl, strlen(purl)) == 0)
          /* displaying the chosen ring -- undisplay it by moving to splash */
          uivis_change_view(UIVIS_SPLASH);

     /* is it in the active file / purl list ? */
     if ( fname ) {
          /* file */
          if (tree_find(uichoice_fnames, fname) == TREE_NOVAL ) {
	       elog_printf(FATAL, "Closed file not cosidered to be active: "
			   "fname=%s purl=%s", fname, purl);
	       return;
	  }
     } else if ( purl ) {
          /* purl */
          if ( tree_find(uichoice_fnames, purl) == TREE_NOVAL ) {
	       elog_printf(FATAL, "Closed choice item not considered active "
			   "purl=%s", purl);
	       return;
	  }
     } else {
          return;
     }

     /* remove node from choice tree: file or host subtree ? */
     if ( fname ) {
	  /* its a file, remove from the file section of the choice tree */
          uichoice_rm_myfiles_load(fname);

	  if (gtk_tree_model_iter_children(GTK_TREE_MODEL(choicestore), 
					   &iter, &fileparent) == FALSE) {
	       elog_printf(FATAL, "Unable to find children of file parent");
	       return;
	  }
	  do {
	       gtk_tree_model_get(GTK_TREE_MODEL (choicestore), &iter, 
				  UICHOICE_COL_FNAME,           &choicefile,
				  -1);
	       if (strcmp(fname, choicefile) == 0) {
		    gtk_tree_store_remove(choicestore, &iter);
		    g_free(choicefile);
		    return;
	       }
	       g_free(choicefile);
	  } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(choicestore), 
					    &iter));
	  elog_printf(FATAL, "Unable to find choice file node with name %s ",
		      fname);
     } else {
          /* its a host: remove from the host section of the choice tree */

	  if (gtk_tree_model_iter_children(GTK_TREE_MODEL(choicestore), 
					   &iter, &hostparent) == FALSE) {
	       elog_printf(FATAL, "Unable to find children of host parent");
	       return;
	  }
	  do {
	       gtk_tree_model_get(GTK_TREE_MODEL (choicestore), &iter, 
				  UICHOICE_COL_LABEL,           &choicelabel,
				  UICHOICE_COL_PURL,            &choicepurl,
				  -1);
	       if (strcmp(purl, choicepurl) == 0) {
		    gtk_tree_store_remove(choicestore, &iter);
		    uichoice_rm_myhosts_load(choicelabel);

		    g_free(choicelabel);
		    return;
	       }
	       g_free(choicelabel);
	  } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(choicestore), 
					    &iter));
	  elog_printf(FATAL, "Unable to find choice host node with purl %s", 
		      purl);

     }

}


/* callback to close files and hosts and remove them from the choice menu */
G_MODULE_EXPORT void 
uichoice_on_source_close  (GtkButton *object, gpointer user_data)
{
     char *purl, *fname;
     GtkWidget *close_win;
     GtkTreeView *closetree;
     GtkTreeSelection *selection;
     GtkTreeIter iter;
     GtkTreeModel *model;
     GtkListStore *closestore;

     /* grab widgets */
     close_win = get_widget("close_win");
     closetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder,
						      "close_tree"));
     closestore = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
						      "close_liststore"));

     /* get selected close item */
     selection = gtk_tree_view_get_selection(closetree);
     if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
	  /* extract data from the tree model */
          gtk_tree_model_get (model, &iter, 
			      UICHOICE_CLOSE_COL_PURL,  &purl,
			      UICHOICE_CLOSE_COL_FNAME, &fname,
			      -1);
     } else {
          /* unslected: do nothing */
          return;
     }

     /* remove item from close list */
     gtk_list_store_remove(closestore, &iter);

     uichoice_source_close(fname, purl);

     /* update the interface: query and hide the filechooser, set status */
     uilog_setprogress("Closing source", 0.0, 0);
     gtk_widget_hide(close_win);

     uilog_clearprogress();
     g_free(purl);
     g_free(fname);
}


/* callback to close files and hosts and remove them from the choice menu 
 * when given a signal from the close list row (row-activate) */
G_MODULE_EXPORT void 
uichoice_on_source_close_by_row  (GtkButton *object, gpointer user_data)
{
     uichoice_on_source_close(object, user_data);
}


/* popup window handler, needed for keyboard generated right-clicks */
G_MODULE_EXPORT gboolean
uichoice_on_popup_menu(GtkWidget *treeview, gpointer user_data)
{
     g_print("uichoice_on_popup_menu\n");
     uichoice_popup_menu(treeview, NULL, user_data);
     return TRUE;	/* been handled, go to next handler */
}


/* callback for button press to intercept a right click (button3)  */
G_MODULE_EXPORT gboolean
uichoice_on_button_press(GtkWidget *treeview, GdkEventButton *event, 
			 gpointer user_data)
{
     if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
	  /* single right click */
          GtkTreeSelection *selection;

	  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	  if (gtk_tree_selection_count_selected_rows(selection)  <= 1) {
	       GtkTreePath *path;

	       if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
						 (gint) event->x, 
						 (gint) event->y,
						 &path, NULL, NULL, NULL)) {

		    gtk_tree_selection_unselect_all(selection);
		    gtk_tree_selection_select_path(selection, path);
		    gtk_tree_path_free(path);
	       }
	  }

	  uichoice_popup_menu(treeview, event, user_data);

	  return TRUE;	/* we did handle this */
     }

     return FALSE;	/* we did not handle this */
}


/* Popup menu on the chocie tree */
void 
uichoice_popup_menu  (GtkWidget *treeview, GdkEventButton *event, 
		      gpointer user_data)
{
     GtkWidget *menu, *menuitem;

     /* get widgets */
     menu     = get_widget("choice_popup");
     menuitem = get_widget("pop_choice_close");

     /* post the popup */
     gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		    (event != NULL) ? event->button : 0,
		    gdk_event_get_time((GdkEvent*)event));
}


/* Callback to close file from the popup menu
 * Close the entry highlighted by the selected row. If no row is selected, 
 * then popup the general close source window */
G_MODULE_EXPORT void 
uichoice_on_source_close_by_popup  (GtkButton *object, gpointer user_data)
{
     GtkWidget *closewin;
     GtkTreeView *choicetree;
     GtkTreeSelection *selection;
     GtkTreeIter iter;
     GtkTreeModel *model;
     GtkListStore *closestore;
     char *purl, *fname, *close_purl, *close_fname;

     closewin = get_widget("close_win");
     choicetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder,
						       "choice_tree"));

     /* get selected choice item */
     selection = gtk_tree_view_get_selection(choicetree);
     if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
	  /* extract data from the tree model */
          gtk_tree_model_get (model, &iter, 
			      UICHOICE_COL_PURL,  &purl,
			      UICHOICE_COL_FNAME, &fname,
			      -1);

	  if ( fname || purl ) {
	       uichoice_source_close(fname, purl);
	  } else {
	       /* not a file or host: open close source window to select */
	       gtk_window_present(GTK_WINDOW(closewin));
	       return;
	  }
     } else {
          /* unselected: open close source window to select */
          gtk_window_present(GTK_WINDOW(closewin));
	  return;
     }

     /* Remove from close tree by searching for purl and fname */
     closestore = GTK_LIST_STORE(gtk_builder_get_object(gui_builder,
							"close_liststore"));
     if ( gtk_tree_model_get_iter_first(GTK_TREE_MODEL(closestore), 
					&iter) == FALSE ) {
          elog_printf(FATAL, "Unable to find children of close list");
	  return;
     }
     do {
          gtk_tree_model_get(GTK_TREE_MODEL(closestore), &iter,
			     UICHOICE_CLOSE_COL_PURL,    &close_purl,
			     UICHOICE_CLOSE_COL_FNAME,   &close_fname,
			     -1);
	  if (fname && close_fname && strcmp(fname, close_fname) == 0) {
	       gtk_list_store_remove(closestore, &iter);
	       break;
	  } else if (purl && close_purl && strcmp(purl, close_purl) == 0) {
	       gtk_list_store_remove(closestore, &iter);
	       break;
	  }
     } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(closestore), &iter));

	 g_free(purl);
       g_free(fname);
}


/*
 * Add a repository to the choice tree.
 * The route should be a url to a web object that understands standard
 * system garden repository addressing and responds to sqlrs: or grs: 
 * style formats.
 * For example, 'http://host[:port]/path/to/repos/server'.
 */
void uichoice_loadrepository(char *purl,	/* route spec p-url */
			     char *org		/* organisation name */)
{
     GtkTreeIter newrepos;
     GdkPixbuf *icon, *bigicon;
     char fullinfo[1024];

     /* check if we have already opened this repository */
     /* TODO currently this does not support multiple organisations per 
      * repos and will need to be changed */
     if ( tree_find(uichoice_repnames, purl) != TREE_NOVAL ) {
          elog_printf(INFO, "Repository %s has already been loaded", purl);
          uilog_modal_alert("Repository Already Loaded", 
			    "The repository %s has already been loaded", purl);
	  return;
     }

     /* Add the repository to the 'REPOSITORY' node */
     gtk_tree_store_set(choicestore, &reposparent, 
			UICHOICE_COL_ISVISIBLE,   1,
			-1);
     icon = uichoice_load_pixbuf(UICHOICE_ICON_HARVEST);
     bigicon = uichoice_load_pixbuf(UICHOICE_BIGICON_HARVEST);
     gtk_tree_store_append(choicestore, &newrepos, &reposparent);
     gtk_tree_store_set(choicestore, &newrepos, 
			UICHOICE_COL_LABEL,     org,
			UICHOICE_COL_NAME,      org,
			UICHOICE_COL_TOOLTIP,   "Harvest, repository and "
						"utilisation analysis",
			UICHOICE_COL_IMAGE,     icon,
			UICHOICE_COL_BIGIMAGE,  bigicon,
			UICHOICE_COL_HELP,      fullinfo,
			UICHOICE_COL_ISDYNAMIC, 1,
			UICHOICE_COL_PURL,      "sqlrs:g=",
			UICHOICE_COL_GETDYNCB,  uichoice_on_mknode_repos_level,
			UICHOICE_COL_ISVISIBLE, 1,
			UICHOICE_COL_VISUALISE, UIVIS_TABLE,
			UICHOICE_COL_TYPE,      FILEROUTE_TYPE_RS,
			-1);

     elog_printf(INFO, "Repository enabled (%s)", purl);
}


/*
 * Callback to update or populate a repository node level.
 * Called by the choice tree updater visitor
 */
void uichoice_on_mknode_repos_level(GtkTreeModel *model, 
				    GtkTreePath *path, 
				    GtkTreeIter *iter)
{
     uichoice_mknode_repos_level(iter, "");
}


/*
 * Build an internal node in the choice tree to represent the repository.
 * Given a parent node, will query the repository for its children
 * and will create these nodes in the choice tree's model.
 * These will casecade to form a full tree (ie this node's callback will call 
 * the next), finally followed by a regular ROUTE as a terminal node.
 *
 * Final structure will be
 * 
 * <reposname> -+- <reposlevel0,1> -+- <reposlevel1,1> -+- [hostname1]
 *              |                   |                   \- [hostname2]
 *              |                   +- <reposlevel1,2> -+- [hostname3]
 *              |                                       \- [hostname4]
 *              +- <reposlevel0,2> -+- <reposlevel1,3> -+- [hostname5]
 *                                  |                   \- [hostname6]
 *                                  +- <reposlevel1,4> -+- [hostname7]
 *                                                      \- [hostname8]
 *
 * Where <reposname> is the node created by uichoice_loadrepository(),
 * <reposlevelN,M> are created by this routine 
 * uichoice_mknode_repository_level() and are terminated by a regular ROUTE
 * node.
 */
void uichoice_mknode_repos_level(GtkTreeIter *parent_node, 
				 char *level_name)
{
#if 0
     ITREE *nodelist, *leafnodes;
     char *basepurl, purl[256], purl2[256], *pt, *stack[256];
     TABLE groups;
     int r, stackpt;
     struct uichoice_node *node, *walknode;

     /* get args */
     basepurl = tree_find(nodeargs, "repurl");
     if ( basepurl == TREE_NOVAL ) {
	  elog_printf(DIAG, "No respository address set up to query");
	  return NULL;
     }

     /* Repository is enabled and the string will be picked up by the 
      * route class code, thus we can now talk in sqlrs: style
      * addressing.
      *
      * Compose the repository string to query for all groups.
      * NB this query may take a significant time */
     snprintf(purl, 256, "sqlrs:g=");
     groups = route_tread(purl, NULL);
     if ( ! groups ) {
          elog_printf(ERROR, "Unable to read repository groups. "
		      "Check diagnostic logs with your administrator", purl);
	  return NULL;
     }

     /* change the names of the columns to match the expected
      * and add some more */
     table_renamecol(groups, "group_id",     "key");
     table_renamecol(groups, "group_parent", "parent");
     table_renamecol(groups, "group_name",   "label");
     table_addcol   (groups, "info",         NULL);
     table_addcol   (groups, "help",         NULL);

     /* create a node list from the table, which is returned from this
      * routine. Currently, it is expected to be attached to a repository
      * node or a named repository */
     nodelist = uichoice_mknodelist_from_table(groups, "0", UI_SPLASH, 
					       UI_ICON_NET, NULL, NULL, 0, 
					       NULL, 0);

     /* patch the leaf nodes with routines that fetch dynamic hosts */
     leafnodes = itree_create();
     if (nodelist)
          itree_traverse(nodelist)
	       uichoice_findleafnodes((struct uichoice_node *) 
				      itree_get(nodelist), leafnodes);
     itree_traverse(leafnodes) {
	  /* set features on node */
	  walknode = node = itree_get(leafnodes);
	  node->dynchildren = ghchoice_tree_hostgroup_tab;

	  /* compile a fully qualified group name by walking up from
	   * the leaf and pushing the label values on a stack.
	   * Read the stack in reverse to compile the fully qualified
	   * group name. (Top node will have a parent of NULL as it
	   * does not have a parent). */
	  stackpt = 0;
	  while (walknode->parent != NULL) {
	       stack[stackpt++] = walknode->label;	/* all but top */
	       walknode = walknode->parent;
	  }
	  stack[stackpt] = walknode->label;		/* top node */

	  *(pt = purl) = '\0';
	  for (; stackpt >= 0; stackpt--) {
	       pt += strlen (strcpy (pt, stack[stackpt]) );
	       pt += strlen (strcpy (pt, ".") );
	  }
	  *(--pt) = '\0';		/* overwrite the final stop */

	  /* add group name and route address (escaped with HTTP rules as it
	   * can contain spaces) to node */
	  strcpy(purl2, "sqlrs:g=");
	  util_strencode(purl2+8, 255-8, purl);
	  uichoice_putnodearg_str(node, "group", purl);
	  uichoice_putnodearg_str(node, "grouppurl", purl2);
     }

     /* clear up */
     table_destroy(groups);
     itree_destroy(leafnodes);

     return nodelist;
#endif
}

/*
 * Refresh the selected choice and its children
 * Calls the choice refresh function on the currently selected node
 * and its children.
 */
G_MODULE_EXPORT void 
uichoice_on_refresh_choice (GtkButton *object, gpointer user_data)
{
     GtkTreeSelection *selection;
     GtkTreeIter iter;
     GtkTreeModel *model;
     GtkTreePath *path;
     GtkTreeView *choicetree;

     g_print("uichoice_on_refresh_choice\n");

     /* get selected choice item */
     choicetree = GTK_TREE_VIEW(gtk_builder_get_object(gui_builder,
						       "choice_tree"));
     selection = gtk_tree_view_get_selection(choicetree);
     if (gtk_tree_selection_get_selected (selection, &model, &iter)) {

          /* set progress total */
	  visitor_progress.nchildren = 1;
	  visitor_progress.visited = 0;

	  path = gtk_tree_model_get_path(model, &iter);
	  uichoice_refresh_choice_visitor(model, path, &iter, &visitor_progress);
	  gtk_tree_path_free(path);
     }
}


/*
 * Refresh all the nodes in the current choice tree
 * Calls the choice refresh function on the currently selected node
 * and its children
 */
G_MODULE_EXPORT void 
uichoice_on_refresh_all_choices (GtkButton *object, gpointer user_data)
{
     int nchildren;

     g_print("uichoice_on_refresh_all_choices\n");

     /* set progress total */
     nchildren = gtk_tree_model_iter_n_children (GTK_TREE_MODEL(choicestore),
						 &habparent);
     visitor_progress.nchildren = nchildren;
     visitor_progress.visited = 0;

     gtk_tree_model_foreach(GTK_TREE_MODEL(choicestore), 
			    uichoice_refresh_choice_visitor, 
			    &visitor_progress);
}


/*
 * Visitor pattern to be used to walk a tree
 * Refreshed visible dynamic nodes that are due a refresh. Does not touch data.
 */
gboolean uichoice_refresh_choice_visitor(GtkTreeModel *model, 
					 GtkTreePath *path, 
					 GtkTreeIter *iter, 
					 gpointer data)
{
     int dyntime, isdynamic, isvisible;
     time_t dyntimeout, now_t;
     /*     gpointer getdyncb;*/
     void (*getdyncb)(GtkTreeModel *, GtkTreePath *, GtkTreeIter *);
     char *name, status[100];
     struct uichoice_progress *progress;

     progress = data;
     gtk_tree_model_get(model, iter,
			UICHOICE_COL_NAME,       &name,
			UICHOICE_COL_ISVISIBLE,  &isvisible,
			UICHOICE_COL_ISDYNAMIC,  &isdynamic,
			UICHOICE_COL_DYNTIME,    &dyntime,
			UICHOICE_COL_DYNTIMEOUT, &dyntimeout,
			UICHOICE_COL_GETDYNCB,   &getdyncb,
			-1);
     now_t = time(NULL);

     g_print("Visiting -> %s vis=%d dyn=%d time=%d timeout=%ld %s callback ",
	     name, isvisible, isdynamic, dyntime, dyntimeout, 
	     getdyncb?"has":"no");

     /* log progress */
     progress->visited++;
     snprintf(status, 100, "Updating %s", name);
     uilog_setprogress(status, progress->visited / progress->nchildren, 1);

     /* call dynamic finction if set, is visible, is dynamic and is due */
     if (getdyncb && isvisible && isdynamic && dyntimeout < now_t) {
          /* call */
          g_print(" -- calling");
          (*getdyncb)(model, path, iter);
 
	  /* set next refresh time */
	  dyntimeout = dyntime + now_t;
	  gtk_tree_store_set(GTK_TREE_STORE(model), iter, 
			     UICHOICE_COL_DYNTIMEOUT, dyntimeout, 
			     NULL);
     }
     g_print("\n");

     /* clear up */
     uilog_clearprogress();
     g_free(name);

     return FALSE;	/* continue iteration */
}


/* Recurvisely call the dynamic update function on the given choice 
 * tree node */
void uichoice_refresh_node()
{
     g_print("uichoice_refresh_node\n");
}





/* set of simple list access/manipulators, several lists */
TREE * uichoice_get_myfiles_load() {return uichoice_myfiles_load; }
void   uichoice_add_myfiles_load(char *fname) {
     if ( ! tree_present(uichoice_myfiles_load, fname))
	  tree_add(uichoice_myfiles_load, xnstrdup(fname), NULL);
}
void   uichoice_add_myfiles_load_from_tree(TREE *new) {
     tree_traverse(new)
	  if ( ! tree_present(uichoice_myfiles_load, tree_getkey(new)))
	       tree_add(uichoice_myfiles_load, xnstrdup(tree_getkey(new)),
			NULL);
}
void   uichoice_rm_myfiles_load(char *fname) {
     if (tree_find(uichoice_myfiles_load, fname) != TREE_NOVAL) {
	  tree_infreemem( tree_getkey(uichoice_myfiles_load) );
	  tree_rm(uichoice_myfiles_load);
     }
}
TREE * uichoice_get_myfiles_hist() {return uichoice_myfiles_hist; }
void   uichoice_add_myfiles_hist(char *fname) {
     if ( ! tree_present(uichoice_myfiles_hist, fname))
	  tree_add(uichoice_myfiles_hist, xnstrdup(fname), NULL);
}
void   uichoice_add_myfiles_hist_from_tree(TREE *new) {
     tree_traverse(new)
	  if ( ! tree_present(uichoice_myfiles_hist, tree_getkey(new)))
	       tree_add(uichoice_myfiles_hist, xnstrdup(tree_getkey(new)),
			NULL);
}


TREE * uichoice_get_myhosts_load() {return uichoice_myhosts_load; }
void   uichoice_add_myhosts_load(char *hostname, char *purl) {
     tree_adduniqandfree(uichoice_myhosts_load, xnstrdup(hostname), 
			 xnstrdup(purl));
}
void   uichoice_add_myhosts_load_from_tree(TREE *new) {
     tree_traverse(new)
	  tree_adduniqandfree(uichoice_myhosts_load, 
			      xnstrdup(tree_getkey(new)), 
			      xnstrdup(tree_get(new)));
}
void   uichoice_rm_myhosts_load(char *hostname) {
     if (tree_find(uichoice_myhosts_load, hostname) != TREE_NOVAL) {
	  tree_infreemem( tree_getkey(uichoice_myhosts_load) );
	  tree_infreemem( tree_get(uichoice_myhosts_load) );
	  tree_rm(uichoice_myhosts_load);
     }
}
TREE * uichoice_get_myhosts_hist() {return uichoice_myhosts_hist; }
void   uichoice_add_myhosts_hist(char *hostname, char *purl) {
     tree_adduniqandfree(uichoice_myhosts_hist, xnstrdup(hostname), 
			 xnstrdup(purl));
}
void   uichoice_add_myhosts_hist_from_tree(TREE *new) {
     tree_traverse(new)
	  tree_adduniqandfree(uichoice_myhosts_hist,
			      xnstrdup(tree_getkey(new)),
			      xnstrdup(tree_get(new)));
}


/* Save the configuration of GUI elements covered by uichoice to 
 * a passed configuration list */
void uichoice_cfsave(CF_VALS cf) {
     ITREE *lst;

     /* convert myfiles load from TREE to ITREE and load */
     lst = itree_create();
     tree_traverse(uichoice_myfiles_load)
	  itree_append(lst, tree_getkey(uichoice_myfiles_load));
     if ( ! itree_empty(lst) )
	  cf_putvec(cf, UICHOICE_CF_MYFILES_LOAD, lst);
     itree_clearout(lst, NULL);	/* empty list and reuse */

     /* convert myfiles history list from TREE to ITREE and load */
     tree_traverse(uichoice_myfiles_hist)
	  itree_append(lst, tree_getkey(uichoice_myfiles_hist));
     if ( ! itree_empty(lst) )
	  cf_putvec(cf, UICHOICE_CF_MYFILES_HIST, lst);
     itree_clearout(lst, NULL);	/* empty list and reuse */

     /* convert myhosts list from TREE to ITREE and load */
     tree_traverse(uichoice_myhosts_load)
	  itree_append(lst, util_strjoin(tree_getkey(uichoice_myhosts_load),
					 "@",
					 tree_get(uichoice_myhosts_load), 
					 NULL) );
     if ( ! itree_empty(lst) )
	  cf_putvec(cf, UICHOICE_CF_MYHOSTS_LOAD, lst);
     itree_clearoutandfree(lst);	/* empty list and reuse */

     /* convert myhosts history list from TREE to ITREE and load */
     tree_traverse(uichoice_myhosts_hist)
	  itree_append(lst, util_strjoin(tree_getkey(uichoice_myhosts_hist),
					 "@",
					 tree_get(uichoice_myhosts_hist), 
					 NULL) );
     if ( ! itree_empty(lst) )
	  cf_putvec(cf, UICHOICE_CF_MYHOSTS_HIST, lst);
     itree_clearoutandfree(lst);	/* empty list and reuse */
     itree_destroy(lst);
}


/* 
 * Load the configuration into uichoice.
 * This routine loads additional components into the choice tree
 * using values or files derived from the configuration tree.
 * This adds nodes that use the dynamic and static structures set up.
 * Specifically will load the previous routes so there needs to be
 * enough nodes created to allow the file load to work.
 * Also configures and enables the repository branch.
 */
void   uichoice_configure(CF_VALS cf)
{
     ITREE *lst;
     char *host, *purl;
     char *org, *passwd, *user;

     /* load files */
     if (cf_defined(cf, UICHOICE_CF_MYFILES_LOAD)) {
          /* get the session information from the config */
          lst = cf_getvec(cf, UICHOICE_CF_MYFILES_LOAD);
	  if (lst) {
	       /* list of session files */
	       itree_traverse(lst) {
		    /* get filename & load, always remember in list */
		    uichoice_loadfile(itree_get(lst), NULL);
	       }
	  } else {
	       /* get filename & load, always remember in list */
	       uichoice_loadfile(cf_getstr(cf, UICHOICE_CF_MYFILES_LOAD), 
				 NULL); 
	       uichoice_add_myfiles_hist(
			          cf_getstr(cf, UICHOICE_CF_MYFILES_LOAD) );
	  }
     }

     /* load file history */
     if (cf_defined(cf, UICHOICE_CF_MYFILES_HIST)) {
     }

     /* load hosts */
     if (cf_defined(cf, UICHOICE_CF_MYHOSTS_LOAD)) {
          /* get the session information from the config */
          lst = cf_getvec(cf, UICHOICE_CF_MYHOSTS_LOAD);
	  if (lst) {
	       /* list of session files */
	       itree_traverse(lst) {
		    /* get string in the form of <host>@<purl> and
		     * load it into 'my hosts' in the choice tree */
		    host = xnstrdup(itree_get(lst));
		    strtok(host, "@");
		    purl = strchr(itree_get(lst), '@');
		    if (purl) {
		         purl++;
			 uichoice_loadhost(purl, host);
		    } else {
		         uichoice_loadhost(host, host);
		    }
		    nfree(host);
	       }
	  } else {
	       /* get string in the form of <host>@<purl> and
		* load it into 'my hosts' in the choice tree */
	       host = xnstrdup(cf_getstr(cf, UICHOICE_CF_MYHOSTS_LOAD));
	       strtok(host, "@");
	       purl = strchr(cf_getstr(cf,UICHOICE_CF_MYHOSTS_LOAD),'@');
	       if (purl) {
		    purl++;
		    uichoice_loadhost(purl, host);
	       } else {
		    uichoice_loadhost(host, host);
	       }
	       nfree(host);
	  }
     }

     /* load file history */
     if (cf_defined(cf, UICHOICE_CF_MYHOSTS_HIST)) {
     }

     /* set up harvest repository if enabled, but as it is dynamic 
      * it won't yet load */
     if (cf_defined(cf, RT_SQLRS_GET_URLKEY)) {

          /* address of repository is set up, see if enabled in gui */
          if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
		gtk_builder_get_object(gui_builder,"harv_enable_check")))) {

	       /* harvest is enabled, extract other data */
	       user = gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(
					gui_builder,"harv_username_entry")));
	       passwd = gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(
					gui_builder,"harv_password_entry")));
	       org = gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(
					gui_builder,"harv_org_entry")));

	       if (user && passwd && org) {
		    /* open harvest rebpository */
		    uichoice_loadrepository(cf_getstr(cf, RT_SQLRS_GET_URLKEY), 
					    org);
	       } else {
		    elog_printf(INFO, "Harvest account details have not been "
				"entered");
		    uilog_modal_alert("Harvest repository details have not "
				      "been entered",
				      "Please make sure that you have entered "
				      "your Harvest account details correctly. "
				      "Click <i>Repository</i> below (or "
				      "<i>Edit->Harvest</i> from the menu "
				      "above) to fill in your username, "
				      "password and organisation. Click on the "
				      "<b>Get Account</b> button if you do not "
				      "already have an account for Harvest");
	       }
	  }
     } else {
          elog_printf(INFO, "Repository not configured");
     }

}



/*
 * Return a list of currently loaded performance data files.
 * The keys are the filenames, values are the uichoice nodes.
 */
TREE *uichoice_getloadedfiles()
{
     return uichoice_fnames;
}




#if 0

/*
 * Builds a single level list of hosts that belong to a particular group.
 * Each host is obtained from a repository by using standard addressing 
 * from a route p-url base.
 * Each host node is set up to have their own host and basepurl node
 * arguments set.
 *
 * <parent> -+- host1
 *           +- host2
 *           +- host3
 *           +- host4
 *           +- host5
 *
 * Each node is also set up to expand into a set of choices suitable 
 * for network available statistics.
 *
 * Nodeargs in     group      leaf group name
 *                 grouppurl  route address of leaf group
 *          out    host       host name
 *                 basepurl   route address of host
 *
 * Returns the completed node tree for the group as an ITREE list, which
 * should be freed with itree_destroy().
 */
ITREE *ghchoice_tree_hostgroup_tab(TREE *nodeargs)
{
     ITREE *nodelist;
     char *group, *grouppurl, purl[256];
     TABLE hosts;
     char fullinfo[1024], *shortname=NULL;
     int infolen=0;
     struct uichoice_node *node;
     TREE *row;

     /* get args */
     group = tree_find(nodeargs, "group");
     if ( group == TREE_NOVAL )
          elog_die(ERROR, "no group node argument");
     grouppurl = tree_find(nodeargs, "grouppurl");
     if ( grouppurl == TREE_NOVAL )
          elog_die(ERROR, "no grouppurl node argument");

     /* find the hosts contined in this group */
     hosts = route_tread(grouppurl, NULL);
     if ( ! hosts ) {
          elog_printf(ERROR, "unable to read %s", grouppurl);
	  return NULL;
     }

     nodelist = itree_create();

     /* collect information from the table which will be 0 or more lines */
     table_traverse(hosts) {
          shortname="none";
	  fullinfo[0]='\0';
	  infolen=0;
	  row = table_getcurrentrow(hosts);
	  tree_traverse(row) {
	       if (strcmp(tree_getkey(row), "host name") == 0) {
		    shortname = tree_get(row);
	       } else {
		    infolen += snprintf(fullinfo+infolen, 1024-infolen, 
					"%s: %s\n", tree_getkey(row), 
					(char *) tree_get(row));
	       }
	  }
	  tree_destroy(row);

	  /* create host purl route address */
	  snprintf(purl, 256, "sqlrs:%s", shortname);

	  /* make a node from the information we have */
	  node = uichoice_mknode(shortname, fullinfo, "no help", 1, 
				 UI_SPLASH, UI_ICON_NET, hostfeatures, 
				 NULL, 0, NULL, 0, NULL);
	  
	  /* file specific node arguments */
	  node->nodeargs = tree_create();
	  tree_add(node->nodeargs, xnstrdup("basepurl"), xnstrdup(purl));
	  tree_add(node->nodeargs, xnstrdup("host"),     xnstrdup(shortname));

	  /* finally, add to nodelist to return */
	  itree_append(nodelist, node);
     }

     return nodelist;
}





#if TEST

#define TEST_FILE1 "t.uichoice.1.dat"
#define TEST_RING1 "tring1"
#define TEST_RING2 "tring2"
#define TEST_RING3 "tring3"
#define TEST_RING4 "tring4"
#define TEST_TABLE1 "ttable1"
#define TEST_TABLE2 "ttable2"
#define TEST_TABLE3 "ttable3"
#define TEST_VER1 "vobj1"
#define TEST_VER2 "vobj2"
#define TEST_VER3 "vobj3"
#define TEST_VTEXT1 "eeny meeny"
#define TEST_VTEXT2 "miny"
#define TEST_VTEXT3 "mo"
#define TEST_VAUTHOR "nigel"
#define TEST_VCMT "some text"

main(int argc, char **argv)
{
     HOLD hid;
     TS_RING tsid;
     TAB_RING tabid;
     VS vs1;
     rtinf err;
     struct uichoice_node *node1;
     ITREE *nodelist;
     int r;

     route_init("stderr", 0);
     err = route_open("stderr", NULL, NULL, 0);
     elog_init(err, 0, argv[0], NULL);
     ghchoice_init();
     hol_init(0, 0);

     /* generate a holstore test file */
     unlink(TEST_FILE1);
     hid = hol_create(TEST_FILE1, 0644);
     hol_put(hid, "toss", "bollocks", 9);
     hol_close(hid);

     /* test 1: show a tree */
     r = ghchoice_loadfile(TEST_FILE1);
     if ( r != 1 )
          elog_die(FATAL, "[1] unable to load %s, return %d", TEST_FILE1, r);
     r = ghchoice_loadfile(TEST_FILE1);
     if ( r != -2 )
          elog_die(FATAL, "[1] shouldn't load %s again, return %d", 
		   TEST_FILE1, r);
     printf("test 1:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     /* test 2: find a node */
     node1 = uichoice_findlabel(ghchoice_myfilenode, TEST_FILE1);
     if ( ! node1 )
          elog_die(FATAL, "[2] unable to find %s", TEST_FILE1);

     /* generate a timestore test file */
     tsid = ts_create(TEST_FILE1, 0644, TEST_RING1, "five slot ring", NULL, 5);
     if ( ! tsid )
          elog_die(FATAL, "[2] unable to create ring");
     ts_put(tsid, "whitley",   8);
     ts_put(tsid, "milford",   8);
     ts_put(tsid, "godalming", 10);
     ts_put(tsid, "farncombe", 10);
     ts_put(tsid, "guildford", 10);
     ts_put(tsid, "woking",    7);
     ts_put(tsid, "waterloo",  9);
     ts_close(tsid);

     /* test 3: expand the timestore branch */
     node1 = uichoice_findlabel(ghchoice_myfilenode, TEST_FILE1);
     if ( ! node1 )
          elog_die(FATAL, "[3] unable to find %s", TEST_FILE1);
     node1 = uichoice_findlabel(node1, "raw timestore");
     if ( ! node1 )
          elog_die(FATAL, "[3] unable to find %s", "raw timestore");
     nodelist = uichoice_gendynamic(node1);
     if ( ! nodelist )
          elog_die(FATAL, "[3] unable to expand");
     if (itree_n(nodelist) != 1)
          elog_die(FATAL, "[3] nodelist (%d) != 1", 
		   itree_n(nodelist));

     /* test 4: show expanded branch */
     printf("test 4:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     /* test 5: some empty timestore rings */
     tsid = ts_create(TEST_FILE1, 0644, TEST_RING2, "second five slot ring", 
		      NULL, 5);
     if ( ! tsid )
          elog_die(FATAL, "[5a] unable to create ring");
     ts_close(tsid);
     tsid = ts_create(TEST_FILE1, 0644, TEST_RING3, "third five slot ring", 
		      NULL, 5);
     if ( ! tsid )
          elog_die(FATAL, "[5b] unable to create ring");
     ts_close(tsid);
     tsid = ts_create(TEST_FILE1, 0644, TEST_RING4, "forth five slot ring", 
		      NULL, 5);
     if ( ! tsid )
          elog_die(FATAL, "[5c] unable to create ring");
     ts_close(tsid);

     /* test 6: expand the timestore branch (node1 still set up from [3] */
     nodelist = uichoice_gendynamic(node1);
     if ( ! nodelist )
          elog_die(FATAL, "[6] unable to expand");
     if (itree_n(nodelist) != 4)
          elog_die(FATAL, "[6] nodelist (%d) != 4", itree_n(nodelist));

     /* test 7: show expanded branch */
     printf("test 7:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     /* test 8: some empty tablestore rings to give us data in
      * spanstore and tablestore */
     tabid = tab_create(TEST_FILE1, 0644, TEST_TABLE1, "tom dick harry", 
			TAB_HEADINCALL, "table storage 1", NULL, 5);
     if ( ! tabid )
          elog_die(FATAL, "[8a] unable to create table ring");
     tab_put(tabid, "first second third");
     tab_close(tabid);
     tabid = tab_create(TEST_FILE1, 0644, TEST_TABLE2, "tom dick harry", 
			TAB_HEADINCALL, "table storage 2", NULL, 5);
     if ( ! tabid )
          elog_die(FATAL, "[8b] unable to create table ring");
     tab_put(tabid, "one two three");
     tab_put(tabid, "isaidone againtwo andoncemorethree");
     tab_close(tabid);
     tabid = tab_create(TEST_FILE1, 0644, TEST_TABLE3, "tom dick harry", 
			TAB_HEADINCALL, "table storage 3", NULL, 5);
     if ( ! tabid )
          elog_die(FATAL, "[8c] unable to create table ring");
     tab_put(tabid, "nipples thighs bollocks");
     tab_put(tabid, "lies damnlies andstatistics");
     tab_put(tabid, "and another thing");
     tab_close(tabid);

     /* test 9: expand the spanstore branch */
     node1 = uichoice_findlabel(uichoice_myfilenode, TEST_FILE1);
     if ( ! node1 )
          elog_die(FATAL, "[9] unable to find %s", TEST_FILE1);
     node1 = uichoice_findlabel(node1, "raw spanstore");
     if ( ! node1 )
          elog_die(FATAL, "[9] unable to find %s", "raw spanstore");
     nodelist = uichoice_gendynamic(node1);
     if ( ! nodelist )
          elog_die(FATAL, "[9] unable to expand");
     if (itree_n(nodelist) != 3)
          elog_die(FATAL, "[9] nodelist (%d) != 3", itree_n(nodelist));

     /* test 10: show expanded branch */
     printf("test 10:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     /* test 11: expand the tablestore branch */
     node1 = uichoice_findlabel(ghchoice_myfilenode, TEST_FILE1);
     if ( ! node1 )
          elog_die(FATAL, "[11] unable to find %s", TEST_FILE1);
     node1 = uichoice_findlabel(node1, "raw tablestore");
     if ( ! node1 )
          elog_die(FATAL, "[11] unable to find %s","raw tablestore");
     nodelist = uichoice_gendynamic(node1);
     if ( ! nodelist )
          elog_die(FATAL, "[11] unable to expand");
     if (itree_n(nodelist) != 3)
          elog_die(FATAL, "[11] nodelist (%d) != 3", itree_n(nodelist));

     /* test 12: show expanded branch */
     printf("test 12:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     /* test 13: add some version information */     
     vs1 = vers_create(TEST_FILE1, 0644, TEST_VER1, NULL, "test version 1");
     if ( ! vs1 )
          elog_die(FATAL, "[13a] unable to create version obj");
     r = vers_new(vs1, TEST_VTEXT1, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 0)
          elog_die(FATAL, "[13a1] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT2, 0, TEST_VAUTHOR, "");
     if (r != 1)
          elog_die(FATAL, "[13a2] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT3, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 2)
          elog_die(FATAL, "[13a3] verison number wrong");
     vers_close(vs1);
     vs1 = vers_create(TEST_FILE1, 0644, TEST_VER2, NULL, "test version 2");
     if ( ! vs1 )
          elog_die(FATAL, "[13b] unable to create version obj");
     r = vers_new(vs1, TEST_VTEXT1, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 0)
          elog_die(FATAL, "[13b1] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT2, 0, TEST_VAUTHOR, "");
     if (r != 1)
          elog_die(FATAL, "[13b2] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT3, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 2)
          elog_die(FATAL, "[13b3] verison number wrong");
     vers_close(vs1);
     vs1 = vers_create(TEST_FILE1, 0644, TEST_VER3, NULL, "test version 3");
     if ( ! vs1 )
          elog_die(FATAL, "[13c] unable to create version obj");
     r = vers_new(vs1, TEST_VTEXT1, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 0)
          elog_die(FATAL, "[13c1] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT2, 0, TEST_VAUTHOR, "");
     if (r != 1)
          elog_die(FATAL, "[13c2] verison number wrong");
     r = vers_new(vs1, TEST_VTEXT3, 0, TEST_VAUTHOR, TEST_VCMT);
     if (r != 2)
          elog_die(FATAL, "[13c3] verison number wrong");
     vers_close(vs1);

     /* test 14: expand the versionstore branch */
     node1 = uichoice_findlabel(ghchoice_myfilenode, TEST_FILE1);
     if ( ! node1 )
          elog_die(FATAL, "[14] unable to find %s", TEST_FILE1);
     node1 = uichoice_findlabel(node1, "raw versionstore");
     if ( ! node1 )
          elog_die(FATAL, "[14] unable to find %s", "raw versionstore");
     nodelist = uichoice_gendynamic(node1);
     if ( ! nodelist )
          elog_die(FATAL, "[14] unable to expand");
     if (itree_n(nodelist) != 3)
          elog_die(FATAL, "[14] nodelist (%d) != 3", 
		   itree_n(nodelist));

     /* test 15: show expanded branch */
     printf("test 15:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     ghchoice_fini();
     elog_fini();
     route_close(err);
     route_fini();
     exit(0);
}

#endif /* TEST */

#endif
