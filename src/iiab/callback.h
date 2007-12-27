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
 * Nigel Stuckey, January 2001
 * Copyright System Garden Ltd 2001. All rights reserved.
 */

#ifndef _CALLBACK_H_
#define _CALLBACK_H_

void callback_init();
void callback_fini();
void callback_mkevent(char *e_name);
int  callback_rmevent(char *e_name);
void callback_regcb(char *e_name, void (*cb)(void*,void*,void*,void*));
int  callback_unregcb(char *e_name, void (*cb)(void*,void*,void*,void*));
int  callback_raise(char *e_name, void *arg1, void *arg2, void *arg3, 
		    void *arg4);

#endif /* _CALLBACK_H_ */
