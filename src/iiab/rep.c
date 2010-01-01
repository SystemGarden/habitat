/*
 * Replicate - send and recieve ring entries to or from a standard repository
 * Nigel Stuckey, April 2003
 *
 * Copyright System Garden Ltd 2000-2001, all rights reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <sys/utsname.h>
#include "nmalloc.h"
#include "util.h"
#include "tree.h"
#include "itree.h"
#include "iiab.h"
#include "route.h"
#include "rep.h"

/*
 * Carry out replication
 *
 * This command is stateless and can be carried out without initialisation
 * or finalising.
 * The ITREEs are lists of strings which contain replication instructions.
 * Each instruction line is of form 'from>to' or 'to<from'. The to or
 * from should be a route p-url address, with a special token to represent
 * the current running host: %h.
 * On an example machine called kevin, 'sqlrs:%h,tom,3600>tab:var/%h.rs,tom' 
 * in the IN list, will take data from the remote ring 'sqlrs:kevin,tom,3600'
 * and will replicate to the local ring 'tom' in the file 'var/kevin.rs'.
 * The result: Hourly samples are downloaded.
 * Return -1 for error.
 */
int rep_action(ROUTE out,		/* route output */
	       ROUTE err,		/* route errors */
	       ITREE *in_rings,		/* list of rings to input */
	       ITREE *out_rings,	/* list of rings to output */
	       char * state_purl	/* purl to store state */ )
{
     ROUTE state_rt, rt;
     TABLE state, io;
     int r, local_seq, remote_seq, remote_youngest_s, local_max_seq;
     time_t youngest_t, remote_youngest_t;
     char *local_ring, *remote_ring, *buf, *from, *to;
     char *rtstatus, *rtinfo;

     /* check arguments */
     if (state_purl == NULL || *state_purl == '\0') {
	  elog_printf(ERROR, "no state storage supplied");
	  return -1;
     }

     /*
      * The state variables
      * state_purl - p-url location of local state table (given in arg)
      * state_rt   - opened route of state table
      * state      - TABLE containing the in-memory state table
      */

     /* get replication state in table */
     state = route_tread(state_purl, NULL);
     if (!state) {
	  buf   = xnstrdup(REP_STATE_HDS);
	  state = table_create_s(buf);
	  table_freeondestroy(state, buf);
     }

     /* set up the route for saving the state of the replication
      * we only need one copy, so should ideally be one slot ring
      * or a singleton */
     state_rt = route_open(state_purl, "replication state", NULL, 1);
     if (! state_rt) {
	  elog_die(ERROR, "unable to open state storage (%s)", state_purl);
	  table_destroy(state);
	  return -1;
     }

#if 0
     /* debug inputs, outputs and state */
     elog_printf(DIAG, "REPLICATE: state=%s", state_purl);
     itree_strdump(in_rings,  " in-> ");
     itree_strdump(out_rings, " out-> ");
     elog_printf(DIAG, "REPLICATE TABLE\n%s", table_print(state));
#endif

     /* ********** inbound replicated data **********
      * iterate over the in-bound rings, reading new data into local rings */
     itree_traverse(in_rings) {

          /*
	   * The inbound replication variables
	   * from        - default remote ring p-url
	   * to          - default local ring p-url
	   * local_ring  - local ring p-url
	   * remote_ring - remote ring p-url
	   * remote_seq  - youngest remote sequence
	   * io          - new remote data to append to local ring
	   */

          /* get replication end points and last remote sequence */
	  rep_endpoints(itree_get(in_rings), &from, &to);
	  rep_state_new_or_get(state, itree_get(in_rings), from, to, 
			       &local_ring, &remote_ring, &remote_seq, NULL);

	  elog_printf(INFO, "Receiving %d sequences from %s to %s",
		      0, from, to);

	  /* download remote data */
	  io = rep_remote_get(remote_ring, remote_seq+1);
	  if ( ! io )
	       continue;

	  /* open the local ring */
	  rt = rep_local_open_or_create(local_ring, remote_ring);
	  if ( ! rt ) {
	       table_destroy(io);
	       continue;
	  }

	  /* save the remote data locally */
	  rep_local_save(rt, io);
	  route_close(rt);

	  /* find last local location post write */
	  if (!route_stat(local_ring, NULL, &local_seq, &r, &youngest_t))
	       elog_printf(ERROR, "can't stat local ring: %s", 
			   local_ring);

	  /* prepare the inbound data to extract information */
	  table_last(io);	/* ASSUMPTION! that io is seq/time ordered
				 * and the last row is the youngest */
	  remote_seq = strtol(table_getcurrentcell(io, "_seq"), 
			      (char**)NULL, 10);

	  /* Update state table with local and remote ring details */
	  rep_state_update(state, itree_get(in_rings), local_seq, remote_seq, 
			   youngest_t);

	  /* Write state table to local storage (do it every iteration 
	   * for safety) */
	  if (!route_twrite(state_rt, state))
	       elog_printf(ERROR,"unable to save state having read in %s", to);

#if 0
	  elog_printf(DIAG, "REPLICATE: state=%s", state_purl);
	  itree_strdump(in_rings,  " in-> ");
	  itree_strdump(out_rings, " out-> ");
	  elog_printf(DIAG, "REPLICATE TABLE\n%s", table_print(state));
#endif
     }

     /* ********** outbound replicated data ********** */
     itree_traverse(out_rings) {
          /*
	   * The inbound replication variables
	   * from        - default local ring p-url
	   * to          - default remote ring p-url
	   * local_ring  - local ring p-url
	   * remote_ring - remote ring p-url
	   * local_seq   - youngest local sequence
	   * io          - new remote data to append to local ring
	   */

	  /* elog_printf(DEBUG, "REP - search state=%p ringname=%s "
	     "ncols=%d nrows=%d body=%s\n", 
	     state, itree_get(out_rings), table_ncols(state), 
	     table_nrows(state), table_print(state)); */

          /* get replication end points and last remote sequence */
	  rep_endpoints(itree_get(out_rings), &from, &to);
	  rep_state_new_or_get(state, itree_get(out_rings), to, from, 
			       &remote_ring, &local_ring, NULL, &local_seq);

	  /* collect local data that needs to be sent */
	  io = rep_local_get(local_ring, local_seq+1, &local_max_seq);
	  if ( ! io )
	       continue;

	  /* provide a simple log */
	  elog_printf(INFO, "Sending %d sequences (%d rows) from %s to %s",
		      local_max_seq-local_seq+1, table_nrows(io), remote_ring, 
		      local_ring);

	  /* open remote ring */
	  rt = route_open(remote_ring, "", NULL, 0);
	  if (!rt) {
	       elog_printf(ERROR, "unable to open destination route %s to "
			   "replicate; continuing with next replication",
			   remote_ring);
	       table_destroy(io);
	       continue;	/* can't carry on with this ring */
	  }

	  /* post the data up to the repository */
	  r = rep_remote_put(rt, io, &rtstatus, &rtinfo);
	  route_close(rt);
	  table_destroy(io);
	  if (r < 0)
	       continue;	/* can't carry on with this ring */

	  /* collect the remote status */
	  rep_remote_status(rtstatus, rtinfo, &remote_seq, &remote_youngest_t);

	  /* collect local sequence & time */
	  if (!route_stat(local_ring, NULL, &local_seq, &r, &youngest_t))
	       elog_printf(ERROR, "can't stat local ring: %s", 
			   local_ring);

	  /* Update state table with local and remote ring details */
	  rep_state_update(state, itree_get(out_rings), local_seq, remote_seq, 
			   youngest_t);

	  /* Write state table to local storage (do it every iteration 
	   * for safety) */
	  if (!route_twrite(state_rt, state))
	       elog_printf(ERROR,"unable to save state having written to %s",
			   to);

#if 0
	  elog_printf(DIAG, "REPLICATE: state=%s", state_purl);
	  itree_strdump(in_rings,  " in-> ");
	  itree_strdump(out_rings, " out-> ");
	  elog_printf(DIAG, "REPLICATE TABLE\n%s", table_print(state));
#endif

	  /* clear up */
	  nfree(rtstatus);
	  nfree(rtinfo);
     }

     route_close(state_rt);

     return 0;		/* success */
}


