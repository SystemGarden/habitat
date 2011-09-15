/*
 * Gtk multicurve graph widget
 * This is a wrapper over the gtkplot for convenience
 *
 * Nigel Stuckey, September 1999, October 2010
 * Copyright System Garden Limited 1999-2001,2010. All rights reserved.
 */

#ifndef _GRAPHDBOX_H_
#define _GRAPHDBOX_H_

#include <gtk/gtk.h>
#include <gtkdatabox.h>
#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"

#define GRAPHDBOX_NCOLOURS 41
#define GRAPHDBOX_FIRSTTIME 800000000L
#define GRAPHDBOX_DEFGRAPHNAME "default"
#define GRAPHDBOX_SHOWRULERS_CFNAME "graph.showrulers"
#define GRAPHDBOX_SHOWAXIS_CFNAME   "graph.showaxis"
#define GRAPHDBOX_DRAWSTYLE_CFNAME  "graph.drawstyle"
#define GRAPHDBOX_UP_IMG            "pixmaps/arrow-btn-up-12.png"
#define GRAPHDBOX_DOWN_IMG          "pixmaps/arrow-btn-down-12.png"
#define GRAPHDBOX_LEFT_IMG          "pixmaps/arrow-btn-left-12.png"
#define GRAPHDBOX_RIGHT_IMG         "pixmaps/arrow-btn-right-12.png"

/* instance column names */
enum graphdbox_inst_cols {
     GRAPHDBOX_INST_ICON,
     GRAPHDBOX_INST_ACTIVE,
     GRAPHDBOX_INST_BUTTON,
     GRAPHDBOX_INST_STATE,
     GRAPHDBOX_INST_EOL
};

/* data sense */
enum graphdbox_datasense {
     GRAPHDBOX_CNT, 	/* counter, differences should be plotted */
     GRAPHDBOX_ABS	/* absolute, the values should be plotted */
};

/* curve rendering type */
enum graphdbox_graphtype {
     GRAPHDBOX_THINLINE,	/* one pixel line (2 pixel) */
     GRAPHDBOX_MIDLINE,		/* medium line (2 pixels) */
     GRAPHDBOX_FATLINE,		/* fatter line (3 pixels) */
     GRAPHDBOX_POINT,		/* data points */
     GRAPHDBOX_BAR,		/* histogram lines */
     GRAPHDBOX_TEXT,		/* text points */
     GRAPHDBOX_GTYPE_EOL
};

struct graphdbox_curve {
     GtkDataboxGraph *dbgraph;		/* curve/graph widget */
     float *X, *Y;			/* nmalloc'ed allocations for data */
     int colour;			/* colour index */
     enum graphdbox_graphtype style;	/* how curve is drawn */
};

struct graphdbox_graph {
     GtkWidget *gdbox;			/* GtkDatabox widget */
     GtkWidget *gtable;			/* GtkTable containing Databox widget */
     TREE *curves;			/* list of curve names drawn: key is
					 * curve name, data graphdbox_graph */
     enum graphdbox_graphtype style;	/* how to draw curves */
     float minmax;			/* min max on y-axis */
     struct graphdbox_all *parent;	/* parent pointer to graphdbox_all */
};

struct graphdbox_all {
     TREE *graphs;		/* list of graph structures: 
				 * key is graph name, data graphdbox_graph */
     GtkVBox *container;	/* containing widget */
     time_t start, end;		/* timebase boundary values. 
				 * these are real times based on the epoch */
     TREE *curvecol;		/* curve name to colour lookup */
     ITREE *colunused;		/* previously used colours, now unused */
     int nextcol;		/* start of 'virgin', unused colours */
};

typedef struct graphdbox_all GRAPHDBOX;

GRAPHDBOX *graphdbox_create(GtkVBox *box);
void graphdbox_destroy(GRAPHDBOX *g);
GdkColor *graphdbox_draw(GRAPHDBOX *g, char *graph_name, char *curve_name,
			 int nvals, float *xcols, float *ycols, char **text,
			 int overwrite);
struct graphdbox_graph *graphdbox_newgraph(GRAPHDBOX *g, char *graph_name,
					   enum graphdbox_graphtype type);
void
graphdbox_create_box_with_scrollbars_and_rulers (GtkWidget ** p_box,
						 GtkWidget ** p_table,
						 gboolean scrollbar_x,
						 gboolean scrollbar_y,
						 gboolean ruler_x,
						 gboolean ruler_y);
int  graphdbox_iscurvedrawn(GRAPHDBOX *g, char *graph_name, char *curve_name);
int  graphdbox_iszoomed(struct graphdbox_graph *gs);
void graphdbox_update(GRAPHDBOX *g, char *graph_name);
void graphdbox_settimebase(GRAPHDBOX *g, time_t min, time_t max);
void graphdbox_setallminmax(GRAPHDBOX *g, float value);
void graphdbox_updateaxis(struct graphdbox_graph *gs);
void graphdbox_updateallaxis(GRAPHDBOX *g);
void graphdbox_rmcurve(GRAPHDBOX *g, char *graph_name, char *curve_name);
void graphdbox_rmcurveallgraphs(GRAPHDBOX *g, char *curve_name);
void graphdbox_rmallcurves(GRAPHDBOX *g, char *graph_name);
void graphdbox_rmgraph(GRAPHDBOX *g, char *graph_name);
void graphdbox_rmallgraphs(GRAPHDBOX *g);
struct graphdbox_graph *graphdbox_lookupgraph(GRAPHDBOX *g, char *graph_name);
struct graphdbox_curve *graphdbox_lookupcurve(GRAPHDBOX *g, char *graph_name, 
					      char *curve_name);
void graphdbox_allgraph_zoomin_x(GRAPHDBOX *g, float zoomin);
void graphdbox_allgraph_zoomin_y(GRAPHDBOX *g, float zoomin);
void graphdbox_allgraph_zoomout(GRAPHDBOX *g);
void graphdbox_allgraph_zoomout_home(GRAPHDBOX *g);
gfloat graphdbox_majticks(gfloat max);
gfloat graphdbox_minticks(gfloat mix);
void graphdbox_hideallaxis(GRAPHDBOX *g);
void graphdbox_showallaxis(GRAPHDBOX *g);
void graphdbox_hideallrulers(GRAPHDBOX *g);
void graphdbox_showallrulers(GRAPHDBOX *g);
void graphdbox_allgraphstyle(GRAPHDBOX *g, enum graphdbox_graphtype);
void graphdbox_graphstyle(GRAPHDBOX *g, char *graph_name, 
			  enum graphdbox_graphtype);
void graphdbox_usecolour(GRAPHDBOX *g, char *curvename, GdkColor *col);
void graphdbox_recyclecolour(GRAPHDBOX *g, char *curvename);

#endif /* _GRAPHDBOX_H_ */
