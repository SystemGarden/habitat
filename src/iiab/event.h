/*
 * Class to carry out events from a queue, generally having been raised by
 * pattern-action matching. (see pattern.c).
 *
 * Nigel Stuckey, November 2000
 * Copyright System Garden Limited 2000-2001. All rights reserved
 */

#ifndef _EVENT_H_
#define _EVENT_H_

#include "route.h"
#include "tree.h"

#define EVENT_KEEP 1000

struct event_tracking {
     char *rtname;	/* route name, same as key */
     ROUTE rt;
     int lastseq;
};

struct event_information {
     TREE *track;
};

typedef struct event_information *EVENTINFO;

EVENTINFO event_init(char *command);
int event_action(EVENTINFO, ROUTE output, ROUTE error);
int event_execute(char *cmdln, ROUTE output, ROUTE error, char *rtname, 
		  int seq);
void event_fini(EVENTINFO);

#endif /* _EVENT_H_ */
