/*
 * Track the activity in a timestore database using a curses interface
 * Based on track.pl, which was a perl5 implementation
 * Nigel Stuckey, October 1997
 *
 * Revised to use library functions, April 1998
 *
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <curses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "../iiab/timestore.h"
#include "../iiab/cf.h"
#include "../iiab/elog.h"
#include "../iiab/nmalloc.h"
#include "../iiab/iiab.h"
#include "../iiab/ringbag.h"
#include "../trm/cursvu.h"
#include "../iiab/util.h"

#define SHORTSTR 80
#define APPTITLE "HOLSTORE TRACKER"

struct cursvu_keycmd standard_keycmds[] = {
     { '\r',		&cursvu_exit },	/* ^M (return) */
     { KEY_ENTER,	&cursvu_exit },
     { '\033',		&cursvu_exit },	/* <ESC> */
     { 0,		NULL}
};
struct cursvu_keycmd entrymode_keycmds[] = {
     { '\033',		&cursvu_exit },	/* <ESC> */
     { '\t',		&cursvu_exit },	/* <TAB> */
     { KEY_BTAB,	&cursvu_exit },	/* Shift-<TAB> */
     { 'u',		&cursvu_exit },
     { 'n',		&cursvu_exit },
     { 'N',		&cursvu_exit },
     { 'p',		&cursvu_exit },
     { 'P',		&cursvu_exit },
     { 0,		NULL}
};

int main(int, char **);
void interface();
void usage(char *);
void load_ring();
char *mklistsummary(ntsbuf *mgetdata);
char *mkringsummary(struct ringbag_ringent *ringinfo);
int ring_mode(int highlight);
int list_mode(int highlight);
int entry_mode(int highlight);
void list_update();
void entry_update();

/* Initialise globals */
char *binname;
char usagetxt[]=
"[-t <interval>] [-pn] <timestores>...\n"
"where -t <int>   number of seconds before the next rescan (default 5)\n"
"      -p         prefetch whole holstore in advance (lazy by default)\n"
"      -n         don't cache data summaries (cached by default)\n"
"      <holstore> path to the holstore file\n";
char *cfdefaults = 
"debug      0\n"
"nmalloc    0\n"
"elog.all   none:\n"
"elog.above warning stderr:";

#if 0
rtinf err;			/* Route for errors to use */
#endif
int   scantime = 5;		/* Check for new additions in seconds */
int   prefetch = 0;		/* Load entire holstore before starting */
int   nocache = 0;		/* No not cache any data in memory */
char  *holname;			/* holstore/timestore name */
char  *ringname;		/* ringname name */
struct ringbag_ringent *ringinf;/* ring details */
int   entrykey;			/* key of entry within ring */
ITREE *ringsum;			/* List of text ring summaries */

/* test routine */
extern int cursvu_first;
extern int cursvu_fkey;
extern int cursvu_bar;
extern int cursvu_bkey;
void t_prparams() {
     static char m[80];
     sprintf(m, "_first: %d _fkey: %d _bar: %d _bkey: %d", 
	     cursvu_first, cursvu_fkey, cursvu_bar, cursvu_bkey);
     cursvu_drstatus(m);
}


int main(int argc, char *argv[]) {
     int i;
     char argname[SHORTSTR];

     /* Initialisation */
     iiab_start("t:pn", argc, argv, usagetxt, cfdefaults);
     ringbag_init(iiab_err, 1);

     /*
      * Process command line and its switches
      *	-t = rescan time (default 5)
      *	-p = prefetch as much data in advance (lazy by default)
      *	-n = no caching of data (cached when encountered by default)
      * Globals set are:-
      *	 scantime  - Check for new additions after this time in seconds
      *	 prefetch  - Completely scan holstore, so fill memory
      *	 nocache   - No not cache any data in memory
      */
     if (cf_defined(iiab_cmdarg, "t"))
          scantime = cf_getint(iiab_cmdarg, "t");
     if (cf_defined(iiab_cmdarg, "p"))
          prefetch++;
     if (cf_defined(iiab_cmdarg, "n"))
          nocache++;

     for (i=1; i < cf_getint(iiab_cmdarg, "argc"); i++) {
          sprintf(argname, "argv%d", i);
          if ( access( cf_getstr(iiab_cmdarg, argname), R_OK ) )
	       elog_printf(ERROR, "Unable to open %s for reading: %s",
			   cf_getstr(iiab_cmdarg, argname),
			   strerror(errno));
          else
	       if ( ! ringbag_addts( cf_getstr(iiab_cmdarg, argname) ) )
		    elog_printf(ERROR, "Can't load timestore %s",
				cf_getstr(iiab_cmdarg, argname));
     }

#if 0
     /* Set holstore and ring if present to be current */
     if ( ! ringbag_addts(holname) )
	  elog_die(FATAL, "Can't load holstore %s", holname);
#endif

     if ( ! ringbag_getallrings() ) {
	  elog_send(WARNING, "No data available to track");
     } else {
          holname = ringname = NULL;

#if 0
	  if (ringname)
	       load_ring();
	  else
	       ringname = NULL;	/* no ring supplied */
#endif

	  /* The data is now in place to do curses!! */
	  interface();
     }

     /* Interface only returns on shutdown */
     ringbag_fini();
     iiab_stop();
     printf ("Goodbye\n");
     exit(0);
}

