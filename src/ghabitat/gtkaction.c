/*
 * Habitat Gtk GUI implementation
 *
 * Nigel Stuckey, May 1999-Aug 2000
 * Copyright System Garden Limited 1999-2004. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include "gtkaction.h"
#if GTKACTION_SHEET4PICK
#include <gtkextra/gtkextra.h>
#endif
#include "support.h"
#include "interface.h"
#include "main.h"
#include "uichoice.h"
#include "uidata.h"
#include "gmcgraph.h"
#include "misc.h"
#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"
#include "../iiab/iiab.h"
#include "../iiab/util.h"
#include "../iiab/table.h"

/* presentation structure, for the right hand side of the screen */
enum uidata_type datapres_type;		/* the currently displayed type of 
					 * data presentation widget */
GtkWidget *   datapres_widget;		/* view specific widget */
GtkWidget *   datapres_widget2;		/* 2nd view specific widget */
struct uichoice_node *datapres_node;	/* the uichoice node that is being
					 * displayed */
TREE *        datapres_nodeargs;	/* node arguments for current data */
RESDAT        datapres_data;		/* data for those display widgets
					 * that need to keep them around */
TREE *        gtkaction_graphsel;	/* graph names selected by user from
					 * the graph pick list; contains 
					 * historic names also; list in keys */
TREE *        gtkaction_inst;		/* current set of instances (in keys)
					 * that can potentially be selected */
char *        gtkaction_keycol;		/* current key column from data */
TREE *        gtkaction_curvesel;	/* list of curve names selected from
					 * the graph pick list; list in keys */
int           gtkaction_prestimer=-1;	/* handle to the update timer */

/* static gui deatils */
gint          gtkaction_progresstimer;

FILE *        clockwork_fstream;	/* clockwork read stream if running,
					 * NULL if not */
toggle_state *picklist_button;		/* current picklist button states */
int           picklist_nbuttons;	/* number of buttons */
GtkWidget *   picklist_hdscale=NULL;	/* scale table heading widget */
GtkWidget *   picklist_hdoffset=NULL;	/* offset table heading widget */

/* popup log details */
enum elog_severity logpopup_severity=NOELOG;
int                logpopup_coloured=0;
GtkWidget *        logpopup_table=NULL;

/* icon declarations */
#include "../pixmaps/holstore1.xpm"
#include "../pixmaps/tablestore1.xpm"
#include "../pixmaps/spanstore1.xpm"
#include "../pixmaps/ringstore1.xpm"
#include "../pixmaps/versionstore2.xpm"
#include "../pixmaps/graph7.xpm"
#include "../pixmaps/graph9.xpm"
#include "../pixmaps/graph10.xpm"
#include "../pixmaps/error1.xpm"
#include "../pixmaps/filedata2.xpm"
#include "../pixmaps/homedata3.xpm"
#include "../pixmaps/netdata4.xpm"
#include "../pixmaps/habitat_flower_16.xpm"
#include "../pixmaps/sysgarlogo4_t.xpm"
#include "../pixmaps/sysgarlogo4_xt.xpm"
#include "../pixmaps/uptime1.xpm"
#include "../pixmaps/bottleneck2.xpm"
#include "../pixmaps/quality1.xpm"
#include "../pixmaps/trend1.xpm"
#include "../pixmaps/raw1.xpm"
#include "../pixmaps/logs1.xpm"
#include "../pixmaps/route2.xpm"
#include "../pixmaps/jobs1.xpm"
#include "../pixmaps/watch1.xpm"
#include "../pixmaps/event1.xpm"
#include "../pixmaps/morewidget1.xpm"
#include "../pixmaps/lesswidget1.xpm"
#include "../pixmaps/cpu1.xpm"
#include "../pixmaps/csv1.xpm"
#include "../pixmaps/disk1.xpm"
#include "../pixmaps/net1.xpm"
#include "../pixmaps/rep4.xpm"
GdkPixmap *icon_holstore;
GdkBitmap *mask_holstore;
GdkPixmap *icon_ringstore;
GdkBitmap *mask_ringstore;
GdkPixmap *icon_spanstore;
GdkBitmap *mask_spanstore;
GdkPixmap *icon_tablestore;
GdkBitmap *mask_tablestore;
GdkPixmap *icon_versionstore;
GdkBitmap *mask_versionstore;
GdkPixmap *icon_graph;
GdkBitmap *mask_graph;
GdkPixmap *icon_graphon;
GdkBitmap *mask_graphon;
GdkPixmap *icon_graphoff;
GdkBitmap *mask_graphoff;
GdkPixmap *icon_error;
GdkBitmap *mask_error;
GdkPixmap *icon_filedata;
GdkBitmap *mask_filedata;
GdkPixmap *icon_homedata;
GdkBitmap *mask_homedata;
GdkPixmap *icon_netdata;
GdkBitmap *mask_netdata;
GdkPixmap *icon_sysgarlogo;
GdkBitmap *mask_sysgarlogo;
GdkPixmap *icon_sysgarwm;
GdkBitmap *mask_sysgarwm;
GdkPixmap *icon_uptime;
GdkBitmap *mask_uptime;
GdkPixmap *icon_bottleneck;
GdkBitmap *mask_bottleneck;
GdkPixmap *icon_quality;
GdkBitmap *mask_quality;
GdkPixmap *icon_trend;
GdkBitmap *mask_trend;
GdkPixmap *icon_raw;
GdkBitmap *mask_raw;
GdkPixmap *icon_logs;
GdkBitmap *mask_logs;
GdkPixmap *icon_route;
GdkBitmap *mask_route;
GdkPixmap *icon_jobs;
GdkBitmap *mask_jobs;
GdkPixmap *icon_watch;
GdkBitmap *mask_watch;
GdkPixmap *icon_event;
GdkBitmap *mask_event;
GdkPixmap *icon_lesswidget;
GdkBitmap *mask_lesswidget;
GdkPixmap *icon_morewidget;
GdkBitmap *mask_morewidget;
GdkPixmap *icon_cpu;
GdkBitmap *mask_cpu;
GdkPixmap *icon_csv;
GdkBitmap *mask_csv;
GdkPixmap *icon_disk;
GdkBitmap *mask_disk;
GdkPixmap *icon_net;
GdkBitmap *mask_net;
GdkPixmap *icon_rep;
GdkBitmap *mask_rep;

/* message context */
guint gtkaction_elogmsgid;	/* for elogs and messages */

gchar *gtkaction_graphattr_headers[] = { "name     ", "scale", "offset"};
#define GTKACTION_GRAPHATTR_NHEADERS 3

/* browser list */
char *gtkaction_browsers[] = {"mozilla", "konqueror", "netscape", "opera", 
			      "safari", "chimera",  "chimera2",  NULL};

/* Colours for the popup list */
char *logpopup_bgcolname[] = {"black", 		/* fatal */
			      "red",	 	/* error */
			      "yellow", 	/* warning */
			      "Gold",		/* info */
			      "LightGoldenRod", /* diag */
			      "PaleGoldenRod" 	/* debug */ };
char *logpopup_fgcolname[] = {"white", 		/* fatal */
			      "white",	 	/* error */
			      "black",	 	/* warning */
			      "black", 		/* info */
			      "black", 		/* diag */
			      "black" 		/* debug */ };
GdkColor logpopup_bgcolour[6];
GdkColor logpopup_fgcolour[6];

void gtkaction_init()
{
     /* default to splash */
     datapres_type     = UI_NONE;
     datapres_widget   = splash_view;
     datapres_widget2  = NULL;
     datapres_node     = NULL;
     datapres_nodeargs = tree_create();
     datapres_data.t   = TRES_NONE;

     /* create icons */
     gtkaction_createicons();

     /* initialise graph and curve list */
     gtkaction_graphsel = tree_create();
     gtkaction_curvesel = tree_create();

     /* message bar */
     gtkaction_elogmsgid = gtk_statusbar_get_context_id (
	  GTK_STATUSBAR(messagebar), "iiab");

     /* status bar */
     gtkaction_progresstimer = -1;

     /* popup window colours */
     gtkaction_log_popup_init();

     /* clockwork file status */
     clockwork_fstream = NULL;

     /* root window icon */
     gtkaction_setwmicon(baseWindow->window, icon_sysgarwm, mask_sysgarwm);
}

void gtkaction_fini()
{
     /*gtkaction_choice_deselect();*/ /* disabled temporarily due to a bug */
}


/* 
 * Load the configuration into gtkaction.
 * Specificallly will initialise the default curve selections
 */
void gtkaction_configure(CF_VALS cf)
{
     ITREE *lst;

     /* curve choice */
     if (cf_defined(cf, GTKACTION_CF_CURVES)) {
	  /* get the session information from the config */
	  lst = cf_getvec(cf, GTKACTION_CF_CURVES);
	  if (lst) {
	       /* list of choices */
	       itree_traverse(lst)
		    /* add each curve into the selection list */
		    tree_add(gtkaction_curvesel, itree_get(lst), NULL);
	  } else {
	       tree_add(gtkaction_curvesel, 
			cf_getstr(cf, GTKACTION_CF_CURVES), NULL);
	  }
     }
}


/* creates the icons used in the gtkaction application */
void gtkaction_createicons()
{
     GtkStyle *style;

     /* icons */
     style = gtk_widget_get_style(baseWindow);
     icon_holstore = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_holstore, &style->bg[GTK_STATE_NORMAL], holstore1);
     icon_ringstore = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_ringstore, &style->bg[GTK_STATE_NORMAL], ringstore1);
     icon_spanstore = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_spanstore, &style->bg[GTK_STATE_NORMAL], spanstore1);
     icon_tablestore = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_tablestore, &style->bg[GTK_STATE_NORMAL], tablestore1);
     icon_versionstore = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_versionstore,&style->bg[GTK_STATE_NORMAL],versionstore2);
     icon_graph = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_graph,&style->bg[GTK_STATE_NORMAL],graph10);
     icon_graphon = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_graphon,&style->bg[GTK_STATE_NORMAL],graph9);
     icon_graphoff = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_graphoff,&style->bg[GTK_STATE_NORMAL],graph7);
     icon_error = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_error,&style->bg[GTK_STATE_NORMAL],error1);
     icon_filedata = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_filedata,&style->bg[GTK_STATE_NORMAL],filedata2);
     icon_homedata = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_homedata,&style->bg[GTK_STATE_NORMAL],homedata3);
     icon_netdata = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_netdata,&style->bg[GTK_STATE_NORMAL],netdata4);
     icon_sysgarlogo = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_sysgarlogo,&style->bg[GTK_STATE_NORMAL],habitat_flower_16);
     icon_sysgarwm = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_sysgarwm,&style->bg[GTK_STATE_NORMAL],habitat_flower_16);
     icon_uptime = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_uptime,&style->bg[GTK_STATE_NORMAL],uptime1);
     icon_bottleneck = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_bottleneck,&style->bg[GTK_STATE_NORMAL],bottleneck2);
     icon_quality = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_quality,&style->bg[GTK_STATE_NORMAL],quality1);
     icon_trend = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_trend,&style->bg[GTK_STATE_NORMAL],trend1);
     icon_raw = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_raw,&style->bg[GTK_STATE_NORMAL],raw1);
     icon_logs = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_logs,&style->bg[GTK_STATE_NORMAL],logs1);
     icon_route = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_route,&style->bg[GTK_STATE_NORMAL],route2);
     icon_jobs = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_jobs,&style->bg[GTK_STATE_NORMAL],jobs1);
     icon_watch = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_watch,&style->bg[GTK_STATE_NORMAL],watch1);
     icon_event = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_event,&style->bg[GTK_STATE_NORMAL],event1);
     icon_morewidget = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_morewidget,&style->bg[GTK_STATE_NORMAL],morewidget1);
     icon_lesswidget = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_lesswidget,&style->bg[GTK_STATE_NORMAL],lesswidget1);
     icon_cpu = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_cpu,&style->bg[GTK_STATE_NORMAL],cpu1);
     icon_csv = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_csv,&style->bg[GTK_STATE_NORMAL],csv1);
     icon_disk = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_disk,&style->bg[GTK_STATE_NORMAL],disk1);
     icon_net = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_net,&style->bg[GTK_STATE_NORMAL],net1);
     icon_rep = gdk_pixmap_create_from_xpm_d(baseWindow->window,
		&mask_rep,&style->bg[GTK_STATE_NORMAL],rep4);
}

/* Sets the WM icon for the given toplevel window */
void gtkaction_setwmicon(GdkWindow *w, GdkPixmap *pixmap, GdkBitmap *mask)
{
        GdkWindowAttr attributes;
        gint attributes_mask;
        gint width, height;
        GdkWindow *icon_window, *parent;  

        gdk_window_get_size(pixmap, &width, &height);
	/*gtk_widget_realize(w);*/

        /* Get parent for icon window. */
        parent = gdk_window_get_parent(w);
        if (parent == NULL)
            return;

        /* Create icon window. */
        attributes.width = width;
        attributes.height = height;
        attributes.wclass = GDK_INPUT_OUTPUT;
        attributes.window_type = GDK_WINDOW_TOPLEVEL;
        attributes.wmclass_name = "GHabitat";
        attributes.wmclass_class = "GHabitat";
        attributes.override_redirect = FALSE;
        attributes_mask = GDK_WA_WMCLASS | GDK_WA_NOREDIR;
        icon_window = gdk_window_new(parent, &attributes, attributes_mask);
        if (icon_window == NULL)
            return;

        /* Set icon. */
        gdk_window_set_icon(w, icon_window, pixmap, mask);

        return;
}



/* Create a GtkCTree node based on data from the uichoice_node structure
 * and attach it to a parent. If there is a possibility of children, a 
 * sub tree is created for future descendents, but no child nodes are 
 * created nor uichoice_node expanded.
 * Sets bi-directional references between uichoice_node and GtkCTree.
 * Returns the address of the created node or NULL for error.
 */
