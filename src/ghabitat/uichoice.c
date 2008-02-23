/*
 * Habitat
 * GUI * Designed to be used in conjunction with uidata:: to extract data.
 * Uichoice:: should be called by specific GUI toolkits, which
 * will place the information into a single tree widget
 *
 * Nigel Stuckey, May, June, December 1999
 * Restructured feb 2002
 * Copyright System Garden 1999-2002. All rights reserved.
 */

#define _GNU_SOURCE

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "uichoice.h"
#include "../iiab/nmalloc.h"
#include "../iiab/itree.h"
#include "../iiab/tree.h"
#include "../iiab/elog.h"
#include "../iiab/util.h"
#include "../iiab/route.h"
#include "../iiab/cf.h"

/*
 * Implements a choice or decision tree catering for user interfaces.
 * No specific ui code is included, but there are many callbacks and
 * storage mechanisms that can be used to connect to apecific interfaces
 *
 * The nodes of the tree can be built using static, dynamic or manual methods,
 * each of which creates a branch to be added to the main tree.
 *
 * Static branches are built from C structures of type uichoice_feature and
 * stay for the duration of the tree unless manually removed.
 *
 * Dynamic branches are built by calling functions periodically in each node 
 * which build a whole sub tree and return it for inclusion. The branch
 * returned is cached so that further dynamic nodes within that branch
 * may also be supported. After the timeout period, the branch will be 
 * deleted and a new one created.
 *
 * Manual branches may be created, which may contain static and dynamic
 * content in turn. Changes in the choice tree from a UI callback will
 * be created in this way. For example, the addition of files.
 */

ITREE *uichoice_topnodes;		/* list containing top level nodes */

/*
 * Initialise the choice structures.
 * Should be carried out before the gui is initialised, as it will 
 * need the facilities provided by uichoice.
 */
void uichoice_init(CF_VALS cf)
{
     uichoice_topnodes = itree_create();
}


/* deallocate structures created */
void uichoice_fini()
{
     itree_destroy(uichoice_topnodes); /* no storage to free */
}


/* set of simple list access/manipulators for the topnodes */
ITREE *uichoice_gettopnodes() { return uichoice_topnodes; }
void   uichoice_addtopnodes(struct uichoice_node *node) {
     itree_append(uichoice_topnodes, node);
}
void   uichoice_addtopnodes_list(ITREE *nodelist) {
     itree_traverse(nodelist)
	  itree_append(uichoice_topnodes, itree_get(nodelist));
}


/*
 * Create a choice_node with no children and return its address
 * The choice tree will not be affected and you need to use 
 * uichoice_addchild() in order to attach it to a parent. 
 * Free with uichoice_rmnode() once detached from the tree 
 * using uichoice_rmchild().
 */
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
				 TREE *(*initnodeargs)(struct uichoice_node *))
{
     struct uichoice_node *n;

     n = xnmalloc(sizeof(struct uichoice_node));
     n->label = xnstrdup(label);
     if (info)
          n->info = xnstrdup(info);
     else
          n->info = NULL;
     if (help)
          n->help = xnstrdup(help);
     else
          n->help = NULL;
     n->enabled = enabled;
     n->presentation = presentation;
     n->icon = icon;
     n->getdata = getdata;
     n->datatime = 0;
     n->datatimeout = datatimeout;
     n->children = itree_create();
     n->features = features;
     n->dynchildren = dynchildren;
     n->dyncache = NULL;
     n->dyntime = time(NULL);
     n->dyntimeout = dyntimeout;
     n->initnodeargs = initnodeargs;
     if (initnodeargs)
          n->nodeargs = initnodeargs(n);
     else
          n->nodeargs = NULL;
     n->parent = NULL;
     n->is_dynamic = 0;
     if (presentation == UI_EDTABLE ||
	 presentation == UI_EDFORM ||
	 presentation == UI_EDTEXT ||
	 presentation == UI_EDTREE)
	  n->is_editable = 1;
     else
	  n->is_editable = 0;
     n->is_expanded = 0;
     n->features_expanded = 0;

     return n;
}


