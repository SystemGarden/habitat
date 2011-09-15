/*
 * Habitat ROUTE cache, a cache of data taken from ROUTE sources
 *
 * Nigel Stuckey, August 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#include <string.h>
#include "../iiab/util.h"
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"
#include "rcache.h"
#include "fileroute.h"
#include "main.h"

/* Number of cache calls, growing from 1 and an mru array (most recently used)
 * to hold each. Free markers are indicated by 0
 * Each time a route cache is used (with rcache_request), the route is 
 * updated with the current MRU number, the MRU is then updated.
 */
unsigned long rcache_ncalls = 1;
struct rcache_entry rcache_mru[RCACHE_LIMIT];

/* Initialise the rcache structure */
void rcache_init()
{
     int i;

     /* initialise MRU slots with empty markers */
     for (i=0; i<RCACHE_LIMIT; i++) {
          rcache_mru[i].basepurl  = NULL;
          rcache_mru[i].last_call = 0;
          rcache_mru[i].last_time = 0;
          rcache_mru[i].tab       = NULL;
          rcache_mru[i].oldest    = 0;
          rcache_mru[i].youngest  = 0;
	  rcache_mru[i].status    = RCACHE_LOAD_EMPTY;
     }
}



/* empty cache, helpful to check on memory leaks */
void rcache_fini()
{
     int i;

     /* free memory in the cache for all entries */
     for (i=0; i<RCACHE_LIMIT; i++) {
          if (rcache_mru[i].basepurl)
	       nfree(rcache_mru[i].basepurl);
          if (rcache_mru[i].tab)
	       table_destroy(rcache_mru[i].tab);
          rcache_mru[i].basepurl  = NULL;
          rcache_mru[i].last_call = 0;
          rcache_mru[i].last_time = 0;
          rcache_mru[i].tab       = NULL;
          rcache_mru[i].oldest    = 0;
          rcache_mru[i].youngest  = 0;
	  rcache_mru[i].status    = RCACHE_LOAD_EMPTY;
     }
}

/* Request the cache is filled from route called purl running from min to max.
 * Basepurl is the base psudeo-url for route style addressing and needs 
 * to have qualifiers added to extract the specific slices of data we need.
 * Returns:   RCACHE_LOAD_FAIL for a failure with no previous cache entry 
 *              (ie it failed and it has never worked)
 *            RCACHE_LOAD_HOLE for current failure but past success (ie 
 *              there is probably a hole in the data)
 *            RCACHE_LOAD_TIMETABLE for complete success loading a table with
 *               a _time column: either more data was fetched or all the data 
 *               needed was in the cache
 *            RCACHE_LOAD_TABLE for current success loading a table without
 *              a _time column
 *            RCACHE_LOAD_TEXT for current success loading text
 *            RCACHE_LOAD_RING
 */
enum rcache_load_status rcache_request(char *basepurl, 
				       time_t min_t, time_t max_t,
				       FILEROUTE_TYPE hint)
{
     char purl[512];
     time_t from_t, to_t;
     char *from_txt, *to_txt;
     struct rcache_entry *existing;
     TABLE tab;
     int slot;
     enum rcache_load_status status;

     if (!basepurl)
          return RCACHE_LOAD_FAIL;

     /*g_print("rcache request %s from %ld to %ld\n", basepurl, min_t, max_t);*/

     /* find the cache entry */
     existing = rcache_priv_find_entry(basepurl);

     /* check requested times against what we already have.
      * Produce from_t and to_t which bounds the missing data
      */
     if (existing) {
          if (existing->oldest <= min_t)
	       /* we have oldest in cache, set to be above the cache */
	       from_t = existing->youngest+1;
	  else
	       /* we don't have oldest, set to asked */
	       from_t = min_t;
	  if (existing->youngest >= max_t)
	       /* we have the younger in cache, set to be below cache */
	       to_t = existing->oldest-1;
	  else
	       /* we don't have youngest, set to asked */
	       to_t = max_t;
	  if (from_t > to_t) {
	       g_print("already have %s from %s to %s, ", basepurl, 
		       util_decdatetime(existing->oldest), 
		       util_sdecdatetime(existing->youngest));       
	       g_print("asked for %s to %s\n",
		       util_decdatetime(min_t), 
		       util_sdecdatetime(max_t));
	       return existing->status;		/* complete success */
	  }
     } else {
          from_t = min_t;
	  to_t = max_t;
     }

     /* Collect data from the route using time, unless from_t==0
      * when we assume that time is irrelevant. 
      * The route address requests consolidation across rings 
      * of all duration */
     from_txt = xnstrdup(util_decdatetime(from_t));
     to_txt   = xnstrdup(util_decdatetime(to_t));
     if (from_t) {
          snprintf(purl, 512, "%s,cons,*,t=%ld-%ld", basepurl, from_t, to_t);
	  elog_printf(DIAG, "Reading %s into cache from %s to %s", basepurl, 
		      from_txt, to_txt);
	  /* g_print("** reading into cache => %s (%s-%s)", purl, from_txt, 
		  to_txt);*/
     } else {
          strncpy(purl, basepurl, 512);
	  elog_printf(DIAG, "Reading %s into cache without time", basepurl);
	  /* g_print("** reading into cache => %s (no time)\n", purl); */
     }

     /* always read data as a table: can be in three formats */
     tab = fileroute_tread(purl, hint);
     if (!tab) {
          /* No data available for this time range. We assume that the 
	   * transport is reliable and mark it in the cache table so
	   * we dont attempt to fetch the data again */
          elog_printf(DIAG, 
		      "No data available between %s and %s from '%s' (%s)\n", 
		      from_txt, to_txt, basepurl, purl);
	  g_print("-- no data available between %s and %s from '%s'\n", 
		      from_txt, to_txt, basepurl);
	  nfree(from_txt);
	  nfree(to_txt);
	  if (existing) {
	       rcache_priv_grow_entry(existing, from_t, to_t);
	       return RCACHE_LOAD_HOLE;	/* partial fail */
	  } else {
	       return RCACHE_LOAD_FAIL;	/* total fail */
	  }
     }
     nfree(from_txt);
     nfree(to_txt);

     /* remove _ringid column if it exists */
     table_rmcol(tab, "_ringid");
     table_rmcol(tab, "_dur");
     table_rmcol(tab, "_seq");

     /* classify the format of the table for our return */
     if (table_ncols(tab) <= 2 && table_hascol(tab, "data"))
          status = RCACHE_LOAD_TEXT;
     else if (table_hascol(tab, "_time"))
          status = RCACHE_LOAD_TIMETABLE;
     else
          status = RCACHE_LOAD_TABLE;

     /* find the cache entry */
     if (existing) {
          /* add new data to existing table */
          table_addtable(existing->tab, tab, 1);
	  table_sortnumeric(existing->tab, "_time", NULL);
	  g_print("- appended %d rows, %d rows total\n", 
		  table_nrows(tab), table_nrows(existing->tab));
	  rcache_priv_grow_entry(existing, from_t, to_t);
	  table_destroy(tab);
     } else {
          /* find a new slot to store the data in */
          slot = rcache_priv_oldest_entry();
	  rcache_priv_free_entry(slot);
	  rcache_priv_create_entry(slot, basepurl, tab, min_t, max_t, status);
	  g_print("- new, %d rows in slot %d\n", table_nrows(tab), slot);
     }

     return status;		/* complete success */
}


