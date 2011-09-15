/*
 * Table set manipulation class
 * Used to make stateful changes on TABLE data so that the TABLE is unchanged.
 * Loosely follows the verbs from a SQL statement. How to use:-
 * Stage 1, initialise with a data source using tableset_create().
 * Stage 2, (optional) select the columns required using 
 *          tableset_select() / tableset_selectt() or exclude columns using 
 *          tableset_exclude() / tabset_excludet(). If _select is not used, 
 *          the default is all columns.
 * Stage 3, (optional) filter rows in or out using tableset_where() and
 *          tableset_unless(). The conditions are run in the call order
 *          and are AND'ed together.
 * Stage 4, (optional) group rows. Not yet implemeted
 * Stage 5, (optional) sort the accumulated rows with tableset_sortby()
 * Stage 6, use the final data with tableset_into() to save in a new table,
 *          tableset_print() to format to a string.
 *
 * Nigel Stuckey, November 2003, June 2004, July 2009
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "table.h"
#include "tableset.h"
#include "nmalloc.h"
#include "strbuf.h"
#include "elog.h"

void tableset_priv_execute_where(TABSET tset);

char *tableset_optxt[] = {"eq", "ne", "gt", "lt", "ge", "le", "begins", NULL};

/* create a TABSET instance from a TABLE instance and reset the filters */
TABSET tableset_create (TABLE tab)
{
     TABSET tset;
     tset = nmalloc(sizeof(struct tableset));
     tset->tab     = tab;
     tset->cols    = NULL;	/* default - all columns */
     tset->where   = NULL;	/* default - no filtering */
     tset->sortby  = NULL;	/* default - no sort */
     tset->nwhere  = 0;		/* number of where conditions */
     tset->nunless = 0;		/* number of unless conditions */
     tset->rownums = NULL;	/* default - no row numbers */
     tset->groupby = NULL;	/* default - no groupings */
     tset->tobegarbage = NULL;	/* no garbage to start with */

     return tset;
}


/* destroy a TABSET class */
void tableset_destroy(TABSET tset)
{
     /* clear internal storage */
     tableset_reset(tset);

     /* garbage collect */
     if (tset->tobegarbage) {
	  itree_traverse(tset->tobegarbage)
	       nfree( itree_get(tset->tobegarbage) );
	  itree_destroy(tset->tobegarbage);
     }

     nfree(tset);
}


/* add to garbage collection when class is destroyed */
void tableset_freeondestroy(TABSET tset, void *tokill)
{
     if (!tset->tobegarbage)
	  tset->tobegarbage = itree_create();
     itree_append(tset->tobegarbage, tokill);
}


/* Reset the filters and column selections to give a full view to the 
 * underlying table data. Also causes all data to be garbaged collected,
 * that is all the data passed in tableset_tobefreed() will be sent to nfree().
 */
void   tableset_reset  (TABSET tset)
{
     if (tset->cols) {
          itree_destroy(tset->cols);
	  tset->cols = NULL;
     }
     if (tset->where) {
          itree_destroy(tset->where);
	  tset->where = NULL;
	  tset->nwhere = 0;
	  tset->nunless = 0;
     }
     if (tset->sortby) {
          nfree(tset->sortby);
	  tset->sortby = NULL;
     }
     if (tset->rownums) {
          itree_destroy(tset->rownums);
	  tset->rownums = NULL;
     }
     if (tset->groupby) {
          tree_destroy(tset->groupby);
	  tset->groupby = NULL;
     }
}


/*
 * Only use ordered cols.
 * The column names in the list should survive as long as the tableset instance.
 * Replaces any previous selected or excluded columns
 */
void   tableset_select (TABSET tset	/* tableset instance */, 
			ITREE *cols	/* ordered list of columns */)
{
     if (tset->cols)
	  itree_destroy(tset->cols);
     tset->cols = itree_create();
     itree_traverse(cols)
          if (table_hascol(tset->tab, itree_get(cols)))
	       itree_append(tset->cols, itree_get(cols));
}

/* only use ordered cols, text version
 * column names should be separated by whitespace */
