/*
 * Execution methods
 * Nigel Stuckey, August 1996
 * Major revision and reorganisation January 1998
 * Major modifications January-February 1999
 * Minor mods July 1999
 * Major mods October 1999
 * Addition of meth_isrunning() & meth_isrunning_s() December 2000
 * Hooks for generic socket I/O, esp for web serving February 2001
 *
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

/*
 * After initialising with meth_init(), the method class is loaded with
 * all the 'built-in' methods provided by the struct meth_builltins[]
 * and implemented from the meth_b.c.
 *
 * Additional methods provided externally are installed with meth_load(),
 * which takes .so files that provide a set of functions:
 *   id(), info(), type(), preaction() & action()
 * See meth_load() for more information.
 *
 * The current state of the methods available may be inspected by
 * meth_dump() and their presence check with meth_check().
 *
 * To call a method, use meth_execute() [or meth_execute_s()], which
 * will arrange the correct execution environment, possibly in another
 * process, and set up the stdin, stdout and stderr. The method chosen 
 * dictates whether it is an internal or external command. A unique key 
 * should be provided by the user to refer to the invocation, which may
 * be reused once the work is finished.
 *
 * When a method executed as a sub process has finished, the signal handler
 * meth_sigchild() is called by the SIGCHLD signal to collect the exit 
 * condition and enqueue for later processing by meth_exitchildren(), 
 * which clear ups, flush buffers and calculates audit details. 
 * You can check for a running process by using meth_isrunning() with the 
 * user supplied key.
 *
 * Once jobs have been set up, meth_relay() should be used to implement
 * non native i/o, specifically the timestore route. It implements
 * select() and should be used during idle times.
 *
 * The caller may know that an individual method run is part of series, in
 * which case I/O efficiency can be increased by keeping the I/O channels 
 * open during the series. The caller does this by calling meth_startorfun()
 * with the arguments, then using the same arguments to meth_execute().
 * It is then the caller's responsibility to close the I/O by calling 
 * meth_endofrun() when the series is finished and the final execution 
 * has been completed [use meth_isrunning()].
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include "nmalloc.h"
#include "sig.h"
#include "itree.h"
#include "route.h"
#include "elog.h"
#include "meth.h"
#include "meth_b.h"
#include "callback.h"

TREE    *meth_methods;	/* loaded methods; tree of meth_info index by name */
TREE    *meth_rsetbykey;/* open routes indexed by work key */
ITREE   *meth_procbypid;/* running method processes; 
			 * tree of meth_runprocinfo index by pid */
ITREE   *meth_exitbypid;/* exited processes; tree of pids (in key) populated
			 * by meth_sigchild() and consumed by meth_child() */
ITREE   *meth_cbbyfd;	/* callback by file descriptor */
int   meth_restartselect; /* set by meth_child() for meth_relay() to restart
			   * its select() as the results may not be correct */
int      meth_argc;	  /* saved argc used by restart method */
char   **meth_argv;	  /* saved argv used by restart method */
void *meth_shutdown_func; /* shutdown function used by restart and shutdown
			   * built-in methods */

/* Initialise method structures
 * Arguments are argc and argv to recreate the process on restart and
 * a pointer to a shutdown function for the application or NULL
 * for no argument. Used by 'restart' and 'shutdown' built-in methods 
 * (see meth_b.c)
 */
void meth_init(int argc, char *argv[],		/* used for restart built-in */
	       void *(this_shutdown_func)()	/* used for restart+shutdown */
	       )
{
     int i;

     sig_setchild(meth_sigchild);
     sig_off();		/* normally unaccepted - use preemption points */
     meth_methods       = tree_create();
     meth_rsetbykey     = tree_create();
     meth_procbypid     = itree_create();
     meth_exitbypid     = itree_create();
     meth_cbbyfd        = itree_create();
     meth_restartselect = 0;
     meth_shutdown_func = this_shutdown_func;
     meth_argc          = argc;
     meth_argv          = argv;

     for (i=0; meth_builtins[i].name; i++)
	  /* Add to tree with the method's id name as key */
	  tree_add(meth_methods, meth_builtins[i].name(), &meth_builtins[i]);

     return;
}

/* Shutdown the method class without waiting for jobs */
void meth_fini()
{
     struct meth_info *m;
     struct meth_runset *rset;
     struct meth_runprocinfo *rp;
     int status, pid;

     /* All plain sailing apart from running processes which potentially 
      * have outstanding I/O, but we don't care about them. So the only
      * thing we want to do is to prevent ourselves from being interrupted
      * then tear down the structures */
     /* close open routes & destroy its tree */
     while ( ! tree_empty(meth_rsetbykey) ) {
	  tree_first(meth_rsetbykey);
	  rset = tree_get(meth_rsetbykey);
	  if (meth_endrun(tree_getkey(meth_rsetbykey), 0, "--shutdown--",
			  rset->res_purl, rset->err_purl, 0) == -1) {
	       /* force end of run */
	       elog_printf(INFO, "ending method still running: key %s "
			   "results %s errors %s", 
			   tree_getkey(meth_rsetbykey), 
			   rset->res_purl, rset->err_purl);
	       nfree(tree_getkey(meth_rsetbykey));
	       route_close(rset->res);
	       route_close(rset->err);
	       nfree(rset->res_purl);
	       nfree(rset->err_purl);
	       nfree(rset);
	       tree_rm(meth_rsetbykey);
	  }
     }
     tree_destroy(meth_rsetbykey);

     /* remove exitbypid - the list of child processes that have exit()ed */
     while ( ! tree_empty(meth_exitbypid) ) {
	  itree_first(meth_exitbypid);
	  pid    = itree_getkey(meth_exitbypid);
	  status = ((long long) itree_get(meth_exitbypid) & 0xffff);
	  elog_printf(INFO, "Child process %d exited with status %d", pid, 
		      status);
	  itree_rm(meth_exitbypid);
     }

     /* remove procbypid */
     while ( ! tree_empty(meth_procbypid) ) {
	  itree_first(meth_procbypid);
	  rp = itree_get(meth_procbypid);
	  itree_rm(meth_procbypid);
	  nfree(rp->key);
	  nfree(rp);
     }

     /* remove methods */
     while ( ! tree_empty(meth_methods) ) {
          tree_first(meth_methods);
          m = tree_get(meth_methods);
	  if (m->fname) {
	       /* dynamically loaded & allocated methods added by
		* meth_load() will have m->fname set to the filename of
		* to .so file. Builtin methods will have m->fname == NULL */
	       nfree(m->fname);
	       nfree(m);
	  }
	  tree_rm(meth_methods);
     }
     tree_destroy(meth_methods);

     /* remove exited process tree */
     itree_destroy(meth_exitbypid);

     /* remove running process tree */
     itree_destroy(meth_procbypid);

     /* remove callback tree */
     itree_destroy(meth_cbbyfd);
}


