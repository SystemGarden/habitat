/*
 * Work queue
 * Nigel Stuckey, August 1996
 *
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _RUNQ_H_
#define _RUNQ_H_

#include <time.h>
#include <limits.h>

#define RUNQ_TMPBUF 100
#define RUNQ_EXPIREWAITDEF 2
#define RUNQ_EXPIREWAITMAX 10
#define RUNQ_MAXID INT_MAX
#define RUNQ_CB_EXPIRED "runq_expired"

/* Work queue structure */
struct runq_work {
     time_t start;		/* start time */
     long interval;		/* time in between each execution */
     long phase;		/* order in each time point */
     long count;		/* number of times to repeat */
     char *desc;		/* string description */
     int (*startofrun)();	/* call at start of run set */
     int (*command)();		/* command to execute */
     int (*isrunning)();	/* Test for command still running */
     int (*endofrun)();		/* call at end of run set */
     void *argument;		/* argument buffer */
     int arglen;		/* length of argument buffer */
     int nruns;			/* accumulated number of runs */
     int expired;		/* =1 if no further executions of work */
     int clearup;		/* =1 remove from runq_tab list */
};

/* Time resolution list */
struct runq_resolve {
	int ikey;		/* Index for runq_tab list */
	struct runq_resolve *next; /* Next thing to do */
};

/* Functional prototype */
void runq_init(time_t);
void runq_fini();
void runq_dump();
int  runq_add(long start, long interval, long phase, long count, char *desc, 
	      int (*startofrun)(), int (*command)(), int (*isrunning)(),
	      int (*endofrun)(), void *argument, int arglen);
int  runq_rm(int ikey);
void runq_clear();
int  runq_ntab();
int  runq_nsched();
int  runq_sched (int ikey,           time_t last, time_t now);
int  runq_schedw(struct runq_work *, time_t last, time_t now);
void runq_schedall();
int  runq_schedrm(int ikey);
void runq_schedrmall();
void runq_dispatch();
void runq_setdispatch();
void runq_sigdispatch();
void runq_methfinished(void *key);
void runq_disable();
void runq_enable();

#endif /* _RUNQ_H_ */
