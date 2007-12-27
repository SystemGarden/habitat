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
 * Hourly samples are downloaded.
 * Return -1 for error.
 */
int rep_action(ROUTE out,		/* route output */
	       ROUTE err,		/* route errors */
	       ITREE *in_rings,		/* list of rings to input */
	       ITREE *out_rings,	/* list of rings to output */
	       char *state_purl		/* purl to store state */ )
{
     ROUTE state_rt, rt;
     TABLE state, io, info, seq_data;
     ITREE *seq_i;
     TREE  *seq_a;
     int r, local_seq, remote_seq, nslots;
     time_t youngest_t;
     char *local_ring, *remote_ring, purl[REP_PURL_LEN], *buf, *from, *to;
     char *desc, *pt;

     /* check arguments */
     if (state_purl == NULL || *state_purl == '\0') {
	  elog_printf(ERROR, "no state storage supplied");
	  return -1;
     }

     /* get state in table */
     state = route_tread(state_purl, NULL);
     if (!state) {
	  buf   = xnstrdup(REP_STATE_HDS);
	  state = table_create_s(buf);
	  table_freeondestroy(state, buf);
     }

     /* set up the route for saving the state of the replication
      * we only need one copy, so should ideally be a holstore  */
     state_rt = route_open(state_purl, "replication state", NULL, 1);
     if (! state_rt) {
	  elog_die(ERROR, "unable to open state storage (%s)", state_purl);
	  table_destroy(state);
	  return -1;
     }

#if 0
     elog_printf(DIAG, "REPLICATE: state=%s", state_purl);
     itree_strdump(in_rings,  " in-> ");
     itree_strdump(out_rings, " out-> ");
     elog_printf(DIAG, "REPLICATE TABLE\n%s", table_print(state));
#endif

     /* *** inbound *** */
     itree_traverse(in_rings) {
	  rep_endpoints(itree_get(in_rings), &from, &to);

	  /* find last sequence from state, using the local ring as the name */
	  if (table_search(state, "name", itree_get(in_rings)) == -1) {
	       /* no record of it before, start a new one and fill in 
		* some initial values */
	       table_addemptyrow(state);
	       table_replacecurrentcell_alloc(state, "name",  
					      itree_get(in_rings));
	       table_replacecurrentcell_alloc(state, "lname", to);
	       table_replacecurrentcell_alloc(state, "rname", from);
	       table_replacecurrentcell_alloc(state, "rseq",  "-1");
	       table_replacecurrentcell_alloc(state, "lseq",  "-1");
	       table_replacecurrentcell_alloc(state, "rep_t", "0");
	  }

	  /* read back values */
	  local_ring  = table_getcurrentcell(state, "lname");
	  remote_ring = table_getcurrentcell(state, "rname");
	  remote_seq  = atoi(table_getcurrentcell(state, "rseq"));

	  /* run fetch method using standard ringstore addressing */
	  r = snprintf(purl, REP_PURL_LEN, "%s,*,s=%d-", remote_ring, 
		       remote_seq+1);
	  if (r >= REP_PURL_LEN)
	       elog_die(FATAL, 
			"purl far too long (%d); under attack?", r);
	  elog_printf(DIAG, "replicating inbound %s from repository to store "
		      "locally as %s", purl, local_ring);
	  io = route_tread(purl, NULL);
	  if (!io) {
	       elog_printf(DIAG, "unable to read remote route %s as source", 
			   purl);
	       continue;	/* remote ring does not exist or is corrupt:
				 * try the next one in the inbound list */
	  }

	  /* save the table to the local ring */
	  rt = route_open(local_ring, "", "", 0);
	  if (!rt) {
	       /* the table does not exist: get details from the repository */
	       pt = strchr(remote_ring, ',');
	       if (pt) {
		    pt = strchr(pt+1, ',');
		    if (pt) {
			 /* two commas */
			 strncpy(purl, remote_ring, pt - remote_ring);
			 strcpy(purl + (pt - remote_ring), "?info");
		    } else {
			 /* one comma */
			 snprintf(purl, REP_PURL_LEN, "%s?info", remote_ring);
		    }
		    /* read ring's config */
		    info = route_tread(purl, NULL);
	       } else {
		    info = NULL;
	       }
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
		    nslots = atoi(table_getcurrentcell(info, 
						       "number of slots"));
		    table_destroy(info);
	       }
	       rt = route_open(local_ring, desc, NULL, nslots);
	       if (!rt) {
		    elog_printf(ERROR, "unable to write to local route %s "
				"(%d slots) having imported from %s "
				"successfully", 
				local_ring, nslots, remote_ring);
		    table_destroy(io);
		    continue;	/* unable to complete this replication
				 * try again with the next one */
	       }
	  }

	  /* NOTE: Save inbound data.
	   * If using ringstore, we could save the whole table as a single
	   * operation and be faster.
	   * However, the code below is for tablestores and other 
	   * similar methods */

	  /* save each sequence of inbound data
	   * Do this getting the unique sequences, transform them to a list
	   * of integers (so they are sorted), then save each table subset
	   * of sequence data separately */
	  seq_i = itree_create();
	  seq_a = table_uniqcolvals(io, "_seq", NULL);
	  tree_traverse(seq_a)
	       itree_add(seq_i, atoi(tree_getkey(seq_a)), tree_getkey(seq_a));
	  itree_traverse(seq_i) {
	       seq_data = table_selectcolswithkey(io, "_seq", 
						  (char *) itree_get(seq_i),
						  NULL);
	       if ( ! route_twrite(rt, seq_data) )
		    elog_printf(ERROR, "unable to write seq=%d to local "
				"route %s", itree_get(seq_i), local_ring);
	       table_destroy(seq_data);
	  }
	  itree_destroy(seq_i);
	  tree_destroy(seq_a);
	  route_close(rt);

	  /* last local location */
	  if (!route_stat(local_ring, NULL, &local_seq, &r, &youngest_t))
	       elog_printf(ERROR, "can't stat local ring: %s", 
			   local_ring);

	  /* Write state table to storage every time a fetch has been done.
	   * Whilst inefficient, it is far safer */
	  table_last(io);	/* ASSUMPTION! that io is seq/time ordered
				 * and the last row is the youngest */
	  table_replacecurrentcell_alloc(state, "lseq", 
					 util_i32toa(local_seq));
	  table_replacecurrentcell_alloc(state, "rseq", 
					 table_getcurrentcell(io, "_seq"));
	  table_replacecurrentcell_alloc(state, "youngest_t", 
					 table_getcurrentcell(io, "_time"));
	  table_replacecurrentcell_alloc(state, "rep_t", 
					 util_i32toa(time(NULL)));
	  if (!route_twrite(state_rt, state))
	       elog_printf(ERROR, "unable to save state having read in %s", to);
     }

     /* *** outbound *** */
     tree_traverse(out_rings) {
	  rep_endpoints(itree_get(out_rings), &from, &to);

	  /* find last sequence from state */
	  /* elog_printf(DEBUG, "REP - search state=%x ringname=%s "
	     "ncols=%d nrows=%d body=%s\n", 
	     state, itree_get(out_rings), table_ncols(state), 
	     table_nrows(state), table_print(state)); */
	  if (table_search(state, "name", itree_get(out_rings)) == -1) {
	       /* no record of it before, start a new one and fill in 
		* some initial values */
	       table_addemptyrow(state);
	       table_replacecurrentcell_alloc(state, "name",  
					      itree_get(out_rings));
	       table_replacecurrentcell_alloc(state, "lname", from);
	       table_replacecurrentcell_alloc(state, "rname", to);
	       table_replacecurrentcell_alloc(state, "lseq",  "-1");
	       table_replacecurrentcell_alloc(state, "rseq",  "-1");
	       table_replacecurrentcell_alloc(state, "rep_t", "0");
	  }

	  /* read back state values */
	  local_ring  = table_getcurrentcell(state, "lname");
	  remote_ring = table_getcurrentcell(state, "rname");
	  local_seq   = atoi(table_getcurrentcell(state, "lseq"));

	  /* select data to send */
	  elog_printf(DIAG, "replicating outbound local %s,s=%d- "
		      "to store as %s on repository", 
		      local_ring, local_seq, remote_ring);
	  rt = route_open(local_ring, "", "", 0);
	  if (!rt) {
	       elog_printf(ERROR, "unable to open source route %s to "
			   "replicate", local_ring);
	       continue;	/* can't carry on with this ring */
	  }
	  io = route_seektread(rt, local_seq+1, 0);
	  route_close(rt);
	  if (!io) {
	       elog_printf(DIAG, "either up-to-date or "
			   "unable to read local source route %s", 
			   local_ring);
	       continue;	/* local ring does not exist or is corrupt:
				 * try the next one in the outbound list */
	  }
	  if (table_nrows(io) == 0) {
	       elog_printf(DIAG, "no new rows in local route %s", 
			   local_ring);
	       table_destroy(io);
	       continue;	/* local ring has no rows to offer --
				 * try the next one in the outbound list */
	  }

	  /* run send method using address supplied */
	  rt = route_open(remote_ring, "", "", 0);
	  if (!rt) {
	       elog_printf(ERROR, "unable to open destination route %s to "
			   "replicate", remote_ring);
	       table_destroy(io);
	       continue;	/* can't carry on with this ring */
	  }
	  if (!route_twrite(rt, io)) {
	       buf = route_read("sqlrs:_WRITE_STATUS_", NULL, &r);
	       elog_printf(ERROR, "failed to replicate to repository "
			   "address %s error returned: %s", 
			   remote_ring, buf);
	       nfree(buf);
	       route_close(rt);
	       table_destroy(io);
	       continue;	/* can't carry on with this ring */
	  }
	  route_close(rt);
	  table_destroy(io);

	  /* collect sequence & time - locally and remotely */
	  info = route_tread("sqlrs:_WRITE_RETURN_", NULL);
	  if (!info) {
	       elog_printf(ERROR, "no repository state returned but "
			   "outbound replication suceeded: unable to save "
			   "state, out of sync");
	       continue;
	  }
	  if (!route_stat(local_ring, NULL, &local_seq, &r, &youngest_t))
	       elog_printf(ERROR, "can't stat local ring: %s", 
			   local_ring);

	  /* Write state table to storage every time a fetch has been done.
	   * Whilst inefficient, it is far safer */
	  table_last(info);	/* ASSUMPTION! that io is seq/time ordered
				 * and the last row is the youngest */
	  table_replacecurrentcell_alloc(state, "lseq", 
					 util_i32toa(local_seq));
	  table_replacecurrentcell_alloc(state, "rseq", 
					 table_getcurrentcell(info, 
							      "youngest") );
	  table_replacecurrentcell_alloc(state, "youngest_t", 
					 table_getcurrentcell(info,
							      "youngest_t") );
	  table_replacecurrentcell_alloc(state, "rep_t", 
					 util_i32toa(time(NULL)));
	  if (!route_twrite(state_rt, state))
	       elog_printf(ERROR, "unable to save state having written to %s",
			   to);
	  table_destroy(info);
     }

     route_close(state_rt);

     return 0;		/* success */
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
     seq1 = atoi(table_getcurrentcell(tab1, "lseq"));
     if (seq1 == 0)
	  elog_die(FATAL, "[2a] lseq should not be 0");
     seq1 = atoi(table_getcurrentcell(tab1, "rseq"));
     if (seq1 == 0)
	  elog_die(FATAL, "[2a] rseq should not be 0");

     /* replicate again and check nothing has changed in the state */
     if (rep_action(out, err,in_ring1,out_ring1,TSTATEPURL1) == -1)
	  elog_die(FATAL, "[2b] unable to replicate");
     tab2 = route_tread(TSTATEPURL1, NULL);
     if (!tab2)
	  elog_die(FATAL, "[2b] no state table returned");
     table_first(tab2);
     seq1 = atoi(table_getcurrentcell(tab1, "lseq"));
     seq2 = atoi(table_getcurrentcell(tab2, "lseq"));
     if (seq1 != seq2)
	  elog_die(FATAL, 
		   "[2b] lseq is different after a empty rep: %d != %d", 
		   seq1, seq2);
     seq1 = atoi(table_getcurrentcell(tab1, "rseq"));
     seq2 = atoi(table_getcurrentcell(tab2, "rseq"));
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
     seq1 = atoi(table_getcurrentcell(tab1, "lseq"));
#endif

     fprintf(stderr, "%s: tests finished successfully\n", argv[0]);
     route_close(out);
     route_close(err);
     iiab_stop();
     exit(0);
}

#endif /* TEST */
