/*
 * In memory, ordered integer tree abstraction layer
 * Uses integers for keys, not strings
 *
 * Nigel Stuckey, August 1996
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nmalloc.h"
#include "itree.h"

/*
 * Create an empty tree.
 * Integers will be stored as keys and pointers to data (void *) 
 * used as each node's `payload'. 
 * The structure of the tree is organised by following the order of 
 * integer key values.
 * The node payload will not be managed or stored by the tree; just its 
 * pointer. The integer key, however, is stored in each node.
 * Returns an itree handle
 */
ITREE *itree_create() {
	ITREE *t;

	t = xnmalloc(sizeof(ITREE));
	t->node = t->root = make_rb();
	if (t->node)
		return t;
	else
		return NULL;
}

/*
 * Destroy the tree and all the index nodes with it 
 * The node payload will not be removed or affected by this operation; 
 * just its pointer. The value of the key will be lost.
 */
void itree_destroy(ITREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(itree_destroy) t == NULL\n");
		abort();
	}
	rb_free_tree(t->root);
	nfree(t);
}

/*
 * Add datum and key to tree. 
 * The dataum is not copied into the tree; just its reference
 */
void itree_add(ITREE *t, unsigned int ikey, void *datum) {
	if (t == NULL) {
		fprintf(stderr, "(itree_add) t == NULL\n");
		abort();
	}
	t->node = rb_inserti(t->root, ikey, datum);
	/*fprintf(stderr, "itree_add() added key %u\n", ikey);*/
}

/* 
 * Remove the current node from tree.
 * The data is left to be freed externally
 * Advances the current pointer to the next node
 */
void itree_rm(ITREE *t) {
	Rb_node wasteme;

	if (t == NULL) {
		fprintf(stderr, "(itree_rm) t == NULL\n");
		abort();
	}
	wasteme = t->node;
	t->node = rb_next(t->node);
	rb_delete_node(wasteme);
}

/* alter the tree's state to point to the first tree element */
void itree_first(ITREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(itree_first) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_first(t->root);
}

/* alter the tree's state to point to the last tree element */
void itree_last(ITREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(itree_last) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_last(t->root);
}

/* alter the tree's state to point to the next tree element */
void itree_next(ITREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(itree_next) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_next(t->node);
}

/* alter the tree's state to point to the previous tree element */
void itree_prev(ITREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(itree_prev) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_prev(t->node);
}

/* Return the data part of the current node */
void *itree_get(ITREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(itree_get) t == NULL\n");
		abort();
	}
	return rb_val(t->node);
}

/* Return the key of the current node */
unsigned int itree_getkey(ITREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(itree_getkey) t == NULL\n");
		abort();
	}
	return t->node->k.ikey;
}

/* Put new data in an existing node
 * Replace the data pointer in the current node, leaving its key unchanged.
 * Returns a pointer to the previous (replaced) data item.
 */
void *itree_put(ITREE *t, void *dat) {
	void *old;

	if (t == NULL) {
		fprintf(stderr, "(itree_put) t == NULL\n");
		abort();
	}
	old = rb_val(t->node);
	t->node->v.val = dat;		/* Replace node */
	return old;
}

/* Find data given a key and make it current.
 * Returns address of data or ITREE_NOVAL if key not found
 */
void *itree_find(ITREE *t, unsigned int ikey) {
	int found;
	Rb_node n;

	if (t == NULL) {
		fprintf(stderr, "(itree_find) t == NULL\n");
		abort();
	}
	n = rb_find_ikey_n(t->root, ikey, &found);
	if (found) {
		t->node = n;		/* Save node location */
		return rb_val(n);
	}
	else
		return ITREE_NOVAL;
}

/*
 * Search the tree's elements for 'needle' and return the key of the 
 * first match.
 * The search is made scanning the tree sequentially and so can be long 
 * [O(n/2)].
 * A match is made when 'needle' corresponds to the first 'needlelen' bytes of
 * an element.
 * Returns -1 if no match is made.
 */
int itree_search(ITREE *haystack, void *needle, int needlelen) {
     itree_traverse(haystack) {
	  if (memcmp(needle, itree_get(haystack), needlelen) == 0)
	       return itree_getkey(haystack);
     }

     return -1;
}

/* Count and return the number of elements in the tree */
int itree_n(ITREE *t) {
	int n=0;

	if (t == NULL) {
		fprintf(stderr, "(itree_n) t == NULL\n");
		abort();
	}
	itree_traverse(t)
		n++;
	return n;
}