/* ------- Replication helpers -------- */

/*
 * Return details & status on the local and remote rings for the 
 * replication relationship called 'name'.
 * Returns local ring name and youngest sequence, remote ring name and 
 * its youngest sequence.
 * If the relationship does not exist in the table, it is created using
 * the default ring names provided by the caller.
 * Don't nfree() the returned data as they are part of the table.
 * Copies of the default are made.
 */
void rep_state_new_or_get(TABLE state, char *name, 
			  char *default_remote, char *default_local,
			  char **actual_remote, char **actual_local,
			  int *remote_seq, int *local_seq) {

     /* find last sequence from state, using the local ring as the name */
     if (table_search(state, "name", name) == -1) {
          /* no record of it before, start a new one and fill in 
	   * some initial/default values */
          table_addemptyrow(state);
	  table_replacecurrentcell_alloc(state, "name", name);
	  table_replacecurrentcell_alloc(state, "lname", default_local);
	  table_replacecurrentcell_alloc(state, "rname", default_remote);
	  table_replacecurrentcell_alloc(state, "rseq",  "-1");
	  table_replacecurrentcell_alloc(state, "lseq",  "-1");
	  table_replacecurrentcell_alloc(state, "rep_t", "0");
     }

     /* read back values & assign to caller if they want to know and 
      * haven't passed NULLs */
     if (actual_remote)
          *actual_remote = table_getcurrentcell(state, "rname");
     if (actual_local)
          *actual_local = table_getcurrentcell(state, "lname");
     if (remote_seq)
          *remote_seq = strtol(table_getcurrentcell(state, "rseq"),
			       (char**)NULL, 10);
     if (local_seq)
          *local_seq = strtol(table_getcurrentcell(state, "lseq"), 
			      (char**)NULL, 10);

     return;
}



