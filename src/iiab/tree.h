/*
 * In memory, ordered tree abstrction layer
 * Strings are used as keys
 *
 * Nigel Stuckey, August 1996
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _TREE_H_
#define _TREE_H_

#include "../pd/red-black/rb.h"

struct tree_head {
	Rb_node root;	/* Head of the tree */
	Rb_node node;	/* Cached node for state */
};

typedef struct tree_head TREE;

#define TREE_NOVAL (void *) -1

/* Functional Prototype */
TREE *tree_create(); /* macro prototype */
TREE *tree_really_create(char *rfile, int rline, const char *rfunc);
void  tree_destroy(); /* macro prototype */
void  tree_really_destroy(TREE *, char *rfile, int rline, const char *rfunc);
void  tree_add(TREE *, char *key, void *block);
void  tree_rm(TREE *);
void  tree_first(TREE *);
void  tree_last(TREE *);
void  tree_next(TREE *);
void  tree_prev(TREE *);
void *tree_get(TREE *);
char *tree_getkey(TREE *);
void *tree_put(TREE *, void *);
void *tree_find(TREE *, char *key);
char *tree_search(TREE *haystack, void *needle, int needlelen);
int   tree_n(TREE *);
int   tree_present(TREE *, char *key);
void  tree_clearout(TREE *t, void (*run_on_node_key)(void *), void (*run_on_node_data)(void *));
void  tree_adduniqandfree(TREE *t, char *key, void *data);
void  tree_infreemem(void *memtofree);	/* private */
void  tree_strdump(TREE *, char *leadin);
void  tree_pintdump(TREE *, char *leadin);

#define tree_create()	tree_really_create(__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define tree_destroy(t) tree_really_destroy(t,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define tree_clearoutandfree(t) \
		tree_clearout((t),tree_infreemem,tree_infreemem)
#define tree_traverse(t) rb_traverse((t)->node, (t)->root)
#define tree_empty(t) rb_empty((t)->root)/*or t->node?*/
#define tree_adduniq(t,k,d) \
		(tree_find(t,k)==TREE_NOVAL ? tree_add(t,k,d) : tree_put(t,d))
#define tree_isatend(t) ((t)->root==(t)->node->c.list.flink)
#define tree_isatstart(t) ((t)->root==(t)->node->c.list.blink)
#define tree_isbeyondend(t) ((t)->root==(t)->node)
#define tree_print(t) \
	    rb_traverse((t)->node, (t)->root) printf("%s\n",rb_val((t)->node))
#endif /* _TREE_H_ */
