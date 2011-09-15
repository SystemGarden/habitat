/*
 * Ringstore low level storage using an abstracted interface
 * GDBM (GNU DBM) implementataion
 *
 * Nigel Stuckey, September 2001 using code from January 1998 onwards
 * Copyright System Garden Limited 1998-2001. All rights reserved.
 */
#ifndef _RS_GDBM_H_
#define _RS_GDBM_H_

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <gdbm.h>
#include "route.h"
#include "tree.h"
#include "rs.h"

struct rs_gdbm_desc {
     enum rs_lld_type lld_type;	/* low level descriptor type (run time 
				 * checking) */
     char * name;		/* database file name */
     mode_t mode;		/* database file mode */
     GDBM_FILE ref;		/* GDBM file descriptor */
     RS_SUPER super;		/* superblock structure */
     int   lock;		/* lock flag: 0=none, 1=read, 2=write */
     int   inhibitlock;		/* inhibit lock flag */
     char *lastkey;		/* last key (for traversal) [needed by GDBM] */
};
typedef struct rs_gdbm_desc * RS_GDBMD;

extern const struct rs_lowlevel rs_gdbm_method;

#define RS_GDBM_MAGIC		"685570" /* Telephone numbers rule our lives */
#define RS_GDBM_VERSION		2
#define RS_GDBM_MAGICLEN	6
#define RS_GDBM_SUPERMAX	1000
#define RS_GDBM_SUPERNAME	"superblock"
#define RS_GDBM_SUPERNLEN	strlen(RS_GDBM_SUPERNAME)
#define RS_GDBM_ERRBUFSZ	1000
#define RS_GDBM_NTRYS		80
#define RS_GDBM_WAITTRY		50000000	/* 5 miliseconds */
#define RS_GDBM_READ_PERM	0400		/* just need to read */
#define RS_GDBM_RINGDIR		"ringdir"
#define RS_GDBM_HEADDICT	"headdict"
#define RS_GDBM_INDEXNAME	"ri"
#define RS_GDBM_INDEXKEYLEN	15
#define RS_GDBM_DATAKEYLEN	25
#define RS_GDBM_DATANAME	"rd"

/* functional prototypes */
void   rs_gdbm_init         ();
void   rs_gdbm_fini         ();
RS_LLD rs_gdbm_open         (char *filename, mode_t perm, int create);
void   rs_gdbm_close        (RS_LLD);
int    rs_gdbm_exists       (char *filename, enum rs_db_writable todo);
int    rs_gdbm_lock         (RS_LLD, enum rs_db_lock rw, char *where);
void   rs_gdbm_unlock       (RS_LLD);
RS_SUPER rs_gdbm_read_super      (RS_LLD rs);
RS_SUPER rs_gdbm_read_super_file (char *dbname);
RS_SUPER rs_gdbm_read_super_fd   (GDBM_FILE fd);
int    rs_gdbm_write_super       (RS_LLD rs, RS_SUPER);
int    rs_gdbm_write_super_file  (char *dbname, mode_t perm, RS_SUPER);
int    rs_gdbm_write_super_fd    (GDBM_FILE fd, RS_SUPER);
void   rs_gdbm_free_super        (RS_SUPER super);
TABLE  rs_gdbm_read_rings   (RS_LLD);
int    rs_gdbm_write_rings  (RS_LLD, TABLE rings);
ITREE *rs_gdbm_read_headers (RS_LLD);
int    rs_gdbm_write_headers(RS_LLD, ITREE *headers);
TABLE  rs_gdbm_read_index   (RS_LLD, int ringid);
int    rs_gdbm_write_index  (RS_LLD, int ringid, TABLE index);
int    rs_gdbm_rm_index     (RS_LLD, int ringid);
int    rs_gdbm_append_dblock(RS_LLD, int ringid, int start_seq, ITREE *dblock);
ITREE *rs_gdbm_read_dblock  (RS_LLD, int ringid, int start_seq, int nblocks);
int    rs_gdbm_expire_dblock(RS_LLD, int ringid, int from_seq, int to_seq);
TREE  *rs_gdbm_read_substr  (RS_LLD, char *substr_key);
char  *rs_gdbm_read_value   (RS_LLD, char *key, int *length);
int    rs_gdbm_write_value  (RS_LLD, char *key, char *value, int length);
int    rs_gdbm_checkpoint   (RS_LLD);
int    rs_gdbm_footprint    (RS_LLD);
int    rs_gdbm_dumpdb       (RS_LLD);
void   rs_gdbm_errstat      (RS_LLD, int *errnum, char **errstr);

/* low level gdbm access functions */
RS_GDBMD rs_gdbmd_from_lld(RS_LLD lld);
void   rs_gdbm_dberr();
int    rs_gdbm_dbopen(RS_GDBMD rs, char *where, enum rs_db_lock readwrite);
void   rs_gdbm_dbclose(RS_GDBMD rs);
char * rs_gdbm_dbfetch(RS_GDBMD rs, char *key, int *ret_length);
int    rs_gdbm_dbreplace(RS_GDBMD rs, char *key, char *value, int length);
int    rs_gdbm_dbdelete(RS_GDBMD rs, char *key);
char * rs_gdbm_dbfirstkey(RS_GDBMD rs);
char * rs_gdbm_dbnextkey(RS_GDBMD rs, char *lastkey);
int    rs_gdbm_dbreorganise(RS_GDBMD rs);
char * rs_gdbm_readfirst(RS_GDBMD rs, char **key, int *length);
char * rs_gdbm_readnext(RS_GDBMD rs, char **key, int *length);
void   rs_gdbm_readend(RS_GDBMD rs);
#endif /* _RS_GDBM_H_ */
