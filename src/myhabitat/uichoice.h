/*
 * GHabitat GUI choice three
 *
 * Nigel Stuckey, May 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#ifndef _UICHOICE_H_
#define _UICHOICE_H_

/* Config sybbol definitions */
#define UICHOICE_CF_MYFILES_LOAD "myfiles.load"
#define UICHOICE_CF_MYFILES_HIST "myfiles.hist"
#define UICHOICE_CF_MYHOSTS_LOAD "myhosts.load"
#define UICHOICE_CF_MYHOSTS_HIST "myhosts.hist"

/* Icon definitions */
#define UICHOICE_APPICON_HABITAT "pixmaps/habitat_flower_32.png"
#define UICHOICE_APPICON_SUBHAB  "pixmaps/habitat_flower_32.png"
#define UICHOICE_ICON_HABITAT    "pixmaps/habitat_flower_16.png"
#define UICHOICE_ICON_THISHOST   "pixmaps/home-16.png"
#define UICHOICE_ICON_MYFILES    "pixmaps/file-16.png"
#define UICHOICE_ICON_MYHOSTS    "pixmaps/server-16.png"
#define UICHOICE_ICON_REPOS      "pixmaps/replicate2-16.png"
#define UICHOICE_ICON_HARVEST    "pixmaps/harvest_flower_16.png"
#define UICHOICE_ICON_HUNTER     "pixmaps/hunter_flower_16.png"
#define UICHOICE_ICON_PEERHOST   "pixmaps/screen-16.png"
#define UICHOICE_ICON_HARVHOST   "pixmaps/server-16.png"
#define UICHOICE_ICON_REPOSHOST  "pixmaps/server-16.png"
#define UICHOICE_ICON_HUNTHOST   "pixmaps/server-16.png"
#define UICHOICE_ICON_CHART      "pixmaps/barchart-16.png"
#define UICHOICE_ICON_TEXT       "pixmaps/font-16.png"
#define UICHOICE_ICON_CSV        "pixmaps/spreadsheet-16.png"
#define UICHOICE_ICON_NET        "pixmaps/network-16.png" /* network2-32 */
#define UICHOICE_ICON_UPTIME     "pixmaps/uptime-16.png"
#define UICHOICE_ICON_PERF       "pixmaps/chart-16.png"
#define UICHOICE_ICON_EVENTS     "pixmaps/bell-16.png"
#define UICHOICE_ICON_LOGS       "pixmaps/paper+pencil-16.png"
#define UICHOICE_ICON_REP        "pixmaps/replicate2-16.png"
#define UICHOICE_ICON_JOBS       "pixmaps/clock-16.png"
#define UICHOICE_ICON_DATA       "pixmaps/file-table-16.png"
#define UICHOICE_ICON_CURVEON    "pixmaps/graph9.xpm"
#define UICHOICE_ICON_CURVEOFF   "pixmaps/graph7.xpm"
#define UICHOICE_ICON_RINGSTORE  "pixmaps/ringstore1.xpm"
#define UICHOICE_ICON_SPANSTORE  "pixmaps/spanstore1.xpm"
#define UICHOICE_ICON_TABLESTORE "pixmaps/tablestore1.xpm"

/* Big icon definitions */
#define UICHOICE_BIGICON_HABITAT    "pixmaps/habitat_flower_32.png"
#define UICHOICE_BIGICON_THISHOST   "pixmaps/home-32.png"
#define UICHOICE_BIGICON_MYFILES    "pixmaps/file-32.png"
#define UICHOICE_BIGICON_MYHOSTS    "pixmaps/server-32.png"
#define UICHOICE_BIGICON_REPOS      "pixmaps/replicate2-32.png"
#define UICHOICE_BIGICON_HARVEST    "pixmaps/harvest_flower_32.png"
#define UICHOICE_BIGICON_HUNTER     "pixmaps/hunter_flower_32.png"
#define UICHOICE_BIGICON_PEERHOST   "pixmaps/screen-32.png"
#define UICHOICE_BIGICON_HARVHOST   "pixmaps/server-32.png"
#define UICHOICE_BIGICON_REPOSHOST  "pixmaps/server-32.png"
#define UICHOICE_BIGICON_HUNTHOST   "pixmaps/server-32.png"
#define UICHOICE_BIGICON_CHART      "pixmaps/barchart-32.png"
#define UICHOICE_BIGICON_TEXT       "pixmaps/font-32.png"
#define UICHOICE_BIGICON_CSV        "pixmaps/file-csv-32.png"
#define UICHOICE_BIGICON_NET        "pixmaps/network2-128.png"
#define UICHOICE_BIGICON_UPTIME     "pixmaps/uptime-32.png"
#define UICHOICE_BIGICON_PERF       "pixmaps/chip-32.png"
#define UICHOICE_BIGICON_EVENTS     "pixmaps/bell-32.png"
#define UICHOICE_BIGICON_LOGS       "pixmaps/paper+pencil-32.png"
#define UICHOICE_BIGICON_REP        "pixmaps/replicate2-32.png"
#define UICHOICE_BIGICON_JOBS       "pixmaps/clock-32.png"
#define UICHOICE_BIGICON_DATA       "pixmaps/file-table-32.png"

