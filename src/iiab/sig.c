/*
 * class to handle the signal requirements of the iiab library
 *
 * Nigel Stuckey, January 1998
 * Modified Feb 1999 with more handlers
 *
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <signal.h>
#include "sig.h"
#include "elog.h"

sigset_t blockchild;	/* signal set to block SIGCHLD */
sigset_t blockalarm;	/* signal set to block SIGALRM */
sigset_t blockall;	/* signal set to block SIGCHLD and SIGALRM */
sigset_t blocktty;	/* signal set to block SIGTSTP, SIGTTOU, SIGTTIN */
sigset_t blockwork;	/* signal set to block all catachable signals */
sigset_t blocknothing;	/* no signals set */
sigset_t blockprev;	/* holds disabled signal set from sig_off() */

/* Initialise the signals */
void sig_init()
{
     /* Prepare a block nothing set */
     if (sigemptyset(&blocknothing))
	  elog_die(FATAL,"unable to prepare blocknothing");

     /* Prepare SIGCHLD blocking mask */
     if (sigemptyset(&blockchild) && sigaddset(&blockchild, SIGCHLD))
	  elog_die(FATAL, "unable to prepare blockchild");

     /* Prepare SIGALRM blocking mask */
     if (sigemptyset(&blockalarm) && sigaddset(&blockalarm, SIGALRM))
	  elog_die(FATAL, "unable to prepare blockalarm");

     /* Prepare tty signal blocking mask */
     if (sigemptyset(&blocktty) && sigaddset(&blocktty, SIGTSTP) 
	  && sigaddset(&blockwork, SIGTTOU) && sigaddset(&blockwork, SIGTTIN))
	  elog_die(FATAL, "unable to prepare blocktty");

     /* Prepare all signal blocking mask */
     if (sigemptyset(&blockwork) && sigaddset(&blockwork, SIGCHLD) 
	 && sigaddset(&blockwork, SIGALRM))
	  elog_die(FATAL, "unable to prepare blockwork");

     /* Prepare all signal blocking mask */
     if (sigfillset(&blockall))
	  elog_die(FATAL, "unable to prepare blockall");
}

/* Install a SIGCHLD signal handler */
void sig_setchild(void (*handler)(int))
{
     struct sigaction s;
     int r;

     elog_printf(DEBUG, "handler=%x", handler);

     /* Set up child event handler */
     s.sa_handler = handler;
     s.sa_mask = blockwork;
     s.sa_flags = SA_RESTART;	/* Linux likes! -may be non portable */
     r = sigaction(SIGCHLD, &s, NULL);
     if (r == -1)
	  elog_die(FATAL, "unable to install signal handler");
}


/* Install a SIGALRM signal handler */
void sig_setalarm(void (*handler)(int))
{
     struct sigaction s;
     int r;

     elog_printf(DEBUG, "handler=%x", handler);

     /* Set up the alarm() callback */
     s.sa_handler = handler;
     s.sa_mask = blockwork;
     s.sa_flags = SA_RESTART;	/* Linux likes! - may be non portable */
     r = sigaction(SIGALRM, &s, NULL);
     if (r == -1)
	  elog_die(FATAL, "unable to install signal handler");
}


/* Install signal handlers that will catch normal requests for a 
 * graceful shutdown */
void sig_setexit(void (*handler)(int))
{
     struct sigaction s;
     int r;

     elog_printf(DEBUG, "handler=%x", handler);

     s.sa_handler = handler;
     s.sa_mask = blockalarm;	/* let SIGCHLD through as it represents 
				 * the draining down of work, but dont allow
				 * SIGALRM as means more work!! */
     s.sa_flags = SA_RESTART;	/* linux likes! - may be non portable */
     r = sigaction(SIGHUP, &s, NULL);
     if (r == -1)
	  elog_die(FATAL, "SIGHUP failed to install");
     r = sigaction(SIGINT, &s, NULL);
     if (r == -1)
	  elog_die(FATAL, "SIGINT failed to install");
     r = sigaction(SIGQUIT, &s, NULL);
     if (r == -1)
	  elog_die(FATAL, "SIGQUIT failed to install");
     r = sigaction(SIGTERM, &s, NULL);
     if (r == -1)
	  elog_die(FATAL, "SIGTERM failed to install");
}


/* Block tty signal handlers (SIGTSTP, SIGTTOU, SIGTTIN) */
void sig_blocktty()
{
     /* Set up the alarm() callback */
     if (sigprocmask(SIG_BLOCK, &blocktty, NULL) == -1)
	  elog_die(FATAL, "unable to block tty signals");
}


/* Disable all preventable signals and save the previous signal set */
void sig_off()
{
     elog_send(DEBUG, "disable signals");

     if (sigprocmask(SIG_BLOCK, &blockall, &blockprev) == -1)
	  elog_die(FATAL, "unable to BLOCK");
}

/* Restore signal set */
void sig_on()
{
     elog_send(DEBUG, "restore signals");

     if (sigprocmask(SIG_SETMASK, &blockprev, NULL) == -1)
	  elog_die(FATAL, "unable to UNBLOCK");
}
