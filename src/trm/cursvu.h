/*
 * Track the activity in a holstore database using a curses interface
 * Based on track.pl, which was a perl5 implementation
 * Nigel Stuckey, October 1997
 *
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _CURSVU_H_
#define _CURSVU_H_

#include "../iiab/itree.h"

#define CURSVU_SHORTSTR 80
#define CURSVU_CURBUF    (cursvu_internal? cursvu_intbuf : cursvu_extbuf)
#define CURSVU_CURNLINES (cursvu_internal? cursvu_intnlines : cursvu_extnlines)

struct cursvu_keycmd {
     int keytoken;
     void (*action)();
};

/* Functional prototypes */
void cursvu_init();
void cursvu_fini();
void cursvu_drtitle(char *);
void cursvu_drtime();
void cursvu_drholstore_spec(char *);
void cursvu_drring_spec(char *, char *);
void cursvu_drentry_spec(char *, char *, int);
void cursvu_drbuffer_spec();
void cursvu_drcols(char *colnames);
void cursvu_ldbuffer(char *text);
void cursvu_drbuffer(int nlines, ITREE *line, int key_bar);
int cursvu_navigate(int nlines,			/* Buf lines, -1 for intern */
		    ITREE *line,		/* Buf line list */
		    int virtfirst,		/* First virtual line */
		    int virtnlines,		/* Virtual lines */
		    int highlightseq,		/* Goto sequence; -1 for top */
		    int scantime, 		/* Seconds before update */
		    void (*updaterun)(),	/* Run at update */
		    void (*prekeyrun)(),	/* Run before key cmd */
		    void (*postkeyrun)(),	/* Run after key cmd */
		    struct cursvu_keycmd newkeys[]); /* Precedence key cmds */
int cursvu_getselect();
void cursvu_drhelp(char *helptxt);
void cursvu_drstatus(char *stat);
char *cursvu_getpw(char *prompt);
void cursvu_exit();
void cursvu_message(char *msg, int msglen);
void cursvu_help();
void cursvu_down();
void cursvu_up();
void cursvu_left();
void cursvu_right();
void cursvu_pgdown();
void cursvu_pgup();
void cursvu_top();
void cursvu_bot();
void cursvu_redraw();
void cursvu_leftscn();
void cursvu_rightscn();
void cursvu_viewbar();
void cursvu_setfirst();
void cursvu_keytoindex();

#endif /* _CURSVU_H_ */
