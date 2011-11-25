/*
 * Habitat's interactive ring store tool  -  habrs
 * Makes ringstore API available from the command line together with some
 * convenience and management functions.
 *
 * Nigel Stuckey, July 2004
 * Copyright System Garden Ltd 2004-11. All rights reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <ctype.h>
#include <string.h>
#include "../iiab/tree.h"
#include "../iiab/itree.h"
#include "../iiab/nmalloc.h"
#include "../iiab/util.h"
#include "../iiab/elog.h"
#include "../iiab/iiab.h"
#include "../trm/cmdln.h"
#include "../iiab/rs.h"
#include "../iiab/rs_gdbm.h"


/* Functional prototypes */
int  do_open(int argc, char **argv);
int  do_create(int argc, char **argv);
int  do_ring(int argc, char **argv);
int  do_duration(int argc, char **argv);
int  do_open_call(char *fname, char *rname, int dur);
int  do_close(int argc, char **argv);
void do_close_call();
int  do_lastresort(int argc, char **argv);
int  do_rm(int argc, char **argv);
int  do_put(int argc, char **argv);
int  do_get(int argc, char **argv);
int  do_mget(int argc, char **argv);
int  do_mgetfree(int argc, char **argv);
int  do_getall(int argc, char **argv);
int  do_jump(int argc, char **argv);
int  do_jumpto(int argc, char **argv);
int  do_resize(int argc, char **argv);
int  do_stat(int argc, char **argv);
int  do_lsrings(int argc, char **argv);
int  do_inforings(int argc, char **argv);
int  do_purge(int argc, char **argv);
int  do_footprint(int argc, char **argv);
int  do_remain(int argc, char **argv);
int  do_rs(int argc, char **argv);
int  do_change(int argc, char **argv);
int  do_exit(int argc, char **argv);

/* This array of command definitions defines the interface and work
   that its will carry out */
struct cmdln_def cmds[] = {
  {"open",	do_open,	"Open an existing file & ring (see create)"},
  {"ring",	do_ring,	"Open a ring with name and duration in an already open file"},
  {"dur",	do_duration,	"Open a ring of 'duration' with an already opened file and named ring"},
  {"duration",	do_duration,	"Open a ring of 'duration' with an already opened file and named ring"},
  {"close",	do_close,	"Close a ringstore ring"},
  {"create",	do_create,	"Create and open a new ringstore ring with "
				"<n> slots. When <n>=0, it is boundless: "
				"create <n>"},
  {"rm",	do_rm,		"Delete current ring"},
  {"put",	do_put,		"Append data into ring: put <data>"},
  {"get",	do_get,		"Get oldest unread data from ring: get, "
				"returns value"},
  {"mget",	do_mget,	"Get at most n sequences of the oldest data "
				"from ring: get <n>, returns value"},
  {"getall",	do_getall,	"Get all the data in a ring"},
  {"jump",	do_jump,	"Jump relative sequences or to oldest or "
				"youngest: [+-]<n> | 'oldest' | 'youngest'"},
  {"jumpto",	do_jumpto,	"Jump to specific sequence 'n', oldest or "
				"youngest: <n> | 'oldest' | 'youngest'"},
  {"goto",	do_jumpto,	"Jump to specific sequence 'n', oldest or "
				"youngest: <n> | 'oldest' | 'youngest'"},
  {"resize",	do_resize,	"Change the number of slots in ring: resize "
				"<n>"},
  {"stat",	do_stat,	"Return statistics about current table ring"},
  {"ls",	do_lsrings,	"List all rings in ringstore datastore"},
  {"lsrings",	do_lsrings,	"List all rings in ringstore datastore"},
  {"lsl",	do_inforings,	"Long list of all rings in datastore"},
  {"purge",	do_purge,	"Delete data at sequence and everything "
				"older: purge <killbefore>"},
  {"footprint",	do_footprint,	"Print space taken by ringstore"},
  {"remain",	do_remain,	"Calculate the amount of space into which "
				"this holstore can grow"},
  {"rs",	do_rs,		"Low level ringstore information; "
				"usage: rs <info>\n"
				"info:  s | superblock  superblock\n"
				"       r | rings       rings\n"
				"       h | headers     header hash table\n"
				"       i | index       record index"},
  {"change",	do_change,	"Change ringstore information; "
				"usage: change <thing> <new val>\n"
				"info:  n | name        ring name\n"
				"       d | duration    duration\n"
				"       l | long        long name\n"
				"       s | slots       number of slots\n"
                                "       a | about       comment about ring"},
  {"exit",	do_exit,	"Exit irs"},
  {"e",		do_exit,	"Exit irs"},
  {"quit",	do_exit,	"Quit irs"},
  {"q",		do_exit,	"Quit irs"},
  {"",		NULL,		""}
};

