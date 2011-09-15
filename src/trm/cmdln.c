/*
 * Command line framework
 * Based on ihol.pl, which was a perl5 implementation
 * Nigel Stuckey, Feburary 1998.
 * Modified to use elog for error and log messages.
 *
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <malloc.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "../iiab/nmalloc.h"
#include "../iiab/tree.h"
#include "../iiab/itree.h"
#include "../iiab/elog.h"
#include "../iiab/util.h"
#include "cmdln.h"

/* This array of command definitions defines the interface and work
   that its will carry out */
struct cmdln_def cmdln_builtincmds[] = {
  {"help",	cmdln_do_help,	"Give help on all or some commands"},
  {"?",		cmdln_do_help,	"Give help on all or some commands"},
  {"!",		cmdln_do_shell,	"`! <cmd>' runs <cmd> in a shell"},
  {"sh",	cmdln_do_shell,	"Escape to a sub shell"},
  {"exit",	cmdln_do_exit,	"Leave this application"},
  {"quit",	cmdln_do_exit,	"Leave this application"},
  {"bye",	cmdln_do_exit,	"Leave this application"},
  {"",		NULL,		""}
};

/* -- Globals -- */
char *cmdln_binname;		/* global holding the binary's name */
TREE *cmdln_commands;		/* command list (hash of struct cmdln_def) */
char *cmdln_argv[CMDLN_MAXARGS];/* internal argument passing array */
int   cmdln_argc=0;		/* internal argument passing count */
char *cmdln_prompt;		/* prompt string */
int   cmdln_done;		/* =0 read from cmdline, =1 leave */
ITREE*cmdln_tablist;		/* tabulation list */
int   cmdln_tabwidest;		/* widest token */
int (*cmdln_lastresort)(int, char **);	/* When set, call on unknown command */

/* Initialise the command line framework */
void cmdln_init(char *binname,		/* name of application */
		struct cmdln_def *cmds	/* command list */ )
{
     /* initialise */
     cmdln_binname = binname;
     cmdln_commands = tree_create();
     cmdln_lastresort = NULL;
     cmdln_tablist = NULL;

     /* add commands */
     cmdln_addcommand(cmdln_builtincmds);
     cmdln_addcommand(cmds);

     cmdln_initialize_readline();
}

/* Shutdown the cmdln class */
void cmdln_fini() {
     tree_destroy(cmdln_commands);
     if (cmdln_prompt)
	  nfree(cmdln_prompt);
}

/* Set prompt to new value */
void cmdln_setprompt(char *newprompt)
{
     if (cmdln_prompt)
	  nfree(cmdln_prompt);
     cmdln_prompt = xnstrdup(newprompt);
}

/*
 * Set the last resport command. This should be used when a command has
 * not been recognised. It takes argc & argv pair, returning 0 if
 * successful or non-0 if not
 */
void cmdln_setlastresort(int (*lastresort)(int, char **))
{
     cmdln_lastresort = lastresort;
}

/*
 * Add a list of command definitions (in the form of an array) to the
 * command tree. Commands passed should be nmalloc'ed or statics as
 * we do not duplicate them, and will not free them later on. Potential
 * source of minor memory leeks, but as a cmd line application, I
 * don't think it will be a problem.
 */
void cmdln_addcommand(struct cmdln_def *command /* command array to add */ )
{
     struct cmdln_def *c;

     if (!command)
	  return;

     c = command;
     while (*c->name != '\0') {
	  tree_add(cmdln_commands, c->name, c);
	  c++;
     }
}

/*
 * Remove command from command tree. Does not free the structures that 
 * it removes.
 * Returns 1 for success or 0 for failure.
 */
int cmdln_rmcommand(char *command	/* name of command to remove */ )
{
     struct cmdln_def *cmd;
     cmd = tree_find(cmdln_commands, command);
     if (cmd == TREE_NOVAL)
	  return 0;
     tree_rm(cmdln_commands);
     return 1;
}

/*
 * Find command definition associcated with command name.
 * Returns a pointer to the structure or NULL if unable to find.
 */
struct cmdln_def *cmdln_findcommand(char *name	/* command name */ )
{
     return tree_find(cmdln_commands, name);
}

/* Start tabulation: prepare to collect tokens */
void  cmdln_begintabulate()
{
     if (cmdln_tablist) {
          elog_printf(ERROR, "cmdln_begintabulate()", 0, 
		      "list shouldn't exist!\n");
	  cmdln_freetablist();
     }

     cmdln_tablist = itree_create();
     cmdln_tabwidest = 0;
}

/* Tabulate a string: collect it and check its length */
void  cmdln_tabulate(char *token)
{
     int l;

     if ( ! cmdln_tablist) {
          elog_printf(ERROR, "cmdln_tabulate()", 0, "list not set\n");
	  return;
     }

     l = strlen(token);
     if (l > cmdln_tabwidest)
	  cmdln_tabwidest = l;
     itree_append(cmdln_tablist, xnstrdup(token));

     return;
}