/* 
 * Free the node and all its children recursively (both dynamic and static).
 * The node should be removed from its parent before calling as further 
 * referencies to the node will result in error.
 * Following this call, nothing is left to free including the node itself.
 */
void uichoice_rmnode(struct uichoice_node *n)
{
     nfree(n->label);
     if (n->info)
          nfree(n->info);
     if (n->help)
          nfree(n->help);
     if (n->nodeargs) {
          tree_clearoutandfree(n->nodeargs);
	  tree_destroy(n->nodeargs);
	  n->nodeargs = NULL;
     }
     uichoice_rmnodetree(n->children);
     if (n->dyncache)
          uichoice_rmnodetree(n->dyncache);
     nfree(n);
}


/*
 * Free all the nodes in the list and recurse to all their children
 * (both dynamic and static). The list may implement either part or
 * a complete branch of the tree.
 * The nodes should be removed from their parents before caling as further 
 * referencies to the tree (or nodes in the list) will result in error
 */
void uichoice_rmnodetree(ITREE *t)
{
     if (t == NULL)
	  return;
     itree_traverse(t)
          uichoice_rmnode( itree_get(t) );
     itree_destroy(t);
}


/* 
 * add a child node to a parent; if the parent is disabled, then it will
 * be enabled. If the parent is dynamic or editable, then those attributes
 * will be inherited.
 */
void uichoice_addchild(struct uichoice_node *parent, 
		       struct uichoice_node *child)
{
     child->parent = parent;
     itree_append(parent->children, child);
     parent->enabled++;
     if (parent->is_dynamic)
	  uichoice_setdynamic(child, parent->is_dynamic);
     if (parent->is_editable)
	  uichoice_seteditable(child, parent->is_editable);
}


/* 
 * add a list of child nodes to a parent; if the parent is disabled, 
 * then it will be enabled. If the parent is dynamic or editable, then 
 * those attributes will be inherited.
 */
void uichoice_addchildren(struct uichoice_node *parent, 
			  ITREE *children)
{
     struct uichoice_node *child;

     if ( ! children)
	  return;
     itree_traverse(children) {
	  child = itree_get(children);
	  uichoice_addchild(parent, child);
     }
}


/*
 * Remove child from parent; if parent has no more children, it will be 
 * disbled. After this call, the child will be parentless but will still exist
 */
void   uichoice_rmchild(struct uichoice_node *child)
{
     int ndynchildren=0;

     /* remove from parent's child list */
     itree_traverse(child->parent->children)
	  if ( itree_get(child->parent->children) == child ) {
	       itree_rm(child->parent->children);
	       break;
	  }

     /* set parent inactive if there are no siblings */
     if (child->parent->dyncache)
	  ndynchildren = itree_n(child->parent->dyncache);
     if (itree_n(child->parent->children) == 0 && ndynchildren == 0)
	  child->parent->enabled = 0;

     /* renounce the parent */
     child->parent = NULL;
}


/* return 1 if maybe_ancestor is an ancestor of child or is the child itself
 * otherwise 0 if not */
int    uichoice_isancestor(struct uichoice_node *maybe_ancestor,
			   struct uichoice_node *child)
{
     if (child == NULL || child->parent == NULL || maybe_ancestor == NULL)
	  return 0;

     if (child == maybe_ancestor)
	  return 1;
     else
	  return uichoice_isancestor(maybe_ancestor, child->parent);
}



/*
 * Recursively search the nodes from parent downwards until a node with
 * the requred label is found. Searches the static then the cached dynamic
 * children, but does not expand any node in the tree.
 * Returns a pointer to the node if successful or NULL otherwise
 */
struct uichoice_node *uichoice_findlabel(struct uichoice_node *node, 
					 char *label)
{
     struct uichoice_node *rnode;

     if ( strcmp(label, node->label) == 0 )
          return node;
     if (node->children)
	  itree_traverse(node->children)
	       if ( (rnode = uichoice_findlabel(itree_get(node->children), 
						label)) )
		    return rnode;
     if (node->dyncache)
	  itree_traverse(node->dyncache)
	       if ( (rnode = uichoice_findlabel(itree_get(node->dyncache), 
						label)) )
		    return rnode;
     return NULL;
}



