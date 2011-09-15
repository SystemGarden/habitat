/*
 * Callback class.
 *
 * A generic software event system, that maintains a one to many relationship
 * between software events and interested callback functions.
 *
 * An event is registered using a string as an identifier. Intrested parties
 * that would like to know when the event occurs register callback functions
 * against the event identifier. The event is raised by the emitter, 
 * control passes to the callback, which traverses the callback list.
 * when the list is exhausted, control passes to the emitter.
 *
 * Arguments are simple and must be agreed between the parites. It is
 * just a set of four (void *) with no memory management.
 * Callback functions are called in the order they were registered.
 * Events can be removed, in which case associated callbacks are removed.
 *
 * Copyright System Garden Ltd 2001. All rights reserved.
 */

#include "callback.h"
#include "tree.h"
#include "ptree.h"
#include "nmalloc.h"
#include "elog.h"

/* Data structure: the event string is the key, the value is an PTREE.
 * This value holds the list of callback functions, with the key being
 * the function pointer cast to int, the value of the PTREE is NULL.
 */
TREE *callback_events=0;

void callback_init()
{
     if (callback_events)
	  return;

     callback_events = tree_create();
}


/* shutdown the class and remove all the callbacks & events */
void callback_fini()
{
     if ( ! callback_events )
	  return;

     /* ptree_destroy casted as takes an (ptree *) arg but tree_clearout()
      * wants (void *) */
     tree_clearout(callback_events, tree_infreemem, 
		   (void (*)(void *)) ptree_destroy);
     tree_destroy(callback_events);
}


/*
 * Make an event with out any callbacks. This call is optional, as it
 * will be implied by callback_regcb() when the event does not exist.
 */
void callback_mkevent(char *e_name)
{
     char *name;
     PTREE *cbs;

     if (tree_find(callback_events, e_name) != TREE_NOVAL)
	  return;

     name = xnstrdup(e_name);
     /* only need a linked list or dynamic array or addresses as the 
      * structure. Dont have that atm, so we use a PTREE */
     cbs = ptree_create();
     tree_add(callback_events, name, cbs);
}


/*
 * Remove an event and all associcated callbacks. callback_fini() will
 * have the same effect. Return 1 if successful or 0 if the event does 
 * not exist.
 */
int  callback_rmevent(char *e_name)
{
     if (tree_find(callback_events, e_name) == TREE_NOVAL)
	  return 0;

     nfree( tree_getkey(callback_events) );
     ptree_destroy( (PTREE *) tree_get(callback_events) );
     tree_rm(callback_events);

     return 1;
}


/*
 * Register a callback function against an event. If the event does not 
 * exist then it will be created. 
 */
void callback_regcb(char *e_name, void(*cb)(void*,void*,void*,void*))
{
     PTREE *cbs;

     if (tree_find(callback_events, e_name) == TREE_NOVAL) {
	  callback_mkevent(e_name);
	  tree_find(callback_events, e_name);
     }

     cbs = tree_get(callback_events);
     if (ptree_find(cbs, cb) == PTREE_NOVAL)
	  ptree_add(cbs, cb, NULL);
}


/*
 * Remove a callback function from an event. Returns 1 for successful 
 * or 0 if the event or callback does not exist
 */
int  callback_unregcb(char *e_name, void(*cb)(void*,void*,void*,void*))
{
     PTREE *cbs;

     if (tree_find(callback_events, e_name) == TREE_NOVAL)
	  return 0;

     cbs = tree_get(callback_events);
     if (ptree_find(cbs, cb) == PTREE_NOVAL)
	  return 0;

     ptree_rm(cbs);

     return 1;
}


/*
 * Raise an event. Find all the callbacks associated with the event
 * e_name and propogate a call to each of the callback functions registered
 * against it. The order of calling is not specified.
 * Returns the number of callbacks made or 0 if the event does not exist
 * or there are no callbacks registered.
 */
int  callback_raise(char *e_name, void *arg1, void *arg2, void *arg3,
		    void *arg4)
{
     PTREE *cbs;
     void (*cb)(void*,void*,void*,void*) = 0;
     int n=0;

     if (tree_find(callback_events, e_name) == TREE_NOVAL) {
	  elog_printf(DEBUG, "event %s not registered", e_name);
	  return 0;
     }

     cbs = tree_get(callback_events);
     ptree_traverse(cbs) {
	  cb = ptree_getkey(cbs);
	  elog_printf(DEBUG, "event %s raised -> calling %p", 
		      e_name, cb);
	  (*cb)(arg1, arg2, arg3, arg4);
	  n++;
     }

     return n;
}