/* 
 * Append with automatic key creation.
 * Treat the tree as a list and add the data using a key which is one
 * advanced from the maximum in existance. If the tree is empty, it starts
 * from 0.
 * This function is special to itrees only.
 * Returns the key selected for insertion
 */
unsigned int itree_append(ITREE *t, void *datum) {
	int i;

	if (t == NULL) {
		fprintf(stderr, "(itree_append) t == NULL\n");
		abort();
	}

	if (rb_empty(t->root)) {
		t->node = rb_inserti(t->root, (i=0), datum);
	} else {
		itree_last(t);
		t->node = rb_inserti(t->root, (i=itree_getkey(t)+1), datum);
	}

	return i;
}

/*
 * Return 1 if the key is present in tree or 0 absent
 * Does not alter the current position
 */
int itree_present(ITREE *t, unsigned int ikey) {
	int found;

	if (t == NULL) {
		fprintf(stderr, "(itree_present) t == NULL\n");
		abort();
	}
	rb_find_ikey_n(t->root, ikey, &found);

	return found;
}

/*
 * Remove all the contents from the tree. 
 * The ITREE descriptior will still be valid
 */
void itree_clearout(ITREE *t, void (*run_on_node)(void *)) {
     if (t == NULL) {
	 fprintf(stderr, "(itree_clearout) t == NULL\n");
	 abort();
     }

     itree_first(t);
     while ( ! itree_empty(t) ) {
	 if (run_on_node)
	      run_on_node( itree_get(t) );
	 itree_rm(t);
     }

     return;
}

/* Private function to free memory. NULL memory references are tolerated */
void itree_infreemem(void *memtofree) {
     if (memtofree)
          nfree(memtofree);
}


/*
 * Dump the contents of the TREE to stdout for diagnostics. The format is 
 * one record per line of the form <leadin><key>=<value>. Currently, key
 * and value must be strings.
 */
void itree_strdump(ITREE *t, char *leadin)
{
	itree_traverse(t)
		printf("%s%u=%s\n", leadin, itree_getkey(t), 
		       (char *) itree_get(t));
}
/*
 * Dump the contents of the TREE to stdout for diagnostics. The format is 
 * one record per line of the form <leadin><key>=<value>. Currently, key
 * must be a string, value must point to an int.
 */
void itree_pintdump(ITREE *t, char *leadin)
{
	itree_traverse(t)
		printf("%s%u=%d\n", leadin, itree_getkey(t), 
		       *((int *) itree_get(t)));
}


#if TEST
#include <stdio.h>
int main(int argc, char **argv) {
     ITREE *t;

     nm_deactivate();
     t = itree_create();

     /* Check a single insertion */
     itree_add(t, 1, "hello nigel");
     itree_first(t);
     if (strcmp("hello nigel", itree_get(t)) != 0) {
	  fprintf(stderr, "[1] node does not match\n");
	  exit(1);
     }

     /* The following is bollocks */
     itree_add(t, 1, "Second buffer");
     itree_add(t, 1, "third text");
     if (itree_n(t) != 3) {
	  fprintf(stderr, "tree does not have three elements\n");
	  exit(1);
     }

     itree_destroy(t);

     /* Test append */
     t = itree_create();
     if (itree_append(t, "first") != 0) {
	  fprintf(stderr, "itree_append does not start at 0\n");
	  exit(1);
     }
     if (itree_append(t, "second") != 1) {
	  fprintf(stderr, "itree_append does not continue to 1\n");
	  exit(1);
     }
     if (itree_append(t, "third") != 2) {
	  fprintf(stderr, "itree_append does not continue to 2\n");
	  exit(1);
     }
     itree_append(t, "4"); itree_append(t,  "5"); itree_append(t,  "6");
     itree_append(t, "7"); itree_append(t,  "8"); itree_append(t,  "9");
     itree_append(t, "8"); itree_append(t, "11"); itree_append(t, "12");
     itree_append(t, "9"); itree_append(t, "14"); itree_append(t, "15");
     if (itree_append(t, "sixteenth") != 15) {
	  fprintf(stderr, "itree_append does not continue to 16\n");
	  exit(1);
     }

     itree_destroy(t);

     printf("%s: test finished successfully\n", argv[0]);
     exit (0);
}
#endif /* TEST */
