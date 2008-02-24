/* 
 * Table class to hold and manipulate data in a two dimensional form, 
 * with named columns and indexed rows.
 *
 * Nigel Stuckey, June 1999
 * Copyright System Garden Limitied 1999-2001. All rights reserved.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tree.h"
#include "itree.h"
#include "table.h"
#include "elog.h"
#include "nmalloc.h"
#include "util.h"

/* globals */

/* Create an empty table */
TABLE table_create()
{
  TABLE t;

  t = xnmalloc(sizeof(struct table_info));
  t->nrows = 0;
  t->ncols = 0;
  t->minrowkey = -1;
  t->maxrowkey = -1;
  t->data = tree_create();
  t->colorder = itree_create();
  t->info = tree_create();
  t->infolookup = tree_create();
  t->tobegarbage = itree_create();
  t->refcount = 1;
  t->roworder = NULL;
  t->separator = TABLE_DEFSEPERATOR;

  return t;
}


/*
 * Create a table using column names supplied in the ITREE list colnames.
 * The values of the ITREE colnames are refered to by the table and 
 * should not be deleted. Please arrange for the data to be cleaned up
 * after the life of the table with table_freeondestroy().
 * The column names must be strings only. No info rows are created.
 */
TABLE table_create_t(ITREE *colnames)
{
     TABLE t;

     t = table_create();
     t->ncols = itree_n(colnames);
     itree_traverse(colnames) {
	  tree_add(t->data, itree_get(colnames), itree_create());
	  itree_append(t->colorder, itree_get(colnames));
     }

     return t;
}


/*
 * Create a table using column names in an array of strings: colnames.
 * Colnames should be terminated with NULL entry. The data is refered to
 * for the lifetime of the TABLE, so it should not be fred or removed.
 * The array WILL NOT be removed by the TABLE destruction, as it likely
 * to be a constant. Consequently, if it is xnmalloc()ed, it should be
 * added to table_freeondestroy() by the caller. No info rows are created.
 */
TABLE table_create_a(char **colnames)
{
  ITREE *cols;
  char **col;
  TABLE tab;

  cols = itree_create();
  for (col = colnames; *col; col++)
    itree_append(cols, *col);

  tab = table_create_t(cols);

  itree_destroy(cols);

  return tab;
}


/*
 * Create a table with column names specified in the string `colnamestr'.
 * If `colnamestr' includes a new line, then the second and secessive lines
 * are scanned for info lines.
 * Each column name and token is seperated by a tab, which should 
 * never be part of a column name
 * The string will be needed by TABLE until its destruction and must not
 * be a const so the caller should add the string to table_freeondestroy() 
 * if it has been created by xnmalloc().
 * The data will be modified. The table is returned on success or NULL
 * if there is an error.
 */
TABLE table_create_s(char *colnamestr)
{
     ITREE *list, *cols;
     TABLE tab;
     int nlines;
     char *infoname;

     if ( ! colnamestr )
	  return NULL;

     /* scan string */
     nlines = util_scantext(colnamestr, "\t", UTIL_SINGLESEP, &list);
     if (nlines < 1)
	  return NULL;

     /* create table */
     itree_first(list);
     cols = itree_get(list);
     tab = table_create_t(cols);

     /* augment with headers */
     itree_next(list);
     while ( ! itree_isbeyondend(list) ) {
	  /* get the info line and remove its name from the end of the list */
	  cols = itree_get(list);
	  itree_last(cols);
	  infoname = itree_get(cols);
	  itree_rm(cols);

	  /* add info to table */
	  table_addinfo_it(tab, infoname, cols);

	  itree_next(list);	/* iterate */
     }

     /* free working storage and return */
     util_scanfree(list);
     return tab;
}


/*
 * Create table by cloning, using another table to provide the headers
 * and info lines. All the information will be copied so that there is
 * no link to the donor table. No body data is copied.
 */
TABLE table_create_fromdonor(TABLE donor)
{
     TABLE t;
     char *s;
     ITREE *newcol, *donorcol;

     t = table_create();

     /* copy from donor */
     t->ncols = donor->ncols;
     tree_traverse(donor->data) {
	  s = xnstrdup(tree_getkey(donor->data));
	  tree_add(t->data, s, itree_create());
	  table_freeondestroy(t, s);
     }
     itree_traverse(donor->colorder) {
	  /* colorder is an ordered list, values must point to valid memory
	   * in our table */
	  tree_find(t->data, itree_get(donor->colorder));
	  itree_add(t->colorder, itree_getkey(donor->colorder), 
		    tree_getkey(t->data));
     }
     itree_traverse(donor->infolookup) {
	  /* name to rowkey mapping */
	  s = xnstrdup(tree_getkey(donor->infolookup));
	  tree_add(t->infolookup, s, tree_get(donor->infolookup));
	  table_freeondestroy(t, s);
     }
     itree_traverse(donor->info) {
	  /* copy all info rows for each column in independent memory */
	  newcol = itree_create();
	  s = xnstrdup(tree_getkey(donor->info));
	  tree_add(t->info, s, newcol);
	  table_freeondestroy(t, s);

	  donorcol = tree_get(donor->info);
	  itree_traverse(donorcol) {
	       s = xnstrdup(itree_get(donorcol));
	       itree_add(newcol, itree_getkey(donorcol), s);
	       table_freeondestroy(t, s);
	  }
     }
     t->separator = donor->separator;

     return t;
}


/*
 * Returns a TREE list containing the table column names as the keys.
 * The values of each element should be ignored.
 * Do not alter or free this list as it is internal to the TABLE structure
 */
TREE *table_getheader(TABLE t)
{
     return t->data;
}


/*
 * Remove table and invalidate further requests with it.
 * Cell data is not removed or altered.
 * A reference count is used for multiple users: use incref() to increment
 * before passing a reference on to others. If the refcount does not fall 
 * below 0, the table wont actually be destroyed.
 */
void table_destroy(TABLE t)
{
     if (t == NULL)
	  return;
     if (--t->refcount > 0)
	  return;

     /* data */
     tree_traverse(t->data)
	  itree_destroy( (ITREE *) tree_get(t->data) );
     tree_destroy(t->data);

     /* colorder */
     if (t->colorder)
	  itree_destroy(t->colorder);

     /* info */
     tree_traverse(t->info)
	  itree_destroy( (ITREE *) tree_get(t->info) );
     tree_destroy(t->info);

     /* infolookup */
     tree_destroy(t->infolookup);

     /* roworder */
     if (t->roworder)
	  itree_destroy(t->roworder);

     /* tobegarbage memory maintenance list */
     itree_traverse(t->tobegarbage)
	  nfree( itree_get(t->tobegarbage) );
     itree_destroy(t->tobegarbage);

     nfree(t);
}


/*
 * Add block of nmalloc() allocated memory to maintenance list of the TABLE.
 * When the TABLE is either table_destroy()ed the maintenance list will be 
 * subject to nfree() in list order.
 */
void  table_freeondestroy(TABLE t, void *block)
{
  itree_append(t->tobegarbage, block);
}


/*
 * Add a row of data from the list row.
 * Cell data is xnmalloc'ed, that the caller may free the memory after use.
 * The columns in `row' may be a subset of those in `t', in which case
 * the unaddressed columns in the new row are set to NULL.
 * Columns in 'row' not in 't' are currently ignored and not created in 't'.
 * Returns the index key of the newly inserted row or -1 if a column
 * in `row' was not present in `t'.
 */
int table_addrow_alloc(TABLE t, TREE *row)
{
     int rowkey;
     char *cellcpy;

     rowkey = -1;

     tree_traverse(t->data) {
          /* find new row value */
          if (tree_find(row, tree_getkey(t->data)) == TREE_NOVAL) {
	       /* NULL value */
	       cellcpy = NULL;
	  } else {
	       if (tree_get(row)) {
		    cellcpy = xnstrdup(tree_get(row));
		    table_freeondestroy(t, cellcpy);
	       } else {
		    cellcpy = tree_get(row);
	       }
	  }
	  rowkey = itree_append( (ITREE *) tree_get(t->data), cellcpy );
     }

     if ( t->nrows == 0 )
	  t->minrowkey = 0;
     t->nrows++;
     t->maxrowkey = rowkey;

     return rowkey;
}



/*
 * Add a row of data from the list row, using supplied data rather than 
 * taking a copy. Data will not be reparented or deleted.
 * The user may free the TREE that supplied the row and key strings, but
 * not the data.
 * The columns in `row' may be a subset of those in `t', in which case
 * the unaddressed columns in the new row are set to NULL.
 * If row is a superset of columns, only those in the table will be stored.
 * Returns the index key of the newly inserted row or -1 if a column
 * in `row' was not present in `t'.
 */
int table_addrow_noalloc(TABLE t, TREE *row)
{
     int rowkey;

     rowkey = -1;

#if 0
     if (tree_n(row) != t->ncols) {
	  elog_printf(DEBUG, "row of %d columns fails to "
		      "add to a table of %d columns", tree_n(row), t->ncols);
	  return -1;
     }
#endif

     /* add rows from list */
     tree_traverse(row) {
	  if (tree_find(t->data, tree_getkey(row)) == TREE_NOVAL)
	       continue; /* return -1; */
	  rowkey = itree_append( tree_get(t->data), tree_get(row) );
     }

     if ( t->nrows == 0 )
	  t->minrowkey = 0;
     t->nrows++;
     t->maxrowkey = rowkey;

     /* go through table and check we have values for all columns, 
      * if not even up by adding NULLS */
     tree_traverse(t->data) {
	  itree_last(tree_get(t->data));
	  if (itree_getkey(tree_get(t->data)) != rowkey)
	       itree_append(tree_get(t->data), NULL);
     }

     return rowkey;
}



/*
 * add rows from a 2d array. array should be NULL terminated.
 * returns the number of rows added or -1 if there was a problem
 */
int table_addrows_a(TABLE t, void ***array){
  /*  TO BE IMPLEMENTED */
     return 0;
}


/* 
 * Adds a row, with the cells being set to NULL.
 * The table's state is made to point to the new row.
 * Use table_replacecell() or table_replacecurrentcell() to fill with data.
 * Returns the row key of the new row 
 */
int table_addemptyrow(TABLE t)
{
     int rowkey;

     rowkey = -1;

     /* add blank cells to all columns */
     tree_traverse(t->data) {
	  rowkey = itree_append( tree_get(t->data), NULL );
	  itree_last( tree_get(t->data) );
     }

     /* update counters */
     if ( t->nrows == 0 )
	  t->minrowkey = 0;
     t->nrows++;
     t->maxrowkey = rowkey;

     return rowkey;
}


/*
 * Add the contents of table `rows' to this one; new rows are appended.
 * If expand is true, columns from the donor table will be added to
 * the recipient; if false, the column quantity and names are checked 
 * for an exact match before attempting an add.
 * If a column is added, its info cells are added also, creating the 
 * necessary info lines as appropreate.
 * Only columns set in the colorder list will be transferred (which may 
 * fight against the column checking when expand is false).
 * All data is duplicated resulting in the new table having no dependency 
 * on the contributing table.
 * Returns the number of rows added or -1 if the colums did not match and 
 * expand was false.
 */
