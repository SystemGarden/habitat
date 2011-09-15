/*
 * Class that provides a curses based interface for various data monitoring
 * activities. Based on rtmbv.pm and track.pl in perl
 * Nigel Stuckey, October & November 1997
 * Modified and placed into iiab library April 1998
 * Heavily modified December & January 1999
 *
 * Nigel Stuckey
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

/* There is a standard look and feel as follows:-
 *
 * Screen format:-
 * dd-mmm____________APPLICATION TITLE__________hh:mm:ss
 * <holstore>,<ring>,<seq>         lines: <x>-<y> of <z>
 * COLUMN HEADERS
 * <buffer text line...................................>
 * <buffer text line...................................>
 * <buffer text line...................................>
 * .
 * .
 * <buffer text line...................................>
 * (r)ings (u)pdate Arrows move
 * Sataus line, whats going on etc.
 */

/*
 * How to use
 *
 * Initialise with cursvu_init(), draw title with cursvu_drtitle(),
 * time with cursvu_drtime(), column headers with cursvu_drcols()
 * and help text with cursvu_drhelp().
 *
 * The location of display (holstore, ring, sequence) is set by 
 * several functions depending on text effect:-
 *     cursvu_drholstore_spec()  <holstore>
 *     cursvu_drring_spec()      <holstore,ring>
 *     cursvu_drentry_spec()     <holstore,ring,sequence>
 *
 * To display your text, you should supply either a text buffer
 * with cursvu_ldbuffer( yourtextbuffer ), which should not be freed 
 * until you do not wish to display it again or a list of lines 
 * contained in an ITREE and ordered by its key.
 *
 * There is an event loop to use the interface: 
 * cursvu_navigate( -1 , NULL, ... ) if you have predefined text or 
 * cursvu_navigate( nlines, listoflines, ... ) if you have a tree. 
 * You can use both without having to reload the text 
 * buffer, just with the -1 flag to _navigate().
 *
 * _navigate() has many parameters to set viewport, highlighted line, 
 * overriding key mappings, and overriding key processing. Processing 
 * routines are called with the key as argument. In those rouitines
 * and outside _navigate(), there are status routine _setselect(), _getpw().
 */

#include <curses.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include "../iiab/nmalloc.h"
#include "../iiab/itree.h"
#include "../iiab/util.h"
#include "cursvu.h"

char *cursvu_title;	/* Title of application */
int cursvu_titlelen;	/* Length of title string */
char *cursvu_header;	/* Column headers for buffer navigation */
int cursvu_headerlen;	/* Length of column headers */
int cursvu_continue;	/* 1=continue navigation, 0=stop navigating */
int cursvu_update;	/* 1=redraw buffer after keycmd, 0=dont */
int cursvu_first;	/* Ordinal index of first displayed line */
int cursvu_fkey;	/* Key of first displayed line */
int cursvu_bar;		/* Ordinal index of higlighted line */
int cursvu_bkey;	/* Key of highlighted line */
int cursvu_internal;	/* Internal/external flag */
int cursvu_virtfirst;	/* First line maps to virtual line number */
int cursvu_virtnlines;	/* Number of virtual lines */
int cursvu_intnlines;	/* Length of cursvu_intbuf */
int cursvu_extnlines;	/* Length of cursvu_extbuf */
ITREE *cursvu_intbuf;	/* Internal buffer, tree of lines */
ITREE *cursvu_extbuf;	/* External buffer, tree of lines */
time_t stattime = LONG_MAX;	/* Time the status line should be removed */

struct cursvu_keycmd default_keycmds[] = {
     { 'q',		&cursvu_exit },
     { 'Q',		&cursvu_exit },
     { 'h',		&cursvu_help },
     { '?',		&cursvu_help },
     { KEY_DOWN,	&cursvu_down },
     { '\016',		&cursvu_down },		/* ^N */
     { KEY_UP,		&cursvu_up },
     { '\020',		&cursvu_up },		/* ^P */
     { KEY_LEFT,	&cursvu_left },
     { '\002',		&cursvu_left },		/* ^B */
     { KEY_RIGHT,	&cursvu_right },
     { '\006',		&cursvu_right },	/* ^F */
     { KEY_NPAGE,	&cursvu_pgdown },
     { '\026',		&cursvu_pgdown },	/* ^V */
     { KEY_PPAGE,	&cursvu_pgup },
     { KEY_HOME,	&cursvu_top },
     { KEY_END,		&cursvu_bot },
     { '\n',		&cursvu_down },
     { KEY_BACKSPACE,	&cursvu_up },
     { ' ',		&cursvu_pgdown },
     { 'b',		&cursvu_pgup },
     { '[',		&cursvu_leftscn },
     { ']',		&cursvu_rightscn },
     { '\003',		&cursvu_exit },		/* ^C */
     { '\014',		&cursvu_redraw },	/* ^L */
     { 0,		NULL}
};

