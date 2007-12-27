/* 
 * Timeseries storage on top of holstore
 * Nigel Stuckey, January/February 1998 
 * Based on original timeseries API of 1996-7
 *
 * Copyright System Garden Limitied 1996-2001, All rights reserved.
 */

#ifndef _TIMESTORE_H_
#define _TIMESTORE_H_

#include <time.h>
#include "holstore.h"
#include "itree.h"
#include "table.h"

/*
 * Description of Timeseries implementation
 *
 * The timeseries storage (TS) provides facilities for data to be stored 
 * sequentially, keyed by an automatically generated sequence number.
 * The TS is implemented using a single holstore, which may contain zero 
 * or more TS. A single index of TS is kept cooperatively by the time stores,
 * thus it is important that only compatible versions are alowed in a single
 * holstore.
 * Each instance of a TS is called a ring once in the holstore.
 * Each ring may hold either the most recent N records or an every growing
 * collection of records, which should be purged periodically.
 *
 * Format of timeseries data
 *
 * All time series keys are prefixed by `ts.'
 * A superblock exists to contain shared information and is called `ts.'.
 * It contains versions, dates etc and synonyms for long ringnames.
 * Each ring has a header containing its configuration in a record 
 * called `ts.<RingName>'.
 * Each element in the ring is stored using the key `ts.<RingName>.<SeqNum>'. 
 * Finally, each element contains a header, that indicates when it was 
 * inserted, by whom etc
 */

#define TS_MAGICNUMBER 8220	/* Timestore data file magic number */
#define TS_VERSIONNUMBER 1	/* Timestore data file version */
#define TS_MAXSUPERLEN 16000	/* Maximum size of timestore superblock */
#define TS_DATASPACE	"__ts__"/* Prefix of timestore data key names */
#define TS_RINGSPACE	"__ts_"	/* Prefix of timestore ring names */
#define TS_SUPERNAME	"__ts"	/* Name of timestore superblock */
#define TS_RINGREMATCH	"^%s%s[^_].+$" /* Regular expression to match ring */
#define TS_MIDSTRLEN	128	/* Medium string length  */
#define TS_LONGSTRLEN	1024	/* Long string length */

/* Associates alias name with computer generated short name (key) */
struct ts_synonyms {
     char *name;
     char *key;
};

/*
 * This structure holds an in-memory copy of the Time Series superblock.
 * It is stored in an ASCII representation on the disk.
 */
struct ts_superblock {
     int magic;			/* magic number of timestore */
     int version;		/* format version of timestore */
     int nrings;		/* number of rings in timestore */
     int nalias;		/* number of alias in list */
     struct ts_synonyms *alias;	/* list/array of aliases */
};

/* This holds the details of each ring */
struct ts_ring {
     HOLD hol;			/* Descriptor of holstore - in mem only */
     struct ts_superblock *super; /* Copy of superblock - in mem only */
     int lastread;		/* Last datum read - in mem only */
     int nslots;		/* Size of ring */
     int oldest;		/* oldest sequence number in ring */
     int youngest;		/* youngest sequence number in ring */
     char *name;		/* Null terminated string */
     char *description;		/* Null terminated string */
     char *password;		/* Null terminated string */
};

typedef struct ts_ring * TS_RING;

/* This holds the details of each datum */
struct ts_ringslot {
     int seq;			/* Sequence number */
     time_t time;		/* Time inserted */
};

/* This holds a record of returned data from ts_mget() */
typedef struct ts_mgetbuffer {
     void *buffer;	/* Buffer */
     int len;		/* Length of buffer */
     int seq;		/* Sequence of buffer */
     time_t instime;	/* Insertion time of buffer */
     char *spantext;	/* Unused by timestore but reserved for use by
			 * tablestore */
} ntsbuf;

/* Functional prototypes */
#ifndef _ROUTE_C_INCLUDE_
void  ts_init();
void  ts_fini();
#endif
TS_RING ts_open(char *holname, char *ringname, char *password);
TS_RING ts_create(char *holname, int mode, char *ringname, 
		    char *description, char *password, int nslots);
void  ts_close(TS_RING ring);
int   ts_rm(TS_RING ring);
int   ts_put(TS_RING ring, void *block, int length);
int   ts_put_withtime(TS_RING ring, void *block, int length, time_t instime);
#if 0
int   ts_mput(TS_RING ring, ITREE *list);
#endif
void *ts_get(TS_RING ring, int *retlength, time_t *retinstime, int *retseq);
int   ts_mget(TS_RING ring, int want, ITREE **retlist);
void  ts_mgetfree(ITREE *list);
void  ts_mgetfree_leavedata(ITREE *list);
TABLE ts_mget_t(TS_RING ring, int want);
int   ts_replace(TS_RING ring, void *block, int length);
int   ts_lastread(TS_RING ring);
int   ts_youngest(TS_RING ring);
int   ts_oldest(TS_RING ring);
int   ts_jump(TS_RING ring, int jump);
int   ts_jumpyoungest(TS_RING ring);
int   ts_jumpoldest(TS_RING ring);
int   ts_setjump(TS_RING ring, int setjump);
int   ts_prealloc(TS_RING ring, int size);
int   ts_resize(TS_RING ring, int size);
int   ts_tell(TS_RING ring, int *ret_nrings, int *ret_nslots, 
	      int *ret_nread, int *ret_nunread, char **ret_description);
TREE *ts_lsrings(TS_RING ring);
TREE *ts_lsringshol(HOLD hol, char *ringpat);
void  ts_freelsrings(TREE *list);
int   ts_footprint(TS_RING ring);
int   ts_remain(TS_RING ring);
int   ts_purge(TS_RING ring, int kill);

/*--unimlemented below--*/
/*time_t ts_newtime(TS_RING ring);*/
/*time_t ts_oldtime(TS_RING ring);*/

/* Private functions */
struct ts_superblock *ts_increatesuper();
struct ts_superblock *ts_inreadsuper(HOLD h);
int     ts_inwritesuper(HOLD h, struct ts_superblock *sb);
void    ts_infreesuper(struct ts_superblock *sb);
TS_RING ts_increatering(HOLD h, struct ts_superblock *sb, char *ringname, 
			char *description, char *password, int nslots);
TS_RING ts_inreadring(HOLD h, struct ts_superblock *sb, char *ringname, 
		      char *password);
int     ts_inwritering(struct ts_ring *ring);
int     ts_inupdatering(struct ts_ring *ring);
void    ts_infreering(struct ts_ring *ring);
int     ts_inwritedatum(struct ts_ring *ring, char *caller, int element, 
			void *block, int length);
void *  ts_inreaddatum(struct ts_ring *ring, char *caller, int element,	
		       int *length);
int     ts_inrmdatum(struct ts_ring *ring, char *caller, int element);

/* Macro definitions */
#define ts_lsrings(ring) ts_lsringshol(ring->hol, "")
#define ts_freelsrings(l) hol_freesearch(l)
#define ts_footprint(l) hol_footprint(l->hol)
#define ts_remain(l) hol_remain(l->hol)
#define ts_platform(l) hol_platform(l->hol)
#define ts_os(l) hol_os(l->hol)
#define ts_host(l) hol_host(l->hol)
#define ts_created(l) hol_created(l->hol)
#define ts_version(l) hol_version(l->hol)
#define ts_holstore(l) l->hol
#define ts_name(l) l->name

#endif /* _TIMESTORE_H_ */
