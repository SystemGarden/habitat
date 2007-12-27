/*
 * Span Store include file
 *
 * Nigel Stuckey, February 1999
 * Copyright System Garden Ltd 1999-2001. All rights reserved.
 */

#ifndef _NSPANSTORE_H_
#define _NSPANSTORE_H_

#include <time.h>
#include "tree.h"
#include "table.h"
#include "timestore.h"

#define SPANSTORE_KEYLEN 128
#define SPANSTORE_DATASPACE "__span_"
#define SPANSTORE_SPACEDELIM '.'
#define SPANS TABLE
#define SPANS_FROMCOL "from_seq"
#define SPANS_TOCOL "to_seq"
#define SPANS_FROMDTCOL "from_time"
#define SPANS_TODTCOL "to_time"
#define SPANS_DATACOL "header"
#define SPANS_NOHUNT 0
#define SPANS_HUNTPREV 1
#define SPANS_HUNTNEXT 2
#define spans_create table_create
#define spans_freels(d) hol_freesearch(d)

SPANS spans_readblock (TS_RING ts);
int   spans_writeblock(TS_RING ts, SPANS tab);
int   spans_new       (SPANS tab, int from, int to, time_t fromdt, 
		       time_t todt, char *data);
int   spans_getlatest (SPANS tab, int *from, int *to, time_t *fromdt, 
		       time_t *todt, char **data);
int   spans_getoldest (SPANS tab, int *from, int *to, time_t *fromdt, 
		       time_t *todt, char **data);
int   spans_getseq    (SPANS tab, int seq, int *from, int *to, time_t *fromdt, 
		       time_t *todt, char **data);
int   spans_gettime   (SPANS tab, time_t dt, int findnearest, int *from, 
		       int *to, time_t *fromdt, time_t *todt, char **data);
int   spans_search    (SPANS tab, char *data, int *from, int *to, 
		       time_t *fromdt, time_t *todt);
int   spans_extend    (SPANS tab, int from, int to, int newto, time_t newtodt);
int   spans_purge     (SPANS tab, int oldestseq, time_t oldestdt);
SPANS spans_readringblocks(HOLD hol);
int   spans_overlap   (SPANS tab, int from, int to);
TREE *spans_lsringshol(HOLD h);
TREE *spans_getnameroots(SPANS ringblocks);
TREE *spans_getrings_byrootandtime(SPANS ringblocks, char *nameroot, 
				   time_t fromdt, time_t todt, 
				   time_t *ret_begin, time_t *ret_end);
TREE *spans_getrings_byseqrange(SPANS ringblocks, int fromseq, int toseq);


#endif /* _NSPANSTORE_H_ */