/*
 * Recursively search for a label from the whole node tree.
 * Return the node address or a NULL (and raise an diag elog) if not found.
 * See uichoce_findlabel() */
struct uichoice_node *uichoice_findlabel_all(char *label)
{
     struct uichoice_node *node=NULL;

     itree_traverse(uichoice_topnodes) {
	  node = uichoice_findlabel(itree_get(uichoice_topnodes), label);
	  if (node)
	       break;
     }
     if (node == NULL) {
	  elog_printf(DIAG, "unable to find node %s", label);
	  return NULL;
     }

     return node;
}


/*
 * Find a list of terminal nodes or leaf nodes from 'node' and place
 * the list in 'list', which should be an existing ITREE.
 * If node is a leaf itself, then it will be added. A leaf is defined
 * as a node having no children or cached dynamic children currently
 * in existance. Leaves may have a dynamic child creation routine or
 * choice features, but would still be added if no instantiated children
 * exist.
 * Returns nothing by adds nodes to list of type uichoice_node.
 */
void uichoice_findleafnodes(struct uichoice_node *node, ITREE *list)
{
     int n=0;

     if (node == NULL || list == NULL)
	  return;

     /* calculate the number of descendents */
     if (node->children)
	  n += itree_n(node->children);
     if (node->dyncache)
	  n += itree_n(node->dyncache);
     if (n==0) {
	  /* i am a leaf, return a list of one */
	  itree_append(list, node);
	  return;
     }

     /* I am a parent, with children to support */
     if (node->children)
	  itree_traverse(node->children)
	       uichoice_findleafnodes(
		    (struct uichoice_node *) itree_get(node->children), list);
     if (node->dyncache)
	  itree_traverse(node->dyncache)
	       uichoice_findleafnodes(
		    (struct uichoice_node *) itree_get(node->children), list);
     return;
}



/* 
 * Compile a string representing the path from the root of the tree
 * to the given node. Each node is separated by the string 'sep'.
 * An nmalloc()ed string is returned.
 */
char *uichoice_nodepath(struct uichoice_node *node, char *sep)
{
     char *path, *newpath;

     if (!node->parent)
	  return xnstrdup(node->label);

     path = uichoice_nodepath(node->parent, sep);
     newpath = util_strjoin(path, sep, node->label, NULL);

     nfree(path);
     return newpath;
}



/* 
 * Diagnostic dump of a node and its children to stdout. 
 * Always call with indentsz=0. 
 * See uichoice_printnodes for a simplified version 
 */
void uichoice_dumpnodes(ITREE *tree, int indentsz)
{
     int i;
     char indent[1024], indent2[1024];
     struct uichoice_node *node;

     if ( ! tree )
          return;

     indent[0] = '\0';
     for (i=0; i<indentsz; i++)
          strcat(indent, " ");

     itree_traverse(tree) {
          node = itree_get(tree);
          printf("%s=>%s (%s/%s) %s pres=%d icon=%d\n"
		 "%s  dynamic=%s editable=%s expanded=%s-%s dyntime=%s\n", 
		 indent,
		 node->label,
		 node->info ? node->info : "",
		 node->help ? node->help : "",
		 node->enabled ? "enab " : "disab",
		 node->presentation,
		 node->icon,
		 indent,
		 node->is_dynamic    ? "yes"  : "no",
		 node->is_editable   ? "yes"  : "no",
		 node->is_expanded   ? "yes"  : "no",
		 node->features_expanded ? "features" : "never",
		 node->dynchildren   ? util_shortadaptdatetime(node->dyntime)
		                              :"n/a");
	  if (node->nodeargs) {
	       strcpy(indent2, indent);
	       strcat(indent2, "  nodeargs:");
	       tree_pintdump(node->nodeargs, indent2);
	  }
	  if (node->children)
	       uichoice_dumpnodes(node->children, indentsz+2);
	  /*dynchildren = uichoice_gendynamic(node, NULL);*/
	  if (node->dyncache)
	       uichoice_dumpnodes(node->dyncache, indentsz+2);
     }
}