/* 
 * Dump method parameters.
 * Sends diagnostic information to the debug log
 */
void meth_dump() {
     struct meth_info *m;
     struct meth_runprocinfo *p;
     int i=1;

     elog_startsend(DEBUG, "Methods -----------------------------------------------------------\n");
     tree_traverse(meth_methods) {
	  m = tree_get(meth_methods);
	  elog_contprintf(DEBUG, "    %2d %8s %35s %s\n", 
			i, tree_getkey(meth_methods), (*m->info)(), m->fname);
	  i++;
     }
     elog_startsend(DEBUG, "Running methods (meth_procbypid) ----------------------------------\n");
     i = 1;
     itree_traverse(meth_procbypid) {
	  p = itree_get(meth_procbypid);
	  elog_contprintf(DEBUG, "    %2d pid %5d started %8d\n", 
			i, p->pid, p->start);
	  i++;
     }
     elog_endsend(DEBUG,  "-------------------------------------------------------------------");
}


/* Add a single method from the available address space */
void meth_add(struct meth_info *newm /* method callback structure */)
{
     char *fnname;
     struct meth_info *oldm;

     /* add to method list uniquely; replaces exsiting method if not unique */
     fnname = newm->name();
     if (tree_find(meth_methods, fnname) != TREE_NOVAL) {
          oldm = tree_get(meth_methods);
	  if (oldm->fname) {
	       nfree(oldm->fname);
	       nfree(oldm);
	  }
	  tree_put(meth_methods, newm);
     } else {
          tree_add(meth_methods, fnname, newm);
     }
}


/*
 * Load the execution method shared object from the file
 * Returns 0 for success, -1 for error.
 */
int meth_load(char *fname /* filename of shared object (.so) */) {
     struct meth_info *m, *oldm;
     void *od;
     char *(*name)(), *fnname;
     char *(*info)();
     enum exectype (*type)();
     int (*prerun)();
     int (*preact)();
     int (*act)();
     int (*postrun)();
     const char *err;

     /* Open the loadable object and link to the following functions
      * id()		For the method's symbolic name
      * info()		To describe the method
      * type()		How the method needs to be called
      * beforerun()     Before a run of 1 or more actions (optional)
      * preaction()	Early action (type dependent) (optional)
      * action()	The main work of the method
      * afterrun()      After a run of 1 or more actions (optional)
      */
     od = dlopen(fname, RTLD_NOW);
     if (od == NULL) {
	  elog_printf(ERROR, "unable to open method object: %s", dlerror());
	  return -1;
     }
     name = dlsym(od, "id");
     if ((err = dlerror()) != NULL) {
	  elog_printf(ERROR, "can't link to id() in file %s: %s", fname, err);
	  return -1;
     }
     info = dlsym(od, "info");
     if ((err = dlerror()) != NULL) {
	  elog_printf(ERROR, "can't link to info() in file %s: %s",fname,err);
	  return -1;
     }
     type = dlsym(od, "type");
     if ((err = dlerror()) != NULL) {
	  elog_printf(ERROR, "can't link to type() in file %s: %s",fname,err);
	  return -1;
     }
     switch ((*type)()) {			/* Check type is OK */
     case METH_FORK:
     case METH_SOURCE:
	  break;
     case METH_THREAD:
	  elog_printf(ERROR, "threaded methods unsupported");
	  return -1;
     default: 	  elog_printf(ERROR, "unknown method type (%d)", (*type)());
	  return -1;
     }
     preact = NULL;
     preact = dlsym(od, "preaction");	/* Optional */
     dlerror();				/* Linux MUST check failure */
     prerun = NULL;
     prerun = dlsym(od, "beforerun");	/* Optional */
     dlerror();				/* Linux MUST check failure */
     act = dlsym(od, "action");
     if ((err = dlerror()) != NULL) {
	  elog_printf(ERROR, "can't link to action() in file %s: %s", fname, 
		      err);
	  return -1;
     }
     postrun = NULL;
     postrun = dlsym(od, "afterrun");	/* Optional */
     dlerror();				/* Linux MUST check failure */
     
     /* Store everything that we have learned in a meth_info struct */
     m = nmalloc(sizeof(struct meth_info));
     if (m == NULL) {
	  elog_printf(ERROR, "can't nmalloc for meth_info (%d bytes)", 
		      sizeof(struct meth_info));
	  return -1;
     }
     m->fname = nmalloc(strlen(fname)+1);
     if (m->fname == NULL) {
	  elog_printf(ERROR, "can't nmalloc meth_info fname (%d bytes)", 
		      strlen(fname)+1);
	  nfree(m);
	  return -1;
     }
     strcpy(m->fname, fname);
     m->name = name;
     m->info = info;
     m->type = type;
     m->prerun = prerun;
     m->preaction = preact;
     m->action = act;
     m->postrun = postrun;

     /* Add to tree with the method's id name as key; duplicates are not
      * allowed and the previous version should be cleared up if 
      * dynamically allocated */
     fnname = (*name)();
     if (tree_find(meth_methods, fnname) != TREE_NOVAL) {
          oldm = tree_get(meth_methods);
	  if (oldm->fname) {
	       nfree(oldm->fname);
	       nfree(oldm);
	  }
	  tree_put(meth_methods, m);
     } else {
          tree_add(meth_methods, fnname, m);
     }

     return 0;
}

/*
 * Has the method has been loaded
 * Returns 0=loaded, -1=not loaded
 */
int meth_check(char *name /* name of method */) {
     return((tree_present(meth_methods, name) == 1 ? 0 : -1));
}

#if 0
/*
 * Lookup the method name and return a METHID identifier.
 * Returns NULL is unable to match the method or METHID otherwise.
 */
METHID meth_lookup(char *name /* name of method */) {
     if (tree_find(meth_methods, name) == TREE_NOVAL)
	  return NULL;
     else
	  return tree_get(meth_methods);
}
#endif


/*
 * Add a file descriptor and callback pair for socket I/O.
 * File descriptor must exist and cb_name is a non-malloced string
 * that is sent to the callback class when an event occurs on fd.
 */
void meth_add_fdcallback(int fd, char *cb_name)
{
     if (itree_find(meth_cbbyfd, fd) == ITREE_NOVAL)
	  itree_add(meth_cbbyfd, fd, cb_name);
     else
	  itree_put(meth_cbbyfd, cb_name);
}

/* Remove a file descriptor from the sock I/O list */
void meth_rm_fdcallback(int fd)
{
     if (itree_find(meth_cbbyfd, fd) != ITREE_NOVAL)
	  itree_rm(meth_cbbyfd);
}