/* Return a table containing all the new remote data starting from
 * remote_seq or NULL for error */
TABLE rep_remote_get(char *remote_ring, int remote_seq) {
     int r;
     char *purl;
     TABLE io;

     /* download new sequences, using standard route addressing */
     r = snprintf(purl, REP_PURL_LEN, "%s,*,s=%d-", remote_ring, remote_seq);
     if (r >= REP_PURL_LEN)
          elog_die(FATAL, "purl far too long (%d); under attack?", r);
     elog_printf(DIAG, "replicating inbound (download) %s", purl);
     io = route_tread(purl, NULL);
     if (!io) {
          elog_printf(ERROR, "unable to read remote route %s as source;"
		      "aborting, moving to next replication", purl);
	  return NULL;	/* remote ring does not exist or is corrupt:
			 * try the next one in the inbound list */
     }

     return io;
}


/* Return a table containing all the new local data starting from
 * local_seq or NULL for error. Set the sequence number of the most recent
 * data in local_max_seq or -1 if there is a problem or its empty */
TABLE rep_local_get(char *local_ring, int local_seq, int *local_max_seq) {
     TABLE io;
     ROUTE rt;
     int maxseq, rtsize;
     time_t modt;

#if 0 /* superseeded by caller message */
     elog_printf(DIAG, "replicating outbound local (upload) %s,s=%d- ",
		 local_ring, local_seq);
#endif

     /* open local route, non-creating */
     rt = route_open(local_ring, "", "", 0);
     if (!rt) {
          elog_printf(ERROR, "unable to open local route %s as source;"
		      "aborting, moving to next replication", local_ring);
	  return NULL;	/* can't carry on with this ring */
     }

     /* grab last sequence status */
     if (route_tell(rt, &maxseq, &rtsize, &modt)) {
          *local_max_seq = maxseq-1;
	  if (*local_max_seq < 0)
	       *local_max_seq = -1;
     } else {
          *local_max_seq = -1;
     }

     /* select data to send */
     io = route_seektread(rt, local_seq, 0);
     route_close(rt);
     if (!io) {
          elog_printf(DIAG, "either up-to-date or unable to read local "
		      "source route %s; moving to next replication", 
		      local_ring);
	  return NULL;	/* local ring does not exist or is corrupt:
			 * try the next one in the outbound list */
     }

     /* check size of return */
     if (table_nrows(io) == 0) {
          elog_printf(DIAG, "no new rows in local route %s", local_ring);
	  table_destroy(io);
	  return NULL;	/* local ring has no rows to offer --
			 * try the next one in the outbound list */
     }

     return io;
}


