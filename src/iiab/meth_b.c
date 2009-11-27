/*
 * Builtin methods for the meth class.
 * Nigel Stuckey, January 1988, based on code from August 1996
 * Modifications November 1999 and March 2000
 *
 * Copyright System Garden Ltd 1996-2001, all rights reserved.
 */

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include "nmalloc.h"
#include "cf.h"
#include "iiab.h"
#include "meth_b.h"
#include "cascade.h"
#include "pattern.h"
#include "record.h"
#include "event.h"
#include "rep.h"

/* Manual link to builtin methods */
struct meth_info meth_builtins[]= { 
     /* exec method */
     { meth_builtin_exec_id,		/* method id */
       meth_builtin_exec_info,		/* text description */
       meth_builtin_exec_type,		/* one of METH_{SOURCE,FORK,THREAD} */
       NULL,				/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_exec_action,	/* action - JFDI!*/
       NULL,				/* end of run finalisation */
       NULL				/* name of shared library */ },
     /* shell method */
     { meth_builtin_sh_id,		/* method id */
       meth_builtin_sh_info,		/* text description */
       meth_builtin_sh_type,		/* one of METH_{SOURCE,FORK,THREAD} */
       NULL,				/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_sh_action,		/* action - JFDI!*/
       NULL,				/* end of run finalisation */
       NULL				/* name of shared library */ },
     /* snapshot method */
     { meth_builtin_snap_id,		/* method id */
       meth_builtin_snap_info,		/* text description */
       meth_builtin_snap_type,		/* one of METH_{SOURCE,FORK,THREAD} */
       NULL,				/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_snap_action,	/* action - JFDI!*/
       NULL,				/* end of run finalisation */
       NULL				/* name of shared library */ },
     /* timestamp method */
     { meth_builtin_tstamp_id,		/* method id */
       meth_builtin_tstamp_info,	/* text description */
       meth_builtin_tstamp_type,	/* one of METH_{SOURCE,FORK,THREAD} */
       NULL,				/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_tstamp_action,	/* action - JFDI!*/
       NULL,				/* end of run finalisation */
       NULL				/* name of shared library */ },
     /* sample method */
     { meth_builtin_sample_id,		/* method id */
       meth_builtin_sample_info,	/* text description */
       meth_builtin_sample_type,	/* one of METH_{SOURCE,FORK,THREAD} */
       meth_builtin_sample_init,	/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_sample_action,	/* action - JFDI!*/
       meth_builtin_sample_fini,	/* end of run finalisation */
       NULL				/* name of shared library */ },
     /* pattern method */
     { meth_builtin_pattern_id,		/* method id */
       meth_builtin_pattern_info,	/* text description */
       meth_builtin_pattern_type,	/* one of METH_{SOURCE,FORK,THREAD} */
       meth_builtin_pattern_init,	/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_pattern_action,	/* action - JFDI!*/
       meth_builtin_pattern_fini,		/* end of run finalisation */
       NULL				/* name of shared library */ },
#if 0
     /* record method */
     { meth_builtin_record_id,		/* method id */
       meth_builtin_record_info,	/* text description */
       meth_builtin_record_type,	/* one of METH_{SOURCE,FORK,THREAD} */
       meth_builtin_record_init,	/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_record_action,	/* action - JFDI!*/
       meth_builtin_record_fini,	/* end of run finalisation */
       NULL				/* name of shared library */ },
#endif
     /* event method */
     { meth_builtin_event_id,		/* method id */
       meth_builtin_event_info,		/* text description */
       meth_builtin_event_type,		/* one of METH_{SOURCE,FORK,THREAD} */
       meth_builtin_event_init,		/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_event_action,	/* action - JFDI!*/
       meth_builtin_event_fini,		/* end of run finalisation */
       NULL				/* name of shared library */ },
     /* replicate method */
     { meth_builtin_rep_id,		/* method id */
       meth_builtin_rep_info,		/* text description */
       meth_builtin_rep_type,		/* one of METH_{SOURCE,FORK,THREAD} */
       NULL,				/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_rep_action,		/* action - JFDI!*/
       NULL,				/* end of run finalisation */
       NULL				/* name of shared library */ },
     /* restart method */
     { meth_builtin_restart_id,		/* method id */
       meth_builtin_restart_info,	/* text description */
       meth_builtin_restart_type,	/* one of METH_{SOURCE,FORK,THREAD} */
       NULL,				/* start of run initialisation */
       NULL,				/* pre-action call */
       meth_builtin_restart_action,	/* action - JFDI!*/
       NULL,				/* end of run finalisation */
       NULL				/* name of shared library */ },
     {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}	/* End of builtins */
};