void   tableset_selectt(TABSET tset	/* tableset instance */, 
			char *cols	/* whitespace separated string */)
{
     char *mycols, *thiscol;
     ITREE *listcols;

     mycols = xnstrdup(cols);
     tableset_freeondestroy(tset, mycols);

     thiscol = strtok(mycols, " \t");
     if (thiscol)
	  listcols = itree_create();
     else
	  return;		/* no cols to index */
     while (thiscol) {
	  itree_append(listcols, thiscol);
	  thiscol = strtok(NULL, " \t");
     }

     tableset_select(tset, listcols);
     itree_destroy(listcols);
     return;
}

/*
 * Use all columns but the ones in the list.
 * Replaces any previously selected or excluded columns
 * The column names in the list should survive as long as the tableset instance.
 */
void   tableset_exclude(TABSET tset	/* tableset instance */, 
			TREE *nocols	/* list of columns to exclude */)
{
     ITREE *colorder;
     char *colname;

     if (tset->cols)
	  itree_destroy(tset->cols);
     tset->cols = itree_create();
     colorder = table_getcolorder(tset->tab);
     itree_traverse(colorder) {
          colname = itree_get(colorder);
	  if (tree_find(nocols, colname) == TREE_NOVAL)
	       itree_append(tset->cols, colname);
     }
}

/* use all but nocols, text version. tab and space used as delimiter */
void   tableset_excludet(TABSET tset	/* tableset instance */, 
			 char *nocols	/* whitespace separated string */)
{
     char *mycols, *thiscol;
     TREE *listcols;

     mycols = xnstrdup(nocols);
     tableset_freeondestroy(tset, mycols);

     thiscol = strtok(mycols, " \t");
     if (thiscol)
	  listcols = tree_create();
     else
	  return;		/* no cols to index */
     while (thiscol) {
          tree_add(listcols, thiscol, NULL);
	  thiscol = strtok(NULL, " \t");
     }

     tableset_exclude(tset, listcols);
     tree_destroy(listcols);
     return;
}

/*
 * Choose rows depending on column relationships. 
 * Where and unless conditions accumulate in the order they are executed
 * and form AND relationships between each condition.
 * They can be reset using tableset_reset().
 * The data for col and val are copied and may be released after this call.
 */
void   tableset_where  (TABSET tset		/* tableset instance */, 
			char *col		/* column name */,
			enum tableset_op op	/* condition operator */, 
			char *val		/* value */ )
{
     TABSET_COND cond;

     if ( ! tset->where )
          tset->where = itree_create();

     cond = xnmalloc(sizeof(struct tableset));
     cond->col     = xnstrdup(col);
     cond->op      = op;
     cond->value   = xnstrdup(val);
     cond->iswhere = 1;		/* where */
     tableset_freeondestroy(tset, cond);
     tableset_freeondestroy(tset, cond->col);
     tableset_freeondestroy(tset, cond->value);

     itree_append(tset->where, cond);
     tset->nwhere++;
}

/* 
 * Filter out rows depending on column relationships. 
 * Where and unless conditions accumulate in the order they are executed.
 * In effect, each call to where or unless AND's the conditions.
 * They can be reset using tableset_reset().
 * The data for col and val are copied and may be released after this call.
 */
void   tableset_unless (TABSET tset		/* tableset instance */, 
			char *col		/* column name */,
			enum tableset_op op	/* condition operator */, 
			char *val		/* value */ )
{
     TABSET_COND cond;

     if ( ! tset->where )
          tset->where = itree_create();

     cond = xnmalloc(sizeof(struct tableset));
     cond->col     = xnstrdup(col);
     cond->op      = op;
     cond->value   = xnstrdup(val);
     cond->iswhere = 0;		/* unless */
     tableset_freeondestroy(tset, cond);
     tableset_freeondestroy(tset, cond->col);
     tableset_freeondestroy(tset, cond->value);

     itree_append(tset->where, cond);
     tset->nunless++;
}


/*
 * Configure the tableset from text, returning an error if parse fails
 * Currently, this is a very simple format consisting of individual lines,
 * each introduced by the verb token. Conditions are AND'ed together
 * Syntax:  where  <col> <op> <val>
 *          unless <col> <op> <val>
 * <op>:    eq - equal                       (string context)
 *          ne - not equal                   (string context)
 *          gt - greater than                (numeric context)
 *          lt - less than                   (numeric context)
 *          ge - greater than or equal to    (numeric context)
 *          le - less than or equal to       (numeric context)
 *          begins - text string begins with (string context)
 * example: "where col1 eq fred" or "unless col4 begins kev"
 * Returns 1 for success or 0 for failure, errors are sent to elog
 * The commands text buffer does not have to be kept, as it is duplicated 
 * internally.
 */
