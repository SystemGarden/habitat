/*
 * iiab conversion routine
 *
 * This class contains code that converts foreign files to and from
 * native data formats.
 *
 * Nigel Stuckey, October 1999
 * Copyright System Garden Limited 1999-2001. All rights reserved
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include "conv.h"
#include "table.h"
#include "elog.h"
#include "nmalloc.h"
#include "util.h"
#include "tablestore.h"
#include "spanstore.h"

/*
 * Convert sun solaris sar format into tablestore ring.
 * Returns 0 for failure or 1 for success
 */
int conv_solsar2tab(char *sarfile, char *holname, char *ringname,
		    char *fromdate, char *todate)
{
     char *pt, cmd[CONV_SARCMDLEN], *buf, *bufnext, *usdate, *machine, *val;
     char *tmpstr;
     time_t datetime;
     TAB_RING outring;
     TABLE t, singlerow;
     int nread, totread, killtime;
     FILE *fs;
     ITREE *buflist, *bufrow, *column;
     TREE *row;
     struct tm insdate, instime;

     t = table_create();

     /* run sar several times with different commands to get different data */
     killtime = 0;
     for (pt = CONV_SARCMDS2D; *pt; pt++) {
	  sprintf(cmd, "sar -%c -f %s %s %s %s", *pt, sarfile, fromdate, 
		  todate, CONV_SARFILTER);
	  fs = popen(cmd, "r");
	  if (fs == NULL) {
	       elog_printf(ERROR, "unable to find sar");
	       return 0;
	  }

	  /* collect data from pipe */
	  totread = 0;
	  bufnext = buf = xnmalloc(PIPE_BUF +1 +100);
	  while ( (nread = fread(bufnext, 1, PIPE_BUF, fs)) > 0) {
	       totread += nread;
	       buf = xnrealloc(buf, totread + PIPE_BUF +1 +100);
	       bufnext = buf + totread;
	  }
	  *bufnext = '\0';
	  pclose(fs);

	  /* buf is now in the following format:-
	   *   <os> <host> <osver> <oslabel> <arch> <usdate>
	   *   <starttime> <head1> ... <headN>
	   *   <samptime>  <data1> ... <dataN>
	   *     (blank line)
	   */

	  /* Make headers unique, using the extra 100 bytes allocated.
	   * The problems are with the overun flags after proc-sz, inod-sz 
	   * and file-sz in option `v'. 
	   * We assume that we encounter them in that order */
	  if (*pt == 'v') {
	       util_strsub(buf, "ov ", "ov-proc-sz ");
	       util_strsub(buf, "ov ", "ov-inod-sz ");
	       util_strsub(buf, "ov ", "ov-file-sz ");
	  }

	  /* scan buf headers and collect information */
	  if (util_scantext(buf, " \t", UTIL_MULTISEP, &buflist) < 3) {
	       nfree(buf);
	       continue;
	  }
	  itree_first(buflist);
	  bufrow = itree_get(buflist);
	  machine = itree_find(bufrow, 0);
	  usdate = itree_find(bufrow, 5);
	  strptime(usdate, "%D", &insdate);
	  itree_destroy(bufrow);
	  itree_rm(buflist);		/* remove header */

	  /* get column headers */
	  itree_first(buflist);
	  bufrow = itree_get(buflist);
	  itree_rm(buflist);		/* remove column titles */
	  itree_first(bufrow);
	  if (killtime)
	       /* remove date and time form header, which will prevent
		* the column from being extracted */
	       itree_rm(bufrow);
	  else
	       /* replace the start time from sar with a meaningful title */
	       itree_put(bufrow, "instime");

	  /* form column lists and insert in table */
	  itree_traverse(bufrow) {
	       column = itree_create();
	       itree_traverse(buflist) {
		    val = itree_find(itree_get(buflist), itree_getkey(bufrow));
		    if (killtime) {
			 itree_append(column, val == ITREE_NOVAL ? NULL : val);
		    } else {
			 /* interpret time, patch in date and store as str */
			 strptime(val, "%T", &instime);
			 instime.tm_mday = insdate.tm_mday;
			 instime.tm_mon  = insdate.tm_mon;
			 instime.tm_year = insdate.tm_year;
			 instime.tm_yday = insdate.tm_yday;
			 instime.tm_wday = insdate.tm_wday;
			 datetime = mktime(&instime);
			 tmpstr = xnstrdup( util_u32toa(datetime) );
			 itree_append(column, tmpstr);
			 table_freeondestroy(t, tmpstr);
		    }
	       }
	       table_addcol(t, itree_get(bufrow), column);
	       itree_destroy(column);
	       killtime++;
	  }

	  /* free storage and loop */
	  killtime++;
	  itree_destroy(bufrow);
	  table_freeondestroy(t, buf);
	  util_scanfree(buflist);
     }

     /*
      * At this point `t' contains the converted data, with one row per
      * sample. We now have to separate these rows into a sequence of 
      * tablestore records and patch the insertion time or the datum with 
      * a synthesised time from the sar data
      */

     if (table_nrows(t) <= 0)
	  return 0;		/* no samples */

     /* open destination ring */
     outring = tab_create(holname, 0644, ringname, "converted from sar file", 
			  NULL, 0);
     if ( !  outring ) {
	  outring = tab_open(holname, ringname, NULL);
	  if ( ! outring )
	       return 0;
     }

     /* create a new table for insertion and fill with single row at
      * a time from the compiled table above (t) */
     singlerow = table_create_fromdonor(t);
     table_first(t);
     table_traverse(t) {
	  row = table_getcurrentrow(t);
	  if ( tree_find(row, "instime") == TREE_NOVAL )
	       datetime = time(NULL);
	  else
	       datetime = strtoul( tree_get(row), NULL, 0 );
	  table_addrow_noalloc(singlerow, row);
	  tab_put_withtime(outring, singlerow, datetime);
	  table_first(singlerow);
	  table_rmcurrentrow(singlerow);
	  tree_destroy(row);
     }

     tab_close(outring);

     /* free storage */
     table_destroy(singlerow);
     table_destroy(t);

     return 1;
}