int table_addtable(TABLE t, TABLE rows, int expand) {
     int nrowsadded=0;
     TREE *singlerow, *infocol;
     char *tmp, *colname;

#if 0
     /* check number of rows */
     if (rows->nrows <= 0)
	  return 0;
#endif

     if (expand) {
	  /* create new columns to accommodate data */
	  itree_traverse(rows->colorder) {
	       if (tree_find(t->data, itree_get(rows->colorder))==TREE_NOVAL) {
		    colname = xnstrdup(itree_get(rows->colorder));
		    table_addcol(t, colname, NULL);
		    table_freeondestroy(t, colname);
		    infocol = table_getinfocol(rows, colname);
		    tree_traverse(infocol) {
			 if (tree_find(t->infolookup,
				       tree_getkey(infocol)) == TREE_NOVAL) {
			      tmp = xnstrdup(tree_getkey(infocol));
			      table_addemptyinfo(t, tmp);
			      table_freeondestroy(t, tmp);
			 }
			 tmp = xnstrdup(tree_get(infocol));
			 table_replaceinfocell(t, tree_getkey(infocol),
					       colname, tmp);
			 table_freeondestroy(t, tmp);
		    }
		    tree_destroy(infocol);
	       }
	  }
     } else {
	  /* check number of columns */
	  if (t->ncols && rows->ncols != t->ncols)
	       return -1;

	  if (t->ncols) {
	       /* check column names */
	       tree_first(rows->data);
	       tree_traverse(t->data) {
		    if (strcmp(tree_get(t->data), tree_get(rows->data)))
			 return -1;
		    tree_next(rows->data);
	       }
	  } else {
	       /* use the column headers to setup this table
		* column headers are duplicated for saftey */
	       t->ncols = rows->ncols;
	       tree_traverse(rows->data) {
		    tmp = xnstrdup(tree_get(rows->data));
		    tree_add(t->data, tmp, itree_create());
		    table_freeondestroy(t, tmp);
	       }
	  }
     }

     /* iterate over the import table and append to our rows.
      * again duplicate cells for saftey */
     table_traverse(rows) {
#if 0
     table_first(rows);
     while (1) {
#endif
	  singlerow = table_getcurrentrow(rows);
	  table_addrow_alloc(t, singlerow);
	  tree_destroy(singlerow);
	  nrowsadded++;
#if 0
	  if (table_isatlast(rows))
	       break;
	  table_next(rows);
#endif
     }

     return nrowsadded;
}

/* remove the row addressed by rowkey */
void table_rmrow(TABLE t, int rowkey)
{
     ITREE *column=NULL;
     int rowfound=0;

     if (t->nrows == 0)
	  return;	/* nothing to remove */

     tree_traverse(t->data) {
	  column = tree_get(t->data);
	  if (itree_find(column, rowkey) != ITREE_NOVAL) {
	       itree_rm(column);
	       rowfound++;
	  }
     }

     if ( ! rowfound )
	  return;	/* key not there to remove */

     /* handling emptyness */
     t->nrows--;
     if (t->nrows == 0) {
	  /* empty */
	  t->minrowkey = t->maxrowkey = -1;
     } else {
	  /* update counter, first and last row indexes */
	  itree_first(column);
	  t->minrowkey = itree_getkey(column);
	  itree_last(column);
	  t->maxrowkey = itree_getkey(column);
     }
}


/* return the location of the cell addressed by rowkey,colname */
void *table_getcell(TABLE t, int rowkey, char *colname)
{
  ITREE *column;
  void *celldata;

  /* find cell */
  column = tree_find(t->data, colname);
  if ( column == TREE_NOVAL )
    return NULL;
  celldata = itree_find(column, rowkey);
  if ( celldata == ITREE_NOVAL )
    return NULL;

  return celldata;
}


/*
 * Search the table for data `needle' within the column named `haystack'.
 * Return the row index if a match is made or -1 for no match.
 * The row found from the table is set to be current, so a call to 
 * table_getcurrentcell(t, haystack) will return needle.
 * Note that this operation is slow as the search is linear: use on small
 * tables!!
 */
int    table_search(TABLE t, char *haystack, char *needle) 
{
     int rowidx;
     ITREE *c;

     /* get column's ITREE (the selected haystack) and search for needle */
     c = tree_find(t->data, haystack);
     if (c == TREE_NOVAL)
	  return -1;
     rowidx = itree_search(c, needle, strlen(needle));
     if (rowidx == -1)
	  return -1;

     /* set the current row to the one that contains the key */
     tree_traverse(t->data) {
	  c = tree_get(t->data);
	  itree_find(c, rowidx);
     }

     return rowidx;
}


/*
 * Search the table for two data: needle1 in the column haystack1 and
 * needle2 in haystack2.
 * Return the row index if a match is made or -1 for no match.
 * The row found from the table is set to be current, so a call to 
 * table_getcurrentcell(t, haystack1) will return needle1.
 * Note that this operation is slow as the search is linear: use on small
 * tables!!
 */
int    table_search2(TABLE t, char *haystack1, char *needle1, char *haystack2,
		     char *needle2) 
{
     table_traverse(t) {
          if (strcmp(table_getcurrentcell(t, haystack1), needle1) == 0
	      && strcmp(table_getcurrentcell(t, haystack2), needle2) == 0) {

	       /* we have a match */
	       return table_getcurrentrowkey(t);
	  }
     }

     return -1;
}


/*
 * Copy string `newcelldata' into cell addressed by rowkey,column;
 * the previous location is just dereferenced. The new cell is nmalloc()ed
 * for saftey.
 * Returns 1 if successful or 0 if unable to find the cell
 */
int table_replacecell_alloc(TABLE t, int rowkey, char *colname, 
			    char *newcelldata)
{
     ITREE *column;
     void *celldata, *dupnewdata;

     /* find cell */
     column = tree_find(t->data, colname);
     if ( column == TREE_NOVAL)
          return 0;
     celldata = itree_find(column, rowkey);
     if ( celldata == ITREE_NOVAL )
          return 0;

     /* free previous inhabitant and set new one */
     dupnewdata = xnstrdup(newcelldata);
     itree_put(column, dupnewdata);
     table_freeondestroy(t, dupnewdata);

     return 1;
}


/*
 * Set string `newcelldata' into cell addressed by rowkey,column;
 * the previous location is just dereferenced. The new cell will rely on the
 * data to be there for the lifetime of this TABLE.
 * Returns 1 if successful or 0 if unable to find the cell
 */
int table_replacecell_noalloc(TABLE t, int rowkey, char *colname, 
			      char *newcelldata)
{
  ITREE *column;
  void *celldata;

  /* find cell */
  column = tree_find(t->data, colname);
  if ( column == TREE_NOVAL )
    return 0;
  celldata = itree_find(column, rowkey);
  if ( celldata == ITREE_NOVAL )
    return 0;

  /* free previous inhabitant and set new one */
  itree_put(column, newcelldata);

  return 1;
}


/*
 * return a row from the table, specified by the index rowkey. The TREE
 * returned has its cells indexed by column name. free the list with
 * tree_destroy() as NONE of the data or keys are copies 
 * returns NULL if unable to find the rowkey.
 */
TREE *table_getrow(TABLE t, int rowkey)
{
  TREE *row;
  ITREE *column;

  row = tree_create();
  tree_traverse(t->data) {
    column = tree_get(t->data);
    if ( itree_find(column, rowkey) == ITREE_NOVAL) {
      elog_printf(DEBUG, "cant find row key %d", rowkey);
      return NULL;
    }
    tree_add(row, tree_getkey(t->data), itree_get(column));
  }

  return row;
}


/*
 * Return a column from the table, specified by colname. 
 * The ITREE returned has its cells indexed by rowindex.
 * Free the list with itree_destroy() as NONE of the data or keys are copies.
 * Returns NULL if unable to find the rowkey.
 */
ITREE *table_getcol(TABLE t, char *colname)
{
  ITREE *column, *newcolumn = NULL;

  column = tree_find(t->data, colname);
  if (column == TREE_NOVAL) {
       return NULL;
  } else {
       newcolumn = itree_create();
       itree_traverse(column)
	    itree_add(newcolumn, itree_getkey(column), itree_get(column));
  }

  return newcolumn;
}



/*
 * Return a column from the table, specified by colname and sorted into
 * order by roworder.
 * Roworder is an ordered list of rowkeys, where the key is a casted int
 * (sorry), thus `(int) tree_get(roworder)' will get the value.
 * The ITREE returned has its cells indexed by rowindex.
 * Free the list with itree_destroy() as NONE of the data or keys are copies.
 * Returns NULL if unable to find the rowkey.
 */
ITREE *table_getsortedcol(TABLE t, char *colname)
{
     ITREE *column, *newcolumn = NULL;
     int rowindex;

     if (colname == NULL)
	  return NULL;

     if (t->roworder == NULL)
	  return table_getcol(t, colname);

     column = tree_find(t->data, colname);
     if (column == TREE_NOVAL) {
	  return NULL;
     } else {
	  newcolumn = itree_create();
	  itree_traverse(t->roworder) {
	       rowindex = (int) itree_get(t->roworder);
	       itree_add(newcolumn, rowindex, itree_find(column, rowindex));
	  }
     }

     return newcolumn;
}




/*
 * Add column to table, adding the data present in coldata.
 * The keys present in coldata need not match with the table keys as they
 * will be added in order using the existing state of the keys. 
 * If the column has fewer cells than rows, the remainder
 * of the column will be padded with empty strings. If coldata has
 * more data than can be fitted, then it will be truncated.
 * If coldata is NULL, then a column list is created and all the rows 
 * will be set to NULL.
 * If the table is empty, then there are no restrictions and the table
 * is made the height of the input rows, with sequential keys.
 * If the column already exists, and error is returned.
 * The column is appended to the order of columns.
 * coldata values and column name should be kept during the lifetime 
 * of the TABLE,
 * so consider using table_freeondestroy().
 * Returns the number of cells created in the table or -1 for an error
 */
int table_addcol(TABLE t, char *colname, ITREE *coldata)
{
     ITREE *newcol, *refcol;
     int remain, refcolkey;

     if (table_hascol(t, colname))
	  return -1;	/* already have column */

     if (t->nrows) {
	  /* --- table has existing data --- */

	  /* collect reference column for current row keys */
	  tree_first(t->data);
	  refcol = tree_get(t->data);
	  refcolkey = itree_getkey(refcol);

	  /* prepare supplied data */
	  if (coldata) {
	       remain = itree_n(coldata);
	       itree_first(coldata);
	  } else
	       remain = 0;

	  /* create new column and make similar to reference */
	  newcol = itree_create();
	  itree_traverse(refcol) {
	       if (remain) {
		    /* copy data out of supplied column */
		    itree_add(newcol, itree_getkey(refcol), 
			      itree_get(coldata));
		    itree_next(coldata);
		    remain--;
	       } else {
		    /* supplied column data exhausted, fill with NULLs */
		    itree_add(newcol, itree_getkey(refcol), NULL );
	       }
	  }
	  itree_find(refcol, refcolkey);	/* restore ref col position */
     } else {
	  /* --- table is empty --- */

	  /* create new column and fill sequentially from provided list */
	  newcol = itree_create();
	  if (coldata) {
	       itree_traverse(coldata)
		    itree_append(newcol, itree_get(coldata) );
	       /* set up min and max keys */
	       t->minrowkey = 0;
	       t->nrows = itree_n(coldata);
	       t->maxrowkey = t->nrows-1;
	  }
     }

     /* add new column to table and return. */
     tree_add(t->data, colname, newcol);
     itree_append(t->colorder, colname);
     t->ncols++;
     return t->nrows;
}


/* remove column but leave data intact */
void table_rmcol(TABLE t, char *colname)
{
     ITREE *collist;

     collist = tree_find(t->data, colname);
     if (collist == TREE_NOVAL)
	  return;

     /* column exists, so remove it */
     itree_destroy(collist);
     tree_rm(t->data);
     t->ncols--;
     itree_traverse(t->colorder)
	  if ( strcmp((char *)itree_get(t->colorder), colname) == 0 ) {
	       itree_rm(t->colorder);
	       break;
	  }
     if (tree_find(t->info, colname) != TREE_NOVAL) {
          itree_destroy( tree_get(t->info) );
	  tree_rm(t->info);
     }
}


/*
 * Rename a column.
 * Returns 1 for successful or 0 if there the column did not exist or 
 * there was already a column with the new name
 */
int    table_renamecol(TABLE t, char *oldcolname, char *newcolname)
{
     char *name;
     void *data;

     if (tree_find(t->data, newcolname) != TREE_NOVAL)
	  return 0;	/* failure - new col name already exists */

     if ((data = tree_find(t->data, oldcolname)) == TREE_NOVAL)
	  return 0;	/* failure - rename col not there */

     /* make and register new nme */
     name = xnstrdup(newcolname);
     table_freeondestroy(t, name);

     /* update data: colnames */
     tree_rm(t->data);
     tree_add(t->data, name, data);

     /* update col order list: change name but keep same order */
     itree_traverse(t->colorder)
	  if (strcmp(itree_get(t->colorder), oldcolname) == 0)
	       itree_put(t->colorder, name);

     /* update info */
     data = tree_find(t->info, oldcolname);
     if (data != TREE_NOVAL) {
	  tree_rm(t->info);
	  tree_add(t->info, name, data);
     }

     return 1;	/* success */
}


/*
 * Set the column order of table_print*(), table_*width() and 
 * table_out*() commands to be `colorder'.
 * Colorder is an ordered list of column names, that by default are in the
 * order in which they were created.
 * The colorder data and tree should be kept intact for the life of the table,
 * and only the ITREE will be free()ed on the destruction of the table.
 * Thus, the column names should be given to table_freeondestroy() or should
 * be the internal values used by the TABLE implementation.
 * Can be used to remove columns from being printed.
 * Returns the old list is being displaced without destruction or NULL 
 * if there was no list before.
 */
ITREE *table_setcolorder(TABLE t, ITREE *colorder)
{
     ITREE *co=NULL;

     if (t->colorder)
	  co = t->colorder;
     t->colorder = colorder;

     return co;
}


/*
 * Return the column order of the table as an ITREE.
 * The returned tree is part of the table and should not be altered.
 * The ITREE may be used until the destruction of the table or until
 * the column order is reset, in the latter case the data will be available
 * until destruction but the ITREE will be destroyed.
 */
ITREE *table_getcolorder(TABLE t)
{
     return t->colorder;
}


/* 
 * Traverse the table and work out the maximum widths of the cells 
 * specified in the list `cols'. Count only the band of rows
 * starting with the rowkey `fromrowkey' and ending `torowkey'.
 * Returns a nmalloc()ed array of ints, the size and order of the
 * colname index. Free with nfree() after use.
 * Return NULL for error.
 */
int *table_datawidths(TABLE t, int fromrowkey, int torowkey, ITREE *cols)
{
  int max, lencell, row, *cw, ncol;
  ITREE *col;
  void *val;

  if (cols == 0)
       return NULL;

  if (itree_n(cols) == 0)
       return NULL;

  cw = xnmalloc( itree_n(cols) * sizeof(int) );
  ncol = 0;
  itree_traverse(cols) {
    /* get named column */
    tree_find(t->data, itree_get(cols) );
    col = tree_get(t->data);

    /* find max in row band */
    max = 0;
    val = itree_find(col, fromrowkey);
    if ( val != ITREE_NOVAL && val )
	 for (row=fromrowkey; row<=torowkey; row++) {
	      if (itree_get(col)) {
		   lencell = strlen( itree_get(col) );
		   if (lencell > max)
			max = lencell;
	      }
	      itree_next(col);
	 }

    /* store result */
    cw[ncol++] = max;
  }

  return cw;
}


/* 
 * Find the widths of all data columns in the band of rows `fromrowkey'
 * to `torowkey'.
 * Returns an array of ints, the size and order of the internal t->data
 * list (alphabetical column list). This list should be released with 
 * nfree() after use.
 */
int *table_alldatawidths(TABLE t, int fromrowkey, int torowkey)
{
  return table_datawidths(t, fromrowkey, torowkey, t->colorder);
}


/* 
 * Find the widths of all data columns in all rows
 * Returns an array of ints, the size and order of the internal t->data
 * list (alphabetical column list). This list should be released with 
 * nfree() after use.
 */
int *table_everydatawidth(TABLE t)
{
     return table_alldatawidths(t, t->minrowkey, t->maxrowkey);
}


/* 
 * Find the maximum widths of the columns specified in the 
 * list `cols': headers strings and all the data cells in the column 
 * are checked. 
 * Count only the band of rows starting with the rowkey `fromrowkey' 
 * and ending `torowkey' and their headers.
 * Returns a nmalloc()ed array of ints, the size and order of the
 * cols index. Free with nfree() after use.
 * Return NULL for error.
 */
int *table_colwidths(TABLE t, int fromrowkey, int torowkey, ITREE *cols)
{
  int *colwidths, hwidth, ncol;

  colwidths = table_datawidths(t, fromrowkey, torowkey, cols);
  if (colwidths) {
    ncol = 0;
    tree_traverse(cols) {
      hwidth = strlen(itree_get(cols));
      if (hwidth > colwidths[ncol])
	colwidths[ncol] = hwidth;
      ncol++;
    }
  }

  return colwidths;
}


/* 
 * Find the widths of all the columns in a band of rows starting with 
 * the rowkey `fromrowkey' and ending `torowkey' and their
 * headers. The number for each column is the larger of the banded data
 * width and the length of its column name.
 * Returns a nmalloc()ed array of ints, the size and order of the
 * cols index. Free with nfree() after use.
 * Return NULL for error.
 */
int *table_allcolwidths(TABLE t, int fromrowkey, int torowkey)
{
  return table_colwidths(t, fromrowkey, torowkey, t->colorder);
}



/* 
 * Find the widths of all the columns and headers in the table.
 * The number for each column is the larger of the data
 * width and the length of its column name.
 * Returns a nmalloc()ed array of ints, the size and order of the
 * cols index. Free with nfree() after use.
 * Return NULL for error.
 */
int *table_everycolwidth(TABLE t)
{
     return table_allcolwidths(t, t->minrowkey, t->maxrowkey);
}


/* print entire table in string, see table_printrows() for details */
char *table_print(TABLE t) {
  return table_printrows(t, t->minrowkey, t->maxrowkey);
}

/* print entire table in string, see table_printrows() for details */
char *table_printrow(TABLE t, int rowkey) {
  return table_printrows(t, rowkey, rowkey);
}

/*
 * print a sequence of several rows from the table and return a string 
 * to print. when finished, nfree() the string. Columns are printed
 * in the order of colorder, if set or the alphabetical order of the
 * headers otherwise.
 */
char *table_printrows(TABLE t, int fromrowkey, int torowkey)
{
     return table_printselect(t, fromrowkey, torowkey, t->colorder);
}



/*
 * Print a sequence of several rows from the table including the columns
 * specified by name and order by colorder and return a string 
 * to print. When finished, nfree() the string.
 * Returns NULL for error.
 */
char *table_printselect(TABLE t, int fromrowkey, int torowkey, ITREE *colorder)
{
  char *output, *outputpt, fmt[TABLE_FMTLEN];
  int maxwidth, i, j, colindex, outalloc, *widths, ncols;

  /* get the column widths in an nmalloced array of int */
  widths = table_colwidths(t, fromrowkey, torowkey, colorder);
  if ( ! widths )
    return NULL;

  /* calculate max width or each row and thus create string buffer */
  ncols = itree_n(colorder);
  maxwidth = 0;
  for (i=0; i < ncols; i++)
    maxwidth += widths[i] + 1;	/* column width + space */
  maxwidth++;			/* newline */
  outalloc = maxwidth * (torowkey - fromrowkey + 3) + 10;
  output = outputpt = xnmalloc(outalloc);

  /* output header */
  for (i=0, itree_first(colorder); i < ncols; i++, itree_next(colorder)) {
       sprintf(fmt, "%%-%ds ", widths[i]);
       outputpt += sprintf(outputpt, fmt, itree_get(colorder));
  }
  *(outputpt-1) = '\n';

  /* output ruler */
  for (i=0; i < ncols; i++) {
    for (j=0; j < widths[i]; j++)
      *(outputpt++) = '-';
    *(outputpt++) = ' ';
  }
  *(outputpt-1) = '\n';

  /* initialise data column trees to correct starting point */
  tree_traverse(t->data)
    itree_find( tree_get(t->data), fromrowkey );

  /* traverse the data rows, if there are any */
  for (i=fromrowkey; i<=torowkey && torowkey!=-1; i++) {
    /* loop over each cell in the row & print */
    j=0;
    itree_traverse(colorder) {
      /* create a srintf format string at the correct width */
      sprintf(fmt, "%%-%ds ", widths[j]);

      /* get data cell and print as string if in range or `-' if not */
      tree_find(t->data, itree_get(colorder));
      itree_find( tree_get(t->data), i);
      colindex = itree_getkey( tree_get(t->data) );
      if (colindex >= fromrowkey && colindex <= torowkey)
	if ( tree_get( tree_get(t->data) ) &&
	     *((char *) itree_get( tree_get(t->data) )) )
	  outputpt += sprintf(outputpt, fmt, itree_get( tree_get(t->data) ) );
        else
	  outputpt += sprintf(outputpt, fmt, "-");
      else
	outputpt += sprintf(outputpt, fmt, "-");
      j++;
    }

    /* end of row: newline */
    *(outputpt-1) = '\n';
  }

  /* terminate string, clear up and return */
  *(outputpt++) = '\0';

  nfree(widths);
  return output;
}


/*
 * Render a table in HTML suitable for human reading rather than machine 
 * parsing. 
 * If fromrowkey is -1, the first row will be used, 
 * if torowkey is -1 then the last value will be used. 
 * If colorder is NULL, then all attributes are rendered in the default 
 * column order of the table.
 * Returns a buffer containing the HTML which should be nfree()ed after use
 */
char *table_html(TABLE t, int fromrowkey, int torowkey, ITREE *colorder)
{
  char *out, *cell;
  int outlen, outalloc, i, fromrow=0, torow=0;
  ITREE *attr, *col;

  /* set defaults */
  if (colorder)
       attr = colorder;
  else
       attr = t->colorder;
  if (fromrowkey == -1) {
       tree_first(t->data);
       col = tree_get(t->data);
       itree_first(col);
       fromrow = itree_getkey(col);
  }
  if (torowkey == -1) {
       tree_first(t->data);
       col = tree_get(t->data);
       itree_last(col);
       torow = itree_getkey(col);
  }

  /* output header */
  out = xnmalloc((outalloc = TABLE_OUTBLOCKSZ));
  outlen = snprintf(out, outalloc, "<table>\n<tr align=left>");
  itree_traverse(attr) {
       if (outlen + 50 > outalloc) {
	    outalloc += TABLE_OUTBLOCKSZ;
	    out = xnrealloc(out, outalloc);
       }
       outlen += snprintf(out+outlen, outalloc-outlen, "<th>%s</th>", 
			  (char *) itree_get(attr));
  }
  outlen += snprintf(out+outlen, outalloc-outlen, "</tr>\n");

  /* output info */
  tree_traverse(t->infolookup) {
       outlen += snprintf(out+outlen, outalloc-outlen, "<tr align=left>");
       itree_traverse(attr) {
	    col = tree_find(t->info, itree_get(attr));
	    if (col == TREE_NOVAL) {
		 cell = "";
	    } else {
		 cell = itree_find(col, (int) tree_get(t->infolookup));
		 if (cell == ITREE_NOVAL || cell == NULL) {
		      cell = "";
		 }
	    }

	    if (outlen + 70 > outalloc) {
		 outalloc += TABLE_OUTBLOCKSZ;
		 out = xnrealloc(out, outalloc);
	    }
	    outlen += snprintf(out+outlen, outalloc-outlen, 
			       "<td><i>%s<i></td>", cell);
       }
       outlen += snprintf(out+outlen, outalloc-outlen, "</tr>\n");
  }

  /* output data */
  for (i = fromrow; i <= torow; i++) {
       outlen += snprintf(out+outlen, outalloc-outlen, "<tr align=left>");
       itree_traverse(attr) {
	    col = tree_find(t->data, itree_get(attr));
	    if (col == TREE_NOVAL) {
		 cell = "";
	    } else {
		 cell = itree_find(col, i);
		 if (cell == ITREE_NOVAL || cell == NULL) {
		      cell = "";
		 }
	    }

	    if (outlen + 200 > outalloc) {
		 outalloc += TABLE_OUTBLOCKSZ;
		 out = xnrealloc(out, outalloc);
	    }
	    outlen += snprintf(out+outlen, outalloc-outlen, "<td>%s</td>", 
			       cell);
       }
       outlen += snprintf(out+outlen, outalloc-outlen, "</tr>\n");
  }

  outlen += snprintf(out+outlen, outalloc-outlen, "</table>\n");

  /* return the stuff */
  return out;
}


/*
 * Print header to buffer in the order set by the structure's colorder.
 * Returns the header string with no terminating new line, which should 
 * be nfree()ed after use
 */
char *table_outheader(TABLE t)
{
     int i=0;
     char *outbuf, *outpt;

     /* return if no columns */
     if (t == NULL || t->ncols == 0)
	  return NULL;

     /* get size of buffer */
     tree_traverse(t->data)
	  i += strlen( tree_getkey(t->data) )+3;

     /* allocate and copy headers */
     outpt = outbuf = xnmalloc(i);
     itree_traverse(t->colorder) {
	  if (tree_find(t->data, itree_get(t->colorder)) == TREE_NOVAL)
	       continue;
	  util_quotestr(tree_getkey(t->data), "\t", outpt, i-(outpt-outbuf));
	  while (*outpt)
	       outpt++;
	  *(outpt++) = t->separator;
     }
     *(--outpt) = '\0';

     return outbuf;
}


/*
 * Print the table's info section in the order dictated by colorder.
 * Unlike the _outhead() and _outbody(), this will print an additional column
 * at the end of each info line, which contains the info name of that line.
 * Scanners/parsers beware!!
 * Returns the info header string, which should be nfree()ed after use
 */
char *table_outinfo(TABLE t)
{
     int toklen, bufalloc;
     char *outbuf, *outpt, *tok, *newbuf, strbuf[8000];

     /* return if no data or info */
     if (t == NULL || t->ncols == 0 || tree_n(t->infolookup) == 0)
	  return NULL;

     /* allocate fixed buffer for speed */
     bufalloc = TABLE_OUTINFOBUFSZ;
     outpt = outbuf = xnmalloc(TABLE_OUTINFOBUFSZ);

     /* allocate and copy headers */
     tree_traverse(t->infolookup) {
	  itree_traverse(t->colorder) {
	       tok = "";
	       if (tree_find(t->info, itree_get(t->colorder)) != TREE_NOVAL)
		    if (itree_find(tree_get(t->info), (int) 
				   tree_get(t->infolookup)) != TREE_NOVAL)
		         tok = util_quotestr( itree_get( tree_get(t->info) ), 
					      "\t", strbuf, 8000 );
	       toklen = strlen(tok);
	       if (toklen + outpt > outbuf + bufalloc + 100) {
		    /* enlarge buffer and adjust pointers */
		    bufalloc += TABLE_OUTINFOBUFSZ;
		    newbuf = xnrealloc(outbuf, bufalloc);
		    outpt += outbuf - newbuf;
		    outbuf = newbuf;
	       }
	       strcpy(outpt, tok);
	       outpt += toklen;
	       *(outpt++) = t->separator;
	  }
	  tok = util_quotestr( tree_getkey(t->infolookup), 
			       "\t", outpt, bufalloc - (outpt-outbuf) );
	  outpt += strlen(tok);	/* assume max 100 chars */
	  *(outpt++) = '\n';
     }
     *(outpt-1) = '\0';

     return outbuf;
}


/*
 * Print body of table to buffer in order dictated by colorder.
 * Returns the body string, which should be nfree()ed after use
 */
char *table_outbody(TABLE t)
{
     int alloc=0, used=0, len;
     char *outbuf, *outpt, *tok, *oldbuf, strbuf[8000], *cell;
     ITREE *col;

     /* return if no data */
     if (t == NULL || t->nrows == 0)
	  return NULL;

     /* get size of buffer */
     tree_traverse(t->data) {
	  col = tree_get(t->data);
	  itree_traverse(col)
	       if ( itree_get(col) )
		    alloc += strlen( itree_get(col) )+3;
	       else
		    alloc += 3;
     }

     if (alloc == 0)
	  return NULL;

     /* allocate and copy body cells */
     outpt = outbuf = xnmalloc(alloc);
     table_traverse(t) {
	  itree_traverse(t->colorder) {
	       /* get each cell, quote it and copy into buffer. If the
		* buffer is too small, allocate a bigger one */
	       cell = table_getcurrentcell(t, itree_get(t->colorder));
	       tok = util_quotestr(cell, "\t", strbuf, 8000 );
	       len = strlen(tok);
	       used += len+1;	/* include the additional trailing space */
	       if (used+1 > alloc) {
		    /* allocate more memory to fit the current requirement, 
		     * plus some more (64 at mo) */
		    alloc = used+64;
		    oldbuf = outbuf;
		    outbuf = xnrealloc(outbuf, alloc);
		    if (outbuf != oldbuf)
			 /* location of buffer has changed: so adjust */
			 outpt += outbuf-oldbuf;
	       }
	       strcpy(outpt, tok);
	       outpt += len;
	       *(outpt++) = t->separator;
	  }
	  *(outpt-1) = '\n';
     }
     *(outpt) = '\0';


     return outbuf;
}



/*
 * Print selected rows from the body of the table to buffer, in an order 
 * dictated by colorder.
 * Returns the string of rows, which should be nfree()ed after use
 */
char *table_outrows(TABLE t, 		/* table */
		    int fromkey,	/* start from this rowkey */
		    int tokey		/* end at this rowkey */ )
{
     int i, alloc=0, used=0, len;
     char *outbuf, *outpt, *tok, *oldbuf, strbuf[8000];
     ITREE *col;

     /* return if no data */
     if (t == NULL || t->nrows == 0)
	  return NULL;

     /* get size of buffer */
     tree_traverse(t->data) {
	  col = tree_get(t->data);
	  for (i=fromkey; i<=tokey; i++) {
	       if (itree_find(col, i) == ITREE_NOVAL)
		    continue;
	       if ( itree_get(col) )
		    alloc += strlen( itree_get(col) )+3;
	       else
		    alloc += 3;
	  }
     }

     if (alloc == 0)
	  return NULL;

     /* allocate and copy body cells */
     outpt = outbuf = xnmalloc(alloc);
     for (i=fromkey; i<=tokey; i++) {
	  itree_traverse(t->colorder) {
	       /* locate valid lines */
	       tree_find(t->data, itree_get(t->colorder));
	       if (itree_find(tree_get(t->data), i) == ITREE_NOVAL)
		    goto table_outrows_nextrow;		/* deleted row */

	       /* get each cell, quote it and copy into buffer. If the
		* buffer is too small, allocate a bigger one */
	       tok = util_quotestr( itree_get( tree_get(t->data) ),
				    "\t", strbuf, 8000 );
	       len = strlen(tok);
	       used += len+1;	/* include the additional trailing space */
	       if (used+1 > alloc) {
		    /* allocate more memory to fit the current requirement, 
		     * plus some more (64 at mo) */
		    alloc = used+64;
		    oldbuf = outbuf;
		    outbuf = xnrealloc(outbuf, alloc);
		    if (outbuf != oldbuf)
			 /* location of buffer has changed: so adjust */
			 outpt += outbuf-oldbuf;
	       }
	       strcpy(outpt, tok);
	       outpt += len;
	       *(outpt++) = t->separator;
	  }
	  *(outpt-1) = '\n';
     table_outrows_nextrow:
	  ;
     }
     if (outpt == outbuf)
	  outpt++;
     *(outpt-1) = '\0';


     return outbuf;
}



/*
 * Output the whole table suitable for scanning, rather than for human reading.
 * Columns are not justified to save space and time.
 * The column delimiter is t->separator (usually '\t') and instances 
 * of this character in cells are escaped.
 * The column names always occupy a singe line. 
 * The ruler line is present but truncated to just '--' and
 * is used to seperate the arbitary number of info lines from the body.
 * The text buffer returned should be nfree()ed once used. If the table
 * has no rows, the NULL will be returned.
 */
char *table_outtable(TABLE t)
{
     char *header, *info, *body, *output;

     /* return if no data */
     if (t == NULL || t->nrows == 0)
	  return NULL;

     /* get data */
     header = table_outheader(t);
     info = table_outinfo(t);
     body = table_outbody(t);

     /* perform joins, depending on data */
     if (info)
	  output = util_strjoin(header, "\n", info, "\n--\n", body, NULL);
     else
	  output = util_strjoin(header, "\n--\n", body, NULL);

     /* clean up */
     nfree(header);
     if (info)
	  nfree(info);
     nfree(body);

     return output;
}


/* 
 * Output the whole table suitable for scanning, rather than for human reading.
 * Columns are not justified to save space and time and cells have their 
 * delimiting/special characters escaped. 
 * `sep' is the character used to seperate the cells on each line.
 * If `withcolnames' is set, a row of column names are printed on a single 
 * line at the top of the output text.
 * If `withinfo' is set, the zero or more lines are printed representing
 * each colummns info data, followed by a single line containing `--',
 * wich separates the info from the body vertically.
 * Note: the info lines always have one more column that the header or body,
 * which is the name of that info line.
 * Finally, the body of the table is formatted into the text buffer.
 *
 * The format should look like the following:-
 * WITHCOLNAMES-> col1_name  <sep> col2_name
 * WITHINFO->     col1_info1 <sep> col2_info1 <sep> info1_name
 * WITHINFO->     col1_info2 <sep> col2_info2 <sep> info2_name
 * WITHINFO->     --
 *                col1_row1  <sep> col2_row1
 *                col1_row2  <sep> col2_row2
 *
 * The text buffer returned should be nfree()ed once used. If the table
 * has no rows, the NULL will be returned.
 */
char  *table_outtable_full(TABLE t, char sep, int withcolnames, int withinfo)
{
     char *header=NULL, *info=NULL, *body, *output, origsep;

     /* return if no data */
     if (t == NULL || t->nrows == 0)
	  return NULL;

     /* get data */
     origsep = t->separator;
     t->separator = sep;
     if (withcolnames)
	  header = table_outheader(t);
     if (withinfo)
	  info = table_outinfo(t);
     body = table_outbody(t);

     /* perform joins and free working data after use, depending on what 
      * has been asked for and if info is available */
     if (withcolnames) {
	  if (withinfo) {
	       if (info) {
		    output = util_strjoin(header, "\n", info, "\n--\n", body, 
					  NULL);
		    nfree(info);
	       } else {
		    output = util_strjoin(header, "\n--\n", body, NULL);
	       }
	  } else {
	       output = util_strjoin(header, "\n--\n", body, NULL);
	  }
	  nfree(header);
	  nfree(body);
     } else {
	  output = body;
     }

     t->separator = origsep;
     return output;
}



/*
 * Scan a text buffer with a multi-row, multi-column structure into
 * the table.
 * The characters that can separate the values of each column in the
 * buffer are specified by the string `sepstr'.
 * If mode is set to TABLE_MULTISEP, multiple separators are treated as 
 * one; else if TABLE_SINGLESEP, seach separator will be treated as 
 * a column delimiter (cf. util_scantext()); else if TABLE_CFMODE
 * the table will follow configuration separation rules, which are multisep
 * and filter out comments (magic numbers not allowed).
 * If the table has columns defined, then the text buffer should have 
 * the same number. If the table has no columns, then the table will be
 * given the number of column present in the buffer.
 * If the `hascolnames' flag is set, then the column names will be 
 * taken from the first line of the buffer and the strings must be unique
 * (TABLE_HASCOLNAMES & TABLE_NOCOLNAMES can be used as flags.)
 * If the flag is not set, then default column names will be created.
 * If the flag `hasruler' is set, then the buffer is scanned from the 
 * header line to a line containing `--' as the first string of the first 
 * column. This line is discarded and the following line is treated as data.
 * Lines between the column names and the ruler are treated as info lines.
 * Info lines have one more column than the table as the final column will
 * be the info name.
 * (TABLE_HASRULER & TABLE_NORULER can be used as flags.)
 * If the flag is not set, there will be no info lines and the next buffer
 * line is considered data.
 * The input buffer should be kept for the life of the table, as the 
 * data will be refered to and will be modified in the scanning process. 
 * The caller could consider passing the data to table_freeondestroy() 
 * for co-ordinated destruction.
 * Returns -1 for error, the buffer did not have the right number of columns
 * or the columns were not constant in each row (allowing for the extra info 
 * name column)
 * Otherwise returns the number of data rows parsed, which exclude the 
 * columnn names, info lines and separator (--).
 */
int table_scan(TABLE t, char *buffer, char *sepstr, int mode, 
	       int hascolnames, int hasruler)
{
     int i, nlines, ncols, line, row_ncols;
     ITREE *colnames=NULL, *row, *parselist=NULL;
     char tmp[20], *tmpcpy, *iname;

     /* scan the buffer into word lists */
     if (mode == TABLE_SINGLESEP)
          nlines = util_scantext(buffer, sepstr, UTIL_SINGLESEP, &parselist);
     else if (mode == TABLE_MULTISEP)
          nlines = util_scantext(buffer, sepstr, UTIL_MULTISEP, &parselist);
     else if (mode == TABLE_CFMODE)
          nlines = util_scancftext(buffer, sepstr, NULL, &parselist);
     if (nlines == -1) {
	  if (parselist)
	       util_scanfree(parselist);
	  return -1;		/* error */
     }
     if (nlines == 0) {
	  return 0;		/* empty buffer */
     }

     /* check number of lines */
     i=0;
     if (hascolnames)
	  i++;
     if (hasruler)
	  i++;
     if (nlines < i) {
       elog_printf(DIAG, "need %d lines in buffer, only have %d",
		      i, nlines);
	  util_scanfree(parselist);
	  return -1;
     }
     if (nlines == i) {
	  util_scanfree(parselist);
	  return 0;
     }

     /* check the number of buffer columns match ours */
     ncols = 0;
     itree_first(parselist);
     if (hascolnames) {
	  ncols = itree_n(itree_get(parselist));
	  if (t->ncols && ncols != t->ncols) {
	       util_scanfree(parselist);
	       elog_printf(DIAG, "header cols (%d) and table cols "
			   "(%d) do not match", ncols, t->ncols);
	       return -1;
	  }
	  itree_next(parselist);
     }
     if (hasruler) {
	  while ( ! itree_isbeyondend(parselist) ) {
	       row = itree_get(parselist);
	       row_ncols = itree_n(row);
	       if (strncmp(itree_find(row, 0), "--", 2) == 0) {
	       /*if ( *( (char *) itree_find(row, 0) ) == '-' ) {*/
		    itree_next(parselist);
		    break;
	       }
	       if (t->ncols && row_ncols != t->ncols+1) {
		    util_scanfree(parselist);
		    elog_printf(ERROR, "info cols (%d+1) and header "
				"(%d) do not match", row_ncols, t->ncols);
		    return -1;
	       }
	       if (ncols == 0)
		    ncols = row_ncols-1;
	       if (ncols && row_ncols != ncols+1) {
		    util_scanfree(parselist);
		    elog_printf(DIAG, "info cols (%d+1) and header "
				"(%d) do not match", row_ncols, ncols);
		    return -1;
	       }
	       itree_next(parselist);
	  }
     }
     line = 1;
     while ( ! itree_isbeyondend(parselist) ) {
	  row = itree_get(parselist);
	  row_ncols = itree_n(row);
	  if (t->ncols && row_ncols != t->ncols) {
	       elog_printf(DIAG, "scanned text at data line %d has %d "
			   "cols not %d cols expected by table",
			   line, row_ncols, t->ncols);
	       util_scanfree(parselist);
	       return -1;
	  }
	  if (ncols == 0)
	       ncols = row_ncols;
	  if (ncols && row_ncols != ncols) {
	       elog_printf(DIAG, "scanned text at data line %d has %d "
			   "cols not %d cols expected by header",
			   line, row_ncols, ncols);
	       util_scanfree(parselist);
	       return -1;
	  }
	  itree_next(parselist);
	  line++;
     }

     /* check the header names matches our table's */
     itree_first(parselist);
     line = 1;
     if (hascolnames) {
	  colnames = itree_get(parselist);
	  if (t->ncols) {
	       /* check the column headers match the existing ones */
	       /* NOT IMPLEMENTED YET */
	  } else {
	       /* use the column headers to setup this table */
	       t->ncols = ncols;
	       itree_traverse(colnames) {
		    tree_add(t->data, itree_get(colnames), itree_create());
		    itree_append(t->colorder, itree_get(colnames));
	       }
	  }
	  itree_next(parselist);
	  line++;
     } else {
	  if (t->ncols) {
	       /* treat the whole buffer as data */
	       /* NOT IMPLEMENTED YET */
	  } else {
	       /* generate default column headers to match the buffer */
	       t->ncols = ncols;
	       for (i=0; i<ncols; i++) {
		    /* create data to be freed on destruction */
		    sprintf(tmp, "column_%d", i);
		    tmpcpy = xnstrdup(tmp);
		    tree_add(t->data, tmpcpy, itree_create());
		    itree_append(t->colorder, tmpcpy);
		    table_freeondestroy(t, tmpcpy);
	       }
	  }
     }

     /* scan for info lines if a ruler is present */
     if (hasruler) {
	  while (line <= nlines) {
	       row = itree_get(parselist);
	       itree_first(row);
	       if (strncmp(itree_get(itree_get(parselist)), "--", 2) == 0) {
	       /*if (*((char *)itree_get(itree_get(parselist))) == '-') {*/
		    /* ruler found */
		    itree_next(parselist);
		    line++;
		    break;
	       }

	       /* info line - create an empty info row */
	       itree_last(row);
	       iname = itree_get(row);
	       i = table_addemptyinfo(t, iname);

	       /* add to row */
	       itree_first(row);
	       itree_traverse(t->colorder) {
		    table_replaceinfocell(t, iname, itree_get(t->colorder),
					  itree_get(row));
		    itree_next(row);
	       }

	       itree_next(parselist);
	       line++;
	  }
     }

     /* add remaining lines to table */

     /* reset linecounter: this represents the number of non-header
     * lines parsed and should be equal to nrows */
     nlines -= line-1;

     /* by not providing headers we have to follow t->colorder for the 
      * assignment of tokens to columns. */
     if ( ! hascolnames) {
	  colnames = itree_create();
	  tree_traverse(t->colorder)
	       itree_append(colnames, itree_get(t->colorder));
     }

     while( ! itree_isbeyondend(parselist) ) {
	  itree_first(colnames);
	  row = itree_get(parselist);
	  table_addemptyrow(t);
	  itree_traverse(row) {
	       if (table_replacecurrentcell(t, itree_get(colnames), 
					    itree_get(row)) == 0) {
		    /* insertion failed */
		    util_scanfree(parselist);
		    if ( ! hascolnames )
			 itree_destroy(colnames);
		    return -1;
	       }
	       itree_next(colnames);
	  }
	  itree_next(parselist);
     }

     /* clear up and return */
     util_scanfree(parselist);
     if ( ! hascolnames )
	  itree_destroy(colnames);
     return nlines;
}



/*
 * print the whole table, printing the only the columns named in 
 * the list `colnameorder' and in that order
 */
char *table_printcols_t(TABLE t, ITREE *colnameorder)
{
  return table_printselect(t, t->minrowkey, t->maxrowkey, colnameorder);
}


/*
 * print the whole table, printing the only the columns named in 
 * the array `colnameorder' and in that order. The array should be
 * a NULL terminated set of string pointers.
 */
char *table_printcols_a(TABLE t, char **colnameorder)
{
  ITREE *colorder;
  char **pt, *buffer;

  colorder = itree_create();
  pt = colnameorder;
  while (*pt)
    itree_append(colorder, *pt++);

  buffer = table_printcols_t(t, colorder);
  itree_destroy(colorder);

  return buffer;
}


int table_nrows(TABLE t)
{
  return t->nrows;
}


int table_ncols(TABLE t)
{
  return t->ncols;
}


/* 
 * Return a list of column names, with the names to be read as keys 
 * from a TREE. Do not alter the data as it part of the internal state
 * of the TABLE. Free with itree_destroy()
 */
ITREE *table_colnames(TABLE t)
{
  ITREE *colnames;

  colnames = itree_create();
  tree_traverse(t->data)
    itree_append(colnames, tree_getkey(t->data) );

  return colnames;
}


/* 
 * move to first row. traversal friendly.
 * if not followed by a traversal friendly command then the current 
 * position will be unknown
 * understand the concept of rowkey
 */
void table_first(TABLE t)
{
     int datakey;

     if (t->roworder) {
       itree_first(t->roworder);
       datakey = (int) itree_get(t->roworder);
       tree_traverse(t->data)
	    itree_find( (ITREE *) tree_get(t->data), datakey );
     } else {
          tree_traverse(t->data)
	       itree_first( (ITREE *) tree_get(t->data) );
     }
}

void table_next(TABLE t)
{
     int datakey;

     if (t->roworder) {
       itree_next(t->roworder);
       datakey = (int) itree_get(t->roworder);
       tree_traverse(t->data)
	    itree_find( (ITREE *) tree_get(t->data), datakey );
     } else {
          tree_traverse(t->data)
	       itree_next( (ITREE *) tree_get(t->data) );
     }
}

void table_prev(TABLE t)
{
     int datakey;

     if (t->roworder) {
       itree_prev(t->roworder);
       datakey = (int) itree_get(t->roworder);
       tree_traverse(t->data)
	    itree_find( (ITREE *) tree_get(t->data), datakey );
     } else {
          tree_traverse(t->data)
	       itree_prev( (ITREE *) tree_get(t->data) );
     }
}

void table_last(TABLE t)
{
     int datakey;

     if (t->roworder) {
       itree_last(t->roworder);
       datakey = (int) itree_get(t->roworder);
       tree_traverse(t->data)
	    itree_find( (ITREE *) tree_get(t->data), datakey );
     } else {
          tree_traverse(t->data)
	       itree_last( (ITREE *) tree_get(t->data) );
     }
}

void table_gotorow(TABLE t, int rowkey)
{
     tree_traverse(t->data) {
	  if ( itree_find(tree_get(t->data), rowkey) == ITREE_NOVAL)
	       elog_printf(ERROR, "cant find row key %d in col %s", 
			   rowkey, tree_getkey(t->data));
     }
}

int table_isatfirst(TABLE t)
{
  ITREE *column;

  if (t->roworder) {
       column = t->roworder;
  } else {
       tree_first(t->data);
       column = tree_get(t->data);
  }

  return itree_isatstart(column);
}

/* Is the current position at the end of the table.
 * Also returns true if the table is empty */
int table_isatlast(TABLE t)
{
  ITREE *column;

  if (t->ncols <= 0 || t->nrows <= 0)
       return 1;

  if (t->roworder) {
       column = t->roworder;
  } else {
       tree_first(t->data);
       column = tree_get(t->data);
  }

  return itree_isatend(column);
}

/* Is the current position beyond the end of the table.
 * Also returns true if the table is empty */
int table_isbeyondend(TABLE t)
{
  ITREE *column;

  if (t->ncols <= 0 || t->nrows <= 0)
       return 1;

  if (t->roworder) {
       column = t->roworder;
  } else {
       tree_first(t->data);
       column = tree_get(t->data);
  }

  return itree_isbeyondend(column);
}

/* 
 * Return the current row. Cells are returned indexed by their column names.
 * Free the returned tree with tree_destroy(), as the key and cell data
 * are NOT copies.
 */
TREE *table_getcurrentrow(TABLE t)
{
  TREE *row;

  row = tree_create();
  tree_traverse(t->data)
    tree_add( row, tree_getkey(t->data), itree_get(tree_get(t->data)) );

  return row;
}


/*
 * Remove the row at the current row position and advance to the next row
 */
void table_rmcurrentrow(TABLE t)
{
     ITREE *column;
     int rowkey = 0;
     int is_beyond = 0;

     if (t->nrows == 0)
	  return;	/* nothing to remove */

     tree_traverse(t->data)
	  itree_rm( tree_get(t->data) );

     /* handle emptyness */
     t->nrows--;
     if (t->nrows == 0) {
	  /* empty */
	  t->minrowkey = t->maxrowkey = -1;
     } else {
	  /* save row location whilst checking for 'beyondness' */
	  tree_first(t->data);
	  column = tree_get(t->data);
	  if (itree_isbeyondend(column))
	       is_beyond++;
	  else
	       rowkey = itree_getkey(column);

	  /* update counter, first and last row indexes */
	  itree_first(column);
	  t->minrowkey = itree_getkey(column);
	  itree_last(column);
	  t->maxrowkey = itree_getkey(column);

	  /* restore row location */
	  if (is_beyond) {
	       itree_last(column);
	       itree_next(column);
	  } else
	       itree_find(column, rowkey);
     }
}


/* 
 * return the location of a named cell in the current row. 
 * the returned data is not a copy, so dont free it or overwrite it.
 */
void *table_getcurrentcell(TABLE t, char *colname)
{
  ITREE *column;

  column = tree_find(t->data, colname);
  if (column != TREE_NOVAL)
    return (  itree_get(column)  );
  else
    return NULL;
}

/* returns the row key of the current row */
int table_getcurrentrowkey(TABLE t)
{
  tree_first(t->data);
  return itree_getkey( tree_get(t->data) );
}

/*
 * Replace cell in the current row with newcelldata. No data copies 
 * are made and the data must continue to exist as long as the table 
 * does (or this cell).  Old data is left alone.
 * Returns 1 if successful or 0 if unable to find cell
 */
int table_replacecurrentcell(TABLE t, char *colname, void *newcelldata)
{
  ITREE *column;

  column = tree_find(t->data, colname);
  if (column == TREE_NOVAL) 
    return 0;

  itree_put(column, newcelldata);

  return 1;
}

/*
 * Replace cell in the current row with a copy of newcelldata, which will
 * be freed automatically when the table is destroyed.
 * The old data allocation is not destroyed.
 * Returns 1 if successful or 0 if unable to find cell
 */
int table_replacecurrentcell_alloc(TABLE t, char *colname, void *newcelldata)
{
     ITREE *column;
     char *dupdata;

     if (newcelldata == NULL)
	  return table_replacecurrentcell(t, colname, newcelldata);

     column = tree_find(t->data, colname);
     if (column == TREE_NOVAL) 
	  return 0;

     dupdata = xnstrdup(newcelldata);
     table_freeondestroy(t, dupdata);
     itree_put(column, dupdata);

     return 1;
}

/*
 * Note on info:-
 * Info is a set of special data rows that is treated as part of the header.
 * Info rows are keyed by a string in their own namespace and cells are
 * additionally keyed by the column name.
 * Infos are not normally printed out unless requested. 
 * table_outtable() does print info lines.
 */

/* 
 * Add or replace the info cell indexed by column colname and row infoname.
 * Value should be valid for the life of the table: consider ..freeondestroy().
 * If column does not exist in the table, then the call fails and the 
 * column remains uncreated.
 * Returns 1 for success or 0 for failure
 */
int table_replaceinfocell(TABLE t, char *infoname, char *colname, void *value)
{
     ITREE *col;
     int infoindex;

     /* reject if the column and row do not exist */
     if ( tree_find(t->data, colname) == TREE_NOVAL )
	  return 0;
     if ( tree_find(t->infolookup, infoname) == TREE_NOVAL )
	  return 0;

     /* get column and row indexes/references */
     infoindex = (int) tree_find(t->infolookup, infoname);
     col = tree_find(t->info, colname);
     if (col == TREE_NOVAL) {
	  /* if column does not exist, make it */
	  col = itree_create();
	  tree_add(t->info, tree_getkey(t->data), col);
     }

     /* now add or replace data */
     if (itree_find(col, infoindex) != ITREE_NOVAL)
	  itree_put(col, value);
     else
	  itree_add(col, infoindex, value);

     return 1;
}


/* 
 * Add an empty row to the info section.
 * The info name is not copied and should exist externally; 
 * use table_freeondestroy() to coordinate if needed.
 * Returns the index of the info row is successful or 0 if the name has 
 * already has been added.
 */
int table_addemptyinfo(TABLE t, char *infoname)
{
     int i;

     if (tree_find(t->infolookup, infoname) != TREE_NOVAL)
	  return 0;		/* name already exists */

     /* find higest index in use */
     i=0;
     tree_traverse(t->infolookup) {
	  if ((int) tree_get(t->infolookup) > i)
	       i = (int) tree_get(t->infolookup);
     }

     /* set minimum index (no info rows exist) or next index */
     if (i < 1)
	  i = 1;
     else
	  i++;

     /* now add */
     tree_add(t->infolookup, infoname, (void *) i);

     return i;
}


/*
 * Add one line of info to the table.
 * Inforow contains a list of key value pairs that index column names.
 * If the info name does NOT exist, then it will be created.
 * If the info name DOES exist, then the values contain in inforow will 
 * replace existing data.
 * If a column name in inforow does not exist, then that data is ignored:
 * no additional columns will be produced, but the rest of the list is 
 * processed.
 * Returns 0 for failure or 1 for success.
 */
int   table_addinfo_t(TABLE t, char *infoname, TREE *inforow)
{
     /* make sure row has been created and get the index */
     table_addemptyinfo(t, infoname);

     /* add row to info data */
     tree_traverse(inforow) {
	  table_replaceinfocell(t, infoname, tree_getkey(inforow), 
				tree_get(inforow));
     }

     return 1;
}

/*
 * Add one line of info to the table.
 * Inforow contains a list of values in order `t->colorder'.
 * If the info name does not exist, then it will be created.
 * If the info name does exist, then the values contained in inforow will 
 * replace existing data.
 * The list may be shorted or longer than the number of columns, in which
 * case some info cells will be blank or trailing part of the list is ignored.
 * Returns 0 for failure or 1 for success.
 */
int   table_addinfo_it(TABLE t, char *infoname, ITREE *inforow)
{
     /* make sure row has been created and get the index */
     table_addemptyinfo(t, infoname);

     /* add row to info data */
     itree_first(inforow);
     itree_traverse(t->colorder) {
	  /* replace each cell */
	  table_replaceinfocell(t, infoname, itree_get(t->colorder),
				itree_get(inforow));

	  /* iterate over inforow */
	  if (itree_isatend(inforow))
	       break;
	  itree_next(inforow);
     }

     return 1;
}

/*
 * Remove an info row of data. 
 * Return 1 for success or 0 if info does not exist
 */
int  table_rminfo(TABLE t, char *infoname)
{
     void *infoptr;
     int infoindex;

     infoptr = tree_find(t->infolookup, infoname);
     if (infoptr == TREE_NOVAL)
	  return 0;
     infoindex = (int) infoptr;

     /* remove the info from the columns */
     tree_traverse(t->info) {
	  /* remove the list entry and data */
	  if (itree_find( tree_get(t->info), infoindex ) != ITREE_NOVAL)
	       itree_rm( tree_get(t->info) );

#if 0
	  /* dont bother removing the column table */

	  /* remove the column list if empty */
	  if (itree_n( tree_get(t->info) ) == 0) {
	       itree_destroy( tree_get(t->info) );
	       tree_rm(t->info);
	  }
#endif
     }

     /* remove the info from the lookup */
     tree_rm(t->infolookup);

     return 1;
}

/*
 * Get a row of info data from the info line infoname.
 * Returns a list of data items indexed by column name or NULL if the
 * info name does not exist.
 * The list should be free'ed with tree_destroy() and the the data left intact.
 */
TREE *table_getinforow(TABLE t, char *infoname)
{
     void *infoptr;
     int infoindex;
     TREE *retrow;
     char *datum;

     /* look up info */
     infoptr = tree_find(t->infolookup, infoname);
     if (infoptr == TREE_NOVAL)
	  return NULL;
     infoindex = (int) infoptr;

     /* extract the list */
     retrow = tree_create();
     tree_traverse(t->info) {
	  datum = itree_find(tree_get(t->info), infoindex);
	  if (datum != ITREE_NOVAL)
	       tree_add(retrow, tree_getkey(t->info), datum);
     }

     return retrow;
}

/*
 * Get a column of info data.
 * Returns a list of data items all related to one column and indexed
 * by info name.
 * The list should be freed with tree_destroy() and the the data left intact.
 */
TREE *table_getinfocol(TABLE t, char *colname)
{
     TREE *retcol;

     if (tree_find(t->data, colname) == TREE_NOVAL)
	  return NULL;
     retcol = tree_create();
     if (tree_find(t->info, colname) != TREE_NOVAL)
	  tree_traverse(t->infolookup)
	       if (itree_find(tree_get(t->info), 
			      (int) tree_get(t->infolookup)) != ITREE_NOVAL)
		    tree_add(retcol, tree_getkey(t->infolookup), 
			     itree_get(tree_get(t->info)));

     return retcol;
}


/*
 * Get a cell of info data, given the info row name and the column name.
 * Returns a pointer to the cell, which should not be freed as it is in
 * use, or NULL of the cell does not exist or has not been set.
 */
char *table_getinfocell(TABLE t, char *infoname, char *colname)
{
     void *infoptr;
     int infoindex;
     ITREE *col;
     char *datum;

     /* look up info */
     infoptr = tree_find(t->infolookup, infoname);
     if (infoptr == TREE_NOVAL)
	  return NULL;
     infoindex = (int) infoptr;

     /* look up column */
     col = tree_find(t->info, colname);
     if (col == TREE_NOVAL)
	  return NULL;

     /* find the cell */
     datum = itree_find(col, infoindex);
     if (datum == ITREE_NOVAL)
	  return NULL;
     else 
	  return datum;
}


/* Check the internal consistency of the table. Returns 1 for ok, 0 for bad */
int   table_check(TABLE t)
{
     /* check there are enough columns */
     if (tree_n(t->data) != t->ncols) {
	  elog_printf(DEBUG, "column mismatch: ncols=%d != data cols=%d",
		      t->ncols, tree_n(t->data));
	  return 0;
     }
     /* check that the columns have equal rows */
     tree_traverse(t->data) {
	  if (t->nrows != itree_n(tree_get(t->data))) {
	       elog_printf(DEBUG, "row mismatch: column=%s, nrows=%d != data "
			   "rows=%d",
			   tree_getkey(t->data), t->nrows,
			   tree_n(tree_get(t->data)));
	       return 0;
	  }
     }

     return 1;
}

/*
 * Return the names of the info lines in a TREE, where the names are held
 * in the keys. The table is part of the internal workings of TABLE and 
 * should not be altered.
 */
TREE *table_getinfonames(TABLE t)
{
     return t->infolookup;
}


/*
 * Add row order to table. On destruction, the input list (ITREE) 
 * roworder will be destroyed.
 * Replaces any list that was there before, which is destroyed during
 * this method.
 * Roworder should be an ordered list of row indexes, with the row index
 * cast to a void* and held in the list value.
 */
void   table_addroworder(TABLE t, ITREE *roworder)
{
     /* check parameters */
     if (t == NULL)
	  elog_die(FATAL, "t==NULL");
     if (roworder == NULL)
	  return;

     if (t->roworder)
	  itree_destroy(t->roworder);
     t->roworder = roworder;
}




/*
 * Add row order to table. A private copy of roworder is made and the 
 * user continues to be responsible for his own list.
 * Replaces any list that was there before.
 * As table_roworder() above, but uses a TREE to order the list (using 
 * string keys). The row indexes are cast to void* and placed in the
 * value of each list entry (naughty, I know).
 */
void   table_addroworder_t(TABLE t, TREE *roworder)
{
     /* check parameters */
     if (t == NULL)
	  elog_die(FATAL, "t==NULL");
     if (roworder == NULL)
	  return;

     if (t->roworder)
	  itree_destroy(t->roworder);

     /* copy roworder to an itree inernally */
     t->roworder = itree_create();
     tree_traverse(roworder)
	  itree_append(t->roworder, tree_get(roworder));
}



/*
 * Sort the table by ASCII order using primary and secondary keys, 
 * attaching the row order to the table. 
 * The sorted data may only be accessable using specific methods, 
 * such as table_getsortedcol().
 * If there are any additions, the order will be invalid and this
 * method should be run again.
 * If the values are numeric, use table_sortnumeric() instead.
 * Returns 1 for success or 0 for failure or no work carried out.
 */
/* SECONDARY KEY NOT CURRENTLY SUPPORTED */
int   table_sort(TABLE t, char *primarykey, char *secondarykey)
{
     TREE *order;
     ITREE *col;

     /* check input parameters */
     if (primarykey == NULL)
	  return 0;

     /* locate primary key column */
     col = tree_find(t->data, primarykey);
     if (col == TREE_NOVAL)
	  return 0;	/* failure */

     /* order */
     order = tree_create();
     itree_traverse(col)
	  tree_add(order, itree_get(col), (void*) itree_getkey(col));

     /* attach row order and clear up */
     table_addroworder_t(t, order);
     tree_destroy(order);

     return 1;		/* success */
}


/*
 * Sort the table by numeric order using primary and secondary keys, 
 * attaching the row order to the table. 
 * The sorted data may only be accessable using specific methods, 
 * such as table_getsortedcol().
 * If there are any additions, the order will be invalid and this
 * method should be run again.
 * If you have ASCII to sort, use table_sortascii().
 * Returns 1 for success or 0 for failure or no work carried out.
 */
/* SECONDARY KEY NOT CURRENTLY SUPPORTED */
int   table_sortnumeric(TABLE t, char *primarykey, char *secondarykey)
{
     ITREE *iorder, *col;

     /* check input parameters */
     if (primarykey == NULL)
	  return 0;

     /* locate primary key column */
     col = tree_find(t->data, primarykey);
     if (col == TREE_NOVAL)
	  return 0;	/* failure */

     /* order */
     iorder = itree_create();
     itree_traverse(col)
          itree_add(iorder, strtol((char *) itree_get(col), NULL, 10), 
		    (void*) itree_getkey(col));

#if 0
     itree_traverse(iorder)
       printf("key %d-row %d\n", itree_getkey(iorder), (int)itree_get(iorder));
#endif

     /* attach row order, addorder will reparent the list */
     table_addroworder(t, iorder);

     return 1;		/* success */
}


/* Returns 1 if column exists in table, 0 otherwise */
int    table_hascol(TABLE t, char *colname) {
     if (tree_find(t->data, colname) == TREE_NOVAL)
	  return 0;
     else
	  return 1;
}

/*
 * Scan the table to find unique values within colname and 
 * return the values as a list (values are list keys, val in list is
 * NULL). Values are not copies and refer to values in the passed table.
 * Uniq should be either NULL, the address of a NULL or the address 
 * of a pointer to a TREE. If not null, the addressed TREE list 
 * will be used, adding to values already there; if NULL is pointer to, 
 * a tree will be created and the address written in place.
 * If NULL is passed as uniq, the tree will be returned.
 * Meant to be used to find unique values for key columns.
 * Returns the list on success and sets it in uniq (if non-NULL) as output. 
 * On failure NULL is returned and uniq is not altered.
 * To free the returned tree list when uniq==NULL, just use tree_destroy();
 * if uniq!=NULL, then unix will be returned, so there is no additional data
 * to manage (or free).
 */
TREE *table_uniqcolvals(TABLE t, char *colname, TREE **uniq)
{
     ITREE *col;
     TREE *myuniq=NULL;

     if (t == NULL || colname == NULL || *colname == '\0')
	  return NULL;

     col = table_getcol(t, colname);
     if (itree_n(col) > 0) {
	  if (uniq == NULL || *uniq == NULL)
	       myuniq = tree_create();
	  else
	       myuniq = *uniq;
     }
     itree_traverse(col) {
	  if (tree_find(myuniq, itree_get(col)) == TREE_NOVAL)
	       tree_add(myuniq, itree_get(col), NULL);
     }
     itree_destroy(col);
     if (myuniq && uniq && *uniq == NULL)
	  *uniq = myuniq;

     return myuniq;
}



/*
 * Select out values from the columns in the list datacol,
 * that share rows with the specified key. The value is in key, 
 * taken from the column keycol. This is equivalent to
 * the SQL command 'select <contents of datacols> from t where keycol=k'.
 * If datacols==NULL, then all columns are extracted.
 * It returns a TABLE with the subset of columns and data if successful
 * or NULL otherwise. The TABLE will not have the info columns present 
 * in the parent.
 * The returned table uses the parent data for data. if the parent is 
 * destroyed, you will not be able to use this. 
 * Please free with table_destroy() as normal
 */
TABLE table_selectcolswithkey(TABLE t, 		/* table */
			      char *keycol,	/* column containing key */
			      char *key,	/* value of key*/
			      ITREE *datacols	/* list of data columns */ )
{
     TABLE out;
     ITREE *cols;

     if (datacols)
	  cols = datacols;
     else
	  cols = table_getcolorder(t);

     out = table_create_t(cols);

     /* we can only do this with a table scan...*/
     table_traverse(t) {
	  if (strcmp(table_getcurrentcell(t, keycol), key) == 0) {
	       /* key match: extract the cells from this row */
	       table_addemptyrow(out);
	       itree_traverse(cols) {
		    table_replacecurrentcell(out, itree_get(cols), 
					     table_getcurrentcell(
						  t, itree_get(cols)));
	       }
	  }
     }

     /* remove table if we added nothing */
     if (table_nrows(out) == 0) {
	  table_destroy(out);
	  out = NULL;
     }

     return out;
}



/*
 * Compare tables returning 1 if equal or 0 otherwise
 */
int    table_equals(TABLE t, TABLE other)
{
  ITREE *mydata, *otherdata, *myinfo, *otherinfo;
  int myidx, otheridx;

     /* compare info row name quantity */
     if (tree_n(t->infolookup) != tree_n(other->infolookup))
          return 0;

     /* compare info row names */
     tree_first(other->infolookup);
     tree_traverse(t->infolookup) {
          if (strcmp(tree_getkey(t->infolookup), 
		     tree_getkey(other->infolookup)))
	       return 0;
	  tree_next(other->infolookup);
     }

     /* compare column quantity */
     if (itree_n(t->colorder) != itree_n(other->colorder))
          return 0;

     /* compare column names */
     itree_first(other->colorder);
     itree_traverse(t->colorder) {
          if (strcmp(itree_get(t->colorder), itree_get(other->colorder)))
	       return 0;

	  /* get info columns */
	  if ( (myinfo    = tree_find(t->info, 
				      itree_get(t->colorder))) == TREE_NOVAL)
	       return 0;
	  if ( (otherinfo = tree_find(other->info, 
				      itree_get(other->colorder))) 
	                                                       == TREE_NOVAL)
	       return 0;

	  /* compare info cols */
	  tree_first(other->infolookup);
	  tree_traverse(t->infolookup) {
	       myidx    = (int) tree_get(t->infolookup);
	       otheridx = (int) tree_get(other->infolookup);
	       if (itree_find(myinfo, myidx) == ITREE_NOVAL)
		    return 0;

	       if (strcmp(itree_get(myinfo), itree_find(otherinfo, otheridx)))
		    return 0;
	       tree_next(other->infolookup);
	  }


	  /* get data columns */
	  if ( (mydata    = tree_find(t->data, 
				      itree_get(t->colorder))) == TREE_NOVAL)
	       return 0;
	  if ( (otherdata = tree_find(other->data, 
				      itree_get(other->colorder))) 
	                                                       == TREE_NOVAL)
	       return 0;
	  
	  /* compare data column quantity */
	  if (itree_n(mydata) != itree_n(otherdata))
	       return 0;
	  
	  /* compare data columns */
	  itree_first(otherdata);
	  itree_traverse(mydata) {
	       if (strcmp(itree_get(mydata), itree_get(otherdata)))
		    return 0;
	       itree_next(otherdata);
	  }
	  
	  itree_next(other->colorder);
     }


  return 1;
}





#if TEST

#include <stdlib.h>
#include "route.h"
#include "rt_std.h"

#define TEST_TEXT1 "one two three"
#define TEST_TEXT2 "c1 c2 c3\none two three\n"
#define TEST_TEXT3 "c1 c2 c3\n-- -- --\none two three\n"
#define TEST_TEXT4 "c1\tc2\tc3\nint\tnano\tfloat\ttypes\n--\t--\t--\none\ttwo\tthree\n"
#define TEST_TEXT5 "c1\tc2\tc3\nint\tnano\tfloat\ttypes\nfirst column\tsecond column\tcolumn number three\thelp\n--\t--\t--\none\ttwo\tthree\n"

int main(int argc, char **argv)
{
     TABLE tab1, tab2, tab3;
     ITREE *setupcolnames, *col1;
     TREE *setuprow1, *inforow1, *row1;
     int r, i;
     char *cell1, *buf1, *buf2, *buf3, *buf4;

     route_init(NULL, 0);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     elog_init(0, "holstore test", NULL);

     /* test 1: create and destroy */
     setupcolnames = itree_create();
     itree_append(setupcolnames, "tom");
     itree_append(setupcolnames, "dick");
     itree_append(setupcolnames, "harry");
     tab1 = table_create_t(setupcolnames);
     if ( ! tab1 )
       elog_die(FATAL, "[1] unable to create tab1");
     table_destroy(tab1);

     /* test 2: create and add a row */
     setuprow1 = tree_create();
     tree_add(setuprow1, "tom", "one");
     tree_add(setuprow1, "dick", "two");
     tree_add(setuprow1, "harry", "three");
     tab1 = table_create_t(setupcolnames);
     if ( ! tab1 )
       elog_die(FATAL, "[2] unable to create tab1");
     r = table_addrow_noalloc(tab1, setuprow1);
     if (r == -1)
       elog_die(FATAL, "[2] unable add row");
     table_destroy(tab1);

     /* test 3: add several rows */
     tab1 = table_create_t(setupcolnames);
     if ( ! tab1 )
       elog_die(FATAL, "[3] unable to create tab1");
     r = table_addrow_noalloc(tab1, setuprow1);
     if (r == -1)
       elog_die(FATAL, "[3a] unable add row");
     r = table_addrow_noalloc(tab1, setuprow1);
     if (r == -1)
       elog_die(FATAL, "[3b] unable add row");
     r = table_addrow_noalloc(tab1, setuprow1);
     if (r == -1)
       elog_die(FATAL, "[3c] unable add row");
     table_destroy(tab1);

     /* test 4: add several rows to test verious extraction methods */
     tab1 = table_create_t(setupcolnames);
     if ( ! tab1 )
       elog_die(FATAL, "[4] unable to create tab1");
     r = table_addrow_noalloc(tab1, setuprow1);
     if (r == -1)
       elog_die(FATAL, "[4a] unable add row");
     r = table_addrow_noalloc(tab1, setuprow1);
     if (r == -1)
       elog_die(FATAL, "[4b] unable add row");
     r = table_addrow_noalloc(tab1, setuprow1);
     if (r == -1)
       elog_die(FATAL, "[4c] unable add row");

     /* test 5: get row */
     row1 = table_getrow(tab1, 1);
     if ( ! row1 )
       elog_die(FATAL, "[5] unable to get row");
     if ( (cell1 = tree_find(row1, "tom")) == TREE_NOVAL )
       elog_die(FATAL, "[5] unable find tom in row");
     if (strcmp(cell1, "one"))
       elog_die(FATAL, "[5] tom != one in row");
     if ( (cell1 = tree_find(row1, "dick")) == TREE_NOVAL )
       elog_die(FATAL, "[5] unable find dick in row");
     if (strcmp(cell1, "two"))
       elog_die(FATAL, "[5] dick != two in row");
     if ( (cell1 = tree_find(row1, "harry")) == TREE_NOVAL )
       elog_die(FATAL, "[5] unable find harry in row");
     if (strcmp(cell1, "three"))
       elog_die(FATAL, "[5] harry != three in row");
     if ( tree_find(row1, "sploge") != TREE_NOVAL)
       elog_die(FATAL, "[5] shouldnt find sploge in row");
     tree_destroy(row1);

     /* test 6: get column */
     col1 = table_getcol(tab1, "harry");
     if ( ! col1 )
       elog_die(FATAL, "[6] unable to get col");
     if ( (cell1 = itree_find(col1, 0)) == ITREE_NOVAL )
       elog_die(FATAL, "[6] unable find 0 in col");
     if (strcmp(cell1, "three"))
       elog_die(FATAL, "[6] 0 != three in col");
     if ( (cell1 = itree_find(col1, 1)) == ITREE_NOVAL )
       elog_die(FATAL, "[6] unable find 1 in col");
     if (strcmp(cell1, "three"))
       elog_die(FATAL, "[6] 1 != three in col");
     if ( (cell1 = itree_find(col1, 2)) == ITREE_NOVAL )
       elog_die(FATAL, "[6] unable find 2 in col");
     if (strcmp(cell1, "three"))
       elog_die(FATAL, "[6] 2 != three in col");
     if ( itree_find(col1, 99) != ITREE_NOVAL )
       elog_die(FATAL, "[6] shouldnt find 99 in row");
     itree_destroy(col1);

     /* test 7: iterative/sequential extraction */
     table_first(tab1);
     i=0;
     while ( ! table_isbeyondend(tab1) ) {
       row1 = table_getcurrentrow(tab1);
       if ( ! row1 )
	 elog_die(FATAL, "[7] unable to get row %d", i);
       if ( (cell1 = tree_find(row1, "tom")) == TREE_NOVAL )
	 elog_die(FATAL, "[7] unable find tom in row %d", i);
       if (strcmp(cell1, "one"))
	 elog_die(FATAL, "[7] tom != one in row %d", i);
       if ( (cell1 = tree_find(row1, "dick")) == TREE_NOVAL )
	 elog_die(FATAL, "[7] unable find dick in row %d", i);
       if (strcmp(cell1, "two"))
	 elog_die(FATAL, "[7] dick != two in row %d", i);
       if ( (cell1 = tree_find(row1, "harry")) == TREE_NOVAL )
	 elog_die(FATAL, "[7] unable find harry in row %d", i);
       if (strcmp(cell1, "three"))
	 elog_die(FATAL, "[7] harry != three in row %d", i);
       if ( tree_find(row1, "sploge") != TREE_NOVAL )
	 elog_die(FATAL, "[7] shouldnt find sploge in row %d", i);
       tree_destroy(row1);
       i++;
       table_next(tab1);
     }
     if (i != 3)
       elog_die(FATAL, "[7] there are not three rows");

     /* test 8: create and empty table and add a couple of columns */
     tab2 = table_create();
     col1 = itree_create();
     itree_append(col1, "one");
     itree_append(col1, "one");
     itree_append(col1, "one");
     r = table_addcol(tab2, "tom", col1);
     itree_destroy(col1);
     col1 = itree_create();
     itree_append(col1, "two");
     itree_append(col1, "two");
     itree_append(col1, "two");
     r = table_addcol(tab2, "dick", col1);
     itree_destroy(col1);
     col1 = itree_create();
     itree_append(col1, "three");
     itree_append(col1, "three");
     itree_append(col1, "three");
     r = table_addcol(tab2, "harry", col1);
     itree_destroy(col1);

     /* test 9: print a table, but then save it as a tab delim format */
     buf1 = table_print(tab1);
     printf("Test 9 table:-\n%s", buf1);
     nfree(buf1);
     buf1 = table_outtable(tab1);

     /* test 10: table traversing */
     i=0;
     table_traverse(tab1)
          i++;
     if (i != 3)
       elog_die(FATAL, "[10] there are not three rows");

     /* test 11: add info */
     inforow1 = tree_create();
     tree_add(inforow1, "tom", "thomas");
     tree_add(inforow1, "dick", "richard");
     tree_add(inforow1, "harry", "henry");
     r = table_addinfo_t(tab1, "real names", inforow1);
     if (r == 0)
	  elog_die(FATAL, "[11a] can't addinfo_t()");
     r = table_addemptyinfo(tab1, "blank line");
     if (r != 2)
	  elog_die(FATAL, "[11b] can't addemptyinfo() again");
     r = table_rminfo(tab1, "not here");
     if (r != 0)
	  elog_die(FATAL, "[11c] shouldn't be able to delete");
     r = table_rminfo(tab1, "real names");
     if (r != 1)
	  elog_die(FATAL, "[11d] not able to delete real names");
     r = table_rminfo(tab1, "blank line");
     if (r != 1)
	  elog_die(FATAL, "[11e] not able to delete bank line");

     /* test 12: scan a text representation of a table */
     tab3 = table_create();
     r = table_scan(tab3, buf1, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 3)
	  elog_die(FATAL, "[12a] does not scan 3 lines");
     table_freeondestroy(tab3, buf1);
     buf2 = table_print(tab3);
     printf("Test 12 table (same as 9):-\n%s", buf2);
     nfree(buf2);
     buf2 = table_outtable(tab3);
     r = table_scan(tab3, buf2, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 3)
	  elog_die(FATAL, "[12b] does not scan 3 lines");
     table_freeondestroy(tab3, buf2);
     buf2 = table_print(tab3);
     buf3 = buf2 -1;
     i=0;
     while ((buf3=strchr(buf3+1, '\n')))	/* count \n */
	  i++;
     if (i != 8)
	  elog_die(FATAL, "[12c] has not output 6 lines");
     nfree(buf2);
     table_destroy(tab2);

     /* test 13: outheader, outbody and outtable for scanning and escaping */
     buf3 = table_outtable(tab3);
     tab2 = table_create_t(setupcolnames);
     r = table_scan(tab2, buf3, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 6)
	  elog_die(FATAL, "[13a] does not scan 6 lines");
     table_freeondestroy(tab2, buf3);
     tree_destroy(setuprow1);
     setuprow1 = tree_create();
     tree_add(setuprow1, "tom", "thomas a becket");
     tree_add(setuprow1, "dick", "richard the III");
     tree_add(setuprow1, "harry", "henry the VIII");
     r = table_addrow_noalloc(tab2, setuprow1);
     if (r == -1)
       elog_die(FATAL, "[13a] unable add row");
     tree_destroy(setuprow1);
     setuprow1 = tree_create();
/*     tree_add(setuprow1, "tom", "some words with \" that in");*/
     tree_add(setuprow1, "tom", "some loose words");
     tree_add(setuprow1, "dick", "");
     tree_add(setuprow1, "harry", "the last one was empty");
     r = table_addrow_noalloc(tab2, setuprow1);
     if (r == -1)
       elog_die(FATAL, "[13b] unable add row");
     buf3 = table_outtable(tab2);
     r = table_scan(tab2, buf3, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 8)
	  elog_die(FATAL, "[13b] does not scan 8 lines");
     table_freeondestroy(tab2, buf3);
     buf3 = table_outtable(tab2);
     buf2 = buf3 -1;
     i=0;
     while ((buf2=strchr(buf2+1, '\n')))	/* count \n */
	  i++;
     if (i != 18)
	  elog_die(FATAL, "[13b] has not output 18 lines");

     /* remove lists and tables */
     nfree(buf3);
     table_destroy(tab1);
     table_destroy(tab2);
     table_destroy(tab3);
     itree_destroy(setupcolnames);
     tree_destroy(setuprow1);
     tree_destroy(inforow1);

     /* test 14: scan a text buffer with out column headers into a table */
     tab1 = table_create();
     tab2 = table_create();
     buf3 = xnstrdup(TEST_TEXT1);
     r = table_scan(tab1, buf3, "\t", TABLE_SINGLESEP, TABLE_NOCOLNAMES, 
		    TABLE_NORULER);
     if (r != 1)
	  elog_die(FATAL, "[14a] does not scan 1 line");
     buf1 = table_print(tab1);
     buf4 = xnstrdup(buf1);
     r = table_scan(tab2, buf4, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[14b] does not scan 1 line");
     buf2 = table_print(tab2);
     if (strcmp(buf1, buf2) != 0)
	  elog_die(FATAL, "[14] buffer mismatch");
     nfree(buf1);
     nfree(buf2);
     nfree(buf3);
     nfree(buf4);
     table_destroy(tab1);
     table_destroy(tab2);

     /* test 15: scan a text buffer with column headers into a table */
     tab1 = table_create();
     tab2 = table_create();
     buf3 = xnstrdup(TEST_TEXT2);
     r = table_scan(tab1, buf3, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_NORULER);
     if (r != 1)
	  elog_die(FATAL, "[15a] does not scan 1 line");
     buf1 = table_outtable(tab1);
     buf4 = xnstrdup(buf1);
     r = table_scan(tab2, buf4, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[15b] does not scan 1 line");
     buf2 = table_outtable(tab2);
     if (strcmp(buf1, buf2) != 0)
	  elog_die(FATAL, "[15] buffer mismatch");
     nfree(buf1);
     nfree(buf2);
     nfree(buf3);
     nfree(buf4);
     table_destroy(tab1);
     table_destroy(tab2);

     /* test 16: scan a text buffer with column headers and ruler into 
      * a table */
     tab1 = table_create();
     tab2 = table_create();
     buf3 = xnstrdup(TEST_TEXT3);
     r = table_scan(tab1, buf3, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[16a] does not scan 1 line");
     buf1 = table_outtable(tab1);
     buf4 = xnstrdup(buf1);
     r = table_scan(tab2, buf4, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[16b] does not scan 1 line");
     buf2 = table_outtable(tab2);
     if (strcmp(buf1, buf2) != 0)
	  elog_die(FATAL, "[16] buffer mismatch");
     nfree(buf1);
     nfree(buf2);
     nfree(buf3);
     nfree(buf4);
     table_destroy(tab1);
     table_destroy(tab2);

     /* test 17: scan a text buffer with column headers, ruler and 
      * an info line into a table */
     tab1 = table_create();
     tab2 = table_create();
     buf3 = xnstrdup(TEST_TEXT4);
     r = table_scan(tab1, buf3, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[17a] does not scan 1 line");
     buf1 = table_outtable(tab1);
     buf4 = xnstrdup(buf1);
     r = table_scan(tab2, buf4, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[17b] does not scan 1 line");
     buf2 = table_outtable(tab2);
     if (strcmp(buf1, buf2) != 0)
	  elog_die(FATAL, "[17] buffer mismatch");
     nfree(buf1);
     nfree(buf2);
     nfree(buf3);
     nfree(buf4);
     table_destroy(tab1);
     table_destroy(tab2);

     /* test 18: scan a text buffer with column headers, ruler and 
      * two info lines (one with spaces) into a table */
     tab1 = table_create();
     tab2 = table_create();
     buf3 = xnstrdup(TEST_TEXT5);
     r = table_scan(tab1, buf3, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[18a] does not scan 1 line");
     buf1 = table_outtable(tab1);
     buf4 = xnstrdup(buf1);
     r = table_scan(tab2, buf4, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		    TABLE_HASRULER);
     if (r != 1)
	  elog_die(FATAL, "[18b] does not scan 1 line");
     buf2 = table_outtable(tab2);
     if (strcmp(buf1, buf2) != 0)
	  elog_die(FATAL, "[18] buffer mismatch");
     nfree(buf1);
     nfree(buf2);
     nfree(buf3);
     nfree(buf4);
     table_destroy(tab1);
     table_destroy(tab2);

     /* shutdown and exit */
     elog_fini();
     route_fini();

     printf("tests finished successfully\n");
     exit(0);
}

#endif /* TEST */