/* Open or create a local ring. If creating, use information provided 
 * by the remote ring */
ROUTE rep_local_open_or_create(char *local_ring, char *remote_ring) {
     ROUTE rt;
     char *purl, *desc, *pt;
     TABLE info;
     int nslots;

     rt = route_open(local_ring, "", "", 0);
     if (!rt) {
          /* the table does not exist: get details from the repository */
          pt = strchr(remote_ring, ',');
	  if (pt) {
	       pt = strchr(pt+1, ',');
	       if (pt) {
		    /* two commas in remote route purl */
		    strncpy(purl, remote_ring, pt - remote_ring);
		    strcpy(purl + (pt - remote_ring), "?info");
	       } else {
		    /* one comma in remote route purl */
		    snprintf(purl, REP_PURL_LEN, "%s?info", remote_ring);
	       }
	       /* read ring's config */
	       info = route_tread(purl, NULL);
	  } else {
	       info = NULL;
	  }

	  /* extract information from the remote info table */
	  if (!info) {
	       /* no details from remote ring, make some up */
	       snprintf(purl, REP_PURL_LEN, "replicated import of %s", 
			remote_ring);
	       desc = purl;
	       nslots = REP_DEFAULT_NSLOTS;
	  } else {
	       table_first(info);
	       desc = strncpy(purl, 
			      table_getcurrentcell(info, "description"), 
			      REP_PURL_LEN);
	       nslots = strtol(table_getcurrentcell(info, 
						    "number of slots"), 
			       (char**)NULL, 10);
	       table_destroy(info);
	  }

	  /* create local ring using the remote details */
	  rt = route_open(local_ring, desc, NULL, nslots);
	  if (!rt) {
	       elog_printf(ERROR, "unable to write to local route %s "
			   "(%d slots) having imported from %s "
			   "successfully", 
			   local_ring, nslots, remote_ring);
	       return NULL;	/* unable to complete this replication
				 * try again with the next one */
	  }
     }

     return rt;
}



/* Save inbound data. 
 * The data needs its sequence rebased to suit the local rings */
void rep_local_save(ROUTE rt, TABLE io) {
     ITREE *seq_i;
     TREE *seq_a;
     TABLE seq_data;

     /* save each sequence of inbound data
      * Do this getting the unique sequences, transform them to a list
      * of integers (so they are sorted), then save each table subset
      * of sequence data separately */
     seq_i = itree_create();
     seq_a = table_uniqcolvals(io, "_seq", NULL);
     tree_traverse(seq_a)
          itree_add(seq_i, strtol(tree_getkey(seq_a), (char**)NULL, 10), 
		    tree_getkey(seq_a));
     itree_traverse(seq_i) {
          seq_data = table_selectcolswithkey(io, "_seq", 
					     (char *) itree_get(seq_i),
					     NULL);
	  if ( ! route_twrite(rt, seq_data) )
	       elog_printf(ERROR, "unable to write seq=%d to local "
			   "route %s", itree_get(seq_i), route_getpurl(rt));
	  table_destroy(seq_data);
     }
     itree_destroy(seq_i);
     tree_destroy(seq_a);
}


/* Update the state of a named replication relationship in the stat table.
 * Everything is allocated, none of the storage of the callers is needed */
void rep_state_update(TABLE state, char *name, int local_seq, int remote_seq, 
		      time_t youngest_t) {

     /* make sure the named record is current */
     if (table_search(state, "name", name) == -1) {
          elog_printf(ERROR,"unable to find state for '%s' which should "
		      "be there!! Can't save record details, state will "
		      "not be correct; continuing with next record", name);
	  return;
     }


     /* update the record: get remote sequence and youngest remote data time 
      * from the data (io) */
     table_replacecurrentcell_alloc(state, "lseq", util_i32toa(local_seq));
     table_replacecurrentcell_alloc(state, "rseq", util_i32toa(remote_seq));
     table_replacecurrentcell_alloc(state, "youngest_t", 
				    util_i32toa(youngest_t));
     table_replacecurrentcell_alloc(state, "rep_t", util_i32toa(time(NULL)));

}



