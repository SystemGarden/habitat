/*
 * Verison store
 * Stores text and meta data regarding the entry, additionally maintaining 
 * a historical record of it.
 *
 * Nigel Stuckey, May 1999
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <string.h>
#include <stdio.h>
#include "timestore.h"
#include "versionstore.h"
#include "elog.h"
#include "util.h"
#include "tree.h"
#include "itree.h"
#include "table.h"
#include "nmalloc.h"

/* TABLE data type schmeas */
char *vers_getall_schema[] = {"version", "time", "author", "comment", \
			      "data", NULL};


/*
 * Open an existing version store, returning NULL for failure or if the 
 * store does not exist. Returns a VS descriptor if successful.
 */
VS vers_open(char *holname,	/* filename of holstore containg version */
	     char *id,		/* version object name */
	     char *pw		/* password to object (or NULL for none) */ )
{
     VS v;
     char *vblock, *tok;
     int found=0, idlen, vlen;

     v = ts_open(holname, id, pw);
     if (v) {
	  /* read the versionstore superblock to check that this ring
	   * is, in fact, in versionstore format */
          hol_begintrans(v->hol, 'r');
          vblock = hol_get(v->hol, VS_SUPERNAME, &vlen);
	  hol_endtrans(v->hol);

	  if (vblock != NULL) {
	       /* search for whitespace seperated id */
	       idlen = strlen(id);
	       while ( (tok = strstr(vblock, id)) != NULL) {
		    if ( (   tok        == vblock || *(tok-1)     == ' ') &&
			 ( *(tok+idlen) == '\0'   || *(tok+idlen) == ' ') ) {
			 found++;
			 break;
		    }
	       }
	       nfree(vblock);
	  }

	  if ( ! found ) {
	       ts_close(v);
	       v  = NULL;
	  }
     }

     return v;
}


/*
 * Create a new version store, returning NULL for failure or a VS 
 * descriptor if successful. If the version already exists, then it is 
 * treated as a success.
 */
VS vers_create(char *holname,	/* filename of holstore containg version */
	       int mode,	/* file permission mode for holstore */
	       char *id,	/* version object name */
	       char *pw,	/* password to object (or NULL for none) */
	       char *desc	/* text description */ )
{
     VS v;
     char *vblock, *tok;
     int vlen, newvlen, idlen, r;

     /* create the version superblock */
     v = ts_create(holname, mode, id, desc, pw, 0);
     if (v) {
          /* Add new version store to version superblock.
	   * The superblock is just a space seperate list of version
	   * object names */
	  idlen = strlen(id);
          hol_begintrans(v->hol, 'w');
          vblock = hol_get(v->hol, VS_SUPERNAME, &vlen);
	  if (vblock) {
	       /* search for whitespace seperated id */
	       while ( (tok = strstr(vblock, id)) != NULL) {
		    if ( (   tok        == vblock || *(tok-1)     == ' ') &&
			 ( *(tok+idlen) == '\0'   || *(tok+idlen) == ' ') ) {

			 /* version object already exists: do nothing */
			 hol_endtrans(v->hol);
			 nfree(vblock);
			 return v;
		    }
	       }
	       newvlen = vlen+idlen+1;
	  } else {
	       newvlen = idlen+2;
	       vlen = 1;
	  }
	  vblock = xnrealloc(vblock, newvlen);
	  strcpy(vblock+vlen-1, id);
	  strcpy(vblock+vlen-1+idlen, " ");
          r = hol_put(v->hol, VS_SUPERNAME, vblock, newvlen);
	  hol_commit(v->hol);
	  if ( ! r )
	       elog_printf(ERROR, "unable to update versionstore "
			   "superblock");
	  nfree(vblock);
     }
     return v;
}

/*
 * Add a new version of data to the version store.
 * dlen may be 0, in which case the data is assumed to be a string
 * and it is counted. Author, comment and data must be provided: NULLs for
 * either of these will cause an error.
 * Returns the version number if successful or -1 for error.
 */