/* Load ring using the global variables */
void load_ring() {
     char compound[SHORTSTR];
     int r;

     r = snprintf(compound, SHORTSTR, "%s,%s", holname, ringname);
     if (r == -1)
	  elog_die(FATAL, "Compound name too long");

     /* open without password for now */
     if ( ! ringbag_setring(compound, NULL))
	  elog_die(FATAL, "Can't open ring %s. Try starting "
		   "without a ring and choosing inside the application or "
		   "open with a password", compound);
}

/* Make a one line summary of data. Free returned string with nfree() */
char *mklistsummary(ntsbuf *mgetdata) {
     char *b, *prt;

     b = xnmalloc(COLS+2);	/* to allow \n\0 to terminate string */
     prt = util_bintostr(COLS+2, mgetdata->buffer, mgetdata->len);
     snprintf(b, COLS+2, "%6d %-8s %s\n", mgetdata->seq, 
	      util_shortadaptdatetime(mgetdata->instime), prt);
     nfree(prt);

     return(b);
}

/* Make a one line summary of a ring */
char *mkringsummary(struct ringbag_ringent *ringinfo) {
     char *b;
     int r;

     b = xnmalloc(COLS+2);	/* to allow \n\0 to terminate string */
     r = snprintf(b, COLS+2, "%-14s %-8s %4d/%-4d %s\n", ringinfo->tsname, 
		  ringinfo->ringname, ringinfo->seen, ringinfo->available, 
		  ringinfo->description);

     return(b);
}

/* Run the interface, returning when application exit is requested */
void interface() {
     int mode, token;
     int saved_highlight[4] = {-1,-1,-1,-1};	/* 0=holstore, 1=ring, 2=list, 
						 * 3=entry */

     /*
      * The curses screen interface may take on four modes of operation,
      * each of which is in a heirarchy, thus:-
      *
      * 1. holstore_mode() Select which holstore to use
      * 2. ring_mode()     Select the ring to use
      * 3. list_mode()     Browse the list of entries in each ring
      * 4. entry_mode()    View an entry's data
      *
      * Each mode will arrange to call its subrdinate and we just call
      * the top to get the ball rolling. BTW, if on entry to a mode, the
      * data is avalable to go to the next mode -- do just that. Only
      * on return should it be assumed that a selection needs to be made.
      */

     cursvu_init();		/* Start curses */
     mode = 1;
     while (1) {
	  switch(mode) {
	  case 1: token = ring_mode(saved_highlight[1]);
	          break;
	  case 2: token = list_mode(saved_highlight[2]);
	          break;
	  case 3: token = entry_mode(saved_highlight[3]);
	          break;
	  }
	  switch(token) {
	  case '\033':
	  case 'u':
	       saved_highlight[mode--] = -1;
	       break;
	  case KEY_ENTER:
	  case '\r':
	       saved_highlight[mode++] = cursvu_getselect();
	       break;
	  case 'q':
	  case 'Q':
	       cursvu_fini();	/* stop curses */
	       return;
	       break;
	  }
	  if (mode > 3)		/* mind you dont fall off the end */
	       mode = 3;
	  if (mode < 1)
	       mode = 1;
     }
}

void usage(char *argv0) {
     fprintf(stderr, "Usage: %s %s", argv0, usagetxt);
     exit(1);
}

/* 
 * Screen mode to select rings. Returns token on exit.
 * Default sequence to highlight (seq) is the top line
 */
