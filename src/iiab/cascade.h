/*
 * Cascade sampling class.
 * Nigel Stuckey, November 1999.
 *
 * Copyright System Garden Ltd 1999-2001, all rights reserved.
 */

#include "route.h"
#include "table.h"
#include "rs.h"

/* Cascade functions */
enum cascade_fn {
     CASCADE_AVG,	/* Average function */
     CASCADE_MIN,	/* Minimum function */
     CASCADE_MAX,	/* Maximum function */
     CASCADE_SUM,	/* Summing function */
     CASCADE_LAST,	/* Last result function */
     CASCADE_FIRST,	/* First result function */
     CASCADE_DIFF,	/* Difference function */
     CASCADE_RATE	/* Mean rate function */
};

/* Request structure for sample method (sample_tab) */ 
struct cascade_info {
     enum cascade_fn fn;	/* function to apply to data */
     char *purl;		/* opened, monitored route */
     int seq;			/* last sequence read */
};

typedef struct cascade_info CASCADE;

#define METH_BUILTIN_SAMPLE_NTABS 200
#define CASCADE_INFOKEYROW "key"

CASCADE *cascade_init(enum cascade_fn func, char *monroute);
void cascade_fini(CASCADE *sampent);
int cascade_sample(CASCADE *sampent, ROUTE output, ROUTE error);
TABLE cascade_aggregate(enum cascade_fn func, TABLE dataset);
void cascade_finalsample(CASCADE *sampent, ROUTE output, ROUTE error,
			 TABLE basetab,	TABLE sampletab, int nsamples,
			 char *keycol, time_t base_t, time_t sample_t);