GtkCTreeNode *
gtkaction_makechoice(GtkCTreeNode *parent,       /* gui parent */
		     struct uichoice_node *node, /* choice struct */
		     GtkTooltips *tip	         /* Tooltip widget */ )
{
     GtkCTreeNode *treeitem;
     GdkPixmap *pixmap;
     GdkBitmap *mask;
     static GtkStyle *edstyle=NULL, *dystyle=NULL;
     GdkColor red, blue;

     /* initialisation */
     if ( ! edstyle ) {
	  red.red   = 65535;
	  red.green = 0;
	  red.blue  = 0;
	  blue.red   = 0;
	  blue.green = 0;
	  blue.blue  = 65535;

	  edstyle = gtk_style_new ();
	  edstyle->fg[GTK_STATE_NORMAL] = red;

	  dystyle = gtk_style_new ();
	  dystyle->fg[GTK_STATE_NORMAL] = blue;
     }

     /* load the node icon */
     pixmap = NULL;
     mask = NULL;
     switch (node->icon) {
     case UI_ICON_HOL:
	  pixmap = icon_holstore;
	  mask   = mask_holstore;
	  break;
     case UI_ICON_RING:
	  pixmap = icon_ringstore;
	  mask   = mask_ringstore;
	  break;
     case UI_ICON_SPAN:
	  pixmap = icon_spanstore;
	  mask   = mask_spanstore;
	  break;
     case UI_ICON_TABLE:
	  pixmap = icon_tablestore;
	  mask   = mask_tablestore;
	  break;
     case UI_ICON_VERSION:
	  pixmap = icon_versionstore;
	  mask   = mask_versionstore;
	  break;
     case UI_ICON_GRAPH:
	  pixmap = icon_graph;
	  mask   = mask_graph;
	  break;
     case UI_ICON_ERROR:
	  pixmap = icon_error;
	  mask   = mask_error;
	  break;
     case UI_ICON_HOME:
	  pixmap = icon_homedata;
	  mask   = mask_homedata;
	  break;
     case UI_ICON_FILE:
	  pixmap = icon_filedata;
	  mask   = mask_filedata;
	  break;
     case UI_ICON_NET:
	  pixmap = icon_netdata;
	  mask   = mask_netdata;
	  break;
     case UI_ICON_SYSGAR:
	  pixmap = icon_sysgarlogo;
	  mask   = mask_sysgarlogo;
	  break;
     case UI_ICON_UPTIME:
	  pixmap = icon_uptime;
	  mask   = mask_uptime;
	  break;
     case UI_ICON_BNECK:
	  pixmap = icon_bottleneck;
	  mask   = mask_bottleneck;
	  break;
     case UI_ICON_QUALITY:
	  pixmap = icon_quality;
	  mask   = mask_quality;
	  break;
     case UI_ICON_TREND:
	  pixmap = icon_trend;
	  mask   = mask_trend;
	  break;
     case UI_ICON_RAW:
	  pixmap = icon_raw;
	  mask   = mask_raw;
	  break;
     case UI_ICON_LOG:
	  pixmap = icon_logs;
	  mask   = mask_logs;
	  break;
     case UI_ICON_ROUTE:
	  pixmap = icon_route;
	  mask   = mask_route;
	  break;
     case UI_ICON_JOB:
	  pixmap = icon_jobs;
	  mask   = mask_jobs;
	  break;
     case UI_ICON_WATCH:
	  pixmap = icon_watch;
	  mask   = mask_watch;
	  break;
     case UI_ICON_EVENT:
	  pixmap = icon_event;
	  mask   = mask_event;
	  break;
     case UI_ICON_CPU:
	  pixmap = icon_cpu;
	  mask   = mask_cpu;
	  break;
     case UI_ICON_CSV:
	  pixmap = icon_csv;
	  mask   = mask_csv;
	  break;
     case UI_ICON_DISK:
	  pixmap = icon_disk;
	  mask   = mask_disk;
	  break;
     case UI_ICON_NETPERF:
	  pixmap = icon_net;
	  mask   = mask_net;
	  break;
     case UI_ICON_REP:
	  pixmap = icon_rep;
	  mask   = mask_rep;
	  break;
     case UI_ICON_SERVICE:
	  pixmap = icon_quality;
	  mask   = mask_quality;
	  break;
     case UI_ICON_NONE:
     default:
	  break;
     }

     /* create a parent node */
     treeitem = gtk_ctree_insert_node(GTK_CTREE(tree), parent, NULL, 
				      &node->label, 3, pixmap, mask, 
				      pixmap, mask, FALSE, FALSE);

     /*
      * We can't tell for certain that there will be children as we are
      * not necessarily expanded (for performance reasons).
      * The best we can do is to check the features, dynamic children for
      * potential and the children and dyncache for actual data.
      * If there is the potential, there should be an expander at this
      * node to show the potential that it is a perent.
      */
     if (node->features || node->dynchildren || 
	 (node->children && itree_n(node->children) > 0) || 
	 (node->dyncache && itree_n(node->dyncache) > 0) ) {

	  /* create child to give the [+] expander */
	  gtk_ctree_insert_node(GTK_CTREE(tree), treeitem, NULL, NULL, 0, 
				NULL, NULL, NULL, NULL, TRUE, FALSE);
     }

     if (treeitem == NULL) {
	  elog_printf(ERROR, "unable to create gui node");
	  return NULL;
     }

     /* tag the node so different functionality has different styles */
     if (node->is_editable) {
	  gtk_ctree_node_set_row_style(GTK_CTREE(tree), treeitem, edstyle);
     } else if (node->is_dynamic) {
	  gtk_ctree_node_set_row_style(GTK_CTREE(tree), treeitem, dystyle);
     }

     /* add bi-directional referencies between uichoice and gui nodes */
     gtk_ctree_node_set_row_data(GTK_CTREE(tree), treeitem, (gpointer) node);
     uichoice_putnodearg_mem(node, GTKACTION_GUIITEMKEY, &treeitem, 
			     sizeof(GtkCTreeNode *));

#if 0
     /* add tooltips by digging into their defintions and attaching
      * to the internal button */
     if (node->info)
       gtk_tooltips_set_tip(tip, GTK_CLIST_ROW(tree)->row->cell->u.widget, node->info, NULL);
#endif

#if 0
     if (! node->enabled)
	  gtk_widget_set_sensitive(box, FALSE);

     if (node->parent) {
	  /* parents should be sensitive (not greyed out).
	   * I would check this with the GUI, but can't find the API call.
	   * Yes, I did try ...get_sensitive() */
	  parentitem = GTK_CTREE_ITEM(*(GtkTreeItem **) uichoice_getnodearg
				     (node->parent, GTKACTION_GUIITEMKEY) );
	  /*if ( ! GTK_WIDGET_IS_SENSITIVE(parentitem))*/
	       if (GTK_IS_BOX(GTK_BIN (parentitem)->child))
		    gtk_widget_set_sensitive(GTK_BIN (parentitem)->child, 
					     TRUE);
     }
#endif


     if (! node->enabled)
	  gtk_ctree_node_set_selectable(GTK_CTREE(tree), treeitem, FALSE);

     return treeitem;
}


/* 
 * Recursively delete the node `treenode' and remove data and referencies to
 * uichoice nodes that are contained within each gui node. The allocation
 * of the uichoice is not altered by this, so that must be taken care of 
 * by other functions.
 */
void gtkaction_deletechoice(GtkCTree *tree, GtkCTreeNode *treeitem)
{
     GtkCTreeNode *guic;

     for (guic = GTK_CTREE_ROW(treeitem)->children; guic; 
	  guic = GTK_CTREE_ROW(guic)->sibling) {

	  gtkaction_deletechoice(tree, guic);
     }

     gtk_ctree_remove_node(tree, treeitem);
}


#if 0
/* Add a gui tree node to the parent node */
void gtkaction_parentchoice(GtkCTree *parent, GtkWidget *treenode)
{
     if (treenode == NULL) {
	  elog_printf(ERROR, "treenode is null");
	  return;
     }
     if ( ! GTK_IS_CTREE_ITEM(treenode) ) {
	  elog_printf(ERROR, "treenode is not GTK_CTREE_ITEM");
	  return;
     }

}
#endif



/* Given a GtkCTreeNode within tree, recursively expand its items brethd 
 * first by several layers, creating or refereshing nodes to suit.
 * GtkCTreeNode must exist and its descendents have uichoice_node 
 * counterparts refered to in their data area. 
 */
void gtkaction_expandlist(GtkCTreeNode *treeitem, int nlayers, 
			  GtkTooltips *tip)
{
     GtkCTreeNode *guic;

     for (guic = GTK_CTREE_ROW(treeitem)->children; guic; 
	  guic = GTK_CTREE_ROW(guic)->sibling) {

	  gtkaction_expandchoice(guic, nlayers, tip);
     }

#if 0
     GList *children;

     /* check parameters */
     if (nlayers < 1)
          return;

     /* GtkCTrees don't directly hold the nodes we are intrested in, 
      * we have to desend to the tree items for that */
     children = gtk_container_children(GTK_CONTAINER(tree));
     while (children) {
	  gtkaction_expandchoice( GTK_CTREE_ITEM(children->data), nlayers, tip);
	  children = g_list_remove_link(children, children);
     }
#endif
}


/*
 * Expand or update a tree item on the screen by visiting the corresponding 
 * uichoice_node structure, expanding if necessary and creating or updating
 * the children that will become displayed.
 * A difference is carried out between the uichoice children and the
 * children of this gui node, new ones are added, missing ones removed.
 * This takes care of the dummy entry placed in the node to make it a parent
 * and make the little [+] appear.
 * The corresponding uichoice_node data node must exist as data to the
 * treeitem.
 */
void gtkaction_expandchoice(GtkCTreeNode *treeitem, int nlayers, 
			    GtkTooltips *tip)
{
     struct uichoice_node *parent, *child;
     ITREE *child_nodes, *current_children, *remove_children;
     GtkCTreeNode *guic;
     struct uichoice_node *testuic;

     /* check parameters */
     if (nlayers < 1)
          return;

     /* get uichoice_node reference from gui GtkCTree node */
     parent = gtk_ctree_node_get_row_data(GTK_CTREE(tree), treeitem);
     if (parent == NULL) {
	  /*elog_printf(ERROR, "unable to get uichoice node");*/
	  return;
     }

     /* expand uichoice_node (if not already) and update the dynamic
      * children if not already done by the expand node */
     uichoice_expandnode(parent);
     /*uichoice_updatedynamic(parent);*/
     current_children = itree_create();

     gtk_clist_freeze (GTK_CLIST(tree));

     /* draw new static children */
     child_nodes = parent->children;
     if (child_nodes) {
	  itree_traverse(child_nodes) {
	       child = itree_get(child_nodes);
	       itree_add(current_children, (int)child, NULL);

	       if (uichoice_getnodearg (child, GTKACTION_GUIITEMKEY) != NULL)
		    continue;	/* child already drawn */

	       gtkaction_makechoice(treeitem, child, tip);
	  }
     }
     /* draw new dynamic children */
     child_nodes = parent->dyncache;
     if (child_nodes) {
	  itree_traverse(child_nodes) {
	       child = itree_get(child_nodes);
	       itree_add(current_children, (int)child, NULL);

	       if (uichoice_getnodearg (child, GTKACTION_GUIITEMKEY) != NULL)
		    continue;	/* child already drawn */

	       gtkaction_makechoice(treeitem, child, tip);
	  }
     }

     /* collect child gui nodes no longer children in the uichoice node */
     remove_children = itree_create();
     for (guic = GTK_CTREE_ROW(treeitem)->children; guic; 
	  guic = GTK_CTREE_ROW(guic)->sibling) {

	  testuic = gtk_ctree_node_get_row_data(GTK_CTREE(tree), guic);
	  if (testuic == NULL || 
	      itree_find(current_children, (int) testuic) == ITREE_NOVAL) {

	       itree_append(remove_children, guic);
	  }
     }

     /* remove marked children */
     itree_traverse(remove_children)
	  gtk_ctree_remove_node(GTK_CTREE(tree), 
				GTK_CTREE_NODE(itree_get(remove_children)));

     gtk_clist_thaw (GTK_CLIST(tree));


     itree_destroy(current_children);
     itree_destroy(remove_children);
#if 0

#if 0
     /* make sure the gui node is expanded, but dont cause a loop so
      * block the handler for the duration of the explicit call */
     gtk_signal_handler_block_by_func(GTK_OBJECT(treeitem),
				      GTK_SIGNAL_FUNC(gtkaction_choice_select),
				      parent);
     gtk_ctree_item_expand(treeitem);
     gtk_signal_handler_unblock_by_func(GTK_OBJECT(treeitem),
				      GTK_SIGNAL_FUNC(gtkaction_choice_select),
				      parent);
#endif

     /* descend if more layers are needed */
     if (nlayers > 1)
	  gtkaction_expandlist(treeitem, nlayers-1, tip);
#endif
}

void gtkaction_contractchoice(GtkCTreeNode *treeitem)
{
}


/*
 * Attempt to display the data given by a uichoice node.
 * The choice node may not have a gui tree node associated with it, so
 * the node is recursed upwards & down again to fill in the tree icons.
 * Then the data for that node is presented.
 * Level should always be set to 0 on invocation, this will be
 * incremented during recursion.
 */
void gtkaction_gotochoice(struct uichoice_node *node, int level)
{
     GtkCTreeNode *treeitem;

     if (node == NULL)
	  return;

     /* recurse upwards until there is a uichoice node that 
      * has been correctly displayed in the gui tree */
     if (uichoice_getnodearg(node, GTKACTION_GUIITEMKEY) == NULL)
	  gtkaction_gotochoice(node->parent, level+1);

     /* we should now have a valid gui tree node, display our children */
     treeitem = *(GtkCTreeNode **) uichoice_getnodearg(node, 
						       GTKACTION_GUIITEMKEY);
     if (treeitem == NULL)
	  elog_printf(ERROR, "can't find valid gui tree item");
     else
	  gtkaction_expandchoice(treeitem, 1, tooltips);

     if (level == 0)
	  gtk_ctree_select(GTK_CTREE(tree), treeitem);
}


/*
 * Synchronise the descendents of node labeled 'nodelabel' from the choice 
 * tree with that in uichoice. The uichoice node and associated gui node
 * should be in existance before calling.
 */
void gtkaction_choice_sync(GtkCTree *tree, char *nodelabel)
{
     struct uichoice_node *mynode;
     GtkCTreeNode *treeitem;

     /* The gui catches up with uichoice.
      * First, find the named node, see if it has children to bother 
      * with and its corresponding gui tree */
     mynode = uichoice_findlabel_all(nodelabel);
     if ( !mynode )
	  return;
     if ( (mynode->children == NULL || itree_empty(mynode->children)) &&
	  (mynode->dynchildren == NULL) &&
	  (mynode->dyncache == NULL || itree_empty(mynode->dyncache)) )
	  return;
     treeitem = *(GtkCTreeNode **) uichoice_getnodearg(mynode, 
						       GTKACTION_GUIITEMKEY);

     /* update the named gui node */
     gtkaction_expandchoice(treeitem, 1, tooltips);
}