/* ----- builtin exec method ----- */
char *meth_builtin_exec_id() { return "exec"; }
char *meth_builtin_exec_info() { return "Direct submission to exec(2)"; }
enum exectype meth_builtin_exec_type() { return METH_FORK; }

/* 
 * This method runs things directly into exec after some packaging of
 * its command string into an argv style list.
 * Returns -1 if there was an error with the exec.
 */
int meth_builtin_exec_action(char *command, ROUTE output, ROUTE error, 
			     struct meth_runset *rset) {
	char **argv;
	char *args;
	int i;

	/*printf("** Arguments are: ");*/

	if ( !command || !*command) {
	     elog_printf(ERROR, "no command supplied - use exec <command>");
	     return -1;
	}

	/* Vectorise arguments! Warning: it ignores quoted strings */
	args = xnstrdup(command);
	argv = xnmalloc(100 * sizeof(char *));	/* Assume max of 100 args */
	i = 0;
	argv[i++] = strtok(args, " ");
	while((argv[i] = strtok(NULL, " ")) != NULL) {
	     /*printf("arg[%d] = %s", i, argv[i]);*/
	     i++;
	}
	argv[i] = NULL;

	execvp(argv[0], argv);
	/*perror("exec method action(), execp failure");*/
	return -1;
}


/* ----- builtin sh method ----- */
char *meth_builtin_sh_id() { return "sh"; }
char *meth_builtin_sh_info() { return "Test submit command line to sh(1)"; }
enum exectype meth_builtin_sh_type() { return METH_FORK; }

/* 
 * This method runs things by passing the command to a shell
 * Returns -1 if there was an error with the exec.
 */
int meth_builtin_sh_action(char *command) {
	if (command == NULL)
	     return -1;

	execl("/bin/sh", "", "-c", command, NULL);
	return -1;				/* Should never be reached */
}


/* ----- builtin snap (snapshot) method ----- */
char *meth_builtin_snap_id() { return "snap"; }
char *meth_builtin_snap_info() { return "Take a snapshot of a route"; }
enum exectype meth_builtin_snap_type() { return METH_SOURCE; }

/* 
 * This method opens a route, reads its contents and writes it to the
 * output route.
 * Returns -1 if there was an error, such as the input route not existing.
 */
int meth_builtin_snap_action(char *command, ROUTE output, ROUTE error) {
     char *data;
     int dlen, r;

     /* check route */
     if ( ! command || !*command) {
          elog_printf(ERROR, "no method command - use %s <snaproute>",
		       "snap");
	  return -1;
     }

     /* read route */
     data = route_read(command, NULL, &dlen);
     if ( ! data )
	  return -1;

     /* write route */
     r = route_write(output, data, dlen);
     nfree(data);				/* clear up data */
     if (r != dlen)
	  return -1;

     return 0;
}


/* ----- builtin tstamp (timestamp) method ----- */
char *meth_builtin_tstamp_id() { return "tstamp"; }
char *meth_builtin_tstamp_info() { return "Timestamp in seconds since "
					  "1/1/1970 00:00:00"; }
enum exectype meth_builtin_tstamp_type() { return METH_SOURCE; }

/* 
 * This method opens a route, reads its contents and writes it to the
 * output route.
 * Returns -1 if there was an error, such as the input route not existing.
 */
int meth_builtin_tstamp_action(char *command, ROUTE output, ROUTE error) {
     int r;

     /* write epoch time to route */
     r = route_printf(output, "%d ", time(NULL));
     if (r <= 0)
	  return -1;

     return 0;
}


/* ----- builtin sample method ----- */
PTREE *cascade_tab=NULL;	/* table of CASCADE, keyed by output route */
char *meth_builtin_sample_id() { return "sample"; }
char *meth_builtin_sample_info() { return "Sample tables from a timestore "
					"and produce a single table"; }