/* Initialise the interface */
void cursvu_init() {
     cursvu_title = NULL;
     cursvu_titlelen = 0;
     cursvu_header = NULL;
     cursvu_headerlen = 0;
     cursvu_internal = 1;
     cursvu_intnlines = 0;
     cursvu_intbuf = itree_create();
     cursvu_extnlines = 0;
     cursvu_extbuf = NULL;
     cursvu_virtfirst = 0;
     cursvu_virtnlines = 0;

     initscr();
     cbreak();
     noecho();
     nonl();
     intrflush(stdscr, FALSE);
     keypad(stdscr, TRUE);
}

/* Shutdown the interface */
void cursvu_fini() {
     if (cursvu_title)
           nfree(cursvu_title);
     if (cursvu_header)
          nfree(cursvu_header);
     itree_destroy(cursvu_intbuf);
     endwin();
}

/* Draw title line. If title is NULL, use the previous title string provided.
 * Otherwise, if title is given, set that to be the current title. Setting
 * to "" will clear the string and not print a title.
 */
void cursvu_drtitle(char *title) {
     if (title) {
	  if (cursvu_title)
	       nfree(cursvu_title);
	  cursvu_title = xnstrdup(title);
	  cursvu_titlelen = strlen(title);
     }
     if (cursvu_titlelen)
	  mvaddstr(0, (COLS-cursvu_titlelen) / 2, cursvu_title);
}

/* Draw or update the date and time of the title line */
void cursvu_drtime() {
     static char buf[CURSVU_SHORTSTR];
     struct tm *now;
     time_t now_t;

     /* Display time - top right */
     now_t = time(NULL);
     now = localtime(&now_t);
     strftime(buf, CURSVU_SHORTSTR, "%H:%M:%S", now);
     mvaddstr(0, COLS-9, buf);

     /* Display date - top left */
     strftime(buf, CURSVU_SHORTSTR, "%d %b %Y", now);
     mvaddstr(0, 0, buf);
}

/*
 * Draw data specification of entry: <holstore>. If holstore contains 
 * a leading path, it will be stripped off
 */
void cursvu_drholstore_spec(char *holname) {
     move(1, 0);
     clrtoeol();
     if (holname)
	  addstr(util_basename(holname));
}

/* Draw data specification of entry: <holstore,ring> */
void cursvu_drring_spec(char *holname, char *ringname) {
     cursvu_drholstore_spec(holname);
     printw(",%s", ringname);
}

/* Draw data specification of entry: <holstore,ring,entryseq> */
void cursvu_drentry_spec(char *holstore, char *ringname, int entryseq) {
     cursvu_drring_spec(holstore, ringname);
     printw(",%d", entryseq);
}

/* Draw or update the buffer location string. Called by cursvu_navigate() */
void cursvu_drbuffer_spec() {
     char buf[CURSVU_SHORTSTR];
     int r, bot;

     bot = cursvu_virtfirst + cursvu_first + (LINES-5);
     if (bot > cursvu_virtnlines)
	  bot = cursvu_virtnlines;
     r = snprintf(buf, CURSVU_SHORTSTR, "%d-%d of %d", 
		  cursvu_virtfirst+cursvu_first+1, 
		  bot, cursvu_virtnlines);
     if (r < 0)
	  ; /* error condition: not decided yet */
     move(1, COLS-21);
     clrtoeol();
     if (cursvu_virtnlines <= 0)
	  printw("no lines");
     else {
	  if (r < 15)
	       printw("lines: %s", buf);
	  else if (r == 15)
	       printw("lines:%s", buf);
	  else
	       addstr(buf);
     }
}