/* Callback when a tree item is selected */
void gtkaction_choice_select(GtkCTreeNode *treeitem, gpointer user_data)
{
     struct uichoice_node *node;
     char progress[64], *path, *shortpath;

     /* expand the node */
     /*gtkaction_expandchoice(GTK_CTREE_NODE(treeitem), 1, tooltips);*/

     /* check node has changed */
     node = (struct uichoice_node *) user_data;
     if (node == NULL) {
	  elog_printf(ERROR, "NULL choice node");
	  return;
     }
     /* g_print("choice selected, node label %s\n", node->label); */
     if (node == datapres_node) {
          /*elog_print(ERROR, "node has not changed\n");*/
	  return;
     }
     if (datapres_type == UI_SPLASH && node->presentation == UI_SPLASH) {
	  /*elog_print(ERROR, "node changed but still splash\n");*/
	  return;
     }

     /* The presentation of data in ghabitat is done by switching frames
      * of data from specialist objects within a single presentation box.
      * This achieves speed, simplicity and better user appearence. 
      * The frame widgets are:-
      *    graphframe    The timeseries graph
      *    splash_view   Splash screen
      *    tableframe    Table widget
      *    edtreeframe   Editable table information
      * datapres_type and datapres_widget (and widget2) are set to refer 
      * to the information being displayed.
      */

     /* hide previous presentation, stop updates and change the 
      * mouse pointer to a clock/watch etc */
     gtkaction_setprogress("clearing up", 0.0, 0);
     gtkaction_choice_update_stop();
     gdk_window_set_cursor(baseWindow->window, mouse_pointer_wait);

     gtkaction_choice_deselect();

     /* instatiate new wigets */
     switch (node->presentation) {
     case UI_HELP:	/* help screen */
          /*g_print("presentation: help\n");*/
	  break;
     case UI_NONE:	/* no interface (use splash anyway)*/
     case UI_SPLASH:	/* splash graphic */
	  gtk_widget_show(splash_view);
	  datapres_widget = splash_view;
	  gtkaction_clearprogress();
	  break;
     case UI_TABLE:	/* table or grid interface */
	  gtkaction_setprogress("collecting", 0.33, 0);
	  uichoice_getinheritedargs(node, datapres_nodeargs);
	  gtkaction_setprogress(NULL, 0.5, 0);
	  datapres_data = node->getdata(datapres_nodeargs);
	  node->datatime = time(NULL);
	  gtkaction_setprogress("writing", 0.66, 0);
	  datapres_widget = gtkaction_mktable(datapres_data);
	  gtk_container_add (GTK_CONTAINER (tablescroll), datapres_widget);
	  gtk_widget_show(datapres_widget);

	  /* set the label with the choice node path */
	  path = uichoice_nodepath(node, " - ");
	  shortpath = strstr(path, "-");
	  shortpath = (shortpath) ? shortpath+2 : path;
	  gtk_frame_set_label (GTK_FRAME(tableframe), shortpath);
	  nfree(path);

	  if (GTK_CLIST(datapres_widget)->rows == 0 ||
	      GTK_CLIST(datapres_widget)->columns == 0) {
	       gtkaction_setprogress("table is empty", 0.8, 0);
	  } else {
	       sprintf(progress, "%d row%s %d column%s", 
		       GTK_CLIST(datapres_widget)->rows, 
		       GTK_CLIST(datapres_widget)->rows==1?"":"s", 
		       GTK_CLIST(datapres_widget)->columns, 
		       GTK_CLIST(datapres_widget)->columns==1?"":"s");
	       gtkaction_setprogress(progress, 0.8, 0);
	  }
	  gtk_widget_show(tableframe);
	  gtk_widget_set_sensitive(save_viewed_data, TRUE);
	  gtk_widget_set_sensitive(send_data_to_app, TRUE);
	  gtk_widget_set_sensitive(send_data_to_email, TRUE);
	  gtkaction_setprogress(NULL, 0, 0);
	  break;
     case UI_EDTABLE:	/* editable table or grid interface */
          /*g_print("presentation: edtable\n");*/
	  break;
     case UI_FORM:	/* form interface: prompt text and value */
          /*g_print("presentation: form\n");*/
	  break;
     case UI_EDFORM:	/* editable form interface: prompt text and value */
          /*g_print("presentation: edform\n");*/
	  break;
     case UI_TEXT:	/* text interface */
          /*g_print("presentation: text\n");*/
	  break;
     case UI_EDTEXT:	/* editable text interface */
          /*g_print("presentation: edtext\n");*/
	  break;
     case UI_EDTREE:	/* editable table using a tree interface */
          /*g_print("presentation: edtree\n");*/
	  gtkaction_setprogress("collecting", 0.33, 0);
	  uichoice_getinheritedargs(node, datapres_nodeargs);
	  datapres_data = node->getdata(datapres_nodeargs);
	  node->datatime = time(NULL);
	  gtkaction_setprogress("writing", 0.66, 0);
	  gtkaction_mkedtree(datapres_data);
	  gtk_widget_show(edtreeframe);
	  gtkaction_clearprogress();
	  break;
     case UI_GRAPH:	/* graph or curve drawing interface */
	  /* get the data set to graph: find runtime arguments 
	   * inherited by the selected choice node, then call the
	   * get data method on that node and finally date stamp the
	   * node */
	  gtkaction_setprogress("collecting", 0.33, 0);
	  uichoice_getinheritedargs(node, datapres_nodeargs);
	  datapres_data = node->getdata(datapres_nodeargs);
	  node->datatime = time(NULL);
	  /* draw the data set: set the timebase implied by the node,
	   * create the graphattr and graphinst windows and pack them
	   * into the control area of the graph pane */
	  gtkaction_setprogress("drawing", 0.66, 0);
	  gmcgraph_settimebasebynode(graph, datapres_nodeargs);
	  datapres_widget2 = gtkaction_mkgraphinst(datapres_data);
	  datapres_widget  = gtkaction_mkgraphattr(datapres_data);
	  gtk_container_add (GTK_CONTAINER(attributeview), datapres_widget);
	  if (datapres_widget2) {
	       /* enable the instance display */
	       gtk_container_add (GTK_CONTAINER(instanceview), 
				  datapres_widget2);
	       gtk_paned_handle_size(GTK_PANED(listpanes), 8);
	       gtk_paned_gutter_size(GTK_PANED(listpanes), 9);
	       gtk_paned_set_position(GTK_PANED(listpanes), 100);
	       gtk_widget_show(datapres_widget2);
	       gtk_widget_show(instanceframe);
	  } else {
	       /* disable any previous display */
	       gtk_paned_handle_size(GTK_PANED(listpanes), 1);
	       gtk_paned_gutter_size(GTK_PANED(listpanes), 1);
	       gtk_paned_set_position(GTK_PANED(listpanes), 1);
	       gtk_widget_hide(instanceframe);
	  }

	  /* set the label with the choice node path */
	  path = uichoice_nodepath(node, " - ");
	  shortpath = strstr(path, "-");
	  shortpath = (shortpath) ? shortpath+2 : path;
	  gtk_frame_set_label (GTK_FRAME(graphframe), shortpath);
	  nfree(path);

	  /* want a better way of doing the following */
	  gtk_paned_set_position(GTK_PANED (graphpanes),
		 GTK_WIDGET (panes)->allocation.width - 330);
	  gtk_widget_show(graphframe);
	  gtk_widget_show(datapres_widget);
	  gtk_widget_show(menugraph);
	  gtk_widget_set_sensitive(save_viewed_data, TRUE);
	  gtk_widget_set_sensitive(send_data_to_app, TRUE);
	  gtk_widget_set_sensitive(send_data_to_email, TRUE);
	  gtkaction_clearprogress();
	  break;
     default:
          elog_printf(ERROR, "presentation: UNKNOWN!!\n");
	  gtkaction_clearprogress();
	  return;
     }

     /*gtkaction_setprogress("complete", 1.0, 0);*/
     datapres_type = node->presentation;
     datapres_node = node;
     gtkaction_choice_update_start();
     gdk_window_set_cursor(baseWindow->window, mouse_pointer_normal);
}

/* Deselect the currently selected item and free its data pointers.
 * Set the global parameters to neutral ie NULL/SPLASH settings etc
 */
void gtkaction_choice_deselect()
{
     /* make sure nothing is updating to impeed us */
     gtkaction_choice_update_stop();

     /* clear up argument nodes.
      * The data is the property of the uichoice routines so we
      * don't free or destroy any of the data. We just remove the entries
      * from the tree which point to them.
      */
     tree_clearout(datapres_nodeargs, NULL, NULL);

     /* clear up previous presentation widgets */
     switch (datapres_type) {
     case UI_HELP:	/* help screen */
          /*g_print("remove pres widget: help\n");*/
	  break;
     case UI_NONE:	/* no interface (use splash anyway)*/
     case UI_SPLASH:	/* splash graphic */
	  gtk_widget_hide(splash_view);
	  break;
     case UI_TABLE:	/* table or grid interface */
	  uidata_freeresdat(datapres_data);
	  gtk_widget_hide(tableframe);
	  gtk_container_remove(GTK_CONTAINER(tablescroll), datapres_widget);
	  gtk_widget_set_sensitive(save_viewed_data, FALSE);
	  gtk_widget_set_sensitive(send_data_to_app, FALSE);
	  gtk_widget_set_sensitive(send_data_to_email, FALSE);
	  break;
     case UI_EDTABLE:	/* editable table or grid interface */
          /*g_print("remove pres widget: edtable\n");*/
	  break;
     case UI_FORM:	/* form interface: prompt text and value */
          /*g_print("remove pres widget: form\n");*/
	  break;
     case UI_EDFORM:	/* editable form interface: prompt text and value */
          /*g_print("remove pres widget: edform\n");*/
	  break;
     case UI_TEXT:	/* text interface */
          /*g_print("remove pres widget: text\n");*/
	  break;
     case UI_EDTEXT:	/* editable text interface */
          /*g_print("remove pres widget: edtext\n");*/
	  break;
     case UI_EDTREE:	/* editable table using a tree interface */
          /*g_print("remove pres widget: edtree\n");*/
	  gtk_widget_hide(edtreeframe);
	  gtkaction_rmedtree();
	  break;
     case UI_GRAPH:	/* graph or curve drawing interface */
          /*g_print("remove pres widget: graph\n");*/
	  gtk_widget_hide(graphframe);
	  gtk_container_remove (GTK_CONTAINER(attributeview), datapres_widget);
	  if (datapres_widget2)
	       gtk_container_remove (GTK_CONTAINER(instanceview), 
				     datapres_widget2);
	  gtkaction_rmgraphinst();
	  gtkaction_rmgraphattr();
	  gmcgraph_rmallgraphs(graph);
	  uidata_freeresdat(datapres_data);
	  gtk_widget_hide(menugraph);
	  gtk_widget_set_sensitive(save_viewed_data, FALSE);
	  gtk_widget_set_sensitive(send_data_to_app, FALSE);
	  gtk_widget_set_sensitive(send_data_to_email, FALSE);
	  break;
     default:
          elog_printf(ERROR, "remove pres widget: UNKNOWN!!\n");
	  break;
     }

     /* default to splash */
     datapres_type   = UI_NONE;
     datapres_widget = splash_view;
     datapres_widget2= NULL;
     datapres_node   = NULL;
     datapres_data.t = TRES_NONE;
}



/* start the update timer */
void gtkaction_choice_update_start() {
     /* start data view update timer */
     if (gtkaction_prestimer != -1)
	  gtkaction_choice_update_stop();
     gtkaction_prestimer = gtk_timeout_add(GTKACTION_PRESTIMEOUT, 
					   gtkaction_choice_updateifneeded, 
					   NULL);
}


/* stop the update timer */
void gtkaction_choice_update_stop() {
     if (gtkaction_prestimer != -1)
	  gtk_timeout_remove( gtkaction_prestimer );
     gtkaction_prestimer = -1;
}


/* 
 * Check the choice node associated with the currently displayed data
 * to see if the data or the dynamic nodes have timed-out and need 
 * to be updated.
 */
gint gtkaction_choice_updateifneeded() {
     time_t now;

     now = time(NULL);

     /* update viewed data first as thats the main thing on show to users  */
     if (datapres_node && datapres_node->datatimeout > 0) {
	  if (datapres_node->datatime + datapres_node->datatimeout < now)
	       gtkaction_choice_update();
     }

#if 0
     /* update dynamic children under the current node */
     if (datapres_node && datapres_node->dyntimeout > 0) {
	  if (datapres_node->dyntime + datapres_node->dyntimeout < now)
	       gtkaction_node_update( NEED A NODE );
     }
#endif

     return TRUE;		/* continue timeouts */
}



/*
 * update the dynamic children of the specified node.
 */
void gtkaction_node_update(struct uichoice_node *node)
{
     int nchildren;
     GtkCTreeNode *treeitem;

     /* turn off data updates */
     gtkaction_choice_update_stop();

     /* update the dynamic menu */
     nchildren = uichoice_updatedynamic(node);

     /* update the menu interface */
     treeitem = *(GtkCTreeNode **) uichoice_getnodearg(node, 
						       GTKACTION_GUIITEMKEY);
     gtkaction_expandchoice(treeitem, 1, tooltips);

     /* re-enable updates */
     gtkaction_choice_update_start();
}



/* 
 * Update the currently selected tree item, using the the node and 
 * widgets held globally: gtkaction_datapres, gtkaction_node
 */
