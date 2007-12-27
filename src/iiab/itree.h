/*
 * In memory, ordered tree abstrction layer
 * Integers are used as keys rather than strings.
 *
 * Nigel Stuckey, August 1996
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _ITREE_H_
#define _ITREE_H_

#include "../pd/red-black/rb.h"

struct itree_head {
	Rb_node root;	/* Head of the tree */
	Rb_node node;	/* Cached node for state */ /* NOT NEEDED */
};

typedef struct itree_head ITREE;

#define ITREE_NOVAL (void *) -1

/* Functional Prototype */
ITREE *itree_create();
void itree_destroy(ITREE *);
void itree_add(ITREE *, unsigned int key, void *block);
void itree_rm(ITREE *);
void itree_first(ITREE *);
void itree_last(ITREE *);
void itree_next(ITREE *);
void itree_prev(ITREE *);
void *itree_get(ITREE *);
unsigned int itree_getkey(ITREE *);
void *itree_put(ITREE *, void *);
void *itree_find(ITREE *, unsigned int key);
int itree_search(ITREE *haystack, void *needle, int needlelen);
int itree_n(ITREE *);
unsigned int itree_append(ITREE *t, void *block);
int itree_present(ITREE *t, unsigned int ikey);
void itree_clearout(ITREE *t, void (*run_on_node)(void *));
void itree_infreemem(void *memtofree);	/* private */
void itree_strdump(ITREE *, char *leadin);
void itree_pintdump(ITREE *, char *leadin);

#define itree_clearoutandfree(t) itree_clearout((t),itree_infreemem)
#define itree_traverse(t) rb_traverse((t)->node, (t)->root)
#define itree_backtraverse(t)		\
  for((t)->node  = rb_last((t)->root);	\
      (t)->node != rb_nil ((t)->root);	\
      (t)->node  = rb_prev((t)->node));
#define itree_empty(t) rb_empty((t)->root) /*or t->node?*/
#define itree_isatend(t) ((t)->root==(t)->node->c.list.flink)
#define itree_isatstart(t) ((t)->root==(t)->node->c.list.blink)
#define itree_isbeyondend(t) ((t)->root==(t)->node)
#define itree_print(t) rb_traverse((t)->node, (t)->root) printf("%s\n",rb_val((t)->node))

#endif /* _ITREE_H_ */