int    tableset_configure(TABSET t, char *commands) {
     TABLE tabcmds;
     int r, i;
     char *cmds, *mode, *col, *optxt, *val;
     enum tableset_op op;

     /* check arguments are present */
     if ( ! t )
          elog_die(FATAL, "No tableset specified");
     if ( ! commands || ! *commands ) {
          elog_printf(DIAG, "No commands supplied with which to configure");
	  return 0;	/* failure */
     }
     cmds = xnstrdup(commands);

     /* scan text commands into an unheaded table, which is to manage memory */
     tabcmds = table_create();
     r = table_scan(tabcmds, cmds, " \t", TABLE_MULTISEP, TABLE_NOCOLNAMES, 
		    TABLE_NORULER);
     if (r == -1) {
          elog_printf(ERROR, "Unable to scan commands: '%s'", commands);
	  nfree(cmds);
	  return 0;	/* failure */
     }
     table_freeondestroy(tabcmds, cmds);

     /* now interpret the commands into the tableset */
     table_traverse(tabcmds) {
          /* First column is the column/attribute name, second is the 
	   * operation to apply, third is the value */
          mode  = table_getcurrentcell(tabcmds, "column_0");
          col   = table_getcurrentcell(tabcmds, "column_1");
          optxt = table_getcurrentcell(tabcmds, "column_2");
          val   = table_getcurrentcell(tabcmds, "column_3");
	  if ((mode && col && optxt && val) == 0) {
	       elog_printf(ERROR, "None of the rows has the right number of "
			   "columns (should be 4: mode col optxt val)");
	       table_destroy(tabcmds);
	       return 0;	/* failure */
	  }

	  /* translate the operation text into enum */
	  for (i=0; tableset_optxt[i]; i++)
	       if (strcmp(tableset_optxt[i], optxt) == 0) {
		    op = i;
		    break;
	       }
	  if (tableset_optxt[i] == NULL) {
	       elog_printf(ERROR, "Line %d has an unknown operator '%s'; "
			   "not using this line", 
			   table_getcurrentrowkey(tabcmds) + 1, optxt);
	       table_destroy(tabcmds);
	       return 0;	/* failure */
	    
	  }

	  /* match the command to carry out operation */
          if (strncmp(mode, "where", 5) == 0) {
	       tableset_where(t, col, op, val);
	  } else if (strncmp(mode, "unless", 6) == 0) {
	       tableset_unless(t, col, op, val);
	  } else {
	       elog_printf(ERROR, "Unable to recognise configuration "
			   "statement, line %d: %s %s %s %s; not using this "
			   "line", table_getcurrentrowkey(tabcmds) + 1, 
			   mode, col, optxt, val);
	  }
     }

     table_destroy(tabcmds);
     return 1;	/* success */
}


/* row grouping dependent on column relationship */
/* NOT YET IMPLEMENTED */
void   tableset_groupby(TABSET tset, 
			char *col, 
			enum tableset_op op, 
			char *val)
{
}

/* row order dependent on column value, which can be an ascii or 
 * numeric sort, ascending or descending */
void   tableset_sortby (TABSET tset, 
			char *col, 
			int how /* 0=ascii desc, 1= ascii asc, 
				 * 2=num desc, 3=num asc */)
{
     /* check input parameters */
     if (!col)
          return;

     tset->sortby = xnstrdup(col);
     tset->sorthow = how;
}


/*
 * Carries out all the pending actions and creates a table containing 
 * the data. Table should be freed after use. The parent TABSET and TABLE
 * classes must still be in existance during the use of the table as certain
 * values depend on the existance of both classes.
 * Always creates a TABLE, but there may be no rows.
 */