enum exectype meth_builtin_sample_type() { return METH_SOURCE; }

/* 
 * This method implements the cascade class on a specified timestore.
 * Cascade samples the tablestore over time and produces computed
 * summaries of the fields. See cascade_* for more information.
 *
 * The command in the 'sample' method command should be of the form:-
 *
 *     <function> <route>
 *
 * Where <route> must be a tablestore (currently) and function should be
 * one of the following:-
 *
 *     avg   - Calculate average the corrseponding figures (or avg)
 *     min   - Find minimum of corresponding figures
 *     max   - Find maximum of corresponding figures
 *     sum   - Calculate the sum of the corresponding figures
 *     last  - Echo the last set of figures (same result as snap method)
 *     rate  - Calculate mean rate, producing per sec dataa
 *
 * Returns -1 if there was an error, such as the input route not existing
 * or if the input route was not a tablestore.
 */
int meth_builtin_sample_init(char *command, ROUTE output, ROUTE error, 
			     struct meth_runset *rset)
{
     char *fntxt, *intxt;
     int spn1, spn2;
     enum cascade_fn fn;
     CASCADE *sampinfo;

     /* check route */
     if ( ! command ) {
	  route_printf(error, "no command supplied - probe: %s, "
		      "output: %s\n", "sample", route_getpurl(output));
	  return -1;
     }

     /* seperate and check arguments wthout strtok */
     fntxt = command;
     spn1 = strcspn(fntxt, " \t");	/* skip function word */
     if (spn1 == 0) {
	  route_printf(error, "no command parsed - probe: %s, "
		      "output: %s\n", "sample", route_getpurl(output));
	  return -1;
     }
     spn2 = strspn(fntxt+spn1, " \t");	/* skip intervening whitespace */
     if (spn2 == 0) {
	  route_printf(error, "no tablestore route found - probe: %s, "
		      "command: %s\n", "sample", command);
	  return -1;
     }
     intxt = fntxt+spn1+spn2;

     /* parse function string in text */
     if (strncmp(fntxt, "ave", 3) == 0 || strncmp(fntxt, "avg", 3) == 0)
	  fn = CASCADE_AVG;
     else if (strncmp(fntxt, "min", 3) == 0)
	  fn = CASCADE_MIN;
     else if (strncmp(fntxt, "max", 3) == 0)
	  fn = CASCADE_MAX;
     else if (strncmp(fntxt, "sum", 3) == 0)
	  fn = CASCADE_SUM;
     else if (strncmp(fntxt, "last", 4) == 0)
	  fn = CASCADE_LAST;
     else if (strncmp(fntxt, "first", 5) == 0)
	  fn = CASCADE_FIRST;
     else if (strncmp(fntxt, "diff", 4) == 0)
	  fn = CASCADE_DIFF;
     else if (strncmp(fntxt, "rate", 4) == 0)
	  fn = CASCADE_RATE;
     else {
	  route_printf(error, "function is not recognised, must be one "
		      "of: ave, avg, min, max, sum, last, rate - probe: %s "
		      "command: %s\n", "sample", command);
	  return -1;
     }

     /* initialise this cascade */
     sampinfo = cascade_init(fn, intxt);
     if ( ! sampinfo )
	  route_printf(error, "unable to sample - method: %s, "
		      "command: %s\n", "sample", command);

     /* save the returned details in cascade_tab */
     if (cascade_tab == NULL)
	  cascade_tab = ptree_create();
     ptree_add(cascade_tab, rset, sampinfo);

     return 0;
}

int meth_builtin_sample_action(char *command, ROUTE output, ROUTE error,
			       struct meth_runset *rset)
{
     CASCADE *sampent;

     if (command == NULL) {
	  route_printf(error, "no command supplied - probe: %s, "
		       "output: %s\n", "sample", route_getpurl(output));
	  return -1;
     }

     if (cascade_tab == NULL) {
	  route_printf(error, "not successfully initialised - probe: %s, "
		       "output: %s\n", "sample", route_getpurl(output));
	  return -1;
     }

     /* fetch sample entry */
     sampent = ptree_find(cascade_tab, rset);
     if (sampent == PTREE_NOVAL) {
	  route_printf(error, "can't find details - probe: %s, "
		      "command: %s\n", "sample", command);
	  return -1;
     }

