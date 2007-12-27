/*
 * Pattern - route pattern matching and watching class
 * Nigel Stuckey, March 2000
 *
 * Copyright System Garden Ltd 2000-2001, all rights reserved.
 */

#ifndef _PATTERN_H_
#define _PATTERN_H_

#include <time.h>
#include "route.h"
#include "elog.h"
#include "regex.h"

struct pattern_info {
     char * patact;		/* pattern action p-url */
     time_t patact_modt;	/* pattern action route modification time */
     ROUTE  patact_rt;		/* pattern action open route */
     char * watch;		/* route list p-url */
     time_t watch_modt;		/* watch route modification time */
     ROUTE  watch_rt;		/* watch route open route */
     TREE * patterns;		/* compiled pattern list: key is text (char *)
				 * data is struct watch_action */
     TREE * watchlist;		/* list of route to watch: key is purl (char*)
				 * data is struct watch_routes */
     int    rundirectly:1;	/* if set, actions trigger jobs directly, if
				 * unset, summaries are output to result 
				 * route for queuing and later processing */
};

struct pattern_action {
     regex_t comp;		/* compiled pattern */
     int     embargo_time;	/* embargo of event until time has passed */
     int     embargo_count;	/* emabrgo of event for count repeats */
     time_t  event_timeout;	/* wall clock time to unembargo events */
     int     event_count;	/* number of embargoed events */
     enum elog_severity severity;/*severity */
     char *  action_method;	/* method */
     char *  action_arg;	/* method arguments */
     char *  action_message;	/* message to be sent */
     int     ref;		/* reference count */
};

struct pattern_route {
     char * key;		/* list key, also the route purl */
     int    last_size;		/* size at last route_stat() (if applicable) */
     int    last_seq;		/* sequence at last route_stat() (if applic) */
     time_t last_modt;		/* last modification time */
     int    ref;		/* reference count */
};

typedef struct pattern_info *WATCHED;

#define PATTERN_PATACT_HEAD "pattern\t" "embargo_time\t" "embargo_count\t" \
			    "severity\t" "action_method\t" "action_arg\t" \
			    "action_message"
#define PATTERN_ERRTEXTLEN 100
#define PATTERN_SUMTEXTLEN 1024
#define PATTERN_KEEP 1000

WATCHED pattern_init(ROUTE out, ROUTE err, char *patact, char *rtwatch);
void   pattern_fini(WATCHED);
void   pattern_rundirectly(WATCHED w, int torf);
int    pattern_isrundirectly(WATCHED w);
int    pattern_action(WATCHED, ROUTE out, ROUTE err);
int    pattern_load_patact(WATCHED w);
int    pattern_load_watch(WATCHED w);
ITREE *pattern_getchanged(struct pattern_route *wat);
void   pattern_matchbuffer(ROUTE out, ROUTE err, TREE *palist,
			   struct pattern_route *wat, char *buf, 
			   int rundirectly);
void   pattern_raiseevent(ROUTE out, ROUTE err, struct pattern_action *act, 
			  struct pattern_route *wat, char *buf, 
			  int rundirectly);

#endif /* _PATTERN_H_ */
