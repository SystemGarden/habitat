/*
 * Class to manipulate and manage a pool of timestore rings.
 *
 * All the timestores at which you want to look are first provided by
 * the user of the class: _addts() _rmts() _rmallts(). This list may be
 * inspected by using the TREE _ts, which contains the file names in
 * its index.
 *
 * Then use _getallrings() [_rmallrings()] to obtain or refresh a list
 * of all the rings in the pool of timestore rings. The list of names
 * be inspected by using the TREE _ring, which has a compound key consisting
 * of <timestore>,<ringname> and a body which points to a ringent structure.
 *
 * Select a ring by using _setring(), providing the compound name from
 * the _ring TREE. The ring may be deselected with _unsetring().
 *
 * Two routines are provided for getting data from the timestore and 
 * summerising into a memory list. _scan() fetches data from a sequence and
 * either side of it, _update() appends new data to the  emory list. Both
 * routines remove old data from the ring (user specified amount) and
 * take a user written routine to summerise the data
 *
 * Nigel Stuckey, March & April 1998
 * Copyright System Garden Limitied 1998-2001. All rights reserved.
 */

#include <limits.h>
#include <string.h>
#include "itree.h"
#include "holstore.h"
#include "timestore.h"
#include "ringbag.h"
#include "nmalloc.h"
#include "route.h"
#include "elog.h"

TREE *ringbag_ts;	/* list of timestore names (in key, no bodies) */
TREE *ringbag_ring;	/* list of rings (key=ts,ring body=ringbag_ringent) */
char *ringbag_openkey;	/* key of open ring (ts,ring) */
struct ringbag_ringent *ringbag_openrent;	/* ringentry of open ring */
TS_RING ringbag_openid;	/* TS_RING id of open ring */

void ringbag_init()
{
     ringbag_ts = tree_create();
     ringbag_ring = tree_create();
     ringbag_openkey = NULL;
     ringbag_openid = 0;
}

void ringbag_fini() {
     ringbag_unsetring();
     ringbag_rmallrings();
     ringbag_rmallts();
     tree_destroy(ringbag_ts);
     tree_destroy(ringbag_ring);
}

/* Add a timestore to the coverage of ringbag.
 * Returns 1 for success, 0 for failure
 */
int ringbag_addts(char *tsname	/* name of timestore to add */)
{
     HOLD h;

     /* check its there, if so add to ringbag_ts */
     h = hol_open(tsname);
     if ( ! h )
	  return 0;

     hol_close(h);

     tree_add(ringbag_ts, xnstrdup(tsname), "dummy");

     return 1;
}

/* Remove a timestore from the coverage of tsring. 
 * Returns 1 for success, 0 for failure
 */
int ringbag_rmts(char *tsname	/* name of timestore to remove */)
{
     if (tree_find(ringbag_ts, tsname) != TREE_NOVAL) {
	  nfree( tree_getkey(ringbag_ts) );
	  tree_rm(ringbag_ts);
	  return 1;
     } else
	  return 0;
}

/* Remove all timestores from the coverage of tsring. 
 * Returns number of timestores removed.
 */
int ringbag_rmallts()
{
     int i;

     i = 0;
     while ( ! tree_empty(ringbag_ts)) {
	  tree_first(ringbag_ts);
	  nfree( tree_getkey(ringbag_ts) );
	  tree_rm(ringbag_ts);
	  i++;
     }

     return i;
}

/* remove all rings, returns number of rings removed */
void ringbag_rmallrings()
{
     int i;
     struct ringbag_ringent *rent;

     i = 0;
     while ( ! tree_empty(ringbag_ring)) {
	  tree_first(ringbag_ring);
	  nfree( tree_getkey(ringbag_ring) );	/* remove key */
	  rent = tree_get(ringbag_ring);
	  if (rent) {
	       /* if passworded, body not always there */
	       nfree( rent->tsname );		/* remove body parts */
	       nfree( rent->ringname );
	       nfree( rent->description );
	       nfree( rent->password );
	       while ( ! itree_empty(rent->summary)) {
		    itree_first(rent->summary);
		    nfree( itree_get(rent->summary) );
		    itree_rm(rent->summary);
	       }
	       itree_destroy(rent->summary);
	       nfree( rent );			/* remove body */
	  }
	  tree_rm(ringbag_ring);
	  i++;
     }
}

/*
 * Create a summary of all available rings under the coverage of ringbag.
 * The results are kept statically in the class and available with
 * ringbag_getrings().
 * Returns the number of rings encountered (may be 0) or -1 for failure.
 */
int ringbag_getallrings()
{
     TREE *l;
     char *compound;
     struct ringbag_ringent *rent;
     TS_RING ring;
     int i, dummy;
     HOLD h;

     /* 
      * Visit each timestore in ringbag_ts and load their ring directories
      * after deleting previously stored ones. Thus, we always end up with up 
      * to date information.
      */
     ringbag_rmallrings();
     i = 0;

     tree_traverse(ringbag_ts) {
	  /* open ring and continue if failed */
	  h = hol_open(tree_getkey(ringbag_ts));
	  if ( ! h )
	       continue;
	  l = ts_lsringshol(h, "");

	  tree_traverse(l) {
	       /* form a key from timestore and ring name:  ts,ring  */
	       compound = xnmalloc(strlen( tree_getkey(ringbag_ts) ) + 
				   strlen( tree_getkey(l) ) +2 );
	       strcpy(compound, tree_getkey(ringbag_ts));
	       strcat(compound, ",");
	       strcat(compound, tree_getkey(l));

	       /* open ring */
	       ring = ts_open(tree_getkey(ringbag_ts), tree_getkey(l), NULL);
	       if (ring) {
		    /* compile a ring entry with some vitalstatistics */
		    rent = xnmalloc(sizeof(struct ringbag_ringent));
		    rent->tsname = xnstrdup(tree_getkey(ringbag_ts));
		    rent->ringname = xnstrdup(tree_getkey(l));
		    ts_tell(ring, &dummy, &dummy, &rent->seen, 
			     &rent->available, &rent->description);
		    ts_close(ring);
		    rent->summary = itree_create();
		    rent->password = NULL;
	       } else {
		    rent = NULL;
	       }
	       /* save data */
	       tree_add(ringbag_ring, compound, rent);
	       i++;
	  }

	  ts_freelsrings(l);
	  hol_close(h);
     }

     return i;
}


