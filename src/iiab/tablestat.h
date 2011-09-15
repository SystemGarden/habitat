/*
 * Cascade sampling class.
 * Nigel Stuckey, November 1999.
 *
 * Copyright System Garden Ltd 1999-2001, all rights reserved.
 */

#include "table.h"

/* Statistical functions */
enum tablestat_fn {
     TABSTAT_AVG,	/* Average function */
     TABSTAT_MIN,	/* Minimum function */
     TABSTAT_MAX,	/* Maximum function */
     TABSTAT_SUM,	/* Summing function */
     TABSTAT_LAST,	/* Last result function */
     TABSTAT_RATE	/* Per-second averaging/rate function */
};

/* Request structure for sample method (sample_tab) */ 
struct tablestat_info {
     enum tablestat_fn default_fn;	/* default function */
     tree *col_fn;			/* column function: hash of colname 
					 * to tablestat_fn */
     TABLE
};

typedef struct tablestat_info *TABSTAT;

#define METH_BUILTIN_SAMPLE_NTABS 200
#define TABLESTAT_INFOKEYROW "key"

TABSTAT tablestat_init(enum tablestat_fn func);
void    tablestat_addfn(TABSTAT session, char *col, enum tablestat_fn);
int     tablestat_sample(TABSTAT session, TABLE data);
void    tablestat_finalsample(TABLESTAT *session, ROUTE output, ROUTE error,
			 TABLE basetab,	TABLE sampletab, int nsamples,
			 char *keycol, time_t base_t, time_t sample_t);
void tablestat_fini(TABLESTAT *sampent);