#define RS_LONGSTR 32768
#define RS_DEFMODE 0x644

/* Globals */
RS rsid=NULL;
char *filepath, *filename, *ringname;
int duration;
char usagetxt[] = "[ purl | file,ring,dur ]\n" \
     "where purl        pseudo-url in the form grs:file,ring,dur\n"
     "      file        file containing ringstore\n"
     "      ring        ringstore ring buffer\n"
     "      dur         duration within ring (numeric only)";
char welcome[] = "Habitat Interactive Ringstore Browser\n(c) System Garden 2004-11. This is GPL software, see COPYING file";
char *cfdefaults = 
     "nmalloc    0\n"	/* 0: memory checking off, !0: memory checking on */
     "elog.all   none:\n"
     "elog.above warning stderr:\n";

int main(int argc, char *argv[]) {
     /* Initialisation */
     rsid = NULL;
     filepath = NULL;
     filename = NULL;
     ringname = NULL;
     iiab_start("", argc, argv, usagetxt, cfdefaults);
     cmdln_init(argv[0], cmds);
     cmdln_setprompt("> ");
     cmdln_setlastresort(do_lastresort);
     puts(welcome);

     /* The main work */
     if (argc > 1)
	  cmdln_run(argc-1, argv+1);
     cmdln_readloop();

     /* Finalising & exit */
     do_exit(0, NULL);
     exit(-1);
}

/* ----- The callbacks from here on ----- */

/* Various degrees of 'open'. 
 * 1. Open <file> will open a file but not the ring. 
 *    You can use file level commands like ls, stat and rs.
 *    You should specify the ring using 'ring <ringname> <duration>' and 
 *    you get the same as (3) below
 * 2. Open <file> <ring> will open the file and name the ring, but it
 *    cannot be fully opened until the duration is given. 
 *    Specify the duration using 'dur <dur>' or 'duration <dur>' which will
 *    open the ring fully giving you access to full set of commands like
 *    get and put.
 * 3. Open <file> <ring> <duration> will open the ring with all commands
 *    available to access data.
 * 4. Open <file>,<ring> if its in a route nomeclature, then the same as (2)
 *    and will be partly opened
 * 5. Open <file>,<ring>,<duration> if its in a route nomeclature, 
 *    then the same as (3) and will be fully open
 */
int do_open(int argc, char **argv) {
     char *fname=NULL, *rname=NULL, *dur_s;
     int r, dur=0;

     /* check for a route style address: host,ring[,dur] */
     if (argc == 2 && (rname = strchr(argv[1], ','))) {
	  rname++;
	  fname = xnstrdup(argv[1]);
	  strtok(fname, ",");
	  if ( (dur_s = strchr(rname, ',')) ) {
	       dur_s++;
	       dur = strtol(dur_s, (char**)NULL, 10);
	       strtok(rname, ",");
	  }
	  if (!access(fname, R_OK))
	       r = do_open_call(fname, rname, dur);
	  else
	       r = do_open_call(fname, NULL, dur);
	  nfree(fname);
	  return r;
     } else if (argc == 2) {
          return do_open_call(argv[1], NULL, 0);
     } else if (argc == 3) {
          return do_open_call(argv[1], argv[2], 0);
     } else if (argc == 4) {
       return do_open_call(argv[1], argv[2], strtol(argv[3], (char**)NULL, 
						    10));
     } else {
	  printf("Usage: open <file> [<ring> [ <duration> ] ]\n");
	  return 1;	/* fail */
     }
}

/* Open a ring having already opened a file */
int do_ring(int argc, char **argv) {
     char *sfile;
     int r;

     if (!filename) {
	  printf("Open file first\n");
	  return 1;	/* fail */
     } else if (argc == 2) {
	  sfile = xnstrdup(filepath);
	  r = do_open_call(sfile, argv[1], 0);
	  nfree(sfile);
	  return r;
     } else if (argc == 3) {
	  sfile = xnstrdup(filepath);
	  r = do_open_call(sfile, argv[1], strtol(argv[2], (char**)NULL, 10));
	  nfree(sfile);
	  return r;
     } else {
	  printf("Usage: ring <ringname> [<duration>]\n");
	  return 1;	/* fail */
     }
}