/* Draw a header of the entry's column names */
void cursvu_drcols(char *colnames) {
     int i;

     if (colnames) {
	  if (cursvu_header)
	       nfree(cursvu_header);
	  cursvu_header = xnstrdup(colnames);
	  cursvu_headerlen = strlen(colnames);
     }

     /* move and clear */
     move(2, 0);
     clrtoeol();
     if ( ! cursvu_headerlen)
	  return;

     /* Draw it */
/*      standout(); */     /* Highlights commented out */
     mvaddnstr(2, 0, cursvu_header, COLS);
     for (i=cursvu_headerlen; i < COLS; i++)
	  addch(' ');
/*      standend(); */
}

/* Load text into the internal buffer and draw it to screen
 * The text provided is not copied by crusvu and the user has to
 * manage its memory allocation, freeing it when finished. 
 * It will be referenced by cursvu until a further call to _ldbuffer is made 
 */
void cursvu_ldbuffer(char *text) {
     char *start, *end;
     int i;

     /* clear out references to old lines */
     itree_clearout(cursvu_intbuf, NULL);

     /* load tree with referencies to lines (NOT copies) */
     i = 0;
     start = end = text;
     while (end != NULL && end != '\0' && start != NULL && *start != '\0') {
	  end = strchr(start, '\n');
	  itree_append(cursvu_intbuf, start);
	  start = end+1;
	  i++;
     }
     cursvu_intnlines = i;	/* No trailing empty lines */

     cursvu_first = cursvu_bar = 0;
     cursvu_fkey = cursvu_bkey = 0;	/* internal keys start from 0 */
     cursvu_drbuffer(-1, NULL, 0);
}

/* Write buffer to screen from the line indexed by cursvu_first and
 * highlight the line indexed by cursvu_bar, if bar_key==-1.
 * If cursvu_bar is offscreen, the display area is recalculated.
 * If bar_key != -1, the display is also recalculted, using the bar_key
 * as a tree key to base it on.
 * 
 * Internal or external buffers may be used: for internal, initialise with
 * cursvu_ldbuffer() and _drbuffer() with a_nlines==-1.
 * For external, provide your own ITREE list and line count as arguments.
 * Supplied (extenral) lists should have ordered keys to display correctly, 
 * but the keys do not have to be contiguous. 
 * Internal and external buffers are treated seperately and an internal
 * may be returned to after an external is displayed.
 */
void cursvu_drbuffer(int a_nlines, ITREE *a_buffer, int bar_key) {
     int i, j, len, nlines;
     ITREE *line;

     if (a_nlines == -1) {
          /* internal buffer */
          cursvu_internal = 1;
	  nlines = cursvu_intnlines;
	  line = cursvu_intbuf;
     } else {
          /* external buffer */
          cursvu_internal = 0;
	  nlines = cursvu_extnlines = a_nlines;
	  line = cursvu_extbuf = a_buffer;
     }

     /* set the initial highlighted bar */
     if (bar_key != -1) {
          if (itree_find( CURSVU_CURBUF, bar_key) != ITREE_NOVAL) {
	       cursvu_bkey = bar_key;
          } else {
	       /* set the first key */
	       itree_first(CURSVU_CURBUF);
	       cursvu_bkey = itree_getkey(CURSVU_CURBUF);
	       /* set cursvu_first to centre the highlight */
	  }
	  cursvu_keytoindex();
     }

     /* Draw the screen */
     itree_find(line, cursvu_fkey);
     cursvu_drbuffer_spec();
     for (i=3; i < LINES-2; i++) {
	  move(i, 0);
	  clrtoeol();
	  if (i-3+cursvu_first >= nlines || nlines == 0)
	       continue;
	  len = strcspn(itree_get(line), "\n");
	  if (len > COLS)
	       len = COLS;
	  if (cursvu_bar == i-3+cursvu_first) {
	       /* Highlighted ring */
	       standout();
	       addnstr(itree_get(line), len);
	       for (j=len; j < COLS; j++)
		    addch(' ');
	       standend();
	  } else
	       addnstr(itree_get(line), len);
	  itree_next(line);
     }
}

/* Main event loop
 * Use cursor keys to move a highlight bar around the bufer, carrying
 * out repositioning, redrawing and updating the bufer spec.
 * The newkeys[] list should follow the format of default_keycmds[] and 
 * be terminated with a record that has keytoken == NULL.
 * If the method cursvu_exit() is called as a key command, navigate_buffer()
 * returns with the key that caused it.
 * Buffers should be supplied in text form with load_buffer() or
 * supplied in line vector for here. In the former case, set nlines to -1
 * and line to NULL.
 * The buffer is mapped to virtual lines numbers [virtfirst,virtnlines+nlines].
 * If nlines == -1, virtfirst and virtnlines are ignored.
 * Gotoseq will highlight the line it indexes, which is the line number
 * if nlines==-1 or the key'ed user data otherwise. If no key matches
 * gotoseq, the first line is highlighted.
 * If user data does not provide keys below 0, gotoseq==0 highlights the
 * first line.
 * Gotoseq==-1 is special and will cause the previous highlight to be used.
 */