/*
 * Import a text representation of a ring from a file into either
 * a timestore or a tablestore.
 * See conv_mem2ts_or_tab() for details.
 * Returns -1 for faiure or the number of data samples placed into the
 * timestore or tablestore rings.
 */
int conv_file2ring(char *infile, 	/* input file name */
		   char *holname, 	/* file name of holstore */
		   int mode, 		/* file mode */
		   char *ringname, 	/* ring name in file */
		   char *description, 	/* description of ring */
		   char *password, 	/* optional password or "" */
		   int nslots,		/* slots when creating rings */
		   char *separator, 	/* separating character */
		   int withcolnames, 	/* column name header */
		   int hasruler,		/* has header/body separator */
		   int hastimecol, 	/* has time col called _time */
		   int hasseqcol, 	/* has sequence col called _seq */
		   int hashostcol,	/* has host col called _host */
		   int hasringcol,	/* has ring col called _ring */
		   int hasdurcol	/* has duration col called _dur */)
{
     char *txt;
     int fd, r;
     struct stat sbuf;

     /* open import file */
     fd = open(infile, O_RDONLY);
     if (fd == -1) {
	  elog_printf(ERROR, "can't read file");
	  return -1;
     }

     /* import & scan file into a TABLE */
     /* get size of file */
     if (fstat(fd, &sbuf) == -1) {
	  elog_printf(ERROR, "unable to stat file %s (%s)", infile, 
		      strerror(errno));
	  close(fd);
	  return -1;
     }

     /* read data and close file */
     txt = xnmalloc(sbuf.st_size+1);
     if (read(fd, txt, sbuf.st_size) == -1) {
	  elog_printf(ERROR, "unable to read file %s (%s)", infile, 
		      strerror(errno));
	  close(fd);
	  nfree(txt);
	  return -1;
     }
     close(fd);
     txt[sbuf.st_size] = '\0';

     r = conv_mem2ring(txt, holname, mode, ringname, description, 
		       password, nslots, separator, withcolnames, 
		       hasruler, hastimecol, hasseqcol, hashostcol, 
		       hasringcol, hasdurcol);
     nfree(txt);
     return r;
}

/*
 * Import a text representation of a ring from memory into either
 * a timestore or a tablestore.
 * Specifed are the standard details to create a new ring, value separator
 * character for the import file and flags for the inclusion of column 
 * names, info/ruler lines, time column and sequence column.
 * The size of the ring will come from 'nslots' and should be big enough
 * for the number of rows being imported (including 0 for unbounded). 
 * Returns -1 for failure or the number of data samples placed into the
 * timestore or tablestore rings.
 * `intext' will be subject to scanning and changed, so it must be
 * writable.
 */
