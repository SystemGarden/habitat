/*
 * Habitat GUI
 * Designed to be used in conjunction with uidata:: to extract data.
 * Uichoice:: should be called by specific GUI toolkits, which
 * will place the information into a single tree widget
 *
 * Nigel Stuckey, May, June, December 1999
 * Copyright System Garden 1999-2004. All rights reserved.
 */

#define _GNU_SOURCE

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "uichoice.h"
#include "ghchoice.h"
#include "../iiab/nmalloc.h"
#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/tableset.h"
#include "../iiab/elog.h"
#include "../iiab/util.h"
#include "../iiab/route.h"
#include "../iiab/rt_sqlrs.h"
#include "../iiab/rs.h"
#include "../iiab/rs_gdbm.h"
#include "../iiab/cf.h"
#include "../iiab/httpd.h"

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
 * A combined approach:-
 *
 * this client
 * my host
 * my files
 * other hosts
 * grouped hosts
 * my applications
 * grouped applications
 * my services
 * other services
 * grouped services
 * my errors
 * other errors
 * grouped errors
 * wizards
 *
 * Source led approach:-
 *
 * client --------- config, errors, discovered db, status
 * files ---------- my list
 * hosts ---------- local, networked (other hosts), managed (grouped hosts)
 * applications --- local, networked, centraly grouped
 * services ------- local, networked, centraly grouped [as in svc level]
 * errors --------- local, networked, centrally grouped
 * grouped -------- hosts, apps, services, errors
 * archive -------- central file list
 *
 * Function led approach:-
 *
 * config --------- my client, my machine, other machines
 * data ----------- local machines, etc...
 * 
 *
 *   My host --+- "machine1" -+- "configuration"
 *             |              +- "data" -+- "ring1
 *             |              |          +- "ring2" -+- "dur1"
 *             |              |          |           +- "dur2"
 *             |              |          +- "jobs"
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
 *   My services --- "service1" -+- "config levels"
 *                               +- "probes"
 *                               +- "raw data"
 *                               +- "graph"
 *                               +- "trend"
 *                               +- "service levels"
 */


/* standard features for ringstores */
struct uichoice_feature rsfeatures[] = {
/*  label                   key   parent info/tooltip         help   enable  GUI       ICON        features  dynchild dynre getdata datare initarg */
  {"uptime",                "up",  NULL, NULL,                "No help", 1, UI_TABLE,  UI_ICON_UPTIME,  NULL, NULL, 0, uidata_get_uptime, 300, NULL},
#if 0
  {"quality",               "qos", NULL, NULL,                "No help", 0, UI_SPLASH, UI_ICON_QUALITY, NULL, NULL,   0, NULL, 0, NULL},
#endif
  {"perf charts",           "gra", NULL, NULL,                "No help", 1, UI_SPLASH, UI_ICON_GRAPH,   NULL, ghchoice_tree_consring_graph, 0, uidata_get_route_cons, 0, ghchoice_args_perf},
  {"perf tables",           "dat", NULL, NULL,                "No help", 1, UI_SPLASH, UI_ICON_TABLE,   NULL, ghchoice_tree_consring_tab, 0, uidata_get_route_cons, 0, ghchoice_args_perf},
  {"events",                "evt", NULL, "Detected data",     "No help", 1, UI_SPLASH, UI_ICON_WATCH,   NULL, NULL,   0, NULL, 0, NULL},
   {"raised events",        "evm","evt", "Event commands raised","No help",1, UI_SPLASH,UI_ICON_LOG,    NULL, ghchoice_tree_ring_tab, 0, uidata_get_route, 0, ghchoice_arg_begin_event},
   {"pattern-actions",      "pat","evt", NULL,                "No help", 1, UI_SPLASH, UI_ICON_WATCH,   NULL, ghchoice_tree_ring_tab, 0, uidata_get_route, 0, ghchoice_arg_begin_patact},
   {"watched sources",      "sor","evt", NULL,                "No help", 1, UI_SPLASH, UI_ICON_WATCH,   NULL, ghchoice_tree_ring_tab, 0, uidata_get_route, 0, ghchoice_arg_begin_watched},
   {"watching jobs",        "job","evt", NULL,                "No help", 1, UI_TABLE,  UI_ICON_JOB,     NULL, ghchoice_tree_ring_tab, 0, uidata_get_route, 0, ghchoice_arg_begin_patact},
#if 0
  {"recording",             "rec", NULL, NULL,                "No help", 1, UI_SPLASH, UI_ICON_WATCH,   NULL, NULL,   0, NULL, 0, NULL},
   {"changes",              "cha","rec", "Recorded changes",  "No help", 1, UI_SPLASH, UI_ICON_ERROR,   NULL, ghchoice_recchangechild, 0, NULL, 0, NULL},
   {"recording jobs",       "job","rec", NULL,                "No help", 1, UI_TABLE,  UI_ICON_JOB,     NULL, NULL,   0, uidata_getrecjobs, 0, NULL},
   {"recorded sources",     "sor","rec", NULL,                "No help", 1, UI_SPLASH, UI_ICON_LOG,     NULL, ghchoice_recsrcchild, 0, NULL, 0, NULL},
#endif
  {"logs",                  "log", NULL, "Logs & errors",     "No help", 1, UI_SPLASH, UI_ICON_LOG,   NULL, NULL,   0, NULL, 0, NULL},
   {"logs",                 "llg","log", "Logs",              "No help", 1, UI_SPLASH, UI_ICON_LOG,     NULL, ghchoice_tree_ring_tab, 0, uidata_get_route, 0, ghchoice_arg_begin_log},
   {"errors",               "ler","log", "Error logs",        "No help", 1, UI_SPLASH, UI_ICON_ERROR,   NULL, ghchoice_tree_ringdur_tab, 0, uidata_get_route, 0, ghchoice_args_err},
  {"replication",           "rep", NULL, "Replication",       "No help", 1, UI_SPLASH, UI_ICON_REP,     NULL, NULL,   0, NULL, 0, NULL},
   {"log",                  "rlg","rep", "Replication logs",  "No help", 1, UI_SPLASH, UI_ICON_LOG,     NULL, ghchoice_tree_seqs_tab, 0, uidata_get_route, 0, ghchoice_arg_begin_rep},
  {"state",                 "rst","rep", "Replication state", "No help", 1, UI_TABLE,  UI_ICON_REP,     NULL, NULL,    0, uidata_get_route, 0, ghchoice_args_rstate},
  {"jobs",                  "job", NULL, "Job table",         "No help", 1, UI_TABLE,  UI_ICON_JOB,     NULL, NULL,   0, uidata_get_jobs, 300, NULL},
  {"data",                  "raw", NULL, "Unintrepreted data","No help", 1, UI_SPLASH, UI_ICON_RAW,     NULL, NULL,   0, NULL, 0, NULL},
   {"ringstore charts",     "rsc","raw", NULL,                "No help", 1, UI_SPLASH, UI_ICON_GRAPH,   NULL, ghchoice_tree_ring_graph, 0, uidata_get_route, 0, NULL},
   {"ringstore tables",     "rst","raw", NULL,                "No help", 1, UI_SPLASH, UI_ICON_TABLE,   NULL, ghchoice_tree_ring_tab, 0, uidata_get_route, 0, NULL},
#if 0
  {"service level",         "svc", NULL, "Service levels",    "No help", 0, UI_SPLASH, UI_ICON_SERVICE, NULL, NULL,   0, NULL, 0, NULL},
  {"trend",                 "trn", NULL, "Computed trends",   "No help", 0, UI_SPLASH, UI_ICON_TREND,   NULL, NULL,   0, NULL, 0, NULL},
  {"bottleneck",            "bot", NULL, "Bottlenecks",       "No help", 0, UI_SPLASH, UI_ICON_BNECK,   NULL, NULL,   0, NULL, 0, NULL},
  {"logs",                  "log", NULL, "View log messages", "No help", 0, UI_TABLE,  UI_ICON_LOG,     NULL, NULL,   0, uidata_getlocallogs, 0, NULL},
  {"administration",        "adm", NULL, "Administration",    "No help", 0, UI_SPLASH, UI_ICON_NONE,    NULL, NULL,   0, NULL, 0, NULL},
   {"edit pattern watching","epw","adm", NULL,                "No help", 0, UI_SPLASH, UI_ICON_LOG,     NULL, NULL,   0, NULL, 0, NULL},
    {"edit watching jobs",  "job","epw", NULL,                "No help", 1, UI_EDTREE, UI_ICON_JOB,     NULL, NULL,   0, uidata_getpatjobs, 0, NULL},
    {"edit pattern-actions","pat","epw", NULL,                "No help", 1, UI_SPLASH, UI_ICON_LOG,     NULL, ghchoice_edpatactionchild, 0, NULL, 0, NULL},
    {"edit watched sources","sor","epw", NULL,                "No help", 1, UI_SPLASH, UI_ICON_LOG,     NULL, ghchoice_edpatsrcchild, 0, NULL, 0, NULL},
   {"edit route recording", "err","adm", NULL,                "No help", 0, UI_SPLASH, UI_ICON_LOG,     NULL, NULL,   0, NULL, 0, NULL},
    {"edit recording jobs", "job","err", NULL,                "No help", 1, UI_EDTREE, UI_ICON_JOB,     NULL, NULL,   0, uidata_getrecjobs, 0, NULL},
    {"edit recorded sources","sor","err",NULL,                "No help", 1, UI_SPLASH, UI_ICON_LOG,     NULL, ghchoice_edrecsrcchild, 0, NULL, 0, NULL},
  {"logging",               "log", NULL, "Logging routes",    "No help", 0, UI_SPLASH, UI_ICON_ROUTE,   NULL, NULL,   0, NULL, 0, NULL},
#endif
  {NULL,                     NULL, NULL, NULL,                NULL,      0, UI_SPLASH, UI_ICON_NONE,    NULL, NULL,   0, NULL, 0, NULL},
};