/* Write a table to the remote location and retrieve the status
 * Returns the number of characters written or -1 for error */
int  rep_remote_put(ROUTE rt, TABLE io, char **rtstatus, char **rtinfo) {
     int r;

     /* run send method using supplied opened route & grab the status */
     r = route_twrite(rt, io);
     route_getstatus(rt, rtstatus, rtinfo);

     if ( !r ) {
          /* failure - try to find the status */
	  if (rtstatus) {
	       elog_printf(ERROR, "failed to replicate to repository "
			   "address '%s': %s %s", route_getpurl(rt), 
			   *rtstatus, *rtinfo);
	       nfree(*rtstatus);
	       nfree(*rtinfo);
	  } else {
	       elog_printf(ERROR, "failed to replicate to repository "
			   "address '%s', no status", route_getpurl(rt));
	  }
	  return -1;
     }

     return r;
}


/* Work out the remote sequence and time from the return status (if possible)
 * or try other methods which may be more expensive. If it is not possible 
 * to aquire the data, call the _t will be set to 0, _s is set to -1. */
void  rep_remote_status(char *rtstatus, char *rtinfo, int *remote_youngest_s,
			time_t *remote_youngest_t) {

     char *buf, *ry_t=NULL, *ry_s=NULL;
     int len;
     TABLE info;

     /* For efficiency, attempt to parse the successful info message
      * from the post to the repository for sequences and times.
      * If it fails then we need to revert to a remote status call
      * to get the same info, which will slow us down...
      */
     if (rtstatus && rtinfo) {
          len = strlen(rtinfo);
	  /* cut out the youngest time from return buffer */
	  buf = strstr(rtinfo, "youngest_t");
	  if (buf) {
	       buf += 10;
	       buf += strspn(buf, " ");
	       len = strcspn(buf, " ");
	       ry_t = xnmemdup(buf, len+1);
	       ry_t[len] = '\0';

	       /* cut out the youngest sequence from return buffer */
	       buf = strstr(rtinfo, "youngest_s");
	       if (buf) {
		    buf += 10;
		    buf += strspn(buf, " ");
		    len = strcspn(buf, " ");
		    ry_s = xnmemdup(buf, len+1);
		    ry_s[len] = '\0';
	       } else {
		    /* bufer does not contain 'youngest_s' */
		    ry_s = NULL;
	       }
	  } else {
	       /* bufer does not contain 'youngest_t' */
	       ry_t = NULL;
	       ry_s = NULL;
	  }
     } else {
          /* no return information */
          ry_t = NULL;
	  ry_s = NULL;
     }


     if ( ! ry_s ) {
          /* No return info or status, so make a specific request */

          /* TO BE IMPLEMENTED */
          info = route_tread("sqlrs:remote_ring", NULL);
	  if (!info) {
	       elog_printf(ERROR, "no repository state returned but "
			   "outbound replication suceeded: unable to save "
			   "state, out of sync");
	       if (remote_youngest_s)
		    *remote_youngest_s = -1;
	       if (remote_youngest_t)
		    *remote_youngest_t = 0;
	       return;
	  }

	  table_last(info);	/* ASSUMPTION! that io is seq/time ordered
				 * and the last row is the youngest */
	  ry_s = xnstrdup(table_getcurrentcell(info, "youngest"));
	  ry_t = xnstrdup(table_getcurrentcell(info, "youngest_t"));
     }

     /* convert to numeric */
     if (remote_youngest_s)
          if (ry_s)
	       *remote_youngest_s = strtol(ry_s, (char**)NULL, 10);
	  else
	       *remote_youngest_s = -1;
     if (remote_youngest_t)
          if (ry_t)
	       *remote_youngest_t = strtol(ry_t, (char**)NULL, 10);
	  else
	       *remote_youngest_t = 0;

     if (ry_s)
          nfree(ry_s);
     if (ry_t)
          nfree(ry_t);
}




