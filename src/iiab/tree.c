/*
 * In memory, ordered string tree abstraction layer
 * Uses strings for keys.
 *
 * Nigel Stuckey, August 1996
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nmalloc.h"
#include "tree.h"

/*
 * Create an empty tree.
 * String pointers will be stored as keys and pointers to data (void *) 
 * used as each node's `payload'. 
 * The structure of the tree is organised by following the string pointers 
 * and using ASCII ordering on their values.
 * Neither the key strings or the node payload will be managed or stored 
 * by the tree; just the pointers.
 * Returns a tree handle
 */
TREE *tree_really_create(char *rfile, int rline, const char *rfunc) {
	TREE *t;

	/* allocate using normal malloc but then ask nmalloc to track
	 * TREE top level allocation as a first class object, showing 
	 * the actuall caller */
	t = malloc(sizeof(TREE));
	if (nm_active)
	     nm_add(NM_TREE, t, sizeof(TREE), rfile, rline, rfunc);

	t->root = make_rb();
	if (t->root)
		return t;
	else
		return NULL;
}

/*
 * Destroy the tree and all the index nodes with it 
 * Neither the key strings or the node payloads will be removed or affected
 * by this operation; just the pointers.
 */
void tree_really_destroy(TREE *t, char *rfile, int rline, const char *rfunc) {
	if (t == NULL) {
		fprintf(stderr, "(tree_destroy) t == NULL\n");
		abort();
	}
	rb_free_tree(t->root);

	/* free from special tracking */
	if (nm_active)
	     nm_rm(NM_TREE, t, rfile, rline, rfunc);
	free(t);
}

/*
 * Add datum and key to tree.
 * Copies are not made in the tree
 */
void tree_add(TREE *t,		/* tree reference */ 
	      char *key, 	/* string key */
	      void *datum	/* Pointer to datum associated with key */) {
	if (t == NULL) {
		fprintf(stderr, "(tree_add) t == NULL\n");
		abort();
	}
	t->node = rb_insert(t->root, key, datum);
}

/*
 * Remove the current node from tree.
 * The key and data and left to be freed externally and the current point
 * in the tree is advanced to the next node
 */
void tree_rm(TREE *t) {
	Rb_node wasteme;

	if (t == NULL) {
		fprintf(stderr, "(tree_rm) t == NULL\n");
		abort();
	}
	wasteme = t->node;
	t->node = rb_next(t->node);
	rb_delete_node(wasteme);

}

/* alter the tree's state to point to the first tree element */
void tree_first(TREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(tree_first) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_first(t->root);
}

/* alter the tree's state to point to the last tree element */
void tree_last(TREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(tree_last) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_last(t->root);
}

/* alter the tree's state to point to the next tree element */
void tree_next(TREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(tree_next) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_next(t->node);
}

/* alter the tree's state to point to the previous tree element */
void tree_prev(TREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(tree_prev) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_prev(t->node);
}

/* Return the data part of the current node */
void *tree_get(TREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(tree_get) t == NULL\n");
		abort();
	}
	return rb_val(t->node);
}

#if 0
/* Get the current node's key.
 * Return the key of the current node in a variable pointed to by retkey
 */
void tree_getkey(TREE *t, char **retkey /* Returned key */) {
	if (t == NULL) {
		fprintf(stderr, "(tree_getkey) t == NULL\n");
		aborrt();
	}
	*retkey = t->node->k.key;
}
#endif

/* Get the current node's key. Returns a pointer to the key */
char *tree_getkey(TREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(tree_getkey) t == NULL\n");
		abort();
	}
	return t->node->k.key;
}

/* Put new data in an existing node
 * Replace the data pointer in the current node, leaving its key unchanged.
 * Returns a pointer to the previous (replaced) data item.
 */
void *tree_put(TREE *t, void *dat) {
	void *old;

	if (t == NULL) {
		fprintf(stderr, "(tree_put) t == NULL\n");
		abort();
	}
	old = rb_val(t->node);
	t->node->v.val = dat;		/* Replace node */
	return old;
}

/* Find data given a key and make its position current.
 * Returns address of data or TREE_NOVAL if key not found
 */
