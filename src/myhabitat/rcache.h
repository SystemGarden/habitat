/*
 * Habitat ROUTE cache, a cache of data taken from ROUTE sources
 *
 * Nigel Stuckey, August 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#ifndef _RCACHE_H_
#define _RCACHE_H_

#include "../iiab/table.h"
#include "fileroute.h"

/* cache limit */
#define RCACHE_LIMIT 10

/* combined cache return status and format */
enum rcache_load_status {
     RCACHE_LOAD_EMPTY=0,
     RCACHE_LOAD_FAIL,
     RCACHE_LOAD_HOLE,
     RCACHE_LOAD_TIMETABLE,
     RCACHE_LOAD_TABLE,
     RCACHE_LOAD_TEXT,
     RCACHE_LOAD_RING
};

/* struct for each cache entry. held in rcache_mru[] */
struct rcache_entry {
     char * basepurl;	/* address of source data and key */
     long   last_call;	/* sequence number when entry last used */
     time_t last_time;	/* time stamp when entry last used */
     TABLE  tab;	/* the data structure */
     /* NB oldest and youngest below cover both data actually in cache 
      * and request but non existant */
     time_t oldest;	/* oldest time in the cache */
     time_t youngest;	/* youngest time in the cache */
     enum rcache_load_status status;	/* status of first load */
};

void  rcache_init();
void  rcache_fini();
enum rcache_load_status rcache_request(char *purl, time_t min_t, time_t max_t,
				       FILEROUTE_TYPE hint);
TABLE rcache_find(char *basepurl);

/* private finctions */
void  rcache_priv_create_entry(int slot, char *basepurl, TABLE tab, 
			       time_t oldest, time_t youngest,
			       enum rcache_load_status status);
void  rcache_priv_free_entry  (int slot);
int   rcache_priv_oldest_entry();
struct rcache_entry *rcache_priv_find_entry  (char *basepurl);
void rcache_priv_grow_entry(struct rcache_entry *existing, 
			    time_t new_from, time_t new_to);

#endif /* _RCACHE_H_ */