     return cascade_sample(sampent, output, error);
}

int meth_builtin_sample_fini(char *command, ROUTE output, ROUTE error,
			     struct meth_runset *rset)
{
     CASCADE *sampent;

     if (command == NULL || cascade_tab == NULL)
     	  return -1;

     /* fetch sample entry */
     sampent = ptree_find(cascade_tab, rset);
     if (sampent == ITREE_NOVAL) {
	  route_printf(error, "can't find details - probe: %s, "
		      "command: %s\n", "sample", command);
	  return -1;
     }

     /* close open resources and free storage */
     cascade_fini(sampent);
     ptree_rm(cascade_tab);

     return 0;
}


/* ----- builtin pattern watching method ----- */
PTREE *pattern_tab=NULL;	/* table of WATCHED, keyed by output route */
char *meth_builtin_pattern_id()   { return "pattern"; }
char *meth_builtin_pattern_info() { return "Match patterns on groups of "
					   "routes to raise events"; }
enum exectype meth_builtin_pattern_type() { return METH_SOURCE; }

/*
 * This method implements the pattern class on specified routes.
 * The usage of which is:-
 *
 *     <pat-act route> <watch route>
 *
 * Where <pat-act route> is the name of the route that contains a list 
 * of pattern-action pairs. This route may be any standard Habiat route
 * in the p-url format. See the pattern class for details of the
 * pattern-action format.
 *
 * The <Watch-route> argument specifies the route that contains a list
 * of routes to monitor.
 *
 * Returns -1 if there was an error, although the watching routine is 
 * designed to be tolerate a number of errors and not report them.
 */
int meth_builtin_pattern_init(char *command, ROUTE output, ROUTE error, 
			      struct meth_runset *rset)
{
     WATCHED watchinfo;
     char *patact, *rtwatch;
     int spn1, spn2;

     /* check route */
     if ( ! command ) {
	  route_printf(error, "no command supplied - probe: %s "
		      "output: %s\n", "pattern", route_getpurl(output));
	  return -1;
     }

     /* seperate and check arguments wthout strtok */
     spn1 = strcspn(command, " \t");	/* length of pattern-action arg */
     if (spn1 == 0) {
	  route_printf(error, "no pattern-action found as arg 1 "
		       "- probe: %s output: %s\n", "pattern", 
		       route_getpurl(output));
	  return -1;
     }
     patact = xnmemdup(command, spn1+1);/* make copy for initialisation */
     patact[spn1] = '\0';

     spn2 = strspn(command+spn1, " \t");/* skip intervening whitespace */
     if (spn2 == 0) {
	  route_printf(error, "no watch list found as arg 2 - probe: %s "
		      "command: %s", "pattern", command);
	  return -1;
     }
     rtwatch = command+spn1+spn2;

     /* initialise this watch */
     watchinfo = pattern_init(output, error, patact, rtwatch);
     if ( ! watchinfo )
	  route_printf(error, "unable to watch - method: %s "
		      "command: %s\n", "pattern", command);
     nfree(patact);

     pattern_rundirectly(watchinfo, 1);

     /* save the returned details in pattern_tab */
     if (pattern_tab == NULL)
	  pattern_tab = ptree_create();
     ptree_add(pattern_tab, rset, watchinfo);

     return 0;
}

int meth_builtin_pattern_action(char *command, ROUTE output, ROUTE error,
				struct meth_runset *rset)
{
     WATCHED watchinfo;

     if (command == NULL) {
	  route_printf(error, "no command supplied - probe: %s "
		       "output: %s\n", "pattern", route_getpurl(output));
	  return -1;
     }

     if (pattern_tab == NULL) {
	  route_printf(error, "not successfully initialised - probe: %s "
		       "output: %s\n", "pattern", route_getpurl(output));
	  return -1;
     }

     /* fetch watch entry */
     watchinfo = ptree_find(pattern_tab, rset);
     if (watchinfo == PTREE_NOVAL) {
	  route_printf(error, "can't find details - probe: %s "
		      "command: %s\n", "pattern", command);
	  return -1;
     }

     return pattern_action(watchinfo, output, error);
}

int meth_builtin_pattern_fini(char *command, ROUTE output, ROUTE error,
			      struct meth_runset *rset)
{
     WATCHED watchinfo;

