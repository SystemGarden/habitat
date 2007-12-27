/*
 * Habitat Gtk GUI implementation
 *
 * Nigel Stuckey, May 1999
 * Copyright System Garden 1999-2001. All rights reserved.
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include "uichoice.h"
#include "uidata.h"
#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/elog.h"

#define GTKACTION_NTREELEV 2
#define GTKACTION_SHEET4PICK 0		/* 0=GtkCList, 1=GtkSheet */
#define GTKACTION_TREE4PICK 1		/* 0=GtkCList, 1=GtkTree */
#define GTKACTION_UICHOICEKEY "uichoice_node"
#define GTKACTION_GUIITEMKEY "gtktreeitem"
#define GTKACTION_PRESTIMEOUT 15000	/* 15000 ms => 15 seconds */
#define GTKACTION_CF_CURVES "gtkaction.curves"

extern FILE *clockwork_fstream;		/* clockwork read stream */
extern GtkWidget *datapres_widget;	/* gtk descriptor of current widget */
extern RESDAT datapres_data;		/* data for current widgets */
extern struct uichoice_node *datapres_node; /* the node to which the widget is 
					     * applied */


typedef struct {
     GtkWidget *on;	/* on image widget */
     GtkWidget *off;	/* off image widget */
     GtkWidget *label;	/* label widget */
     char *colname;	/* column name */
     GdkGC *bg_gc;	/* colour allocation for button */
     int state;		/* is the button up or down? */
     GtkWidget *scale;	/* scale widget next door (or NULL for unallocated) */
     GtkWidget *offset;	/* offset widget next door (or NULL for unallocated) */
     float max;		/* maximum value if != 0.0 */
} toggle_state;

void gtkaction_init();
void gtkaction_fini();
void gtkaction_configure(CF_VALS cf);
void gtkaction_createicons();
void gtkaction_setwmicon(GdkWindow *w, GdkPixmap *pixmap, GdkBitmap *mask);
GtkCTreeNode *
     gtkaction_makechoice(GtkCTreeNode *parent, struct uichoice_node *node, 
			  GtkTooltips *);
void gtkaction_deletechoice(GtkCTree *tree, GtkCTreeNode *treeitem);
void gtkaction_parentchoice(GtkCTree *parent, GtkWidget *treenode);
void gtkaction_expandlist(GtkCTreeNode *treeitem, int nlayers, GtkTooltips *);
void gtkaction_expandchoice(GtkCTreeNode *treeitem, int nlayers, 
			    GtkTooltips *tip);
void gtkaction_contractchoice(GtkCTreeNode *treeitem);
void gtkaction_gotochoice(struct uichoice_node *node, int level);
void gtkaction_choice_sync(GtkCTree *choice, char *nodelabel);
void gtkaction_choice_select(GtkCTreeNode *treeitem, gpointer user_data);
void gtkaction_choice_deselect();
void gtkaction_choice_update_start();
void gtkaction_choice_update_stop();
int  gtkaction_choice_updateifneeded();
void gtkaction_node_update();
void gtkaction_choice_update();
GtkWidget *gtkaction_mktable(RESDAT dres);
#if 0
gint gtkaction_table_select (GtkWidget *widget, GdkEventButton *event, 
			     gpointer func_data);
#endif
void gtkaction_table_select (GtkWidget *widget, gint row, gint column,
			     GdkEventButton *event, gpointer data);
GtkWidget *gtkaction_mkgraphinst(RESDAT dres);
void gtkaction_rmgraphinst();
void gtkaction_graphinst_clicked(GtkWidget *widget, char *);
GtkWidget *gtkaction_mkgraphattr(RESDAT dres);
void gtkaction_rmgraphattr();
void gtkaction_graphattr_morewidgets(GtkTable *wtable, RESDAT dres);
void gtkaction_graphattr_lesswidgets(GtkTable *wtable);
void gtkaction_graphattr_select(GtkWidget *widget, toggle_state *s);
void gtkaction_graphattr_scale(GtkWidget *widget, toggle_state *s);
void gtkaction_graphattr_offset(GtkWidget *widget, toggle_state *s);
void gtkaction_graphattr_redraw(RESDAT dres);
GdkColor *gtkaction_drawcurve(RESDAT dres, char *curve, float scale, 
			      float offset);
void gtkaction_updateall();
void gtkaction_drawgraph(RESDAT dres, char *instance);
void gtkaction_mkedtree(RESDAT dres);
void gtkaction_mkedtreerow(GtkTooltips *tips, GtkWidget *subtree, 
			   char *prompt, TABLE tab, int is_value_insert);
void gtkaction_rmedtree();
void gtkaction_edtree_update_cb (GtkButton       *button,
				 gpointer         user_data);
void gtkaction_edtree_abort_cb (GtkButton       *button,
				gpointer         user_data);
void gtkaction_edtree_create_cb (GtkButton       *button,
				 gpointer         user_data);
void gtkaction_elog_raise(const char *errtext, int etlen);
void gtkaction_setprogress(char *text, float percent, int showpercent);
gint gtkaction_sigprogress(gpointer data);
void gtkaction_clearprogress();
void gtkaction_log_popup_init();
void gtkaction_log_popup_created(GtkWidget *w);
GtkWidget *gtkaction_log_popup_available();
void gtkaction_log_popup_destroyed();
void gtkaction_anypopup_setwmicon(GtkWidget *w);
void gtkaction_log_popup_draw(GtkWidget *clist, enum elog_severity sev, 
			      int coloured);
void gtkaction_log_popup_dline(char ecode, time_t time, char *sev, 
			       char *file, char *func, char *line, 
			       char *text);
void gtkaction_log_popup_state(enum elog_severity *sev, int *coloured);
void gtkaction_create_record_window(char *w_title, int row, int rows, 
				    ITREE *c_title, ITREE *c_val);
void gtkaction_distroy_record_window(GtkWidget *widget, gpointer user_data);
void gtkaction_askclockwork();
void gtkaction_startclockwork();
void gtkaction_stopclockwork();
int  gtkaction_browse_help(char *helpfile);
int  gtkaction_browse_man(char *manpage);
int  gtkaction_browse_web(char *url);
TABLE gtkaction_resdat2table(RESDAT rdat);
char *gtkaction_resdat2text(RESDAT rdat, int withtime, int withseq, 
			    int withtitle, int withruler, int createcsv);