/*
 * Scan the inbound text buffer into a data table and summerise
 * details of the last datum to record state.
 * Returns the table if successful, in which case local_seq, remote_seq 
 * and youngest_t will be set, or return NULL for error
 */
TABLE rep_scan_inbound(char *buf, int len, int *local_seq, int *remote_seq, 
		       time_t *youngest_t) {
     if (!buf)
	  return NULL;	/* no data to load */

     return NULL;	/* failure */
}

/* gather fresh data from local ringstores and concatinate in a table
 * ready for transfer. Pass the replication state table to work out
 * the rings to look at and work out what is new.
 * Return a table of data to write out, which should be empty if there 
 * is nothing to do or NULL if there has been an error
 */
TABLE rep_gather_outbound(TABLE state) {
     return NULL;
}


/* read a replication directive and return the two endpoints as
 * nmalloc'ed strings (which should be freed). If there is only
 * one ring specified, both ringpoints will have the same value
 * (but will different allocations).
 * the character '<' or '>' is used to specify the direction of 
 * replication and separate the two ringnames.
 */
void rep_endpoints(char *directive, char **from, char **to) {
     char *pt;

     if (!directive) {
	  *to   = xnstrdup("");
	  *from = xnstrdup("");
     }
     if ((pt = strchr(directive, '<'))) {
	  *to   = xnmemdup(directive, pt-directive+1);
	  *from = xnstrdup(pt+1);
	  (*to)[pt-directive] = '\0';
     } else if ((pt = strchr(directive, '>'))) {
	  *to   = xnstrdup(pt+1);
	  *from = xnmemdup(directive, pt-directive+1);
	  (*from)[pt-directive] = '\0';
     } else {
	  *to   = xnstrdup(directive);
	  (*from) = xnstrdup(directive);
     }

     return;
}



#if TEST
#include <unistd.h>
#include "iiab.h"
#include "route.h"
#define TFILE1 "t.rep.rs"
#define TSTATE1 "state"
#define TSTATEPURL1 "rs:" TFILE1 "," TSTATE1 ",0"