/*
 * Open the specified ring by the compound name: ts,ring. If password is 
 * non NULL, it will be passed to open function for validation.
 * On successful completion, a timestore will be left open so that its
 * position state may be used when appending with _update().
 * returns 1 if successful or 0 if not.
 */
int ringbag_setring(char *compound, /* compound id string: "timestore,ring" */
		    char *password  /* password string; NULL for none */)
{
     struct ringbag_ringent *rent;
     TS_RING ring;

     /* atempt to find and open the requested ring */
     rent = tree_find(ringbag_ring, compound);
     if ( rent == TREE_NOVAL )
	  return 0;

     ring = ts_open(rent->tsname, rent->ringname, password);
     if ( ! ring)
	  return 0;

     /* clear currently open value */
     ringbag_unsetring();

     /* now make this new ring the main one */
     ringbag_openkey = xnstrdup(compound);
     ringbag_openrent = rent;
     ringbag_openid = ring;

     return 1;
}


/* Close the previously selected and set ring */
void ringbag_unsetring()
{
     /* remove the key */
     if (ringbag_openkey) {
	  nfree(ringbag_openkey);
	  ringbag_openkey = NULL;
     }

     /* clear the pointer strucure */
     ringbag_openrent = NULL;

     /* close the ring */
     if (ringbag_openid) {
	  ts_close(ringbag_openid);
	  ringbag_openid = NULL;
     }
}


/*
 * This routine scan the ring for records either side of a sequence.
 * If there are any, a user defined summary routine will be called 
 * for each new entry. For example, dumb terminals will want a one line 
 * summary, graphics terminals may want more. The summary function should 
 * nmalloc() its own storage, which will be cleared by nfree() later.
 *
 * The summary text will be stored in the ring's memory entry and stay 
 * there until the ring is removed, the timestore is removed or taken out
 * of the pool of files, or finally the ring goes out of scope.
 *
 * The scope is a user defined limit of rings either side of a sequence
 * designed to reduce the ammount of storage needed for very large rings.
 * If a ring goes out of scope, its in-memory data will be lost.
 *
 * Returns the first actual sequence available in the ring's memory entry or
 * -1 if there was an error.
 */
