/*
 * Record - route pattern matching and watching class
 * Nigel Stuckey, April 2000
 *
 * Copyright System Garden Ltd 2000-2001, all rights reserved.
 */

#ifndef _RECORD_H_
#define _RECORD_H_

#include <time.h>
#include "route.h"
#include "elog.h"
#include "regex.h"

struct record_info {
     char * target;		/* filename for recordings */
     char * watch;		/* route list p-url */
     time_t watch_modt;		/* watch route modification time */
     int    watch_size;		/* watch route size */
     int    watch_seq;		/* watch route sequence */
     ROUTE  watch_rt;		/* watch route open route */
     TREE * watchlist;		/* list of route to watch: key is purl (char*)
				 * data is struct watch_routes */
};

struct record_route {
     char * key;		/* list key, also the route purl */
     int    last_size;		/* size at last route_stat() (if applicable) */
     int    last_seq;		/* sequence at last route_stat() (if applic) */
     time_t last_modt;		/* last modification time */
     int    ref;		/* reference count */
};

typedef struct record_info *RECINFO;

#define RECORD_VERPREFIX "ver:"
#define RECORD_RINGPREFIX ",c."

RECINFO record_init(ROUTE out, ROUTE err, char *target, char *rtwatch);
void    record_fini(RECINFO);
int     record_action(RECINFO, ROUTE out, ROUTE err);
int     record_load_watch(RECINFO w);
int     record_haschanged(struct record_route *wat	/* watch */ );
void    record_save(ROUTE out, ROUTE err, RECINFO, struct record_route *wat);

#endif /* _RECORD_H_ */