int ring_mode(int highlight	/* initial key; -1=default */) {
     int token, nrings, sel, i;
     TREE *ringinfo;

     /* Screen format:-
      * dd-mmm_____________HOLSTORE TRACKER__________hh:mm:ss
      * <holstore>                      lines: <x>-<y> of <z>
      * <ringName Unread Total Descrption...................>
      * <ringName Unread Total Descrption...................>
      * <ringName Unread Total Descrption...................>
      * <ringName Unread Total Descrption...................>
      * .
      * .
      * <ringName Unread Total Descrption...................>
      * Arrow keys, (h)elp, (q)uit, <ret> select
      * Sataus line, whats going on etc.
      */

     while(1) {
	  /* Update display */
	  /*clear();*/
	  /*draw_title();*/
	  cursvu_drtime();
	  cursvu_drholstore_spec(NULL);
	  cursvu_drcols("TIMESTORE      RING   UNREAD AVIL DESCRIPTION");
	  cursvu_drhelp("Arrow keys, (h)elp, (q)uit, <ret> select");

	  /* Make summary of available rings */
	  nrings = ringbag_getallrings();
	  ringsum = itree_create();
	  ringinfo = ringbag_getrings();
	  tree_traverse(ringinfo)
	       itree_append(ringsum, mkringsummary(tree_get(ringinfo)));

	  token = cursvu_navigate(nrings, 
				  ringsum,
				  0,
				  nrings,
				  highlight == -1 ? 0: highlight,
				  scantime,
				  entry_update,
				  NULL,
				  /*t_prparams*/ NULL,
				  standard_keycmds);

	  /* clear up ring summaries */
	  itree_traverse(ringsum)
	       nfree(itree_get(ringsum));
	  itree_destroy(ringsum);

	  switch(token) {
	  case '\r':
	  case KEY_ENTER:
	       /* A ring has been selected. Get its key, which will
	        * be an ordinal index corresponding to another tree, which 
		* we walk to find the name of the ring. */
	       sel = cursvu_getselect();
	       tree_first(ringinfo);
	       for (i=0; i<sel; i++)
		    tree_next(ringinfo);
	       if (ringbag_setring(tree_getkey(ringinfo), NULL)) {
		    /* store timestore and ring name globally */
		    ringinf = tree_get(ringinfo);
		    nfree(holname);
		    holname = xnstrdup(ringinf->tsname);
		    nfree(ringname);
		    ringname = xnstrdup(ringinf->ringname);
	       } else {
		    cursvu_drstatus("Unable to use ring");
		    continue;
	       }
	       break;
	  }

	  return(token);
     }
}

void entry_update() {
     TREE *ringinfo;
     int nrings;

     /* Make summary of available rings */
     /* 
      * Currently, this is not efficient and needs to be improved.
      * the only call available is ringbag_getallrings(), which deletes
      * the current state and re-scans the store for new rings.
      * Thus, we have to make a new list from scratch
      */
     itree_clearoutandfree(ringsum);
     nrings = ringbag_getallrings();
     ringinfo = ringbag_getrings();
     tree_traverse(ringinfo)
          itree_append(ringsum, mkringsummary(tree_get(ringinfo)));

     /*ringbag_update(100,mkringsummary);*/
     cursvu_drbuffer(nrings, ringsum, -1);

     return;
}

/*
 * Screen mode to view contents of ring and select an entry
 * Called by inteface. Returns the token pressed.
 * Relies on globals ringinf, holname and ringname to be set.
 * Default highlight (-1) is last line.
 */
int list_mode(int highlight	/* initial key; default=-1*/) {
     int token, hkey, n;

     /* Screen format:-
      * dd-mmm_____________HOLSTORE TRACKER__________hh:mm:ss
      * <holstore>,<ring>               lines: <x>-<y> of <z>
      * <entry....columns...................................>
      * <entry summary......................................>
      * <entry summary......................................>
      * <entry summary......................................>
      * .
      * .
      * <entry summary......................................>
      * Arrow keys, (h)elp, (q)uit, <ret> select
      * Sataus line, whats going on etc.
      */
     /*ringinf = ringbag_getents();*/
     if (highlight == -1) {
          ringbag_firstseq();
          ringbag_update(200, mklistsummary);
          hkey = ringbag_lastseq();
     } else
          hkey = highlight;

     while(1) {
          ringbag_scan(200, 200, hkey, mklistsummary);

	  clear();
	  cursvu_drtitle(APPTITLE);
	  cursvu_drtime();
	  cursvu_drring_spec(holname, ringname);
	  cursvu_drcols("   SEQ   WHEN   TEXT");
	  cursvu_drhelp("Arrow keys, (h)elp, (q)uit, <ret> select");

	  n = itree_n(ringinf->summary);
	  token = cursvu_navigate(n,
				  ringinf->summary,
				  ringinf->seen-n,
				  ringinf->seen+ringinf->available,
				  hkey,
				  scantime,			  
				  list_update,
				  NULL,
				  /*t_prparams*/ NULL,
				  standard_keycmds);
	  switch(token) {
	  case '\r':
	  case KEY_ENTER:
	       /* An entry has been selected from the list.
		* Get the key and store it globally
		*/
	       entrykey = cursvu_getselect();
	       return(token);
	       break;
	  case KEY_UP:
	  case KEY_PPAGE:
	       /* Moved above the top of the buffer -- get more */
	       /*highlight = cursvu_getselect();*/
	       break;
	  case KEY_DOWN:
	  case KEY_NPAGE:
	       /* Moved below the bottom of the buffer -- get more */
	       /*highlight = cursvu_getselect();*/
	       break;
	  default:
	       return(token);
	  }
     }
}