/* Return the data table from the cache using the route name (purl)
 * or NULL if it does not exist.  The returned table will exist until
 * the entry is removed from the cache. It is possible that this will occur
 * after a call to rcache_request(), so you should always follow with a 
 * call to rcache_find. Do not use in reenterent code unless the reference 
 * count of the TABLE is incremented or a copy is made. */
TABLE rcache_find(char *basepurl)
{
     struct rcache_entry *existing;

     existing = rcache_priv_find_entry(basepurl);
     if (existing)
          return existing->tab;
     else
          return NULL;
}


/*
 * Add a new entry to the cache at the given slot, with basepurl and 
 * store TABLE tab as data, running between oldest and youngest times. 
 * Neither basepurl nor tab will be duplicated and if they are assigned 
 * they will be freed with rcache_priv_free_entry().
 */
void rcache_priv_create_entry(int slot, char *basepurl, TABLE tab, 
			      time_t oldest, time_t youngest,
			      enum rcache_load_status status)
{
     rcache_mru[slot].basepurl  = xnstrdup(basepurl);
     rcache_mru[slot].tab       = tab;
     rcache_mru[slot].last_call = ++rcache_ncalls;
     rcache_mru[slot].last_time = time(NULL);
     rcache_mru[slot].oldest    = oldest;
     rcache_mru[slot].youngest  = youngest;
     rcache_mru[slot].status    = status;
}


/* Free the cache entry given its slot index. Data is freed with nfree()
 * and table_destroy() */
void rcache_priv_free_entry(int slot)
{
     /* free the slot */
     if (rcache_mru[slot].basepurl) {
          nfree(rcache_mru[slot].basepurl);
	  rcache_mru[slot].basepurl = NULL;
     }
     rcache_mru[slot].basepurl = NULL;
     if (rcache_mru[slot].tab) {
          table_destroy(rcache_mru[slot].tab);
	  rcache_mru[slot].tab = NULL;
     }
}


/* Linear search of the cache to find the least recently used entry (or oldest)
 * or first blank.
 * Returns the index and does not update the call stamp */
int rcache_priv_oldest_entry()
{
     int i, min, oldest_idx = -1;

     min = rcache_ncalls;

     /* Iterate over the array, finding the oldest entry or an empty slot */
     for (i=0; i<RCACHE_LIMIT; i++) {
          if (rcache_mru[i].basepurl) {
	       /* all entries have to have a name */
	       if ( rcache_mru[i].last_call &&
		    rcache_mru[i].last_call < min) {

		        /* found an older entry. select it */
		        min = rcache_mru[i].last_call;
			oldest_idx = i;
	       }

	  } else {
	       return i;	/* index of unused slot */
	  }
     }

     return i;	/* first oldest entry */
}


/* Return the cache entry using the route name (purl)+update the timestamp
 * or NULL if it does not exist */
struct rcache_entry *rcache_priv_find_entry(char *basepurl)
{
     int i;

     for (i=0; i<RCACHE_LIMIT; i++) {

          if (rcache_mru[i].basepurl && 
	      strcmp(basepurl, rcache_mru[i].basepurl) == 0) {

	       /* found the match - touch the stamp */
	       rcache_mru[i].last_call = ++rcache_ncalls;
	       rcache_mru[i].last_time = time(NULL);

	       /* return the data */
	       return &rcache_mru[i];
	  }
     }

     /* not found, return NULL */
     return NULL;
}


/* Update the existing entry with the new data times and freshen the 
 * MRU count and timestamp. The data times will be used to extend the
 * time stored.
 */
void rcache_priv_grow_entry(struct rcache_entry *existing, 
			    time_t new_from, time_t new_to)
{
     /* increase MRU */
     rcache_ncalls++;

     /* MRU stamp */
     existing->last_call = rcache_ncalls;
     existing->last_time = time(NULL);

     /* update times */
     if (new_from < existing->oldest)
          existing->oldest = new_from;
     if (new_to > existing->youngest)
          existing->youngest = new_to;
}

