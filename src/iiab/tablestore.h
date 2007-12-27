/* 
 * Table store on top of timestore, spanstore and holstore
 * Nigel Stuckey, April & August 1999
 *
 * Copyright System Garden Limited 1996-2001. All rights reserved.
 */

#ifndef _TABLESTORE_H_
#define _TABLESTORE_H_

#include <time.h>
#include "itree.h"
#include "nmalloc.h"
#include "holstore.h"
#include "timestore.h"

/*
 * Description of Tablestore
 *
 * This class stores tabular data over time, using the timestore
 * ring buffers for storage.
 * The column names and types (the schema) are provided by the caller
 * each time a table store is opened or created, in a format outlined below.
 * Each time a table is written, using tab_put(), the data is expected
 * to be in the format specified by the schema and this will be checked
 * by the class, resulting in the possible failure of tab_put().
 * The data written in a session, between hol_open() [or hol_create()] and 
 * hol_close() is associated with the schema in a structure called a span.
 * This structure is also stored to disk using spanstore.
 *
 * Tablestore is a subclass of timestore, providing storage of data
 * in a ring buffer held on disk. All the methods used by timestore are
 * made available in tablestore, together with other specific methods for
 * manipulation of tabular data.
 *
 */

#define TAB_SMLSTRLEN	12	/* Short string length */
#define TAB_MIDSTRLEN	128	/* Medium string length  */
#define TAB_LONGSTRLEN	1024	/* Long string length */
#define TAB_MAXMGETSZ   100	/* Size of mget request */

/* This structure is the handle for tablestore opertions and is created
 * by tab_open() or tab_create(). It is not saved to disk as it
 * can always be rebuilt */
struct tab_session {
     TS_RING ts;		/* data stored in a time series ring */
     ITREE *schema;		/* parsed headers (NULL if headers==NULL) */
     int ncols;			/* number of columns required in data */
     int from;			/* starting sequence number */
     int to;			/* ending sequence number */
};

typedef struct tab_session * TAB_RING;

/* Functional prototypes */
void   tab_init();
void   tab_fini();
TAB_RING tab_open(char *holname, char *tablename, char *password);
TAB_RING tab_open_fromts(TS_RING ts);
TAB_RING tab_create(char *holname, int mode, char *tablename, 
		    char *description, char *password, int nslots);
void   tab_close(TAB_RING ring);
int    tab_rm(TAB_RING ring);
int    tab_put(TAB_RING ring, TABLE data);
int    tab_put_withtime(TAB_RING ring, TABLE data, time_t instime);
int    tab_puttext(TAB_RING ring, const char *tabtext);
char  *tab_getraw(TAB_RING ring, int *retlength, time_t *retinstime, 
		  int *retseq);
TABLE  tab_get(TAB_RING ring, time_t *retinstime, int *retseq);
TABLE  tab_mget_byseqs(TAB_RING t, int ufrom, int uto);
int    tab_mgetraw(TAB_RING ring, int want, ITREE **retlist);
void   tab_mgetrawfree(ITREE *list);
void   tab_mgetrawfree_leavedata(ITREE *list);
int    tab_replace(TAB_RING ring, char *table);
int    tab_lastread(TAB_RING ring);
int    tab_youngest(TAB_RING ring);
int    tab_oldest(TAB_RING ring);
int    tab_jump(TAB_RING ring, int jump);
int    tab_jumpyoungest(TAB_RING ring);
int    tab_jumpoldest(TAB_RING ring);
int    tab_setjump(TAB_RING ring, int setjump);
void   tab_jumptime(TAB_RING ring, time_t fromt, int hintseq);
int    tab_prealloc(TAB_RING ring, int size);
int    tab_resize(TAB_RING ring, int size);
int    tab_tell(TAB_RING ring, int *ret_nrings, int *ret_nslots, 
		int *ret_nread, int *ret_nunread, char **ret_description);
TREE  *tab_lsrings(TAB_RING ring);
TREE  *tab_lsringshol(HOLD hol);
void   tab_freelsrings(TREE *list);
int    tab_footprint(TAB_RING ring);
int    tab_remain(TAB_RING ring);
int    tab_purge(TAB_RING ring, int kill);
char  *tab_getheader_latest(TAB_RING t);
char  *tab_getheader_oldest(TAB_RING t);
char  *tab_getheader_seq(TAB_RING t, int seq);
int    tab_jump_youngestspan(TAB_RING t);
int    tab_jump_oldestspan(TAB_RING t);
int    tab_jump_seqspan(TAB_RING t, int seq);
/*TABLE   tab_getbyseqs(TAB_RING t, int ufrom, int uto);*/
TABLE   tab_getspanbyseq(TAB_RING t, int containseq);
int     tab_getconsbytime(ITREE *lst, TAB_RING t, time_t from, time_t to);
/*int     tab_getconsbytime(TABLE tab, TAB_RING t, time_t from, time_t to);*/
HOLD    tab_holstore(TAB_RING t);
TS_RING tab_tablestore(TAB_RING t);
ITREE  *tab_schema(TAB_RING t);
int     tab_ncols(TAB_RING t);
int     tab_from(TAB_RING t);
int     tab_to(TAB_RING t);
int     tab_addtablefrom_tabnts(TABLE t, char *header, ITREE *data);

/* Macro definitions */
#define tab_getraw(t,retlength,retinstime,retseq) ts_get(t->ts,retlength,retinstime,retseq)
#if 0
#define tab_mgetraw(t,want,retlist) ts_mget(t->ts,want,retlist)
#define tab_mgetrawfree(list) ts_mgetfree(list)
#define tab_mgetrawfree_leavedata(list) ts_mgetfree_leavedata(list)
#endif
#define tab_replace(t,table) ts_replace(t->ts,table,strlen(table)+1)
#define tab_lastread(t) ts_lastread(t->ts)
#define tab_youngest(t) ts_youngest(t->ts)
#define tab_oldest(t) ts_oldest(t->ts)
#define tab_jump(t,jump) ts_jump(t->ts,jump)
#define tab_jumpyoungest(t) ts_jumpyoungest(t->ts)
#define tab_jumpoldest(t) ts_jumpoldest(t->ts)
#define tab_setjump(t,setjump) ts_setjump(t->ts,setjump)
#define tab_prealloc(t,size) ts_prealloc(t->ts,size)
#define tab_resize(t,size) ts_resize(t->ts,size)
#define tab_tell(t,nrg,ns,nrd,nun,d) ts_tell(t->ts,nrg,ns,nrd,nun,d)
#define tab_lsringshol(hol) ts_lsringshol(hol, "")
#define tab_lsrings(t) tab_lsringshol(t->ts->hol)
#define tab_freelsrings(l) ts_freelsrings(l)
#define tab_purge(t,kill) ts_purge(t->ts,kill)
#define tab_footprint(t) hol_footprint(t->ts->hol)
#define tab_remain(t) hol_remain(t->ts->hol)
#define tab_platform(t) hol_platform(t->ts->hol)
#define tab_os(t) hol_os(t->ts->hol)
#define tab_host(t) hol_host(t->ts->hol)
#define tab_created(t) hol_created(t->ts->hol)
#define tab_version(t) hol_version(t->ts->hol)
#define tab_holstore(t) t->ts->hol
#define tab_tablestore(t) t->ts
#define tab_name(t) t->ts->name
#define tab_schema(t) t->schema
#define tab_ncols(t) t->ncols
#define tab_from(t) t->from
#define tab_to(t) t->to

#endif /* _TABLESTORE_H_ */