void gtkaction_choice_update()
{
     RESDAT dres;
     GtkWidget *new_widget;
     char progress[64];
     TABLE tab;
     time_t youngest_t, oldest_t, tsecs, tsecs_orig, test_young_t;
     char  *youngest_str, *oldest_str;

     /* The presentation of data in ghabitat is done by switching frames
      * of data from specialist objects within a single presentation box.
      * This achieves speed, simplicity and better user appearence. 
      * The frame widgets are:-
      *    graphframe    The timeseries graph
      *    splash_view   Splash screen
      *    tableframe    Table widget
      *    edtreeframe   Editable table information
      * datapres_type and datapres_widget are set to refer to the 
      * information being displayed.
      */

     /* only dynamic nodes may be updated */
     switch (datapres_node->presentation) {
     case UI_HELP:	/* help screen */
     case UI_NONE:	/* no interface (use splash anyway)*/
     case UI_SPLASH:	/* splash graphic */
     case UI_EDTABLE:	/* editable table or grid interface */
     case UI_FORM:	/* form interface: prompt text and value */
     case UI_EDFORM:	/* editable form interface: prompt text and value */
     case UI_TEXT:	/* text interface */
     case UI_EDTEXT:	/* editable text interface */
     case UI_EDTREE:	/* editable table using a tree interface */
	  /* do nothing but exit for all these: none are dynamic */
          elog_printf(DIAG, "Can't update node %s, as it is not dynamic "
		      "(presentation type %d)", datapres_node->label, 
		      datapres_node->presentation);
	  return;
     }

     /*
      * For efficiency, we only want new data that may have been collected
      * since the last collection. This will be used to extend the length 
      * of the sample.
      * For now, we won't expire the older data, ie we will accumulate.
      * By convention, the node argument 'tsecs' dictates the amount
      * of historic data to collect. We alter it following the normal 
      * trawl through the choice tree arguments to only take the most recent.
      */

     /* find most recent time of the current displayed data
      * (taken from table in TRES structure, _time column).
      * Customise the duration of the node to pick up the new data */
     switch (datapres_data.t) {
     case TRES_NONE:
	  return;
     case TRES_TABLE:
	  tab = datapres_data.d.tab;
	  break;
     case TRES_TABLELIST:
	  itree_last(datapres_data.d.tablst);
	  tab = itree_get(datapres_data.d.tablst);
	  break;
     case TRES_EDTABLE:
	  /* not dynamic so exit */
	  return;
     }
     table_last(tab);
     youngest_str = table_getcurrentcell(tab, "_time");
     if (youngest_str) {
          youngest_t = strtol(youngest_str, (char**)NULL, 10);
	  tsecs = time(NULL) - (youngest_t+1);	/* extend node duration to
						 * pick up new data */
     } else {
	  elog_printf(DIAG, "no _time column, can't update with recent "
		      "data so completely redisplaying");
	  youngest_t = oldest_t = -1;
     }

     /* clear up argument nodes in order to fetch again.
      * The data is the property of the uichoice routines so we
      * don't free or destroy any of the data. We just remove the entries
      * from the tree which point to them.
      */
     tree_clearout(datapres_nodeargs, NULL, NULL);

     /* get new node arguments and override the tsecs value with the
      * new one prepared above, which will request for updates.
      * when we have got the new data, we will replace tsecs */
     gtkaction_setprogress("preparing", 0.2, 0);
     uichoice_getinheritedargs(datapres_node, datapres_nodeargs);
     gtkaction_setprogress("collecting latest", 0.4, 0);
     if (youngest_t != -1) {
	if (tree_find(datapres_nodeargs, "tsecs") != TREE_NOVAL) {
	     tsecs_orig = *((time_t *) tree_get(datapres_nodeargs));
	     *((time_t *) tree_get(datapres_nodeargs)) = tsecs;
	     /*elog_printf(DEBUG, "tsecs: read=%d, stored=%d", tsecs_orig, 
	       tsecs);*/
	}
     }

     /* now collect the most recent data */
     dres = datapres_node->getdata(datapres_nodeargs);
     datapres_node->datatime = time(NULL);
#if 0
     elog_printf(DEBUG, "current last data %d %s, tsecs=%d",
		 youngest_t, util_decdatetime(youngest_t), tsecs);
     if (dres.t == TRES_NONE)
	  printf("no new contents\n");
     else
	  printf("appending contents=%s\n", table_print(dres.d.tab));
#endif

     /* Restore the value of tsecs back in choice tree */
     if (tree_find(datapres_nodeargs, "tsecs") != TREE_NOVAL) {
	  tsecs = *((time_t *) tree_get(datapres_nodeargs));
	  *((time_t *) tree_get(datapres_nodeargs)) = tsecs_orig;
	  /*elog_printf(DEBUG, "tsecs: read=%d, stored=%d", tsecs, 
	    tsecs_orig);*/
     }

     if (dres.t == TRES_NONE) {
          elog_printf(DIAG, "No new data available to %s current view", 
		      youngest_t == -1 ? "replace" : "append");
	  gtkaction_setprogress("no update", 0, 0);
	  return;
     }


     /* Treat time based data differently from non-time.
      * Non-time data removes old data and replace it with the new.
      * Conversely, time-based data is appended to the existing data */
     if (youngest_t == -1) {
          /* -- data is a complete refresh and will replace the current set */

	  uidata_freeresdat(datapres_data);
	  datapres_data.t = dres.t;
	  if (dres.t == TRES_TABLELIST) {
	       /* new data is a list of tables */
	       datapres_data.d.tablst = itree_create();
	       itree_traverse(dres.d.tablst)
		    itree_append(datapres_data.d.tablst, 
				 itree_get(dres.d.tablst));
	  } else {
	       /* new data is a single table */
	       datapres_data.d.tab = dres.d.tab;
	  }
     } else {
          /* -- time based data is prepended to the current set*/

          /* converting existing TRES TABLE types to TABLELIST types;
	   * We use TABLELIST as it will be much faster to add on data,
	   * although slower to clear down */
          if (datapres_data.t == TRES_TABLE) {
	       datapres_data.t = TRES_TABLELIST;
	       datapres_data.d.tablst = itree_create();
	       itree_append(datapres_data.d.tablst, tab);
	  }

	  /* new data is an update, append depending on its type */
	  if (dres.t == TRES_TABLELIST) {
	       /* new data is a list of tables */
	       itree_traverse(dres.d.tablst)
		    itree_append(datapres_data.d.tablst, 
				 itree_get(dres.d.tablst));
	  } else {
	       /* new data is a single table */
	       itree_append(datapres_data.d.tablst, dres.d.tab);
	  }

	  /* find youngest time in new data, as it will have changed */
	  itree_last(datapres_data.d.tablst);
	  table_last(itree_get(datapres_data.d.tablst));
	  youngest_str = table_getcurrentcell(
	       itree_get(datapres_data.d.tablst), "_time");
	  if (youngest_str)
	       youngest_t = strtol(youngest_str, (char**)NULL, 10);
     }


     /* Expire old data
      * Used to prevent excessive data build up in time based data.
      * To save time at the expense of space, only remove whole tables
      * out of our table list in TABLST, rather than removing lines 
      * in big tables.
      * Remove tables whose youngest data have been expired */
     if (youngest_t != -1) {

          /* Calculate the oldest time */
          oldest_t = time(NULL) - tsecs_orig;

	  /* Walk the table list */
	  itree_first(datapres_data.d.tablst);
	  while ( ! itree_isbeyondend(datapres_data.d.tablst) ) {
	       /* For each table, find the youngest time */
	       table_last(itree_get(datapres_data.d.tablst));
	       youngest_str = table_getcurrentcell(
	             itree_get(datapres_data.d.tablst), "_time");
	       if (youngest_str) {
		    test_young_t = strtol(youngest_str, (char**)NULL, 10);
		    if (test_young_t < oldest_t) {
		         /* Remove table from list if old */
		         elog_printf(DEBUG, "removing old data: youngest_t=%d",
				     test_young_t);
			 table_destroy(itree_get(datapres_data.d.tablst));
			 itree_rm(datapres_data.d.tablst);
			 continue;
		    } else {
		         /* The list is in order, so this is the first table
			  * we want to keep. Find the oldest time in the 
			  * table, save it and break out for reporting */
		         table_first(itree_get(datapres_data.d.tablst));
			 oldest_str = table_getcurrentcell(
			       itree_get(datapres_data.d.tablst), "_time");
			 if (oldest_str)
			      oldest_t = strtol(oldest_str, (char**)NULL, 10);
			 else
			      oldest_t = -1;
			 break;
		    }
	       }
	       itree_next(datapres_data.d.tablst);	/* next entry if no _time */
	  }
     }

     /* draw specific widget types */
     gtkaction_setprogress("redrawing", 0.66, 0);
     switch (datapres_type) {
     case UI_TABLE:	/* table or grid interface */
	  /* reaquire the args again as they may have been changed,
	   * collect data */
	  new_widget = gtkaction_mktable(datapres_data);
	  gtkaction_setprogress("display", 0.80, 0);
	  gtk_widget_hide(datapres_widget);
	  gtk_container_remove(GTK_CONTAINER(tablescroll), datapres_widget);
	  datapres_widget = new_widget;
	  gtk_container_add   (GTK_CONTAINER(tablescroll), datapres_widget);
	  gtk_widget_show(datapres_widget);
	  if (GTK_CLIST(datapres_widget)->rows == 0 ||
	      GTK_CLIST(datapres_widget)->columns == 0) {
	       gtkaction_setprogress("table is empty", 0.8, 0);
	  } else {
	       sprintf(progress, "%d row%s %d column%s", 
		       GTK_CLIST(datapres_widget)->rows, 
		       GTK_CLIST(datapres_widget)->rows==1?"":"s", 
		       GTK_CLIST(datapres_widget)->columns, 
		       GTK_CLIST(datapres_widget)->columns==1?"":"s");
	       gtkaction_setprogress(progress, 0.8, 0);
	  }
	  gtkaction_setprogress(progress, 0, 0);
	  break;
     case UI_GRAPH:	/* graph or curve drawing interface */
	  /* curves, colours and the oldest point do not change, just the
	   * new youngest data time */
	  /* TODO: set oldest point based on the existing data */
	  gmcgraph_settimebase(graph, oldest_t, youngest_t);
	  gtkaction_graphattr_redraw(datapres_data);
	  gtkaction_clearprogress();
	  break;
     default:
          g_print("presentation: UNKNOWN!!\n");
	  gtkaction_clearprogress();
	  return;
     }
}




/*
 * Create a new table widget from a RESDAT, ordering and filtering
 * the columns from 'columnorder'. Returns a clist widget.
 * Column order follows that set in TABLE.
 */
GtkWidget *gtkaction_mktable(RESDAT dres)
{
     TABLE dtab=0;
     GtkWidget *wtable;
     GtkTooltips *tips;
     int i, ncols;
     gchar **cols;
     ITREE *hdorder;
     char *name, *bigtip, *info, *key, *keystr, *cell;

     /* form a single consolidated table from the list of tables */
     if (dres.t == TRES_TABLE)
	  dtab = dres.d.tab;
     else if (dres.t == TRES_TABLELIST) {
	  dtab = table_create();
	  itree_traverse(dres.d.tablst)
	       table_addtable(dtab, itree_get(dres.d.tablst), 1);
     } else {
	  elog_printf(INFO, "No data");
     }

     /* empty table, but we have to return something */
     if ( ! dtab || table_nrows(dtab) == 0 ) {
	  return gtk_clist_new(1);
     }

     /* find out the order from the TABLE */
     hdorder = dtab->colorder;
     ncols = itree_n(hdorder);

     /* create clist using headings from TABLE headers */
     cols = xnmalloc( itree_n(hdorder) * sizeof(gchar *) );
     i = 0;
     itree_traverse( hdorder ) {
          cols[i] = itree_get( hdorder );
	  name = table_getinfocell(dtab, "name", cols[i]);
	  if (name && *name && strcmp(name, "0") != 0 
	      && strcmp(name, "-") != 0)
	       cols[i] = name;
	  i++;
     }

     /* make list */
     wtable = gtk_clist_new_with_titles( itree_n(hdorder), cols );
     if ( ! wtable )
          elog_die(FATAL, "unable to make clist");

     /* signal for double clicking */
#if 0
     gtk_signal_connect(GTK_OBJECT(wtable),
			"button_press_event",
			GTK_SIGNAL_FUNC(gtkaction_table_select),
			NULL);
#endif
     gtk_signal_connect(GTK_OBJECT(wtable),
			"select_row",
			GTK_SIGNAL_FUNC(gtkaction_table_select),
			NULL);

     /* create a tooltips for the clist, such that is 'garbage collect'ed
      * once the clist is destroyed */
     tips = gtk_tooltips_new();

     /* add tooltips into the headers, by digging into their definitions
      * and using the internal button widgets. A composite big tip is 
      * provided and tagged on the table for later garbage collection */
     i=0;
     itree_traverse( hdorder ) {
          name = itree_get( hdorder );
          key  = table_getinfocell(dtab, "key", name);
          info = table_getinfocell(dtab, "info", name);
	  if ( ! info)
	       info = "";
	  if (key == NULL)
	       keystr = "";
	  else if (*key == '1')
	       keystr = ",primary key";
	  else if (*key == '2')
	       keystr = ",secondary key";
	  else if (*key == '3')
	       keystr = ",tertiary key";
	  else
	       keystr = "";
	  if (*keystr) {
	       bigtip = util_strjoin(info, " (", keystr, ")", NULL);
	  } else {
	       bigtip = xnstrdup(info);
	  }
	  table_freeondestroy(dtab, bigtip);
	  gtk_tooltips_set_tip(tips, GTK_CLIST(wtable)->column[i].button, 
			       bigtip, NULL);
	  i++;
     }

     /* traverse TABLE and append into the clist widget */
     table_traverse(dtab) {
	  i = 0;
	  itree_traverse( hdorder ) {
	       cell = table_getcurrentcell(dtab, itree_get(hdorder));
	       if (strncmp(itree_get(hdorder), "_time", 5) == 0) {
		 cols[i++] = util_decdatetime(strtol(cell, (char**)NULL, 10));
	       } else {
		 cols[i++] = cell;
	       }
	  }
	  gtk_clist_append( GTK_CLIST(wtable), cols);
     }

     /* size for the text */
     gtk_clist_columns_autosize( GTK_CLIST(wtable) );

     gtk_clist_thaw( GTK_CLIST(wtable) );

     /* clear up and free */
     nfree(cols);
     return wtable;
}


/* Calback for double click events from the table presentation type */
void gtkaction_table_select (GtkWidget *widget,
			     gint row,
			     gint column,
			     GdkEventButton *event,
			     gpointer data)
{
     int i;
     char *ctitle, *val;
     ITREE *t_ctitle, *t_val;
     GtkWidget *tableframe;

     if (GTK_IS_CLIST(widget) &&
	 (event->type==GDK_2BUTTON_PRESS ||
	  event->type==GDK_3BUTTON_PRESS) ) {

	  /*
	  g_print("I feel %s clicked on button %d, row %d, col %d\n", 
		  event->type==GDK_2BUTTON_PRESS ? "double" : "triple",
		  event->button, row, column);
	  */

	  /* create a duplicate set of data, so that it is properly 
	   * indpendent of the source. 
	   * Make up-down order the same as the left-right order */
	  t_ctitle = itree_create();
	  t_val    = itree_create();

	  for (i=0; i < GTK_CLIST(widget)->columns; i++) {
	       ctitle = gtk_clist_get_column_title(GTK_CLIST(widget), i);
	       gtk_clist_get_text(GTK_CLIST(widget), row, i, &val);

	       itree_append(t_ctitle, xnstrdup(ctitle));
	       itree_append(t_val,    xnstrdup(val));

	       /*g_print("%20s   %s\n", ctitle, val);*/
	  }

	  /* prepare title */
	  tableframe = lookup_widget(GTK_WIDGET(widget),"tableframe");

	  gtkaction_create_record_window(GTK_FRAME(tableframe)->label,
					 row, GTK_CLIST(widget)->rows, 
					 t_ctitle, t_val);
     }
}


/* create a graph instance pick list, which is a simple table of 
 * check buttons, connected to call backs, each of which represents 
 * a key value from the data. The key values are taken from the column
 * flagged as primary key (has info line 'key', column is marked '1').
 * Returns the GtkTable widget on successfully finding keys, which 
 * should be packed into a suitable space in the interface.
 * Returns NULL if no keys are found.
 * As globals, set gtkaction_graphsel, which are the instances currently
 * selected, which may not be current for the viewed data set and
 * gtkaction_inst which is the contents of the displayed instance list.
 */
GtkWidget *gtkaction_mkgraphinst(RESDAT dres	/* result structure */)
{
     int i;
     GtkWidget *wtable;
     GtkWidget *witem=NULL;
     TREE *whd;
     char *keyval=NULL;

     /* Find the unique key values that the key column holds */
     if (dres.t == TRES_NONE) {
	  return NULL;
     } else if (dres.t == TRES_TABLE) {
	  whd = table_getheader(dres.d.tab);
	  tree_traverse(whd) {
	       keyval = table_getinfocell(dres.d.tab, "key", tree_getkey(whd));
	       if (keyval && *keyval == '1') {
		    /* found the primary key column, now get the
		     * unique key values which represent the 
		     * instances on the GUI */
		    gtkaction_keycol = tree_getkey(whd);
		    table_uniqcolvals(dres.d.tab, gtkaction_keycol, 
				      &gtkaction_inst);
		    break;
	       }
	  }
     } else {
	  itree_traverse(dres.d.tablst) {
	       whd = table_getheader( (TABLE) itree_get(dres.d.tablst) );
	       tree_traverse(whd) {
		    keyval=table_getinfocell((TABLE) itree_get(dres.d.tablst),
					     "key", tree_getkey(whd));
		    if (keyval && *keyval == '1') {
			 /* found the primary key column, now get the
			  * unique key values which represent the 
			  * instances on the GUI and add them to the
			  * other from previous iterations */
			 gtkaction_keycol = tree_getkey(whd);
			 table_uniqcolvals((TABLE) itree_get(dres.d.tablst),
					   gtkaction_keycol, &gtkaction_inst);
			 break;
		    }
	       }
	  }
     }

     /* create the instance UI item, which is a gtktable and fill with rows 
      * of button & field widgets as dictated by the attribute list */
     if (gtkaction_inst && tree_n(gtkaction_inst) > 0) {
	  /* --- we have keys: multi-instance data --- */

	  /* create a default in the selected instance (graph) list if 
	   * there is nothing already set */
	  i = 0;
	  tree_traverse(gtkaction_inst)
	       if (tree_find(gtkaction_graphsel, 
			     tree_getkey(gtkaction_inst)) != TREE_NOVAL)
		    i++;
	  if ( ! i ) {
	       tree_first(gtkaction_inst);
	       tree_add(gtkaction_graphsel, 
			xnstrdup(tree_getkey(gtkaction_inst)), 
			NULL);
	  }

	  /* set frame title and holding table */
	  gtk_frame_set_label(GTK_FRAME(instanceframe), gtkaction_keycol);
	  wtable = gtk_table_new(tree_n(gtkaction_inst), 1, FALSE);

	  /* add buttons to table */
	  i = 0;
	  tree_traverse(gtkaction_inst) {
	       /* create button and attach to table */
	       witem = gtk_check_button_new_with_label(tree_getkey(
							    gtkaction_inst));
	       gtk_table_attach (GTK_TABLE(wtable), witem, 0, 1, i, i+1, 
				 GTK_FILL|GTK_EXPAND,0,0,0);
	       gtk_widget_show (witem);

	       /* set active if selected, and THEN setup the callback signal.
		* We only want ATTRIBUTES to draw curves as they will
		* draw on all selected graphs anyway. If we draw here there
		* would be a loop! */
	       if (tree_find(gtkaction_graphsel, 
			     tree_getkey(gtkaction_inst)) != TREE_NOVAL)
		    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(witem), 
						 TRUE);
	       gtk_signal_connect(GTK_OBJECT(witem),  "clicked", 
				  GTK_SIGNAL_FUNC(gtkaction_graphinst_clicked),
				  tree_getkey(gtkaction_inst));
	       i++;
	  }

     } else {
	  /* we don't have keys: simple data, signal with table with no rows */
	  wtable = NULL;
     }

     return wtable;
}


/* Remove the instance pick list. The GUI widgets are actually removed by the 
 * recursive gtk_widget_destroy() command on the list.
 */
void gtkaction_rmgraphinst()
{
     /* remove the instance list */
     if (gtkaction_inst) {
	  tree_destroy(gtkaction_inst);	/* the keys are referencies to the
					 * DRES graph data contained in 
					 * tables */
	  gtkaction_inst = NULL;
     }
}



