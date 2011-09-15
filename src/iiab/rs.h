/* 
 * Ringstore
 * 
 * Provides flexible storage and quick access of time series data in 
 * database file. Designed for Habitat, implementing an low level
 * abstract interface and providing storage for TABLE data types
 *
 * Nigel Stuckey, September 2001
 *
 * Copyright System Garden Limited 2001. All rights reserved.
 */

#ifndef _RS_H_
#define _RS_H_

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tree.h"
#include "itree.h"
#include "nmalloc.h"
#include "table.h"
#include "elog.h"

/*
 * Description of Ringstore
 *
 * This class stores tabular data over time in sequence.
 * Multiple rings can co-exist in a single file.
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
 * IMPLEMENTATION
 *
 * The public interface and the high level implementation is provided
 * by this class. It calls a set of pluggable low level routines that 
 * actually store the information to disk and manage the indexes.
 * When an ringstore file is opened, a set of vectors are passed that
 * describe the low level methods.
 *
 * Ring details are kept in a single description table and will be
 * cached for fast access.
 * When table data is stored, the headers are removed and stored in a
 * dictionary indexed by a hash of the contents. The table data is 
 * then stored with the hash key of the header, the time and its key
 * is the ring id and sequence.
 * The time and sequence is stored in an index for fast retreaval
 * of data.
 *
 */

/* ------ declarations ------ */
#define RS_SUPER_VERSION	2
#define RS_CREATE		1
#define RS_VALSEP		"\t"


/* ------ enumerations ------ */
enum rs_db_writable {
     RS_RW,	/* read write */
     RS_RO	/* read only */
};

enum rs_db_lock {
     RS_WRLOCK,		/* normal write lock request (make several attempts) */
     RS_WRLOCKNOW,	/* immediate write lock request (just once) */
     RS_RDLOCK,		/* normal read lock request (make several attempts) */
     RS_RDLOCKNOW,	/* immediate read lock request (just once) */
     RS_CRLOCKNOW,	/* immediate create lock request (just once) */
     RS_UNLOCK
};

enum rs_lld_type {
     RS_LLD_TYPE_NONE,	/* no descriptor */
     RS_LLD_TYPE_GDBM,	/* GDBM descriptor */
     RS_LLD_TYPE_BERK	/* Berkley DB descriptor */
};


/* ----- externs ----- */
extern char *rs_ringdir_hds[];
extern char *rs_ringidx_hds[];

/* ----- public structures ----- */

/* low level descriptor */
typedef void * RS_LLD;

/* This structure is the handle for ringstore operations and is created
 * by rs_open(). It is used by all public facing interfaces */
struct rs_session {
     const struct rs_lowlevel *method;/* method vectors */
     RS_LLD *handle;		/* method descriptor */
     int  *errnum;		/* error number */
     char *errstr;		/* descriptive error string */
     char *ringname;		/* name of ring */
     int   generation;		/* generation number from superblock */
     int   ringid;		/* ring id */
     int   nslots;		/* number of slots in ring */
     int   youngest;		/* youngest sequence in ring */
     int   youngest_t;		/* youngest sequence time in ring */
     int   youngest_hash;	/* youngest sequence header hash in ring */
     int   oldest;		/* oldest sequence in ring */
     int   oldest_t;		/* oldest sequence time in ring */
     int   oldest_hash;		/* oldest sequence header hash in ring */
     int   current;		/* current sequence in ring (next to read) */
     int   duration;		/* duration in seconds */
     ITREE *hdcache;		/* cached headers keyed by hash value */
};
typedef struct rs_session * RS;

/* This holds the superblock information about the local host */
struct rs_superblock {
     int   version;		/* db format version */
     time_t created;		/* create time */
     char *os_name;		/* operating system name */
     char *os_release;		/* operating system release */
     char *os_version;		/* operating system version within release */
     char *hostname;		/* creating system host name */
     char *domainname;		/* creating system domain name */
     char *machine;		/* hardware type and model */
     int   timezone;		/* seconds west of GMT */
     int   generation;		/* ring dir generation */
     int   ringcounter;		/* holds next ring id */
};
typedef struct rs_superblock * RS_SUPER;



/* ----- private structures ----- */
/* data portion of table for low level storage */
struct rs_data_block {
     time_t time;
     unsigned long hd_hashkey;
     char *data;
     void *__priv_alloc_mem;	/* ignore */
};
typedef struct rs_data_block * RS_DBLOCK;

