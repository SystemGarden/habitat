/*
 * Execution methods
 * Nigel Stuckey, August 1996
 * Major revision and reorganisation January 1998
 *
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _METH_H_
#define _METH_H_

#include <time.h>
#include "tree.h"
#include "route.h"

/* five seconds time out in the relay or standby function */
#define METH_RELAY_TOSEC 30
#define METH_RELAY_TOUSEC 0
/* maximum length of descriptions */
#define METH_DESC_LEN 64
/* signals with which to abort jobs */
#define METH_SIG_KILL 15
#define METH_SIG_BUTCHER 9
/* seconds to wait after killing and butchering for responses */
#define METH_SHUT_KILLSEC 3
#define METH_SHUT_KILLUSEC 0
#define METH_SHUT_BUTCHERSEC 2
#define METH_SHUT_BUTCHERUSEC 0
/* callback identifier */
#define METH_CB_FINISHED "meth_finished"

extern TREE *meth_methods;	/* Loaded methods; tree of meth_info by name */

/* method specification, containing callbacks to the implementing code */
struct meth_info {
     char *(*name)();	/* Name of method */
     char *(*info)();	/* String description of method */
     enum exectype (*type)();	/* How the method should be spawned */
     int (*prerun)();	/* Function to run before a run of preaction&actions */
     int (*preaction)();/* Function to run before spawning */
     int (*action)();	/* Function to run after spawning */
     int (*postrun)();	/* Function to run after a run of preaction&actions */
     char *fname;	/* .so file containing this function */
};

typedef struct meth_info * METHID;

/* method invocation structure, associating method with a command and 
 * IO routes */
struct meth_invoke {
     char *key;		/* name identifing job/process. Name makes it unique
			 * and used to return results if timestore */
     METHID run;       	/* method specification */
     char *command;	/* command string */
     char *res_purl;	/* route to output results */
     char *err_purl;	/* route to output errors */
     long keep;		/* number of recent data to keep */
};

/* per run-set structure, instantiating a meth_invoke structure with 
 * specific open routes and time. */
struct meth_runset {
     char *res_purl;	/* route to output results */
     char *err_purl;	/* route to output errors */
     ROUTE res;		/* destination for results */
     ROUTE err;		/* destination for errors */
     time_t opened;	/* time at which the routes were opened */
     int pid;		/* current running pid for METH_FORK */
     int oneshot;	/* flag single run and locally managed I/O */
};

/* per process structure. used for process spawing method types (METH_FORK) */
struct meth_runprocinfo {
     char *key;		/* job key or identifier */
     int pid;		/* process identifier */
     time_t start;	/* time proccess was started */
     int resfd;		/* per-run file descriptor for incomming results */
     int errfd;		/* per-run file descriptor for incomming errors */
};


/* Functional prototypes */
void meth_init();
void meth_fini();
void meth_add(struct meth_info *newm);
int  meth_load(char *fname);
void meth_dump();
int  meth_check(char *name);
/*int (*meth_lookup(char *name))();*/
METHID meth_lookup(char *name);
void meth_add_fdcallback(int fd, char *cb_name);
void meth_rm_fdcallback(int fd);
int  meth_startrun_s(struct meth_invoke *args, int argl);
int  meth_startrun(char *key, METHID run, char *command, char *res_purl, 
		   char *err_purl, long keep);
int  meth_execute_s(struct meth_invoke *args, int argl);
int  meth_execute(char *key, METHID run, char *command, char *res_purl, 
		  char *err_purl, long keep);
int  meth_actiononly(METHID run, char *command,	char *res_purl,	
		     char *err_purl, long keep);
int  meth_endrun_s(struct meth_invoke *args, int argl);
int  meth_endrun(char *key, METHID run, char *command, char *res_purl, 
		 char *err_purl, long keep);
int  meth_isrunning_s(struct meth_invoke *args, int argl);
int  meth_isrunning(char *key);
void meth_kill(struct meth_runprocinfo *rp);
void meth_butcher(struct meth_runprocinfo *rp);
void meth_child(int signum);
int  meth_relay();
int  meth_shutdown();

/* Macros */
	/* Return name as a string */
#define meth_name(m) (m==0 ? "unknown": (*(m)->name)())
	/* Return METHID for name `n' or NULL if non-existant */
#define meth_lookup(n) tree_find(meth_methods, (n))

/* ------ Definitions for loadable methods ------ */

/* Method execution types */
enum exectype {	METH_NONE,	/* no method */
		METH_FORK,	/* fork() before running (*action)() */
		METH_THREAD,	/* run (*action)() in a thread */
		METH_SOURCE	/* run (*action)() in the same process 
				 * as the dispatcher */
	      };

/* Generic functional prototypes for loadable modules */
char *id();
char *info();
enum exectype type();
int beforerun(char *);
int preaction(char *);
int action(char *, ROUTE result, ROUTE error);
int afterrun(char *);

#endif /* _METH_H_ */
