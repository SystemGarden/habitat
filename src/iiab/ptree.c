/*
 * In memory, ordered pointer tree abstraction layer
 * Uses strings for keys.
 *
 * Nigel Stuckey, August 2009
 * Copyright System Garden Ltd 1996-2009. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nmalloc.h"
#include "ptree.h"

/*
 * Create an empty tree.
 * Pointers are the keys and a different pointers to data (void *) 
 * used as each node's `payload'. 
 * The structure of the tree is organised by using the pointer addresses 
 * themselves, without dereferencing. The pointers will be inserted
 * inorder, but this is typically unimportant and just used for indexing.
 * The node payload will not be managed or stored by the tree; just its 
 * pointer. The pointer key, however, is stored in each node.
 * Returns an ptree handle
 */
PTREE *ptree_create() {
	PTREE *t;

	t = xnmalloc(sizeof(PTREE));
	t->root = make_rb();
	if (t->root)
		return t;
	else
		return NULL;
}

/*
 * Destroy the tree and all the index nodes with it 
 * The node payloads will not be removed or affected by this operation;
 * just its pointer. The value of the key will be lost
 */
void ptree_destroy(PTREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(ptree_destroy) t == NULL\n");
		abort();
	}
	rb_free_tree(t->root);
	nfree(t);
}

/*
 * Add datum and key to tree.
 * The dataum is not copied into the tree; just its reference
 */
void ptree_add(PTREE *t,	/* tree reference */ 
	       void *key, 	/* address key */
	       void *datum	/* Pointer to datum associated with key */) {
	if (t == NULL) {
		fprintf(stderr, "(ptree_add) t == NULL\n");
		abort();
	}
	t->node = rb_insertg(t->root, key, datum, ptree_cmp);
}

/*
 * Remove the current node from tree.
 * The data is left to be freed externally and the current point
 * in the tree is advanced to the next node
 */
void ptree_rm(PTREE *t) {
	Rb_node wasteme;

	if (t == NULL) {
		fprintf(stderr, "(ptree_rm) t == NULL\n");
		abort();
	}
	wasteme = t->node;
	t->node = rb_next(t->node);
	rb_delete_node(wasteme);

}

/* alter the tree's state to point to the first tree element */
void ptree_first(PTREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(ptree_first) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_first(t->root);
}

/* alter the tree's state to point to the last tree element */
void ptree_last(PTREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(ptree_last) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_last(t->root);
}

/* alter the tree's state to point to the next tree element */
void ptree_next(PTREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(ptree_next) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_next(t->node);
}

/* alter the tree's state to point to the previous tree element */
void ptree_prev(PTREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(ptree_prev) t == NULL\n");
		abort();
	}
	if ( ! rb_empty(t->root))
	     t->node = rb_prev(t->node);
}

/* Return the data part of the current node */
void *ptree_get(PTREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(ptree_get) t == NULL\n");
		abort();
	}
	return rb_val(t->node);
}

/* Get the current node's key. Returns a pointer */
void *ptree_getkey(PTREE *t) {
	if (t == NULL) {
		fprintf(stderr, "(ptree_getkey) t == NULL\n");
		abort();
	}
	return t->node->k.key;
}

/* Put new data in an existing node
 * Replace the data pointer in the current node, leaving its key unchanged.
 * Returns a pointer to the previous (replaced) data item.
 */
void *ptree_put(PTREE *t, void *dat) {
	void *old;

	if (t == NULL) {
		fprintf(stderr, "(ptree_put) t == NULL\n");
		abort();
	}
	old = rb_val(t->node);
	t->node->v.val = dat;		/* Replace node */
	return old;
}

/* Find data given a key and make its position current.
 * Returns address of data or PTREE_NOVAL if key not found
 */
void *ptree_find(PTREE *t, void *key) {
	int found;
	Rb_node n;

	if (t == NULL) {
		fprintf(stderr, "(ptree_find) t == NULL\n");
		abort();
	}

	n = rb_find_gkey_n(t->root, key, ptree_cmp, &found);
	if (found) {
		t->node = n;		/* Save node location */
		return rb_val(n);
	}
	else
		return PTREE_NOVAL;
}