     if (command == NULL || pattern_tab == NULL)
     	  return -1;
     
     /* fetch watch entry */
     watchinfo = ptree_find(pattern_tab, rset);
     if (watchinfo == PTREE_NOVAL) {
	  route_printf(error, "can't find details - probe: %s "
		      "command: %s\n", "pattern", command);
	  return -1;
     }

     /* close open resources and free storage */
     pattern_fini(watchinfo);
     ptree_rm(pattern_tab);

     return 0;
}


#if 0
/* ----- builtin record method ----- */
ITREE *record_tab=NULL;	/* table of RECINFO, keyed by output route */
char *meth_builtin_record_id()   { return "record"; }
char *meth_builtin_record_info() { return "Record changed contents in "
					  "groups of routes"; }
enum exectype meth_builtin_record_type() { return METH_SOURCE; }

/*
 * This method implements the record class on specified routes.
 * The command in the 'record' method command should be of the form:-
 *
 *     <target file>  <watch route>
 *
 * The <target file> argument specifies a file that should contain the
 * generated version stores that contains the recorded contents of
 * the files.
 *
 * The <Watch-route> argument specifies the route that contains a list
 * of routes to monitor.
 *
 * Returns -1 if there was an error, although the watching routine is 
 * designed to be tolerate a number of errors and not report them.
 */
int meth_builtin_record_init(char *command, ROUTE output, ROUTE error, 
			     struct meth_runset *rset)
{
     RECINFO rinfo;
     char *target, *rtwatch;
     int spn1, spn2;

     /* check route */
     if ( ! command ) {
	  route_printf(error, "no command supplied - probe: %s "
		      "output: %s\n", "record", route_getpurl(output));
	  return -1;
     }

     /* seperate and check arguments wthout strtok */
     spn1 = strcspn(command, " \t");	/* skip target arg */
     if (spn1 == 0) {
	  route_printf(error,"no target filename found as arg 1 - "
		       "probe: %s output: %s\n", "record", 
		       route_getpurl(output));
	  return -1;
     }
     target = xnmemdup(command, spn1+1);/* make copy for initialisation */
     target[spn1] = '\0';

     spn2 = strspn(command+spn1, " \t");/* skip intervening whitespace */
     if (spn2 == 0) {
	  route_printf(error, "no watch list found as arg 2 - probe: %s "
		      "command: %s", "record", command);
	  return -1;
     }
     rtwatch = command+spn1+spn2;

     /* initialise this recording session */
     rinfo = record_init(output, error, target, rtwatch);
     if ( ! rinfo )
	  route_printf(error, "unable to initialise - method: %s "
		      "command: %s\n", "record", command);
     nfree(target);

     /* save the returned details in record_tab */
     if (record_tab == NULL)
	  record_tab = itree_create();
     itree_add(record_tab, (int) rset, rinfo);

     return 0;
}

int meth_builtin_record_action(char *command, ROUTE output, ROUTE error,
			       struct meth_runset *rset)
{
     RECINFO rinfo;

     if (command == NULL) {
	  route_printf(error, "no command supplied - probe: %s "
		      "output: %s\n", "record", route_getpurl(output));
	  return -1;
     }

     if (record_tab == NULL) {
	  route_printf(error, "not successfully initialised - probe: %s "
		       "output: %s\n", "record", route_getpurl(output));
	  return -1;
     }

     /* fetch record entry */
     rinfo = itree_find(record_tab, (int) rset);
     if (rinfo == ITREE_NOVAL) {
	  route_printf(error, "can't find details - probe: %s "
		      "command: %s\n", "record", command);
	  return -1;
     }

     return record_action(rinfo, output, error);
}

int meth_builtin_record_fini(char *command, ROUTE output, ROUTE error,
			     struct meth_runset *rset)
{
     RECINFO rinfo;

     if (command == NULL || record_tab == NULL)
     	  return -1;

     /* fetch record entry */
     rinfo = itree_find(record_tab, (int) rset);
     if (rinfo == ITREE_NOVAL) {
	  route_printf(error, "can't find details - probe: %s "
		      "command: %s\n", "record", command);
	  return -1;
     }

     /* close open resources and free storage */
     record_fini(rinfo);
     itree_rm(record_tab);