/* standard features for files */
struct uichoice_feature filefeatures[] = {
/*  label                   key   parent info/tooltip         help   enable  GUI       ICON        features  dynchild dynre getdata datare initarg */
  {"perf charts",           "pfc", NULL, NULL,                "No help", 1, UI_GRAPH, UI_ICON_GRAPH,   NULL, NULL,   0, uidata_get_file, 0, ghchoice_args_perf},
  {"perf tables",           "pft", NULL, NULL,                "No help", 1, UI_TABLE, UI_ICON_TABLE,   NULL, NULL,   0, uidata_get_file, 0, ghchoice_args_perf},
  {NULL,                     NULL, NULL, NULL,                NULL,      0, UI_SPLASH, UI_ICON_NONE,    NULL, NULL,   0, NULL, 0, NULL},
};


/* top level features */
struct uichoice_feature topfeatures[] = {
/*  label              key   parent info/tooltip          help   enable  GUI      ICON  features dynchild dynre getdata datare initarg */
  {"this host",       "this", NULL, NULL,              "No help", 1, UI_TABLE, UI_ICON_HOME,  rsfeatures, NULL, 0, uidata_get_hostinfo, 0, ghchoice_args_thishost},
#if 0
   {"perf charts",    "gra", "this","performance charts","No help",1,UI_SPLASH, UI_ICON_GRAPH, NULL, ghchoice_tree_consring_graph, 0, uidata_get_route_cons, 0, ghchoice_args_perf},
#endif
  {"my files",        "file", NULL, NULL,              "No help", 0, UI_SPLASH, UI_ICON_FILE,  NULL, NULL, 0, NULL, 0, NULL},
  {"my hosts",        "mhos", NULL, NULL,              "No help", 0, UI_SPLASH, UI_ICON_NET,   NULL, NULL, 0, NULL, 0, NULL},
  {"repository",      "rep",  NULL, NULL,              "No help", 1, UI_SPLASH, UI_ICON_NET,   NULL, ghchoice_tree_group_tab, 86400, NULL, 0, NULL},
#if 0
  {"my services",     "mserv",NULL, NULL,              "No help", 0, UI_SPLASH, UI_ICON_NET,   NULL, NULL, 0, NULL, 0, NULL},
  {"grouped services","gserv",NULL, NULL,              "No help", 0, UI_SPLASH, UI_ICON_NET,   NULL, NULL, 0, NULL, 0, NULL},
#endif
  {"this client",     "clnt", NULL, NULL,              "No help", 1, UI_SPLASH, UI_ICON_HOME,  NULL, NULL, 0, NULL, 0, NULL},
   {"configuration",  "cfg", "clnt","View configuration","No help",1,UI_TABLE,  UI_ICON_NONE,  NULL, NULL, 0, uidata_getlocalcf, 0, NULL},
   {"log routes",     "lgrt","clnt","View log routes", "No help", 1, UI_TABLE,  UI_ICON_ROUTE, NULL, NULL, 0, uidata_getlocalelogrt, 0, NULL},
   {"logs",           "log", "clnt","View log messages","No help",1, UI_TABLE,  UI_ICON_LOG, NULL, NULL, 0, uidata_getlocallogs, 15, NULL},
  {NULL,            NULL,  NULL, NULL,                 NULL,      0, UI_SPLASH, UI_ICON_NONE,  NULL, NULL, 0, NULL, 0, NULL},
};


/* standard features for hosts */
struct uichoice_feature hostfeatures[] = {
/*  label                   key   parent info/tooltip          help   enable  GUI      ICON      features dynchild dynre getdata datare  initarg */
#if 0
  {"uptime",                "up",  NULL, NULL,                "No help", 0, UI_SPLASH, UI_ICON_UPTIME,  NULL, NULL,   0, NULL, 0, NULL},
  {"quality",               "qos", NULL, NULL,                "No help", 0, UI_SPLASH, UI_ICON_QUALITY, NULL, NULL,   0, NULL, 0, NULL},
#endif
  {"perf charts",           "gra", NULL, NULL,                "No help", 1, UI_SPLASH, UI_ICON_GRAPH,   NULL, ghchoice_tree_consring_graph, 0, uidata_get_route_cons, 0, ghchoice_args_perf},
  {"perf data",             "dat", NULL, NULL,                "No help", 1, UI_SPLASH, UI_ICON_TABLE,   NULL, ghchoice_tree_consring_tab, 0, uidata_get_route_cons, 0, ghchoice_args_perf},
  {NULL,                     NULL, NULL, NULL,                NULL,      0, UI_SPLASH, UI_ICON_NONE,    NULL, NULL,   0, NULL, 0, NULL},
};



/* standard features for services */
struct uichoice_feature servfeatures[] = {
/*  label            key   parent info/tooltip          help   enable  GUI      ICON    features dynchild dynre getdata datare initarg */
  {"graphs",        "gra", NULL, NULL,                 "No help", 1, UI_SPLASH, UI_ICON_GRAPH, NULL, NULL, 0, NULL, 0, NULL},
  {"configuration", "cfg", NULL, "View configuration", "No help", 1, UI_TABLE, UI_ICON_NONE, NULL, NULL, 0, uidata_getroutecf, 0, NULL},
  {"probes",        "pro", NULL, NULL,                 "No help", 1, UI_SPLASH, UI_ICON_NONE, NULL, NULL, 0, NULL, 0, NULL},
  {"trends",        "trn", NULL, "Computed trends",    "No help", 0, UI_SPLASH, UI_ICON_NONE, NULL, NULL, 0, NULL, 0, NULL}, 
  {"service levels","svc", NULL, "Set service levels", "No help", 0, UI_SPLASH, UI_ICON_NONE, NULL, NULL, 0, NULL, 0, NULL},
  {"raw data",      "raw", NULL, "Unintrepreted data", "No help", 1, UI_SPLASH, UI_ICON_NONE, NULL, NULL, 0, NULL, 0, NULL},
  {"jobs",          "job", "raw","View jobs",          "No help", 0, UI_SPLASH, UI_ICON_NONE, NULL, NULL, 0, NULL, 0, NULL},
  {"logs",          "log", NULL, "Logging routes",     "No help", 1, UI_TABLE,  UI_ICON_NONE, NULL, NULL, 0, uidata_getrouteelogrt, 0, NULL},
  {NULL,            NULL,  NULL, NULL,                 NULL,      0, UI_SPLASH, UI_ICON_NONE, NULL, NULL, 0, NULL, 0, NULL},
};


/* Timebase constants */
struct ghchoice_timebase timebase[] = {
     {"5 minutes", 300,	       1, 30},
     {"1 hour",	   3600,       1, 300},
     {"4 hours",   14400,      0, 600},
     {"8 hours",   28800,      1, 600},
     {"12 hours",  43200,      0, 600},
     {"24 hours",  86400,      1, 600},
     {"7 days",	   604800,     1, 600},
     {"2 weeks",   1209600,    1, 600},
     {"4 weeks",   2419200,    1, 600},
     {"2 months",  4838400,    0, 600},
     {"3 months",  7257600,    1, 600},
     {"4 months",  9676800,    0, 600},
     {"6 months",  15768000,   1, 600},
     {"1 year",	   31536000,   1, 600},
     {"2 years",   63072000,   1, 600},
     {"5 years",   157680000,  1, 600},
     {"10 years",  315360000,  1, 600},
     {"20 years",  630720000,  1, 600},
     {"30 years",  946080000,  1, 600},
     {"40 years",  1261440000, 1, 600},
     {"50 years",  1576800000, 1, 600},
     {NULL,        0,          0, 0}
};


/* The list for ghchoice_initialview(), used to search the choice tree.
 * Format is:-
 *    - First string is searched by ghchoice_findlabel_all().
 *    - Remaining strings are searched using iochoice_findlabel() down
 *      the tree leg located by the first.
 *    - %h represents the hostname, which will be substituted
 *    - %f represents the first leaf found at that level
 *    - NULL not in the fist string position, terminates the current
 *      search and uses the current node as the initial view.
 *    - NULL in the first string position ends the list
 */
char *ghchoice_initialsearch[] = {
     "this host",            "perf charts", "system", "1 hour", NULL,
     "this host",            "perf charts", "system", "%f", NULL,
     "my hosts", "%h",       "perf charts", "system", "%f", NULL,
     "my hosts", "localhost","perf charts", "system", "%f", NULL,
     "my files", "%h.ts",    "perf charts", "system", "%f", NULL,
     "my files", "%f",       "perf charts", "system", "%f", NULL,
     NULL
};


/* file and repository lookups */
TREE  *ghchoice_fnames;			/* active file to node lookup */
TREE  *ghchoice_repnames;		/* active repository to node lookup */

/* file and route lists */
TREE *ghchoice_myfiles_load;		/* open files being displayed in 
					 * choice tree. Removed from list
					 * when file is closed */
TREE *ghchoice_myfiles_list;		/* all observed file names for 
					 * history in combo boxes */
TREE *ghchoice_myhosts_load;		/* open hosts being displayed in 
					 * choice tree. Removed from list
					 * when host is closed */
TREE *ghchoice_myhosts_list;		/* all observed hostnames for 
					 * history in combo boxes */

/*
 * Initialise the choice structures.
 * Should be carried out before the gui is initialised, as it will 
 * need the facilities provided by uichoice.
 */
void ghchoice_init(CF_VALS cf)
{
     /* create top level nodes and the child trees and add them to 
      * the top level */
     uichoice_addtopnodes (
	  uichoice_mknode("habitat", NULL, NULL, 1, 
			  UI_SPLASH, UI_ICON_SYSGAR, topfeatures,
			  NULL, 0, NULL, 0, NULL)
	  );

     /* create session lists */
     ghchoice_myfiles_load = tree_create();	/* loaded files */
     ghchoice_myfiles_list = tree_create();	/* file history */
     ghchoice_myhosts_load = tree_create();	/* loaded files */
     ghchoice_myhosts_list = tree_create();	/* file history */
     ghchoice_fnames       = tree_create();	/* file -> file node list */
     ghchoice_repnames     = tree_create();	/* reposit -> rep node list */
}