/* 
 * Print the node and its children to stdout. Always call with level=0
 * See uichoice_dumpnodes for a comprehensive version.
 */
void uichoice_printnodes(ITREE *tree, int level)
{
     int i;
     char indent[1024];
     struct uichoice_node *node;
     ITREE *dynchildren;

     if ( ! tree )
          return;

     indent[0] = '\0';
     for (i=0; i<level; i++)
          strcat(indent, "    ");

     itree_traverse(tree) {
          node = itree_get(tree);
          printf("%s%-10s (%s) %s\n", indent, node->label,
		 node->info ? node->info : "",
		 node->enabled ? "" : "disabled");
          uichoice_printnodes(node->children, level+1);
	  dynchildren = uichoice_gendynamic(node, NULL);
	  if (dynchildren)
	       uichoice_printnodes(dynchildren, level+1);
     }
}


/* Set this node and its children to be editable, which is a flag 
 * currently used for display emphasis. */
void   uichoice_seteditable(struct uichoice_node *node, int is_editable)
{
     node->is_editable = is_editable;

     if (node->children)
	  itree_traverse(node->children)
	       uichoice_seteditable(itree_get(node->children), is_editable);

     if (node->dyncache)
	  itree_traverse(node->dyncache)
	       uichoice_seteditable(itree_get(node->dyncache), is_editable);
}


/* Set this node and its children to be dynamic, which is a flag 
 * currently used for display emphasis. */
void   uichoice_setdynamic(struct uichoice_node *node, int is_dynamic)
{
     node->is_dynamic = is_dynamic;

     if (node->children)
	  itree_traverse(node->children)
	       uichoice_setdynamic(itree_get(node->children), is_dynamic);

     if (node->dyncache)
	  itree_traverse(node->dyncache)
	       uichoice_setdynamic(itree_get(node->dyncache), is_dynamic);
}




/* add a node argument to the uichoice_node structure.
 * The argument is a string and a duplicate copy is taken.*/
void   uichoice_putnodearg_str(struct uichoice_node *node, char *key, 
			       char *val) {
     if (node->nodeargs == NULL)
	  node->nodeargs = tree_create();

     if (tree_find(node->nodeargs, key) == TREE_NOVAL) {
	  tree_add(node->nodeargs, xnstrdup(key), xnstrdup(val));
     } else {
	  if (tree_get(node->nodeargs) != NULL)
	       nfree(tree_get(node->nodeargs));
	  tree_put(node->nodeargs, xnstrdup(val));
     }
}



/* add a node argument to the uichoice_node structure.
 * the argument is binary, pointed to by mem and of size bytes.
 * A private copy of that memory is taken, so the node argument should be
 * updated or removed if the data is no longer valid */
void   uichoice_putnodearg_mem(struct uichoice_node *node, char *key, 
			       void *mem, int size) {
     if (node->nodeargs == NULL)
	  node->nodeargs = tree_create();

     if (tree_find(node->nodeargs, key) == TREE_NOVAL) {
	  tree_add(node->nodeargs, xnstrdup(key), xnmemdup(mem, size));
     } else {
	  if (tree_get(node->nodeargs) != NULL)
	       nfree(tree_get(node->nodeargs));
	  tree_put(node->nodeargs, xnmemdup(mem, size));
     }
}


/* return the value of the requested key or NULL if it does not exist */
void * uichoice_getnodearg(struct uichoice_node *node, char *key) {
     void *r;

     if (node->nodeargs == NULL)
	  return NULL;
     r = tree_find(node->nodeargs, key);
     if (r == TREE_NOVAL)
	  return NULL;
     else
	  return r;
}

/* free the memory taken by the node argument */
void uichoice_rmnodearg(struct uichoice_node *node, char *key)
{
     void *r;

     if (node->nodeargs) {
	  r = tree_find(node->nodeargs, key);
	  if (r != TREE_NOVAL) {
	       nfree(tree_getkey(node->nodeargs));
	       nfree(r);
	  }
	  tree_rm(node->nodeargs);
     }
}



