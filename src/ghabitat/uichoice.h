/*
 * Habitat
 * GUI independent presentation and selection of choice
 * Designed to be used in conjunction with uidata:: to extract data.
 * Uichoice:: should be called by specific GUI toolkits, which
 * will place the information into a single tree widget
 *
 * Nigel Stuckey, May 1999
 * Copyright System Garden 1999-2001. All rights reserved.
 */

#ifndef _UICHOICE_H_
#define _UICHOICE_H_

#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/cf.h"
#include "uidata.h"

enum uichoice_icontype {
     UI_ICON_NONE,
     UI_ICON_HOL,
     UI_ICON_RING,
     UI_ICON_SPAN,
     UI_ICON_TABLE,
     UI_ICON_VERSION,
     UI_ICON_GRAPH,
     UI_ICON_ERROR,
     UI_ICON_HOME,
     UI_ICON_FILE,
     UI_ICON_NET,
     UI_ICON_SYSGAR,
     UI_ICON_UPTIME,
     UI_ICON_BNECK,
     UI_ICON_SERVICE,
     UI_ICON_TREND,
     UI_ICON_RAW,
     UI_ICON_LOG,
     UI_ICON_ROUTE,
     UI_ICON_QUALITY,
     UI_ICON_JOB,
     UI_ICON_WATCH,
     UI_ICON_EVENT,
     UI_ICON_CPU,
     UI_ICON_CSV,
     UI_ICON_DISK,
     UI_ICON_NETPERF,
     UI_ICON_REP
};

struct uichoice_node;
struct uichoice_feature;

/* describes the interface independent choice node */
struct uichoice_node {
     char *label;	/* name that appears on tree node */
     char *info;	/* text that appears on tool tips */
     char *help;	/* long text or url pointer to give help */
     int enabled;	/* non-0=choice enabled, 0=disabled/greyed out */
     enum uidata_type presentation;	/* data display type */
     enum uichoice_icontype icon;	/* icon type (optional) */
     TREE *(*initnodeargs)(struct uichoice_node *); /* set up node args */
     TREE *nodeargs;			/* node argument list key and data
					 * always a pointer; NULL for empty */
     RESDAT (*getdata)(TREE*);		/* standard get data for node */
     time_t datatime;			/* time data was collected */
     int datatimeout;			/* seconds to refresh data 0=don't */
     ITREE *children;			/* list of statically allocated 
					 * children, each of type 
					 * uichoice_node */
     struct uichoice_feature *features; /* children prototype */
     ITREE *(*dynchildren)(TREE*);	/* function that returns a list of
					 * dynamic children of type 
					 * uichoice_node, or NULL if there
					 * are no dynamic children */
     ITREE *dyncache;			/* cache of last dynamic child list */
     time_t dyntime;			/* time dynamic cache was updated */
     int dyntimeout;			/* seconds to refesh dynamic
					 * children 0=don't */
     struct uichoice_node *parent;	/* pointer to parent */
     int is_dynamic:1;	/* 1=I am a dynamically generated node, 0=static */
     int is_editable:1;	/* 1=choice may cause changes, 0=view only */
     int is_expanded:1; /* 1=children built, details visible, 0=invisible */
     int features_expanded:1; /* has had static children expanded */
};

/*
 * The arguments for the staticly generated tree are:-
 *   label     - text to appear on the menu
 *   key       - unique label key (used to collect children)
 *   parent    - parent's unique key
 *   info/tooltip  - tooltip string when mouse is over label
 *   help      - help string
 *   enable    - 0=disabled, not responsive to mouse clicks, 1=enabled
 *               can be altered when manually adding options
 *   GUI       - type of data visualisation when clicked
 *   ICON      - the tree icon next to the label
 *   features  - prototype of children (line similar to this)
 *   dynchild  - dynamic children: function that returns a list of child
 *               node: ITREE *()(TREE *nodeenv)
 *   dynre     - update frequency of dynamic children 0=never
 *   getdata   - data gathering routine: RESDAT ()(TREE *nodeenv)
 *               data returned in RESDAT is visualised
 *   datare    - update frequency of data 0=never
 *   initarg   - function returning environment for this node:
 *               TREE *(node)
 */