/* deallocate structures created */
void ghchoice_fini()
{
     /* data is cleared by rmnode above, but need to free dup'ed keys */
     tree_clearout(ghchoice_fnames,   tree_infreemem, NULL);
     tree_clearout(ghchoice_repnames, tree_infreemem, NULL);

     /* remove remaining trees */
     tree_destroy(ghchoice_fnames);
     tree_destroy(ghchoice_repnames);

     /* remove session lists: dup'ed keys */
     tree_clearout       (ghchoice_myfiles_load, tree_infreemem, NULL);
     tree_clearout       (ghchoice_myfiles_list, tree_infreemem, NULL);
     tree_destroy        (ghchoice_myfiles_load);
     tree_destroy        (ghchoice_myfiles_list);
     tree_clearoutandfree(ghchoice_myhosts_load);
     tree_clearoutandfree(ghchoice_myhosts_list);
     tree_destroy        (ghchoice_myhosts_load);
     tree_destroy        (ghchoice_myhosts_list);
}


/* set of simple list access/manipulators, several lists */
TREE * ghchoice_get_myfiles_load() {return ghchoice_myfiles_load; }
void   ghchoice_add_myfiles_load(char *fname) {
     if ( ! tree_present(ghchoice_myfiles_load, fname))
	  tree_add(ghchoice_myfiles_load, xnstrdup(fname), NULL);
}
void   ghchoice_add_myfiles_load_tree(TREE *new) {
     tree_traverse(new)
	  if ( ! tree_present(ghchoice_myfiles_load, tree_getkey(new)))
	       tree_add(ghchoice_myfiles_load, xnstrdup(tree_getkey(new)),
			NULL);
}
void   ghchoice_rm_myfiles_load(char *fname) {
     if (tree_find(ghchoice_myfiles_load, fname) != TREE_NOVAL) {
	  tree_infreemem( tree_getkey(ghchoice_myfiles_load) );
	  tree_rm(ghchoice_myfiles_load);
     }
}
TREE * ghchoice_get_myfiles_list() {return ghchoice_myfiles_list; }
void   ghchoice_add_myfiles_list(char *fname) {
     if ( ! tree_present(ghchoice_myfiles_load, fname))
	  tree_add(ghchoice_myfiles_list, xnstrdup(fname), NULL);
}
void   ghchoice_add_myfiles_list_tree(TREE *new) {
     tree_traverse(new)
	  if ( ! tree_present(ghchoice_myfiles_list, tree_getkey(new)))
	       tree_add(ghchoice_myfiles_list, xnstrdup(tree_getkey(new)),
			NULL);
}


TREE * ghchoice_get_myhosts_load() {return ghchoice_myhosts_load; }
void   ghchoice_add_myhosts_load(char *hostname, char *purl) {
     tree_adduniqandfree(ghchoice_myhosts_load, xnstrdup(hostname), 
			 xnstrdup(purl));
}
void   ghchoice_add_myhosts_load_tree(TREE *new) {
     tree_traverse(new)
	  tree_adduniqandfree(ghchoice_myhosts_load, 
			      xnstrdup(tree_getkey(new)), 
			      xnstrdup(tree_get(new)));
}
void   ghchoice_rm_myhosts_load(char *hostname) {
     if (tree_find(ghchoice_myhosts_load, hostname) != TREE_NOVAL) {
	  tree_infreemem( tree_getkey(ghchoice_myhosts_load) );
	  tree_infreemem( tree_get(ghchoice_myhosts_load) );
	  tree_rm(ghchoice_myhosts_load);
     }
}
TREE * ghchoice_get_myhosts_list() {return ghchoice_myhosts_list; }
void   ghchoice_add_myhosts_list(char *hostname, char *purl) {
     tree_adduniqandfree(ghchoice_myhosts_list, xnstrdup(hostname), 
			 xnstrdup(purl));
}
void   ghchoice_add_myhosts_list_tree(TREE *new) {
     tree_traverse(new)
	  tree_adduniqandfree(ghchoice_myhosts_list,
			      xnstrdup(tree_getkey(new)),
			      xnstrdup(tree_get(new)));
}