/*
 * Callback when an instance button has been clicked, which will cause a
 * new graph to be displayed or an existing one to be removed
 */
void gtkaction_graphinst_clicked(GtkWidget *widget, char *name)
{
     if (GTK_TOGGLE_BUTTON (widget)->active) {
	  /* add graph name to list */
	  tree_add(gtkaction_graphsel, xnstrdup(name), NULL);

	  /* draw the new graph */
	  gtkaction_drawgraph(datapres_data, name);
     } else {
	  /* remove graph name from list */
	  if (tree_find(gtkaction_graphsel, name) != TREE_NOVAL) {
	       nfree(tree_getkey(gtkaction_graphsel));
	       tree_rm(gtkaction_graphsel);
	  }

	  /* remove instance graph */
	  gmcgraph_rmgraph(graph, name);
     }
}




/* 
 * Create a graph attribute pick list, which is a table of widgets each 
 * line pertaining to a graph curve. gtkaction_mkgraphinst should be
 * called first to set up the graph list (if any) for data instances and
 * the graph GMCGRAPH widget graph should have been initialised before 
 * calling.
 *
 * The list gtkaction_curvesel is checked for previously 
 * selected curves and the appropreate lines are set to `on'.
 * If the list is empty, a default curve is chosen, set to `on',
 * and this creates an entry in gtkaction_curvesel.
 * Curves that have been selected are drawn on the graphs named in 
 * union of gtkaction_graphsel AND gtkaction_inst (selected current 
 * instances which are set up by gtkaction_mkgraphinst)
 * or the default graph if single instance.
 *
 * Should really be done with a clist, but it doesn't support embedded 
 * widgets yet.
 * Sets up the global picklist_button as a side effect, which is used 
 * when dynamically adding the scale and offset widgets.
 */
GtkWidget *gtkaction_mkgraphattr(RESDAT dres	/* result structure */)
{
     TREE *hd, *whd;		/* headers and working headers */
     TREE *cname;		/* cosmetic column name */
     TREE *maxes;		/* maximum col values */
     int nrows, i;
     GtkWidget *wtable;
     GtkWidget *witem;
     GtkWidget *box;
     GtkWidget *label;
     GtkTooltips *tips;
     char *bigtip, *name, *info, *max;
     GdkColor *colour;
     GtkStyle *newstyle;
     float maxval=0.0;

     /* delete old widgets */
     if (picklist_hdscale) {
          /*gtk_widget_destory(picklist_hdscale);*/
          /*gtk_widget_destory(picklist_hdoffset);*/
	  picklist_hdscale = NULL;
	  picklist_hdoffset = NULL;
     }

     /* Compile union of all column names and a summary of info lines
      * tooltips from dres, but excluding the columns that are keys.
      * Only primary keys are currently supported, however.
      * The infolines are to be used for button tooltips */
     if (dres.t == TRES_NONE)
	  return gtk_table_new(1, 1, FALSE);

     hd = tree_create();
     cname = tree_create();
     maxes = tree_create();
     if (dres.t == TRES_TABLE) {
	  whd = table_getheader(dres.d.tab);
	  tree_traverse(whd) {
	       if (gtkaction_keycol && 
		   strcmp(gtkaction_keycol, tree_getkey(whd)) == 0) {
		    continue;	/* skip all cols flagged as keys */
	       }
	       info = table_getinfocell(dres.d.tab, "info", tree_getkey(whd));
	       if (info)
		    bigtip = xnstrdup(info);
	       else
		    bigtip = xnstrdup("");
	       tree_add(hd, tree_getkey(whd), bigtip);
	       table_freeondestroy(dres.d.tab, bigtip);
	       name = table_getinfocell(dres.d.tab, "name", tree_getkey(whd));
	       if (name && *name && strcmp(name, "0") != 0 
		   && strcmp(name, "-") != 0)
		    tree_add(cname, tree_getkey(whd), name);
	       else
		    tree_add(cname, tree_getkey(whd), tree_getkey(whd));
	       max = table_getinfocell(dres.d.tab, "max", tree_getkey(whd));
	       if (max && *max && strcmp(max, "0") != 0 
		   && strcmp(max, "0.0") != 0 && strcmp(max, "-") != 0)
		    tree_add(maxes, tree_getkey(whd), max);
	  }
     } else {
	  itree_traverse(dres.d.tablst) {
	       whd = table_getheader( (TABLE) itree_get(dres.d.tablst) );
	       tree_traverse(whd) {
		    if (gtkaction_keycol && 
			strcmp(gtkaction_keycol, tree_getkey(whd)) == 0) {
			 continue;	/* skip all cols flagged as keys */
		    }
		    if ( tree_find(hd, tree_getkey(whd)) == TREE_NOVAL ) {
		         info = table_getinfocell(
					   (TABLE) itree_get(dres.d.tablst),
					   "info", tree_getkey(whd));
			 if (info)
			      bigtip = xnstrdup(info);
			 else
			      bigtip = xnstrdup("");
			 tree_add(hd, tree_getkey(whd), bigtip);
			 table_freeondestroy((TABLE) itree_get(dres.d.tablst),
					     bigtip);
			 name=table_getinfocell((TABLE)itree_get(dres.d.tablst),
						"name", tree_getkey(whd));
			 if (name && *name && strcmp(name, "0") != 0 
			     && strcmp(name, "-") != 0)
			      tree_add(cname, tree_getkey(whd), name);
			 else
			      tree_add(cname, tree_getkey(whd), 
				       tree_getkey(whd));
			 max = table_getinfocell((TABLE)itree_get(dres.d.tablst),
						 "max", tree_getkey(whd));
			 if (max && *max && strcmp(max, "0") != 0 
			     && strcmp(max, "0.0") != 0 
			     && strcmp(max, "-") != 0)
			      tree_add(maxes, tree_getkey(whd), max);
		    }
	       }
	  }
     }

     /* count visable rows (not starting with `_') */
     nrows = 0;
     tree_traverse(hd) {
	  if (*tree_getkey(hd) != '_')
	       nrows++;
     }

     if (nrows <= 0) {
          tree_destroy(hd);
          tree_destroy(cname);
	  return gtk_table_new(1, 1, FALSE);
     }

     /* The table layout widget should be 3 columns wide:-
      *   col 1 - icon + label
      *   col 2 - scale pull-down
      *   col 3 - offset spinbox
      * However, for speed we hide cols 2 and 3 and dont create the
      * widgets (we don't usually need them).
      */
     wtable = gtk_table_new (nrows+1, 3, FALSE);
     /*gtk_table_set_col_spacing (GTK_TABLE(wtable), 0, 2);*/

     witem = gtk_label_new ("name");
     gtk_table_attach (GTK_TABLE(wtable), witem, 0, 1, 0, 1, 
		       GTK_FILL|GTK_EXPAND,0,0,0);
     gtk_misc_set_alignment (GTK_MISC(witem), 0.0, 0.0);
     gtk_widget_show (witem);

     tips = gtk_tooltips_new();

     /* create holder for all toggle_states */
     picklist_button = nmalloc(sizeof(toggle_state) * nrows);
     picklist_nbuttons = nrows;

     /* create a default if no curve is being displayed; use the 
      * hd list, which has had keys removed, but meta cols like _time and
      * _seq are still present. As a default, find the first non
      * meta colum  */
     i = 0;
     tree_traverse(hd)
	  if (tree_find(gtkaction_curvesel, tree_getkey(hd)) != TREE_NOVAL)
	       i++;
     if ( ! i ) {
	  tree_traverse(hd) {
	       if ( *tree_getkey(hd) != '_' ) {
		    tree_add(gtkaction_curvesel, xnstrdup(tree_getkey(hd)), 
			     NULL);
		    break;
	       }
	  }
     }

     /* traverse TABLE `hd' and insert rows into table */
     i = 0;
     tree_traverse(hd) {
	  /* create pick rows for all columns except those begining with `_' */
	  if (*tree_getkey(hd) == '_')
	       continue;	/* next column */

	  /* create toggle button from two images */
	  picklist_button[i].bg_gc = NULL;
	  witem = gtk_toggle_button_new ();
	  gtk_container_set_border_width (GTK_CONTAINER(witem), 0); /*?*/
	  box = gtk_hbox_new (FALSE, 0);
	  gtk_container_set_border_width (GTK_CONTAINER(box), 0); /*?*/
	  picklist_button[i].off = gtk_pixmap_new (icon_graphoff, 
						   mask_graphoff);
	  picklist_button[i].on  = gtk_pixmap_new (icon_graphon,  
						   mask_graphon );
	  gtk_misc_set_alignment(GTK_MISC(picklist_button[i].off), 0.0, 0.5);
	  gtk_misc_set_alignment(GTK_MISC(picklist_button[i].on), 0.0, 0.5);
	  gtk_box_pack_start (GTK_BOX(box), picklist_button[i].off, 
			      FALSE, FALSE, 0);
	  gtk_box_pack_start (GTK_BOX(box), picklist_button[i].on,  
			      FALSE, FALSE, 0);

	  /* work out which name to use as a label */
	  picklist_button[i].colname = xnstrdup(tree_getkey(hd));
	  label = gtk_label_new(tree_find(cname, tree_getkey(hd)));
	  picklist_button[i].label = label;
	  /*gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);*/
	  gtk_box_pack_start (GTK_BOX(box), label, FALSE, FALSE, 0);
	  gtk_container_add (GTK_CONTAINER (witem), box);
	  gtk_table_attach (GTK_TABLE(wtable), witem, 0, 1,
			    i+1, i+2, GTK_FILL|GTK_EXPAND,0,0,0);
	  gtk_tooltips_set_tip(tips, witem, tree_get(hd), NULL);

          /* draw the button in 'on' or 'off' state */
	  if (tree_present(gtkaction_curvesel, tree_getkey(hd))) {
	       /*g_print("drawing already selected curve %s\n", 
		 tree_getkey(hd));*/
	       gtk_widget_show (picklist_button[i].on);
	       picklist_button[i].state = 1;
	       gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(witem), TRUE);
	  } else {
	       gtk_widget_show (picklist_button[i].off);
	       picklist_button[i].state = 0;
	  }
	  gtk_signal_connect (GTK_OBJECT(witem),  "clicked", 
			      GTK_SIGNAL_FUNC(gtkaction_graphattr_select), 
			      &picklist_button[i]);
	  gtk_widget_show(label);
	  gtk_widget_show(box);
	  gtk_widget_show(witem);
	  picklist_button[i].scale = NULL;
	  picklist_button[i].offset = NULL;

	  /* set the maximum value if present */
	  if (tree_find(maxes, tree_getkey(hd)) == TREE_NOVAL)
	       picklist_button[i].max = 0.0;
	  else
	       picklist_button[i].max = atof(tree_get(maxes));
	  if (picklist_button[i].state && picklist_button[i].max > maxval)
	       maxval = picklist_button[i].max;

	  /* draw graph if slected (vanilla, no gradient or scale changes) */
	  if (picklist_button[i].state == 1) {
	       colour = gtkaction_drawcurve(dres, tree_getkey(hd), 1.0, 0.0);

	       /* colour the button */
	       gdk_color_alloc (gtk_widget_get_colormap (witem), colour);
	       newstyle = gtk_style_copy(gtk_widget_get_default_style());
	       newstyle->bg[GTK_STATE_ACTIVE] = *colour;
	       newstyle->bg[GTK_STATE_PRELIGHT] = *colour;
	       gtk_widget_set_style(witem, newstyle);
	  }

	  i++;
     }

     /* set max value */
     gmcgraph_setallminmax(graph, maxval);
     gmcgraph_updateallaxis(graph);
     gtkaction_updateall();

     /* clear up and return */
     tree_destroy(hd);
     tree_destroy(cname);
     return wtable;
}


/* Remove the pick list. The GUI widgets are actually removed by the 
 * recursive gtk_widget_destroy() command on the list, but the nmalloc name
 * & globals still need to be recovered & reset.
 */
void gtkaction_rmgraphattr()
{
     GtkWidget *more, *less;
     int i;

     /* remove the instance list */
     /* TODO */

     /*g_print("gtkaction_rmgraphattr()\n");*/
     /* free memory and make globals NULL, as they are used as flags
      * between gtkaction_morewidgets() and gtkaction_lesswidgets() */
     picklist_hdscale  = NULL;
     picklist_hdoffset = NULL;
     if (picklist_button != NULL) {
	  for (i=0; i<picklist_nbuttons; i++)
	       nfree(picklist_button[i].colname);
	  nfree(picklist_button);
	  picklist_button = NULL;
     }
     picklist_nbuttons = 0;

     /* lookup the more and less button widgets to ensure the 
      * more is visible */
     more = lookup_widget(baseWindow, "ctl_morewidgets");
     less = lookup_widget(baseWindow, "ctl_lesswidgets");
     gtk_widget_show(more);
     gtk_widget_hide(less);
}



/* 
 * Make extra widgets for the existing graphattr table to control the 
 * offset and gradient of curves. Relys on the global picklist_button 
 * list being setup with the picklist details.
 */
void gtkaction_graphattr_morewidgets(GtkTable *wtable, RESDAT dres)
{
     GtkWidget *witem;
     GtkObject *wsubitem;
     GList *glist;
     int i;

     /* check if we need to do any work */
     if (dres.t == TRES_NONE)
	  return;

     /* check to see if the widgets already exist from a previous
      * `morewidgets()' call. If so, reuse those */
     if (picklist_hdscale) {
	  g_print("reusing widgets\n");
	  gtk_widget_show (picklist_hdscale);
	  gtk_widget_show (picklist_hdoffset);
	  for (i=0; i< picklist_nbuttons; i++) {
	       gtk_widget_show(picklist_button[i].scale);
	       gtk_widget_show(picklist_button[i].offset);
	  }
	  return;
     }

     /* add labels to top of table */
     witem = gtk_label_new("scale");
     gtk_table_attach (GTK_TABLE(wtable), witem, 1, 2, 0, 1, GTK_FILL,0,0,0);
     gtk_misc_set_alignment (GTK_MISC(witem), 0.0, 0.0);
     gtk_widget_show (witem);
     picklist_hdscale = witem;

     witem = gtk_label_new("offset");
     gtk_table_attach (GTK_TABLE(wtable), witem, 2, 3, 0, 1, GTK_FILL,0,0,0);
     gtk_misc_set_alignment (GTK_MISC(witem), 0.0, 0.0);
     gtk_widget_show (witem);
     picklist_hdoffset = witem;

     /* iterate over the graph pick list */
     for (i=0; i< picklist_nbuttons; i++) {
	  /*gtk_label_get (GTK_LABEL(picklist_button[i].label), &key);*/

	  /* create scale pull-down or combo box */
	  witem = gtk_combo_new();
	  glist = NULL;
	  glist = g_list_append(glist, "1000");
	  glist = g_list_append(glist, "100"); 
	  glist = g_list_append(glist, "10");
	  glist = g_list_append(glist, "1");
	  glist = g_list_append(glist, "0.1");
	  glist = g_list_append(glist, "0.01"); 
	  glist = g_list_append(glist, "0.001");
	  gtk_combo_set_popdown_strings( GTK_COMBO(witem), glist);
	  g_list_free(glist);
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(witem)->entry), "1");
	  /*gtk_misc_set_alignment(GTK_MISC(witem), 0.0, 0.5);*/
	  gtk_table_attach (GTK_TABLE(wtable), witem, 1, 2, i+1, i+2,
			    GTK_SHRINK, 0, 0, 0);
	  gtk_widget_set_usize (witem, 50, -2);
	  gtk_signal_connect(GTK_OBJECT(GTK_COMBO(witem)->entry), "changed",
			     GTK_SIGNAL_FUNC (gtkaction_graphattr_scale),
			     &picklist_button[i]);
	  gtk_widget_show(witem);
	  picklist_button[i].scale = witem;

	  /* create spin button for offsets */
	  wsubitem = gtk_adjustment_new(0.0, -10000000.0, +10000000.0, 0.1, 
					10.0, 0.0);
	  witem = gtk_spin_button_new (GTK_ADJUSTMENT(wsubitem), 0.1, 1);
	  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (witem), TRUE);
	  /*gtk_misc_set_alignment(GTK_MISC(witem), 0.0, 0.5);*/
	  gtk_table_attach (GTK_TABLE(wtable), witem, 2, 3, i+1, i+2,
			    GTK_SHRINK, 0, 0, 0);
	  gtk_signal_connect(GTK_OBJECT(wsubitem), "value_changed",
			     GTK_SIGNAL_FUNC (gtkaction_graphattr_offset),
			     &picklist_button[i]);
	  gtk_widget_show(witem);
	  picklist_button[i].offset = witem;
     }
}