int cursvu_navigate(int nlines,			/* Buf lines, -1 for intern */
		    ITREE *line,		/* Buf line list */
		    int virtfirst,		/* First virtual line */
		    int virtnlines,		/* Virtual lines */
		    int gotoseq,		/* Goto sequence; 0 for top */
		    int scantime, 		/* Seconds before update */
		    void (*updaterun)(),	/* Run at update */
		    void (*prekeyrun)(),	/* Run before key cmd */
		    void (*postkeyrun)(),	/* Run after key cmd */
		    struct cursvu_keycmd newkeys[]) /* Precedence key cmds */
{
     time_t nexttime;
     int r, key, i, timeout;
     void (*runthis)();
     char msg[CURSVU_SHORTSTR];

     if (nlines == -1) {
          /* use internal buffer */
          cursvu_virtfirst = 0;
	  cursvu_virtnlines = cursvu_intnlines;
     } else {
          cursvu_virtfirst = virtfirst;	/* set up drbuffer_spec() */
	  cursvu_virtnlines =  virtnlines;
     }
     key=0;				/* shut -Wall up */
     cursvu_continue=1;
     cursvu_update=0;
     cursvu_drbuffer(nlines, line, gotoseq); /* sets cursvu_extnlines */
     nexttime = time(NULL) + scantime;	/* Next wall clock time to update */
     timeout = scantime * 10;		/* ...in 1/10ths */
     while (cursvu_continue) {
	  if (cursvu_update) {
	       cursvu_update=0;
	       cursvu_drbuffer((nlines==-1 ? -1 : cursvu_extnlines), line, -1);
	  }
	  refresh();
	  r = halfdelay(timeout);
	  key = getch();
	  if (key == ERR) {
	       /* Time out
		* Set new timer, update clock and run the supplied 
		* update routine. If the buffer length changes, 
		* then redisplay.
		*/
	       nexttime = time(NULL) + scantime;/* Next wall clock update */
	       timeout = scantime * 10;		/* ...in 1/10ths */
	       cursvu_drtime();
	       if (time(NULL) > stattime) {
		    cursvu_drstatus("");
		    stattime = LONG_MAX;
	       }
	       if (updaterun != NULL)
		    (*updaterun)();
	  } else {
	       /* Check the key pressed with our lists */
	       runthis = NULL;
	       if (newkeys != NULL)
		    for (i=0; newkeys[i].keytoken != 0; i++)
			 if (newkeys[i].keytoken == key) {
			      runthis = newkeys[i].action;
			      break;
			 }
	       if (runthis == NULL)
		    for (i=0; default_keycmds[i].keytoken != 0; i++)
			 if (default_keycmds[i].keytoken == key) {
			      runthis = default_keycmds[i].action;
			      break;
			 }
	       if (runthis != NULL) {
		    /* Key pressed with associated command
		     * Run the pre-command, the command and the post-command
		     * die if the commands give errors
		     */
		    if (prekeyrun != NULL)
			 (*prekeyrun)(key);
		    (*runthis)(key);
		    if (postkeyrun != NULL)
			 (*postkeyrun)(key);
	       } else {
		    /* The default */
		    i = sprintf(msg, "Unknown command: `");
		    if (iscntrl(key)) {
			 msg[i++] = '^';
			 msg[i++] = key+'@';
			 msg[i] = '\0';
		    } else {
			 msg[i++] = key;
			 msg[i] = '\0';
		    }
		    sprintf(msg+i, "' 0x%x (press `h' for help)", key);

		    cursvu_drstatus(msg);
	       }
	       timeout = (nexttime - time(NULL) ) *10; /* Timeout in 1/10ths */
	  }
     }

     /* get the key corresponding to updated cursvu_bar index */
     if (line) {
	  itree_find(line, cursvu_fkey);
	  for (i=cursvu_first; i<cursvu_bar; i++)
	       itree_next(line);
	  cursvu_bkey = itree_getkey(line);
     } else {
	  cursvu_fkey = cursvu_bar;
     }