/* Alternative signature for meth_startrun() */
int  meth_startrun_s(struct meth_invoke *args, 	/* argument structure */
		   int argl			/* size of structure */)
{
     return meth_startrun(args->key, args->run, args->command, 
			args->res_purl, args->err_purl, args->keep);
}

/*
 * Open I/O routes for this work, call beforerun() for this method,
 * create and store the details in a runset structure.
 * Called with the same arguments as meth_execute() below.
 * Returns the code returned by the beforerun() function or 0.
 * Running this function tells meth that the there will be a run of
 * executions and the i/o should be left open
 */
int  meth_startrun(char *key, METHID run, char *command, char *res_purl, 
		   char *err_purl, long keep)
{
     char description[METH_DESC_LEN];
     struct meth_runset *rset;
     int ret=0;

     /* Debug: log open io */
     elog_printf(DEBUG, "start-of-run for %s method %s command `%s' opening "
		 "routes: results %s errors %s",
		 key, meth_name(run), command, res_purl, err_purl);

     /* Initialise a runset structure and open the result and error routes */
     rset = xnmalloc(sizeof(struct meth_runset));
     rset->res_purl = xnstrdup(res_purl);
     rset->err_purl = xnstrdup(err_purl);
     rset->opened = time(NULL);
     snprintf(description, METH_DESC_LEN,"output from %s", command);
     rset->res = route_open(res_purl, description, NULL, keep);
     snprintf(description, METH_DESC_LEN, "error from %s", command);
     rset->err = route_open(err_purl, description, NULL, keep);
     rset->pid = -1;
     rset->oneshot = 0;

     /* handle p-url errors by assiging to the default route */
     if (rset->res == NULL) {
	  elog_printf(ERROR, "job %s: can't open %s for results; using "
		      "default", key, res_purl);
	  rset->res = route_open("stdout:", "failsafe output for results",
				 NULL, 1);
     }
     if (rset->err == NULL) {
	  elog_printf(ERROR, "job %s: can't open %s for errors; using "
		      "default", key, err_purl);
	  rset->err = route_open("stderr:", "failsafe output for errors",
				 NULL, 1);
     }

     /* store */
     tree_add(meth_rsetbykey, xnstrdup(key), rset);

     /* call method start-of-run function */
     if (run->prerun) {
	  ret = (run->prerun)(command, rset->res, rset->err, rset);
	  if (ret)
	       elog_printf(ERROR, "job %s prerun() returns %d", key, ret);
     } else
	  ret = 0;

     return ret;
}


/*
 * Alternative signature to meth_execute().
 * Using a structure to pass args
 */
int meth_execute_s(struct meth_invoke *args,	/* Argument structure */
		   int argl			/* Size of structure */ ) {
     return meth_execute(args->key, args->run, args->command, 
			 args->res_purl, args->err_purl, args->keep);
}

/*
 * Execute a method.
 * The method is to be named by the string key (which should be unique, 
 * used for killing etc), METHID run specifies the method and passes the
 * arguments in a string called command.
 * Res_purl and err_purl specify the pseudo-urls for the job's 
 * result and error channels (stdout and stderr).
 * The method's definition (METHID->type) dictates how to run/schedule:-
 *    (1) the main thread (discouraged as it may take lots of time)
 *    (2) a new thread
 *    (3) a new process
 * The method is passed the command as its argument
 * The I/O should have been setup before execution.
 * Returns 0 for success, non-0 otherwise.
 */