     return 0;
}
#endif


/* ----- builtin event method ----- */
PTREE *event_tab=NULL;	/* table of EVENTINFO, keyed by output route */
char *meth_builtin_event_id()   { return "event"; }
char *meth_builtin_event_info() { return "Process event queues to carry "
				         "out instructions"; }
enum exectype meth_builtin_event_type() { return METH_SOURCE; }

/*
 * This method watches of rings that contain ordered events and
 * carries out their instructions. Generally, the instructions are to run
 * programs or utilities, such as a pager call or snmp trap etc.
 * 
 * The command in the 'event' method command should be of the form:-
 *
 *     <event queue>...
 *
 * Where <event queue> are the list of rings that should be checked for 
 * new data. New entries should be executed in order.
 *
 * Returns -1 if there was an error
 */
int meth_builtin_event_init(char *command, ROUTE output, ROUTE error, 
			    struct meth_runset *rset)
{
     EVENTINFO einfo;

     /* check route */
     if ( ! command ) {
	  route_printf(error, "no command supplied - probe: %s "
		      "output: %s\n", "event", route_getpurl(output));
	  return -1;
     }

     /* initialise and check state */
     einfo = event_init(command);
     if (einfo == NULL) {
	  route_printf(error, "empty command supplied (1) - probe: %s "
		       "output: %s\n", "event", route_getpurl(output));
	  return -1;
     }

     /* success, create another instance */
     if (event_tab == NULL)
	  event_tab = ptree_create();
     ptree_add(event_tab, rset, einfo);

     return 0;
}

int meth_builtin_event_action(char *command, ROUTE output, ROUTE error,
			      struct meth_runset *rset)
{
     EVENTINFO einfo;

     if (event_tab == NULL) {
	  route_printf(error, "not successfully initialised - probe: %s "
		       "output: %s\n", "event", route_getpurl(output));
	  return -1;
     }

     /* fetch record entry */
     einfo = ptree_find(event_tab, rset);
     if (einfo == PTREE_NOVAL) {
	  route_printf(error, "can't find details - probe: %s "
		      "command: %s\n", "event", command);
	  return -1;
     }

     return event_action(einfo, output, error);
}

int meth_builtin_event_fini(char *command, ROUTE output, ROUTE error,
			    struct meth_runset *rset)
{
     EVENTINFO einfo;

     if (command == NULL || event_tab == NULL)
     	  return -1;

     /* fetch record entry */
     einfo = ptree_find(event_tab, rset);
     if (einfo == ITREE_NOVAL) {
	  route_printf(error, "can't find details - probe: %s "
		      "command: %s\n", "event", command);
	  return -1;
     }

     /* close open resources and free storage */
     event_fini(einfo);
     ptree_rm(event_tab);

     return 0;
}


/* ----- builtin replicate method ----- */
char *meth_builtin_rep_id()   { return "replicate"; }
char *meth_builtin_rep_info() { return "Replicate rings to and from a "
				       "repository"; }
enum exectype meth_builtin_rep_type() { return METH_FORK; }

/*
 * This method implements the replicate class to transfer data to/from
 * a repository.
 *
 * The command in the 'replicate' method command should be of the form:-
 *
 *     <in> <out> <state>
 *
 * where <in>, <out> and <state> are names of routes. <in> and <out> should
 * refer to free text storage that defines the replication relationships.
 * The relationships are held in a semi-colon (;) separated list, with
 * each element describing a source and destination ring.
 * <state> is the name of a route where the state can be saved.
 *
 * Returns -1 if there was an error, such as the external command
 * not existing or a failure to reach the reposiotry
 */