/* Save the configuration of uichoice to a configuration list */
void ghchoice_cfsave(CF_VALS cf) {
     ITREE *lst;

     /* convert myfiles load from TREE to ITREE and load */
     lst = itree_create();
     tree_traverse(ghchoice_myfiles_load)
	  itree_append(lst, xnstrdup(tree_getkey(ghchoice_myfiles_load)));
     if ( ! itree_empty(lst) )
	  cf_putvec(cf, GHCHOICE_CF_MYFILES_LOAD, lst);

     /* convert myfiles history list from TREE to ITREE and load */
     lst = itree_create();
     tree_traverse(ghchoice_myfiles_list)
	  itree_append(lst, xnstrdup(tree_getkey(ghchoice_myfiles_list)));
     if ( ! itree_empty(lst) )
	  cf_putvec(cf, GHCHOICE_CF_MYFILES_LIST, lst);

     /* convert myhosts list from TREE to ITREE and load */
     lst = itree_create();
     tree_traverse(ghchoice_myhosts_load)
	  itree_append(lst, util_strjoin(tree_getkey(ghchoice_myhosts_load),
					 "@",
					 tree_get(ghchoice_myhosts_load), 
					 NULL) );
     if ( ! itree_empty(lst) )
	  cf_putvec(cf, GHCHOICE_CF_MYHOSTS_LOAD, lst);

     /* convert myhosts history list from TREE to ITREE and load */
     lst = itree_create();
     tree_traverse(ghchoice_myhosts_list)
	  itree_append(lst, util_strjoin(tree_getkey(ghchoice_myhosts_list),
					 "@",
					 tree_get(ghchoice_myhosts_list), 
					 NULL) );
     if ( ! itree_empty(lst) )
	  cf_putvec(cf, GHCHOICE_CF_MYHOSTS_LIST, lst);
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
void   ghchoice_configure(CF_VALS cf)
{
     ITREE *lst;
     struct uichoice_node *myfiles=NULL;
     struct uichoice_node *myhosts=NULL;
     struct uichoice_node *repository=NULL;
     char *host, *purl;
     int r;

     /* load files */
     if (cf_defined(cf, GHCHOICE_CF_MYFILES_LOAD)) {
	  myfiles = uichoice_findlabel_all("my files");
	  if (myfiles == NULL) {
	       elog_printf(ERROR, "can't load my previous files");
	  } else {
	       /* get the session information from the config */
	       lst = cf_getvec(cf, GHCHOICE_CF_MYFILES_LOAD);
	       if (lst) {
		    /* list of session files */
		    itree_traverse(lst) {
			 /* get filename & load, always remember in list */
			 ghchoice_loadfile(itree_get(lst), myfiles, &r);
		         ghchoice_add_myfiles_list(itree_get(lst));
		    }
	       } else {
		    /* get filename & load, always remember in list */
		    ghchoice_loadfile(cf_getstr(cf, GHCHOICE_CF_MYFILES_LOAD), 
					   myfiles, &r);
		    ghchoice_add_myfiles_list(
			          cf_getstr(cf, GHCHOICE_CF_MYFILES_LOAD) );
	       }
	  }
     }

     /* load file history */
     if (cf_defined(cf, GHCHOICE_CF_MYFILES_LIST)) {
     }

     /* load hosts */
     if (cf_defined(cf, GHCHOICE_CF_MYHOSTS_LOAD)) {
	  myhosts = uichoice_findlabel_all("my hosts");
	  if (myhosts == NULL) {
	       elog_printf(ERROR, "can't load my previous hosts");
	  } else {
	       /* get the session information from the config */
	       lst = cf_getvec(cf, GHCHOICE_CF_MYHOSTS_LOAD);
	       if (lst) {
		    /* list of session files */
		    itree_traverse(lst) {
			 /* get string in the form of <host>:<purl> and
			  * load it into 'my hosts' in the choice tree */
			 host = xnstrdup(itree_get(lst));
			 strtok(host, "@");
			 purl = strchr(itree_get(lst), '@');
			 if (purl) {
			      purl++;
			      ghchoice_loadroute(purl, host, myhosts, &r);
			      ghchoice_add_myhosts_list(host, purl);
			 } else {
			      ghchoice_loadroute(host, host, myhosts, &r);
			      ghchoice_add_myhosts_list(host, purl);
			 }
			 nfree(host);
		    }
	       } else {
		    /* get string in the form of <host>:<purl> and
		     * load it into 'my hosts' in the choice tree */
		    host = xnstrdup(cf_getstr(cf, GHCHOICE_CF_MYHOSTS_LOAD));
		    strtok(host, "@");
		    purl = strchr(cf_getstr(cf,GHCHOICE_CF_MYHOSTS_LOAD),'@');
		    if (purl) {
			 purl++;
			 ghchoice_loadroute(purl, host, myhosts, &r);
			 ghchoice_add_myhosts_list(host, purl);
		    } else {
			 ghchoice_loadroute(host, host, myhosts, &r);
			 ghchoice_add_myhosts_list(host, purl);
		    }
		    nfree(host);
	       }
	  }
     }

     /* load file history */
     if (cf_defined(cf, GHCHOICE_CF_MYHOSTS_LIST)) {
     }

     /* set up repository if enabled, but as it is dynamic 
      * it won't yet load */
     if (cf_defined(cf, RT_SQLRS_GET_URLKEY)) {
	  repository = uichoice_findlabel_all("repository");
	  if (repository == NULL) {
	       elog_printf(ERROR,  "unable to find repository node to attach");
	  } else {
	       ghchoice_loadrepository(cf_getstr(cf, 
						 RT_SQLRS_GET_URLKEY), 
				       repository, &r);
	       switch (r) {
	       case 1:		/* good */
		    break;
	       case -1:
		    elog_printf(ERROR, "Unable to load repository %s",
				cf_getstr(cf, RT_SQLRS_GET_URLKEY));
		    break;
	       case -2:
		    elog_printf(WARNING, "repository %s has already been loaded", 
				cf_getstr(cf, RT_SQLRS_GET_URLKEY));
		    break;
	       case 0:
	       default:
		    elog_printf(ERROR, "Error loading repository %s", 
				cf_getstr(cf, RT_SQLRS_GET_URLKEY));
		    break;
	       }
	  }
     } else {
          elog_printf(INFO, "static repository not configured");
     }
}



/*
 * Open a file containing performance data and load it into the choice tree.
 * Work out the file type and make an appropreate description for the 
 * tooltip and the child choice tree.
 * If it is valid and readable, make a node in the uichoice tree as a child
 * of 'parent'. Do not fill in any details for the holstore, but wait until
 * its node is expanded.
 *
 * Returns in status:-	1  for successful
 *			0  for a general error
 *			-1 if unable to find the file
 *			-2 if is already loaded
 */
struct uichoice_node *
ghchoice_loadfile(char *fname,			/* holstore filename */
		  struct uichoice_node *parent,	/* parent node */
		  int *status			/* returned status */)
{
     struct uichoice_node *node;
     char *shortname, fullinfo[1024];
     RS_SUPER super;
     int footprint, remain;

     /* check if we have already read this file */
     if ( tree_find(ghchoice_fnames, fname) != TREE_NOVAL ) {
	  *status = -2;
          return NULL;
     }

     /* check read access */
     if ( access( fname, R_OK ) ) {
          elog_printf(ERROR, "Unable to open %s for reading", fname);
	  *status = -1;
	  return NULL;
     }

     /* gather information from ringstore */
     super = rs_info_super(&rs_gdbm_method, fname);
     if (super) {
          /* file is readable, not loaded before and is a holstore.
	   * Make a node from the filename and info from the full path and
	   * other details. */
          shortname = util_basename(fname);
	  sprintf(fullinfo, 
		  "%s (ringstore v%d, OS %s %s %s %s, on %s, created %s)", 
		  fname, super->version,
		  super->os_name, super->os_release, super->os_version,
		  super->machine, super->machine,
		  util_decdatetime(super->created));

	  rs_free_superblock(super);

	  node = uichoice_mknode(shortname, fullinfo, "no help", 1, 
				 UI_TABLE, UI_ICON_FILE, rsfeatures, 
				 NULL, 0, uidata_get_rsinfo, 0, NULL);
	  
	  /* file specific node arguments */
	  node->nodeargs = tree_create();
	  tree_add(node->nodeargs, xnstrdup("fname"), xnstrdup(fname));
	  tree_add(node->nodeargs, xnstrdup("basepurl"), 
		   util_strjoin("rs:", fname, NULL));
     } else {
          /* not a ringstore, try a FHA */
          elog_printf(DIAG, "unable to read superblock from %s, assuming "
		      "not a ringstore", fname);

          shortname = util_basename(fname);
	  node = uichoice_mknode(shortname, "text file", "no help", 1, 
				 UI_TABLE, UI_ICON_CSV, filefeatures, 
				 NULL, 0, uidata_get_fileinfo, 0, NULL);
	  node->nodeargs = tree_create();
	  tree_add(node->nodeargs, xnstrdup("fname"), xnstrdup(fname));
	  tree_add(node->nodeargs, xnstrdup("basepurl"), 
		   util_strjoin("file:", fname, NULL));
     }
#if 0
     /* error */
     *status = -1;
     return NULL;
#endif

     /* add to file lists: referenced file to node, session & history */
     tree_add(ghchoice_fnames, xnstrdup(fname), node);
     ghchoice_add_myfiles_load(fname);
     ghchoice_add_myfiles_list(fname);

     /* finally, add to parent */
     uichoice_addchild(parent, node);

     *status = 1;
     return node;
}


/*
 * Unload a file (fname) from the choice tree and remove it from the 
 * loaded file list
 * NB. You can get the loaded files from ghchoice_getloadedfiles().
 * Return 1 for success or 0 for failure due to the file not existing in
 * the tree
 */
int ghchoice_unloadfile(char *fname)
{
     struct uichoice_node *node;

     /* check if the file is available */
     if ( (node = tree_find(ghchoice_fnames, fname)) == TREE_NOVAL )
          return 0;

     /* remove the node representing the file and its children */
     uichoice_rmchild(node);
     uichoice_rmnode(node);
     tree_rm(ghchoice_fnames);

     /* remove from file lists */
     ghchoice_rm_myfiles_load(fname);
     return 1;
}


/*
 * Return a list of currently loaded performance data files.
 * The keys are the filenames, values are the uichoice nodes.
 */
TREE *ghchoice_getloadedfiles()
{
     return ghchoice_fnames;
}




/*
 * Open a route and make a description summary from its meta information.
 * The route should refer to the top most component of the specification, 
 * for instance 'sqlrs:myhost' or 'rs:/path/to/ringstorefile' or
 * 'http://host[:port]/path/to/tab/fmt/server'.
 * If it is valid, readable and contains data that can be read, make a node 
 * in the uichoice tree as a child of 'parent'. 
 * Do not fill in any details for the route's data or create any children,
 * but wait until its node is expanded.
 * The node argument 'host' is created in the choice tree.
 *
 * Returns in status:-	1  for successful
 *			0  for a general error
 *			-1 if unable to open the route
 *			-2 if is already loaded
 */
struct uichoice_node *
ghchoice_loadroute(char *purl,			/* route spec p-url */
		   char *label,			/* visible route */
		   struct uichoice_node *parent,/* parent node */
		   int *status			/* returned status */)
{
     struct uichoice_node *node;
     char infopurl[256], *shortname=NULL, fullinfo[1024], *hostinfo;
     TABLE tab;
     int r, i, infolen=0;
     TREE *row;

     /* check if we have already read this file */
     if ( tree_find(ghchoice_fnames, purl) != TREE_NOVAL ) {
	  *status = -2;
          return NULL;
     }

     /* read in the status of the route by appending '?info' to the p-url
      * seeing if scannable data is returned. The name at least should 
      * come back */
     snprintf(infopurl, 256, "%s?info", purl);
     tab = route_tread(infopurl, NULL);
     if ( ! tab ) {
          elog_printf(DIAG, "unable to read %s as table", infopurl);
	  *status = -1;
	  return NULL;
     }

     /* collect information from the table (which should only be one line), 
      * removing the hostname */
     table_first(tab);
     row = table_getcurrentrow(tab);
     tree_traverse(row) {
	  if (strcmp(tree_getkey(row), "host name") == 0) {
	       shortname = tree_get(row);
	  } else {
	       infolen += snprintf(fullinfo+infolen, 1024-infolen, "%s: %s ", 
				   tree_getkey(row), (char *) tree_get(row));
	  }
     }
     tree_destroy(row);

     /* check the host name exists and use a default otherwise */
     if (shortname == NULL)
	  shortname = label;

     /* make a node from the information we have */
     node = uichoice_mknode(shortname, fullinfo, "no help", 1, 
			    UI_TABLE, UI_ICON_NET, hostfeatures, 
			    NULL, 0, uidata_get_hostinfo, 0, NULL);
	  
     /* create hostinfo p-url by lopping off the trailing directory.
      * basepurl is assumed to be of the form:-
      *     hostinfo/killdir/
      * where the trailing slash is optional.
      * 'killdir' is removed from basepurl.
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
	  strcpy(&hostinfo[i], "?info");
     } else {
	  /* its a single host, direct request to another habitat instance
	   * append 'info' */
	  hostinfo[++i] = '\0';
	  hostinfo = xnrealloc( hostinfo, strlen(hostinfo) + 10 );
	  strcpy(&hostinfo[i], "info");		/* turns into '/info' */
     }

     /* file specific node arguments */
     node->nodeargs = tree_create();
     tree_add(node->nodeargs, xnstrdup("basepurl"), xnstrdup(purl));
     tree_add(node->nodeargs, xnstrdup("host"),     xnstrdup(shortname));
     tree_add(node->nodeargs, xnstrdup("hostinfo"), hostinfo);

     /* add to file lists: referenced file to node, session & history */
     tree_add(ghchoice_fnames, xnstrdup(purl), node);
     ghchoice_add_myhosts_load(shortname, purl);
     ghchoice_add_myhosts_list(shortname, purl);

     /* finally, add to parent */
     uichoice_addchild(parent, node);

     table_destroy(tab);
     *status = 1;
     return node;
}


/*
 * Add a repository to the choice tree.
 * The route should be a url to a web object that understands standard
 * addressing and responds to sqlrs: or rs: style formats.
 * For example, 'http://host[:port]/path/to/tab/fmt/server'.
 * ghchoice_tree_group_tab() is called to fill in the tree.
 * The node argument 'repurl' is created in the choice tree,
 * at the repository node and a dynamic child update is forced. 
 * The structure gives us the flexability to have multiple 
 * repositories in the future.
 *
 * Returns in status:-	1  for successful
 *			0  for a general error
 *			-1 if unable to open the route
 *			-2 if is already loaded
 */
struct uichoice_node *
ghchoice_loadrepository(char *purl,			/* route spec p-url */
			struct uichoice_node *node,	/* target node */
			int *status			/* returned status */)
{
     /* check if we have already read this file */
     if ( tree_find(ghchoice_repnames, purl) != TREE_NOVAL ) {
	  *status = -2;
          return NULL;
     }

     elog_printf(INFO, "repository enabled (%s)", purl);

     /* we do not check to see if the address if valid, this is
      * left to ghchoice_tree_group_tab(). All we do is to enable it
      * by updating the node arguments with the purl token */

     /* repository drawing is dynamic, we just need to feed in the correct
      * repository URL as a node argument and redraw to have the
      * branch displayed in the choice tree */

     /* add url as a node arg and index the replication node by url */
     uichoice_putnodearg_str(node, "repurl", purl);
     tree_add(ghchoice_repnames, xnstrdup(purl), node);
     uichoice_gendynamic(node, NULL);

     *status = 1;
     return node;
}