int vers_new(VS vs,		/* version store identifier */
	     char *data, 	/* data block */
	     int dlenarg,	/* length of data block (0 for string) */
	     char *author, 	/* name of this version's author */
	     char *comment	/* comment on this version */ )
{
     char *buf;
     int r, alen, blen, clen, dlen;

     /* check parameters */
     if (data == NULL || author == NULL || comment == NULL) {
          elog_printf(ERROR, "data, author and comment must all be supplied");
	  return -1;
     }

     /* create buffer with data block, author and comment
      * format is:-
      *
      *       author \0 comment \0 data \0
      *
      * Data may be text or binary. when reading, the data length will 
      * be calculated so it does not include the null!!
      */
     alen = strlen(author);
     clen = strlen(comment);
     dlen = ( dlenarg ? dlenarg : strlen(data) );
     blen = dlen + alen + clen +3;	/* NULLS for all tokens */
     buf = xnmalloc(blen);
     strcpy(buf, author);
     strcpy(buf+alen+1, comment);
     memcpy(buf+alen+clen+2, data, dlen);
     *(buf+blen-1) = '\0';		/* terminated for safety */

     /* put the data block and handle errors */
     r = ts_put(vs, buf, blen);
     nfree(buf);
     if (r == -1) {
          elog_printf(ERROR, "Unable to put new version of %s",
		      vs->name);
	  return -1;
     }
     return r;
}

/*
 * Get the current version
 * Returns the current data from the tablestore ring.
 * `Current' is not really a user concept for version store, but it is used 
 * by the other access methods.
 * Outbound parameters are data, dlen, author, comment, mtime and version.
 * Data, author and comment are all nmalloc()ed and should be nfree()ed
 * when finished with.
 * Returns 1 for successful or 0 if there is no data or a failure has occured.
 */

int vers_getcurrent(VS vs, 		/* version store object identifier */
		    char **data,	/* out: data block */
		    int *dlen,		/* out: length of data block */
		    char **author,	/* out: name of the version's author */
		    char **comment,	/* out: comment on this version */
		    time_t *mtime,	/* out: time this version was created*/
		    int *version	/* out: version number */ )
{
     int blen, i;
     char *buf;

     /* get data */
     buf = ts_get(vs, &blen, mtime, version);
     if ( ! buf) {
          elog_printf(DEBUG, "unable to read versionstore data: %s",
		      vs->name);
	  return 0;
     }

     /* scan buffer values and return dups of them */
     *author = xnstrdup(buf);
     i = strlen(buf)+1;
     *comment = xnstrdup(buf+i);
     i += strlen(*comment)+1;
     *dlen = (blen - 1) - i;
     *data = xnmemdup(buf+i, *dlen+1);
     nfree(buf);

     return 1;
}

/* Get the latest record in the ring; arguments as vers_getcurrent().
 * Returns 1 for successful or 0 otherwise */
int vers_getlatest(VS vs, 		/* version store object identifier */
		   char **data,		/* out: data block */
		   int *dlen,		/* out: length of data block */
		   char **author,	/* out: name of the version's author */
		   char **comment,	/* out: comment on this version */
		   time_t *mtime,	/* out: time this version was created*/
		   int *version		/* out: version number */ )
{
     ts_jumpyoungest(vs);
     ts_jump(vs, -1);
     return vers_getcurrent(vs, data, dlen, author, comment, mtime, version);
}

/* Get the version specified by sequence number; arguments as 
 * vers_getcurrent().
 * Returns 1 for successful or 0 otherwise */
int vers_getversion(VS vs, 		/* version store object identifier */
		   int version,		/* in: version number */
		   char **data,		/* out: data block */
		   int *dlen,		/* out: length of data block */
		   char **author,	/* out: author of this version */
		   char **comment,	/* out: comment on this version */
		   time_t *mtime	/* out: time version was created*/ )
{
     int novers;

     /*ts_jumpyoungest(vs);
       ts_jump(vs, -1);*/
     ts_setjump(vs, version-1);
     return vers_getcurrent(vs, data, dlen, author, comment, mtime, &novers);
}