int meth_execute(char *key,		/* name identifing job/process  */
		 METHID run,		/* method specification */
		 char *command,		/* command string */
		 char *res_purl,	/* route to output results */
		 char *err_purl,	/* route to output errors */
		 long keep		/* number of recent data to keep */ )
{
     struct meth_runprocinfo *rp;
     struct meth_runset *rset;
     int pid, r, respipefd[2] = {-1,-1}, errpipefd[2] = {-1,-1};

     /* Debug: Execute log */
     elog_printf(DEBUG, "running job %s method %s command `%s' results %s "
		 "errors %s", key, meth_name(run), command, res_purl, 
		 err_purl);

     /* check routes have been prepared */
     if ( (rset = tree_find(meth_rsetbykey, key)) == TREE_NOVAL ) {
	  elog_printf(DEBUG, "job %s: routes not opened, assuming oneshot "
		      "expire", key);
	  /* open I/O routes, create runset structure and set oneshot flag */
	  meth_startrun(key, run, command, res_purl, err_purl, keep);
	  rset = tree_find(meth_rsetbykey, key);
	  rset->oneshot++;
     }

     /* 
      * If available, source preaction() before any process/thread
      * construction. Note, the sourcing of preaction() may be changed
      * in the future with the type.
      */
     if (run->preaction) {
	  r = (*run->preaction)(command);
	  if (r)
	       elog_printf(ERROR, "job %s preaction() returns %d", key, r);
     }

     switch((*run->type)()) {
     case METH_FORK:		/* Use fork() to spawn the method */
	  /* Initialise a run process structure for meth_child() to
	   * collect, clean up and report */
	  rp = xnmalloc(sizeof(struct meth_runprocinfo));
	  rp->key = xnstrdup(key);
	  rp->start = time(NULL);
	  rp->resfd = -1;
	  rp->errfd = -1;

	  /*
	   * Currently, there is a problem with too many writers
	   * using ringstore and this causes reliability problems.
	   * The temporary solution is to relay the result and error
	   * data to the parent (running meth_relay()) using pipes.
	   * For now, construct a pipe for all data types, place the fd's
	   * in pipefd and make the reading end non-blocking as we
	   * don't want the dispatcher going to sleep!
	   * Currently, the parent will relay output from the child
	   * using meth_relay().
	   */
	  /*if (rset->res->method == TIMESTORE || 
	        rset->res->method == TABLESTORE) {*/
	  if (pipe(respipefd) == -1) {
	       elog_printf(ERROR, "job %s: unable to pipe stdout; "
			   "abandon", key);
	       if (rset->oneshot)
		    meth_endrun(key,run,command,res_purl,err_purl,keep);
	       nfree(rp);
	       return 2;
	  }
	  fcntl(respipefd[0], F_SETFL, O_NONBLOCK);
	  rp->resfd = respipefd[0];	/* 0 is the reading end */
	  /*}*/
	  /*if (rset->err->method == TIMESTORE || 
	        rset->err->method == TABLESTORE) {*/
	  if (pipe(errpipefd) == -1) {
	       elog_printf(ERROR, "job %s: unable to pipe stderr; "
			   "abandon", key);
	       /*if (rset->res->method == TIMESTORE || 
		     rset->res->method == TABLESTORE) {*/
	       close(respipefd[0]);
	       close(respipefd[1]);
	       /*}*/
	       if (rset->oneshot)
		    meth_endrun(key,run,command,res_purl,err_purl,keep);
	       nfree(rp);
	       return 3;
	       /*}*/
	       fcntl(errpipefd[0], F_SETFL, O_NONBLOCK);
	       rp->errfd = errpipefd[0];	/* 0 is the reading end */
	  }

	  elog_printf(DEBUG, "fork job %s stdout fd %d stderr fd %d", 
		      key, rp->resfd, rp->errfd);

	  if ( (pid=fork()) ) {

	       /*  =====  parent  =====  */

	       if (pid == -1) {
		    elog_printf(ERROR, "unable to fork(), error %d %s", 
				errno, strerror(errno));
		    /* close all pipes */
		    close(respipefd[0]);
		    close(respipefd[1]);
		    close(errpipefd[0]);
		    close(errpipefd[1]);
		    if (rset->oneshot)
			 meth_endrun(key,run,command,res_purl,err_purl,keep);
		    nfree(rp);
		    return -1;
	       }

	       elog_printf(DEBUG, "job %s pid %d", key, pid);

	       /* Add process to run set and process lists */
	       rp->pid = pid;
	       itree_add(meth_procbypid, pid, rp);
	       rset->pid = pid;

	       /* Close off unnecessary fd's in parent */
	       /* writing end of pipes */
	       close(respipefd[1]);
	       close(errpipefd[1]);

	  } else {

	       /*  =====  child  =====  */

	       /* 
		* Redirect stdin, stdout and stderr as directed by
		* the settings in rp->method.
		*/
	       /* redirect stdout to pipe */
	       if (dup2(respipefd[1], 1) != 1)
		    elog_die(FATAL, 
			     "METH_FORK TIME/TABLESTORE "
			     "can't dup2() stdout command `%s' %d %s",
			     command, errno, strerror(errno));
	       close(respipefd[0]);
	       close(respipefd[1]);

	       /* redirect stderr to pipe */
	       if (dup2(errpipefd[1], 2) != 2)
		    elog_die(FATAL, 
			     "METH_FORK TIME/TABLESTORE "
			     "can't dup2() stderr command `%s' %d %s",
			     command, errno, strerror(errno));
	       close(errpipefd[0]);
	       close(errpipefd[1]);

	       /* 
		* Run the method and return its return code as an exit,
		* which is picked up by wait(). Dont use exit() as it may
		* contain atexit() and on_exit() callbacks, such as nmalloc 
		* or gtk.
		* The gtk one is documented to shut down related windows.
		*/

	       _exit((*run->action)(command, rset->res, rset->err, rset));

	  } /* End of child code */

	  break;
     case METH_THREAD:
	  /* Use thread primatives */
          elog_send(ERROR, "thread method not supported");
	  return 1;
     case METH_SOURCE:
	  /* run inside this thread and process. 'in-proc' */
#if 0
	  elog_printf(DEBUG, "source job %-10s stdout fd %d stderr fd %d", 
		      key, rset->res->open.fd, rset->err->open.fd);
#endif

	  /* run the method and pick up its return code */
	  r = (*run->action)(command, rset->res, rset->err, rset);

	  /* log the return */
	  if (r)
	       elog_printf(ERROR, "source job %-10s failure (%d)", key, r);
	  else
	       elog_printf(INFO,  "source job %-10s success (%d)", key, r);

	  /* flush I/O and close off if we have to */
	  route_flush(rset->res);
	  route_flush(rset->err);
	  if (rset->oneshot)
	       meth_endrun(key,run,command,res_purl,err_purl,keep);

	  return r;
     case METH_NONE:
	  /* no method defined */
	  elog_printf(DEBUG, "no method for job %s", key);
	  if (rset->oneshot)
	       meth_endrun(key,run,command,res_purl,err_purl,keep);
	  return 0;
     }

     return 0;
}


/* 
 * Simplified, alternative version of method running for stand-alone
 * utilities that run one thing at a time.
 * Control is handed over to method for the duration of its execution
 * ignoring METHID->type, persistant routes and without naming.
 * Returns the method's value or 1 if unable to start the method
 */
int meth_actiononly(METHID run,		/* method specification */
		    char *command,	/* command string */
		    char *res_purl,	/* route to output results */
		    char *err_purl,	/* route to output errors */
		    long keep		/* number of recent data to keep */ )
{
     ROUTE out, err;
     int r;

     /* open routes */
     out = route_open(res_purl, command, NULL, keep);
     err = route_open(err_purl, command, NULL, keep);
     if (!out || !err) {
	  elog_printf(FATAL, "unable to open one or more routes:-\n"
		      "out: %s\nerr: %s", res_purl, err_purl);
	  return 1;
     }
     /* preaction */
     if (run->preaction) {
	  r = (*run->preaction)(command);
	  if (r) {
	       elog_printf(ERROR, "method preparation failed");
	       elog_printf(DEBUG, "method preaction() returns %d", r);
	  }
     }

     /* action */
     r = (*run->action)(command, out, err, NULL);
     if (r) {
          elog_printf(ERROR, "method failed");
	  elog_printf(DEBUG, "method action() returns %d", r);
     }

     /* close routes */
     route_close(out);
     route_close(err);

     return 0;
}


/* Alternative signature for meth_endrun() */
int  meth_endrun_s(struct meth_invoke *args, 	/* argument structure */
		   int argl			/* size of structure */)
{
     return meth_endrun(args->key, args->run, args->command, 
			args->res_purl, args->err_purl, args->keep);
}


/*
 * Close the I/O route for this job.
 * Called with the same arguments as meth_execute() above.
 * Returns -1 for error, such as the process was still running
 * or any other code for success.
 */
int  meth_endrun(char *key, METHID run, char *command, char *res_purl, 
		  char *err_purl, long keep)
{
     struct meth_runset *rset;
     int ret;

     /* Debug: log close of io */
     elog_printf(DEBUG, "end-of-run for %s method %s command `%s' closing "
		 "routes: results %s errors %s",
		 key, meth_name(run), command, res_purl, err_purl);

     if (meth_isrunning(key))
	  return -1;

     /* get the runset structure */
     rset = tree_find(meth_rsetbykey, key);
     if (rset == TREE_NOVAL) {
	  elog_printf(ERROR, "unable to find runset/open routes for key %s", 
		      key);
	  return -1;
     }

     /* call method end-of-run function */
     if (run && run->postrun) {
	  ret = (run->postrun)(command, rset->res, rset->err, rset);
	  if (ret)
	       elog_printf(ERROR, "job %s afterrun() returns %d", key, ret);
     } else
	  ret = 0;

     /* remove rset structure, close the IO and free memory */
     elog_printf(DEBUG, "closing routes for job key "
		 "%s after %d seconds res %s err %s", key, 
		 time(NULL)-rset->opened, rset->res_purl, rset->err_purl);
     nfree(tree_getkey(meth_rsetbykey));
     route_close(rset->res);
     route_close(rset->err);
     nfree(rset->res_purl);
     nfree(rset->err_purl);
     nfree(rset);
     tree_rm(meth_rsetbykey);

     return ret;
}