/*
 * Find the best view available in the uichoice trees to welcome an initial
 * user.
 * A pointer to a node will be returned, which will have be
 * associated with an apropreate uidata fetching function.
 * This data may be visualised in the normal way with uidata.
 * The list governing the returned data is held in uichoice_initialsearch[];
 * The first string of any of the search paths must already have been
 * expanded before calling this routine, if it is to be found successfully.
 * If the tree is completely devoid of intresting things, 
 * NULL will be returned.
 */
struct uichoice_node *ghchoice_initialview()
{
     struct uichoice_node *node;
     int i;
     char buf[64], hostname[32];

     i=0;
     while (ghchoice_initialsearch[i] != NULL) {
	  /* find initial string that must already be expanded */
	  node = uichoice_findlabel_all( ghchoice_initialsearch[i] );
	  if (node == NULL)
	       return NULL;	/* failure: list at an end */

	  /* process each search path */
	  i++;
	  while (ghchoice_initialsearch[i] != NULL) {

	       /* expand the node & refresh dynamic if needed */
	       uichoice_expandnode(node);

	       /* check flags */
	       if (strcmp(ghchoice_initialsearch[i], "%f") == 0) {

		    /* first child in this node: dynamic then static */
		    if (node->dyncache && itree_n(node->dyncache) > 0) {
			 itree_first(node->dyncache);
			 node = itree_get(node->dyncache);
		    } else if (node->children && itree_n(node->children) > 0) {
			 itree_first(node->children);
			 node = itree_get(node->children);
		    } else {
			 /* no match, path failed, try another */
			 node = NULL;
		    }

	       } else if (strncmp(ghchoice_initialsearch[i], "%h", 2) == 0) {

		    /* substitute host name and look up */
		    if (gethostname(hostname, ROUTE_HOSTNAMELEN-1) != 0)
			 node = NULL;
		    else {
			 strcpy(buf, ghchoice_initialsearch[i]);
			 util_strsub(buf, "%h", hostname);
			 node = uichoice_findlabel(node, buf);
		    }
	       } else {
		    /* string match: lookup from the children of this node
		     * as it may not have been expanded */
		    node = uichoice_findlabel(node, ghchoice_initialsearch[i]);
	       }

	       /* success, found token: iterate */
	       if (node) {
		    i++;
		    continue;
	       }

	       /* failure: find the search terminator and start new one */
	       while (ghchoice_initialsearch[i] != NULL)
		    i++;
	  }

	  if (node)
	       return node;		/* return a node */
	  else
	       i++;			/* try again */
     }

     return NULL;
}



/*
 * Builds a node tree representing the available versionstore objects
 * that match the pattern namespace, 'p.p.*' and established data presentation
 * objects that will edit them.
 * Each node is given an rname argument so that the correct rings may 
 * be displayed by uidata_getrawts().
 * As ghchoice_patactionchild(), but with editing equivolents.
 */
ITREE *ghchoice_edpatactionchild(TREE *nodeargs)
{
#if 0
     ITREE *subnodetree;
     struct uichoice_node *pnode;

     subnodetree = ghchoice_patactionchild(nodeargs);
     if (subnodetree == NULL)
	  return NULL;

     /* patch the uidata functions with editing equivolents,
      * UI_TABLE with UI_EDTREE */
     itree_traverse(subnodetree) {
          pnode = itree_get(subnodetree);
	  pnode->icon = UI_ICON_LOG;
	  pnode->presentation = UI_EDTREE;
	  pnode->getdata = uidata_edtpatact;
	  pnode->enabled++;
     }

     return subnodetree;
#endif
}



/*
 * Create a TREE with a single node. key=begin val=log
 * Used as an argument for a choice node (begin->log)
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_arg_begin_log(struct uichoice_node *node)
{
     TREE *t;

     t = tree_create();
     tree_add(t, xnstrdup("begin"), xnstrdup("log"));
     return t;
}


/*
 * Create a TREE with a single node. key=begin val=rep
 * Used as an argument for a choice node (begin->rep)
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_arg_begin_rep(struct uichoice_node *node)
{
     TREE *t;

     t = tree_create();
     tree_add(t, xnstrdup("begin"), xnstrdup("rep"));
     return t;
}


/*
 * Create a TREE with a single node. key=begin val=patact
 * Used as an argument for a choice node (begin->patact)
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_arg_begin_patact(struct uichoice_node *node)
{
     TREE *t;

     t = tree_create();
     tree_add(t, xnstrdup("begin"), xnstrdup("patact"));
     return t;
}


/*
 * Create a TREE with a single node. key=begin val=event
 * Used as an argument for a choice node (begin->event)
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_arg_begin_event(struct uichoice_node *node)
{
     TREE *t;

     t = tree_create();
     tree_add(t, xnstrdup("begin"), xnstrdup("event"));
     return t;
}


/*
 * Create a TREE with a single node. key=begin val=watched
 * Used as an argument for a choice node (begin->watched)
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_arg_begin_watched(struct uichoice_node *node)
{
     TREE *t;

     t = tree_create();
     tree_add(t, xnstrdup("begin"), xnstrdup("watched"));
     return t;
}


/*
 * Create a TREE with a single node. key=begin val=up
 * Used as an argument for a choice node (begin->up)
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_arg_begin_up(struct uichoice_node *node)
{
     TREE *t;

     t = tree_create();
     tree_add(t, xnstrdup("begin"), xnstrdup("up"));
     return t;
}


/*
 * Create a TREE with two nodes: (begin->up, duration->0)
 * Used as several arguments for a choice node to display errors
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_args_err(struct uichoice_node *node)
{
     TREE *t;

     t = tree_create();
     tree_add(t, xnstrdup("begin"),    xnstrdup("err"));
     tree_add(t, xnstrdup("duration"), xnstrdup("0"));
     return t;
}


/*
 * Create a TREE with four nodes: 
 * (ring->rstate, duration->0, tsecs->0, lastonly->1)
 * Used as several arguments for a choice node to display replication state
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_args_rstate(struct uichoice_node *node)
{
     TREE *t;

     t = tree_create();

     tree_add(t, xnstrdup("ring"), xnstrdup("rstate"));
     tree_add(t, xnstrdup("duration"), xnstrdup("0"));
     tree_add(t, xnstrdup("tsecs"), xnstrdup("0"));
     tree_add(t, xnstrdup("lastonly"), xnstrdup("1"));
     return t;
}


/*
 * Create a TREE containing several nodes that customise the display of 
 * performance data in a choice node.
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_args_perf(struct uichoice_node *node)
{
     TREE *t;
     char args[512];

     t = tree_create();
     tree_add(t, xnstrdup("labels"), 
	      xnstrdup("sys=system;io=storage;net=network;ps=processes;"
		       "intr=interrupts;name=symbols"));
     tree_add(t, xnstrdup("exclude"), 
	      xnstrdup("up;down;clockwork;log;err;boot;alive;rstate"));
     snprintf(args, 512, "sys=%d;io=%d;net=%d;ps=%d;intr=%d;names=%d;timer=%d",
	      UI_ICON_CPU, UI_ICON_DISK, UI_ICON_NETPERF, UI_ICON_TABLE,
	      UI_ICON_TABLE, UI_ICON_TABLE, UI_ICON_TABLE);
     tree_add(t, xnstrdup("icons"), xnstrdup(args));

     return t;
}


/*
 * Create a TREE containing several nodes that customise the display of 
 * a ringstore in a choice node, to get performance data from an HTTP 
 * server on the localhost.
 * Returns a TREE which should be removed with tree_clearoutandfree().
 */
TREE *ghchoice_args_thishost(struct uichoice_node *node)
{
     TREE *t;
     char args[512];

     t = tree_create();
     snprintf(args, 512, "http://localhost:%d/localtsv/", HTTPD_PORT_HTTP);
     tree_add(t, xnstrdup("basepurl"), xnstrdup(args));
     snprintf(args, 512, "http://localhost:%d/info/", HTTPD_PORT_HTTP);
     tree_add(t, xnstrdup("hostinfo"), xnstrdup(args));

     return t;
}


/*
 * Builds a node tree representing data of a specific duration 
 * available in a route, using standard addressing.
 *
 * The generated tree will be three layers deep, the top one for ring name,
 * next for duration and the one below for the timebase, thus:
 *
 *    <parent> --+-- <ring1> --+-- <duration1> --+-- <timebase1>
 *               |             |                 +-- <timebase2>
 *               |             +-- <duration2> --+-- <timebase1>
 *               |                               +-- <timebase2>
 *               +-- <ring2> --+-- <duration1> --+-- <timebase1>
 *                             |                 +-- <timebase2>
 *                             +-- <duration2> --+-- <timebase1>
 *                                               +-- <timebase2>
 *
 * <ring?> are names of the rings found for that host using
 * the standard route specification.
 * The <timebase?> nodes are the periods that cover each view and is 
 * given by 'tsecs' seconds to define the length of the time base.
 *
 * Nodeargs in     basepurl  base address of route
 *                 begin     select rings begining with this text
 *          out    ring      ring name
 *                 duration  ring data duration in seconds
 *                 tsecs     timebase view in seconds
 */
