/*
 * Holstore.
 * Generic storage database.
 * This class implements an abstraction layer over GNU DBM.
 *
 * Nigel Stuckey, January 1998.
 * Copyright System Garden Limited 1998-2001. All rights reserved.
 */
#ifndef _HOLSTORE_H_
#define _HOLSTORE_H_

/* definitions for the underlying data store */
#define DB_GDBM 1
#define DB_RDBM 0

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#if DB_GDBM
#include <gdbm.h>
#elif DB_RDBM
#include <rdbm.h>
#elif DB_MIRD
#include <mird.h>
#else
#endif /* DB_GDBM */
#include "route.h"
#include "tree.h"

struct holstore_descriptor {
     char *name;		/* database file name */
     mode_t mode;		/* database file mode */
#if DB_GDBM
     GDBM_FILE ref;		/* GDBM file descriptor */
#elif DB_RDBM
     struct rdb *ref;		/* pointer to DB structure */
     struct rdb db;		/* RDBM DB structure */
     char *logname;		/* log file name */
#elif DB_MIRD
     struct mird *ref		/* Mird DB structure */
     MIRD_RES *res;		/* Common error handling block */
#else
#endif /* DB_GDBM */
     struct utsname sysbuf;	/* superblock: cached system details */
     time_t created;		/* superblock: cached create time */
     int version;		/* superblock: cached file version */
     datum lastkey;		/* last key (for traversal) [needed by GDBM] */
     int trans;			/* transaction flag: 0=none, 1=read, 2=write */
     int inhibtrans;		/* inhibit transaction flag */
};
typedef struct holstore_descriptor * HOLD;

#define HOLSTORE_MAGIC "828662"	/* Telephone numbers rule our lives */
#define HOLSTORE_VERSION 1
#define HOLSTORE_MAGICLEN 6
#define HOLSTORE_SUPERMAX 1000
#define HOLSTORE_SUPERNAME "superblock"
#define HOLSTORE_SUPERNLEN strlen(HOLSTORE_SUPERNAME)
#define HOLSTORE_ERRBUFSZ 1000
#define HOLSTORE_NTRYS 80
#define HOLSTORE_WAITTRY 50000000	/* 5 miliseconds */

#ifndef _ROUTE_C_INCLUDE_
void  hol_init(int ntrys, long waittry);
void  hol_fini();
#endif
HOLD  hol_open(char *name);
HOLD  hol_create(char *name, mode_t mode);
void  hol_close(HOLD h);
int   hol_put(HOLD h, char *key, void *dat, int length);
void *hol_get(HOLD h, char *key, int *length);
int   hol_rm(HOLD h, char *key);
TREE *hol_search(HOLD h, char *key_regex, char *value_regex);
void  hol_freesearch(TREE *list);
int   hol_begintrans(HOLD h, char mode);
int   hol_endtrans(HOLD h);
int   hol_rollback(HOLD h);
int   hol_commit(HOLD h);
int   hol_inhibittrans(HOLD  h);
int   hol_allowtrans(HOLD  h);
void *hol_readfirst(HOLD h, char **key, int *length);
void *hol_readnext(HOLD h, char **key, int *length);
void  hol_readend(HOLD h);
void  hol_checkpoint(HOLD h);
int   hol_contents(HOLD h);
int   hol_footprint(HOLD h);
int   hol_remain(HOLD h);
char *hol_platform(HOLD h);
char *hol_host(HOLD h);
char *hol_os(HOLD h);
time_t hol_created(HOLD h);
int   hol_version(HOLD h);
int   hol_setsuper(HOLD h, char *platform, char *host, char *os, 
		   time_t created, int version);

/* Internal routines */
void  hol_dberr();
int   hol_dbopen(HOLD h, char *where, char readwrite);
void  hol_dbclose(HOLD h);
datum hol_dbfetch(HOLD h, datum key);
int   hol_dbreplace(HOLD h, datum key, datum value);
int   hol_dbdelete(HOLD h, datum key);
datum hol_dbfirstkey(HOLD h);
datum hol_dbnextkey(HOLD h, datum lastkey);
int   hol_dbreorganise(HOLD h);

/* Macros */
#define hol_platform(h)	(h->sysbuf.machine)
#define hol_host(h)	(h->sysbuf.nodename)
#define hol_os(h)	(h->sysbuf.sysname)
#define hol_created(h)	(h->created)
#define hol_version(h)	(h->version)

#endif /* _HOLSTORE_H_ */