/* Open a ring with duration having already opened a file and named the ring */
int do_duration(int argc, char **argv) {
  char *sfile, *rname;
     int r;

     if (!filename) {
	  printf("Open file first\n");
	  return 1;	/* fail */
     } else if (!ringname) {
	  printf("Specify ring name first\n");
	  return 1;	/* fail */
     } else if (argc == 2) {
	  sfile = xnstrdup(filepath);
	  rname = xnstrdup(ringname);
	  r = do_open_call(sfile, rname, strtol(argv[1], (char**)NULL, 10));
	  nfree(sfile);
	  nfree(rname);
	  return r;
     } else {
	  printf("Usage: duration <dur>\n");
	  return 1;	/* fail */
     }
}

/* Open ringstore using rname, not myargv */
int do_open_call(char *fname, char *rname, int dur)
{
     RS rs;
     TABLE ls_ring;
     char prompt[80], *ls_text;

     if (access(fname, R_OK)) {
	  printf("Unable to access: %s\nUse `create' or check the name\n", 
		 fname);
	  return 1;	/* fail */
     }

     if (rname == NULL) {
	  /* print a list of rings to help using underlying holstore calls */
	  ls_ring = rs_lsrings(&rs_gdbm_method, fname);
	  if (ls_ring) {
	       ls_text = table_print(ls_ring);
	       printf("%s contains the following rings:\n%s\n"
		      "type 'ring <name> <duration>' to open a ring\n", 
		      fname, ls_text);
	       nfree(ls_text);
	       table_destroy(ls_ring);
	  } else {
	       printf("no rings\n");
	  }

	  do_close_call();
	  filepath = xnstrdup(fname);
	  filename = xnstrdup(util_basename(fname));
	  sprintf(prompt, "%s> ", filename);
	  cmdln_setprompt(prompt);
	  return 0;	/* success */
     }

     /* Attempt to open new ringstore */
     rs = rs_open(&rs_gdbm_method, fname, 0644, rname, "dont create", 
		  "dont create", 0, dur, 0);
     if (!rs) {
       printf("Can't open ring: %s,%s,%d\n", fname,rname,dur);
	  return 1;	/* fail */
     }

     /* Register the new ring for the rest of irs */
     do_close_call();
     rsid = rs;
     filepath = xnstrdup(fname);
     filename = xnstrdup(util_basename(fname));
     ringname = xnstrdup(rname);
     duration = dur;
     sprintf(prompt, "%s,%s,%d> ", filename, ringname, duration);
     cmdln_setprompt(prompt);
     return 0;
}


int do_create(int argc, char **argv) {
     RS rs;
     char prompt[80];
     int mode, nslots, dur;

     if (argc != 8) {
	  printf("Usage: create <file> <perm> <ring> <lname> <desc> <nslots> <dur>\n");
	  printf("where <file>   name of holstore file to contain the ring\n");
	  printf("      <perm>   file permissions (eg 0644)\n");
	  printf("      <ring>   name of ringstore ring\n");
	  printf("      <lname>  long name of ring\n");
	  printf("      <desc>   text description of ring\n");
	  printf("      <nslots> number of slots in ring, 0 for unlimitied\n");
	  printf("      <dur>    secs duration of each sample, 0 for irregular\n");
	  return 1;
     }
     if (!access(argv[1], R_OK)) {
	  printf("File exists: %s\nUse `open' or try a new name\n", argv[1]);
	  return 1;
     }
     if (!sscanf(argv[2], "%o", &mode)) {
	  printf("Unable to read file permissions\n");
	  return 1;
     }
     nslots = strtol(argv[6], (char**)NULL, 10);
     dur = strtol(argv[7], (char**)NULL, 10);

     /* Attempt to create new ringstore ring */
     rs = rs_open(&rs_gdbm_method, argv[1], mode, argv[3], argv[4],
		  argv[5], nslots, dur, RS_CREATE);
     if (!rs) {
	  printf("Can't create ring: %s,%s\n", argv[1], argv[3]);
	  return 1;	/* fail */
     }

     if (rsid) {
	  rs_close(rsid);
	  nfree(filepath);
	  nfree(filename);
	  nfree(ringname);
     }

     /* Register the new ring for the rest of ints */
     do_close_call();
     rsid = rs;
     filepath = xnstrdup(argv[1]);
     filename = xnstrdup(util_basename(argv[1]));
     ringname = xnstrdup(argv[3]);
     duration = dur;
     sprintf(prompt, "%s,%s,%d> ", filename, ringname, duration);
     cmdln_setprompt(prompt);

     return 0;
}

