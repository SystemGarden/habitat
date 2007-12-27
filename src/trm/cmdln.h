/*
 * Command line framework
 * Based on ihol.pl, which was a perl5 implementation
 * Nigel Stuckey, Feburary 1998.
 *
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _CMDLN_H_
#define _CMDLN_H_

/* Definitions */
#define CMDLN_MAXARGS 1000	/* Max num of args in cmd line */
#define CMDLN_SHORTSTR 50	/* Max short string length */
#define CMDLN_LONGSTR 250	/* Max long string length */

/* This structure defines a command definition */
struct cmdln_def {
     char *name;		/* Command user types */
     int (*func)(int,char**);	/* Function to run when the name is issued */
     char *help;		/* Text help for that command */
};

void  cmdln_init(char *binname, struct cmdln_def *cmds);
void  cmdln_fini();
void  cmdln_setprompt(char *newprompt);
void  cmdln_addcommand(struct cmdln_def *command);
void  cmdln_setlastresort(int (*lastresort)(int, char **));
void  cmdln_begintabulate();
void  cmdln_tabulate(char *);
void  cmdln_endtabulate();
void  cmdln_freetablist();	/* internal */
int   cmdln_rmcommand(char *command);
struct cmdln_def *cmdln_findcommand(char *name);
void  cmdln_readloop();
int   cmdln_joinargs(char *buf, int fargc, char *fargv[], char *conj,int from);
int   cmdln_parse(char *cmdline);
void  cmdln_run(int argc, char **argv);
void  cmdln_initialize_readline ();
char **cmdln_completion (char *text, int start, int end);
char *cmdln_cmdgenerator (char *text, int state);
int   cmdln_do_help(int argc, char **argv);
int   cmdln_do_helpcmd(int argc, char **argv);
int   cmdln_do_shell(int argc, char **argv);
int   cmdln_do_exit();

#endif /* _CMDLN_H_ */