/* Call vectors to the low level storage */
struct rs_lowlevel {
     void   (*ll_init)         ();
     void   (*ll_fini)         ();
     RS_LLD (*ll_open)         (char *filename, mode_t perm, int create);
     void   (*ll_close)        (RS_LLD);
     int    (*ll_exists)       (char *filename, enum rs_db_writable todo);
     int    (*ll_lock)         (RS_LLD, enum rs_db_lock rw, char *where);
     void   (*ll_unlock)       (RS_LLD);
     RS_SUPER (*ll_read_super) (RS_LLD);
     int    (*ll_write_super)  (RS_LLD, RS_SUPER super);
     TABLE  (*ll_read_rings)   (RS_LLD);
     int    (*ll_write_rings)  (RS_LLD, TABLE rings);
     ITREE *(*ll_read_headers) (RS_LLD);
     int    (*ll_write_headers)(RS_LLD, ITREE *headers);
     TABLE  (*ll_read_index)   (RS_LLD, int ringid);
     int    (*ll_write_index)  (RS_LLD, int ringid, TABLE index);
     int    (*ll_rm_index)     (RS_LLD, int ringid);
     int    (*ll_append_dblock)(RS_LLD, int ringid, int start_seq, 
				ITREE *dblock);
     ITREE *(*ll_read_dblock)  (RS_LLD, int ringid,int start_seq,int nblocks);
     int    (*ll_expire_dblock)(RS_LLD, int ringid, int from_seq, int to_seq);
     TREE * (*ll_read_substr)  (RS_LLD, char *subkey);
     char  *(*ll_read_value)   (RS_LLD, char *key, int *length);
     int    (*ll_write_value)  (RS_LLD, char *key, char *value, int length);
     int    (*ll_checkpoint)   (RS_LLD);
     int    (*ll_footprint)    (RS_LLD);
     int    (*ll_dumpdb)       (RS_LLD);
     void   (*ll_errstat)      (RS_LLD, int *errnum, char **errstr);
};
typedef const struct rs_lowlevel * RS_METHOD;

/* Functional prototypes */

/* file and ring */
void  rs_init();
void  rs_fini();
RS    rs_open(RS_METHOD method, char *filename, int filemode, char *ringname,
	      char *longname, char *description, int nslots, int duration,
	      int flags);
void  rs_close(RS ring);
int   rs_destroy(RS_METHOD method, char *filename, char *ringname);

/* stateful TABLE oriented transfer */
int   rs_put(RS ring, TABLE data);
TABLE rs_get(RS ring, int musthave_meta);
int   rs_replace(RS ring, TABLE data);
TABLE rs_mget_nseq(RS ring, int nsequences);
TABLE rs_mget_to_time(RS ring, time_t last_t);
int   rs_checkpoint(RS ring);

/* stateful TABLE oriented positioning */
int   rs_current  (RS ring, int *sequence, time_t *time);
int   rs_youngest (RS ring, int *sequence, time_t *time);
int   rs_oldest   (RS ring, int *sequence, time_t *time);
int   rs_rewind   (RS ring, int nsequencies);
int   rs_forward  (RS ring, int nsequencies);
int   rs_goto_seq (RS ring, int sequence);
int   rs_goto_time(RS ring, time_t time);

/* stateless column oriented reading */
TABLE  rs_mget_range (RS ring, int from_seq, int to_seq, 
		      time_t from_t, time_t to_t);
TABLE  rs_mget_byseq (RS ring, int from_seq, int to_seq);
TABLE  rs_mget_bytime(RS ring, time_t from_t, time_t to_t);
TABLE  rs_mget_cons  (RS_METHOD method, char *filename, char *ringname, 
		      time_t from_t, time_t to_t);

/* file & ring - modification & information */
int   rs_resize   (RS ring, int newslots);
int   rs_purge    (RS ring, int nkill);
int   rs_stat     (RS ring, int *ret_duration, int *ret_nslots, 
		   int *ret_oldest, int *ret_oldest_t, 
		   unsigned long *ret_oldest_hash, 
		   int *ret_youngest, int *ret_youngest_t,
		   unsigned long *ret_youngest_hash, int *ret_current);
TABLE rs_lsrings      (RS_METHOD method, char *filename);
TABLE rs_inforings    (RS_METHOD method, char *filename);
TABLE rs_lsconsrings  (RS_METHOD method, char *filename);
TABLE rs_infoconsrings(RS_METHOD method, char *filename);
char *rs_filename     (RS ring);
char *rs_ringname     (RS ring);
int   rs_footprint    (RS ring);
int   rs_remain       (RS ring);
int   rs_change_ringname(RS ring, char *newname);
int   rs_change_duration(RS ring, int   newdur);
int   rs_change_longname(RS ring, char *newlong);
int   rs_change_comment (RS ring, char *newcomment);
int   rs_errno    (RS ring);
char *rs_errstr   (RS ring);

/* low level diagnostic information */
RS_SUPER rs_info_super (RS_METHOD method, char *filename);
TABLE    rs_info_ring  (RS ring);
TABLE    rs_info_header(RS ring);
TABLE    rs_info_index (RS ring);

/* superblock management */
RS_SUPER rs_create_superblock();
RS_SUPER rs_copy_superblock  (RS_SUPER src);
void     rs_free_superblock  (RS_SUPER toast);
void     rs_free_dblock      (ITREE *dlist);

/* Macro definitions */

#endif /* _RS_H_ */