/* No argument routine to update of the ringbag's contents */
void list_update() {
     ITREE *list;
     int select;

     /* TODO this is where we put the tailing functionality. ie
      * if at end of list, move to new end */

     /* refresh lines around highlighted selection */
     select = cursvu_getselect();
     if (ringbag_scan(200, 200, select, mklistsummary) == -1) {
	  cursvu_drstatus("Unable to scan ring, possibly empty");
	  return;
     }
     /*ringbag_update(100,mklistsummary);*/
     list = ringbag_getents()->summary;
     cursvu_drbuffer(itree_n(list), list, select);

     return;
}

/* Screen mode to view an entry. 
 * Default highlight action is to display top of buffer/screen
 * Returns token of unhandled keys.
 */
int entry_mode(int highlight	/* initial key; default=-1 */ ) {
     TS_RING ts;
     int token, len, seq, timelen;
     time_t t;
     char buf[SHORTSTR], *databuf;
     struct tm *thetime;

     /* entryseq references the entry */

     /* Screen format:-
      * dd-mmm_____________HOLSTORE TRACKER__________hh:mm:ss
      * <holstore>,<ring>,<seq>         lines: <x>-<y> of <z>
      * DATA   inserted: <date>
      * <buffer text line...................................>
      * <buffer text line...................................>
      * <buffer text line...................................>
      * .
      * .
      * <buffer text line...................................>
      * (r)ings (u)pdate Arrows move
      * Sataus line, whats going on etc.
      */

     while(1) {
	  /* fetch data flagged by list_mode() in entrykey.
	   * Use the already opened timestore from ringbag and check
	   * the data is not beyond the ends of the list. */
	  ts = ringbag_getts();
	  if (entrykey < ts_oldest(ts))
	       entrykey = ts_oldest(ts);
	  if (entrykey > ts_youngest(ts))
	       entrykey = ts_youngest(ts);
	  ts_setjump(ts, entrykey-1);
	  databuf = ts_get(ts, &len, &t, &seq);
	  if (databuf == NULL) {
	       cursvu_drstatus("The ring is empty");
	       return ('\033');
	  }

	  *(databuf+len) = '\0';	/* allowed to terminate block as it
					 * allocates len+sizeof(int) bytes & 
					 * we want to treat it as a string */
	  cursvu_ldbuffer(databuf);

	  /* Display */
	  cursvu_drtime();
	  cursvu_drentry_spec(holname, ringname, seq);
	  thetime = localtime(&t);
	  timelen = strftime(buf, SHORTSTR, "DATA  -- inserted: %c", thetime);
	  sprintf(buf+timelen, " -- sequence: %d -- length: %d", 
		  seq, len);
	  cursvu_drcols(buf);
	  cursvu_drhelp("Arrow keys, (n)ext, (p)rev, (h)elp, <ESC> list, (q)uit");

	  token = cursvu_navigate(-1,	/* Use loaded buffer */
				  NULL,	/* Use loaded buffer */
				  -1, 	/* Use loaded buffer */
				  -1,	/* Use loaded buffer */
				  highlight == -1 ? 0: highlight,
				  scantime,
				  NULL,	/* update_bodies*/
				  NULL,
				  /*t_prparams*/ NULL,
				  entrymode_keycmds);
	  nfree(databuf);

	  switch(token) {
	  case '\t':	/* <TAB> key */
	  case 'n':
	  case 'N':
	       entrykey++;
	       continue;
	  case KEY_BTAB:/* <SHIFT TAB> */
	  case 'p':
	  case 'P':
	       if (entrykey > 0)
		    entrykey--;
	       continue;
	  }
	  return(token);
     }
}
