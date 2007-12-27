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

#ifndef _GHCHOICE_H_
#define _GHCHOICE_H_

#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/cf.h"
#include "uidata.h"

#define GHCHOICE_CF_MYFILES_LOAD "ghchoice.myfiles.load"
#define GHCHOICE_CF_MYFILES_LIST "ghchoice.myfiles.list"
#define GHCHOICE_CF_MYHOSTS_LOAD "ghchoice.myhosts.load"
#define GHCHOICE_CF_MYHOSTS_LIST "ghchoice.myhosts.list"


/*
 * Choice is resented in the form of a tree, which has the general layout:-
 *
 *   group --- control item --- features
 *
 * where a group is a collection related to a purpose, such as
 * hardware, software or service levels. The items in the group may
 * be repeated elsewhere in the choices.
 * Control item is a machine or peice of software.
 * A feature is information that you can see in that item, such as 
 * its confuration, control of specific services or views of data.
 * Below this, there may be feature specific choices.
 *
 * Here are some examples:-
 *
 *   Hardware -+- "machine1" -+- "configuration"
 *             |              +- "raw data" -+- "holstore"
 *             |              |              +- "timestore" -+- "ring1"
 *             |              |              |               +- "ring2"
 *             |              |              +- "spanstore" -+- "seq1 to seq2"
 *             |              |              |               +- "seq2 to seq3"
 *             |              |              +- "tablestore" -+- "table1"
 *             |              |              |                +- "table2"
 *             |              |              +- "versionstore" -- object -- "1"
 *             |              |              +- "jobs"
 *             |              +- "probes"
 *             |              +- "graphing" --- "table1" -+- "5 minutes"
 *             |              |                           +- "15 minutes"
 *             |              +- "trends" -+- extrapolate -+- "hour"
 *             |              |            |               +- "6 hours"
 *             |              |            |               +- "day"
 *             |              |            |               +- "week"
 *             |              |            |               +- "month"
 *             |              |            |               +- "year"
 *             |              +            +- cyclic
 *             |              +- "service levels"
 *             + "machine2" ...etc...
 *   Service --- "service1" -+- "config levels"
 *                           +- "probes"
 *                           +- "raw data"
 *                           +- "graph"
 *                           +- "trend"
 *                           +- "service levels"
 */

/* Structure used to specify the standard length time periods to choose
 * when extracting consolidated data */
struct ghchoice_timebase {
     char *label;	/* text label for this timebase */
     int secs;		/* number of seconds for this timebase */
     int enabled;	/* enabled flag */
     int refresh;	/* seconds until refresh */
};

/* externs */
extern struct uichoice_node *ghchoice_myfilenode; /* my file node pointer */

/* functional prototypes */
void   ghchoice_init(CF_VALS);
void   ghchoice_fini();
TREE * ghchoice_get_myfiles_load();
void   ghchoice_add_myfiles_load(char *fname);
void   ghchoice_add_myfiles_load_tree(TREE *new);
void   ghchoice_rm_myfiles_load(char *fname);
TREE * ghchoice_get_myfiles_list();
void   ghchoice_add_myfiles_list(char *fname);
void   ghchoice_add_myfiles_list_tree(TREE *new);
TREE * ghchoice_get_myhosts_load();
void   ghchoice_add_myhosts_load(char *hostname, char *purl);
void   ghchoice_add_myhosts_load_tree(TREE *new);
void   ghchoice_rm_myhosts_load(char *hostname);
TREE * ghchoice_get_myhosts_list();
void   ghchoice_add_myhosts_list(char *hostname, char *purl);
void   ghchoice_add_myhosts_list_tree(TREE *new);
void   ghchoice_cfsave(CF_VALS cf);
void   ghchoice_configure(CF_VALS cf);
struct uichoice_node *ghchoice_loadfile(char *fname,
					struct uichoice_node *parent,
					int *status);
int    ghchoice_unloadfile(char *fname);
TREE  *ghchoice_getloadedfiles();
struct uichoice_node *ghchoice_loadroute(char *purl, char *label,
					 struct uichoice_node *parent,
					 int *status);
struct uichoice_node *ghchoice_loadrepository(char *purl,
					      struct uichoice_node *node,
					      int *status);
struct uichoice_node *ghchoice_initialview();

/* depricated, but I just can't let go ... */
ITREE *ghchoice_edpatactionchild(TREE *nodeargs);
ITREE *ghchoice_recsrcchild(TREE *nodeargs);

/* new route */
TREE * ghchoice_arg_begin_log  (struct uichoice_node *node);
TREE * ghchoice_arg_begin_rep  (struct uichoice_node *node);
TREE * ghchoice_arg_begin_patact   (struct uichoice_node *node);
TREE * ghchoice_arg_begin_event    (struct uichoice_node *node);
TREE * ghchoice_arg_begin_watched  (struct uichoice_node *node);
TREE * ghchoice_arg_begin_up       (struct uichoice_node *node);
TREE * ghchoice_args_err           (struct uichoice_node *node);
TREE * ghchoice_args_rstate        (struct uichoice_node *node);
TREE * ghchoice_args_perf          (struct uichoice_node *node);
TREE * ghchoice_args_thishost      (struct uichoice_node *node);
ITREE *ghchoice_tree_ring_tab      (TREE *nodeargs);
ITREE *ghchoice_tree_ring_graph    (TREE *nodeargs);
ITREE *ghchoice_tree_consring_tab  (TREE *nodeargs);
ITREE *ghchoice_tree_consring_graph(TREE *nodeargs);
ITREE *ghchoice_tree_group_tab     (TREE *nodeargs);
ITREE *ghchoice_tree_hostgroup_tab (TREE *nodeargs);
ITREE *ghchoice_tree_seqs_tab      (TREE *nodeargs);
ITREE *ghchoice_tree_recent_tab    (TREE *nodeargs);
ITREE *ghchoice_tree_ringdur_tab   (TREE *nodeargs);

#endif /* _GHCHOICE_H_ */
