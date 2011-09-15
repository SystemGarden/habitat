/*
 * In memory, ordered tree abstrction layer
 * Pointers (undereferenced) or addresses are used as keys
 *
 * Nigel Stuckey, August 2009
 * Copyright System Garden Ltd 1996-2009. All rights reserved.
 */

#ifndef _PTREE_H_
#define _PTREE_H_

#include "../pd/red-black/rb.h"

struct ptree_head {
	Rb_node root;	/* Head of the tree */
	Rb_node node;	/* Cached node for state */
};

typedef struct ptree_head PTREE;

#define PTREE_NOVAL (void *) NULL

/* Functional Prototype */
PTREE *ptree_create();
void   ptree_destroy(PTREE *);
void   ptree_add(PTREE *, void *key, void *block);
void   ptree_rm(PTREE *);
void   ptree_first(PTREE *);
void   ptree_last(PTREE *);
void   ptree_next(PTREE *);
void   ptree_prev(PTREE *);
void * ptree_get(PTREE *);
void * ptree_getkey(PTREE *);
void * ptree_put(PTREE *, void *);
void * ptree_find(PTREE *, void *key);
void * ptree_search(PTREE *haystack, void *needle, int needlelen);
int    ptree_n(PTREE *);
int    ptree_present(PTREE *, void *key);
void   ptree_clearout(PTREE *t, void (*run_on_node_data)(void *));
void   ptree_infreemem(void *memtofree);	/* private */
int    ptree_cmp(char *a, char *b);
void   ptree_strdump(PTREE *, char *leadin);
void   ptree_pintdump(PTREE *, char *leadin);

#define ptree_clearoutandfree(t) \
		ptree_clearout((t),ptree_infreemem,ptree_infreemem)
#define ptree_traverse(t) rb_traverse((t)->node, (t)->root)
#define ptree_empty(t) rb_empty((t)->root)/*or t->node?*/
#define ptree_adduniq(t,k,d) \
		(ptree_find(t,k)==PTREE_NOVAL ? ptree_add(t,k,d) : ptree_put(t,d))
#define ptree_isatend(t) ((t)->root==(t)->node->c.list.flink)
#define ptree_isatstart(t) ((t)->root==(t)->node->c.list.blink)
#define ptree_isbeyondend(t) ((t)->root==(t)->node)
#define ptree_print(t) \
	    rb_traverse((t)->node, (t)->root) printf("%s\n",rb_val((t)->node))
#endif /* _PTREE_H_ */