/* End tabulation: print the resutls to screen */
void  cmdln_endtabulate()
{
     int i, ncols;
     char format[10];

     if ( ! cmdln_tablist) {
          elog_printf(ERROR, "cmdln_endtabulate()", 0, "list not set\n");
	  return;
     }

     /* prepare columns */
     ncols = 80 / (cmdln_tabwidest+2);
     sprintf(format, "%%-%ds", cmdln_tabwidest+2);

     i = 0;
     itree_traverse(cmdln_tablist) {
	  printf(format, itree_get(cmdln_tablist));
	  if (++i >= ncols) {
	       printf("\n");
	       i = 0;
	  }
     }
     if (i)
	  printf("\n");

     cmdln_freetablist();

     return;
}

/* internal: free storage taken by freetablist */
void cmdln_freetablist()
{
     itree_traverse(cmdln_tablist)
	  nfree(itree_get(cmdln_tablist));
     itree_destroy(cmdln_tablist);
     cmdln_tablist = NULL;
}

/*
 * Command line loop.
 * Repeatedly read commands using GNU readline (with editing), save them
 * in history, parse them and finally attempt to run the result.
 * Returns when an end of file is encountered or cmdln_done is set to true.
 */
void cmdln_readloop()
{
     char *line;
     int i;

     cmdln_done = 0;
     while (!cmdln_done) {
	  line = readline(cmdln_prompt);
	  nadopt(line);
	  if (!line)
	       break;

	  /* remove leading and trailing whitespace & add to history */
	  util_strtrim(line);
	  if (*line) {
	       add_history(line);
	       cmdln_parse(line);     /* sets global cmdln_arg[cv] as result */
	       cmdln_run(cmdln_argc, cmdln_argv);
	  }

	  /* Free space taken in parsing */
	  for (i=0; i<cmdln_argc; i++) {
	       nfree(cmdln_argv[i]);
	       cmdln_argv[i] = 0;
	  }

	  nfree(line);
     }
}

/* 
 * Join arguments with a string conjunction 'conj' into a single string
 * and place the result in buf. Arguments should be an array of strings
 * and length of array (like argc,argv). Joining should start from the
 * from'th argument. Returns the number of arguments copied.
 */
int cmdln_joinargs(char *buf,	/* buffer large enough to take result */
		   int fargc,	/* number of arguments */
		   char *fargv[], /* array of arguments */
		   char *conj,	/* joining string */
		   int from	/* index to start work: fargv[from] */ )
{
     int i;

     *buf = '\0';	/* Terminate so strcat() works */
     for (i=from; i<fargc; i++) {
	  strcat(buf, fargv[i]);
	  strcat(buf, conj);
     }
     return (i-from < 0 ? 0 : i-from);
}

/* 
 * Parse cmdline already stripped of whitespace into command tokens. 
 * Place copies of tokens into the globals cmdln_argc and cmdln_argv.
 * Returns the number of tokens 
 */
int cmdln_parse(char *cmdline	/* command line */)
{
     char quote[2];
     int span, i=0, nchars=0;

     cmdln_argc = 0;
     if (*cmdline == '\0')
	  return cmdln_argc;

     /* Assume line has been stripped of whitespace already */
     
     /* Split the remaining command line on whitespace outside quotes */
     quote[1] = '\0';
     nchars = strlen(cmdline);
     while (i < nchars) {
	  quote[0] = 0;		/* Not processing quotes initially */
	  span = strcspn(cmdline+i, " \"\'\t\n");
	  if (span == 0) {
	       /* Consume leading or padded whitespace */
	       if (isspace(cmdline[i])) {
		    i++;
		    continue;
	       }

	       /* Quoted token: find matching quote to get entire token */
	       quote[0] = cmdline[i++];
	       span = strcspn(cmdline+i, quote);
	  }
	  
	  /* Store token in a vector of xnmalloc'ed strings */
	  cmdln_argv[cmdln_argc] = xnmalloc(span+1);
	  strncpy(cmdln_argv[cmdln_argc], cmdline+i, span);
	  *(cmdln_argv[cmdln_argc]+span) = '\0';

	  cmdln_argc++;
	  i += span;
	  if (quote[0])		/* Skip end quote if applcable */
	       i++;
     }
     
     return cmdln_argc;
}

/* Run command in passed by argc and argv */
void cmdln_run(int argc, char **argv) {
     struct cmdln_def *dothis;
     int i, r;

     if (argc == 0)	/* No commands to run */
	  return;

     r = -1;
     if ((dothis = tree_find(cmdln_commands, argv[0])) != TREE_NOVAL)
	  r = dothis->func(argc, argv);
     else {
	  if (cmdln_lastresort)
	       r = cmdln_lastresort(argc, argv);
	  if (r) {
	       /* If last resort fails, treat as unknown command */
	       printf("Unknown command:");
	       for (i=0; i<argc; i++)
		    printf(" %s", argv[i]);
	       printf("\n");
	  }
     }
}