/* Alternative signature for meth_isrunning() */
int  meth_isrunning_s(struct meth_invoke *args,	/* argument structure */
		      int argl			/* size of structure */)
{
     return meth_isrunning(args->key);
}


/* Check for a running method identified by `key': 1=running 0=not */
int  meth_isrunning(char *key		/* string key */)
{
     struct meth_runset *rset;

     rset = tree_find(meth_rsetbykey, key);
     if (rset == TREE_NOVAL)
	  return 0;		/* not running */

     if (rset->pid == -1)
	  return 0;
     else
	  return 1;
}


/*
 * Signal handler called when a child process's status changes.
 * Suspends/restarts are ignored, but exits are collected and queued
 * for later processing in meth_childexit() called from the main loop
 */
void meth_sigchild(int sig /* signal vector */) {
     int pid, status;

     sig_off();		/* I'm working */
     while( (pid = waitpid(-1, &status, WNOHANG)) ) {

          /* special statuses */
	  if (pid == -1) {
	       /* interrupted waitpid call */
	       if (errno == EINTR) {
		    continue;
	       }
	       sig_on();
	       return;
	  }

	  /* ignore stopped processes */
	  if ( WIFSTOPPED(status) ) {
	       /*elog_printf(DEBUG, "process %d stopped", pid);*/
	       continue;
	  }

	  /* At this point, a process has terminated normally or from an
	   * uncaught signal. Either way, its dead */

          /* add status and pid in an ITREE making an unordered list */
          itree_add(meth_exitbypid, pid, (void *) status);
     }
     sig_on();
}


/*
 * Process child exit statuses that have been collected by meth_sigchild().
 * Essentially, flushing the I/O buffers of exited methods, removing the 
 * running method structures and logging the progress for information 
 * and debug.
 */
void meth_exitchildren() {
     int pid, status, r;
     char pipebuf[PIPE_BUF];
     struct meth_runprocinfo *rp;
     struct meth_runset *rset;

     /* traverse the exited children list */
     while ( ! tree_empty(meth_exitbypid) ) {
	  itree_first(meth_exitbypid);
	  pid    = itree_getkey(meth_exitbypid);
	  status = ((long long) itree_get(meth_exitbypid) & 0xffff);
       
      meth_restartselect++;		/* select args may have changed */

	  /* Process the child's death */
	  if (itree_find(meth_procbypid, pid) != ITREE_NOVAL) {
	       rp = itree_get(meth_procbypid);
	       rset = tree_find(meth_rsetbykey, rp->key);
	       if (rset == ITREE_NOVAL)
		    elog_die(FATAL, "method key %s not in meth_rsetbykey", 
			     rp->key);

	       /* log the death */
	       elog_startprintf(INFO, "  fork job %-10s pid %d: ",
				rp->key, pid);
	       if (WIFEXITED(status))
		    elog_contprintf(INFO, " exit=%d ", WEXITSTATUS(status));
	       else if (WIFSIGNALED(status))
		    elog_contprintf(INFO, " signal=%d %s ", WTERMSIG(status),
				    strsignal(WTERMSIG(status)));
	       else
		    elog_contprintf(INFO, " UNKNOWN KILL ");
	       elog_endprintf(INFO, " took=%.0fs", difftime(time(NULL), 
							    rp->start));

	       /* close off i/o that the parent may have for the child */

	       /* Empty the result pipe containing data */
	       if (rp->resfd != -1) {
		    r = read(rp->resfd, pipebuf, PIPE_BUF);
		    if (r == -1)
		         elog_printf(ERROR, "result read() error: %d %s",
				     errno, strerror(errno));
		    if (r > 0)
		         if (route_write(rset->res, pipebuf, r) < 0)
			      elog_die(FATAL, "res route problem: "
				       "key %s, start %d res %s err %s",
				       rp->key, rp->start,
				       rset->res_purl, rset->err_purl);
		    close(rp->resfd);
	       }

	       /* Empty the error pipe containing data */
	       if (rp->errfd != -1) {
		    r = read(rp->errfd, pipebuf, PIPE_BUF);
		    if (r == -1)
		         elog_printf(ERROR, "error read() error: %d %s", 
				     errno, strerror(errno));
		    if (r > 0)
		         if (route_write(rset->err, pipebuf, r) < 0)
			   elog_die(FATAL, "err route problem: "
				    "key %s, start %d res %s err %s",
				    rp->key, rp->start,
				    rset->res_purl, rset->err_purl);
		    close(rp->errfd);
	       }
	       
	       route_flush(rset->res);
	       route_flush(rset->err);
	       
	       if (rset->oneshot)
		    meth_endrun(rp->key, 0, "unknown", rset->res_purl, 
				rset->err_purl, 0);

	       /* remove the pid refrence from the list of those running */
	       rset->pid = -1;
	       itree_rm(meth_procbypid);

	       /* propagate the death to anyone listening on the 
		* METH_CB_FINISHED event and pass the method name key */
	       callback_raise(METH_CB_FINISHED, rp->key, NULL, NULL, NULL);

	       /* clear the storage */
	       nfree(rp->key);
	       nfree(rp);

	  } else {
	       /* process does not have an entry in meth_procbypid;
		* log the death */
	       elog_startprintf(ERROR, "unknown process pid %d ", pid);
	       if (WIFEXITED(status))
		    elog_contprintf(ERROR,"exit with %d",WEXITSTATUS(status));
	       else if (WIFSIGNALED(status))
		    elog_contprintf(ERROR, "killed by signal %d", 
				    WTERMSIG(status));
	       else
		    elog_contprintf(ERROR, "UNKNOWN DEATH");
	       elog_endprintf(ERROR, " finished at %d", time(NULL));
	  }

	  /* remove processed item from list */
	  itree_rm(meth_exitbypid);
     }

     return;
}



#if 0
/*
 * Signal handler called when a child process dies.
 * It is assumed that the signal returns the status of 0 or more children
 * and a search is carried out to see what other children have finished.
 * Status is interpreted and reported on.
 */