int meth_builtin_rep_action(char *command, 
			    ROUTE output, ROUTE error,
			    struct meth_runset *rset) {
     char *invar, *outvar, *state, *cmd, *errfld, *val1, *val2,
	  *inval=NULL, *outval=NULL, *expstate=NULL;
     int r;
     ITREE *inlist, *outlist;

     /* check route */
     if ( ! command || !*command ) {
	  route_printf(error, "no command supplied - probe: %s "
		      "output: %s\n", "replicate", route_getpurl(output));
	  return -1;
     }

     /* copy and separate arguments, raise an error if anything is missing */
     cmd = xnstrdup(command);
     invar  = strtok(cmd, " \t");
     outvar = strtok(NULL, " \t");
     state  = strtok(NULL, " \t");

     errfld = NULL;
     if (invar == NULL)
	  errfld = "inbound list variable as arg 1";
     else if (outvar == NULL)
	  errfld = "outbound list variable as arg 2";
     else if (state == NULL)
	  errfld = "state route as arg 3";

     if (errfld) {
	  route_printf(error,"missing %s - probe: %s output: %s\n", 
		       "replicate", route_getpurl(output));
	  return -1;
     }

     /* We have the name of the configuration variable that contains 
      * a name list of the in-bound rings. 
      * Get the contents of the variable, go though route template 
      * substitution, chop up the list into strings and finally save 
      * it in our list mechanism ITREE.
      */
     inlist = itree_create();
     val1 = cf_getstr(iiab_cf, invar);
     if (val1) {
	  inval = xnmalloc((strlen(val1)*4)+100);	/* enough space */
	  route_expand(inval, val1, "NOJOB", 0);
	  val2 = strtok(inval, ";");
	  while (val2) {
	       itree_append(inlist, val2);
	       val2 = strtok(NULL, ";");
	  }
     }

     /* We have the name of the configuration variable that contains 
      * a name list of the out-bound rings. 
      * Get the contents of the variable, go though route template 
      * substitution, chop up the list into strings and finally save 
      * it in our list mechanism ITREE.
      */
     outlist = itree_create();
     val1 = cf_getstr(iiab_cf, outvar);
     if (val1) {
	  outval = xnmalloc((strlen(val1)*4)+100);	/* enough space */
	  route_expand(outval, val1, "NOJOB", 0);
	  val2 = strtok(outval, ";");
	  while (val2) {
	       itree_append(outlist, val2);
	       val2 = strtok(NULL, ";");
	  }
     }

     /* Expand the state, which may also contain special '%' chars in */
     expstate = xnmalloc((strlen(state)*2)+100);	/* enough space */
     route_expand(expstate, state, "NOJOB", 0);

     /* work out route */

     /* carry out replication */
     r = rep_action(output, error, inlist, outlist, expstate);

     /* free storage and return */
     nfree(cmd);
     if (inval)
	  nfree(inval);
     if (outval)
	  nfree(outval);
     if (expstate)
	  nfree(expstate);
     itree_destroy(inlist);
     itree_destroy(outlist);
     return r;
}


/* ----- builtin restart method ----- */
char *meth_builtin_restart_id()   { return "restart"; }
char *meth_builtin_restart_info() { return "Restart collection"; }
enum exectype meth_builtin_restart_type() { return METH_SOURCE; }

/*
 * This method attempts to restart clockwork with its original arguments
 * and configuration location.
 * We do this to free ourselves from any lingering memory or resource leaks
 * or to restart a new job table.
 * 
 * There are currently no arguments and it only works with clockwork (as it
 * uses stopclock() to halt itself).
 *
 * First we log, then we grab the command line arguments from iiab to ensure
 * that we start in exactly the same way. Next, we register the start-up
 * routine with atexit(2) and finally we run stopclock().
 * Clockwork is shutdown as expected (and free all resources, files, 
 * sockets etc), but before the final curtain, exit() will start a 
 * new instance.
 *
 * Returns -1 if there was an error. If successful, this routine will not
 * return and the caller will be terminated
 */
int meth_builtin_restart_action(char *command, 
				ROUTE output, ROUTE error,
				struct meth_runset *rset) {

     /* log the fact that we are going down */
     route_printf(output,"%s: ** shutting down at %s to start again\n", 
		  "restart", util_decdatetime( time(NULL) ) );

     /* register start-up routine */
     atexit( meth_builtin_restart_atexit );

     /* stop the calling process using SIGTERM */
     /*stopclock();*/
     kill(getpid(), SIGTERM);
}

extern char **iiab_argv;

/* routine to register with atexit, which will start clockwork */
void meth_builtin_restart_atexit() {
     int r;

     /* fork a new process */
     if ( fork() == 0) {
          /* child */
          sleep(2);	/* wait a little while */
          r = execv(iiab_argv[0], iiab_argv);
     }

     /* the parent ignores the child as we are in the process of exiting */
}