/*
 * Add a heiracharical list of features to a node.
 * Features are specified by an array of uichoice_feature, which contain
 * keys and parent keys to indicate the heirachy in a flat list.
 * Should always be called with parent key set to NULL.
 * Returns the number of features added
 */
int uichoice_addfeatures(struct uichoice_node *node, 
			 struct uichoice_feature *features,
			 char *parentkey)
{
     int i, added=0;
     struct uichoice_node *newnode;

     if (features == NULL)
	  return 0;

     for (i=0; features[i].key; i++) {
          if ( parentkey && features[i].parentkey 
	       && strcmp(features[i].parentkey, parentkey) )
	       continue;
	  if ( parentkey && !features[i].parentkey )
	       continue;
	  if ( !parentkey && features[i].parentkey )
	       continue;
	  newnode = uichoice_mknode(features[i].label, 
				    features[i].info,
				    features[i].help,
				    features[i].enabled,
				    features[i].presentation,
				    features[i].icon,
				    features[i].features,
				    features[i].dynchildren,
				    features[i].dyntimeout,
				    features[i].getdata,
				    features[i].datatimeout,
				    features[i].initnodeargs);
          uichoice_addchild(node, newnode);
	  added++;
	  added += uichoice_addfeatures(newnode, features, features[i].key);
     }

     return added;
}




/*
 * Create a heiracharical list of sub-nodes from a flat TABLE and
 * return it as a node list of the top parents.
 * Sub-nodes are specified by a TABLE data type, which each line representing
 * one node. The columns should be as follows:-
 *	label	name of node
 *	info	information string
 *	help	help string
 *      key	identification key
 *	parent	parent key
 * Parentkey should be set to the key of the highest level nodes.
 * Set in common for all the nodes are the icons, presentation and 
 * callback details of dynamic and static children.
 * If diffenent details are required for each node (such as icons), 
 * it is recommended that the node tree is traversed again to 'patch' the 
 * different nodes.
 * Returns the node list of parents or NULL if there is no data.
 */
ITREE *uichoice_mknodelist_from_table(TABLE tab,
				      char *parentkey,
				      enum uidata_type presentation,
				      enum uichoice_icontype icon,
				      struct uichoice_feature *features,
				      ITREE *(*dynchildren)(TREE *),
				      int dyntimeout,
				      RESDAT (*getdata)(TREE *),
				      int datatimeout)
{
     struct uichoice_node *newnode;
     char *nodeparent, *label, *info, *help, *key;
     ITREE *nodelist, *childnodelist;
     int rowkey;

     if (tab == NULL)
	  return NULL;

     nodelist = itree_create();

     /* traverse the table; unfortunately table_traverse() is stateful
      * and we reset that state when we recurse. So we save and restore the
      * state (rowkey) before and after recursing and enclose in a while 
      * loop */
     table_first(tab);
     while ( ! table_isbeyondend(tab) ) {
	  nodeparent = table_getcurrentcell(tab, "parent");
          if ( parentkey && nodeparent && strcmp(parentkey, nodeparent) )
	       goto tabloop;
	  if ( parentkey && !nodeparent )
	       goto tabloop;
	  if ( !parentkey && nodeparent )
	       goto tabloop;
	  label = table_getcurrentcell(tab, "label");
	  info  = table_getcurrentcell(tab, "info");
	  help  = table_getcurrentcell(tab, "help");
	  key   = table_getcurrentcell(tab, "key");
	  newnode = uichoice_mknode(label, 
				    info,
				    help,
				    1 /* enabled=1 */,
				    presentation,
				    icon,
				    features,
				    dynchildren,
				    dyntimeout,
				    getdata,
				    datatimeout,
				    NULL /* initialargs=NULL */ );
	  itree_append(nodelist, newnode);
	  rowkey = table_getcurrentrowkey(tab);
	  childnodelist = uichoice_mknodelist_from_table(
	       tab, key, presentation, icon, features,
	       dynchildren, dyntimeout, getdata, datatimeout);
	  table_gotorow(tab, rowkey);
	  uichoice_addchildren(newnode, childnodelist);
     tabloop:
	  table_next(tab);
     }

     /* return NULL if no data */
     if (itree_n(nodelist) == 0) {
	  itree_destroy(nodelist);
	  return NULL;
     }

     return nodelist;
}