void meth_child(int sig /* signal vector */) {
     int pid, status, r;
     char pipebuf[PIPE_BUF];
     struct meth_runprocinfo *rp;
     struct meth_runset *rset;

     /* 
      * Prevent SIGALRM from interrupting us with dispatch() as 
      * we share some data structures.
      */
     sig_off(); /* handler should disable */

     while( (pid = waitpid(-1, &status, WNOHANG)) ) {
          meth_restartselect++;		/* select args may have changed */

	  /* retry if error, prob interrupted syscall */
	  if (pid == -1) {
	       /* no children to wait for!!
		* i don't think it should be an error though */
	       if (errno == ECHILD) {
		    elog_send(DEBUG, "no more children");
		    sig_on();
		    return;
	       }

	       /* interrupted waitpid call */
	       if (errno == EINTR) {
		    elog_send(DEBUG,"interrupted waitpid()");
		    continue;
	       }

	       /* all other errors */
	       elog_printf(ERROR, "waitpid returns error %d %s", errno, 
			   strerror(errno));
	       sig_on();
	       return;
	  }

	  /* ignore stopped processes */
	  if ( WIFSTOPPED(status) ) {
	       elog_printf(DEBUG, "process %d stopped", pid);
	       continue;
	  }

	  /* At this point, a process has terminated normally or from an
	   * uncaught signal. Either way, its dead */

	  /* Process the child's death */
	  if (itree_find(meth_procbypid, pid) != ITREE_NOVAL) {
	       rp = itree_get(meth_procbypid);
	       rset = tree_find(meth_rsetbykey, rp->key);
	       if (rset == ITREE_NOVAL)
		    elog_die(FATAL, "key %s not in meth_rsetbykey", 
			     rp->key);

	       /* log the death */
	       elog_startprintf(INFO, "  fork job %-10s pid %d: ",
				rp->key, pid);
	       if (WIFEXITED(status))
		    elog_contprintf(INFO, " exit=%d ", WEXITSTATUS(status));
	       else if (WIFSIGNALED(status))
		    elog_contprintf(INFO, " signal=%d %s ", WTERMSIG(status),
				    strsignal(WTERMSIG(status)));
	       else
		    elog_contprintf(INFO, " UNKNOWN KILL ");
	       elog_endprintf(INFO, " took=%.0fs", difftime(time(NULL), 
							    rp->start));

	       /* close off i/o that the parent may have for the child */

	       /* Empty the result pipe containing data */
	       if (rp->resfd != -1) {
		    r = read(rp->resfd, pipebuf, PIPE_BUF);
		    if (r == -1)
		         elog_printf(ERROR, "result read() error: %d %s",
				     errno, strerror(errno));
		    if (r > 0)
		         if (route_write(rset->res, pipebuf, r) < 0)
			      elog_die(FATAL, "res route problem: "
				       "key %s, start %d res %s err %s",
				       rp->key, rp->start,
				       rset->res_purl, rset->err_purl);
		    close(rp->resfd);
	       }

	       /* Empty the error pipe containing data */
	       if (rp->errfd != -1) {
		    r = read(rp->errfd, pipebuf, PIPE_BUF);
		    if (r == -1)
		         elog_printf(ERROR, "error read() error: %d %s", 
				     errno, strerror(errno));
		    if (r > 0)
		         if (route_write(rset->err, pipebuf, r) < 0)
			   elog_die(FATAL, "err route problem: "
				    "key %s, start %d res %s err %s",
				    rp->key, rp->start,
				    rset->res_purl, rset->err_purl);
		    close(rp->errfd);
	       }
	       
	       route_flush(rset->res);
	       route_flush(rset->err);
	       
	       if (rset->oneshot)
		    meth_endrun(rp->key, 0, "unknown", rset->res_purl, 
				rset->err_purl, 0);

	       /* remove the pid refrence from the list of those running */
	       rset->pid = -1;
	       itree_rm(meth_procbypid);

	       /* propagate the death */
	       callback_raise(METH_CB_FINISHED, rp->key, NULL, NULL, NULL);

	       /* clear the storage */
	       nfree(rp->key);
	       nfree(rp);

	  } else {
	       /* process does not have an entry in meth_procbypid;
		* log the death */
	       elog_startprintf(ERROR, "unknown process pid %d ", pid);
	       if (WIFEXITED(status))
		    elog_contprintf(ERROR,"exit with %d",WEXITSTATUS(status));
	       else if (WIFSIGNALED(status))
		    elog_contprintf(ERROR, "killed by signal %d", 
				    WTERMSIG(status));
	       else
		    elog_contprintf(ERROR, "UNKNOWN DEATH");
	       elog_endprintf(ERROR, " finished at %d", time(NULL));
	  }
     }
     sig_on(); /* handler should enable */

     return;
}
#endif


/*
 * Dispatch I/O relay and process servicing.
 * Calling this routine enables the results and errors of completed jobs 
 * to be collected, scheduals new ones and coordinates i/o from jobs 
 * with non-file descriptor routes jobs (eg timstore).
 * [The job uses file descriptors, meth_relay turns it into a route]
 * If no asynchronous events occour within METH_RELAY_TOSEC seconds and
 * METH_RELAY_TOUSEC microseconds, 0 is returned indicating that no work
 * was done.
 * Returns -1 to indicate meth_relay() should be rerun as signals had
 * interrupted I/O. Otherwise, the number of relays carried out is returned.
 * Should be run in a loop.
 */
