/*
 * Gtk multicurve graph widget
 * This is a wrapper over the gtkplot for convenience
 *
 * Nigel Stuckey, September 1999
 * Copyright System Garden Limited 1999-2001. All rights reserved.
 */

#ifndef _GMCGRAPH_H_
#define _GMCGRAPH_H_

#include <gtk/gtk.h>
#include "gtkdatabox.h"
#include "../iiab/itree.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"
#include "uidata.h"

#define GMCGRAPH_NCOLOURS 41
#define GMCGRAPH_FIRSTTIME 800000000L
#define GMCGRAPH_DEFGRAPHNAME "default"

/* data sense */
enum gmcgraph_datasense {
     GMCGRAPH_CNT, 	/* counter, differences should be plotted */
     GMCGRAPH_ABS	/* absolute, the values should be plotted */
};

/* curve rendering type */
enum gmcgraph_graphtype {
     GMCGRAPH_THINLINE,	/* one pixel line (2 pixel) */
     GMCGRAPH_MIDLINE,	/* medium line (2 pixels) */
     GMCGRAPH_FATLINE,	/* fatter line (3 pixels) */
     GMCGRAPH_POINT,	/* data points */
     GMCGRAPH_BAR	/* histogram lines */
};

struct gmcgraph_curve {
     int index;			/* widget reference */
     float *X, *Y;		/* nmalloced memory allocations for data */
     int colour;		/* colour index */
     enum gmcgraph_graphtype style; /* how curve is drawn */
};

struct gmcgraph_graph {
     GtkWidget *widget;			/* GtkDatabox widget */
     TREE *curves;			/* list of curve names drawn: key is
					 * curve name, data gmcgraph_graph */
     enum gmcgraph_graphtype style;	/* how to draw curves */
     float minmax;			/* min max on y-axis */
     struct gmcgraph_all *parent;	/* parent pointer to gmcgraph_all */
};

struct gmcgraph_all {
     TREE *graphs;		/* list of graph structures: 
				 * key is graph name, data gmcgraph_graph */
     GtkWidget *container;	/* containing widget */
     time_t start, end;		/* timebase boundary values. 
				 * these are real times based on the epoch */
     TREE *curvecol;		/* curve name to colour lookup */
     ITREE *colunused;		/* previously used colours, now unused */
     int nextcol;		/* start of 'virgin', unused colours */
};

typedef struct gmcgraph_all GMCGRAPH;

GMCGRAPH *gmcgraph_init(GtkWidget *base_window, GtkWidget *hpane);
void gmcgraph_fini();
int  gmcgraph_iscurvedrawn(GMCGRAPH *g, char *graph_name, char *curve_name);
int  gmcgraph_resdat2arrays(GMCGRAPH *g, RESDAT rdat, char *colname,
			    char *keycol, char *keyval, 
			    float **xvals, float **yvals);
GdkColor *gmcgraph_draw(GMCGRAPH *g, char *graph_name, char *curve_name,
			int nvals, float *xcols, float *ycols, int overwrite);
int  gmcgraph_iszoomed(struct gmcgraph_graph *gs);
void gmcgraph_update(GMCGRAPH *g, char *graph_name);
struct gmcgraph_graph *gmcgraph_newgraph(GMCGRAPH *g, char *graph_name);
void gmcgraph_settimebase(GMCGRAPH *g, time_t min, time_t max);
int  gmcgraph_settimebasebynode(GMCGRAPH *g, TREE *nodearg);
void gmcgraph_setallminmax(GMCGRAPH *g, float value);
void gmcgraph_updateaxis(struct gmcgraph_graph *gs);
void gmcgraph_updateallaxis(GMCGRAPH *g);
void gmcgraph_rmcurve(GMCGRAPH *g, char *graph_name, char *curve_name);
void gmcgraph_hidecurve(GMCGRAPH *g, char *graph_name, char *curve_name);
void gmcgraph_showcurve(GMCGRAPH *g, char *graph_name, char *curve_name);
void gmcgraph_rmallcurves(GMCGRAPH *g, char *graph_name);
void gmcgraph_rmgraph(GMCGRAPH *g, char *graph_name);
void gmcgraph_rmallgraphs(GMCGRAPH *g);
struct gmcgraph_graph *gmcgraph_lookupgraph(GMCGRAPH *g, char *graph_name);
struct gmcgraph_curve *gmcgraph_lookupcurve(GMCGRAPH *g, char *graph_name, 
					    char *curve_name);
void gmcgraph_scale(GMCGRAPH *g, char *curve_name, float *scale_factor);
void gmcgraph_offset(GMCGRAPH *g, char *graph_name, char *curve_name, 
		     int offset, float factor);
void gmcgraph_regcurvedb(GMCGRAPH *g, char *curve_name, void (*clickcb)());
void gmcgraph_multiaxis(GMCGRAPH *g);
void gmcgraph_singleaxis(GMCGRAPH *g);
void gmcgraph_getcurveorder(GMCGRAPH *g);
void gmcgraph_setcurveorder(GMCGRAPH *g);
void gmcgraph_curvestyle(GMCGRAPH *g, char *curve_name, int fillarea);
gfloat gmcgraph_majticks(gfloat max);
gfloat gmcgraph_minticks(gfloat mix);
void gmcgraph_hideallaxis(GMCGRAPH *g);
void gmcgraph_showallaxis(GMCGRAPH *g);
void gmcgraph_hideallrulers(GMCGRAPH *g);
void gmcgraph_showallrulers(GMCGRAPH *g);
void gmcgraph_allgraphstyle(GMCGRAPH *g, enum gmcgraph_graphtype);
void gmcgraph_graphstyle(GMCGRAPH *g, char *graph_name, 
			 enum gmcgraph_graphtype);
void gmcgraph_allgraph_zoomin_x(GMCGRAPH *g);
void gmcgraph_allgraph_zoomin_y(GMCGRAPH *g);
void gmcgraph_allgraph_zoomout_x(GMCGRAPH *g);
void gmcgraph_allgraph_zoomout_y(GMCGRAPH *g);
void gmcgraph_usecolour(GMCGRAPH *g, char *curvename, GdkColor *col);
void gmcgraph_recyclecolour(GMCGRAPH *g, char *curvename);

#endif /* _GMCGRAPH_H_ */
