/* 
 * Table class to hold and manipulate data in a two dimensional form, 
 * with named columns and indexed rows.
 *
 * Nigel Stuckey, June 1999
 *
 * Copyright System Garden Limitied 1999-2001. All rights reserved.
 */

#ifndef _TABLE_H_
#define _TABLE_H_

#include "tree.h"
#include "itree.h"

struct table_info {
     int ncols;
     int nrows;
     int minrowkey;
     int maxrowkey;
     TREE *data;	/* keys are column names, values are ITREEs 
			 * representing a column. The key in each column 
			 * list is described as an index key, and the value 
			 * is the cell data. Consequently, this implementation
			 * of table is very poor at inserting rows */
     ITREE *colorder;	/* default column order, list of column names */
     TREE *info;	/* same format as data, column names indexes an ITREE
			 * so giving multiple rows of data per column. 
			 * Note: min key should be 1 and the data may be
			 * sparse: if not there, its blank */
     TREE *infolookup;	/* each row of info is named; the name is key in
			 * this list yielding the row indexkey in info.
			 * Warning: value is void * should be casted to int */
     ITREE *tobegarbage;/* nfree these in order on tree destruction */
     int refcount;	/* reference count. incremented with incref(), 
			 * decrementred with decref() or tree_destroy() */
     ITREE *roworder;	/* optional list or row keys dictating the order
			 * of table rows. will only be used with methods 
			 * that actually use this feature, eg getsortedcol() */
     char separator;	/* value separator for use in table_outbody() */
};

typedef struct table_info *TABLE;

#define TABLE_DEFSEPERATOR '\t'
#define TABLE_FMTLEN 20
#define TABLE_WITHCOLNAMES 1
#define TABLE_WITHINFO 1
#define TABLE_HASCOLNAMES 1
#define TABLE_HASRULER 1
#define TABLE_NOCOLNAMES 0
#define TABLE_NORULER 0
#define TABLE_OUTINFOBUFSZ 64000
#define TABLE_OUTBLOCKSZ 4000
#define TABLE_MULTISEP 1
#define TABLE_SINGLESEP 0
#define TABLE_CFMODE 2


TABLE  table_create();
TABLE  table_create_t(ITREE *colnames);
TABLE  table_create_a(char **colnames);
TABLE  table_create_s(char *colnamestr);
TABLE  table_create_fromdonor(TABLE donor);
void   table_destroy(TABLE t);
void   table_freeondestroy(TABLE t, void *block);
TREE  *table_getheader(TABLE t);
int    table_addrow_alloc(TABLE t, TREE *row);
int    table_addrows_a(TABLE t, void ***array);
int    table_addrow_noalloc(TABLE t, TREE *row);
int    table_addemptyrow(TABLE t);
int    table_addtable(TABLE t, TABLE rows, int expand);
void   table_rmrow(TABLE t, int rowkey);
void  *table_getcell(TABLE t, int rowkey, char *colname);
int    table_search(TABLE t, char *haystack, char *needle);
int    table_search2(TABLE t, char *haystack1, char *needle1, 
		     char *haystack2, char *needle2);
int    table_replacecell_alloc(TABLE t, int rowkey, char *colname, 
			       char *newcelldata);
int    table_replacecell_noalloc(TABLE t, int rowkey, char *colname, 
				 char *newcelldata);
TREE  *table_getrow(TABLE t, int rowkey);
ITREE *table_getcol(TABLE t, char *colname);
ITREE *table_getsortedcol(TABLE t, char *colname);
int    table_addcol(TABLE t, char *colname, ITREE *coldata);
void   table_rmcol(TABLE t, char *colname);
int    table_renamecol(TABLE t, char *oldcolname, char *newcolname);
ITREE *table_setcolorder(TABLE t, ITREE *colorder);
ITREE *table_getcolorder(TABLE t);
int   *table_datawidths(TABLE t, int fromrowkey, int torowkey, ITREE *cols);
int   *table_alldatawidths(TABLE t, int fromrowkey, int torowkey);
int   *table_everydatawidth(TABLE t);
int   *table_colwidths(TABLE t, int fromrowkey, int torowkey, ITREE *cols);
int   *table_allcolwidths(TABLE t, int fromrowkey, int torowkey);
int   *table_everycolwidth(TABLE t);
char  *table_print(TABLE t);
char  *table_printrow(TABLE t, int rowkey);
char  *table_printrows(TABLE t, int fromrowkey, int torowkey);
char  *table_printselect(TABLE t,int fromrowkey,int torowkey,ITREE *colorder);
char  *table_printcols_t(TABLE t, ITREE *colnameorder);
char  *table_printcols_a(TABLE t, char **colnameorder);
char  *table_html(TABLE t, int fromrowkey, int torowkey, ITREE *colorder);
char  *table_outheader(TABLE t);
char  *table_outinfo(TABLE t);
char  *table_outbody(TABLE t);
char  *table_outrows(TABLE t, int fromkey, int tokey);
char  *table_outtable(TABLE t);
char  *table_outtable_full(TABLE t, char sep, int withcolnames, int withinfo);
int    table_scan(TABLE t, char *buffer, char *sepstr, int sepmode,
		  int hascolnames, int hasruler);
int    table_nrows(TABLE t);
int    table_ncols(TABLE t);
ITREE *table_colnames(TABLE t);
void   table_first(TABLE t);
void   table_next(TABLE t);
void   table_prev(TABLE t);
void   table_last(TABLE t);
void   table_gotorow(TABLE t, int rowkey);
int    table_isatfirst(TABLE t);
int    table_isatlast(TABLE t);
int    table_isbeyondend(TABLE t);
TREE  *table_getcurrentrow(TABLE t);
void   table_rmcurrentrow(TABLE t);
void   table_rmallrows(TABLE t);
void  *table_getcurrentcell(TABLE t, char *colname);
int    table_getcurrentrowkey(TABLE t);
int    table_replacecurrentcell(TABLE t, char *colname, void *data);
int    table_replacecurrentcell_alloc(TABLE t, char *colname, void *data);
int    table_replaceinfocell(TABLE t, char *infoname, char *colname,
			     void *value);
int    table_addemptyinfo(TABLE t, char *infoname);
int    table_addinfo_t(TABLE t, char *infoname, TREE *inforow);
int    table_addinfo_it(TABLE t, char *infoname, ITREE *inforow);
int    table_rminfo(TABLE t, char *infoname);
TREE  *table_getinforow(TABLE t, char *infoname);
TREE  *table_getinfocol(TABLE t, char *colname);
char  *table_getinfocell(TABLE t, char *infoname, char *colname);
TREE  *table_getinfonames(TABLE t);
int    table_check(TABLE t);
void   table_addroworder(TABLE t, ITREE *roworder);
void   table_addroworder_t(TABLE t, TREE *roworder);
int    table_sort(TABLE t, char *primarykey, char *secondarykey);
int    table_sortnumeric(TABLE t, char *primarykey, char *secondarykey);
int    table_hascol(TABLE t, char *colname);
TREE  *table_uniqcolvals(TABLE t, char *colname, TREE **uniq);
TABLE  table_selectcolswithkey(TABLE t, char *keycol, char *key, 
			       ITREE *datacols);
int    table_equals(TABLE t, TABLE other);

#define table_traverse(t) for (table_first(t);!(table_isbeyondend(t));table_next(t))
#define table_incref(t) t->refcount++;
#define table_decref(t) t->refcount--;

#endif /* _TABLE_H_*/