int meth_relay() {
     struct meth_runprocinfo *rp;
     struct meth_runset *rset;
     struct timeval timeout;
     char pipebuf[PIPE_BUF+1];
     int i, r, avail, handled, highestfd=0;
     fd_set fds;
     char buf[1025];
     int blen=0;

     /* This routine uses select(), which is a Unix command so this will 
      * need to be modified when ported to Windows */

     /* Construct a list of file descriptors for select() from running
      * processes (meth_procbypid) and relay fds (meth_cbbyfd) */
     blen = sprintf(buf, "selecting on fds: ");
     FD_ZERO(&fds);
     itree_traverse(meth_procbypid) {
	  rp = itree_get(meth_procbypid);
	  rset = tree_find(meth_rsetbykey, rp->key);
	  if (rset == ITREE_NOVAL)
	       elog_die(FATAL, "key %s not in meth_rsetbykey", rp->key);
	  if (rp->resfd != -1) {
	       blen += sprintf(buf+blen, "%d ", rp->resfd);
	       FD_SET(rp->resfd, &fds);
	       if (rp->resfd > highestfd)
		    highestfd = rp->resfd;
	  }
	  if (rp->errfd != -1) {
	       blen += sprintf(buf+blen, "%d ", rp->errfd);
	       FD_SET(rp->errfd, &fds);
	       if (rp->errfd > highestfd)
		    highestfd = rp->errfd;
	  }
     }
     /* callback file descriptors */
     itree_traverse(meth_cbbyfd) {
	  blen += sprintf(buf+blen, "%d ", itree_getkey(meth_cbbyfd));
	  FD_SET(itree_getkey(meth_cbbyfd), &fds);
	  if (itree_getkey(meth_cbbyfd) > highestfd)
	       highestfd = itree_getkey(meth_cbbyfd);
     }
     elog_send(DEBUG, buf);

     timeout.tv_sec = METH_RELAY_TOSEC;
     timeout.tv_usec = METH_RELAY_TOUSEC;

     sig_on();

     handled = avail = select(++highestfd, &fds, NULL, NULL, &timeout);

     sig_off();

     /* work out the fds that were changed */
     if ( avail == -1 ) {
          if (errno == EBADF || errno == EINTR) {
	       elog_startprintf(DEBUG, "select() error %d %s ( ", 
				errno, strerror(errno));
	       for (i=0; i<highestfd; i++)
		    if (FD_ISSET(i, &fds))
		         elog_contprintf(DEBUG, "%d ", i);
	       elog_endprintf(DEBUG, ")");
	  } else {
	       elog_startprintf(ERROR, "select() error %d %s ( ", 
				errno, strerror(errno));
	       for (i=0; i<highestfd; i++)
		    if (FD_ISSET(i, &fds))
		         elog_contprintf(ERROR, "%d ", i);
	       elog_endprintf(ERROR, ")");
	  }
	  return avail;
     }

     /* process the child exit codes */
     meth_exitchildren();

     /* meth_child() has been called, causing the select() results to
      * be possibly incorrect (the orphan catching code below). Restart
      * ourselves just to be sure */
     if (meth_restartselect) {
          meth_restartselect = 0;
	  return -1;			/* needs to be restarted */
     }
     if ( avail == 0 )
          return avail;			/* needs to be restarted */

     /* list available fds to read */
     elog_startsend(DEBUG, "fds to read: ");
     for (i=0; i<highestfd; i++)
          if (FD_ISSET(i, &fds))
	       elog_contprintf(DEBUG, "%d ", i);
     elog_endprintf(DEBUG, "");

     /*
      * The tree meth_procbypid may have been altered by the signals
      * allowed at the preemption point above. If an item is added
      * it will not be in the original fd_set: no problem. If an item
      * is removed, any pending input will be lost: thus, the signal
      * handler (SIGCHLD => meth_child()) should collect any data and
      * redirect before closing the pipe. In any case, using 
      * meth_procbypid is the most correct way of doing things.
      */
     itree_traverse(meth_procbypid) {
	  rp = itree_get(meth_procbypid);
	  rset = tree_find(meth_rsetbykey, rp->key);
	  if (rp->resfd != -1 && FD_ISSET(rp->resfd, &fds)) {
	       /* read from result (stdout) pipe */
	       FD_CLR(rp->resfd, &fds);
	       avail--;
	       r = read(rp->resfd, pipebuf, PIPE_BUF);
	       elog_printf(DEBUG, "read job %s result fd %d nchars %d", 
			   rp->key, rp->resfd, r);
	       if (r == -1) {
		    elog_printf(ERROR, "read() error %d %s", errno, 
				strerror(errno));
	       } if (r == 0) {
		    elog_printf(DEBUG, "closing job %s result fd %d", 
				rp->key, rp->resfd);
		    close(rp->resfd);
		    rp->resfd = -1;
	       } else {
		    if (route_write(rset->res, pipebuf, r) < 0)
		         elog_die(FATAL, "route problem from "
				  "res: key %s, start %d res %s err %s",
				  rp->key,rp->start,rset->res_purl,rset->err_purl);
               }
	  }
	  if (rp->errfd != -1 && FD_ISSET(rp->errfd, &fds)) {
	       /* read from error (stderr) pipe */
	       FD_CLR(rp->errfd, &fds);
	       avail--;
	       r = read(rp->errfd, pipebuf, PIPE_BUF);
	       elog_printf(DEBUG, "read job %s error fd %d nchars %d", 
			   rp->key, rp->errfd, r);
	       if (r == -1) {
		    elog_printf(ERROR, "read() error %d %s", errno, 
				strerror(errno));
	       } if (r == 0) {
		    elog_printf(DEBUG, "closing job %s error fd %d", 
				rp->key, rp->errfd);
		    close(rp->errfd);
		    rp->errfd = -1;
	       } else {
		    if (route_write(rset->err, pipebuf, r) < 0)
		         elog_die(FATAL, "route problem from err: "
				  "key %s, start %d res %s err %s",
				  rp->key,rp->start,rset->res_purl,rset->err_purl);
               }
	  }
     }
     if (avail > 0) {
	  for (i=0; i<highestfd; i++) {
	       if (FD_ISSET(i, &fds)) {
		    if (itree_find(meth_cbbyfd, i) == ITREE_NOVAL) {
			 /* orphaned file descriptor - clear it */
			 r = read(i, pipebuf, PIPE_BUF);
			 if (r == -1)
			      sprintf(pipebuf, "<empty>");
			 else
			      pipebuf[r] = '\0';
			 elog_printf(ERROR, "orphaned fd %d contents %s", i, 
				     pipebuf);
		    } else {
			 /* externally notified file descriptor */
			 callback_raise(itree_get(meth_cbbyfd), 
					(void *) (long) i, NULL, NULL, NULL);
			 /* implementation note
			  * Potentially return here, or count a few times
			  * and return. We don't want to take time from
			  * the real methods */
		    }
	       }
	  }
     }
     

     return handled;
}

/*
 * Send a kill signal to a process spawned by meth_ and let it handle
 * its own shutdown.
 */
void meth_kill(struct meth_runprocinfo *rp) {
     elog_printf(INFO, "shutting down job %s (pid %d)", rp->key, rp->pid);

     if (kill(rp->pid, METH_SIG_KILL) < 0)
	  /* failure to kill, leave it */
          elog_printf(ERROR, "unable to kill pid %d, error %d %s", 
		      rp->pid, errno, strerror);
}

/*
 * Send a non-catchable kill signal to a process spawned by meth_,
 * clear up any problems, flush and close its buffers.
 */
void meth_butcher(struct meth_runprocinfo *rp) {
     elog_printf(WARNING, "aborting job %s (pid %d)", rp->key, rp->pid);
     if (kill(rp->pid, METH_SIG_BUTCHER) < 0)
	  /* failure to kill, leave it */
          elog_printf(ERROR, "unable to kill pid %d, error %d %s", 
		      rp->pid, errno, strerror);
}

