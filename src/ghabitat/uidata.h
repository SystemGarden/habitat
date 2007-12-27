/*
 * Habitat
 * GUI independent data abstraction of data
 * Designed to be used in conjunction with uichoice:: which selects
 * which data should be extracted.
 * Uidata:: should be called by specific GUI toolkits, which
 * will place the information into a few generic viewers.
 *
 * Nigel Stuckey, May 1999
 * Copyright System Garden 1999-2001. All rights reserved.
 */

#ifndef _UIDATA_H_
#define _UIDATA_H_

#include <time.h>
#include "../iiab/tree.h"
#include "../iiab/table.h"
#include "../iiab/cf.h"

#define UIDATA_MAXTSRECS 10000
#define UIDATA_CLOCKWORKKEY "clockwork"
/* result passed back in a structure */
enum uidata_restype {
     TRES_NONE,			/* no or empty result */
     TRES_TABLE,		/* single table */
     TRES_TABLELIST,		/* list of tables */
     TRES_EDTABLE		/* single table with callbacks */
};
struct uidata_result {
     enum uidata_restype t;
     union {
	  TABLE tab;		/* single table */
	  ITREE *tablst;	/* list of tables */
	  struct {		/* single table with callbacks */
	       TABLE tab;
	       TREE *args;
	       char *(*summary)(TREE *row);
	       int (*create)(TREE *nodeargs, TABLE tab);
	       int (*update)(TREE *nodeargs, TABLE tab);
	  } edtab;
     } d;
};
typedef struct uidata_result RESDAT;

/* node types */
enum uidata_type
{
     UI_NONE,		/* no interface */
     UI_HELP,		/* application help */
     UI_SPLASH,		/* splash graphic */
     UI_TABLE,		/* table or grid interface */
     UI_EDTABLE,	/* editable table or grid interface */
     UI_FORM,		/* form interface: prompt text and value */
     UI_EDFORM,		/* editable form interface: prompt text and value */
     UI_TEXT,		/* text interface */
     UI_EDTEXT,		/* editable text interface */
     UI_EDTREE,		/* editable tree interface */
     UI_GRAPH		/* graph or curve drawing interface */
};


void   uidata_init(CF_VALS);
void   uidata_fini();
void   uidata_countresdat(RESDAT dres, int *tables, int *lines, int *cols);
RESDAT uidata_getevents(TREE *args);
RESDAT uidata_getpatact(TREE *args);
RESDAT uidata_edtpatact(TREE *args);
char * uidata_sumpatact(TREE *row);
int    uidata_updpatact(TREE *args, TABLE update);
int    uidata_crtpatact(TREE *args, TABLE new);
RESDAT uidata_getpatwatch(TREE *args);
RESDAT uidata_edtpatwatch(TREE *args);
char * uidata_sumpatwatch(TREE *row);
int    uidata_updpatwatch(TREE *args, TABLE update);
int    uidata_crtpatwatch(TREE *args, TABLE new);
RESDAT uidata_edtrecwatch(TREE *args);
char * uidata_sumrecwatch(TREE *row);
int    uidata_updrecwatch(TREE *args, TABLE update);
int    uidata_crtrecwatch(TREE *args, TABLE new);
RESDAT uidata_getlocalcf(TREE *args);
RESDAT uidata_getroutecf(TREE *args);
RESDAT uidata_getlocalelogrt(TREE *args);
RESDAT uidata_getrouteelogrt(TREE *args);
void   uidata_logmessage(char ecode, time_t time, char *sev, char *file,
			 char *func, char *line, char *text);
RESDAT uidata_getlocallogs(TREE *args);
RESDAT uidata_getroutelogs(TREE *args);
void   uidata_freeresdat(RESDAT d);

RESDAT uidata_get_route     (TREE *args);
RESDAT uidata_get_route_cons(TREE *args);
RESDAT uidata_get_file(TREE *nodeargs);
RESDAT uidata_get_jobs(TREE *nodeargs);
RESDAT uidata_get_uptime(TREE *nodeargs);
RESDAT uidata_get_hostinfo(TREE *nodeargs);
RESDAT uidata_get_rsinfo(TREE *nodeargs);
RESDAT uidata_get_fileinfo(TREE *nodeargs);

#endif /* _UIDATA_H_ */