/* When no command matches, run this to see if we can find something to do */
int do_lastresort(int argc, char **argv) {
     char *fname=NULL, *rname=NULL, *dur_s;
     int dur=0, r;

     if (argc < 1 || argc > 4)
	  return 1;

     /* check for a route style address: host,ring[,dur] */
     if (argc == 1 && (rname = strchr(argv[0], ','))) {
	  rname++;
	  fname = xnstrdup(argv[0]);
	  strtok(fname, ",");
	  if ( (dur_s = strchr(rname, ',')) ) {
	       dur_s++;
	       dur = strtol(dur_s, (char**)NULL, 10);
	       strtok(rname, ",");
	  }
     }     

     if (fname) {
	  if (access(fname, R_OK))
	       r = 1;
	  else
	       r = do_open_call(fname, rname, dur);
	  nfree(fname);
     } else {
	  if (access(argv[0], R_OK))
	       return 1;
	  if (argc == 1)
	       r = do_open_call(argv[0], NULL, 0);
	  if (argc == 2)
	       r = do_open_call(argv[0], argv[1], 0);
	  if (argc == 3)
	    r = do_open_call(argv[0], argv[1], strtol(argv[2], (char**)NULL, 
						      10));
     }

     return r;
}


/* Close ringstore */
int do_close(int argc, char **argv) {
     do_close_call();

     return 0;
}

void do_close_call() {
     if (rsid)
	  rs_close(rsid);
     if (filepath)
	  nfree(filepath);
     if (filename)
	  nfree(filename);
     if (ringname)
	  nfree(ringname);
     rsid     = NULL;
     filepath = NULL;
     filename = NULL;
     ringname = NULL;
     cmdln_setprompt("> ");
}

/* Delete ring */
int do_rm(int argc, char **argv) {
     char *answer;
     int r;

     /* If we have enough, collect the arguments */
     if (argc < 2) {
	  printf("Usage: rm <this ring name>\n");
	  return 1;
     }
     if (rsid == NULL) {
	  printf("Ring not open\n");
	  return 1;
     }

     if (strcmp(ringname, argv[1])) {
	  printf("Ring names do not match\n");
	  return 1;
     }

     answer = readline("Are you sure you want to remove this ring "
		       "(yes or no)? ");
     nadopt(answer);		/* let the memory check keep tabs on this */
     if (strcmp(answer, "yes") && strcmp(answer, "YES")) {
	  nfree(answer);
	  printf("Not removing ring\n");
	  return 1;
     }
     nfree(answer);

     /* get out of the ring before we remove it */
     rs_close(rsid);
     rsid = NULL;
     cmdln_setprompt("> ");

     r = rs_destroy(&rs_gdbm_method, filepath, ringname);
     nfree(filepath);
     nfree(filename);
     nfree(ringname);
     filepath = NULL;
     filename = NULL;
     ringname = NULL;
     if (!r) {
	  printf("Unable to remove ring\n");
	  return 1;
     }

     return 0;
}