/*
 * Send signals to all running proceses associcated with methods.
 * This does not remove methods, which is done with meth_fini().
 * The first cycle kills, after a timeout the second cycle sends an 
 * uninterruptable kill (meth_butcher()).
 * Conceivably, there will be methods that will not be shut down
 * as meth_ does not know about them.
 * Returns 0 if successful or the number of jobs that had to be shutdown
 * inelegently if unsuccessful.
 */
int meth_shutdown() {
     struct meth_runprocinfo *rp;
     struct timespec timeout;
     struct timespec remain;
     int r, carnage=-1;

     elog_send(INFO, "Starting shutdown");

     if ( itree_n(meth_procbypid) == 0 )
          return 0;		/* no work to do */

     /* attempt to shutdown elegently */
     itree_traverse(meth_procbypid) {
          rp = itree_get(meth_procbypid);
	  meth_kill(rp);
     }

     /* wait for all the exits to come through */
     remain.tv_sec = METH_SHUT_KILLSEC;
     remain.tv_nsec = METH_SHUT_KILLUSEC;
     do {
          timeout.tv_sec  = remain.tv_sec;
          timeout.tv_nsec = remain.tv_nsec;

          sig_on();
	  r = nanosleep(&timeout, &remain);
	  sig_off();

	  if (r == -1 && errno != EINTR) {
	       elog_printf(ERROR, "unable to nanosleep() after kill, "
			   "error %d %s", errno, strerror);
	       break;		/* problem! start butchering now */
	  }

	  if (itree_n(meth_procbypid) == 0)
	       return 0;	/* job completed */

     } while (r != 0);

     /* shutdown roughly as some would not die */
     elog_printf(WARNING, "%d jobs remain after %d.%09d seconds",
		 carnage = itree_n(meth_procbypid), METH_SHUT_KILLSEC, 
		 METH_SHUT_KILLUSEC);
     itree_traverse(meth_procbypid) {
          rp = itree_get(meth_procbypid);
	  meth_butcher(rp);
     }

     /* wait for all the butchered exits to come through */
     remain.tv_sec = METH_SHUT_BUTCHERSEC;
     remain.tv_nsec = METH_SHUT_BUTCHERUSEC;
     do {
          timeout.tv_sec  = remain.tv_sec;
          timeout.tv_nsec = remain.tv_nsec;

          sig_on();
	  r = nanosleep(&timeout, &remain);
	  sig_off();

	  if (r == -1 && errno != EINTR) {
	       elog_printf(ERROR, "unable to nanosleep() after butchering, "
			   "error %d %s", errno, strerror);
	       break;		/* problem! nothing more can be done */
	  }

	  if (itree_n(meth_procbypid) == 0)
	       break;		/* job completed */

     } while (r != 0);

     return carnage;		/* number of jobs that wouldn't go quietly */
}


#if TEST
#include "rt_file.h"
#include "rt_std.h"

int main(int argc, char **argv) {
     struct meth_info *runthis;
     int r;
     ROUTE err;

     route_init(NULL, 0);
     route_register(&rt_filea_method);
     route_register(&rt_fileov_method);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     if ( ! elog_init(1, "meth test", NULL))
	  elog_die(FATAL, "didn't initialise elog\n");
     err = route_open("stderr:", NULL, NULL, 0);
     sig_init();
     callback_init();

     /* Simple test is to load the default `exec' action then run it! */
     if (meth_load("./t.meth.exec.so")) {
	  fprintf(stderr, "Unable to load method ./t.meth.exec.so\n");
	  exit(1);
     }
     if (meth_check("t.exec")) {
	  fprintf(stderr, "Can't check t.exec method\n");
	  exit(1);
     }
     runthis = meth_lookup("t.exec");
     if (runthis == NULL) {
	  fprintf(stderr, "Can't lookup t.exec method\n");
	  exit(1);
     }
     r = meth_execute("test1", runthis, 
		      "echo \"If you can read this, its probably working\"", 
		      "stdout", "stderr", 10);
     if (r)
	  fprintf(stderr, "run method returned %d. Continuing...\n", r);

     /* Simple test is to load the `sh' action then run it! */
     if (meth_load("./t.meth.sh.so")) {
	  fprintf(stderr, "Unable to load method ./t.sh.\n");
	  exit(1);
     }
     if (meth_check("t.sh")) {
	  fprintf(stderr, "Can't check t.sh method\n");
	  exit(1);
     }
     runthis = meth_lookup("t.sh");
     if (runthis == NULL) {
	  fprintf(stderr, "Can't lookup t.sh method\n");
	  exit(1);
     }
     r = meth_execute("test2", runthis, 
		      "echo \"If you can read this, its probably working\"", 
		      "stdout", "stderr", 10);
     if (r)
	  fprintf(stderr, "run method returned %d. Continuing...\n", r);

     /* Simple test is to run the builtin `exec' action */
     if (meth_check("exec")) {
	  fprintf(stderr, "Can't check exec method\n");
	  exit(1);
     }
     runthis = meth_lookup("exec");
     if (runthis == NULL) {
	  fprintf(stderr, "Can't lookup exec method\n");
	  exit(1);
     }
     r = meth_execute("test3", runthis, 
		      "echo \"If you can read this, its probably working\"", 
		      "stdout", "stderr", 10);
     if (r)
	  fprintf(stderr, "run method returned %d. Continuing...\n", r);

     /* Simple test is to run the builtin `exec' action */
     if (meth_check("sh")) {
	  fprintf(stderr, "Can't check sh method\n");
	  exit(1);
     }
     runthis = meth_lookup("sh");
     if (runthis == NULL) {
	  fprintf(stderr, "Can't lookup sh method\n");
	  exit(1);
     }
     r = meth_execute("test4", runthis, 
		      "echo \"If you can read this, its probably working\"", 
		      "stdout", "stderr", 10);
     if (r)
	  fprintf(stderr, "run method returned %d. Continuing...\n", r);

     meth_relay();
     sleep(1);

     /* test to kill running methods */
     runthis = meth_lookup("exec");
     if (runthis == NULL) {
	  fprintf(stderr, "Can't lookup exec method\n");
	  exit(1);
     }
     r = meth_execute("test5", runthis, "sleep 10", "stdout", "stderr", 10);
     if (r)
	  fprintf(stderr, "run method returned %d. Continuing...\n", r);
     sleep(1);
     r = meth_shutdown();
     if (r)
          fprintf(stderr, "had to butcher %d jobs\n", r);

     meth_fini();
     elog_fini();
     route_close(err);
     route_fini();
     callback_fini();

     printf("%s: tests finished successfully\n", argv[0]);
     exit(0);
}

#endif /* TEST */