TABLE   tableset_into   (TABSET tset)
{
     TABLE target;
     TREE *row, *infonames, *inforow;

     if (tset->where)
          tableset_priv_execute_where(tset);


     /* set up new table, depending on whether there are custom columns */
     if (tset->cols) {
          target = table_create_t(tset->cols);

	  /* add info lines  */
	  infonames = table_getinfonames(tset->tab);
	  tree_traverse(infonames) {
	       inforow = table_getinforow(tset->tab, tree_getkey(infonames));
	       table_addinfo_t(tset->tab, tree_getkey(infonames), inforow);
	       tree_destroy(inforow);
	  }
     } else {
          target = table_create_fromdonor(tset->tab);
     }

     /* add the selected rows in the tset order to our target table */
     if (tset->rownums) {
          itree_traverse(tset->rownums) {
	       row = table_getrow(tset->tab, (int) (long) 
				  itree_get(tset->rownums));
	       table_addrow_noalloc(target, row);
	       tree_destroy(row);
	  }
     } else {
          /* no where clauses, follow table order, want all rows */
          table_traverse(tset->tab) {
	       row = table_getcurrentrow(tset->tab);
	       table_addrow_noalloc(target, row);
	       tree_destroy(row);
	  }
     }

     return target;
}


/*
 * Carries out all the pending actions and creates a string containing
 * a textual representation of the selection criteria.
 * The string should be freed after use and is independent of the
 * parent TABLE and TABSET classes.
 */
char * tableset_print  (TABSET tset, 
			int pretty, 	/* justify cols and full rulers */
			int with_names, /* col headers */
			int with_info,	/* info rows */
			int with_body)	/* body rows */
{
     ITREE *colnames;
     TREE *infonames, *inforow, *row;
     STRBUF buf;
     char *text;

     buf = strbuf_init();
     if (tset->where)
          tableset_priv_execute_where(tset);

     if (tset->cols)
          colnames = tset->cols;
     else
          colnames = table_getcolorder(tset->tab);

     if (with_names) {
	  itree_traverse(colnames) {
	       strbuf_append(buf, itree_get(colnames));
	       strbuf_append(buf, "\t");
	  }

	  if ( ! itree_empty(colnames))
	       strbuf_backspace(buf);
	  strbuf_append(buf, "\n");
     }

     if (with_info) {
          infonames = table_getinfonames(tset->tab);
	  tree_traverse(infonames) {
	       inforow = table_getinforow(tset->tab, tree_getkey(infonames));
	       itree_traverse(colnames) {
		    strbuf_append(buf, 
				   tree_find(inforow, itree_get(colnames)));
		    strbuf_append(buf, "\t");
	       }
	       strbuf_append(buf, tree_getkey(infonames));
	       strbuf_append(buf, "\n");
	       tree_destroy(inforow);
	  }
	  strbuf_append(buf, "--\n");
     }

     if (with_body) {
          /* add the selected rows in the tset order to our target table */
          if (tset->rownums) {
	       itree_traverse(tset->rownums) {
		 row = table_getrow(tset->tab, (int) (long)
				    itree_get(tset->rownums));
		    itree_traverse(colnames) {
		         strbuf_append(buf, 
				       tree_find(row, itree_get(colnames)));
			 strbuf_append(buf, "\t");
		    }

		    if ( ! itree_empty(colnames))
		         strbuf_backspace(buf);
		    strbuf_append(buf, "\n");
		    tree_destroy(row);
	       }
	  } else {
	       /* no where clauses, follow table order, want all rows */
	       table_traverse(tset->tab) {
		    row = table_getcurrentrow(tset->tab);
		    itree_traverse(colnames) {
		         strbuf_append(buf, 
				       tree_find(row, itree_get(colnames)));
			 strbuf_append(buf, "\t");
		    }

		    if ( ! itree_empty(colnames))
		         strbuf_backspace(buf);
		    strbuf_append(buf, "\n");
		    tree_destroy(row);
	       }
	  }
     }

     text = strbuf_string(buf);
     strbuf_fini(buf);

     return text;
}

void   tableset_delete(TABSET tset)
{
}



/*
 * Carry out the pending row and sorting actions, saving data rows 
 * by index (rownum) in the tableset structure. Effectively an AND 
 * relationship where there are multiple where clauses.
 */