int conv_mem2ring(char *intext, 	/* input text */
		  char *holname, 	/* file name of holstore */
		  int mode, 		/* file mode */
		  char *ringname, 	/* ring name in file */
		  char *description, 	/* description of ring */
		  char *password, 	/* optional password or "" */
		  int nslots,		/* slots when creating rings */
		  char *separator, 	/* separating character */
		  int withcolnames, 	/* column name header */
		  int hasruler,		/* has header/body separator */
		  int hastimecol, 	/* has time col called _time */
		  int hasseqcol, 	/* has sequence col called _seq */
		  int hashostcol,	/* has host col called _host */
		  int hasringcol,	/* has a ring name called _ring */
		  int hasdurcol		/* has duration col called _dur */)
{
     TS_RING tsid;
     TAB_RING tabid;
     TABLE tab, batchtab=NULL;
     int r;
     TREE *colnames, *row;
     time_t instime=-1;
     int seq;
     char *cell=NULL;

     /* scan into table */
     tab = table_create();
     r = table_scan(tab, intext, separator, TABLE_MULTISEP, withcolnames, 
		    hasruler);
     if (r == -1) {
	  elog_printf(ERROR, "unable to scan input data; aborting");
	  table_destroy(tab);
	  return -1;
     }

     /* work out number of columns, removing time and seq */
     colnames = table_getheader(tab);
     if (hastimecol && tree_find(colnames, CONV_TIMESTR) == TREE_NOVAL) {
	  elog_printf(ERROR, "can't find time column `%s' in "
		      "input file; aborting conversion", CONV_TIMESTR);
	  return -1;
     }
     if (hasseqcol && tree_find(colnames, CONV_SEQSTR) == TREE_NOVAL) {
	  elog_printf(ERROR, "can't find sequence column `%s' "
		      "in input file; aborting conversion", CONV_SEQSTR);
	  return -1;
     }

     /* ensure set flags are set to 1 */
     if (hastimecol)
	  hastimecol = 1;
     if (hasseqcol)
	  hasseqcol = 1;

     /* create either timestore or tablestore depending on number of
      * columns remaining. If its one column, create a timestore */
     if ( (table_ncols(tab) - hastimecol - hasseqcol) > 1) {
	  /* ** tablestore import ** */

	  /* create tablestore ring */
	  tabid = tab_create(holname, mode, ringname, description, password, 
			     nslots);
	  if (tabid == NULL) {
	       tabid = tab_open(holname, ringname, password);
	       if (tabid == NULL) {
		    elog_printf(ERROR, "can't create tablestore ring");
		    table_destroy(tab);
		    return -1;
	       }
	  }

	  /*
	   * load TABLE into open tablestore:
	   * traverse each line and attempt to batch up rows into common
	   * sequence numbers or the same time and insert the whole table 
	   * then
	   */
	  seq = -1;
	  instime = -1;
	  table_traverse(tab) {
	       /* attempt to batch the rows into sample tables 
		* by sequence or time */
	       if (hasseqcol) {
		    /* batch by sequence */
		    if (atoi(table_getcurrentcell(tab, CONV_SEQSTR)) != seq) {
			 /* new sequence: save current batch and start a 
			  * new one. Attempt to keep the right time with
			  * the batch */
			 if (seq != -1) {
			      table_rmcol(batchtab, CONV_SEQSTR);
			      table_rmcol(batchtab, CONV_TIMESTR);
			      if (hastimecol)
				   tab_put_withtime(tabid, batchtab, instime);
			      else
				   tab_put(tabid, batchtab);
			      table_destroy(batchtab);
			 }
			 batchtab = table_create_fromdonor(tab);
			 seq = atoi(table_getcurrentcell(tab, CONV_SEQSTR));
			 if (hastimecol)
			      instime = atoi(table_getcurrentcell(
				   tab, CONV_TIMESTR));
		    }
		    /* add to batch */
		    row = table_getcurrentrow(tab);
		    table_addrow_noalloc(batchtab, row);
	       } else if (hastimecol) {
		    /* batch by time */
		    if (atoi(table_getcurrentcell(tab, CONV_TIMESTR)) != 
			instime) {
			 /* new sequence: save current batch and start a 
			  * new one */
			 if (instime != -1) {
			      table_rmcol(batchtab, CONV_SEQSTR);
			      table_rmcol(batchtab, CONV_TIMESTR);
			      tab_put_withtime(tabid, batchtab, instime);
			      table_destroy(batchtab);
			 }
			 batchtab = table_create_fromdonor(tab);
			 instime = atoi(table_getcurrentcell(tab, 
							     CONV_TIMESTR));
		    }
		    /* add to batch */
		    row = table_getcurrentrow(tab);
		    table_addrow_noalloc(batchtab, row);
	       } else {
		    /* no batching possible, assume one row per sample */
		    batchtab = table_create_fromdonor(tab);
		    row = table_getcurrentrow(tab);
		    table_addrow_noalloc(batchtab, row);
		    tab_put(tabid, batchtab);
		    table_destroy(batchtab);
	       }
	  }

	  /* flush the last sample's table */
	  if (batchtab) {
	       table_rmcol(batchtab, CONV_SEQSTR);
	       table_rmcol(batchtab, CONV_TIMESTR);
	       if (hastimecol)
		    tab_put_withtime(tabid, batchtab, instime);
	       else
		    tab_put(tabid, batchtab);
	       table_destroy(batchtab);
	  }
     } else {
	  /* ** timestore import ** */

	  /* create timestore ring */
	  tsid = ts_create(holname, mode, ringname, description, password, 
			   nslots);
	  if (tsid == NULL) {
	       tsid = ts_open(holname, ringname, password);
	       if (tsid == NULL) {
		    elog_printf(ERROR, "can't create timestore ring");
		    table_destroy(tab);
		    return -1;
	       }
	  }

	  /*
	   * load TABLE into open holstore:
	   * traverse each line, looking for a spare cell after the time 
	   * and sequence cells have been discounted; insert that!
	   */
	  table_traverse(tab) {
	       /* get data */
	       row = table_getcurrentrow(tab);

	       /* find insertion time and sequence of each row */
	       if (hastimecol)
		    instime = atoi(tree_find(row, CONV_TIMESTR));
	       if (hasseqcol)
		    seq = atoi(tree_find(row, CONV_SEQSTR));

	       /* find data */
	       tree_traverse(row) {
		    if (strcmp(tree_getkey(row), CONV_TIMESTR) == 0)
			 continue;
		    if (strcmp(tree_getkey(row), CONV_SEQSTR) == 0)
			 continue;
		    cell = tree_get(row);
	       }
	       tree_destroy(row);

	       /* insert, including the string terminating null */
	       if (hastimecol)
		    r = ts_put_withtime(tsid, cell, strlen(cell)+1, instime);
	       else
		    r = ts_put(tsid, cell, strlen(cell)+1);

	       if (r == -1)
		    elog_printf(ERROR, "write of data failed (%s,%s)"
				"but continuing", holname, ringname);
	  }

	  ts_close(tsid);
     }

     return r;
}