/* Put data onto the end of the ring */
int do_put(int argc, char **argv) {
     int nlines, len, buflen;
     char *line, *buffer;
     TABLE tab;

     /* If we have enough, collect the arguments */
     if (argc > 1) {
	  printf("Usage: %s  (type FHA table and end with blank line)\n", 
		 argv[0]);
	  return 1;
     }
     if (!rsid) {
	  printf("Ring not open\n");
	  return 1;
     }

     /* collect an input buffer */
     puts("type FHA table and end with a blank line");
     buffer = line = NULL;
     buflen = 0;
     while ((line = readline("(table) "))) {
	  if (line == NULL || *line == '\0')
	       break;
	  len = strlen(line);
	  buffer = xnrealloc(buffer, buflen + len+2);
	  strncpy(buffer+buflen, line, len);
	  buflen += len+1;
	  buffer[buflen-1] = '\n';
	  buffer[buflen]   = '\0';
     }

     /* convert to TABLE representation */
     tab = table_create();
     nlines = table_scan(tab, buffer, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
			 TABLE_HASRULER);
     if (nlines == -1) {
	  printf("Unable to scan table. Aborted\n");
	  return 1;
     }

     if (rs_put(rsid, tab)) {
	  printf("Saved %d lines\n", nlines);
	  return 0;
     } else {
	  printf("Error saving table. Aborted\n");
	  return 1;
     }

#if 0
     /* apply escaping rules to form a binary */
     l = strlen(argv[1]);
     binblock = nmalloc(l);
     l = util_strtobin(binblock, argv[1], l);
#endif
}


/* Get unread datum off the ring */
int do_get(int argc, char **argv) {
     char *data;
     int seq, dur;
     time_t tim;
     TABLE dtab;

     if (argc > 1) {
	  printf("Usage: get, returns <data>\n");
	  return 1;
     }
     if (!rsid) {
	  printf("Ring not open\n");
	  return 1;
     }

     /* Print results */
     dtab = rs_get(rsid, 1);
     if (dtab) {
	  if (table_nrows(dtab) < 1) {
	       printf("Empty data\n");
	       return 1;
	  }

	  /* As these are single gets, chop off the _seq and _time columns,
	   * after saving the values from the first row and print them as
	   * a status line */
	  table_first(dtab);
	  seq = strtol(table_getcurrentcell(dtab, "_seq"), (char**)NULL, 10);
	  tim = strtol(table_getcurrentcell(dtab, "_time"), (char**)NULL, 10);
	  dur = strtol(table_getcurrentcell(dtab, "_dur"), (char**)NULL, 10);
	  table_rmcol(dtab, "_seq");
	  table_rmcol(dtab, "_time");
	  table_rmcol(dtab, "_dur");
	  data = table_print(dtab);
	  puts(data);
	  printf("%d line%s, sequence %d, %s, %d %s\n", table_nrows(dtab), 
		 table_nrows(dtab) == 1 ? "" : "s", seq, 
		 util_decdatetime(tim), dur, dur ? "seconds" : "(irregular)");
	  nfree(data);
	  table_destroy(dtab);
	  return 0;
     } else {
	  printf("No new data\n");
	  return 1;
     }

     return 0;
}


/* Get a collection of unread datum off the ring */
int do_mget(int argc, char **argv) {
     char *data;
     TABLE dtab;

     /* Take procautions */
     if (argc < 2) {
	  printf("Usage mget <ndata>, returns <ndata> values from current point\n");
	  return 1;
     }
     if (rsid == NULL) {
	  printf("Ring not open\n");
	  return 1;
     }

     /* Print results */
     dtab = rs_mget_nseq(rsid, strtol(argv[1], (char**)NULL, 10));
     if (dtab) {
	  data = table_print(dtab);
	  puts(data);
	  printf("%d lines\n", table_nrows(dtab));
	  nfree(data);
	  table_destroy(dtab);
	  return 0;
     } else {
	  printf("No new data\n");
	  return 1;
     }
}


/* Get all datum off the ring */
int do_getall(int argc, char **argv) {
#if 0
     int r, from, to;
     char *header=NULL;
     ITREE *list;
     ntsbuf *dat;
     char *dhead, *data;
     TABLE dtab;

     /* Take procautions */
     if (argc > 1) {
	  printf("Usage: getall, returns all data\n");
	  return 1;
     }
     if (rsid == NULL) {
	  printf("Ring not open\n");
	  return 1;
     }

     rs_jumpoldest(rsid);
     dtab = rs_mget_byseqs(rsid, 99999);
     if (dtab) {
	  data = table_print(dtab);
	  puts(data);
	  printf("%d lines\n", table_nrows(dtab));
	  nfree(data);
	  table_destroy(dtab);
	  return 0;
     } else {
	  printf("No new data\n");
	  return 1;
     }
#endif
     return 0;
}


