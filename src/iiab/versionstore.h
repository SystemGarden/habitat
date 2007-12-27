
/*
 * Verison store
 * Stores text and meta data regarding the entry, additionally maintaining 
 * a historical record of it.
 *
 * Nigel Stuckey, May 1999
 * Major update,  Oct 1999
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _VERSIONSTORE_H_
#define _VERSIONSTORE_H_

#include "timestore.h"

/* Version store is implemented on top of timestore.
 * The idea is that an item, such as a configuration file, is 
 * storted over time, so that changes can be monitored or backed out.
 * Each change is recorded with some meta data, such as a comment or
 * the author. This information is held in a block managed by versionstore
 * and held in a timestore structre, which provides date and sequencing
 * information.
 * There is no limit to the number of records stored, so the older ones 
 * will not be removed unless requested. This is done by configuring the
 * timestore with unlimited length. It is also possible to truncate the
 * versions by using its(1) etc.
 */

#define VS TS_RING
#define VS_SUPERNAME "__vs"

void  vers_init();
void  vers_fini();
VS    vers_open(char *fname, char *idname, char *passwd);
VS    vers_create(char *fname, int perm, char *idname, char *passwd, 
		  char *description);
void  vers_close(VS);
int   vers_new(VS vs, char *data, int dlen, char *author, char *comment);
int   vers_getcurrent(VS vs, char **data, int *dlen, char **author, 
		      char **comment, time_t *mtime, int *version);
int   vers_getlatest(VS vs, char **data, int *dlen, char **author, 
		     char **comment, time_t *mtime, int *version);
int   vers_getversion(VS vs, int version, char **data, int *dlen, 
		      char **author, char **comment, time_t *mtime);
TABLE vers_getall(VS vs);
int   vers_edit(VS vs, int version, char *author, char *comment);
int   vers_nversions(VS vs);
char *vers_description(VS vs);
int   vers_islatest(VS vs, int version);
int   vers_contains(VS vs, int version);
int   vers_purge(VS vs, int killuptome);
int   vers_rm(VS vs);
/*int   vers_unnew(VS vs);*/
TREE *vers_lsrings(VS vs);
TREE *vers_lsringshol(HOLD hol);
TREE *vers_lsvers(VS vs);
TREE *vers_lsvershol(HOLD hol);
void  vers_freelsrings(TREE *list);

#define vers_init() ts_init()
#define vers_fini() ts_fini()
#define vers_close(vs) ts_close(vs)
#define vers_nversions(vs) (ts_youngest(vs)+1)
#define vers_islatest(vs, ver) (ver==ts_youngest(vs))
#define vers_contains(vs, ver) (ver<=ts_youngest(vs) && ver>=0)
#define vers_purge(vs, ver) ts_purge(vs,ver)
#define vers_rm(vs) ts_rm(vs);
#define vers_lsringshol(hol) ts_lsringshol(hol, "")
#define vers_lsrings(vs) vers_lsringshol(vs->hol)
#define vers_freelsrings(l) ts_freelsrings(l);
#define vers_platform(vs) hol_platform(vs->hol)
#define vers_os(vs) hol_os(vs->hol)
#define vers_host(vs) hol_host(vs->hol)
#define vers_created(vs) hol_created(vs->hol)
#define vers_footprint(vs) hol_footprint(vs->hol)
#define vers_remain(vs) hol_remain(vs->hol)
#define vers_version(vs) hol_version(vs->hol)
#define vers_lsvers(vs) vers_lsvershol(vs->hol);

#endif /* _VERSIONSTORE_H_ */