int ringbag_scan(int beforescope, /* begining of scan window (num entries) */
		 int afterscope,  /* end of scan window (num entries) */
		 int seq,	  /* sequence number of entry in middle */
		 char * (*summaryfunc)(ntsbuf *) /* summary function */ )
{
     int summfirst, summlast;	/* first and last in-memory sequencies */
     int scopefirst, scopelast;	/* first and last user scope summaries */
     int i, j, r, batch, dummy;
     ITREE *dlist;
     int first1, last1, delete1; /* region one actions */
     int first2, last2, delete2; /* region two actions */
     char *clearstr;

     /* initialise some args to shut -Wall up */
     first1 = last1 = first2 = last2 = 0;

     /* update the statistics */
     clearstr = ringbag_openrent->description;
     if (! ts_tell(ringbag_openid, &dummy, &dummy, &ringbag_openrent->seen, 
	    &ringbag_openrent->available, &ringbag_openrent->description)) {
          elog_send(DIAG, "unable to stat");
	  return -1;
     }
     if (clearstr)		/* clear up old description */
          nfree(clearstr);

     /* calculate absolute values for user scope and adjust them to 
      * the data that is available in the timestore
      */
     scopefirst = seq-beforescope;
     if (scopefirst < ts_oldest(ringbag_openid))
	  scopefirst = ts_oldest(ringbag_openid);
     scopelast = seq+afterscope;
     if (scopelast > ts_youngest(ringbag_openid))
	  scopelast = ts_youngest(ringbag_openid);

     /* Obtain first and last sequencies for the in-memory summaries */
     if (itree_empty(ringbag_openrent->summary)) {
	  summfirst = summlast = -1;
     } else {
	  itree_first(ringbag_openrent->summary);
	  summfirst = itree_getkey(ringbag_openrent->summary);
	  itree_last(ringbag_openrent->summary);
	  summlast = itree_getkey(ringbag_openrent->summary);
     }

     /* compile a list of two regions, each with a begin and end plus 
      * a flag to show if the reion should be deleted or fetching from
      * the timestore.
      */
     if (summlast == -1) {
	  /* empty summary list */
	  first1 = scopefirst;
	  last1 = scopelast;
	  delete1 = 0;
	  first2 = last2 = delete2 = -1;
     } else if (summlast < scopefirst || summfirst > scopelast) {
	  /* rm distinct summaries do not overlap scope */
	  first1 = summfirst;
	  last1 = summlast;
	  delete1 = 1;
	  first2 = scopefirst;
	  last2 = scopelast;
	  delete2 = 0;
     } else {
	  /* scope and summaries overlap */
	  /* first region */
	  if (summfirst < scopefirst) {
	       /* rm overlapping summaries before scope */
	       first1 = summfirst;
	       last1 = scopefirst-1;
	       delete1 = 1;
	  } else if (summfirst > scopefirst) {
	       /* read part of scope before summaries */
	       first1 = scopefirst;
	       last1 = summfirst -1;
	       delete1 = 0;
	  } else
	       delete1 = -1;
	  /* second region */
	  if (summlast > scopelast) {
	       /* rm summaries after scope */
	       first2 = scopelast+1;
	       last2 = summlast;
	       delete2 = 1;
	  } else if (summlast < scopelast) {
	       /* read part of scope after summaries */
	       first2 = summlast+1;
	       last2 = scopelast;
	       delete2 = 0;
	  } else
	       delete2 = -1;
     }


     /* delete or fetch first region */
     if (delete1 == 1) {
	  for (i=first1; i<=last1; i++) {
	       if (itree_find(ringbag_openrent->summary, i) != ITREE_NOVAL) {
		    nfree(itree_get(ringbag_openrent->summary));
		    itree_rm(ringbag_openrent->summary);
	       }
	  }
     } else if (delete1 == 0) {
	  ts_setjump(ringbag_openid, first1-1);
	  while (ts_lastread(ringbag_openid) < last1) {
	       /* work out sociable size of fetch */
	       j = last1 - ts_lastread(ringbag_openid);
	       if (j < RINGBAG_MGETBATCH)
		    batch = j;
	       else
		    batch = RINGBAG_MGETBATCH;

	       /* fetch data, make summary and place in memory */
	       r = ts_mget(ringbag_openid, batch, &dlist);
	       if (r == -1) {
		    elog_send(ERROR,"a - unable to mget");
		    return -1;
	       }
	       if (r == 0)
		    break;
	       itree_traverse(dlist)
		    itree_add(ringbag_openrent->summary, itree_getkey(dlist),
			      summaryfunc(itree_get(dlist)));
	       ts_mgetfree(dlist);
	  }
     }

     /* delete or fetch second region (region disabled if delete2 == -1) */
     if (delete2 == 1) {
	  for (i=first2; i<=last2; i++) {
	       if (itree_find(ringbag_openrent->summary, i) != ITREE_NOVAL)  {
		    nfree(itree_get(ringbag_openrent->summary));
		    itree_rm(ringbag_openrent->summary);
	       }
	  }
     } else if (delete2 == 0) {
	  ts_setjump(ringbag_openid, first2-1);
	  while (ts_lastread(ringbag_openid) < last2) {
	       /* work out sociable size of fetch */
	       j = last2 - ts_lastread(ringbag_openid);
	       if (j < RINGBAG_MGETBATCH)
		    batch = j;
	       else
		    batch = RINGBAG_MGETBATCH;

	       /* fetch data, make summary and place in memory */
	       r = ts_mget(ringbag_openid, batch, &dlist);
	       if (r == -1) {
		    elog_send(ERROR,"b - unable to mget");
		    return -1;
	       }
	       if (r == 0)
		    break;
	       itree_traverse(dlist)
		    itree_add(ringbag_openrent->summary, itree_getkey(dlist),
			      summaryfunc(itree_get(dlist)));
	       ts_mgetfree(dlist);
	  }
     }

#if 0
     /* find first entry in memory */
     if (itree_empty(ringbag_openrent->summary))
	  key = INT_MAX;
     else {
	  itree_first(ringbag_openrent->summary);
	  key = itree_getkey(ringbag_openrent->summary);
     }

     /* add or remove entries to begining of in-memory summary list */
     if (key < first) {
	  /* remove summaries from begining of list */
	  j = first - key;
	  for (i=0; i<j; i++) {
	       itree_first(ringbag_openrent->summary);
	       itree_rm(ringbag_openrent->summary);
	  }
     } else if (key > first) {
	  /* add summaries to begining of list */
	  ts_setjump(ringbag_openid, first);
	  while (ts_lastread(ringbag_openid) < key) {
	       /* work out sociable size of fetch */
	       j = first - ts_lastread(ringbag_openid);
	       if (j < RINGBAG_MGETBATCH)
		    batch = j;
	       else
		    batch = RINGBAG_MGETBATCH;

	       /* fetch data, make summary and place in memory */
	       nts_mget(ringbag_openid, batch, &dlist);
	       itree_traverse(dlist)
		    itree_add(ringbag_openrent->summary, itree_getkey(dlist),
			      summaryfunc(itree_get(dlist)));
	       ts_mgetfree(dlist);
	  }
     }

     /* add or remove entries to end of in-memory summary list */
     itree_last(ringbag_openrent->summary);
     if (itree_getkey(ringbag_openrent->summary) > last) {
	  /* remove summaries from end of list */
	  j = itree_getkey(ringbag_openrent->summary) - last;
	  for (i=0; i<j; i++) {
	       itree_last(ringbag_openrent->summary);
	       itree_rm(ringbag_openrent->summary);
	  }
     } else if (itree_getkey(ringbag_openrent->summary) < last) {
	  /* add summaries to end of list */
	  ts_setjump(ringbag_openid, itree_getkey(ringbag_openrent->summary));
	  while (ts_lastread(ringbag_openid) < last) {
	       /* work out sociable size of fetch */
	       j = last - ts_lastread(ringbag_openid);
	       if (j < RINGBAG_MGETBATCH)
		    batch = j;
	       else
		    batch = RINGBAG_MGETBATCH;

	       /* fetch data, make summary and place in memory */
	       ts_mget(ringbag_openid, batch, &dlist);
	       itree_traverse(dlist)
		    itree_add(ringbag_openrent->summary, itree_getkey(dlist),
			      summaryfunc(itree_get(dlist)));
	       ts_mgetfree(dlist);
	  }
     }
#endif

     /* return the first in the sequence */
     itree_first(ringbag_openrent->summary);
     return(itree_getkey(ringbag_openrent->summary));
}


/*
 * Update the in memory cache of the selected ring with its state on disk.
 * Any entries newer that the last in memory are brought into memory
 * and summarised. The first available sequence is returned or -1 if there
 * was an error.
 * The timestore is left open and its state is left at the last read sequence.
 */