/* Jump around in ring */
int do_jump(int argc, char **argv) {
     int r, seq;
     time_t tim;

     /* Take procautions */
     if (!rsid) {
	  printf("Ring not open\n");
	  return 1;
     }
     if (argc < 2) {
	  printf("Usage: jump: [+-]<n> | oldest | youngest\n");
	  return 1;
     }

     /* What do we want to do? */
     if (*argv[1] == 'o' || *argv[1] == 'O') {
	  /* jump to oldest part of ring */
	  rs_oldest(rsid, &seq, &tim);
	  r = rs_goto_seq(rsid, seq);
     } else if (*argv[1] == 'y' || *argv[1] == 'Y') {
	  /* jump to newest part of ring */
	  rs_youngest(rsid, &seq, &tim);
	  r = rs_goto_seq(rsid, seq);
     } else if (*argv[1] == '-') {
       r = rs_rewind(rsid, strtol(argv[1]+1, (char**)NULL, 10));
     } else if (*argv[1] == '+' || isdigit((int)*argv[1])) {
	  /* jump relative */
       r = rs_forward(rsid, strtol(argv[1], (char**)NULL, 10));
     } else {
	  printf("Usage: jump: [+-]<n> | oldest | youngest\n");
	  return 1;
     }

     if (r == -1) {
	  printf("Unable to jump beyond ends of ring\n");
	  return 1;
     }

     return 0;
}


/* Jump to absolute sequences */
int do_jumpto(int argc, char **argv) {
     int r, seq;
     time_t tim;

     /* Take procautions */
     if (!rsid) {
	  printf("Ring not open\n");
	  return 1;
     }
     if (argc < 2) {
	  printf("Usage: %s: <n> | oldest | youngest\n", argv[0]);
	  return 1;
     }

     /* What do we want to do? */
     if (*argv[1] == 'o' || *argv[1] == 'O') {
	  /* jump to oldest part of ring */
	  rs_oldest(rsid, &seq, &tim);
	  r = rs_goto_seq(rsid, seq);
     } else if (*argv[1] == 'y' || *argv[1] == 'Y') {
	  /* jump to newest part of ring */
	  rs_youngest(rsid, &seq, &tim);
	  r = rs_goto_seq(rsid, seq);
     } else if (isdigit((int)*argv[1])) {
	  /* jump absolute */
       r = rs_goto_seq(rsid, strtol(argv[1], (char**)NULL, 10));
     } else {
	  printf("Usage: %s: <n> | oldest | youngest\n", argv[0]);
	  return 1;
     }

     if (r == -1) {
	  printf("Unable to jump beyond ends of ring\n");
	  return 1;
     }

     return 0;
}



/* Resize the current ring. If shrinking the ring, oldest data will be lost */
int do_resize(int argc, char **argv) {
#if 0
     int r;

     /* Take procautions */
     if (!rsid) {
	  printf("Ring not open\n");
	  return 1;
     }
     if (argc < 2) {
	  printf("Usage: resize <nslots>\nChange number of slots in ring to <nslots>\nIf shrinking may cause oldest data to be lost\n");
	  return 1;
     }
     if ( ! isdigit((int)*argv[1])) {
	  printf("Usage: resize <nslots>\n<nslots> should be an integer\n");
	  return 1;
     }

     r = tab_resize(rsid, strtol(argv[1], (char**)NULL, 10));
     if (!r) {
	  printf("Failure\n");
	  return 1;
     }
#endif

     return 0;
}


/* print statistics on ring */
int do_stat(int argc, char **argv) {
     int dur, nslots, old, oldt, young, youngt, cur;
     unsigned long oldhash, younghash;
     char *oldts, *youngts;

     if (!rsid) {
	  printf("Ring not open\n");
	  return 1;
     }

     if ( ! rs_stat(rsid, &dur, &nslots, &old, &oldt, &oldhash, 
		    &young, &youngt, &younghash, &cur)) {
	  printf("Unable to get statistics\n");
	  return 1;
     }

     if (old > -1) {
          oldts   = util_strjoin("time ", util_decdatetime(oldt), NULL);
          youngts = util_strjoin("time ", util_decdatetime(youngt), NULL);
     } else {
          oldts   = xnstrdup("empty");
	  youngts = xnstrdup("empty");
     }

     printf("file:     %s\n"
	    "ring:     %s\n"
	    "duration: %d\n"
	    "nslots:   %d\n"
            "oldest:   %d (%s, header hash %lu)\n"
	    "current   %d\n"
            "youngest: %d (%s, header hash %lu)\n"
	    "slots read: %d, slots available: %d\n",
	    filename, ringname, dur, nslots, old, oldts, oldhash,
	    cur, young, youngts, younghash, cur-old, young-cur+1);

     nfree(oldts);
     nfree(youngts);

     return 0;
}