/*
 * Recurse upwards from the given node to the root node then unwind 
 * and the way collect node arguments from the given nodes's ancestors. 
 * Args toward the leaves will take precedence over ancestor ones and any 
 * key clash will cause the values to be replaced with younger ones.
 * Returns a TREE pointing to the arguments: no duplicates are made as
 * we dont know what the data types are.
 * Free list with tree_destroy().
 */
void uichoice_getinheritedargs(struct uichoice_node *node, TREE *list)
{
     void *data;
     TREE *newargs;

     if ( ! list )
          return;
     if (node->parent)
          uichoice_getinheritedargs(node->parent, list);

     /*
      * refresh the nodeargs at this node: 
      * create a new set of nodeargs and replace any values that are
      * produced
      */
     if (node->initnodeargs) {
	  newargs = node->initnodeargs(node);
	  if (newargs) {
	       if (node->nodeargs == NULL)
		    node->nodeargs = newargs;
	       else {
		    tree_traverse(newargs) {
			 if (tree_find(node->nodeargs, 
				       tree_getkey(newargs)) == TREE_NOVAL) {
			      /* add new nodearg */
			      tree_add(node->nodeargs, tree_getkey(newargs), 
				       tree_get(newargs));
			 } else {
			      /* refresh existing nodearg */
			      nfree(tree_get(node->nodeargs));
			      tree_put(node->nodeargs, tree_get(newargs));
			      nfree(tree_getkey(newargs));
			 }
		    }
		    tree_destroy(newargs);
	       }
	  }
     }

     /* populate the passed list */
     if (node->nodeargs) {
          tree_traverse(node->nodeargs) {
	       if ( (data = tree_find(list, 
			      tree_getkey(node->nodeargs))) == TREE_NOVAL ) {
		    tree_add(list, tree_getkey(node->nodeargs), 
			     tree_get(node->nodeargs));
	       } else {
		    tree_put(list, tree_get(node->nodeargs));
	       }
	  }
     }
     return;
}


/*
 * Create the dynamic children of a given node.
 * If inheritedargs is NULL, then the existing node arguments in this 
 * and all parent nodes will be passed to the dynamic creation routine;
 * if a value is given, this behaviour is removed and only the 
 * arguments specified will be used.
 * The cache will be freed and time updated on the next call to this routine.
 * If the node is shadowed by a GUI, then it should be syncronised
 * with the new state of this node.
 * Dyncache and dyntime are set in the node on successful completion
 * and the list of children is returned. NULL is returned on failure.
 */
ITREE *uichoice_gendynamic(struct uichoice_node *node, TREE *inheritedargs)
{
     TREE *args;
     struct uichoice_node *childnode;

     if ( ! node->dynchildren )
          return NULL;

     uichoice_freedynamic(node);

     /* collect inherited arguments */
     if (inheritedargs == NULL) {
	  args = tree_create();
	  uichoice_getinheritedargs(node, args);
     } else
	  args = inheritedargs;

     /* create dynamic children and store directly in node */
     node->dyncache = node->dynchildren(args);
     node->dyntime = time(NULL);
     if (inheritedargs != NULL)
	  tree_destroy(args);

     /* give all top level children parents and set their dynamic attribute */
     if (node->dyncache)
          itree_traverse(node->dyncache) {
               childnode = itree_get(node->dyncache);
	       childnode->parent = node;
	       uichoice_setdynamic(childnode, 1);
	       if (node->is_editable)
		    uichoice_seteditable(childnode, node->is_editable);
          }

     /* catch empty lists to return NULLs */
     if (node->dyncache && itree_n(node->dyncache) == 0) {
          itree_destroy(node->dyncache);
	  node->dyncache = NULL;
	  node->dyntime = time(NULL);
     }

     return node->dyncache;
}


/*
 * free the list of dynamically created children
 */