/*
 * Convert a timestore or tablestore ring into a flat file representation
 * of a table. tablestores give the whole data as expected whereas timestores
 * give only three values: time, sequence and value.
 * Returns -1 for error or the number of characters output on success.
 */
int conv_ring2file(char *holname,	/* holstore file name */
		   char *ringname,	/* time or tablestore name */
		   char *password,	/* password or NULL */
		   char *outfile,	/* output filename */
		   char separator,	/* separator character */
		   int withtitle,	/* output column titles in header */
		   int withruler,	/* output info rows and header 
					 * terminator '--' */
		   int withtimecol,	/* output time column _time */
		   char *dtformat,	/* date time format, empty for epoch
					 * time or strftime() format */
		   int withseqcol,	/* output sequence column _seq */
		   int withhostcol,	/* output hostname column _host*/
		   int withringcol,	/* output ringname column _ring */
		   int withdurcol,	/* output duration column _dur */
		   time_t from,		/* begin cons, 0=not consolidated */
		   time_t to		/* end cons */ )
{
     int fd;
     char *txt;
     int len, r;

     /* get the data */
     txt = conv_ring2mem(holname, ringname, password, separator,
			 withtitle, withruler, withtimecol, dtformat,
			 withseqcol, withhostcol, withringcol, withdurcol,
			 from, to);
     if (txt == NULL) {
	  elog_printf(ERROR, "unable to convert output data; aborting");
	  return -1;
     }

     /* open output file */
     fd = open(outfile, O_WRONLY | O_CREAT, 0644);
     if (fd == -1) {
	  elog_printf(ERROR, "can't create file");
	  return -1;
     }

     /* output file */
     len = strlen(txt);
     r = write(fd, txt, len);

     /* close off file and clean up */
     close(fd);
     nfree(txt);

     return r;
}



/*
 * Convert a timestore or tablestore ring into a flat representation
 * of a table in memory. Tablestores give the whole data as expected
 * whereas timestores give only three values: time, sequence and value.
 * If from == 0 then just exxtract the specified ring from the file;
 * otherwise, use from and to to define the consolidated table boundary 
 * and the ringname as a root name.
 * Returns NULL for error or the text buffer output on success, which 
 * should be nfree()ed when the caller is finished woih it.
 */