/*
 * Search the tree's elements for 'needle' and return the key of the 
 * first match. The search is made scanning the tree sequentially 
 * and so can't be long [O(n/2)].
 * A match is made when 'needle' corresponds to the first 'needlelen' 
 * bytes of an element.
 * Returns NULL if no match is made or the key of the matching entry.
 */
void *ptree_search(PTREE *haystack, void *needle, int needlelen) {
     ptree_traverse(haystack) {
	  if (memcmp(needle, ptree_get(haystack), needlelen) == 0)
	       return ptree_getkey(haystack);
     }

     return NULL;
}

/* Count and return the number of elements in the tree */
int ptree_n(PTREE *t) {
	int n=0;

	if (t == NULL) {
		fprintf(stderr, "(ptree_n) t == NULL\n");
		abort();
	}
	ptree_traverse(t)
		n++;
	return n;
}

/*
 * Return 1 if the key is present in tree or 0 absent
 * Does not alter the current position
 */
int ptree_present(PTREE *t, void *key) {
	int found;

	if (t == NULL) {
		fprintf(stderr, "(ptree_present) t == NULL\n");
		exit(0);
	}
	rb_find_gkey_n(t->root, key, ptree_cmp, &found);
	return found;
}

/*
 * Remove all the contents from the tree. 
 * The PTREE descriptior will still be valid
 */
void ptree_clearout(PTREE *t, void (*run_on_node_data)(void *)) {
     if (t == NULL) {
	 fprintf(stderr, "(ptree_clearout) t == NULL\n");
	 abort();
     }

     ptree_first(t);
     while ( ! ptree_empty(t) ) {
	 if (run_on_node_data)
	      run_on_node_data( ptree_get(t) );
	 ptree_rm(t);
     }

     return;
}


/* Private function to free memory. NULL memory references are tolorated */
void ptree_infreemem(void *memtofree) {
     if (memtofree)
          nfree(memtofree);
}


/* Private function to compare two addresses and place them in order.
 * Follows the same return as strcmp: a<b -> -1, a==b -> 0, a>b -> 1 */
int ptree_cmp(char *a, char *b) {
     if (a<b)
          return -1;
     if (a==b)
          return 0;
     else 
          return 1;
}


/*
 * Dump the contents of the PTREE to stdout for diagnostics. The format is 
 * one record per line of the form <leadin><key>=<value>. Currently, key
 * and value must be strings.
 */
void ptree_strdump(PTREE *t, char *leadin)
{
	ptree_traverse(t)
		printf("%s%p=%s\n", leadin, ptree_getkey(t), 
		       (char *) ptree_get(t));
}
/*
 * Dump the contents of the PTREE to stdout for diagnostics. The format is 
 * one record per line of the form <leadin><key>=<value>. Currently, key
 * must be a string, value must point to an int.
 */
void ptree_pintdump(PTREE *t, char *leadin)
{
	ptree_traverse(t)
		printf("%s%p=%d\n", leadin, ptree_getkey(t), 
		       *((int *) ptree_get(t)));
}


#if TEST
#include <stdio.h>
int main(int argc, char **argv) {
	PTREE *t;
	char *buf = "one";

	nm_deactivate();
	t = ptree_create();

	/* Check a single insertion */
	ptree_add(t, buf, "hello nigel");
	ptree_first(t);
	if (strcmp("hello nigel", ptree_get(t)) != 0) {
		fprintf(stderr, "[1] node does not match\n");
		abort();
	}

	/* The following is bollocks */
	ptree_add(t, buf, "Second buffer");
	ptree_add(t, buf, "third text");
	if (ptree_n(t) != 1) {
		fprintf(stderr, "tree does not have one element\n");
		abort();
	}

	ptree_destroy(t);

	printf("%s: test finished successfully\n", argv[0]);
	exit (0);
}
#endif /* TEST */
