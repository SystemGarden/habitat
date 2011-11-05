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
int      sig_didinit=0;	/* if the sig class has been initialised */

/* Initialise the class and unblock everything */
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

     sig_didinit++;	/* class initialised */

     elog_send(DEBUG, "enable signals");
     if (sigprocmask(SIG_SETMASK, &blocknothing, NULL) == -1)
          elog_die(FATAL, "unable to BLOCK");
}

/* Install a SIGCHLD signal handler */
void sig_setchild(void (*handler)(int))
{
     struct sigaction s;
     int r;

     elog_printf(DEBUG, "set SIGCHILD handler=%p", handler);

     /* Set up child event handler */
     s.sa_handler = handler;
     s.sa_mask = blockwork;
     s.sa_flags = SA_RESTART;	/* Linux likes! -may be non portable */
     r = sigaction(SIGCHLD, &s, NULL);
     if (r == -1)
	  elog_die(FATAL, "unable to install SIGCHILD signal handler");
}


/* Install a SIGALRM signal handler */
void sig_setalarm(void (*handler)(int))
{
     struct sigaction s;
     int r;

     elog_printf(DEBUG, "set SIGALRM handler=%p", handler);

     /* Set up the alarm() callback */
     s.sa_handler = handler;
     s.sa_mask = blockwork;
     s.sa_flags = SA_RESTART;	/* Linux likes! - may be non portable */
     r = sigaction(SIGALRM, &s, NULL);
     if (r == -1)
	  elog_die(FATAL, "unable to install SIGALRM signal handler");
}


/* Install signal handlers that will catch normal requests for a 
 * graceful shutdown */
void sig_setexit(void (*handler)(int))
{
     struct sigaction s;
     int r;

     elog_printf(DEBUG, "set exit handler=%p for SIGHUP, SIGINT, SIGQUIT & "
		 "SIGTERM", handler);

     s.sa_handler = handler;
     s.sa_mask = blockalarm;	/* let SIGCHLD through for the duration of the
				 * handler as it represents the draining down 
				 * of work, but dont allow SIGALRM as means 
				 * more work!! */
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


/* Disable all preventable signals (SIGALRM & SIGCHLD) and save the previous 
 * signal set */
void sig_off()
{
     if (sigprocmask(SIG_BLOCK, &blockall, &blockprev) == -1)
	  elog_die(FATAL, "unable to BLOCK signals when attempting to disable");

     elog_send(DEBUG, "disabled signals");
}

/* Restore signal set previously diabled by sig_off() */
void sig_on()
{
     if (sig_didinit) {
          elog_send(DEBUG, "restore signals");
     } else {
          elog_send(ERROR, "ask to restore signals but sig not init; "
		    "do nothing");
	  return;
     }

     if (sigprocmask(SIG_SETMASK, &blockprev, NULL) == -1)
	  elog_die(FATAL, "unable to UNBLOCK signal to restore them");
}