/* Remove the offset and gradient widgets from the graphpich table */
void gtkaction_graphattr_lesswidgets(GtkTable *wtable)
{
     int i;

     /* check if we need to do any work */
     if (picklist_hdscale == NULL)
	  return;

     gtk_widget_hide(picklist_hdscale);
     gtk_widget_hide(picklist_hdoffset);
     /* iterate over the graph pick list, hiding the widgets */
     for (i=0; i< picklist_nbuttons; i++) {
	  gtk_widget_hide(picklist_button[i].scale);
	  gtk_widget_hide(picklist_button[i].offset);
     }
}


/* Callback when a graphattr label or button has been selected.
 * It will cause the curve to be drawn or removed */
void gtkaction_graphattr_select(GtkWidget *widget, toggle_state *s)
{
     char *name, *scale_str;
     GdkColor *colour;
     GtkStyle *newstyle;
     float scale=1.0, offset=0.0, max=0.0;
     int i;

     /*g_print("gtkaction_graphattr_clist_select\n");*/

     /* get the curve name */
     /*gtk_label_get(GTK_LABEL(s->label), &name);*/
     name = s->colname;

     if (s->state) {
	  /* curve is currently drawn or `on': turn it off */
	  gtk_widget_hide(s->on);
	  gtk_widget_show(s->off);
	  s->state = 0;

	  /* find and set new max */
	  for (i=0; i < picklist_nbuttons; i++)
	       if (picklist_button[i].state && picklist_button[i].max > max)
		    max = picklist_button[i].max;
	  gmcgraph_setallminmax(graph, max);

	  /* remove curve name from list */
	  if (tree_find(gtkaction_curvesel, name) != TREE_NOVAL) {
	       nfree(tree_getkey(gtkaction_curvesel));
	       tree_rm(gtkaction_curvesel);
	  }

	  /* hide the curve and uncolour the button */
	  if (gtkaction_inst && tree_n(gtkaction_inst) > 0) {
	       tree_traverse(gtkaction_inst) {
		    gmcgraph_rmcurve(graph, tree_getkey(gtkaction_inst), name);
	       }
	  } else {
	       gmcgraph_rmcurve(graph, "default", name);
	  }
	  gtk_widget_set_rc_style(widget);
     } else {
	  /* curve is not drawn or `off': turn it on */
	  gtk_widget_hide(s->off);
	  gtk_widget_show(s->on);
	  s->state = 1;

	  /* find and set new max */
	  for (i=0; i < picklist_nbuttons; i++)
	       if (picklist_button[i].state && picklist_button[i].max > max)
		    max = picklist_button[i].max;
	  gmcgraph_setallminmax(graph, max);

	  /* add curve name to list */
	  tree_add(gtkaction_curvesel, xnstrdup(name), NULL);

	  if (s->scale && s->offset) {
	       /* get the scale/gradient (m) and offset (c) from the 
	        * graphattr list (the formula is y = mx + c) */
	       scale_str = gtk_entry_get_text(GTK_ENTRY
					      (GTK_COMBO(s->scale)->entry));
	       scale = atof(scale_str);
	       offset = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON
							   (s->offset));
	  }

	  /* draw the curve and colour the pick list button */
	  colour = gtkaction_drawcurve(datapres_data, name, scale, offset);
	  gtkaction_updateall();

	  gdk_color_alloc (gtk_widget_get_colormap (widget), colour);
	  newstyle = gtk_style_copy(gtk_widget_get_default_style());
	  newstyle->bg[GTK_STATE_ACTIVE] = *colour;
	  newstyle->bg[GTK_STATE_PRELIGHT] = *colour;
	  gtk_widget_set_style(widget, newstyle);
     }

     /*g_print("gtkaction_graphattr_clist_select finished\n");*/
}


/* 
 * Callback for when a combo box is changed and the appropreate curve 
 * needs to be updated with the new scale
 */
void gtkaction_graphattr_scale(GtkWidget *widget, toggle_state *s)
{
     char *name, *scale_str;
     float scale, offset;

     /* get the curve name */
     /*gtk_label_get(GTK_LABEL(s->label), &name);*/
     name = s->colname;

     /* if curve is inactive, do nothing */
     if (s->state == 0 || s->scale == NULL || s->offset == NULL)
          return;

     /* get the scale/gradient (m) and offset (c) from the graphattr list
      * (the formula is y = mx + c) */
     scale_str = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(s->scale)->entry));
     scale = atof(scale_str);
     offset = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(s->offset));

     /* draw the curve & ignore returned colour, it is already allocated */
     gtkaction_drawcurve(datapres_data, name, scale, offset);
     gtkaction_updateall();
}

/*
 * Callback when the spin button has changed its value and the appropreate
 * curve needs to have its origin changed.
 */
void gtkaction_graphattr_offset(GtkWidget *widget, toggle_state *s)
{
     /* the code is the same as the scale callback, so we reuse that */
     gtkaction_graphattr_scale(widget, s);
}


/*
 * Clear the graph of curves and redraw using the passed RESDAT structure
 * and the curves & colours selected in graphattr.
 */
void gtkaction_graphattr_redraw(RESDAT dres)
{
     int i;
     char *name, *scale_str;
     float scale, offset;

     if (dres.t == TRES_NONE)
	  return;			/* no data: do nothing */

     /* iterate over the graph pick list */
     for (i=0; i< picklist_nbuttons; i++) {
	  if (picklist_button[i].state == 0)
	       continue;

	  if (picklist_button[i].scale && 
	      picklist_button[i].offset) {

	       /* get the scale/gradient (m) and offset (c) from the 
		* graphattr list (the formula is y = mx + c) */
	       scale_str = gtk_entry_get_text
		    (GTK_ENTRY(GTK_COMBO(picklist_button[i].scale)->entry));
	       scale = atof(scale_str);
	       offset = gtk_spin_button_get_value_as_float
		    (GTK_SPIN_BUTTON(picklist_button[i].offset));
	  } else {
	       scale  = 1.0;
	       offset = 0.0;
	  }
	       
	  /* draw the curve & ignore returned colour, 
	   * it is already allocated */
	  /*gtk_label_get (GTK_LABEL(picklist_button[i].label), &name);*/
	  gtkaction_drawcurve(dres, picklist_button[i].colname, scale, 
			      offset);
     }
     gtkaction_updateall();
}



/*
 * Draw a curve in one or more graphs, scaling if required. 
 * Returns the colour assigned by gmcgraph or NULL for error.
 */
GdkColor *gtkaction_drawcurve(RESDAT dres,	/* result structure */
			      char *curve,	/* curve name */
			      float scale,	/* curve scale/magnitude */
			      float offset	/* y-axis offset */ )
{
     float *xvals, *yvals;
     int nvals, i;
     GdkColor *colour = NULL;
     char *prt=NULL;			/* debug variable */

#if 0
     /* debugging code block to show the arguments and table contents */
     if (dres.t==TRES_TABLE) {
	  elog_printf(DEBUG, "drawing %s, %d elements, scale %f, offset %f", 
		      curve, table_nrows(dres.d.tab), scale, offset);
     } else {
	  elog_printf(DEBUG, "drawing %s, %d tables, scale %f, offset %f", 
		      curve, itree_n(dres.d.tablst), scale, offset);
	  itree_traverse(dres.d.tablst) {
	       prt = table_print((TABLE) itree_get(dres.d.tablst));
	       elog_printf(DEBUG, "table %d = %s", 
			   itree_getkey(dres.d.tablst), prt);
	       nfree(prt);
	  }
     }
#endif

     if (gtkaction_inst && tree_n(gtkaction_inst) > 0) {
	  /* multiple instances: multiple graphs */
	  tree_traverse(gtkaction_inst) {
	       if (tree_find(gtkaction_graphsel, 
			     tree_getkey(gtkaction_inst)) == TREE_NOVAL)
		    continue;
	       nvals = gmcgraph_resdat2arrays(graph, dres, curve, 
					      gtkaction_keycol, 
					      tree_getkey(gtkaction_inst), 
					      &xvals, &yvals);
	       if (nvals <= 1)
		    return NULL;
	       if (scale != 1.0 || offset > 0.0)
		    for (i=0; i < nvals; i++)
			 yvals[i] = scale * yvals[i] + offset;
	       colour = gmcgraph_draw(graph, tree_getkey(gtkaction_inst), 
				      curve, nvals, xvals, yvals, 
				      1 /*overwrite*/ );
	       /*gmcgraph_update(graph, tree_getkey(gtkaction_graphsel));*/
	  }
     } else {
	  /* single, default instance */
	  nvals = gmcgraph_resdat2arrays(graph, dres, curve,
					 NULL, NULL,
					 &xvals, &yvals);
	  if (nvals <= 1)
	       return NULL;
	  for (i=0; i < nvals; i++)
	       yvals[i] = scale * yvals[i] + offset;
	  colour = gmcgraph_draw(graph, NULL, curve, nvals,
				 xvals, yvals, 1 /*overwrite*/ );
	  
	  /*gmcgraph_update(graph, NULL);*/
     }

     return colour;
}

/* Calls gmcgraph_update for both single and multi instances */
void gtkaction_updateall() {
     if (gtkaction_inst && tree_n(gtkaction_inst) > 0) {
	  /* multiple instances: multiple graphs */
	  tree_traverse(gtkaction_inst) {
	       /* gtkaction_graphsel hold historic names as well as 
		* current, gtkaction_inst are graphs that can currently 
		* potentially be selected. We need a union of both 
		* structures to work out what needs to be refreshed */
	       if (tree_find(gtkaction_graphsel, 
			     tree_getkey(gtkaction_inst)) == TREE_NOVAL)
		    continue;
	       gmcgraph_update(graph, tree_getkey(gtkaction_graphsel));
	  }
     } else {
	  /* single, default instance */
	  gmcgraph_update(graph, NULL);
     }
}


/*
 * Traverse selected curves and draw them for a single instance on a 
 * single graph. We assume that this is not the first graph to be drawn
 * and that we can ignore the returned colours: the buttons will
 * not need to be coloured.
 */
void gtkaction_drawgraph(RESDAT dres,	/* result structure */
			 char *instance	/* graph name (instance) */ )
{
     float *xvals, *yvals, scale, offset, max;
     char *scale_str, *curve;
     int nvals, i;

     /* iterate over the graph pick list containing current selected curves */
     for (i=0; i< picklist_nbuttons; i++) {
	  if (picklist_button[i].state) {
	       /* get the scale/gradient (m) and offset (c) from the 
		* graphattr list (the formula is y = mx + c) */
	       if (picklist_button[i].scale && picklist_button[i].offset) {
		    scale_str = gtk_entry_get_text
		       (GTK_ENTRY(GTK_COMBO(picklist_button[i].scale)->entry));
		    scale = atof(scale_str);
		    offset = gtk_spin_button_get_value_as_float
			 (GTK_SPIN_BUTTON(picklist_button[i].offset));
	       } else {
		    scale = 1.0;
		    offset = 0.0;
	       }

	       /* get curve name and convert to floats */
	       gtk_label_get (GTK_LABEL(picklist_button[i].label), &curve);
	       nvals = gmcgraph_resdat2arrays(graph, dres, curve, 
					      gtkaction_keycol, instance,
					      &xvals, &yvals);
	       if (nvals <= 1)
		    return;

	       /* scale if needed */
	       if (scale != 1.0 || offset > 0.0)
		    for (i=0; i < nvals; i++)
			 yvals[i] = scale * yvals[i] + offset;

	       /* now draw, ignoring colour */
	       gmcgraph_draw(graph, instance, curve, nvals, xvals, yvals, 1);
	  }
     }

     /* find and set new max */
     for (i=0; i < picklist_nbuttons; i++)
	  if (picklist_button[i].state && picklist_button[i].max > max)
	       max = picklist_button[i].max;
     gmcgraph_setallminmax(graph, max);
     gmcgraph_updateallaxis(graph);

     /* now everything is finished, update graph to screen */
     gmcgraph_update(graph, instance);
}




/*
 * Make a edit tree
 * This is one in which the top level nodes are summaries of each row in the 
 * RESDAT table. Each top level node then desendents in the next level that 
 * contain input widgets corresponding to columns in the table's row. 
 * Also, two buttons are are placed under the input widgets to action or 
 * cancel changes to that row using the new data.
 * A final, empty top level node is provided for new entries.
 * The tree in edtreeframe (edtree) is used and reused for efficiency.
 */
void gtkaction_mkedtree(RESDAT dres	/* result structure */ )
{
     GtkWidget *topitem;
     GtkWidget *subtree;
     GtkWidget *subitem;
     GtkWidget *box;
     GtkWidget *button;
     GtkTooltips *tips;
     ITREE *columns;
     char *summary;
     TREE *row;

     if (dres.t != TRES_EDTABLE)
	  return;

     /* create a tooltips for the clist, such that is 'garbage collect'ed
      * once the clist is destroyed */
     tips = gtk_tooltips_new();

     /* get column names */
     columns = table_getcolorder(dres.d.edtab.tab);

     table_traverse(dres.d.edtab.tab) {
	  /* summerise data */
	  row = table_getcurrentrow (dres.d.edtab.tab);
	  summary = dres.d.edtab.summary( row );
	  tree_destroy(row);

	  /* create top level node */
	  topitem = gtk_tree_item_new_with_label(summary);
	  gtk_tree_append(GTK_TREE(edtree), topitem);
	  gtk_widget_show(topitem);
	  nfree(summary);

	  /* create subtree */
	  subtree = gtk_tree_new();
	  gtk_widget_show(subtree);
	  gtk_tree_item_set_subtree(GTK_TREE_ITEM(topitem), subtree);

	  /* create input widgets */
	  itree_traverse(columns) {
	       gtkaction_mkedtreerow(tips, subtree, itree_get(columns), 
				     dres.d.edtab.tab, 1);
	  }

	  /* create the buttons */
	  subitem = gtk_tree_item_new();
	  gtk_tree_append(GTK_TREE(subtree), subitem);
	  gtk_widget_show(subitem);
	  box = gtk_hbox_new(FALSE, 10);
	  button = gtk_button_new_with_label("update");
	  gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	  gtk_widget_show(button);
	  gtk_signal_connect (GTK_OBJECT (button), "clicked",
			      GTK_SIGNAL_FUNC (gtkaction_edtree_update_cb),
			      NULL);
	  button = gtk_button_new_with_label("abort");
	  gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	  gtk_widget_show(button);
	  gtk_widget_show(box);
	  gtk_signal_connect (GTK_OBJECT (button), "clicked",
			      GTK_SIGNAL_FUNC (gtkaction_edtree_abort_cb),
			      NULL);
	  gtk_container_add (GTK_CONTAINER (subitem), box);
     }

     /* create an empty row for new items */
     topitem = gtk_tree_item_new_with_label("new");
     gtk_tree_append(GTK_TREE(edtree), topitem);
     gtk_widget_show(topitem);

     /* create subtree */
     subtree = gtk_tree_new();
     gtk_widget_show(subtree);
     gtk_tree_item_set_subtree(GTK_TREE_ITEM(topitem), subtree);

     /* insert empty cells */
     itree_traverse(columns) {
	  gtkaction_mkedtreerow(tips, subtree, itree_get(columns), 
				dres.d.edtab.tab, 0);
     }

     /* create the buttons for creation */
     subitem = gtk_tree_item_new();
     gtk_tree_append(GTK_TREE(subtree), subitem);
     gtk_widget_show(subitem);
     box = gtk_hbox_new(FALSE, 10);
     button = gtk_button_new_with_label("create");
     gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
     gtk_widget_show(button);
     gtk_signal_connect (GTK_OBJECT (button), "clicked",
			 GTK_SIGNAL_FUNC (gtkaction_edtree_create_cb),
			 NULL);
     button = gtk_button_new_with_label("abort");
     gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
     gtk_widget_show(button);
     gtk_widget_show(box);
     gtk_container_add (GTK_CONTAINER (subitem), box);
     gtk_signal_connect (GTK_OBJECT (button), "clicked",
			 GTK_SIGNAL_FUNC (gtkaction_edtree_abort_cb),
			 NULL);

     return;
}


