/* 
 * Ringstore
 * 
 * Provides flexible storage and quick access of time series data in 
 * database file. Designed for Habitat, implemented on Mird, providing 
 * storage for TABLE data types
 *
 * Nigel Stuckey, July 2001
 *
 * Copyright System Garden Limited 2001. All rights reserved.
 */

#ifndef _RINGSTORE_H_
#define _RINGSTORE_H_

#include <time.h>
#include "tree.h"
#include "itree.h"
#include "nmalloc.h"
#include "table.h"
#include "elog.h"
#include "mird.h"

/*
 * Description of Ringstore
 *
 * This class stores tabular data over time in sequence.
 *
 * It is implemented using Mird as the low level storage system
 * and is optimised for the retreival of values ordered over time 
 * of a single attribute of data.
 *
 * The storage implements a set of persistant ring buffers in a single
 * disk file (with maybe some key files), with limited or unlimited length
 * (can be a ringbuffer or a queue). If limited in length, old data is 
 * lost as new data `overwrites' its slot.
 * Within each slot, data is input in rows of attributes (using TABLE 
 * data types) where the values share a common sample time. 
 * Multiple instances of the same data type (such as performance of 
 * multiple disks) are held in separate rows in the same sample and 
 * resolved by identifing unique keys.
 * Unique sequencies are automatically allocated to resolve high frequency
 * data (time is only represented in seconds).
 * The default behaviour of insertion may be changed by specifing
 * meta data in the TABLE columns on insertion to give greater flexability.
 * The API is stateful, like file access. You seek, read one or many 
 * records, etc and you close.
 *
 */

/* definitions */
#define RS_CREATE 1
#define RS_ENOINIT   0
#define RS_ENOCREATE 1
#define RS_ENOSYNC   2
#define RS_ENOCLOSE  3
#define RS_ENOOPEN   4
#define RS_ENOREINIT 5

/* This structure is the handle for ringstore operations and is created
 * by rs_open(). */
struct rs_session {
     struct mird *db;
};

typedef struct rs_session * RS;

/* Functional prototypes */
/* file and ring */
void  rs_init();
void  rs_fini();
RS    rs_open(char *filename, int filemode, char *ringname,
	      char *description, int nslots, int flags);
void  rs_close(RS ring);
int   rs_destroy(char *filename, char *ringname);

/* stateful TABLE oriented transfer */
int   rs_put(RS ring, TABLE data);
TABLE rs_get(RS ring);
int   rs_replace(RS ring, TABLE data);
TABLE rs_mget_byseqs(RS ring, int nsequences);
TABLE rs_mget_bytime(RS ring, time_t last_t);

/* stateful TABLE oriented positioning */
int   rs_current  (RS ring, int *sequence, time_t *time);
int   rs_youngest (RS ring, int *sequence, time_t *time);
int   rs_oldest   (RS ring, int *sequence, time_t *time);
int   rs_rewind   (RS ring, int nsequencies);
int   rs_forward  (RS ring, int nsequencies);
int   rs_goto_seq (RS ring, int sequence);
int   rs_goto_time(RS ring, time_t time);

/* stateless column oriented reading */
TREE  *rs_colnames_byseqs(RS ring, int from_seq,   int to_seq);
TREE  *rs_colnames_bytime(RS ring, int from_t,     int to_t);
ITREE *rs_getcol_byseq   (RS ring, char *colname,  int from_seq, int to_seq);
ITREE *rs_getcol_bytime  (RS ring, char *colname,  int from_t,   int to_t);
TABLE  rs_getcols_byseq  (RS ring, TREE *colnames, int from_seq, int to_seq);
TABLE  rs_getcols_bytime (RS ring, TREE *colnames, int from_t,   int to_t);

/* file & ring - modification & information */
int   rs_resize(RS ring, int newslots);
int   rs_purge (RS ring, int nkill);
int   rs_tell  (RS ring, char **ret_filename, char **ret_ringname, 
		int *ret_nrings, int *ret_nslots, int *ret_nread, 
		int *ret_nunread, char **ret_description);
TABLE rs_lsrings(char *filename);
char *rs_filename(RS ring);
char *rs_ringname(RS ring);
int   rs_footprint(RS ring);
int   rs_remain(RS ring);
int   rs_errno();
char *rs_errstr(int errno);

/* Macro definitions */

#endif /* _RINGSTORE_H_ */