     return key;
}

/*
 * Return the key of the selected entry if data was supplied in an ITREE
 * by the user or the line index of selected data if supplied in a text buffer
 */
int cursvu_getselect() {
     return cursvu_bkey;
}

/* Draw help line */
void cursvu_drhelp(char *helptxt) {
     move(LINES-2, 0);
     clrtoeol();
     addstr(helptxt);
}

/* Draw a string on the status line for a few seconds */
void cursvu_drstatus(char *stat) {
     move(LINES-1, 0);
     clrtoeol();
     addstr(stat);
     stattime = time(NULL) + 2;
};

/* Print prompt in the status area and obtain a password */
char *cursvu_getpw(char *prompt) {
     char password[CURSVU_SHORTSTR];

     cursvu_drstatus(prompt);

     noecho();
     move(LINES-1, strlen(prompt));
     wgetnstr(stdscr, password, CURSVU_SHORTSTR);
     /*mvgetnstr(LINES-1, strlen(prompt), password, CURSVU_SHORTSTR);*/
     return xnstrdup(password);
}

void cursvu_exit() {
     cursvu_continue=0;
}

void cursvu_help() {
     int i;

     for (i=3; i<LINES-2; i++) {
	  move(i, 0);
	  clrtoeol();
     }

     mvaddstr(3, 0, "\n"
"     -- Help --\n"
"     Scroll up . . . . . . . . <UpArrow>     ^P\n"
"     Scroll down . . . . . . . <DownArrow>   ^N\n"
"     Scroll left . . . . . . . <LeftArrow>   ^F\n"
"     Scroll right. . . . . . . <RightArrow>  ^B\n"
"     Scroll up one screen. . . <PgUp>        b\n"
"     Scroll down one screen. . <PgDn>        ^V  <Space>\n"
"     Scroll left one screen. . ]\n"
"     Scroll right one screen . [\n"
"     Scroll to home. . . . . . <Home>\n"
"     Scroll to bottom. . . . . <End>\n"
"     Quit. . . . . . . . . . . q             Q\n"
"     Help. . . . . . . . . . . h");

	standout(); 
	cursvu_drstatus(" -- Press any key -- ");
	standend();
	cbreak();		/* To leave half delay mode */
	getch();
	cursvu_drstatus("");
	halfdelay(1);
	cursvu_update=1;		/* Redraw body */
}

/* 
 * Routine to allow users to display ad hoc messages on the screen.
 * The message will be displayed superimposed on the normal text which
 * will be redrawn once the user has read it and pressed  a key to continue.
 * Currently, the screen will be corrupted if msg contains more than 
 * LINES-5 lines of text.
 * The length of the message should be set in msglen, which may be -1
 * to signify a terminated string and all of the message should be
 * printed.
 * Examples of use are for error messages and other notices.
 */
void cursvu_message(char *msg, int msglen) {
     int i;

     for (i=3; i<LINES-2; i++) {
	  move(i, 0);
	  clrtoeol();
     }

     mvaddnstr(3, 0, msg, msglen);
     standout(); 
     cursvu_drstatus(" -- Press any key -- ");
     standend();
     cbreak();		/* To leave half delay mode */
     getch();
     cursvu_drstatus("");
     halfdelay(1);
     cursvu_update=1;		/* Redraw body */
}

void cursvu_down() {
     itree_find( CURSVU_CURBUF, cursvu_bkey );
     if ( ! itree_isatend(CURSVU_CURBUF) ) {
          itree_next(CURSVU_CURBUF);
	  cursvu_bkey = itree_getkey(CURSVU_CURBUF);
	  cursvu_bar++;
     }

     cursvu_viewbar();
}

void cursvu_up() {
     itree_find( CURSVU_CURBUF, cursvu_bkey );
     if ( ! itree_isatstart(CURSVU_CURBUF) ) {
          itree_prev(CURSVU_CURBUF);
	  cursvu_bkey = itree_getkey(CURSVU_CURBUF);
	  cursvu_bar--;
     }

     cursvu_viewbar();
}

void cursvu_left() {}

void cursvu_right() {}