/*
 * Get the data and parameters for all versions in the open version object.
 * Returns the data in a TABLE data type if successful, or NULL for failure.
 */
TABLE vers_getall(VS vs)
{
     TABLE datatab;			/* extraction result */
     int r;
     char *data, *author, *comment;	/* data returned */
     int dlen, version;			/*   "      "    */
     time_t mtime;			/*   "      "    */
     char vertext[20], *alloced;	/* text representation of version x*/

     /* create table and position in storage */
     ts_jumpoldest(vs);
     datatab = table_create_a(vers_getall_schema);

     /* iterate over loop versions, creating rows to insert into table */
     version = -1;
     while (version != ts_youngest(vs)) {
          /* extract data */
          r = vers_getcurrent(vs,&data,&dlen,&author,&comment,&mtime,&version);
	  if ( ! r ) {
	       table_destroy(datatab);
	       return NULL;
	  }

	  /* add data row */
	  table_addemptyrow(datatab);
	  r = table_replacecurrentcell(datatab, "data", data);
	  if ( ! r )
	       elog_die(FATAL, "unable to replace data");
	  table_freeondestroy(datatab, data);
	  r = table_replacecurrentcell(datatab, "comment", comment);
	  if ( ! r )
	       elog_die(FATAL, "unable to replace comment");
	  table_freeondestroy(datatab, comment);
	  r = table_replacecurrentcell(datatab, "author", author);
	  if ( ! r )
	       elog_die(FATAL, "unable to replace author");
	  table_freeondestroy(datatab, author);
	  sprintf(vertext, "%d", version);
	  alloced = xnstrdup(vertext);
	  r = table_replacecurrentcell(datatab, "version", alloced);
	  if ( ! r )
	       elog_die(FATAL, "unable to replace version");
	  table_freeondestroy(datatab, alloced);
	  alloced = xnstrdup(util_decdatetime(mtime));
	  r = table_replacecurrentcell(datatab, "time", alloced);
	  if ( ! r )
	       elog_die(FATAL, "unable to replace time");
	  table_freeondestroy(datatab, alloced);
     }

     /* clear up and return */
     return datatab;
}




/*
 * Edit some fields in an existing version
 * Use NULL for the fields you don't want to replace; you are unable
 * to replace version.
 * Returns 1 for success or 0 for failure.
 */
int vers_edit(VS vs, 		/* version store identifier */
	      int version,	/* version number */
	      char *author,	/* new author name or NULL */
	      char *comment	/* new version comment or NULL */ )
{
     char *st_author, *st_comment, *st_data, *buf;
     time_t mtime;
     int alen, blen, clen, dlen, st_dlen, r;

     if (!author && !comment)
          return 1;	/* strictly successful but really does nothing */

     /* get the stored version */
     r = vers_getversion(vs, version, &st_data, &st_dlen, &st_author, 
			 &st_comment, &mtime);
     if ( ! r )
          return 0;

     /* replace returned values with new ones */
     if (author) {
          nfree(st_author);
	  st_author = xnstrdup(author);
     }
     if (comment) {
          nfree(st_comment);
	  st_comment = xnstrdup(comment);
     }

     /* compile new block */
     alen = strlen(author);
     clen = strlen(comment);
     dlen = ( st_dlen ? st_dlen : strlen(st_data) );
     blen = dlen + alen + clen +3;	/* NULLS for all tokens */
     buf = xnmalloc(blen);
     strcpy(buf, author);
     strcpy(buf+alen+1, comment);
     memcpy(buf+alen+clen+2, st_data, dlen);
     *(buf+blen-1) = '\0';		/* terminated for safety */

     /* now store it back */
     ts_setjump(vs, version-1);
     r = ts_replace(vs, buf, blen);

     /* clean up storage */
     nfree(st_author);
     nfree(st_comment);
     nfree(st_data);
     nfree(buf);

     if (r == -1)
          return 0;
     else
          return 1;
}


/*
 * Return the description of the version object.
 * Free description with nfree() after use. Thank you!
 */