void *tree_find(TREE *t, char *key) {
	int found;
	Rb_node n;

	if (t == NULL) {
		fprintf(stderr, "(tree_find) t == NULL\n");
		abort();
	}

	n = rb_find_key_n(t->root, key, &found);
	if (found) {
		t->node = n;		/* Save node location */
		return rb_val(n);
	}
	else
		return TREE_NOVAL;
}

/*
 * Search the tree's elements for 'needle' and return the key of the 
 * first match. The search is made scanning the tree sequentially 
 * and so can't be long [O(n/2)].
 * A match is made when 'needle' corresponds to the first 'needlelen' 
 * bytes of an element.
 * Returns NULL if no match is made or the key of the matching entry.
 */
char *tree_search(TREE *haystack, void *needle, int needlelen) {
     tree_traverse(haystack) {
	  if (memcmp(needle, tree_get(haystack), needlelen) == 0)
	       return tree_getkey(haystack);
     }

     return NULL;
}

/* Count and return the number of elements in the tree */
int tree_n(TREE *t) {
	int n=0;

	if (t == NULL) {
		fprintf(stderr, "(tree_n) t == NULL\n");
		abort();
	}
	tree_traverse(t)
		n++;
	return n;
}

/*
 * Return 1 if the key is present in tree or 0 absent
 * Does not alter the current position
 */
int tree_present(TREE *t, char *key) {
	int found;

	if (t == NULL) {
		fprintf(stderr, "(tree_present) t == NULL\n");
		exit(-1);
	}
	rb_find_key_n(t->root, key, &found);
	return found;
}

/*
 * Remove all the contents from the tree. 
 * The TREE descriptior will still be valid
 */
void tree_clearout(TREE *t, void (*run_on_node_key)(void *), 
		   void (*run_on_node_data)(void *)) {
     if (t == NULL) {
	 fprintf(stderr, "(tree_clearout) t == NULL\n");
	 abort();
     }

     tree_first(t);
     while ( ! tree_empty(t) ) {
	 if (run_on_node_key)
	      run_on_node_key( tree_getkey(t) );
	 if (run_on_node_data)
	      run_on_node_data( tree_get(t) );
	 tree_rm(t);
     }

     return;
}


/* Private function to free memory. NULL memory references are tolorated */
void tree_infreemem(void *memtofree) {
     if (memtofree)
          nfree(memtofree);
}

/*
 * Add the key data pair to the tree, overwriting existing data
 * if it was there. If it displaces data from the tree or does not
 * use supplied data, they will be freed with tree_infreemem()
 */
void tree_adduniqandfree(TREE *t, char *key, void *data) {
	if (t == NULL) {
		fprintf(stderr, "(tree_adduniqueandfree) t == NULL\n");
		exit(-1);
	}

	if (tree_find(t, key) == TREE_NOVAL) {
	     tree_add(t, key, data);
	} else {
	     tree_infreemem(key);
	     tree_infreemem( tree_get(t) );
	     tree_put(t, data);
	}
}

/*
 * Dump the contents of the TREE to stdout for diagnostics. The format is 
 * one record per line of the form <leadin><key>=<value>. Currently, key
 * and value must be strings.
 */
void tree_strdump(TREE *t, char *leadin)
{
	tree_traverse(t)
		printf("%s%s=%s\n", leadin, tree_getkey(t), 
		       (char *) tree_get(t));
}
/*
 * Dump the contents of the TREE to stdout for diagnostics. The format is 
 * one record per line of the form <leadin><key>=<value>. Currently, key
 * must be a string, value must point to an int.
 */
void tree_pintdump(TREE *t, char *leadin)
{
	tree_traverse(t)
		printf("%s%s=%d\n", leadin, tree_getkey(t), 
		       *((int *) tree_get(t)));
}


#if TEST
#include <stdio.h>
int main(int argc, char **argv) {
	TREE *t;

	nm_deactivate();
	t = tree_create();

	/* Check a single insertion */
	tree_add(t, "one", "hello nigel");
	tree_first(t);
	if (strcmp("hello nigel", tree_get(t)) != 0) {
		fprintf(stderr, "[1] node does not match\n");
		abort();
	}

	/* The following is bollocks */
	tree_add(t, "one", "Second buffer");
	tree_add(t, "one", "third text");
	if (tree_n(t) != 3) {
		fprintf(stderr, "tree does not have three elements\n");
		abort();
	}

	tree_destroy(t);

	printf("%s: test finished successfully\n", argv[0]);
	exit (0);
}
#endif /* TEST */