/* All choice items flat and their relationships are built up using key 
 * matching. This struture defines that relationship */
struct uichoice_feature {
     char *label;	/* name that appears on the gui */
     char *key;		/* id */
     char *parentkey;	/* id of tree partent */
     char *info;	/* information about the cpability */
     char *help;	/* long help information, maybe containing a url */
     int enabled;	/* 1=choice enabled, 0=disabled/greyed out */
     enum uidata_type         presentation;	/* data display type */
     enum uichoice_icontype   icon;		/* icon type (optional) */
     struct uichoice_feature *features;		/* to make static children */
     ITREE *(*dynchildren)(TREE*);	/* list returning dynamic children */
     int dyntimeout;			/* dyn children refresh (secs) 0=dont*/
     RESDAT (*getdata)(TREE *);		/* get data for node */
     int datatimeout;			/* how often to refresh data 0=dont */
     TREE *(*initnodeargs)(struct uichoice_node *); /* set up node args */
};


/* functional prototypes */
void   uichoice_init(CF_VALS);
void   uichoice_fini();
ITREE *uichoice_gettopnodes();
void   uichoice_addtopnodes(struct uichoice_node *node);
void   uichoice_addtopnodes_list(ITREE *nodelist);
struct uichoice_node *uichoice_mknode(char *label, 
				      char *info,
				      char *help,
				      int enabled,
				      enum uidata_type presentation,
				      enum uichoice_icontype icon,
				      struct uichoice_feature *features,
				      ITREE *(*dynchildren)(TREE *),
				      int dyntimeout,
				      RESDAT (*getdata)(TREE *),
				      int datatimeout,
			      TREE *(*initnodeargs)(struct uichoice_node *) );
void   uichoice_rmnode(struct uichoice_node *node);
void   uichoice_rmnodetree(ITREE *tree);
void   uichoice_addchild(struct uichoice_node *parent, 
			 struct uichoice_node *child);
void   uichoice_rmchild(struct uichoice_node *child);
int    uichoice_isancestor(struct uichoice_node *maybeancestor,
			   struct uichoice_node *child);
struct uichoice_node *uichoice_findlabel(struct uichoice_node *node, 
					 char *label);
struct uichoice_node *uichoice_findlabel_all(char *label);
void   uichoice_findleafnodes(struct uichoice_node *node, ITREE *list);
char  *uichoice_nodepath(struct uichoice_node *node, char *sep);
void   uichoice_dumpnodes(ITREE *tree, int indentsz);
void   uichoice_printnodes(ITREE *tree, int level);
void   uichoice_seteditable(struct uichoice_node *node, int is_editable);
void   uichoice_setdynamic(struct uichoice_node *node, int is_dynamic);
void   uichoice_putnodearg_str(struct uichoice_node *node, char *key, 
			       char *val);
void   uichoice_putnodearg_mem(struct uichoice_node *node, char *key,
			       void *mem, int size);
void * uichoice_getnodearg(struct uichoice_node *node, char *key);
void   uichoice_rmnodearg(struct uichoice_node *node, char *key);
int    uichoice_addfeatures(struct uichoice_node *node, 
			    struct uichoice_feature *features,
			    char *parentkey);
ITREE *uichoice_mknodelist_from_table(TABLE tab,
				      char *parentkey,
				      enum uidata_type presentation,
				      enum uichoice_icontype icon,
				      struct uichoice_feature *features,
				      ITREE *(*dynchildren)(TREE *),
				      int dyntimeout,
				      RESDAT (*getdata)(TREE *),
				      int datatimeout);
void   uichoice_getinheritedargs(struct uichoice_node *node, TREE *list);
ITREE *uichoice_gendynamic(struct uichoice_node *node, TREE *inheritedargs);
void   uichoice_freedynamic(struct uichoice_node *node);
int    uichoice_updatedynamic(struct uichoice_node *node);
void   uichoice_expandnode(struct uichoice_node *node);
void   uichoice_collapsenode(struct uichoice_node *node);

#endif /* _UICHOICE_H_ */