void cursvu_pgdown() {
     int i;

     itree_find( CURSVU_CURBUF, cursvu_bkey );
     for (i=0; i < LINES-5; i++) {
          if ( itree_isatend(CURSVU_CURBUF) )
	       break;
          itree_next( CURSVU_CURBUF );
	  cursvu_bar++;
     }

     cursvu_bkey = itree_getkey(CURSVU_CURBUF);
     cursvu_viewbar();
}

void cursvu_pgup() {
     int i;

     itree_find( CURSVU_CURBUF, cursvu_bkey );
     for (i=0; i < LINES-5; i++) {
          if ( itree_isatstart(CURSVU_CURBUF) )
	       break;
          itree_prev( CURSVU_CURBUF );
	  cursvu_bar--;
     }

     cursvu_bkey = itree_getkey(CURSVU_CURBUF);
     cursvu_viewbar();
}

void cursvu_top() {
     cursvu_bar = 0;
     itree_first( CURSVU_CURBUF );
     cursvu_bkey = itree_getkey( CURSVU_CURBUF );
     cursvu_viewbar();
}

void cursvu_bot() {
     cursvu_bar = CURSVU_CURNLINES;
     itree_last( CURSVU_CURBUF );
     cursvu_bkey = itree_getkey( CURSVU_CURBUF );
     cursvu_viewbar();
}

void cursvu_redraw() {
     redrawwin(stdscr);
     wrefresh(stdscr);
}

void cursvu_leftscn() {
}

void cursvu_rightscn() {
}

/* make bar visible, given all parameters have been initialised */
void cursvu_viewbar() {
     cursvu_update = 1;

     /* do nothing if curses_bar is within view */
     if (cursvu_bar >= cursvu_first && cursvu_bar < cursvu_first+(LINES-5))
	  return;

     cursvu_setfirst();
}

/* set cursvu_first & cursvu_fkey so that the bar is visible */
void cursvu_setfirst() {
     int i, extra;

     cursvu_update = 1;

     /* are the parameters trustworthy? */
     if ( ! ( cursvu_bar > 0 && 
	      cursvu_bar < CURSVU_CURNLINES &&
	      itree_find(CURSVU_CURBUF, cursvu_bkey) != ITREE_NOVAL ) ) {
          /* not trustworthy */
	  itree_first( CURSVU_CURBUF );
          cursvu_first = cursvu_bar = 0;
	  cursvu_fkey = cursvu_bkey = itree_getkey( CURSVU_CURBUF );
	  return;
     }

     /* centre highlight on display unless it needlessly causes white space */
     extra = (LINES-5)/2 - (CURSVU_CURNLINES - cursvu_bar);
     if (extra < 0)
          extra = 0;

     /* recalculate first index and key */
     cursvu_first = cursvu_bar;
     for (i=0; i < (LINES-5)/2 + extra; i++) {
          if (itree_isatstart( CURSVU_CURBUF ))
	       break;
	  itree_prev( CURSVU_CURBUF );
	  cursvu_first--;
     }
     cursvu_fkey = itree_getkey( CURSVU_CURBUF );
}

/* calculate indexes from bar key, ensuring highlight is centre of screen */
void cursvu_keytoindex() {
     /* check bar key exists */
     if (itree_find( CURSVU_CURBUF, cursvu_bkey) != ITREE_NOVAL )
          /* count backwards to find the bar's ordinal index */
          for (cursvu_bar = 0; ! itree_isatstart(CURSVU_CURBUF); cursvu_bar++)
	       itree_prev( CURSVU_CURBUF );

     /* for all cases, let _setfirst() work out defaults */
     cursvu_setfirst();
}


#ifdef TEST
#include "route.h"
#include "elog.h"
void t_prparams() {
     static char m[80];
     sprintf(m, "_first: %d _fkey: %d _bar: %d _bkey: %d", 
	     cursvu_first, cursvu_fkey, cursvu_bar, cursvu_bkey);
     cursvu_drstatus(m);
}

