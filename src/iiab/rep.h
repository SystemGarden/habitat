/*
 * Replicate - send and recieve ring entries to or from a standard repository
 * Nigel Stuckey, April 2003
 *
 * Copyright System Garden Ltd 2000-2003, all rights reserved.
 */

#ifndef _REP_H_
#define _REP_H_

#include "tree.h"
#include "itree.h"
#include "table.h"
#include "route.h"
#include "elog.h"

/* definitions */
#define REP_PURL_LEN 200
#define REP_DEFAULT_NSLOTS 1000
#define REP_STATE_HDS "name\tlname\trname\tlseq\trseq\tyoungest_t\trep_t\n" \
                        "name of replication relationship\t"		    \
		        "local ring address\t"				    \
		        "remote ring address\t"				    \
		        "last local sequence replicated\t"		    \
		        "last remote sequence replicated\t"		    \
		        "time stamp of local sequence last replicated\t"    \
		        "time last replication was started\t"  "info"

/* replication state structure */
typedef struct rep_state {
     char * name;	/* Name of replication relationship */
     char * lname;	/* local ring address */
     char * rname;	/* remote ring address */
     int    lseq;	/* last local sequence replicated */
     int    rseq;	/* last remote sequence replicated */
     time_t youngest_t;	/* Time of youngest datum replicated (GMT) */
     time_t rep_t;	/* Replication time (GMT) */
} REP_STATE;


int   rep_action(ROUTE out, ROUTE err, ITREE *rings_in, ITREE *rings_out, 
		 char *ring_state);

/* local */
void  rep_endpoints(char *directive, char **from, char **to);
void  rep_state_new_or_get(TABLE state, char *name, 
			   char *default_remote, char *default_local,
			   char **actual_remote, char **actual_local,
			   int *remote_seq, int *local_seq);
TABLE rep_remote_get(char *remote_ring, int remote_seq);
TABLE rep_local_get(char *local_ring, int local_seq);
ROUTE rep_local_open_or_create(char *local_ring, char *remote_ring);
void  rep_local_save(ROUTE rt, TABLE io);
void  rep_state_update(TABLE state, char *name, int local_seq, 
		       int remote_seq, time_t youngest_t);
int   rep_remote_put(ROUTE rt, TABLE io, char **rtstatus, char **rtinfo);
void  rep_remote_status(char *rtstatus, char *rtinfo, int *remote_youngest_s,
			time_t *remote_youngest_t);


TABLE rep_scan_inbound(char *buf, int len, int *local_seq, int *remote_seq, 
		       time_t *youngest_t);
TABLE rep_gather_outbound(TABLE state);

#endif /* _REP_H_ */