/* print a listing of al the rings in the ringstore */
int do_lsrings(int argc, char **argv) {
     TABLE tab;
     char *text;

     if (!filepath || !*filepath) {
	  printf("No file open: please use 'open' to specify one\n");
	  return 1;
     }
     if (argc > 1) {
	  printf("Usage %s\n", argv[0]);
	  return 1;
     }

     /* get list, remove the id and long name then print it */
     tab = rs_lsrings(&rs_gdbm_method, filepath);
     if (!tab) {
	  printf("Unable to list rings\n");
	  return 1;
     }
     table_rmcol(tab, "id");
     table_rmcol(tab, "long");
     text = table_print(tab);
     puts(text);
     nfree(text);
     table_destroy(tab);

     return 0;
}


/* print a long listing of all the rings in the ringstore */
int do_inforings(int argc, char **argv) {
     TABLE tab;
     char *text, *cell, *newcell;

     if (!filepath || !*filepath) {
	  printf("No file open: please use 'open' to specify one\n");
	  return 1;
     }
     if (argc > 1) {
	  printf("Usage %s\n", argv[0]);
	  return 1;
     }

     /* get list, format the time columns in to human readable and print it */
     tab = rs_inforings(&rs_gdbm_method, filepath);
     if (!tab) {
	  printf("Unable to list ring information\n");
	  return 1;
     }
     table_traverse(tab) {
          cell = table_getcurrentcell(tab, "otime");
	  newcell = util_shortadaptdatetime(atoi(cell));
	  table_replacecurrentcell_alloc(tab, "otime", newcell);
          cell = table_getcurrentcell(tab, "ytime");
	  newcell = util_shortadaptdatetime(atoi(cell));
	  table_replacecurrentcell_alloc(tab, "ytime", newcell);
     }
     text = table_print(tab);
     puts(text);
     nfree(text);
     table_destroy(tab);

     return 0;
}


/* Purge entries from ring */
int do_purge(int argc, char **argv) {

#if 0
     /* If we have enough, collect the arguments */
     if (argc < 2) {
	  printf("Usage: purge <kill this and before>\n");
	  return 1;
     }
     if (rsid == NULL) {
	  printf("Ring not open\n");
	  return 1;
     }

     if ( ! tab_purge(rsid, strtol(argv[1], (char**)NULL, 10))) {
	  printf("Unable to purge\n");
	  return 1;
     }
#endif

     return 0;
}


/* Return the size of the entire ringstore in bytes */
int do_footprint(int argc, char **argv) {
#if 0
     int r;

     if (!rsid) {
	  printf("Ring not open\n");
	  return 1;
     }

     r = tab_footprint(rsid);
     if (r == -1)
	  printf("Failed to get size of ringstore\n");
     else
	  printf("ringstore occupies %d bytes\n", r);
#endif

     return 0;
}


/* Return the curent amount of space into which ringstore can grow in bytes */
int do_remain(int argc, char **argv) {
#if 0
     int r;

     if (!rsid) {
	  printf("Ring not open\n");
	  return 1;
     }

     r = tab_remain(rsid);
     if (r == -1)
	  printf("Failed to get remaining space\n");
     else
	  printf("%d bytes remaining\n", r);
#endif

     return 0;
}



/*
 * Return low level ringstore tables
 * argv[1] values are: (s)uperblock, (r)ings, (h)eaders & (i)index
 */