/* Column definitions for choice tree */
enum {
     UICHOICE_COL_LABEL=0,	/* visible tree label with markup */
     UICHOICE_COL_TOOLTIP,	/* tree node tooltip */
     UICHOICE_COL_IMAGE,	/* tree icon */
     UICHOICE_COL_HELP,
     UICHOICE_COL_ISDYNAMIC,	/* (sub)choices need dynamic updates */
     UICHOICE_COL_DYNTIME,	/* node update frequency (secs) */
     UICHOICE_COL_DYNTIMEOUT,	/* node next update at time_t */
     UICHOICE_COL_GETDYNCB,	/* node update function */
     UICHOICE_COL_DATATIME,	/* data update frequency (secs) */
     UICHOICE_COL_DATATIMEOUT,	/* data next update at time_t */
     UICHOICE_COL_GETDATACB,	/* data returning function if not ROUTE */
     UICHOICE_COL_PURL,		/* p-url of data if ROUTE */
     UICHOICE_COL_BADGE,	/* tree badge icon */
     UICHOICE_COL_ISVISIBLE,	/* if visible in tree */
     UICHOICE_COL_FNAME,	/* filename */
     UICHOICE_COL_VISUALISE,	/* data visualisation */
     UICHOICE_COL_BIGIMAGE,	/* big icon */
     UICHOICE_COL_AVAILFROM,	/* oldest data time_t */
     UICHOICE_COL_AVAILTO,	/* youngest data time_t */
     UICHOICE_COL_NAME,		/* node name without markup */
     UICHOICE_COL_TYPE,		/* type of file (FILEROUTE_TYPE) */
     UICHOICE_COL_EOL
};

/* Column definitions for close list */
enum {
     UICHOICE_CLOSE_COL_NAME=0,
     UICHOICE_CLOSE_COL_DETAILS,
     UICHOICE_CLOSE_COL_PURL,
     UICHOICE_CLOSE_COL_TOOLTIP,
     UICHOICE_CLOSE_COL_ICON,
     UICHOICE_CLOSE_COL_FNAME,
     UICHOICE_CLOSE_COL_EOL
};

/* Functional prototypes */
void   uichoice_init();
void   uichoice_fini();
void   uichoice_init_expand();
GdkPixbuf *uichoice_load_pixbuf(char *pbname);
void   uichoice_on_config(GtkMenuItem *object, gpointer user_data);
void   uichoice_mknode_thishost_config(int show, int run);
void   uichoice_on_replication_log(GtkMenuItem *object, gpointer user_data);
void   uichoice_on_collection_log(GtkMenuItem *object, gpointer user_data);
void   uichoice_on_event_log(GtkMenuItem *object, gpointer user_data);
void   uichoice_mknode_thishost_collector(char *purl, char *label, char *name, 
					  char *tooltip, int show, int run);
void   uichoice_on_more_local(GtkCheckMenuItem *object, gpointer user_data);
void   uichoice_mknode_thishost_extra();
void   uichoice_rmnode_thishost_extra();
void   uichoice_on_file_open(GtkButton *object, gpointer user_data);
void   uichoice_loadfile(char *fname, char *format);
void   uichoice_on_host_add(GtkButton *object, gpointer user_data);
void   uichoice_loadhost(char *purl, char *label);
void   uichoice_source_close(char *fname, char *purl);
void   uichoice_on_source_close(GtkButton *object, gpointer user_data);
void   uichoice_on_source_close_by_row(GtkButton *object, gpointer user_data);
gboolean uichoice_on_popup_menu(GtkWidget *treeview, gpointer user_data);
gboolean uichoice_on_button_press(GtkWidget *treeview, GdkEventButton *event, 
				  gpointer user_data);
void   uichoice_popup_menu  (GtkWidget *treeview, GdkEventButton *event, 
			     gpointer user_data);
void   uichoice_on_source_close_by_popup(GtkButton *object, gpointer user_data);
void   uichoice_loadrepository(char *purl, char *org);
void   uichoice_on_mknode_repos_level(GtkTreeModel *model, 
				      GtkTreePath *path, 
				      GtkTreeIter *iter);
void   uichoice_mknode_repos_level(GtkTreeIter *parent_node, 
				   char *level_name);
void   uichoice_on_refresh_choice (GtkButton *object, gpointer user_data);
void   uichoice_on_refresh_all_choices (GtkButton *object, gpointer user_data);
gboolean uichoice_refresh_choice_visitor(GtkTreeModel *model,GtkTreePath *path, 
					 GtkTreeIter *iter, gpointer data);
void   uichoice_refresh_node();


TREE * uichoice_get_myfiles_load();
void   uichoice_add_myfiles_load(char *fname);
void   uichoice_add_myfiles_load_from_tree(TREE *new);
void   uichoice_rm_myfiles_load(char *fname);
TREE * uichoice_get_myfiles_hist();
void   uichoice_add_myfiles_hist(char *fname);
void   uichoice_add_myfiles_hist_from_tree(TREE *new);
TREE * uichoice_get_myhosts_load();
void   uichoice_add_myhosts_load(char *hostname, char *purl);
void   uichoice_add_myhosts_load_from_tree(TREE *new);
void   uichoice_rm_myhosts_load(char *hostname);
TREE * uichoice_get_myhosts_hist();
void   uichoice_add_myhosts_hist(char *hostname, char *purl);
void   uichoice_add_myhosts_hist_from_tree(TREE *new);
void   uichoice_cfsave(CF_VALS cf);
void   uichoice_configure(CF_VALS cf);
TREE * uichoice_getloadedfiles();

#endif /* _UICHOICE_H_ */