/* Make empty input field widgets in the edtree widget */
void gtkaction_mkedtreerow(GtkTooltips *tips, GtkWidget *subtree, 
			   char *prompt, TABLE tab, int is_value_insert)
{
     GtkWidget *subitem;
     GtkWidget *box;
     GtkWidget *label;
     GtkWidget *field;

     subitem = gtk_tree_item_new();
     gtk_tree_append(GTK_TREE(subtree), subitem);
     gtk_widget_show(subitem);
     box = gtk_hbox_new(FALSE, 10);
     label = gtk_label_new(prompt);
     gtk_widget_set_usize (label, 75, -2);
     gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
     gtk_tooltips_set_tip (tips, label, table_getinfocell(tab, "info", prompt),
			   NULL);
     gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
     gtk_widget_show(label);
     field = gtk_entry_new();
     if (is_value_insert)
	  gtk_entry_set_text (GTK_ENTRY(field), 
			      table_getcurrentcell(tab, prompt));
     gtk_box_pack_start(GTK_BOX(box), field, FALSE, FALSE, 0);
     gtk_widget_show(field);
     gtk_widget_show(box);
     gtk_container_add (GTK_CONTAINER (subitem), box);
}



/* callback for the update button on an edtree row */
void gtkaction_edtree_update_cb (GtkButton       *button,
				 gpointer         user_data)
{
     GtkTreeItem *line_treeitem;
     GtkTree *field_tree;

     /* we have crawl up the list of widgets
      * tree -> line treeitem -> field tree -> box treeitem -> vbox -> button
      * we want to get the line tree item so that we can collapse it.
      * Note that the parent of field tree is the top level tree, not the
      * treeitem; to get that you want the tree_owner of field tree.
      */
     field_tree = GTK_TREE(GTK_WIDGET(button)->parent->parent->parent);
     line_treeitem = GTK_TREE_ITEM(field_tree->tree_owner);
     gtk_tree_item_collapse(line_treeitem);
}


/* callback for the abort button on an edtree row */
void gtkaction_edtree_abort_cb (GtkButton       *button,
				gpointer         user_data)
{
     GtkTreeItem *line_treeitem;
     GtkTree *field_tree;

     /* collapse the subtree
      * we have crawl up the list of widgets
      * tree -> line treeitem -> field tree -> box treeitem -> vbox -> button
      * we want to get the line tree item so that we can collapse it.
      * Note that the parent of field tree is the top level tree, not the
      * treeitem; to get that you want the tree_owner of field tree.
      */
     field_tree = GTK_TREE(GTK_WIDGET(button)->parent->parent->parent);
     line_treeitem = GTK_TREE_ITEM(field_tree->tree_owner);
     gtk_tree_item_collapse(line_treeitem);
}



/* callback for the create button on an edtree row */
void gtkaction_edtree_create_cb (GtkButton       *button,
				 gpointer         user_data)
{
     GtkTreeItem *line_treeitem;
     GtkTree *field_tree;

     /* collapse the subtree
      * we have crawl up the list of widgets
      * tree -> line treeitem -> field tree -> box treeitem -> vbox -> button
      * we want to get the line tree item so that we can collapse it.
      * Note that the parent of field tree is the top level tree, not the
      * treeitem; to get that you want the tree_owner of field tree.
      */
     field_tree = GTK_TREE(GTK_WIDGET(button)->parent->parent->parent);
     line_treeitem = GTK_TREE_ITEM(field_tree->tree_owner);
     gtk_tree_item_collapse(line_treeitem);
}



/* Remove an edit tree */
void gtkaction_rmedtree()
{
     GList *items;

     items = gtk_container_children (GTK_CONTAINER(edtree));
     gtk_tree_remove_items(GTK_TREE(edtree), items);
     while (items)
	  items = g_list_remove_link(items, items);
}


/* Callback when an error or log is routed to here (elog system) */
void gtkaction_elog_raise(const char *errtext, 	/* text containing error */
			  int etlen		/* length of error text */ )
{
     char *errtext_dup, ecode, *esev, *efile, *efunc, *eline, *etext;
     time_t etime;

     /* in main(), we declared the message format to be:-
      *
      *       e|time|severity|file|function|line|text
      *
      * where e is the error character: d, i, w, e, f.
      */

     /* make a copy of the error text so we can patch it to buggery */
     errtext_dup =  xnstrdup(errtext);
     if (errtext_dup[etlen-1] == '\n')
          errtext_dup[etlen-1] = '\0';

     /* isolate the components of the error string */
     ecode = *strtok(errtext_dup, "|");
     etime = strtol(strtok(NULL, "|"), (char**)NULL, 10);
     esev  = strtok(NULL, "|");
     efile = strtok(NULL, "|");
     efunc = strtok(NULL, "|");
     eline = strtok(NULL, "|");
     etext = strtok(NULL, "|");

     /* place error text into the GUI history, push it onto the status bar
      * and append it to the log popup (if it exists */
     uidata_logmessage(ecode, etime, esev, efile, efunc, eline, etext);
     gtk_statusbar_push(GTK_STATUSBAR(messagebar), gtkaction_elogmsgid, etext);
     if (logpopup_table)
	  gtkaction_log_popup_dline(ecode, etime, esev, efile, efunc, 
				    eline, etext);
}


/*
 * Set the text and percentage progress in the progress bar, used for
 * short term, non-logged status messages.
 * If text is NULL, no text changes take place but the percentage complete
 * value is used to update the progress bar.
 * If the percent is -1, then the completion bar is not updated.
 * If showpercent is true, a % figure is appended to the text status
 */
void gtkaction_setprogress(char *text, float percent, int showpercent)
{
     char *ptext;

     if (text) {
	  if (showpercent) {
	       ptext = util_strjoin(text, " %p %%", NULL);
	       gtk_progress_set_format_string (GTK_PROGRESS(progressbar), 
					       ptext);
	       nfree(ptext);
	  } else
	       gtk_progress_set_format_string (GTK_PROGRESS(progressbar), 
					       text);
     }

     if (percent > -1)
	  gtk_progress_bar_update (GTK_PROGRESS_BAR(progressbar), percent);

     /* update pending widgets */
     while (gtk_events_pending())
	  gtk_main_iteration();

#if 0
     gtkaction_progresstimer = gtk_timeout_add(500, gtkaction_sigprogress, 
					       progressbar);
#endif
}

/* gtk timeout callback for progressbar */
gint gtkaction_sigprogress(gpointer data)
{
     gtk_timeout_remove(gtkaction_progresstimer);
     gtkaction_progresstimer = -1;
     return TRUE;
}

/* Remove the status message and progress bar */
void gtkaction_clearprogress()
{
     gtk_progress_set_format_string (GTK_PROGRESS(progressbar), "");
     gtk_progress_bar_update (GTK_PROGRESS_BAR(progressbar), 0.0);

#if 0
     if ( gtkaction_progresstimer != -1 )
	  gtk_timeout_remove(gtkaction_progresstimer);
#endif
}


/* initialise the logpopup colours */
void gtkaction_log_popup_init()
{
     int i;

     for (i=0; i<6; i++) {
	  gdk_color_parse( logpopup_bgcolname[i], &logpopup_bgcolour[i] );
	  gdk_color_parse( logpopup_fgcolname[i], &logpopup_fgcolour[i] );
     }
}


/* Callback to tell gtkaction that the log popup is created and visible */
void gtkaction_log_popup_created(GtkWidget *w		/* log clist table*/ )
{
     /* saved clist table */
     logpopup_table = w;
}


/* If gtkaction_log_popup_created() has been called, return its widget, 
 * otherwise reutrn NULL (implying it does not exist) */
GtkWidget *gtkaction_log_popup_available() {
     return logpopup_table;
}


/* Callback to tell gtkaction that the log popup is unavailable */
void gtkaction_log_popup_destroyed() {
     logpopup_table = NULL;
}


/* Callback to assign the window manager icon */
void gtkaction_anypopup_setwmicon(GtkWidget *w)
{
     /* root window icon */
     gtkaction_setwmicon(w->window, icon_sysgarwm, mask_sysgarwm);
}


/*
 * Function to draw the current state of the elogs into a clist.
 * If sev is set to NOELOG, use the previously seen value for severity.
 * If coloured is set to -1, then use their previous values also.
 * Otherwise, use the settings to change the way text is drawn 
 */
void gtkaction_log_popup_draw(GtkWidget *clist, 
			      enum elog_severity sev, 
			      int coloured)
{
     RESDAT resdat;
     char *clistrow[6];
     int row=0;

     /* handle options */
     if (coloured != -1)
	  logpopup_coloured = coloured;
     if (sev != NOELOG)
	  logpopup_severity = sev;

     /*printf ("log draw: sev=%d col=%d\n", sev, coloured);*/

     /* prepare the table */
     gtk_clist_clear(GTK_CLIST(clist));

     /* get logs */
     resdat = uidata_getlocallogs(NULL);

     /* iterate over the table and populate the UI */
     table_traverse(resdat.d.tab) {
	  /* get data for entire row */
	  clistrow[0] = table_getcurrentcell(resdat.d.tab, "time");
	  clistrow[1] = table_getcurrentcell(resdat.d.tab, "severity");
	  clistrow[2] = table_getcurrentcell(resdat.d.tab, "message");
	  clistrow[3] = table_getcurrentcell(resdat.d.tab, "function");
	  clistrow[4] = table_getcurrentcell(resdat.d.tab, "file");
	  clistrow[5] = table_getcurrentcell(resdat.d.tab, "line");

	  /* filter severity */
	  if (logpopup_severity != NOELOG && logpopup_severity != DEBUG) {
	       if (*clistrow[1] == 'e' && logpopup_severity > ERROR)
		    continue;
	       if (*clistrow[1] == 'w' && logpopup_severity > WARNING)
		    continue;
	       if (*clistrow[1] == 'i' && logpopup_severity > INFO)
		    continue;
	       if (*clistrow[1] == 'd' && clistrow[1][1] == 'i' 
		   && logpopup_severity > DIAG)
		    continue;
	       if (*clistrow[1] == 'd' && clistrow[1][1] == 'e')
		    continue;
	  }

	  /* add data to clist */
	  gtk_clist_append(GTK_CLIST(clist), clistrow);

	  /* colourise the line if required */
	  if (logpopup_coloured) {
	       if (*clistrow[1] == 'f') {
		    gtk_clist_set_background( GTK_CLIST(clist), row,
					      &logpopup_bgcolour[0]);
		    gtk_clist_set_foreground( GTK_CLIST(clist), row,
					      &logpopup_fgcolour[0]);
	       } else if (*clistrow[1] == 'e') {
		    gtk_clist_set_background( GTK_CLIST(clist), row,
					      &logpopup_bgcolour[1]);
		    gtk_clist_set_foreground( GTK_CLIST(clist), row,
					      &logpopup_fgcolour[1]);
	       } else if (*clistrow[1] == 'w') {
		    gtk_clist_set_background( GTK_CLIST(clist), row,
					      &logpopup_bgcolour[2]);
		    gtk_clist_set_foreground( GTK_CLIST(clist), row,
					      &logpopup_fgcolour[2]);
	       } else if (*clistrow[1] == 'i') {
		    gtk_clist_set_background( GTK_CLIST(clist), row,
					      &logpopup_bgcolour[3]);
		    gtk_clist_set_foreground( GTK_CLIST(clist), row,
					      &logpopup_fgcolour[3]);
	       } else if (*clistrow[1] == 'd' && clistrow[1][1] == 'i') {
		    gtk_clist_set_background( GTK_CLIST(clist), row,
					      &logpopup_bgcolour[4]);
		    gtk_clist_set_foreground( GTK_CLIST(clist), row,
					      &logpopup_fgcolour[4]);
	       } else {
		    gtk_clist_set_background( GTK_CLIST(clist), row,
					      &logpopup_bgcolour[5]);
		    gtk_clist_set_foreground( GTK_CLIST(clist), row,
					      &logpopup_fgcolour[5]);
	       }
	  }
	  row++;
     }

     /* big lists will need the most recent mesage shown */
     if ( gtk_clist_row_is_visible( GTK_CLIST(clist), 
				    row-1) == GTK_VISIBILITY_NONE )
	  gtk_clist_moveto( GTK_CLIST(clist), row-1, 0, 1.0, 0.0);

     /* display the UI and free the log data */
     uidata_freeresdat(resdat);
}


/* Function to append a single line the clist of the log popup using the 
 * current settings */
void gtkaction_log_popup_dline(char ecode, time_t time, char *sev, 
			       char *file, char *func, char *line, 
			       char *text) {
     char *clistrow[6];
     int row, shownew;

     if ( !logpopup_table)
	  return;

     /* filter severity */
     if (logpopup_severity != NOELOG && logpopup_severity != DEBUG) {
	  if (*sev == 'e' && logpopup_severity > ERROR)
	       return;
	  if (*sev == 'w' && logpopup_severity > WARNING)
	       return;
	  if (*sev == 'i' && logpopup_severity > INFO)
	       return;
	  if (*sev == 'd' && clistrow[1][1] == 'i' 
	      && logpopup_severity > DIAG)
	       return;
	  if (*sev == 'd' && sev[1] == 'e')
	       return;
     }

     /* hook up into insertion list */
     clistrow[0] = util_shortadaptdatetime(time);
     clistrow[1] = sev;
     clistrow[2] = text;
     clistrow[3] = func;
     clistrow[4] = file;
     clistrow[5] = line;

     /* see if the last message is currently shown. 
      * If it is, assume that the user wants to see the newly appended
      * message when we have finished */
     row = (GTK_CLIST(logpopup_table)->rows);
     if ( row && gtk_clist_row_is_visible( GTK_CLIST(logpopup_table), 
					   row-1) == GTK_VISIBILITY_NONE )
	  shownew = 0;
     else
	  shownew = 1;

     /* draw line */
     gtk_clist_append(GTK_CLIST(logpopup_table), clistrow);

     /* colourise the line if required */
     if (logpopup_coloured) {
	  if (*sev == 'f') {
	       gtk_clist_set_background( GTK_CLIST(logpopup_table), row,
					 &logpopup_bgcolour[0]);
	       gtk_clist_set_foreground( GTK_CLIST(logpopup_table), row,
					 &logpopup_fgcolour[0]);
	  } else if (*sev == 'e') {
	       gtk_clist_set_background( GTK_CLIST(logpopup_table), row,
					 &logpopup_bgcolour[1]);
	       gtk_clist_set_foreground( GTK_CLIST(logpopup_table), row,
					 &logpopup_fgcolour[1]);
	  } else if (*sev == 'w') {
	       gtk_clist_set_background( GTK_CLIST(logpopup_table), row,
					 &logpopup_bgcolour[2]);
	       gtk_clist_set_foreground( GTK_CLIST(logpopup_table), row,
					 &logpopup_fgcolour[2]);
	  } else if (*sev == 'i') {
	       gtk_clist_set_background( GTK_CLIST(logpopup_table), row,
					 &logpopup_bgcolour[3]);
	       gtk_clist_set_foreground( GTK_CLIST(logpopup_table), row,
					 &logpopup_fgcolour[3]);
	  } else if (*sev == 'd' && sev[1] == 'i') {
	       gtk_clist_set_background( GTK_CLIST(logpopup_table), row,
					 &logpopup_bgcolour[4]);
	       gtk_clist_set_foreground( GTK_CLIST(logpopup_table), row,
					 &logpopup_fgcolour[4]);
	  } else {
	       gtk_clist_set_background( GTK_CLIST(logpopup_table), row,
					 &logpopup_bgcolour[5]);
	       gtk_clist_set_foreground( GTK_CLIST(logpopup_table), row,
					 &logpopup_fgcolour[5]);
	  }
     }

     /* show the just drawn line, unless we are looking at some thing else */
     if (shownew)
	  gtk_clist_moveto( GTK_CLIST(logpopup_table), row, 0, 1.0, 0.0);
}