void uichoice_freedynamic(struct uichoice_node *node)
{
     if (node == NULL)
	  return;

     /* clear previous node tree */
     if (node->dyncache)
          uichoice_rmnodetree(node->dyncache);
     node->dyncache = NULL;
}


/*
 * Check if the dynamic children in the uichoice node need to be updated.
 * If so then call uichoice_gendynamic()
 * If the node is shadowed by a GUI, then it should be syncronised
 * with the new state of this node.
 * Returns the number of top level dynamic children created or 
 * 0 for no changes.
 */
int uichoice_updatedynamic(struct uichoice_node *node)
{
     if (node->dyntime + node->dyntimeout > time(NULL)) {
	  uichoice_gendynamic(node, NULL);
	  if (node->dyncache == NULL)
	       return 0;
	  else
	       return itree_n(node->dyncache);
     }

     return 0;
}



/*
 * Expand the node by instantiating its static and dynamic children
 * Further calls to expand will not create any more children or update
 * the dynamic cache: use uichoice_updatedynamic() for that.
 */
void   uichoice_expandnode(struct uichoice_node *node)
{
     if (node->is_expanded == 0) {
	  if (node->features_expanded == 0) {
	       /* never before expanded means we have not set up 
		* static features as children before */
	       uichoice_addfeatures(node, node->features, NULL);
	       uichoice_gendynamic(node, NULL);
	       node->features_expanded = 1;
	  }
	  node->is_expanded = 1;
     }
}


/* collapse the node, as the details are not currently needed */
void   uichoice_collapsenode(struct uichoice_node *node)
{
     /* whilst we could clearup all the children and free storage, it 
      * is much faster just to flag the node as not expanded so
      * that updates pass it by */
     node->is_expanded = 0;
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
     uichoice_init();
     hol_init(0, 0);

     /* generate a holstore test file */
     unlink(TEST_FILE1);
     hid = hol_create(TEST_FILE1, 0644);
     hol_put(hid, "toss", "bollocks", 9);
     hol_close(hid);

     /* test 1: show a tree */
     printf("test 1:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     /* test 2: find a node */
     node1 = uichoice_findlabel(uichoice_myfilenode, TEST_FILE1);
     if ( ! node1 )
          elog_die(FATAL, "[2] unable to find %s", TEST_FILE1);

     /* test 3: expand the timestore branch */

     /* test 4: show expanded branch */
     printf("test 4:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     /* test 5: some empty timestore rings */

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
     node1 = uichoice_findlabel(uichoice_myfilenode, TEST_FILE1);
     if ( ! node1 )
          elog_die(FATAL, "[11] unable to find %s", TEST_FILE1);
     node1 = uichoice_findlabel(node1, "raw tablestore");
     if ( ! node1 )
          elog_die(FATAL, "[11] unable to find %s","raw tablestore");
     nodelist = uichoice_gendynamic(node1);
     if ( ! nodelist )
          elog_die(FATAL, "[11] unable to expand");
     if (itree_n(nodelist) != 3)
          elog_die(FATAL, "[11] nodelist (%d) != 3", 
		   itree_n(nodelist));

     /* test 12: show expanded branch */
     printf("test 12:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     /* test 13: add some version information */     

     /* test 14: expand the versionstore branch */
     node1 = uichoice_findlabel(uichoice_myfilenode, TEST_FILE1);
     if ( ! node1 )
          elog_die(FATAL, "[14] unable to find %s", TEST_FILE1);
     node1 = uichoice_findlabel(node1, "raw versionstore");
     if ( ! node1 )
          elog_die(FATAL, "[14] unable to find %s", 
		   "raw versionstore");
     nodelist = uichoice_gendynamic(node1);
     if ( ! nodelist )
          elog_die(FATAL, "[14] unable to expand");
     if (itree_n(nodelist) != 3)
          elog_die(FATAL, "[14] nodelist (%d) != 3", 
		   itree_n(nodelist));

     /* test 15: show expanded branch */
     printf("test 15:-\n");
     uichoice_dumpnodes(uichoice_nodetree, 0);

     uichoice_fini();
     elog_fini();
     route_close(err);
     route_fini();
     exit(0);
}

#endif /* TEST */