char *vers_description(VS vs)
{
     int nrings, nslots, nread, nunread, r;
     char *description;

     r = ts_tell(vs, &nrings, &nslots, &nread, &nunread, &description);
     if (r)
          return description;
     else
          return NULL;
}


/*
 * Return a list of version objects available in this holstore.
 * Returns NULL if there are no version store object in the holstore.
 * Free the list with vers_freelsrings()
 */
TREE *vers_lsvershol(HOLD hol)
{
     char *vblock, *tok;
     int vlen;
     TREE *vobjs;

     /* get version superblock */
     vblock = hol_get(hol, VS_SUPERNAME, &vlen);
     if ( ! vblock)
          return NULL;

     /* make a list from the space seperated tokens */
     vobjs = tree_create();
     tok = strtok(vblock, " ");
     while (tok) {
          tree_add(vobjs, xnstrdup(tok), xnstrdup(""));
	  tok = strtok(NULL, " ");
     }

     nfree(vblock);

     return vobjs;
}



#if TEST

#include <unistd.h>
#include <stdlib.h>

#define TEST_VFILE "t.vers.1.dat"
#define TEST_VOBJ1 "cfg1"
#define TEST_TEXT1 "piddle pom, da da, blugh"
#define TEST_TEXT2 "what a dog"
#define TEST_TEXT3 "a guide to simple swaring: tosspot"