ITREE *ghchoice_tree_ring_tab(TREE *nodeargs)
{
     char *ringname, *basepurl, *begin, *duration, purl[256];
     TABLE rings, lessrings, morerings=0;
     TABSET tset=0;
     TREE *ringnodes;
     ITREE *ringnodetree;
     struct uichoice_node *pnode, *cnode, *gcnode;
     char timestr[60];
     int i;
     time_t now, secs, start, end;

     /* get args */
     basepurl = tree_find(nodeargs, "basepurl");
     if ( basepurl == TREE_NOVAL )
          elog_die(ERROR, "no basepurl node argument");
     begin = tree_find(nodeargs, "begin");
     if ( begin == TREE_NOVAL )
          begin = NULL;

     /* query route for rings on host */
     snprintf(purl, 256, "%s?linfo", basepurl);
     rings = route_tread(purl, NULL);
     if ( ! rings ) {
          elog_printf(ERROR, "unable to read %s", purl);
	  return NULL;
     }

     /* if there is a 'begin' argument, use only those arguments that 
      * start with it. */
     if (begin) {
	  tset = tableset_create(rings);
	  tableset_where(tset, "name", begins, begin);
	  lessrings = tableset_into(tset);
	  morerings = rings;
	  rings = lessrings;
     }

     /* traverse the table, creating the nodes for ring, duration 
      * and timebases that we find */
     now = time(NULL);
     ringnodetree = itree_create();
     ringnodes = tree_create();
     table_traverse(rings) {
	  /* do I need a new parent to represent ring */
	  ringname = table_getcurrentcell(rings, "name");
	  if (tree_find(ringnodes, ringname) == TREE_NOVAL) {
	       pnode = uichoice_mknode(ringname, NULL, "", 1, UI_SPLASH, 
				       UI_ICON_RING, NULL, NULL, 0, NULL, 
				       0, NULL);
	       pnode->nodeargs = tree_create();
	       tree_add(pnode->nodeargs, xnstrdup("ring"), xnstrdup(ringname));
	       itree_append(ringnodetree, pnode);
	       tree_add(ringnodes, ringname, pnode);
	  } else {
	       pnode = tree_get(ringnodes);
	  }

	  /* Add a duration as a child to the ring node */
	  duration = table_getcurrentcell(rings, "dur");
	  cnode = uichoice_mknode(duration, NULL, "data duration", 1, 
				  UI_SPLASH, UI_ICON_SPAN, NULL, NULL, 0, 
				  NULL, 0, NULL);
	  cnode->nodeargs = tree_create();
	  tree_add(cnode->nodeargs, xnstrdup("duration"), xnstrdup(duration));

	  /* add duration child node to ring parent */
	  uichoice_addchild(pnode, cnode);

	  /* calculate the approximate start time of the ring */
	  start = strtol(table_getcurrentcell(rings, "otime"),
			 (char**)NULL, 10);
	  end = strtol(table_getcurrentcell(rings, "ytime"),
		       (char**)NULL, 10);

	  /* iterate over the predefined timebases, working out which
	   * ones are applicable and make child tree items from them */
	  for (i=0; timebase[i].label != NULL; i++) {
	       if ( ! timebase[i].enabled )
		    continue;

	       /* time bases too short */
	       secs = now - timebase[i].secs;
	       if (secs > end)
		    continue;

	       /* create the grandchild node containing the timebase */
	       sprintf(timestr, "latest %s", util_decdatetime(end) );
	       gcnode = uichoice_mknode(timebase[i].label, timestr,
				       "click for time base", 1, 
				       UI_TABLE, UI_ICON_TABLE, NULL, NULL, 0,
				       uidata_get_route, 
				       timebase[i].refresh, NULL);

	       /* add grandchild node args: tsecs, rings */
	       gcnode->nodeargs = tree_create();
	       tree_add(gcnode->nodeargs, xnstrdup("tsecs"), 
			xnmemdup(&timebase[i].secs, sizeof(time_t)));

	       /* add grandchild node to duration child */
	       uichoice_addchild(cnode, gcnode);

	       /* if we have already provided a view that completely 
		* encompasses the data, then stop: dont do another */
	       if (secs < start)
		    break;
	  }
     }

     /* clear up */
     tree_destroy(ringnodes);
     table_destroy(rings);
     if (morerings)
	  table_destroy(morerings);
     if (tset)
	  tableset_destroy(tset);

     return ringnodetree;
}



/*
 * Builds a node tree representing the available rings and standard 
 * time bases available in the tablestore, whilst setting the node argumments 
 * for graphing.
 * Standard route addressing is used which will return consolidated data
 * See ghchoice_tree_ring_tab() for underlying code; this routine
 * just patches the node built with the UI_GRAPH presentation type
 */
ITREE *ghchoice_tree_ring_graph(TREE *nodeargs)
{
     ITREE *ringnodetree;
     struct uichoice_node *pnode, *cnode, *gcnode;

     /* call raw table dynamic child routine */
     ringnodetree = ghchoice_tree_ring_tab(nodeargs);
     if (ringnodetree == NULL)
	  return NULL;

     /* patch UI_TABLE with UI_GRAPH and UI_ICON_TABLE with UI_ICON_GRAPH */
     itree_traverse(ringnodetree) {
          pnode = itree_get(ringnodetree);
	  if (pnode->icon == UI_ICON_TABLE)
	        pnode->icon = UI_ICON_GRAPH;
	  pnode->enabled++;
	  itree_traverse(pnode->children) {
	       cnode = itree_get(pnode->children);
	       if (cnode->icon == UI_ICON_TABLE)
		    cnode->icon = UI_ICON_GRAPH;
	       cnode->enabled++;
	       itree_traverse(cnode->children) {
		    gcnode = itree_get(cnode->children);
		    gcnode->presentation = UI_GRAPH;
		    gcnode->icon = UI_ICON_GRAPH;
		    gcnode->enabled++;
	       }
	  }
     }

     return ringnodetree;
}



/*
 * Builds a node tree representing consolidated data available in a route, 
 * using standard addressing.
 *
 * The generated tree will be two layers deep, the top one for ring name
 * and the one below for the timebase, thus:
 *
 *    <parent> --+-- <ring1> --+-- <timebase1>
 *               |             +-- <timebase2>
 *               +-- <ring2> --+-- <timebase3>
 *                             +-- <timebase2>
 *
 * <ring?> are names of the rings found for that host using
 * the standard route specification.
 * The <timebase?> nodes are the periods that cover each view and is 
 * given by 'tsecs' seconds to define the length of the time base.
 *
 * Nodeargs in          basepurl  base address of route
 *          in optional exclude   list of rings to ignore
 *                      icons     ring specific icons
 *                      labels    ring name translation and order (not imp)
 *          out         ring      ring name
 *                      tsecs     view duration in seconds
 */
ITREE *ghchoice_tree_consring_tab(TREE *nodeargs)
{
     char *ringname, *basepurl, purl[256], *value, *mylabel, *key;
     TABLE rings;
     ITREE *ringnodetree;
     TREE *ringlab=NULL, *ringex=NULL, *ringicon=NULL;
     struct uichoice_node *pnode, *cnode;
     char timestr[60], *labels, *exclude, *icons, *tok, *tok2,
	  *labelstr=NULL, *excludestr=NULL, *iconstr=NULL;
     int i, pid;
     time_t now, secs, start, end;
     enum uichoice_icontype iconenum;

     /* get args */
     basepurl = tree_find(nodeargs, "basepurl");
     if ( basepurl == TREE_NOVAL )
          elog_die(ERROR, "no basepurl node argument");
     labels = tree_find(nodeargs, "labels");
     if ( labels != TREE_NOVAL && *labels ) {
	  labelstr = xnstrdup(labels);
	  ringlab = tree_create();
	  tok = strtok(labelstr, ";");
	  while (tok) {
	       tok2 = strchr(tok, '=');
	       if (tok2) {
		    *tok2 = '\0';
		    tok2++;
	       }
	       tree_add(ringlab, tok, tok2);
	       tok = strtok(NULL, ";");
	  }
     }
     exclude = tree_find(nodeargs, "exclude");
     if ( exclude != TREE_NOVAL && *exclude ) {
	  excludestr = xnstrdup(exclude);
	  ringex = tree_create();
	  tok = strtok(excludestr, ";");
	  while (tok) {
	       tree_add(ringex, tok, NULL);
	       tok = strtok(NULL, ";");
	  }
     }
     icons = tree_find(nodeargs, "icons");
     if ( icons != TREE_NOVAL && *icons ) {
	  iconstr = xnstrdup(icons);
	  ringicon = tree_create();
	  tok = strtok(iconstr, ";");
	  while (tok) {
	       tok2 = strchr(tok, '=');
	       if (tok2) {
		    *tok2 = '\0';
		    tok2++;
	       }
	       tree_add(ringicon, tok, tok2);
	       tok = strtok(NULL, ";");
	  }
     }

     /* query route for rings on host */
     /* Warning! this query may take some time */
     snprintf(purl, 256, "%s?clinfo", basepurl);
     rings = route_tread(purl, NULL);
     if ( ! rings ) {
          if (strcasestr(basepurl, "localhost") != NULL) {
	       pid = is_clockwork_running(&key, NULL, NULL, NULL);
	       if (pid)
		    elog_printf(ERROR, "Unable to show data from this host. "
				"It may respond if you restart local data "
				"collection (Choose 'Collect->Local Data' "
				"from the menu)");
	       else
		    elog_printf(ERROR, "Unable to show data from this host. "
				"Data is not being collected locally but "
				"can be started with 'Collect->Local Data' "
				"from the menu");
	  } else {
	       elog_printf(ERROR, "Unable to show data. "
			   "Check that the source has data");	    
	  }
	  return NULL;
     }

     /* traverse the table, creating parent and child nodes for the 
      * consolidated ranges that we have found */
     now = time(NULL);
     ringnodetree = itree_create();
     table_traverse(rings) {
	  /* new parent to represent the ring */
	  ringname = table_getcurrentcell(rings, "name");
	  if (!ringname)
	       ringname = table_getcurrentcell(rings, "ring name");
	  if (ringex && tree_find(ringex, ringname) != TREE_NOVAL)
	       continue;
	  if (ringicon && tree_find(ringicon, ringname) != TREE_NOVAL)
	       iconenum = strtol(tree_get(ringicon), (char**)NULL, 10);
	  else
	       iconenum = UI_ICON_TABLE;
	  if (ringlab && tree_find(ringlab, ringname) != TREE_NOVAL)
	       mylabel = tree_get(ringlab);
	  else
	       mylabel = ringname;
	  pnode = uichoice_mknode(mylabel, NULL, "", 1, UI_SPLASH, 
				  iconenum, NULL, NULL, 0, NULL, 0, NULL);
	  pnode->nodeargs = tree_create();
	  tree_add(pnode->nodeargs, xnstrdup("ring"), xnstrdup(ringname));
	  itree_append(ringnodetree, pnode);

	  /* calculate the approximate start time of the ring */
	  value = table_getcurrentcell(rings, "otime");
	  if (!value)
	       value = table_getcurrentcell(rings, "oldest time");
	  start = strtol(value, (char**)NULL, 10);
	  value = table_getcurrentcell(rings, "ytime");
	  if (!value)
	       value = table_getcurrentcell(rings, "youngest time");
	  end = strtol(value, (char**)NULL, 10);

	  /* iterate over the predefined timebases, working out which
	   * ones are applicable and make child tree items from them */
	  for (i=0; timebase[i].label != NULL; i++) {
	       if ( ! timebase[i].enabled )
		    continue;

	       /* time bases too short */
	       secs = now - timebase[i].secs;
	       if (secs > end)
		    continue;

	       /* populate the parent node with child */
	       sprintf(timestr, "latest %s", util_decdatetime(end) );
	       cnode = uichoice_mknode(timebase[i].label, timestr,
				       "click for time base", 1, 
				       UI_TABLE, UI_ICON_TABLE, NULL, NULL, 0,
				       uidata_get_route_cons, 
				       timebase[i].refresh, NULL);

	       /* add child node args: tsecs, rings */
	       cnode->nodeargs = tree_create();
	       tree_add(cnode->nodeargs, xnstrdup("tsecs"), 
			xnmemdup(&timebase[i].secs, sizeof(time_t)));

	       /* add child node to current parent */
	       uichoice_addchild(pnode, cnode);

	       /* if we have already provided a view that completely 
		* encompasses the data, then stop: dont do another */
	       if (secs < start)
		    break;
	  }
     }

     /* clear up */
     table_destroy(rings);
     if (labelstr)
	  nfree(labelstr);
     if (ringlab)
	  tree_destroy(ringlab);
     if (excludestr)
	  nfree(excludestr);
     if (ringex)
	  tree_destroy(ringex);
     if (iconstr)
	  nfree(iconstr);
     if (ringicon)
	  tree_destroy(ringicon);

     return ringnodetree;
}