char *conv_ring2mem(char *holname,	/* holstore file name */
		    char *ringname,	/* time or tablestore name */
		    char *password,	/* password or NULL */
		    char separator,	/* separator character */
		    int withtitle,	/* column titles in header */
		    int withruler,	/* info rows and header 
					 * terminator '--' */
		    int withtimecol,	/* time column */
		    char *dtformat,	/* date time fmt, see strftime
					 * of blank for epoch secs */
		    int withseqcol,	/* output sequence column */
		    int withhostcol,	/* output hostname column _host*/
		    int withringcol,	/* output ringname column _ring */
		    int withdurcol,	/* output duration column _dur */
		    time_t from,	/* begin cons, 0=not consolidated */
		    time_t to		/* end cons */ )
{
     TS_RING tsid;
     TAB_RING tabid=NULL;
     TABLE stab, tab;
     char *txt, tmstr[30], *periodstr, *host, *ring;
     time_t t;
     int period, ringlen, hostlen;

     /* open file */
     tsid = ts_open(holname, ringname, password);
     if (tsid == NULL) {
	  elog_printf(ERROR, "can't open ring");
	  return NULL;
     }

     /* save host name, do not allow the fully qualified name */
     host = nstrdup(ts_host(tsid));
     hostlen = strcspn(host, ".");
     host[hostlen] ='\0';

     /* check if ring is a tablestore by attempting to read an associated
      * spanstore. */
     stab = spans_readblock(tsid);
     if (stab == NULL) {
	  /* export timestore ring */
	  tab = ts_mget_t(tsid, CONV_MAXROWS);
	  ts_close(tsid);
	  if (tab == NULL) {
	       nfree(host);
	       return NULL;
	  }
     } else {
	  table_destroy(stab);
	  tabid = tab_open_fromts(tsid);

	  /* export tablestore ring */
	  tab = tab_mget_byseqs(tabid, tab_oldest(tabid), tab_youngest(tabid));
	  tab_close(tabid);
	  if (tab == NULL) {
	       nfree(host);
	       return NULL;
	  }
     }

     /* prepare time and sequence options */
     if ( withtimecol ) {
	  if (dtformat != NULL && *dtformat != '\0') {
	       /* transform epoc time into a formatted date time */
	       table_traverse(tab) {
		    t = atoi(table_getcurrentcell(tab, "_time"));
		    strftime(tmstr, 30, dtformat, localtime(&t));
		    table_replacecurrentcell_alloc(tab, "_time", tmstr);
	       }
	  }
     } else
	  table_rmcol(tab, "_time");
     if ( ! withseqcol )
	  table_rmcol(tab, "_seq");

     /* add host, ring and duration columns, if asked */
     if (withhostcol) {
       table_addcol(tab, "_host", NULL);
       table_traverse(tab) {
	 table_replacecurrentcell_alloc(tab, "_host", host);
       }
     }
     if (withringcol) {
       /* calculate ring name from name stem excluding period */
       ringlen = strcspn(ringname, "0123456789");
       ring = nmemdup(ringname, ringlen+1);
       ring[ringlen] = '\0';
       table_addcol(tab, "_ring", NULL);
       table_traverse(tab) {
	 table_replacecurrentcell_alloc(tab, "_ring", ring);
       }
     }
     if (withdurcol) {
       /* calculate time period from ring name, which may be 0 for error  */
       periodstr = ringname + strcspn(ringname, "0123456789");
       if (periodstr == ringname) {
	 elog_printf(ERROR, "unable time find ring period");
	 period=0;
       }
       if (periodstr == NULL)
	 periodstr="0";		/* name not structured with period */

       table_addcol(tab, "_dur", NULL);
       table_traverse(tab) {
	 table_replacecurrentcell_alloc(tab, "_dur", periodstr);
       }
     }

     /* format for output */
     txt = table_outtable_full(tab, separator, withtitle, withruler);
     table_destroy(tab);
     nfree(host);
     nfree(ring);

     return txt;
}





#if TEST

#define TSARFILE1 "t.conv.sar"
#define TFILE1 "t.conv.dat"
#define TRING1 "sarimport"
#define TPURL1 "tab:" TSARFILE1 "," TRING1

int main(int argc, char **argv) {
     rtinf err;
     int r;

     route_init("stderr", 0, NULL);
     err = route_open("stderr", NULL, NULL, 0);
     elog_init(err, 0, "holstore test", NULL);
     hol_init(0, 0);

     /* load a copy of sar */
     r = conv_solsar2tab(TSARFILE1, TFILE1, TRING1, "", "");
     if (r==0)
	  exit(1);

     elog_fini();
     route_close(err);
     route_fini();

     return 0;
}

#endif

