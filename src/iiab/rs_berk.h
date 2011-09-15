/*
 * Ringstore low level storage using an abstracted interface
 * Berkley DB implementataion
 *
 * Nigel Stuckey, March 2011 using code from September 2001 and
 * January 1998 onwards
 * Copyright System Garden Limited 1998-2011. All rights reserved.
 */
#ifndef _RS_BERK_H_
#define _RS_BERK_H_

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>
#include "route.h"
#include "tree.h"
#include "rs.h"

struct rs_berk_desc {
     enum rs_lld_type lld_type;	/* low level descriptor type (run time 
				 * checking) */
     char *  name;		/* database file name */
     char *  dir;		/* database dir name */
     mode_t  mode;		/* database file mode */
     DB_ENV *envp;		/* Berkley DB environment pointer */
     DB *    dbp;		/* Berkley DB file descriptor */
     DB_TXN *txn;		/* Berkley DB current transaction or NULL */
     DBC *   cursorp;		/* Berkley DB cursor pointer */
     RS_SUPER super;		/* superblock structure */
     int     lock;		/* lock flag: 0=none, 1=read, 2=write */
};
typedef struct rs_berk_desc * RS_BERKD;

extern const struct rs_lowlevel rs_berk_method;

#define RS_BERK_MAGIC		"683406" /* Telephone numbers rule our lives */
#define RS_BERK_VERSION		3
#define RS_BERK_MAGICLEN	6
#define RS_BERK_SUPERMAX	1000
#define RS_BERK_SUPERNAME	"superblock"
#define RS_BERK_SUPERNLEN	strlen(RS_BERK_SUPERNAME)
#define RS_BERK_ERRBUFSZ	1000
#define RS_BERK_NTRYS		80
#define RS_BERK_WAITTRY		50000000	/* 5 miliseconds */
#define RS_BERK_READ_PERM	0400		/* just need to read */
#define RS_BERK_RINGDIR		"ringdir"
#define RS_BERK_HEADDICT	"headdict"
#define RS_BERK_INDEXNAME	"ri"
#define RS_BERK_INDEXKEYLEN	15
#define RS_BERK_DATAKEYLEN	25
#define RS_BERK_DATANAME	"rd"

/* functional prototypes */
void   rs_berk_init         ();
void   rs_berk_fini         ();
RS_LLD rs_berk_open         (char *filename, mode_t perm, int create);
void   rs_berk_close        (RS_LLD);
int    rs_berk_exists       (char *filename, enum rs_db_writable todo);
int    rs_berk_lock         (RS_LLD, enum rs_db_lock rw, char *where);
void   rs_berk_unlock       (RS_LLD);
RS_SUPER rs_berk_read_super      (RS_LLD rs);
RS_SUPER rs_berk_read_super_file (char *dbname);
RS_SUPER rs_berk_read_super_fd   (DB_ENV *envp, DB *dbp, DB_TXN *txn);
int    rs_berk_write_super       (RS_LLD rs, RS_SUPER);
int    rs_berk_write_super_file  (char *dbname, mode_t perm, RS_SUPER);
int    rs_berk_write_super_fd    (DB_ENV *envp, DB *dbp, DB_TXN *txn, RS_SUPER);
void   rs_berk_free_super        (RS_SUPER super);
TABLE  rs_berk_read_rings   (RS_LLD);
int    rs_berk_write_rings  (RS_LLD, TABLE rings);
ITREE *rs_berk_read_headers (RS_LLD);
int    rs_berk_write_headers(RS_LLD, ITREE *headers);
TABLE  rs_berk_read_index   (RS_LLD, int ringid);
int    rs_berk_write_index  (RS_LLD, int ringid, TABLE index);
int    rs_berk_rm_index     (RS_LLD, int ringid);
int    rs_berk_append_dblock(RS_LLD, int ringid, int start_seq, ITREE *dblock);
ITREE *rs_berk_read_dblock  (RS_LLD, int ringid, int start_seq, int nblocks);
int    rs_berk_expire_dblock(RS_LLD, int ringid, int from_seq, int to_seq);
TREE  *rs_berk_read_substr  (RS_LLD, char *substr_key);
char  *rs_berk_read_value   (RS_LLD, char *key, int *length);
int    rs_berk_write_value  (RS_LLD, char *key, char *value, int length);
int    rs_berk_checkpoint   (RS_LLD);
int    rs_berk_footprint    (RS_LLD);
int    rs_berk_dumpdb       (RS_LLD);
void   rs_berk_errstat      (RS_LLD, int *errnum, char **errstr);

/* low level berk access functions */
RS_BERKD rs_berkd_from_lld(RS_LLD lld);
void   rs_berk_dberr();
int    rs_berk_dbopen(RS_BERKD rs, char *where, enum rs_db_lock readwrite);
void   rs_berk_dbclose(RS_BERKD rs);
char * rs_berk_dbfetch(RS_BERKD rs, char *key, int *ret_length);
int    rs_berk_dbreplace(RS_BERKD rs, char *key, char *value, int length);
int    rs_berk_dbdelete(RS_BERKD rs, char *key);
char * rs_berk_dbfirstkey(RS_BERKD rs);
char * rs_berk_dbnextkey(RS_BERKD rs, char *lastkey);
int    rs_berk_dbreorganise(RS_BERKD rs);
char * rs_berk_readfirst(RS_BERKD rs, char **key, int *length);
char * rs_berk_readnext(RS_BERKD rs, char **key, int *length);
void   rs_berk_readend(RS_BERKD rs);
#endif /* _RS_BERK_H_ */
