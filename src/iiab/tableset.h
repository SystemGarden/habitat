/*
 * Table set manipulation class
 * Used to make stateful but independent changes over TABLE data
 * Nigel Stuckey, November 2003
 */

#ifndef _TABLESET_H_
#define _TABLESET_H_

#include "table.h"
#include "itree.h"
#include "tree.h"

#define TABSET_PRETTY		1
#define TABSET_NOTPRETTY	0
#define TABSET_WITHNAMES	1
#define TABSET_NONAMES		0
#define TABSET_WITHINFO		1
#define TABSET_NOINFO		0
#define TABSET_WITHBODY		1
#define TABSET_NOBODY		0
#define TABSET_SORT_ASCII_DESC	0
#define TABSET_SORT_ASCII_ASC	1
#define TABSET_SORT_NUM_DESC	2
#define TABSET_SORT_NUM_ASC	3

enum tableset_op {eq, ne, gt, lt, ge, le, begins};
struct tableset_cond {
     char *col;
     enum tableset_op op;
     char *value;
     int iswhere;	/* 1=where, 0=unless */
};
typedef struct tableset_cond * TABSET_COND;

struct tableset {
     TABLE tab;		/* pointer to table. Increments the table's reference 
			 * count by one */
     ITREE *cols;	/* ordered list of columns to use */
     ITREE *where;	/* list of TABSET_COND: the collections of 
			 * successive where methods */
     int nwhere;	/* number of where conditions */
     int nunless;	/* number of unless conditions */
     char *sortby;	/* sort by column name */
     int   sorthow; 	/* ascii: 0=desc 1=asc numeric: 2=desc 3=asc */
     ITREE *rownums;	/* ordered list of tab's rownumbers */
     TREE *groupby;	/* tree of ITREEs to the index list */
     ITREE *tobegarbage;/* list of data to be nfree'd() */
};
typedef struct tableset * TABSET;

TABSET tableset_create   (TABLE tab);
void   tableset_destroy  (TABSET t);
void   tableset_freeondestroy(TABSET t, void *tokill);
void   tableset_reset    (TABSET t);		  /* use all cols [default] */
void   tableset_select   (TABSET t, ITREE *cols);  /* only use ordered cols */
void   tableset_selectt  (TABSET t, char *cols);   /* only use ordered cols */
void   tableset_exclude  (TABSET t, TREE *nocols); /* use all but nocols */
void   tableset_excludet (TABSET t, char *nocols); /* use all but nocols */
void   tableset_where    (TABSET t, char *col, enum tableset_op op, char *val);
void   tableset_unless   (TABSET t, char *col, enum tableset_op op, char *val);
void   tableset_groupby  (TABSET t, char *col, enum tableset_op op, char *val);
void   tableset_sortby   (TABSET t, char *col, int ascending);
int    tableset_configure(TABSET t, char *commands);
TABLE  tableset_into     (TABSET t);
char * tableset_print    (TABSET t, int pretty, 
			  int with_names, int with_info, int with_body);
void   tableset_delete();

#endif /* _TABLESET_H_ */