int ringbag_update(int maxkeep, /* historic number of entries to keep */
		   char * (*summaryfunc)(ntsbuf *) /* summary function */ )
{
     int dummy;
     char *dummystr, *clearstr;
     ITREE *dlist;
     int r, key, nsummaries, nremove, available, i;

     /* find last entry and synchronise with timestore */
     if (itree_empty(ringbag_openrent->summary))
	  key = -1;
     else {
	  itree_last(ringbag_openrent->summary);
	  key = itree_getkey(ringbag_openrent->summary);
     }
     ts_setjump(ringbag_openid, key);

     /* How many to keep */
     nsummaries = itree_n(ringbag_openrent->summary);
     ts_tell(ringbag_openid, &dummy, &dummy, &dummy, &available, &dummystr);
     nfree(dummystr);
     nremove = nsummaries + available - maxkeep;
     if (nremove > 0) {
	  /* if nremove is +ve, we need to make space for that many entries.
	   * If we cant make that much space, then we clear all and only
	   * bring in the most recent maxkeep from the ring */
	  itree_first(ringbag_openrent->summary);
	  if (nremove > nsummaries) {
	       /* remove all entries */
	       while ( ! itree_empty(ringbag_openrent->summary)) {
		    itree_first(ringbag_openrent->summary);
		    nfree(itree_get(ringbag_openrent->summary));
		    itree_rm(ringbag_openrent->summary);
	       }
	       /* skip some from the ring */
	       ts_jump(ringbag_openid, nremove-nsummaries);
	  } else {
	       /* remove some entires */
	       for (i=0; i<nremove; i++) {
		    nfree(itree_get(ringbag_openrent->summary));
		    itree_rm(ringbag_openrent->summary);
	       }
	  }
     }

     /* read into memory */
     while ((r = ts_mget(ringbag_openid, RINGBAG_MGETBATCH, &dlist)) != 0) {
	  if (r == -1) {
	       elog_send(ERROR,"unable to mget");
	       return -1;
	  }
	  itree_traverse(dlist)
	       itree_add(ringbag_openrent->summary, itree_getkey(dlist),
			 summaryfunc(itree_get(dlist)));
	  ts_mgetfree(dlist);
     }

     /* update the statistics */
     clearstr = ringbag_openrent->description;
     if (! ts_tell(ringbag_openid, &dummy, &dummy, &ringbag_openrent->seen, 
	    &ringbag_openrent->available, &ringbag_openrent->description)) {
          elog_send(ERROR,"unable to stat");
	  return -1;
     }
     if (clearstr)		/* clear up old description */
          nfree(clearstr);

     /* return the first sequence number */
     itree_first(ringbag_openrent->summary);
     return(itree_getkey(ringbag_openrent->summary));
}

/* Method that returns the first sequence number of ring summaries */
int  ringbag_firstseq() {
     itree_first(ringbag_openrent->summary);
     return(itree_getkey(ringbag_openrent->summary));
}

/* Method that returns the last sequence number of ring summaries */
int  ringbag_lastseq() {
     itree_last(ringbag_openrent->summary);
     return(itree_getkey(ringbag_openrent->summary));
}

/* Method that returns the details obtained about the current ring
 * Returns pointer to current ring description (ringbag_ringent) or
 * NULL if one is not open.
 */
struct ringbag_ringent *ringbag_getents() {
     return ringbag_openrent;
}

/* Returns the timestore reference of the currently open ring */
TS_RING ringbag_getts() {
     return(ringbag_openid);
}

/* Returns the details of all the rings in timestores we know about.
 * TREE contains a list of struct ringbag_ringent, indexed by compound name */
TREE *ringbag_getrings() {
     return(ringbag_ring);
}

/* Returns a list of timstore names that ringbag is configured to use */
TREE *ringbag_gettsnames() {
     return(ringbag_ts);
}

#ifdef TEST

#include <stdio.h>

#define T_FNAME1 "t.ringbag.1.dat"
#define T_FNAME2 "t.ringbag.2.dat"
#define T_FNAME3 "t.ringbag.3.dat"
#define T_RNAME1 "ringbag1"
#define T_RNAME2 "ringbag2"
#define T_RNAME3 "ringbag3"
#define T_RNAME4 "ringbag4"
#define T_RNAME5 "ringbag5"
#define T_RNAME6 "ringbag6"

char *test_summary(ntsbuf *mgetdata) {
     char *b;

     b = xnmalloc(80);
     snprintf(b, 80, "seq %d (%d bytes) %s\n", mgetdata->seq, mgetdata->len, 
	      mgetdata->buffer);
     return(b);
}