int main() {
     VS v1;
     int r, dlen, version;
     char *data, *author, *comment;
     time_t mtime;
     TREE *vobj1;

     /* initialise */
     route_init(NULL, 0);
     elog_init(0, "timestore test", NULL);
     vers_init();

     unlink(TEST_VFILE);

     /* test 1: simple new version */
     v1 = vers_create(TEST_VFILE, 0644, TEST_VOBJ1, NULL, "test 1");
     if (! v1)
          elog_die(FATAL, "[1] unable to create vers");
     r = vers_new(v1, TEST_TEXT1, 0, "nigel", "first version");
     if (r == -1)
          elog_die(FATAL, "[1] unable to make new");
     if (r != 0)
          elog_die(FATAL, "[1] unexpected sequence %d", r);
     vers_close(v1);

     /* test 2: simple addition */
     v1 = vers_open(TEST_VFILE, TEST_VOBJ1, NULL);
     if (! v1)
          elog_die(FATAL, "[2] unable to open vers");
     r = vers_new(v1, TEST_TEXT2, 0, "nigel", "first version");
     if (r == -1)
          elog_die(FATAL, "[2a] unable to make new");
     if (r != 1)
          elog_die(FATAL, "[2a] unexpected sequence %d", r);
     r = vers_new(v1, TEST_TEXT3, 0, "nigel", "first version");
     if (r == -1)
          elog_die(FATAL, "[2b] unable to make new");
     if (r != 2)
          elog_die(FATAL, "[2b] unexpected sequence %d", r);
     vers_close(v1);

     /* test 3: check stats */
     v1 = vers_open(TEST_VFILE, TEST_VOBJ1, NULL);
     if (! v1)
          elog_die(FATAL, "[3] unable to open vers");
     if (vers_nversions(v1) != 3)
          elog_die(FATAL, "[3] wrong number of versions %d",
		   vers_nversions(v1));
     if ( ! vers_islatest(v1, 2))
          elog_die(FATAL, "[3a] v2 not latest");
     if ( vers_islatest(v1, 3))
          elog_die(FATAL, "[3b] v3 is latest apparently!!");
     if ( ! vers_contains(v1, 0))
          elog_die(FATAL, "[3c] does not contain v0");
     if ( ! vers_contains(v1, 1))
          elog_die(FATAL, "[3d] does not contain v1");
     if ( ! vers_contains(v1, 2))
          elog_die(FATAL, "[3e] does not contain v2");
     if (vers_contains(v1, 3))
          elog_die(FATAL, "[3f] does contain v3");
     vers_close(v1);

     /* test 4: test reading with getlatest */
     v1 = vers_open(TEST_VFILE, TEST_VOBJ1, NULL);
     if (! v1)
          elog_die(FATAL, "[4] unable to open vers");
     r = vers_getlatest(v1, &data, &dlen, &author, &comment, &mtime, &version);
     if ( ! r )
          elog_die(FATAL, "[4] unable to get latest");
     if (strcmp(data, TEST_TEXT3))
          elog_die(FATAL, "[4] data does not match");
     if (strcmp(author, "nigel"))
          elog_die(FATAL, "[4] author does not match");
     if (strcmp(comment, "first version"))
          elog_die(FATAL, "[4] comment does not match");
     if (version != 2)
          elog_die(FATAL, "[4] version does not match");
     nfree(data);
     nfree(author);
     nfree(comment);
     vers_close(v1);

     /* test 5: test reading with getversion */
     v1 = vers_open(TEST_VFILE, TEST_VOBJ1, NULL);
     if (! v1)
          elog_die(FATAL, "[5] unable to open vers");
     r = vers_getversion(v1, 1, &data, &dlen, &author, &comment, &mtime);
     if ( ! r )
          elog_die(FATAL, "[5] unable to get latest");
     if (strcmp(data, TEST_TEXT2))
          elog_die(FATAL, "[5] data does not match");
     if (strcmp(author, "nigel"))
          elog_die(FATAL, "[5] author does not match");
     if (strcmp(comment, "first version"))
          elog_die(FATAL, "[5] comment does not match");
     nfree(data);
     nfree(author);
     nfree(comment);
     vers_close(v1);


     /* test 6: test modification with edit */
     v1 = vers_open(TEST_VFILE, TEST_VOBJ1, NULL);
     if (! v1)
          elog_die(FATAL, "[6a] unable to open vers");
     r = vers_getversion(v1, 0, &data, &dlen, &author, &comment, &mtime);
     if ( ! r )
          elog_die(FATAL, "[6a] unable to get latest");
     if (strcmp(data, TEST_TEXT1))
          elog_die(FATAL, "[6a] data does not match");
     if (strcmp(author, "nigel"))
          elog_die(FATAL, "[6a] author does not match");
     if (strcmp(comment, "first version"))
          elog_die(FATAL, "[6a] comment does not match");
     nfree(data);
     nfree(author);
     nfree(comment);
		     /* save */
     r = vers_edit(v1, 0, "sarah", "modified revision");
     if ( ! r )
          elog_die(FATAL, "[6b] unable to edit");
		     /* check it back */
     r = vers_getversion(v1, 0, &data, &dlen, &author, &comment, &mtime);
     if ( ! r )
          elog_die(FATAL, "[6c] unable to get latest");
     if (strcmp(data, TEST_TEXT1))
          elog_die(FATAL, "[6c] data does not match");
     if (strcmp(author, "sarah"))
          elog_die(FATAL, "[6c] author does not match");
     if (strcmp(comment, "modified revision"))
          elog_die(FATAL, "[6c] comment does not match");
     nfree(data);
     nfree(author);
     nfree(comment);
     vers_close(v1);
     
     /* test 7: data at home */
     v1 = vers_open(TEST_VFILE, TEST_VOBJ1, NULL);
     vobj1 = vers_lsvers(v1);
     tree_traverse(vobj1)
       printf("%s\n", tree_getkey(vobj1));

     if (tree_n(vobj1) != 1)
          elog_die(FATAL, "[7] version object list (%d) != 1",
		   tree_n(vobj1));
     tree_first(vobj1);
     if ( strncmp(tree_getkey(vobj1), TEST_VOBJ1, strlen(TEST_VOBJ1)) )
          elog_die(FATAL, "[7] returned object (%s) != %s",
		   tree_getkey(vobj1), TEST_VOBJ1);
     vers_freelsrings(vobj1);
     vers_close(v1);

     /* finalise */
     vers_fini();
     elog_fini();
     route_close(err);
     route_fini();
     printf("tests finished successfully\n");
     exit(0);
}

#endif /* TEST */