/*
 * Builds a node tree representing the available rings and standard 
 * time bases available in the tablestore, whilst setting the node argumments 
 * for graphing.
 * Standard route addressing is used which will return consolidated data
 * See ghchoice_tree_ring_tab() for underlying code; this routine
 * just patches the node built with the UI_GRAPH presentation type
 */
ITREE *ghchoice_tree_consring_graph(TREE *nodeargs)
{
     ITREE *ringnodetree;
     struct uichoice_node *pnode, *cnode;

     /* call raw table dynamic child routine */
     ringnodetree = ghchoice_tree_consring_tab(nodeargs);
     if (ringnodetree == NULL)
	  return NULL;

     /* patch UI_TABLE with UI_GRAPH and UI_ICON_TABLE with UI_ICON_GRAPH */
     itree_traverse(ringnodetree) {
          pnode = itree_get(ringnodetree);
	  if (pnode->icon == UI_ICON_TABLE)
	       pnode->icon = UI_ICON_GRAPH;
	  pnode->enabled++;
	  itree_traverse(pnode->children) {
	       cnode = itree_get(pnode->children);
	       cnode->presentation = UI_GRAPH;
	       cnode->icon = UI_ICON_GRAPH;
	       cnode->enabled++;
	  }
     }

     return ringnodetree;
}



/*
 * Builds a node tree representing the available groups (recursive)
 * and hosts in a repository, using a route as a base and standard 
 * addressing to enquire of the structure, thus:
 *
 * <parent> -+- <group1> -+- <subgroup1> -+- [hostgroup1]
 *           |            |               \- [hostgroup2]
 *           |            +- <subgroup2> -+- [hostgroup3]
 *           |                            \- [hostgroup4]
 *           +- <group2> -+- <subgroup3> -+- [hostgroup5]
 *                        |               \- [hostgroup6]
 *                        +- <subgroup4> -+- [hostgroup7]
 *                                        \- [hostgroup8]
 *
 * There may be an arbitary number of hierarchical groups (shown by 
 * groups and subgroups above), each leaf group is set up to activate
 * ghchoice_tree_hostgroup_tab() (shown by [hostgroup] above), 
 * which will expand the hosts in each group. 
 * (Currently, parent is not produced)
 *
 * This routine expects a repository address as a node argument
 * Each node has its node group addressed..
 *
 * Nodeargs in     repurl     base URL address (from the repository)
 *          out    group      leaf group name
 *                 grouppurl  route address of leaf group
 *
 * Returns the completed node tree for the group as an ITREE list, which
 * should be freed with itree_destroy().
 */
ITREE *ghchoice_tree_group_tab(TREE *nodeargs)
{
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
     itree_traverse(nodelist)
	  uichoice_findleafnodes((struct uichoice_node *) itree_get(nodelist),
				 leafnodes);
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
}


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


/*
 * Builds a node tree representing literal sequences of data in a
 * ring, itemising each in turn as children in the choice tree.
 * Uses standard route addressing.
 *
 * The generated tree will be three layers deep, the top one for ring name,
 * next for duration and the one below for the sequence and time, thus:
 *
 *    <parent> --+-- <ring1> --+-- <duration1> --+-- <sequence1>
 *               |             |                 +-- <sequence2>
 *               |             +-- <duration2> --+-- <sequence1>
 *               |                               +-- <sequence2>
 *               +-- <ring2> --+-- <duration1> --+-- <sequence1>
 *                             |                 +-- <sequence2>
 *                             +-- <duration2> --+-- <sequence1>
 *                                               +-- <sequence2>
 *
 * <ring?> are names of the rings found for that host using
 * the standard route specification.
 *
 * Nodeargs in     basepurl  base address of route
 *                 begin     select rings begining with 'begin'
 *          out    ring      ring name
 *                 duration  ring data duration in seconds
 *                 purl      absolute location as a p-url
 */
ITREE *ghchoice_tree_seqs_tab(TREE *nodeargs)
{
     char *ringname, *basepurl, *begin, *duration, purl[256], *seq, *when;
     TABLE rings, lessrings, morerings=0, index;
     TABSET tset=0;
     TREE *ringnodes;
     ITREE *ringnodetree;
     struct uichoice_node *pnode, *cnode, *gcnode;
     char timestr[60];
     int i;
     time_t now;

     /* get args */
     basepurl = tree_find(nodeargs, "basepurl");
     if ( basepurl == TREE_NOVAL )
          elog_die(ERROR, "no basepurl node argument");
     begin = tree_find(nodeargs, "begin");
     if ( begin == TREE_NOVAL )
          begin = NULL;

     /* query route for rings on host */
     snprintf(purl, 256, "%s?linfo", basepurl);
     rings = route_tread(purl, NULL);
     if ( ! rings ) {
          elog_printf(ERROR, "unable to read %s", purl);
	  return NULL;
     }

     /* if there is a 'begin' argument, use only those arguments that 
      * start with it. */
     if (begin) {
	  tset = tableset_create(rings);
	  tableset_where(tset, "name", begins, begin);
	  lessrings = tableset_into(tset);
	  morerings = rings;
	  rings = lessrings;
     }

     /* traverse the table, creating the nodes for ring, duration 
      * and timebases that we find */
     now = time(NULL);
     ringnodetree = itree_create();
     ringnodes = tree_create();
     table_traverse(rings) {
	  /* do I need a new parent to represent ring */
	  ringname = table_getcurrentcell(rings, "name");
	  if (tree_find(ringnodes, ringname) == TREE_NOVAL) {
	       pnode = uichoice_mknode(ringname, NULL, "", 1, UI_SPLASH, 
				       UI_ICON_RING, NULL, NULL, 0, NULL, 
				       0, NULL);
	       pnode->nodeargs = tree_create();
	       tree_add(pnode->nodeargs, xnstrdup("ring"), xnstrdup(ringname));
	       itree_append(ringnodetree, pnode);
	       tree_add(ringnodes, ringname, pnode);
	  } else {
	       pnode = tree_get(ringnodes);
	  }

	  /* Add a duration as a child to the ring node */
	  duration = table_getcurrentcell(rings, "dur");
	  cnode = uichoice_mknode(duration, NULL, "data duration", 1, 
				  UI_SPLASH, UI_ICON_SPAN, NULL, NULL, 0, 
				  NULL, 0, NULL);
	  cnode->nodeargs = tree_create();
	  tree_add(cnode->nodeargs, xnstrdup("duration"), xnstrdup(duration));

	  /* add duration child node to ring parent */
	  uichoice_addchild(pnode, cnode);

	  /* get a list of time and sequences in the current ring and 
	   * duration and place it in index */
	  snprintf(purl, 256, "%s/%s,%s,_seq~_time", basepurl, ringname, 
		   duration);
	  index = route_tread(purl, NULL);
	  if ( ! index ) {
	       elog_printf(ERROR, "unable to read %s", purl);
	       return NULL;
	  }

	  /* iterate over the predefined timebases, working out which
	   * ones are applicable and make child tree items from them */
	  table_traverse(index) {
	       /* create the grandchild node containing the sequence */
	       seq  = table_getcurrentcell(index, "_seq");
	       when = table_getcurrentcell(index, "_time");
	       sprintf(timestr, "%s %s", seq, 
		       when ? 
		       util_decdatetime(strtol(when, (char**)NULL, 10)) : "" );
	       gcnode = uichoice_mknode(timestr, NULL, "click for data", 1, 
				       UI_TABLE, UI_ICON_TABLE, NULL, NULL, 0,
					uidata_get_route, 0, NULL);

	       /* add grandchild node args: purl */
	       snprintf(purl, 256, "%s/%s,%s,_seq~_time", basepurl, ringname, 
			duration);
	       gcnode->nodeargs = tree_create();
	       tree_add(gcnode->nodeargs, xnstrdup("purl"), xnstrdup(purl));

	       /* add grandchild node to duration child */
	       uichoice_addchild(cnode, gcnode);
	  }
	  table_destroy(index);
     }

     /* clear up */
     tree_destroy(ringnodes);
     table_destroy(rings);
     if (morerings)
	  table_destroy(morerings);
     if (tset)
	  tableset_destroy(tset);

     return ringnodetree;
}