void tableset_priv_execute_where(TABSET tset)
{
     TABSET_COND cond;
     int clause_true, fpcontext;
     double af, bf;
     long al,bl;
     char *value;
     TREE *sorted;
     ITREE *isorted;

     /* no table initialised */
     if (!tset->tab || tset->rownums)
          return;

     /* if we've done it before, assume that there is a change in 
      * circumstance (in the data table or selection conditions) and 
      * do it again */
     if (tset->rownums)
          itree_destroy(tset->rownums);
     tset->rownums = itree_create();

     table_traverse(tset->tab) {
          /* iterate over the conditions for each row */
          clause_true=0;		/* accumulated condition */
          itree_traverse(tset->where) {
	       cond = itree_get(tset->where);
	       value = table_getcurrentcell(tset->tab, cond->col);

	       /* no value is equivolent to NUL in sql */
	       if ( ! value )
		    continue;

	       /* decide context */
	       if (cond->op == gt || cond->op == lt || cond->op == ge || 
		   cond->op == le) {
		    /* numeric context, find out if floating point too */
		    if (strchr(value, '.') || strchr(cond->value, '.')) {
		         /* fp context */
		         af = atof(value)/*strtof(value, (char**)NULL)*/;
			 bf = atof(cond->value);
			 fpcontext = 1;
		    } else {
		         /* long int context */
		         al = strtol(value, (char**)NULL, 10);
			 bl = strtol(cond->value, (char**)NULL, 10);
			 fpcontext = 0;
		    }
	       }

	       /* evaluate operator against arguments */
	       switch (cond->op) {
	       case eq:
	            if (strcmp(value, cond->value) == 0)
		         clause_true++;
	            break;
	       case ne:
	            if (strcmp(value, cond->value) != 0)
		         clause_true++;
	            break;
	       case gt:
		    if (fpcontext) {
		         if (af > bf)
			      clause_true++;
		    } else {
		         if (al > bl)
			      clause_true++;
		    }
	            break;
	       case lt:
		    if (fpcontext) {
		         if (af < bf)
			      clause_true++;
		    } else {
		         if (al < bl)
			      clause_true++;
		    }
	            break;
	       case ge:
		    if (fpcontext) {
		         if (af >= bf)
			      clause_true++;
		    } else {
		         if (al >= bl)
			      clause_true++;
		    }
	            break;
	       case le:
		    if (fpcontext) {
		         if (af <= bf)
			      clause_true++;
		    } else {
		         if (al <= bl)
			      clause_true++;
		    }
	            break;
	       case begins:			/* begins with */
		    if (value && cond->value && 
			strncmp(value, cond->value, strlen(cond->value)) == 0)
		         clause_true++;
	            break;
	       }

	       /* implement an AND relationship with other clauses.
		* if a cluase is false (false for where, true for unless)
		* then the next data is checked */
	       if ( ! clause_true && cond->iswhere )
		    goto next_row;
	       if ( clause_true && ! cond->iswhere )
		    goto next_row;
	       clause_true = 0;
	  }
	  /* save the row number in our set */
	  itree_append(tset->rownums, 
		       (void *) (long) table_getcurrentrowkey(tset->tab));
  next_row:
  	;
     }


     /* sort if requested */
     if (tset->sortby) {
	  isorted = itree_create();
          if (tset->sorthow == TABSET_SORT_ASCII_DESC ||
	      tset->sorthow == TABSET_SORT_ASCII_ASC) {
	      /* ascii sort */
	       sorted = tree_create();
	       itree_traverse(tset->rownums) {
		    value = table_getcell(tset->tab, (int) (long)
					  itree_get(tset->rownums), 
					  tset->sortby);
		    tree_add(sorted, value, (void *) itree_get(tset->rownums));
	       }

	       if (tset->sorthow == TABSET_SORT_ASCII_ASC) {
		    /* ascending */
		    tree_traverse(sorted)
		         itree_append(isorted, tree_get(sorted));
	       } else {
		    /* descending -- slightly harder to sort */
	       }
	       tree_destroy(sorted);
	  } else {
	       /* numeric sort */
	       itree_traverse(tset->rownums) {
		    value = table_getcell(tset->tab, (int) (long)
					  itree_get(tset->rownums), 
					  tset->sortby);
		    itree_add(isorted, strtol(value, (char**)NULL, 10), 
			      (void *) itree_get(tset->rownums));
	       }
	  }

	  /* store */
	  itree_destroy(tset->rownums);
	  tset->rownums = isorted;
     }
}
