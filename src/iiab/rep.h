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
		      "time stamp of local sequence last replicated\t"	    \
		      "time last replication was started\tinfo"

int   rep_action(ROUTE out, ROUTE err, ITREE *rings_in, ITREE *rings_out, 
		 char *ring_state);
TABLE rep_scan_inbound(char *buf, int len, int *local_seq, int *remote_seq, 
		       time_t *youngest_t);
TABLE rep_gather_outbound(TABLE state);
void  rep_endpoints(char *directive, char **from, char **to);

#endif /* _REP_H_ */