main() {
     ITREE *t1, *t2;
     int i;
     char s1[100];
     rtinf err;

     cursvu_init();

     /* test 1: adornment with three lines of text */
     cursvu_drtitle("A TEST");
     cursvu_drtime();
     cursvu_drring_spec("some holstore", "some ring");
     cursvu_drcols("   SEQ   WHEN   TEXT");
     cursvu_drhelp("test 1: three lines of text (h & arrows, q=next test)");
     cursvu_ldbuffer("And men of england\nwill think their\nmanhood cheep\n");
     cursvu_navigate(-1, NULL, -1, -1, 0, 5, NULL, NULL, t_prparams, NULL);

     /* test 2: more than a screenfull of test */
     cursvu_ldbuffer("line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nlineA\nlineB\nlineC\nlineD\nlineE\nlineF\nlineG\nlineH\nlineI\nlineJ\nlineK\nlineL\nlineM\n");
     cursvu_drhelp("test 2: more than a screenful (h & arrows, q=next test)");
     cursvu_navigate(-1, NULL, -1, -1, 0, 5, NULL, NULL, t_prparams, NULL);

     /* test 3: itree list of text */
     t1 = itree_create();
     itree_append(t1, "hello");
     itree_append(t1, "there");
     itree_append(t1, "baby");
     cursvu_drhelp("test 3: external list (h & arrows, q=next test)");
     cursvu_navigate(itree_n(t1), t1, 0, itree_n(t1), 0, 5, NULL, NULL, 
		     t_prparams, NULL);
     itree_destroy(t1);

     /* test 4: itree list of text bigger than screen */
     t2 = itree_create();
     for (i=0; i< 30; i++) {
	  sprintf(s1, "external line %d", i);
	  itree_append(t2, xnstrdup(s1));
     }
     cursvu_drhelp("test 4: big external list (h & arrows, q=next test)");
     cursvu_navigate(itree_n(t2), t2, 0, itree_n(t2), 0, 5, NULL, NULL, 
		     t_prparams, NULL);
     itree_clearoutandfree(t2);
     itree_destroy(t2);

     /* test 5: empty list */
     cursvu_ldbuffer("");
     cursvu_drhelp("test 5: empty list (h & arrows, q=next test)");
     cursvu_navigate(-1, NULL, -1, -1, 0, 5, NULL, NULL, t_prparams, NULL);

     /* test 6: list of a single line */
     cursvu_ldbuffer("this is a single line");
     cursvu_drhelp("test 6: single line (h & arrows, q=next test)");
     cursvu_navigate(-1, NULL, -1, -1, 0, 5, NULL, NULL, t_prparams, NULL);

     /* test 7: external empty list */
     t1 = itree_create();
     cursvu_drhelp("test 7: empty external list (h & arrows, q=next test)");
     cursvu_navigate(itree_n(t1), t1, 0, itree_n(t1), 0, 5, NULL, NULL, 
		     t_prparams, NULL);
     itree_destroy(t1);

     /* test 8: external list with gaps in key continunity */
     t1 = itree_create();
     itree_add(t1, 3, "hello");
     itree_add(t1, 19, "there");
     itree_add(t1, 375, "baby");
     cursvu_drhelp("test 8: external noncontiguous list (h & arrows, q=next test)");
     cursvu_navigate(itree_n(t1), t1, 0, itree_n(t1), 0, 5, NULL, NULL, 
		     t_prparams, NULL);
     itree_destroy(t1);

     /* test 9: message test. not quite as expected in use, which is within
      * cursvu_navigate() as a callback, but it will do! */
     cursvu_drhelp("test 9: pop-up message callback");
     cursvu_message("\n\ntest 9: message\nthis should be a\nmultiline "
		    "message\nhope it works for you", -1);

     /* test 10: external list, non-contignuous keys starting at end */
     t1 = itree_create();
     itree_add(t1, 3, "once");
     itree_add(t1, 19, "I");
     itree_add(t1, 375, "caught");
     itree_add(t1, 376, "a");
     itree_add(t1, 377, "fish");
     itree_add(t1, 400, "alive");
     cursvu_drhelp("test 10: end of ext list, non-contiguous keys (h & arrows, q=next test)");
     cursvu_navigate(itree_n(t1), t1, 0, itree_n(t1), 400, 5, NULL, NULL, 
		     t_prparams, NULL);
     itree_destroy(t1);

     /* test 11: elog callback via route to display a message */
     route_init("stderr", 0, NULL);
     err = route_open("stderr", NULL, NULL, 0);
     route_cbregister("cursvu", cursvu_message);
     elog_init(err, 0, "cursvu test", NULL);
     elog_setsevpurl(INFO, "cb:cursvu");
     elog_send(INFO, "main()", 0, "test 11: elog message in a cursvu popup\nround and round the garden");
     route_cbunregister("cursvu");
     elog_fini();
     route_close(err);
     route_fini();

     cursvu_fini();
}
#endif /* TEST */