int do_rs(int argc, char **argv) {
     RS_SUPER super;
     TABLE tab;
     char *tabtxt;

     if (argc < 2 || argc > 3) {
	  printf("Usage: %s <info>\n"
		 "info:  s | superblock   superblock\n"
		 "       r | rings        ring directory\n"
		 "       h | headers      header hash table\n"
		 "       i | index        record index\n", argv[0]);
	  return 1;
     }
     if (!filename) {
	  printf("Open file first\n");
	  return 1;	/* fail */
     }
     if (rsid == NULL) {
	  printf("Ring not open\n");
	  return 1;
     }

     /* 
      * Because we know that only the commands above call us, we can
      * be mean about the checking that we do, in the name of performance
      */
     switch (*argv[1]) {
     case 's':			/* superblock */
          super = rs_info_super(&rs_gdbm_method, filepath);
	  printf("ringstore version: %d, file created: %s\n"
		 "creating system:   %s, %s, %s\n"
		 "creating hardware: %s\n"
		 "creating host:     %s, domain: %s\n"
		 "secs west of GMT:  %d, generation: %d, ringcounter: %d\n",
		 super->version, util_decdatetime(super->created), 
		 super->os_name, super->os_release, super->os_version, 
		 super->machine,
		 super->hostname, super->domainname, 
		 super->timezone, super->generation, super->ringcounter);
	  rs_free_superblock(super);
	  return 0;
	  break;
     case 'r':			/* ring directory */
	  tab = rs_info_ring(rsid);
	  break;
     case 'h':			/* header hash table */
	  tab = rs_info_header(rsid);
	  break;
     case 'i':			/* record index table */
	  tab = rs_info_index(rsid);
	  break;
     default:
	  printf("unknown information; 'help %s' for usage\n", argv[0]);
	  return 1;
     }

     /* print table result */
     tabtxt = table_print(tab);
     printf("%s", tabtxt);
     nfree(tabtxt);
     table_destroy(tab);
     return 1;
}



/*
 * Return low level ringstore tables
 * argv[1] values are: (n)name, (d)duration, (l)longname, (s)lots & (a)bout
 */
int do_change(int argc, char **argv) {
     char prompt[80];
     int r;

     if (argc != 3) {
	  printf("Usage: %s <thing> <new val>\n"
		 "thing: n | name        ring name\n"
		 "       d | duration    duration\n"
		 "       l | long        long name\n"
		 "       s | slots       number of slots\n"
		 "       a | about       comment about ring\n", argv[0]);
	  return 1;
     }
     if (!filename) {
	  printf("Open file first\n");
	  return 1;	/* fail */
     }
     if (rsid == NULL) {
	  printf("Ring not open\n");
	  return 1;
     }

     /* 
      * Because we know that only the commands above call us, we can
      * be mean about the checking that we do, in the name of performance
      */
     switch (*argv[1]) {
     case 'n':			/* ring name */
	  r = rs_change_ringname(rsid, argv[2]);
	  if ( ! r )
	       return 1;
	  if (ringname)
	       nfree(ringname);
	  ringname = xnstrdup(argv[2]);
	  sprintf(prompt, "%s,%s,%d> ", filename, ringname, duration);
	  cmdln_setprompt(prompt);
	  break;
     case 'd':			/* duration */
	  if ( ! isdigit((int)*argv[2])) {
	       printf("Usage: %s duration <secs>\n"
		      "where <secs> must be an integer\n", argv[0]);
	       return 1;
	  }
	  r = rs_change_duration(rsid, strtol(argv[2], (char**)NULL, 10));
	  if ( ! r )
	       return 1;
	  duration = strtol(argv[2], (char**)NULL, 10);
	  sprintf(prompt, "%s,%s,%d> ", filename, ringname, duration);
	  cmdln_setprompt(prompt);
	  break;
     case 'l':			/* long name */
	  r = rs_change_longname(rsid, argv[2]);
	  if ( ! r )
	       return 1;
	  break;
     case 's':			/* number of slots */
	  if ( ! isdigit((int)*argv[2])) {
	       printf("Usage: %s slots <nslots>\n"
		      "where <nslots> must be an integer\n", argv[0]);
	       return 1;
	  }
	  r = rs_resize(rsid, strtol(argv[2], (char**)NULL, 10));
	  if ( ! r )
	       return 1;
	  break;
     case 'a':			/* comment about ring */
	  r = rs_change_comment(rsid, argv[2]);
	  if ( ! r )
	       return 1;
	  break;
     default:
	  printf("unknown information; 'help %s' for usage\n", argv[0]);
	  return 1;
     }

#if 0
     TABLE tab;
     char *tabtxt;

     /* print table result */
     tabtxt = table_print(tab);
     printf("%s", tabtxt);
     nfree(tabtxt);
     table_destroy(tab);
#endif
     return 1;
}



/*
 * Exit the tool
 */
int do_exit(int argc, char **argv) {
     do_close_call();
     cmdln_fini();
     iiab_stop();

     exit(0);		/* exit with a success */
     return 0;		/* should never run */
}