int main(int argc, char **argv) {
     ITREE *in_ring1, *out_ring1;
     char buf1[50], buf2[50], buf3[50], buf4[50], buf5[50], buf6[50];
     TABLE tab1, tab2;
     int seq1, seq2;
     ROUTE out, err;

     iiab_start("", argc, argv, "", "");
     out = route_open("stdout", NULL, NULL, 10);
     err = route_open("stderr", NULL, NULL, 10);

     unlink(TFILE1);

     /* set up some prerequesits */
     cf_addstr(iiab_cf, "route.sqlrs.geturl", 
	       "http://localhost/harvestapp/pl/sqlrs_get.pl");
     cf_addstr(iiab_cf, "route.sqlrs.puturl", 
	       "http://localhost/harvestapp/pl/sqlrs_put.pl");

     /* test 1: use daft values */
     in_ring1  = itree_create();
     out_ring1 = itree_create();
     route_expand(buf1, "sqlrs:%h,tom>rs:rep.%h.rs,tom", "NOJOB");
     route_expand(buf2, "sqlrs:%h,dick>rs:rep.%h.rs,dick", "NOJOB");
     route_expand(buf3, "sqlrs:%h,harry>rs:rep.%h.rs,harry", "NOJOB");
     route_expand(buf4, "rs:rep.%h.rs,rita>sqlrs:%h,rita", "NOJOB");
     route_expand(buf5, "rs:rep.%h.rs,sue>sqlrs:%h,sue", "NOJOB");
     route_expand(buf6, "rs:rep.%h.rs,bob>sqlrs:%h,bob", "NOJOB");
     itree_append(in_ring1,  buf1);
     itree_append(in_ring1,  buf2);
     itree_append(in_ring1,  buf3);
     itree_append(out_ring1, buf4);
     itree_append(out_ring1, buf5);
     itree_append(out_ring1, buf6);
     if (rep_action(out, err, in_ring1, out_ring1, TSTATEPURL1) == -1)
	  elog_die(FATAL, "[1] unable to replicate");

     itree_destroy(in_ring1);
     itree_destroy(out_ring1);

     /* test 2: use a single real value from the harvest 'justcpu' demo */
     in_ring1  = itree_create();
     out_ring1  = itree_create();
     route_expand(buf1, 
		  "sqlrs:clifton,justcpu,*>tab:" TFILE1 ",r.justcpu3600", 
		  "NOJOB");
     itree_append(in_ring1, buf1);
     if (rep_action(out, err,in_ring1,out_ring1,TSTATEPURL1) == -1)
	  elog_die(FATAL, "[2a] unable to replicate");

     /* get state table: there should only be 1 row as nothing from test 1
      * should have succeeded in creating a state */
     tab1 = route_tread(TSTATEPURL1, NULL);
     if (!tab1)
	  elog_die(FATAL, "[2a] no state table returned");
     if (table_nrows(tab1) != 1)
	  elog_die(FATAL, "[2a] wrong number of rows: %d", 
		   table_nrows(tab1));
     table_first(tab1);
     if (strcmp("sqlrs:clifton,justcpu,*>tab:" TFILE1 ",r.justcpu3600", 
		table_getcurrentcell(tab1, "name")) != 0)
	  elog_die(FATAL, "[2a] name is not expected: %s",
		   table_getcurrentcell(tab1, "name"));
     seq1 = strtol(table_getcurrentcell(tab1, "lseq"), (char**)NULL, 10);
     if (seq1 == 0)
	  elog_die(FATAL, "[2a] lseq should not be 0");
     seq1 = strtol(table_getcurrentcell(tab1, "rseq"), (char**)NULL, 10);
     if (seq1 == 0)
	  elog_die(FATAL, "[2a] rseq should not be 0");

     /* replicate again and check nothing has changed in the state */
     if (rep_action(out, err,in_ring1,out_ring1,TSTATEPURL1) == -1)
	  elog_die(FATAL, "[2b] unable to replicate");
     tab2 = route_tread(TSTATEPURL1, NULL);
     if (!tab2)
	  elog_die(FATAL, "[2b] no state table returned");
     table_first(tab2);
     seq1 = strtol(table_getcurrentcell(tab1, "lseq"), (char**)NULL, 10);
     seq2 = strtol(table_getcurrentcell(tab2, "lseq"), (char**)NULL, 10);
     if (seq1 != seq2)
	  elog_die(FATAL, 
		   "[2b] lseq is different after a empty rep: %d != %d", 
		   seq1, seq2);
     seq1 = strtol(table_getcurrentcell(tab1, "rseq"), (char**)NULL, 10);
     seq2 = strtol(table_getcurrentcell(tab2, "rseq"), (char**)NULL, 10);
     if (seq1 != seq2)
	  elog_die(FATAL, 
		   "[2b] rseq is different after a empty rep: %d != %d", 
		   seq1, seq2);

     itree_destroy(in_ring1);
     itree_destroy(out_ring1);
     table_destroy(tab1);
     table_destroy(tab2);

     /* test 3: upload a test table into the repository */
     in_ring1  = itree_create();
     out_ring1  = itree_create();
     route_expand(buf1, 
		  "sqlrs:%h,reptest,3600<tab:" TFILE1 ",r.justcpu3600", 
		  "NOJOB");
     itree_append(out_ring1, buf1);
     if (rep_action(out, err,in_ring1,out_ring1,TSTATEPURL1) == -1)
	  elog_die(FATAL, "[3a] unable to replicate");

     tab1 = route_tread(TSTATEPURL1, NULL);
     if (!tab1)
	  elog_die(FATAL, "[3a] no state table returned");
     puts(table_print(tab1));
#if 0
     table_first(tab1);
     seq1 = strtol(table_getcurrentcell(tab1, "lseq"), (char**)NULL, 10);
#endif

     fprintf(stderr, "%s: tests finished successfully\n", argv[0]);
     route_close(out);
     route_close(err);
     iiab_stop();
     exit(0);
}

#endif /* TEST */
