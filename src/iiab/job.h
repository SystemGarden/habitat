/*
 * Job class
 * Executes periodic work that requires logging or I/O.
 * Uses runq underneath.
 *
 * Nigel Stuckey, December 1997
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _JOB_H_
#define _JOB_H_

#include <time.h>
#include "meth.h"
#include "table.h"

#define JOB_TMPBUF 100

/* Job queue structure */
struct job_work {
     char *origin;			/* String describing origin */
     struct meth_invoke *runarg;	/* Invokation reference */
     int runq;				/* Runq reference */
};

void  job_init();
void  job_fini();
void  job_dump();
int   job_add(long start, long interval, long phase, long count, 
	      char *key, char *origin, char *result, char *error, 
	      int keep, char *method, char *command);
int   job_rm(int ikey);
void  job_clear();
void  job_runqexpired(void *ikey);
int   job_loadroute(char *purl);
TABLE job_scanintotable(char *jobtext);

#endif /* _JOB_H_ */