/*
 * Builds a node tree representing the last sequence of each ring and 
 * duration in a ring store. Uses standard route addressing.
 *
 * The generated tree will be two layers deep, the top one for ring name,
 * next for duration, thus:
 *
 *    <parent> --+-- <ring1> --+-- <duration1>
 *               |             +-- <duration2>
 *               +-- <ring2> --+-- <duration1>
 *                             +-- <duration2>
 *
 * <ring?> are names of the rings found for that host using
 * the standard route specification.
 *
 * Nodeargs in     basepurl  base address of route
 *                 begin     select rings begining with 'begin'
 *          out    ring      ring name
 *                 duration  ring data duration in seconds
 *                 purl      absolute location as a p-url
 */
ITREE *ghchoice_tree_recent_tab(TREE *nodeargs)
{
     char *ringname, *basepurl, *begin, *duration, purl[256], *endseq;
     TABLE rings, lessrings, morerings=0, index;
     TABSET tset=0;
     TREE *ringnodes;
     ITREE *ringnodetree;
     struct uichoice_node *pnode, *cnode, *gcnode;
     char timestr[60];
     int i;
     time_t now;

     /* get args */
     basepurl = tree_find(nodeargs, "basepurl");
     if ( basepurl == TREE_NOVAL )
          elog_die(ERROR, "no basepurl node argument");
     begin = tree_find(nodeargs, "begin");
     if ( begin == TREE_NOVAL )
          begin = NULL;

     /* query route for rings on host */
     snprintf(purl, 256, "%s?linfo", basepurl);
     rings = route_tread(purl, NULL);
     if ( ! rings ) {
          elog_printf(ERROR, "unable to read %s", purl);
	  return NULL;
     }

     /* if there is a 'begin' argument, use only those arguments that 
      * start with it. */
     if (begin) {
	  tset = tableset_create(rings);
	  tableset_where(tset, "name", begins, begin);
	  lessrings = tableset_into(tset);
	  morerings = rings;
	  rings = lessrings;
     }

     /* traverse the table, creating the nodes for ring, duration 
      * and timebases that we find */
     now = time(NULL);
     ringnodetree = itree_create();
     ringnodes = tree_create();
     table_traverse(rings) {
	  /* do I need a new parent to represent ring */
	  ringname = table_getcurrentcell(rings, "name");
	  if (tree_find(ringnodes, ringname) == TREE_NOVAL) {
	       pnode = uichoice_mknode(ringname, NULL, "", 1, UI_SPLASH, 
				       UI_ICON_RING, NULL, NULL, 0, NULL, 
				       0, NULL);
	       pnode->nodeargs = tree_create();
	       tree_add(pnode->nodeargs, xnstrdup("ring"), xnstrdup(ringname));
	       itree_append(ringnodetree, pnode);
	       tree_add(ringnodes, ringname, pnode);
	  } else {
	       pnode = tree_get(ringnodes);
	  }

	  /* Add a duration as a child to the ring node */
	  duration = table_getcurrentcell(rings, "dur");
	  cnode = uichoice_mknode(duration, NULL, "data duration", 1, 
				  UI_TABLE, UI_ICON_TABLE, NULL, NULL, 0, 
				  uidata_get_route, 0, NULL);
	  cnode->nodeargs = tree_create();
	  tree_add(cnode->nodeargs, xnstrdup("duration"), xnstrdup(duration));

	  /* add duration child node to ring parent */
	  uichoice_addchild(pnode, cnode);

	  /* compose the purl for the last sequence */
	  endseq = table_getcurrentcell(rings, "yseq");
	  snprintf(purl, 256, "%s,%s,%s,s=%s", basepurl, ringname, duration,
		   endseq);
	  tree_add(cnode->nodeargs, xnstrdup("purl"), xnstrdup(purl));
     }

     /* clear up */
     tree_destroy(ringnodes);
     table_destroy(rings);
     if (morerings)
	  table_destroy(morerings);
     if (tset)
	  tableset_destroy(tset);

     return ringnodetree;
}



/*
 * Builds a node tree representing a specific ring and duration, gathering
 * data using timebases. Each leaf addresses route data using standard 
 * addressing
 *
 * The generated tree will be two layers deep, the top one for ring name
 * and the one below for the timebase, thus:
 *
 *    <parent> --+-- <ring1> --+-- <timebase1>
 *               |             +-- <timebase2>
 *               +-- <ring2> --+-- <timebase3>
 *                             +-- <timebase2>
 *
 * <ring?> are names of the rings found for that host using
 * the standard route specification.
 * The <timebase?> nodes are the periods that cover each view and is 
 * given by 'tsecs' seconds to define the length of the time base.
 *
 * Nodeargs in          basepurl  base address of route
 *                      begin     select rings begining with this text (opt)
 *                      duration  all rings have this duration
 *          out         ring      ring name
 *                      tsecs     view duration in seconds
 */
ITREE *ghchoice_tree_ringdur_tab(TREE *nodeargs)
{
     char *ringname, *basepurl, purl[256], *begin, *duration, *value;
     TABLE rings, lessrings, morerings=0;
     TABSET tset=0;
     ITREE *ringnodetree;
     struct uichoice_node *pnode, *cnode;
     char timestr[60], mylabel[50];
     int i;
     time_t now, secs, start, end;

     /* get args */
     basepurl = tree_find(nodeargs, "basepurl");
     if ( basepurl == TREE_NOVAL )
          elog_die(ERROR, "no basepurl node argument");
     begin = tree_find(nodeargs, "begin");
     if ( begin == TREE_NOVAL )
          begin = NULL;
     duration = tree_find(nodeargs, "duration");
     if ( duration == TREE_NOVAL )
          elog_die(ERROR, "no duration node argument");

     /* query route for rings on host */
     /* Warning! this query may take some time */
     snprintf(purl, 256, "%s?linfo", basepurl);
     rings = route_tread(purl, NULL);
     if ( ! rings ) {
          elog_printf(ERROR, "unable to read %s", purl);
	  return NULL;
     }

     /* if there is a 'begin' argument, use only those arguments that 
      * start with it. */
     if (begin) {
	  tset = tableset_create(rings);
	  tableset_where(tset, "name", begins, begin);
	  lessrings = tableset_into(tset);
	  morerings = rings;
	  rings = lessrings;
     }

     /* traverse the table, creating parent and child nodes for the 
      * consolidated ranges that we have found */
     now = time(NULL);
     ringnodetree = itree_create();
     table_traverse(rings) {
	  /* new parent to represent the ring */
	  ringname = table_getcurrentcell(rings, "name");
	  if (!ringname)
	       ringname = table_getcurrentcell(rings, "ring name");
	  snprintf(mylabel, 50, "%s,%s", ringname, duration);
	  pnode = uichoice_mknode(mylabel, NULL, "", 1, UI_SPLASH, 
				  UI_ICON_RING, NULL, NULL, 0, NULL, 0, NULL);
	  pnode->nodeargs = tree_create();
	  tree_add(pnode->nodeargs, xnstrdup("ring"), xnstrdup(ringname));
	  itree_append(ringnodetree, pnode);

	  /* calculate the approximate start time of the ring */
	  value = table_getcurrentcell(rings, "otime");
	  if (!value)
	       value = table_getcurrentcell(rings, "oldest time");
	  start = strtol(value, (char**)NULL, 10);
	  value = table_getcurrentcell(rings, "ytime");
	  if (!value)
	       value = table_getcurrentcell(rings, "youngest time");
	  end = strtol(value, (char**)NULL, 10);


	  /* iterate over the predefined timebases, working out which
	   * ones are applicable and make child tree items from them */
	  for (i=0; timebase[i].label != NULL; i++) {
	       if ( ! timebase[i].enabled )
		    continue;

	       /* time bases too short */
	       secs = now - timebase[i].secs;
	       if (secs > end)
		    continue;

	       /* populate the parent node with child */
	       sprintf(timestr, "latest %s", util_decdatetime(end) );
	       cnode = uichoice_mknode(timebase[i].label, timestr,
				       "click for time base", 1, 
				       UI_TABLE, UI_ICON_TABLE, NULL, NULL, 0,
				       uidata_get_route_cons, 
				       timebase[i].refresh, NULL);

	       /* add child node args: tsecs, rings */
	       cnode->nodeargs = tree_create();
	       tree_add(cnode->nodeargs, xnstrdup("tsecs"), 
			xnmemdup(&timebase[i].secs, sizeof(time_t)));

	       /* add child node to current parent */
	       uichoice_addchild(pnode, cnode);

	       /* if we have already provided a view that completely 
		* encompasses the data, then stop: dont do another */
	       if (secs < start)
		    break;
	  }
     }

     /* clear up */
     table_destroy(rings);

     return ringnodetree;
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