/* ------------- GNU readline and history specifics ---------------- */

/* Tell the GNU Readline library how to complete.  We want to try to complete
   on command names if this is the first word in the line, or on filenames
   if not. */
void cmdln_initialize_readline ()
{
     /* Allow conditional parsing of the ~/.inputrc file. */
     rl_readline_name = cmdln_binname;

     /* Tell the completer that we want a crack first. */
     rl_attempted_completion_function = (CPPFunction *) cmdln_completion;
}

/* Attempt to complete on the contents of TEXT.  START and END bound the
   region of rl_line_buffer that contains the word to complete.  TEXT is
   the word to complete.  We can use the entire contents of rl_line_buffer
   in case we want to do some simple parsing.  Return the array of matches,
   or NULL if there aren't any. */
char **cmdln_completion (char *text, int start, int end)
{
     char **matches;

     matches = (char **)NULL;

     /* If this word is at the start of the line, then it is a command
        to complete.  Otherwise it is the name of a file in the current
        directory. */
     if (start == 0)
          matches = rl_completion_matches (text, cmdln_cmdgenerator);

     return (matches);
}

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *cmdln_cmdgenerator (const char *text, int state)
{
     static int len;
     struct cmdln_def *cmd;

     if (!state) {
	  /* initially find a match for the word root and save state */
	  len = strlen(text);
	  tree_traverse(cmdln_commands) {
	       cmd = tree_get(cmdln_commands);
	       if (strncmp(cmd->name, text, len) == 0)
		    return (strdup(cmd->name));
	  }
     } else {
	  /* successive matches find next word root */
	  if (!tree_isatend(cmdln_commands)) {
	       tree_next(cmdln_commands);
	       cmd = tree_get(cmdln_commands);
	       if (strncmp(cmd->name, text, len) == 0)
		    return (strdup(cmd->name));
	  }
     }

     /* If no names matched, then return NULL. */
     return ((char *)NULL);
}

/* ------------- Built in commands ---------------- */

/* The help routine. HA! Takes command names as its arguments and prints
 * their help text to the screen. If there are no arguments given, print
 * the all the available commands and some comments about the readline
 * edits.
 */
int cmdln_do_help(int argc, char **argv) {
     int i;
     struct cmdln_def *cmd;

     if (argc < 2) {
	  /* A lone cry of help */
	  cmdln_begintabulate();
	  tree_traverse(cmdln_commands)
	       cmdln_tabulate(tree_getkey(cmdln_commands));
	  cmdln_endtabulate();

#if 0
	  printf("Commands:\t");
	  i=0;
	  tree_traverse(cmdln_commands) {
	       /* print command names in 6 columns */
	       if (i == 6) {
		    printf("\n\t\t");
		    i = 0;
	       }
	       cmd = tree_get(cmdln_commands);
	       printf("%s\t", cmd->name);
	       i++;
	  }
	  if (i)
	       printf("\n");
#endif
	  printf("^P/^N-previous/next line   ^F/^B-forward/backward char   <tab>-complete command\n");
#if 0
	  printf("Keyboard:\t<up>/<down>    or ^P/^N\tPrevious/Next line\n");
	  printf("\t\t<left>/<right> or ^F/^B\tForward/Backward character\n");
	  printf("\t\t<tab>\t\t\tComplete command\n");
	  printf("For further commands & editing features, see documentation for %s\n", cmdln_binname);
#endif
     } else {
	  /* Specific help required */
	  for (i=1; i<argc; i++) {
	       /* Give detailed help for each arg */
	       if ((cmd = tree_find(cmdln_commands, argv[i])) == TREE_NOVAL)
		    printf("Can't help; command does not exist: %s\n",argv[i]);
	       else {
		    /* TBI: A way of formatting text to fit the screen */
		    printf("%s\t\t%s\n", cmd->name, cmd->help);
	       }
	  }
     }

     return 0;
}

/* Start a subshell and run the arguments in it */
int cmdln_do_shell(int argc, char **argv) {
     /* Not yet implemented */
     char buf[CMDLN_LONGSTR];
     int r;

     r = cmdln_joinargs(buf, argc, argv, " ", 1);
     if (r > 0) {
	  /* Run single command */
	  r = system(buf);
	  if (r)
	       printf("Command status: %d\n", r);
     } else {
	  /* Escape into a shell */
	  r = system("/bin/sh");
	  if (r)
	       printf("Command returned error: %d\n", r);
     }

     return 0;
}

/* Exit its */
int cmdln_do_exit() {
     cmdln_done = 1;
     return 0;
}

#if TEST
main(int argc, char **argv) {
     rtinf err;

     route_init("stderr", 0, NULL);
     err = route_open("stderr", NULL, NULL, 0);
     elog_init(err, 1, argv[0], NULL);
     cmdln_init(argv[0], NULL);

     cmdln_setprompt("test me> ");
     cmdln_readloop();

     cmdln_fini();
     elog_fini();
     route_close(err);
     route_fini();
}
#endif /* TEST */