main()
{
     HOLD h1, h2, h3;
     TS_RING n1, n2, n3;
     TREE *t1, *t2, *t3;
     ITREE *it1, *it2, *it3;
     char str1[512], str2[512];
     int i1, i2, i3;

     route_init(NULL, 0);
     elog_init(0, "ringbag test", NULL);
     ts_init(err, 1);
     ringbag_init(err, 1);

     /* clear up */
     unlink(T_FNAME1);
     unlink(T_FNAME2);
     unlink(T_FNAME3);

     /* create a few lines for the tests */
     n1 = ts_create(T_FNAME1, 0644, T_RNAME1, "one", NULL, 10);
     n2 = ts_create(T_FNAME2, 0644, T_RNAME2, "two", NULL, 9);
     n3 = ts_create(T_FNAME3, 0644, T_RNAME3, "three", NULL, 8);
     ts_put(n1, "uggblah", 7);
     ts_put(n1, "uggblagh", 8);
     ts_put(n1, "uggblig", 7);
     ts_put(n2, "uggblog", 7);
     ts_put(n2, "uggblrghugh", 11);
     ts_put(n2, "uggbiogger", 10);
     ts_put(n2, "uggblgh", 7);
     ts_put(n3, "uggblwetrt", 10);
     ts_put(n3, "uggblahski", 10);
     ts_put(n3, "mary", 4);
     ts_put(n3, "mungo", 5);
     ts_put(n3, "midge", 5);
     ts_put(n3, "thereisno", 9);
     ts_put(n3, "darkside", 8);
     ts_put(n3, "ofthemoon", 9);
     ts_put(n3, "milford", 7);
     ts_put(n3, "whitley", 7);
     ts_close(n1);
     ts_close(n2);
     ts_close(n3);
     n1 = ts_create(T_FNAME1, 0644, T_RNAME4, "four", NULL, 10);
     n2 = ts_create(T_FNAME2, 0644, T_RNAME5, "five", NULL, 9);
     n3 = ts_create(T_FNAME3, 0644, T_RNAME6, "six", NULL, 8);
     ts_put(n1, "ahhblah", 7);
     ts_put(n1, "ahhblagh", 8);
     ts_put(n1, "ahhblig", 7);
     ts_put(n2, "ahhblog", 7);
     ts_put(n2, "ahhbloghugh", 11);
     ts_put(n2, "ahhblogger", 10);
     ts_put(n2, "ahhblgh", 7);
     ts_put(n3, "ahhblug", 7);
     ts_put(n3, "ahhblwetrt", 10);
     ts_put(n3, "ahhblahski", 10);
     ts_close(n1);
     ts_close(n2);
     ts_close(n3);

     /* test 1: add timestores */
     if ( !ringbag_addts(T_FNAME1))
	  route_die(err, "[1] ringbag_addts(%s) failed\n", T_FNAME1);
     if ( !ringbag_addts(T_FNAME2))
	  route_die(err, "[1] ringbag_addts(%s) failed\n", T_FNAME2);
     if ( !ringbag_addts(T_FNAME3))
	  route_die(err, "[1] ringbag_addts(%s) failed\n", T_FNAME3);
     if ( tree_find(ringbag_ts, T_FNAME1) == TREE_NOVAL )
	  route_die(err, "[1] %s not in ringbag_ts\n", T_FNAME1);
     if ( tree_find(ringbag_ts, T_FNAME2) == TREE_NOVAL )
	  route_die(err, "[1] %s not in ringbag_ts\n", T_FNAME2);
     if ( tree_find(ringbag_ts, T_FNAME3) == TREE_NOVAL )
	  route_die(err, "[1] %s not in ringbag_ts\n", T_FNAME3);

     /* test 2: removing a timestore file */
     if ( !ringbag_rmts(T_FNAME2))
	  route_die(err, "[2] ringbag_rmts(%s) didnt work\n", T_FNAME2);
     if (tree_find(ringbag_ts, T_FNAME2) != TREE_NOVAL )
	  route_die(err, "[2] %s still in ringbag_ts\n", T_FNAME2);
     if ( !ringbag_addts(T_FNAME2))
	  route_die(err, "[2] ringbag_addts(%s) failed\n", T_FNAME2);
     if ( tree_find(ringbag_ts, T_FNAME2) == TREE_NOVAL )
	  route_die(err, "[2] %s not back in ringbag_ts\n", T_FNAME2);

     /* test 3: get rings */
     if (ringbag_getallrings() != 6)
	  route_die(err, "[3] did not get 6 rings\n");
     sprintf(str1, "%s,%s", T_FNAME1, T_RNAME1);
     if ( tree_find(ringbag_ring, str1) == TREE_NOVAL )
	  route_die(err, "[3] did not find %s in ringbag_ring\n", str1);
     sprintf(str1, "%s,%s", T_FNAME2, T_RNAME2);
     if ( tree_find(ringbag_ring, str1) == TREE_NOVAL )
	  route_die(err, "[3] did not find %s in ringbag_ring\n", str1);
     sprintf(str1, "%s,%s", T_FNAME3, T_RNAME3);
     if ( tree_find(ringbag_ring, str1) == TREE_NOVAL )
	  route_die(err, "[3] did not find %s in ringbag_ring\n", str1);

     /* test 4: remove a timestore and redo the test above */
     if ( !ringbag_rmts(T_FNAME2))
	  route_die(err, "[4] ringbag_rmts(%s) didnt work\n", T_FNAME2);
     if (tree_find(ringbag_ts, T_FNAME2) != TREE_NOVAL)
	  route_die(err, "[4] %s still in ringbag_ts\n", T_FNAME2);

     if (ringbag_getallrings() != 4)
	  route_die(err, "[4] did not get 4 rings\n");
     sprintf(str1, "%s,%s", T_FNAME1, T_RNAME1);
     if ( tree_find(ringbag_ring, str1) == TREE_NOVAL )
	  route_die(err, "[4] did not find %s in ringbag_ring\n", str1);
     sprintf(str1, "%s,%s", T_FNAME2, T_RNAME2);
     if ( tree_find(ringbag_ring, str1) != TREE_NOVAL )
	  route_die(err, "[4] still found %s in ringbag_ring\n", str1);
     sprintf(str1, "%s,%s", T_FNAME3, T_RNAME3);
     if ( tree_find(ringbag_ring, str1) == TREE_NOVAL )
	  route_die(err, "[4] did not find %s in ringbag_ring\n", str1);

     /* test 5: add the timestore back again and repeat tests */
     if ( !ringbag_addts(T_FNAME2))
	  route_die(err, "[5] ringbag_addts(%s) failed\n", T_FNAME2);
     if ( tree_find(ringbag_ts, T_FNAME2) == TREE_NOVAL )
	  route_die(err, "[5] %s not back in ringbag_ts\n", T_FNAME2);

     if (ringbag_getallrings() != 6)
	  route_die(err, "[5] did not get 6 rings\n");
     sprintf(str1, "%s,%s", T_FNAME1, T_RNAME1);
     if ( tree_find(ringbag_ring, str1) == TREE_NOVAL )
	  route_die(err, "[5] did not find %s in ringbag_ring\n", str1);
     sprintf(str1, "%s,%s", T_FNAME2, T_RNAME2);
     if ( tree_find(ringbag_ring, str1) == TREE_NOVAL )
	  route_die(err, "[5] did not find %s in ringbag_ring\n", str1);
     sprintf(str1, "%s,%s", T_FNAME3, T_RNAME3);
     if ( tree_find(ringbag_ring, str1) == TREE_NOVAL )
	  route_die(err, "[5] did not find %s in ringbag_ring\n", str1);

     /* test 6: set a ring to be current */
     sprintf(str1, "%s,%s", T_FNAME2, T_RNAME2);
     if ( ! ringbag_setring(str1, NULL))
	  route_die(err, "[6] ring not set\n");
     if ( ! ringbag_openkey)
	  route_die(err, "[6] ringbag_openkey not set\n");
     if ( ! ringbag_openrent)
	  route_die(err, "[6] ringbag_openrent not set\n");
     if ( ! ringbag_openid)
	  route_die(err, "[6] ringbag_openid not set\n");

     /* test 7: update ring initially */
     ringbag_update(100, test_summary);
     i1 = 0;
     itree_traverse(ringbag_openrent->summary) {
	  /*route_printf(err, "Summary %d: %s\n", i1, itree_get(ringbag_openrent->summary));*/
	  if (itree_getkey(ringbag_openrent->summary) > 3)
	       route_die(err, "[7] too many summaries\n");
	  if (!(strstr(itree_get(ringbag_openrent->summary),"uggblog") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblrghugh") ||
		strstr(itree_get(ringbag_openrent->summary),"uggbiogger") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblgh"))) {
	       route_die(err, "[7] entries do not match\n");
	  }
	  i1++;
     }

     /* test 8: update ring when nothing has gone on */
     ringbag_update(100, test_summary);
     i1 = 0;
     itree_traverse(ringbag_openrent->summary) {
	  /*route_printf(err, "Summary %d: %s\n", i1, itree_get(ringbag_openrent->summary));*/
	  if (itree_getkey(ringbag_openrent->summary) > 3)
	       route_die(err, "[8] too many summaries\n");
	  if (!(strstr(itree_get(ringbag_openrent->summary),"uggblog") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblrghugh") ||
		strstr(itree_get(ringbag_openrent->summary),"uggbiogger") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblgh"))) {
	       route_die(err, "[8] entries do not match\n");
	  }
	  i1++;
     }

     /* test 9: add something to ring and test again */
     n2 = ts_open(T_FNAME2, T_RNAME2, NULL);
     ts_put(n2, "bollocks", 8);
     ts_close(n2);
     ringbag_update(100, test_summary);
     i1 = 0;
     itree_traverse(ringbag_openrent->summary) {
	  /*route_printf(err, "Summary %d: %s\n", i1, itree_get(ringbag_openrent->summary));*/
	  if (itree_getkey(ringbag_openrent->summary) > 4)
	       route_die(err, "[9] too many summaries\n");
	  if (!(strstr(itree_get(ringbag_openrent->summary),"uggblog") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblrghugh") ||
		strstr(itree_get(ringbag_openrent->summary),"uggbiogger") ||
		strstr(itree_get(ringbag_openrent->summary),"bollocks") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblgh"))) {
	       route_die(err, "[9] entries do not match\n");
	  }
	  i1++;
     }

     /* test 10: add many data to cycle the ring. There will be a gap in
      * the sequence list because we were unable to capture some data 
      * before it was dropped off the list */
     n2 = ts_open(T_FNAME2, T_RNAME2, NULL);
     for (i1=0; i1<10; i1++) {
	  i2 = sprintf(str1, "datum %d", i1);
	  ts_put(n2, str1, i2);
     }
     ts_close(n2);
     ringbag_update(100, test_summary);
     i1 = 0;
     itree_traverse(ringbag_openrent->summary) {
	  /*route_printf(err, "Summary %d: %s\n", i1, itree_get(ringbag_openrent->summary));*/
	  if (itree_getkey(ringbag_openrent->summary) > 14)
	       route_die(err, "[10] too many summaries\n");
	  if (!(strstr(itree_get(ringbag_openrent->summary),"uggblog") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblrghugh") ||
		strstr(itree_get(ringbag_openrent->summary),"uggbiogger") ||
		strstr(itree_get(ringbag_openrent->summary),"bollocks") ||
		strstr(itree_get(ringbag_openrent->summary),"datum") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblgh")))
	       route_die(err, "[10] entries do not match\n");
	  i1++;
     }

     /* test 11: truncate ring with maxkeep */
     ringbag_update(9, test_summary);
     i1 = 0;
     itree_traverse(ringbag_openrent->summary) {
	  /*route_printf(err, "Summary %d: %s\n", i1, itree_get(ringbag_openrent->summary));*/
	  if (itree_getkey(ringbag_openrent->summary) > 14)
	       route_die(err, "[11] too many summaries\n");
	  if (!	strstr(itree_get(ringbag_openrent->summary),"datum"))
	       route_die(err, "[11] entries do not match\n");
	  i1++;
     }

     /* test 12: add data and read with truncate */
     n2 = ts_open(T_FNAME2, T_RNAME2, NULL);
     for (i1=0; i1<10; i1++) {
	  i2 = sprintf(str1, "more datum %d", i1);
	  ts_put(n2, str1, i2);
     }
     ts_close(n2);
     ringbag_update(9, test_summary);
     i1 = 0;
     itree_traverse(ringbag_openrent->summary) {
	  /*route_printf(err, "Summary %d: %s\n", i1, 
		       itree_get(ringbag_openrent->summary));*/
	  if (itree_getkey(ringbag_openrent->summary) > 24)
	       route_die(err, "[12] too many summaries\n");
	  if (!	strstr(itree_get(ringbag_openrent->summary),"more datum"))
	       route_die(err, "[12] entries do not match\n");
	  i1++;
     }

     /* test 13a: ringbag_scan() with a small scope */
     sprintf(str1, "%s,%s", T_FNAME3, T_RNAME3);
     if ( ! ringbag_setring(str1, NULL))
	  route_die(err, "[13] ring not set\n");
     ringbag_scan(1, 1, 1, test_summary);
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"uggblwetrt") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblahski") ||
		strstr(itree_get(ringbag_openrent->summary),"mary")))
	       route_die(err, "[13a] entries do not match\n");
     }
     /*        13b: extend end scope by one */
     ringbag_scan(1, 2, 1, test_summary);
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"uggblwetrt") ||
		strstr(itree_get(ringbag_openrent->summary),"uggblahski") ||
		strstr(itree_get(ringbag_openrent->summary),"mary") ||
		strstr(itree_get(ringbag_openrent->summary),"mungo")))
	       route_die(err, "[13b] entries do not match\n");
     }
     /*        13c: move focus */
     ringbag_scan(1, 1, 6, test_summary);
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"thereisno") ||
		strstr(itree_get(ringbag_openrent->summary),"darkside") ||
		strstr(itree_get(ringbag_openrent->summary),"ofthemoon")))
	       route_die(err, "[13c] entries do not match\n");
     }

     /* test 14: ringbag_scan() tests */
     /* there should be five full list test conditions:-
      *        a          b          c          d          e
      *    first got  first got  first got  first want first want
      *    last got   first want first want first got  last want
      *    first want last got   last want  last want  first got
      *    last want  last want  last got   last got   last got
      * three boundary conditions tests
      *        f                     g                      h
      *    first got             first got==first want  first want
      *    last got==first want  last got == last want  last want==first got
      *    last want                                    last got
      * four 1 element length tests
      *        i               j                k                  l
      *    first==last got  first want       first got         first==last want
      *    first want       last want        last got          first got
      *    last want        first==last got  first==last want  last got
      * and finally two null length tests
      *        m                   n
      *    first==last got==0  first==last want==0
      *    first want          first got
      *    last want           last got
      */

     /* test 14a */
     ringbag_scan(1, 1, 11, test_summary);
     if (itree_n(ringbag_openrent->summary) != 0)
	  route_die(err, "[14a] wrong number of entries\n");

     /* test 14b */
     ringbag_scan(2, 2, 9, test_summary);
     if (itree_n(ringbag_openrent->summary) != 3)
	  route_die(err, "[14b] wrong number of entries: have %d want 3\n",
		    itree_n(ringbag_openrent->summary));
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"ofthemoon") ||
		strstr(itree_get(ringbag_openrent->summary),"milford") ||
		strstr(itree_get(ringbag_openrent->summary),"whitley")))
	       route_die(err, "[14b] entries do not match\n");
     }

     /* test 14c */
     ringbag_scan(1, 1, 5, test_summary);
     if (itree_n(ringbag_openrent->summary) != 3)
	  route_die(err, "[14c] wrong number of entries\n");
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"thereisno") ||
	        strstr(itree_get(ringbag_openrent->summary),"darkside") ||
		strstr(itree_get(ringbag_openrent->summary),"midge")))
	       route_die(err, "[14c] entries do not match\n");
     }

     /* test 14d */
     ringbag_scan(2, 1, 3, test_summary);
     if (itree_n(ringbag_openrent->summary) != 3)
	  route_die(err, "[14d] wrong number of entries\n");
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"mary") ||
		strstr(itree_get(ringbag_openrent->summary),"mungo") ||
		strstr(itree_get(ringbag_openrent->summary),"midge")))
	       route_die(err, "[14d] entries do not match\n");
     }

     /* test 14e */
     ringbag_scan(1, 0, 1, test_summary);
     if (itree_n(ringbag_openrent->summary) != 0)
	  route_die(err, "[14e] wrong number of entries\n");

     /* test 14f */
     ringbag_scan(1, 0, 2, test_summary);
     if (itree_n(ringbag_openrent->summary) != 1)
	  route_die(err, "[14f] wrong number of entries\n");
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"mary")))
	       route_die(err, "[14f] entries do not match\n");
     }

     /* test 14g */
     ringbag_scan(3, 4, 5, test_summary);
     if (itree_n(ringbag_openrent->summary) != 8)
	  route_die(err, "[14g] wrong number of entries\n");
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"milford") ||
		strstr(itree_get(ringbag_openrent->summary),"whitley") ||
		strstr(itree_get(ringbag_openrent->summary),"mungo") ||
		strstr(itree_get(ringbag_openrent->summary),"midge") ||
		strstr(itree_get(ringbag_openrent->summary),"thereisno") ||
		strstr(itree_get(ringbag_openrent->summary),"darkside") ||
		strstr(itree_get(ringbag_openrent->summary),"ofthemoon") ||
		strstr(itree_get(ringbag_openrent->summary),"mary")))
	       route_die(err, "[14g] entries do not match\n");
     }

     /* test 14h */
     ringbag_scan(1, 0, 2, test_summary);
     if (itree_n(ringbag_openrent->summary) != 1)
	  route_die(err, "[14h] wrong number of entries\n");
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"mary")))
	       route_die(err, "[14h] entries do not match\n");
     }

     /* test 14j */
     ringbag_scan(1, 0, 1, test_summary);
     if (itree_n(ringbag_openrent->summary) != 0)
	  route_die(err, "[14j] wrong number of entries: have %d want 0\n",
		    itree_n(ringbag_openrent->summary));

     /* test 14k */
     ringbag_scan(0, 0, 11, test_summary);
     if (itree_n(ringbag_openrent->summary) != 0)
	  route_die(err, "[14k] wrong number of entries: have %d expecting 0\n", itree_n(ringbag_openrent->summary));

     /* test 14l */
     ringbag_scan(0, 0, 1, test_summary);
     if (itree_n(ringbag_openrent->summary) != 0)
	  route_die(err, "[14l] wrong number of entries: have %d want 0\n",
		    itree_n(ringbag_openrent->summary));

     /* test 14n (has some entries from test 13c, ts3 ring 3) */
     ringbag_scan(0, 0, -3, test_summary);
     if (itree_n(ringbag_openrent->summary) != 0)
	  route_die(err, "[14n] wrong number of entries\n");

     /* test 14i */
     /* For the remainder of 14, we switch to another ring and empty it */
     sprintf(str1, "%s,%s", T_FNAME1, T_RNAME4);
     if ( ! ringbag_setring(str1, NULL))
	  route_die(err, "[14i] ring not set\n");
     ringbag_scan(1, 1, 1, test_summary);	/* attempt to fool */
     n1 = ts_open(T_FNAME1, T_RNAME4, NULL);
     if ( ! n1)
	  route_die(err, "[14i] unable to open ring\n");
     if ( ! ts_purge(n1, 1))	/* purge to leave 1 */
	  route_die(err, "[14i] unable to purge\n");
     ts_close(n1);

     ringbag_scan(1, 1, 5, test_summary);
     if (itree_n(ringbag_openrent->summary) != 0)
	  route_die(err, "[14i] wrong number of entries\n");

     /* test 14m */
     /* Now we completely empty the ringbag */
     n1 = ts_open(T_FNAME1, T_RNAME4, NULL);
     if ( ! n1)
	  route_die(err, "[14m] unable to open ring\n");
     if ( ! ts_purge(n1, 2))	/* purge to leave nothing */
	  route_die(err, "[14m] unable to purge\n");
     ts_close(n1);

     ringbag_scan(5, 5, 5, test_summary);
     if (itree_n(ringbag_openrent->summary) != 0)
	  route_die(err, "[14m] wrong number of entries: have %d expecting 0\n", itree_n(ringbag_openrent->summary));

     /* test 15a: fetch the whole damn lot */
     /* change the ring */
     sprintf(str1, "%s,%s", T_FNAME3, T_RNAME3);
     if ( ! ringbag_setring(str1, NULL))
	  route_die(err, "[14i] ring not set\n");
     ringbag_scan(10, 4, 10, test_summary);
     if (itree_n(ringbag_openrent->summary) != 8)
	  route_die(err, "[15a] wrong number of entries\n");
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"milford") ||
		strstr(itree_get(ringbag_openrent->summary),"whitley") ||
		strstr(itree_get(ringbag_openrent->summary),"mungo") ||
		strstr(itree_get(ringbag_openrent->summary),"midge") ||
		strstr(itree_get(ringbag_openrent->summary),"thereisno") ||
		strstr(itree_get(ringbag_openrent->summary),"darkside") ||
		strstr(itree_get(ringbag_openrent->summary),"ofthemoon") ||
		strstr(itree_get(ringbag_openrent->summary),"mary")))
	       route_die(err, "[15a] entries do not match\n");
     }
		
     /* test 15b: fetch nothing */
     ringbag_scan(0, 0, -1, test_summary);
     if (itree_n(ringbag_openrent->summary) != 0)
	  route_die(err, "[15b] wrong number of entries\n");
		
     /* test 15c: fetch the whole damn lot */
     ringbag_scan(2, 4, 0, test_summary);
     if (itree_n(ringbag_openrent->summary) != 3)
	  route_die(err, "[15c] wrong number of entries\n");
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"mary") ||
		strstr(itree_get(ringbag_openrent->summary),"mungo") ||
		strstr(itree_get(ringbag_openrent->summary),"midge")))
	       route_die(err, "[15c] entries do not match\n");
     }
		
     /* test 16: append to list with a scan */
     ringbag_update(100, test_summary);
     if (itree_n(ringbag_openrent->summary) != 8)
	  route_die(err, "[16] wrong number of entries\n");
     itree_traverse(ringbag_openrent->summary) {
	  if (!(strstr(itree_get(ringbag_openrent->summary),"whitley") ||
		strstr(itree_get(ringbag_openrent->summary),"milford") ||
		strstr(itree_get(ringbag_openrent->summary),"mungo") ||
		strstr(itree_get(ringbag_openrent->summary),"midge") ||
		strstr(itree_get(ringbag_openrent->summary),"thereisno") ||
		strstr(itree_get(ringbag_openrent->summary),"darkside") ||
		strstr(itree_get(ringbag_openrent->summary),"ofthemoon") ||
		strstr(itree_get(ringbag_openrent->summary),"mary")))
	       route_die(err, "[16] entries do not match\n");
     }

#if 0
     itree_traverse(ringbag_openrent->summary)
	  route_printf(err, "seq=%d: %s\n", 
		       itree_getkey(ringbag_openrent->summary), 
		       itree_get(ringbag_openrent->summary));
#endif

#if 0
     /* clear summaries */
     itree_traverse(ringbag_openrent->summary) {
	  itree_first(ringbag_openrent->summary);
	  itree_rm(ringbag_openrent->summary);
     }
#endif

     /* put some more data in and save */

     /* test going back to ring 2 and that everything is still there */

     /* clear up */
     unlink(T_FNAME1);
     unlink(T_FNAME2);
     unlink(T_FNAME3);

     /* finalise */
     ringbag_fini();
     elog_fini();
     route_close(err);
     route_fini();

     printf("tests finished successfully\n");
     exit(0);
}


#endif /* TEST */
