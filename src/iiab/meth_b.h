/*
 * Builtin methods for the meth class.
 * Nigel Stuckey, January 1988, based on code from August 1996
 * Read method added, May 1999
 *
 * Copyright System Garden Ltd 1996-2001, all rights reserved.
 */

#ifndef _METH_B_H_
#define _METH_B_H_

#include "meth.h"

/* Functional prototypes */
char         *meth_builtin_exec_id();
char         *meth_builtin_exec_info();
enum exectype meth_builtin_exec_type();
int           meth_builtin_exec_action(char *command, ROUTE output, 
				       ROUTE error, struct meth_runset *rset);
char         *meth_builtin_sh_id();
char         *meth_builtin_sh_info();
enum exectype meth_builtin_sh_type();
int           meth_builtin_sh_action(char *command);
char         *meth_builtin_snap_id();
char         *meth_builtin_snap_info();
enum exectype meth_builtin_snap_type();
int           meth_builtin_snap_action(char *command, ROUTE out, ROUTE err);
char         *meth_builtin_tstamp_id();
char         *meth_builtin_tstamp_info();
enum exectype meth_builtin_tstamp_type();
int           meth_builtin_tstamp_action(char *command, ROUTE out, ROUTE err);
char         *meth_builtin_time_id();
char         *meth_builtin_time_info();
enum exectype meth_builtin_time_type();
int           meth_builtin_time_action(char *command, ROUTE out, ROUTE err);
char         *meth_builtin_sample_id();
char         *meth_builtin_sample_info();
enum exectype meth_builtin_sample_type();
int           meth_builtin_sample_init(char *command, ROUTE out, ROUTE err,
				       struct meth_runset *rset);
int           meth_builtin_sample_action(char *command, ROUTE out, ROUTE err,
					 struct meth_runset *rset);
int           meth_builtin_sample_fini(char *command, ROUTE out, ROUTE err,
				       struct meth_runset *rset);
char         *meth_builtin_pattern_id();
char         *meth_builtin_pattern_info();
enum exectype meth_builtin_pattern_type();
int           meth_builtin_pattern_init(char *command, ROUTE out, ROUTE err,
					struct meth_runset *rset);
int           meth_builtin_pattern_action(char *command, ROUTE out, ROUTE err,
					  struct meth_runset *rset);
int           meth_builtin_pattern_fini(char *command, ROUTE out, ROUTE err,
					struct meth_runset *rset);
char         *meth_builtin_record_id();
char         *meth_builtin_record_info();
enum exectype meth_builtin_record_type();
int           meth_builtin_record_init(char *command, ROUTE out, ROUTE err,
				       struct meth_runset *rset);
int           meth_builtin_record_action(char *command, ROUTE out, ROUTE err,
					 struct meth_runset *rset);
int           meth_builtin_record_fini(char *command, ROUTE out, ROUTE err,
				       struct meth_runset *rset);
char         *meth_builtin_event_id();
char         *meth_builtin_event_info();
enum exectype meth_builtin_event_type();
int           meth_builtin_event_init(char *command, ROUTE out, ROUTE err,
				      struct meth_runset *rset);
int           meth_builtin_event_action(char *command, ROUTE out, ROUTE err,
					struct meth_runset *rset);
int           meth_builtin_event_fini(char *command, ROUTE out, ROUTE err,
				      struct meth_runset *rset);
char         *meth_builtin_rep_id();
char         *meth_builtin_rep_info();
enum exectype meth_builtin_rep_type();
int           meth_builtin_rep_action(char *command, ROUTE out, ROUTE err,
				      struct meth_runset *rset);
char         *meth_builtin_checkpoint_id();
char         *meth_builtin_checkpoint_info();
enum exectype meth_builtin_checkpoint_type();
int           meth_builtin_checkpoint_action(char *command, 
					     ROUTE out, ROUTE err, 
					     struct meth_runset *rset);
char         *meth_builtin_restart_id();
char         *meth_builtin_restart_info();
enum exectype meth_builtin_restart_type();
int           meth_builtin_restart_action(char *command, ROUTE out, ROUTE err,
					  struct meth_runset *rset);
char         *meth_builtin_shutdown_id();
char         *meth_builtin_shutdown_info();
enum exectype meth_builtin_shutdown_type();
int           meth_builtin_shutdown_action(char *command, ROUTE out, ROUTE err,
					   struct meth_runset *rset);

/* Builtin referencies */
extern struct meth_info meth_builtins[];

#endif /* _METH_B_H_ */