/* routine to query the state of the popup preferences */
void gtkaction_log_popup_state(enum elog_severity *sev, 
			       int *coloured)
{
     *sev = logpopup_severity;
     *coloured = logpopup_coloured;
     /*printf("popup state req: sev=%d col=%d\n", *sev, *coloured);*/
}


/* create an independent top-level window containing a 2-col table
 * of value title and value. It has summary/location information 
 * and a button to dismiss it. */
void gtkaction_create_record_window(char *w_title, int row, int rows, 
				    ITREE *c_title, ITREE *c_val)
{
     GtkWidget *record_window;
     GtkWidget *record_vbox;
     GtkWidget *record_toolbar;
     GtkWidget *record_label;
     GtkWidget *record_ok_button;
     GtkWidget *record_scroll;
     GtkWidget *record_table;
     GtkWidget *record_table_title;
     GtkWidget *record_table_value;
     char *clistrow[2], rowstr[40];;

     record_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
     gtk_widget_set_name (record_window, "record_window");
     gtk_object_set_data (GTK_OBJECT (record_window), "record_window", 
			  record_window);
     gtk_widget_set_usize (record_window, 500, 300);
     gtk_window_set_title (GTK_WINDOW (record_window), w_title);
     gtk_window_set_position (GTK_WINDOW (record_window), 
			      GTK_WIN_POS_MOUSE);
     gtk_window_set_policy (GTK_WINDOW (record_window), TRUE, TRUE, FALSE);

     record_vbox = gtk_vbox_new (FALSE, 0);
     gtk_widget_set_name (record_vbox, "record_vbox");
     gtk_widget_ref (record_vbox);
     gtk_object_set_data_full (GTK_OBJECT (record_window), 
			       "record_vbox", record_vbox,
			       (GtkDestroyNotify) gtk_widget_unref);
     gtk_widget_show (record_vbox);
     gtk_container_add (GTK_CONTAINER (record_window), record_vbox);

     record_toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, 
				       GTK_TOOLBAR_BOTH);
     gtk_widget_set_name (record_toolbar, "record_toolbar");
     gtk_widget_ref (record_toolbar);
     gtk_object_set_data_full (GTK_OBJECT (record_window), 
			       "record_toolbar", record_toolbar,
			       (GtkDestroyNotify) gtk_widget_unref);
     gtk_widget_show (record_toolbar);
     gtk_box_pack_start (GTK_BOX (record_vbox), record_toolbar, FALSE, 
			 FALSE, 0);

     snprintf(rowstr, 40, "row %d of %d", row+1, rows);
     record_label = gtk_label_new(rowstr);
     gtk_widget_set_name (record_label, "record_label");
     gtk_widget_ref (record_label);
     gtk_object_set_data_full (GTK_OBJECT (record_window), "record_label", 
			       record_label,
			       (GtkDestroyNotify) gtk_widget_unref);
     gtk_widget_show (record_label);
     gtk_toolbar_append_widget (GTK_TOOLBAR (record_toolbar), 
				record_label, 
				"location of row in data", NULL);
     gtk_misc_set_padding (GTK_MISC(record_label), 15, 0);
 

     record_ok_button = gtk_button_new_with_label ("OK");
     gtk_widget_set_name (record_ok_button, "record_ok_button");
     gtk_widget_ref (record_ok_button);
     gtk_object_set_data_full (GTK_OBJECT (record_window), 
			       "record_ok_button", record_ok_button,
			       (GtkDestroyNotify) gtk_widget_unref);
     gtk_widget_show (record_ok_button);
     gtk_toolbar_append_widget (GTK_TOOLBAR (record_toolbar), 
				record_ok_button, 
				"Remove the data popup", NULL);
     gtk_widget_set_usize (record_ok_button, 70, -2);

     record_scroll = gtk_scrolled_window_new (NULL, NULL);
     gtk_widget_set_name (record_scroll, "record_scroll");
     gtk_widget_ref (record_scroll);
     gtk_object_set_data_full (GTK_OBJECT (record_scroll), 
			       "record_scroll", record_scroll,
			       (GtkDestroyNotify) gtk_widget_unref);
     gtk_widget_show (record_scroll);
     gtk_box_pack_start (GTK_BOX (record_vbox), record_scroll, TRUE, 
			 TRUE, 0);

     record_table = gtk_clist_new (2);
     gtk_widget_set_name (record_table, "record_table");
     gtk_widget_ref (record_table);
     gtk_object_set_data_full (GTK_OBJECT (record_window), 
			       "record_table", record_table,
			       (GtkDestroyNotify) gtk_widget_unref);
     gtk_widget_show (record_table);
     gtk_container_add (GTK_CONTAINER (record_scroll), record_table);
     gtk_clist_set_column_width (GTK_CLIST (record_table), 0, 80);
     gtk_clist_set_column_width (GTK_CLIST (record_table), 1, 220);
     gtk_clist_column_titles_show (GTK_CLIST (record_table));

     record_table_title = gtk_label_new ("title");
     gtk_widget_set_name (record_table_title, "record_table_title");
     gtk_widget_ref (record_table_title);
     gtk_object_set_data_full (GTK_OBJECT (record_window), 
			       "record_table_title", record_table_title,
			       (GtkDestroyNotify) gtk_widget_unref);
     gtk_widget_show (record_table_title);
     gtk_clist_set_column_widget (GTK_CLIST (record_table), 0, 
				  record_table_title);
     gtk_misc_set_alignment (GTK_MISC (record_table_title), 
			     7.45058e-09, 0.5);

     record_table_value = gtk_label_new ("value");
     gtk_widget_set_name (record_table_value, "record_table_value");
     gtk_widget_ref (record_table_value);
     gtk_object_set_data_full (GTK_OBJECT (record_window), 
			       "record_table_value", 
			       record_table_value,
			       (GtkDestroyNotify) gtk_widget_unref);
     gtk_widget_show (record_table_value);
     gtk_clist_set_column_widget (GTK_CLIST (record_table), 1, 
				  record_table_value);
     gtk_misc_set_alignment (GTK_MISC (record_table_value), 
			     7.45058e-09, 0.5);

     gtk_signal_connect (GTK_OBJECT (record_ok_button), "clicked",
			 GTK_SIGNAL_FUNC (gtkaction_distroy_record_window),
			 NULL);

     /* populate clist */
     itree_first(c_val);
     itree_traverse(c_title) {
	  clistrow[0] = itree_get(c_title);
	  clistrow[1] = itree_get(c_val);
	  gtk_clist_append(GTK_CLIST(record_table), clistrow);
	  itree_next(c_val);
     }
     gtk_widget_show(record_window);

     /* free data */
     itree_clearoutandfree(c_title);
     itree_clearoutandfree(c_val);
}



/* destroy the top level window point to be widget */
void gtkaction_distroy_record_window (GtkWidget *button,
				      gpointer user_data)
{
     GtkWidget *record_window;

     record_window = lookup_widget(GTK_WIDGET(button),"record_window");
     gtk_widget_hide (record_window);
     gtk_widget_destroy (record_window);
}



/*
 * Check to see if clockwork is running and if not, whether the user would 
 * like to start it. Look at the config for defaults and to govern questions
 */
void gtkaction_askclockwork()
{
     int pid, dontask, autorun;
     char *key;
     GtkWidget *w;

     pid = is_clockwork_running(&key, NULL, NULL, NULL);
     if (pid != 0) {
          elog_printf(INFO, "collecting local data with %s on pid %d", 
		      key, pid);
	  return;
     }
     
     autorun = cf_getint(iiab_cf, AUTOCLOCKWORK_CFNAME);
     if (autorun != CF_UNDEF && autorun != 0)
          gtkaction_startclockwork();
     else {
          dontask = cf_getint(iiab_cf, DONTASKCLOCKWORK_CFNAME);
	  if (dontask != CF_UNDEF && dontask != 0) {
	       /* Don't ask, dont start */
	       elog_printf(INFO, "local data not being collected "
			   "(not asking & not auto starting). "
			   "Choose 'Collect->Local Data' from the menu to "
			   "change your mind");
	  } else {
	       /* Ask to start */
	       w = create_start_clockwork_window();
	       gtk_widget_show(w);
	  }
	  return;
     }
}


/* Start clockwork running */
void gtkaction_startclockwork()
{
     int r;
     char *cmd;

     if (is_clockwork_runable()) {
          /* start clockwork daemon using system () */
          cmd = util_strjoin(iiab_dir_bin, "/clockwork", NULL);
	  elog_printf(INFO, "starting %s to collect local data", cmd);
	  r = system(cmd);
	  if (r == -1) {
	       elog_printf(ERROR, "problem starting collector: "
			   "not collecting data locally "
			   "(attempted %s)", cmd);
	       nfree(cmd);
	       return;
	  }
	  nfree(cmd);

	  elog_printf(INFO, "now collecting local data");
	  return;
     } else {
          elog_printf(ERROR, "couldn't find collector: "
		      "not collecting data locally");
     }

     return;
}

/* Stop a clockwork process started by this client. */
void gtkaction_stopclockwork()
{
     int r;
     char *cmd;

     /* stop clockwork daemon using system () */
     cmd = util_strjoin(iiab_dir_bin, "/killclock >/dev/null", NULL);
     elog_printf(INFO, "stopping local data collection with %s", cmd);
     r = system(cmd);
     if (r == -1) {
          elog_printf(ERROR, "unable to stop local data collection "
		      "(attempted %s)", cmd);
	  nfree(cmd);
	  return;
     }
     nfree(cmd);

     return;
}


/*
 * Search for a help file in standard locations and run a web browser
 * on the discovered location.
 * Returns 1 for success or 0 for failure: either no file or no browser.
 */
int gtkaction_browse_help(char *helpfile)
{
     char *url, *file;
     int r;

     /* find the help file in the built location */
     file = util_strjoin(iiab_dir_lib, HELP_BUILT_PATH, helpfile, NULL);
     if (access(file, R_OK) != 0) {
	  /* no files in the built location, try the development place */
	  elog_printf(INFO, "unable to show help %s", file);
	  nfree(file);
	  file = util_strjoin(iiab_dir_bin, HELP_DEV_PATH, helpfile,
			      NULL);
     }
     if (access(file, R_OK) != 0) {
	  /* no files in the dev location either; abort */
	  elog_printf(ERROR, "unable to show help %s", file);
	  nfree(file);
	  return 0;	/* failure */
     }

     /* convert file into a url for the browser and display */
     url = util_strjoin("file://localhost", file, NULL);
     nfree(file);
     r = gtkaction_browse_web(url);

     /* clear up and return */
     nfree(url);
     return r;
}


/*
 * Search for a man file in standard locations and run a web browser
 * on the discovered location.
 * Returns 1 for success or 0 for failure: either no file or no browser.
 */
int gtkaction_browse_man(char *manpage)
{
     char *url, *file;
     int r;

     /* find the help file in the system location (for linux) */
     file = util_strjoin(iiab_dir_lib, MAN_BUILT_PATH, manpage, NULL);
     if (access(file, R_OK) != 0) {
	  /* no files in the built location, try the development place */
	  elog_printf(INFO, "unable to show manpage %s (%s)", manpage, file);
	  nfree(file);
	  file = util_strjoin(iiab_dir_bin, MAN_DEV_PATH, manpage, NULL);
     }
     if (access(file, R_OK) != 0) {
	  /* no files in the dev location either; abort */
	  elog_printf(ERROR, "unable to show manpgage %s (%s)", manpage, 
		      file);
	  nfree(file);
	  return 0;	/* failure */
     }

     /* convert file into a url for the browser and display */
     url = util_strjoin("file://localhost", file, NULL);
     nfree(file);
     r = gtkaction_browse_web(url);

     /* clear up and return */
     nfree(url);
     return r;
}


/*
 * Launch a browser using the url given. Returns 1 if successful or 0 if 
 * failed, such as not finding the correct browser.
 */
int gtkaction_browse_web(char *url)
{
     char **b, *pathenv, *match, cmd[PATH_MAX+1024];
     int r;

     /* find a valid and executable browser, in a determined order;
      * basically, the best first and the last resort trailing up
      * the rear */
     pathenv = getenv("PATH");
     for (b = gtkaction_browsers; *b; b++) {
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
		    elog_printf(INFO, "starting browser...");
		    snprintf(cmd, PATH_MAX+1024, "%s %s &", match, url);
		    r = system(cmd);
		    nfree(match);
		    if (r == -1) {
			 elog_printf(ERROR, "unable to run browser");
			 return 0;	/* fail - browser not worked */
		    } else {
			 return 1;	/* success */
		    }
	       }
	  }
	  nfree(match);
     }

     return 0;		/* fail - run out of browsers */
}


/* 
 * Convert a RESDAT structure to a single table.
 * If a single table already exists, then it will be passed back;
 * if a list of tables exists then the list will be iterated and 
 * a new table created.
 * Returns the TABLE if possible or NULL if no data is available.
 * If the input RESDAT has rdat.t=TRES_TABLE set, then DO NOT DESTROY.
 * If rdat.t=TRES_TABLELIST is set, then free the table with tree_destroy().
 */
TABLE gtkaction_resdat2table(RESDAT rdat)
{
     TABLE tab;

     /* convert the RESDAT format into a simple table for exporting */
     if (rdat.t == TRES_NONE) {
          return NULL;
     }
     if (rdat.t == TRES_TABLELIST) {
          tab = table_create();
	  itree_traverse(rdat.d.tablst)
	       table_addtable(tab, (TABLE) itree_get(rdat.d.tablst), 1);
     } else {
          tab = rdat.d.tab;
     }

     return tab;
}


/* 
 * Convert the RESDAT structure to text, which should be free'ed with
 * nfree(). Then various options may remove columns from the RESDAT
 * structure and thus alter the input data. Make sure you use it 
 * with throw away data.
 * Returns NULL for error
 */
char *gtkaction_resdat2text(RESDAT rdat, int withtime, int withseq, 
			    int withtitle, int withruler, int createcsv)
{
     TABLE tab;
     char *buf;

     /* get a single table */
     tab = gtkaction_resdat2table(rdat);

     /* process the table in the light of the switches and 
      * transform into text */
     if (withtime == 0) {
          table_rmcol(tab, "_time");
     } else {
          table_renamecol(tab, "_time", "time");
     }
     if (withseq == 0)
          table_rmcol(tab, "sequence");
     buf = table_outtable_full(tab, (createcsv ? ',' : '\t'), 
			       withtitle, withruler);

     if (datapres_data.t == TRES_TABLELIST)	/* clear up working table */
          table_destroy(tab);

     return buf;
}


