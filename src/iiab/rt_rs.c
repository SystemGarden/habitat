/*
 * Route driver for ringstore files
 *
 * Nigel Stuckey, October 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include "nmalloc.h"
#include "cf.h"
#include "elog.h"
#include "table.h"
#include "tableset.h"
#include "util.h"
#include "rs_gdbm.h"
#include "rs_berk.h"
#include "rs.h"
#include "rt_rs.h"

/* private functional prototypes */
RT_RSD rt_rs_from_lld(RT_LLD lld);

const struct route_lowlevel rt_grs_method = {
     rt_grs_magic,   rt_grs_prefix,   rt_grs_description,
     rt_rs_init,     rt_rs_fini,      rt_grs_access,
     rt_grs_open,    rt_rs_close,     rt_rs_write,
     rt_rs_twrite,   rt_rs_tell,      rt_rs_read,
     rt_grs_tread,   rt_rs_status,    rt_rs_checkpoint
};

const struct route_lowlevel rt_brs_method = {
     rt_brs_magic,   rt_brs_prefix,   rt_brs_description,
     rt_rs_init,     rt_rs_fini,      rt_brs_access,
     rt_brs_open,    rt_rs_close,     rt_rs_write,
     rt_rs_twrite,   rt_rs_tell,      rt_rs_read,
     rt_brs_tread,   rt_rs_status,    rt_rs_checkpoint
};

char *rt_rs_schema[] = {"_time", "_seq", "text", NULL};

int rt_rs_debug=0;

void   rt_rs_init  (CF_VALS cf, int debug) {rt_rs_debug=debug;}
void   rt_rs_fini  () {}

int    rt_grs_magic()       { return RT_RS_GDBM_LLD_MAGIC; }
char * rt_grs_prefix()      { return RT_RS_GDBM_PREFIX; }
char * rt_grs_description() { return RT_RS_GDBM_DESCRIPTION; }

int    rt_brs_magic ()      { return RT_RS_BERK_LLD_MAGIC; }
char * rt_brs_prefix()      { return RT_RS_BERK_PREFIX; }
char * rt_brs_description() { return RT_RS_BERK_DESCRIPTION; }


/* Check accessability of the ringstore file. Returns 1 for can access or 
 * 0 for no access */
int    rt_grs_access(char *p_url, char *password, char *basename, int flag)
{
     RS id;
     char *file, *ring, *dur;

     if (!basename)
	  return 0;

     /* basename will be of the form: file,ring,dur,attr
      * we dont want the attribute */
     file = xnstrdup(basename);
     util_strtok_sc(file, ",");
     ring = util_strtok_sc(NULL, ",");
     dur = util_strtok_sc(NULL, ",");
     if ( ! (file && ring && dur) ) {
	  nfree(file);
	  elog_printf(ERROR, "need file, ring and duration for "
		      "ringstore (%s:file,ring,dur)", rt_grs_prefix());
	  return 0;
     }

     id = rs_open(&rs_gdbm_method, file, 0644, ring, "don't create", 
		  "don't create", 0, strtol(dur, NULL, 10), 0);

     if (id) {
	  rs_close(id);
	  nfree(file);
	  return 1;	/* success */
     } else {
	  nfree(file);
	  return 0;	/* failure */
     }
}


/* Check accessability of the ringstore file. Returns 1 for can access or 
 * 0 for no access */
int    rt_brs_access(char *p_url, char *password, char *basename, int flag)
{
     RS id;
     char *file, *ring, *dur;

     if (!basename)
	  return 0;

     /* basename will be of the form: file,ring,dur,attr
      * we dont want the attribute */
     file = xnstrdup(basename);
     util_strtok_sc(file, ",");
     ring = util_strtok_sc(NULL, ",");
     dur = util_strtok_sc(NULL, ",");
     if ( ! (file && ring && dur) ) {
	  nfree(file);
	  elog_printf(ERROR, "need file, ring and duration for "
		      "ringstore (" RT_RS_BERK_PREFIX ":file,ring,dur)");
	  return 0;
     }

     id = rs_open(&rs_berk_method, file, 0644, ring, "don't create", 
		  "don't create", 0, strtol(dur, NULL, 10), 0);

     if (id) {
	  rs_close(id);
	  nfree(file);
	  return 1;	/* success */
     } else {
	  nfree(file);
	  return 0;	/* failure */
     }
}


/* open ringstore, returning the descriptor for success or NULL for failure.
 * There are some special routes, donated by adding suffixes:-
 *   ?info    Information about the ring
 *   ?linfo   Long information about the ring (takes more effort)
 *   ?cinfo   Information about consolidated ring
 *   ?clinfo  Long info about consolidated ring (takes more effort)
 */
RT_LLD rt_grs_open (char *p_url, char *comment, char *password, int keep,
		   char *basename)
{
     RS id;
     RT_RSD rt;
     char *file, *ring, *dur, *extra;
     long r_t, from_t=-1, to_t=-1, r_s, from_s=-1, to_s=-1;
     int len, cons=0;
     enum rt_rs_meta meta;

     if (!basename)
	  return NULL;

     /* check for meta information */
     file = xnstrdup(basename);
     util_strrtrim(file);
     len = strlen(file);
     if (len > 6 && strstr(file + len - 5, "?info")) {
          /* ?info special case */
          file[len-5] = '\0';
	  meta = rt_rs_info;
     } else if (len > 7 && strstr(file + len - 6, "?linfo")) {
          /* ?linfo special case */
          file[len-6] = '\0';
	  meta = rt_rs_linfo;
     } else if (len > 7 && strstr(file + len - 6, "?cinfo")) {
          /* ?cinfo special case */
          file[len-6] = '\0';
	  meta = rt_rs_cinfo;
     } else if (len > 8 && strstr(file + len - 7, "?clinfo")) {
          /* ?clinfo special case */
          file[len-7] = '\0';
	  meta = rt_rs_clinfo;
     } else {
          meta = rt_rs_none;
     }

     /* basename will be of the form: file,ring,dur[,attr...] */
     util_strtok_sc(file, ",");
     ring = util_strtok_sc(NULL, ",");
     dur = util_strtok_sc(NULL, ",");
     if ( meta == rt_rs_none && ! (file && ring && dur) ) {
	  nfree(file);
	  elog_printf(ERROR, "need file, ring and duration for "
		      "ringstore (" RT_RS_GDBM_PREFIX 
		      ":file,ring,dur[,attr][,s=..][,t=..]), "
		      "given %s", basename);
	  return NULL;
     }
     while ( (extra = util_strtok_sc(NULL, ",")) ) {
	  if (strncmp(extra, "t=", 2) == 0) {
	       /* time specified */
	       r_t = sscanf(extra, "t=%ld-%ld", &from_t, &to_t);
	  } else if (strncmp(extra, "s=", 2) == 0) {
	       /* sequence specified */
	       r_s = sscanf(extra, "s=%ld-%ld", &from_s, &to_s);
	  } else {
	       /* attributes specified */
	       /* not yet implemented */
	  }
     }
     if (dur && strcmp(dur, "cons") == 0)
          cons++;

     if (meta == rt_rs_none && !cons) {
          id = rs_open(&rs_gdbm_method, file, 0644, ring, "dont create",
		       "dont create", 0, strtol(dur, NULL, 10), 0);
	  if ( !id ) {
	       if (keep)
		    id = rs_open(&rs_gdbm_method, file, 0644, ring, ring,
				 comment, keep, strtol(dur, NULL, 10),
				 RS_CREATE);
	       if ( ! id ) {
		    /* well... we tried */
		    elog_printf(DEBUG, "Unable to open %sringstore "
				"`" RT_RS_GDBM_PREFIX "%s,%s,%s'", 
				(keep > 0) ? "or create " : "", 
				file, ring, dur);
		    nfree(file);
		    return NULL;
	       }
	  }
     } else {
          if (access(file, R_OK)) {
	       elog_printf(DEBUG, "Unable to access ringstore " 
			   RT_RS_GDBM_PREFIX ":%s for info or consolidation", 
			   file);
	       nfree(file);
	       return NULL;
	  }
	  id = NULL;
     }

     rt = nmalloc(sizeof(struct rt_rs_desc));
     rt->magic = rt_grs_magic();
     rt->prefix = rt_grs_prefix();
     rt->description = rt_grs_description();
     rt->p_url = p_url;
     rt->filepath = file;
     rt->ring = ring;
     rt->duration = dur ? strtol(dur, NULL, 10) : 0;
     rt->password = password ? xnstrdup(password) : NULL;
     rt->rs_id = id;
     rt->from_t = (r_t >= 1) ? from_t : -1;
     rt->to_t   = (r_t >= 2) ? to_t   : -1;
     if (r_t == 1) rt->to_t = time(NULL);
     rt->from_s = (r_s >= 1) ? from_s : -1;
     rt->to_s   = (r_s >= 2) ? to_s   : -1;
     if (r_s == 1) rt->to_s = INT_MAX;
     /*elog_printf(DEBUG, "t = %ld - %ld, s = %ld - %ld", rt->from_t, 
       rt->to_t, rt->from_s, rt->to_s);*/
     rt->meta = meta;
     rt->cons = cons;

     return rt;
}


/* open ringstore, returning the descriptor for success or NULL for failure.
 * There are some special routes, donated by adding suffixes:-
 *   ?info    Information about the ring
 *   ?linfo   Long information about the ring (takes more effort)
 *   ?cinfo   Information about consolidated ring
 *   ?clinfo  Long info about consolidated ring (takes more effort)
 */
RT_LLD rt_brs_open (char *p_url, char *comment, char *password, int keep,
		   char *basename)
{
     RS id;
     RT_RSD rt;
     char *file, *ring, *dur, *extra;
     long r_t, from_t=-1, to_t=-1, r_s, from_s=-1, to_s=-1;
     int len, cons=0;
     enum rt_rs_meta meta;

     if (!basename)
	  return NULL;

     /* check for meta information */
     file = xnstrdup(basename);
     util_strrtrim(file);
     len = strlen(file);
     if (len > 6 && strstr(file + len - 5, "?info")) {
          /* ?info special case */
          file[len-5] = '\0';
	  meta = rt_rs_info;
     } else if (len > 7 && strstr(file + len - 6, "?linfo")) {
          /* ?linfo special case */
          file[len-6] = '\0';
	  meta = rt_rs_linfo;
     } else if (len > 7 && strstr(file + len - 6, "?cinfo")) {
          /* ?cinfo special case */
          file[len-6] = '\0';
	  meta = rt_rs_cinfo;
     } else if (len > 8 && strstr(file + len - 7, "?clinfo")) {
          /* ?clinfo special case */
          file[len-7] = '\0';
	  meta = rt_rs_clinfo;
     } else {
          meta = rt_rs_none;
     }

     /* basename will be of the form: file,ring,dur[,attr...] */
     util_strtok_sc(file, ",");
     ring = util_strtok_sc(NULL, ",");
     dur = util_strtok_sc(NULL, ",");
     if ( meta == rt_rs_none && ! (file && ring && dur) ) {
	  nfree(file);
	  elog_printf(ERROR, "need file, ring and duration for "
		      "ringstore (" RT_RS_BERK_PREFIX 
		      ":file,ring,dur[,attr][,s=..][,t=..]), "
		      "given %s", basename);
	  return NULL;
     }
     while ( (extra = util_strtok_sc(NULL, ",")) ) {
	  if (strncmp(extra, "t=", 2) == 0) {
	       /* time specified */
	       r_t = sscanf(extra, "t=%ld-%ld", &from_t, &to_t);
	  } else if (strncmp(extra, "s=", 2) == 0) {
	       /* sequence specified */
	       r_s = sscanf(extra, "s=%ld-%ld", &from_s, &to_s);
	  } else {
	       /* attributes specified */
	       /* not yet implemented */
	  }
     }
     if (dur && strcmp(dur, "cons") == 0)
          cons++;

     if (meta == rt_rs_none && !cons) {
          id = rs_open(&rs_berk_method, file, 0644, ring, "dont create",
		       "dont create", 0, strtol(dur, NULL, 10), 0);
	  if ( !id ) {
	       if (keep)
		    id = rs_open(&rs_berk_method, file, 0644, ring, ring,
				 comment, keep, strtol(dur, NULL, 10),
				 RS_CREATE);
	       if ( ! id ) {
		    /* well... we tried */
		    elog_printf(DEBUG, "Unable to open %sringstore "
				"'" RT_RS_BERK_PREFIX  ":%s,%s,%s'", 
				(keep > 0) ? "or create " : "", 
				file, ring, dur);
		    nfree(file);
		    return NULL;
	       }
	  }
     } else {
          if (access(file, R_OK)) {
	       elog_printf(DEBUG, "Unable to access ringstore " RT_RS_BERK_PREFIX 
			   ":%s for info or consolidation", file);
	       nfree(file);
	       return NULL;
	  }
	  id = NULL;
     }

     rt = nmalloc(sizeof(struct rt_rs_desc));
     rt->magic = rt_brs_magic();
     rt->prefix = rt_brs_prefix();
     rt->description = rt_brs_description();
     rt->p_url = p_url;
     rt->filepath = file;
     rt->ring = ring;
     rt->duration = dur ? strtol(dur, NULL, 10) : 0;
     rt->password = password ? xnstrdup(password) : NULL;
     rt->rs_id = id;
     rt->from_t = (r_t >= 1) ? from_t : -1;
     rt->to_t   = (r_t >= 2) ? to_t   : -1;
     if (r_t == 1) rt->to_t = time(NULL);
     rt->from_s = (r_s >= 1) ? from_s : -1;
     rt->to_s   = (r_s >= 2) ? to_s   : -1;
     if (r_s == 1) rt->to_s = INT_MAX;
     /*elog_printf(DEBUG, "t = %ld - %ld, s = %ld - %ld", rt->from_t, 
       rt->to_t, rt->from_s, rt->to_s);*/
     rt->meta = meta;
     rt->cons = cons;

     return rt;
}


void   rt_rs_close (RT_LLD lld)
{
     RT_RSD rt;

     rt = rt_rs_from_lld(lld);

     if (rt->rs_id)
          rs_close(rt->rs_id);
     rt->magic = 0;	/* don't use again */
     nfree(rt->filepath);
     if (rt->password)
	  nfree(rt->password);
     nfree(rt);
}

/* write to ringstore, return the number of charaters written 
 * or -1 for error */
int    rt_rs_write (RT_LLD lld, const void *buf, int buflen)
{
     RT_RSD rt;
     TABLE tab;
     int r;

     rt = rt_rs_from_lld(lld);

     tab = table_create_a(rt_rs_schema);
     table_addemptyrow(tab);
     table_replacecurrentcell_alloc(tab, "_seq",  "0");
     table_replacecurrentcell_alloc(tab, "_time", util_u32toa(time(NULL)));
     table_replacecurrentcell_alloc(tab, "text",  (char *) buf);
     r = rs_put(rt->rs_id, tab);
     table_destroy(tab);
     if (r)
	  return buflen;
     else
	  return -1;
}

/* write to ringstore, return 1 for success or 0 for failure */
int    rt_rs_twrite (RT_LLD lld, TABLE tab)
{
     RT_RSD rt;

     rt = rt_rs_from_lld(lld);

     return rs_put(rt->rs_id, tab);
}

/* Returns the sequence size of the ringstore;
 * size is set to -1 (always for ringstore)
 * Returns 1 for success, 0 for failure */
int    rt_rs_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     RT_RSD rt;

     rt = rt_rs_from_lld(lld);

     if (rt->rs_id) {
          rs_youngest(rt->rs_id, seq, modt);
     } else {
          /* rs not open but valid RT_RSD, assume it is meta info */
          *seq = 0;
	  *modt = 0;
     }
     *size = -1;

     return 1;	/* success */
}


/* read storage, starting at seq, returning data in a list of ROUTE_BUF 
 * or NULL for failure. No data returns an empty list not an error.
 * Meta data returns special information about the route. */
ITREE *rt_rs_read  (RT_LLD lld, int seq, int offset)
{
     RT_RSD rt;
     TABLE tab;
     TABSET tset;
     TREE *seqvals;
     ITREE *buflist;
     ROUTE_BUF *storebuf;
     int textonly=0;
     char *buf;

     rt = rt_rs_from_lld(lld);

     if (rs_goto_seq(rt->rs_id, seq) != seq)
	  return NULL;
     if ( ! (tab = rs_mget_nseq(rt->rs_id, 10000)))
	  return NULL;
     if (table_ncols(tab) == 4 &&
	 table_hascol(tab, "_seq") &&
	 table_hascol(tab, "_time") &&
	 table_hascol(tab, "_dur") &&
	 table_hascol(tab, "text"))
          textonly++;

     /* create the list */
     buflist = itree_create();
     seqvals = table_uniqcolvals(tab, "_seq", NULL);
     if (seqvals) {
          tset = tableset_create(tab);
	  tree_traverse(seqvals) {
	       /* select out the data by sequence */
	       tableset_reset(tset);
	       tableset_where(tset, "_seq", eq, tree_getkey(seqvals));
	       if (textonly) {
		    /* print unlabelled text column */
		    tableset_selectt(tset, "text");
		    buf = tableset_print(tset, 0, 0, 0, 1);
	       } else {
		    /* print whole table */
		    buf = tableset_print(tset, 0, 1, 1, 1);
	       }
	       /* save as ROUTE_BUF appended to list */
	       storebuf = xnmalloc(sizeof(ROUTE_BUF));
	       itree_append(buflist, storebuf);
	       storebuf->buffer = buf;
	       storebuf->buflen = strlen(buf);
	  }
	  tableset_destroy(tset);
	  tree_destroy(seqvals);
     }
     table_destroy(tab);

     return buflist;
}



/* Read data from seq to the end and return it as a TABLE data type.
 * To read back as a table, the data should have been stored as a table
 * before [with rt_grs_twrite() by writing the output of table_outtable()
 * or table_print()], specifically with headers, info and info separator.
 * If time or sequences were specified when opening [rt_grs_open], then
 * stateless calls will be used.
 * If seq is older than the oldest in the store, then data will be
 * read from from the oldest onwards. 
 * If seq is newer that the youngest stored, then it is treated as 
 * up to date and NULL is returned.
 * Sequences are allowed to be 'loose' as the ringstore operates with
 * many writers changing the state.
 * Returns a TABLE if successful or a NULL otherwise.
 */
TABLE rt_grs_tread  (RT_LLD lld, int seq, int offset)
{
     RT_RSD rt;

     rt = rt_rs_from_lld(lld);

     if (rt->meta == rt_rs_info) {
          /* return meta data not stored data */
          return rs_lsrings(&rs_gdbm_method, rt->filepath);
     } else if (rt->meta == rt_rs_linfo) {
          /* return meta data not stored data */
          return rs_inforings(&rs_gdbm_method, rt->filepath);
     } else if (rt->meta == rt_rs_cinfo) {
          /* return meta data not stored data */
          return rs_lsconsrings(&rs_gdbm_method, rt->filepath);
     } else if (rt->meta == rt_rs_clinfo) {
          /* return meta data not stored data */
          return rs_infoconsrings(&rs_gdbm_method, rt->filepath);
     }

     if (rt->cons) {
          return rs_mget_cons(&rs_gdbm_method, rt->filepath, rt->ring, 
			      rt->from_t, rt->to_t);
     }

     if (rt->from_t == -1 && rt->to_t == -1 && 
	 rt->from_s == -1 && rt->to_s == -1) {
          /* stateful access */
          if (rs_goto_seq(rt->rs_id, seq) == -1)
	       return NULL;
	  if (seq < rt->rs_id->youngest)
	       /* from current to end */
	       return rs_mget_nseq(rt->rs_id, rt->rs_id->youngest - (seq-1));
	  else
	       /* just the last */
	       return rs_get(rt->rs_id, 0);
     } else {
          /* stateless access */
       return rs_mget_range(rt->rs_id, rt->from_s, rt->to_s,
			    rt->from_t, rt->to_t);
     }
}


/* Read data from seq to the end and return it as a TABLE data type.
 * To read back as a table, the data should have been stored as a table
 * before [with rt_brs_twrite() by writing the output of table_outtable()
 * or table_print()], specifically with headers, info and info separator.
 * If time or sequences were specified when opening [rt_brs_open], then
 * stateless calls will be used.
 * If seq is older than the oldest in the store, then data will be
 * read from from the oldest onwards. 
 * If seq is newer that the youngest stored, then it is treated as 
 * up to date and NULL is returned.
 * Sequences are allowed to be 'loose' as the ringstore operates with
 * many writers changing the state.
 * Returns a TABLE if successful or a NULL otherwise.
 */
TABLE rt_brs_tread  (RT_LLD lld, int seq, int offset)
{
     RT_RSD rt;

     rt = rt_rs_from_lld(lld);

     if (rt->meta == rt_rs_info) {
          /* return meta data not stored data */
          return rs_lsrings(&rs_berk_method, rt->filepath);
     } else if (rt->meta == rt_rs_linfo) {
          /* return meta data not stored data */
          return rs_inforings(&rs_berk_method, rt->filepath);
     } else if (rt->meta == rt_rs_cinfo) {
          /* return meta data not stored data */
          return rs_lsconsrings(&rs_berk_method, rt->filepath);
     } else if (rt->meta == rt_rs_clinfo) {
          /* return meta data not stored data */
          return rs_infoconsrings(&rs_berk_method, rt->filepath);
     }

     if (rt->cons) {
       return rs_mget_cons(&rs_berk_method, rt->filepath, rt->ring, 
			   rt->from_t, rt->to_t);
     }

     if (rt->from_t == -1 && rt->to_t == -1 && 
	 rt->from_s == -1 && rt->to_s == -1) {
          /* stateful access */
          if (rs_goto_seq(rt->rs_id, seq) == -1)
	       return NULL;
	  if (seq < rt->rs_id->youngest)
	       /* from current to end */
	       return rs_mget_nseq(rt->rs_id, rt->rs_id->youngest - (seq-1));
	  else
	       /* just the last */
	       return rs_get(rt->rs_id, 0);
     } else {
          /* stateless access */
       return rs_mget_range(rt->rs_id, rt->from_s, rt->to_s,
			    rt->from_t, rt->to_t);
     }
}


/*
 * Return the status of an open RS descriptor.
 * Free the data from status and info with nfree() if non NULL.
 * If no data is available, either or both status and info may return NULL
 */
void   rt_rs_status(RT_LLD lld, char **status, char **info) {
     if (status)
          *status = NULL;
     if (info)
          *info = NULL;
}


/*
 * Checkpoint using GDBM
 */
int rt_rs_checkpoint(RT_LLD lld)
{
     RT_RSD rt;

     rt = rt_rs_from_lld(lld);

     return rs_checkpoint(rt->rs_id);
}




/* --------------- Private routines ----------------- */


RT_RSD rt_rs_from_lld(RT_LLD lld	/* typeless low level data */)
{
     if (!lld)
	  elog_die(FATAL, "passed NULL low level descriptor");
     if ( ((RT_RSD)lld)->magic != RT_RS_GDBM_LLD_MAGIC &&
	  ((RT_RSD)lld)->magic != RT_RS_BERK_LLD_MAGIC )
	  elog_die(FATAL, "Magic type mismatch: we were given "
		   "%s (%s) but can only handle either %s (%s) or %s (%s)", 
		   ((RT_RSD)lld)->prefix, 
		   ((RT_RSD)lld)->description,
		   rt_brs_prefix(),  rt_brs_description(),
		   rt_grs_prefix(),  rt_grs_description() );

     return (RT_RSD) lld;
}


#if TEST

#define TFILE1 "t.rt_file.dat"
#define TURL1 "file:" TFILE1

int main(int argc, char **argv) {
     int r, seq1, size1;
     CF_VALS cf;
     RT_LLD lld1;
     time_t time1;
     ITREE *chain;
     ROUTE_BUF *rtbuf;

     cf = cf_create();
     rt_store_init(cf, 1);
     unlink(TFILE1);

     /* test 1: is it there? */
     r = rt_store_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have read access to file %s", 
		   TFILE1);
     r = rt_store_access(TURL1, NULL, TFILE1, ROUTE_WRITEOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have write access to file %s", 
		   TFILE1);

     /* test 2: open for read only should not create file */
     lld1 = rt_store_open(TURL1, "blah", NULL, 0, TFILE1);
     if (lld1)
	  elog_die(FATAL, "[2] shouldn't be able to open file %s", 
		   TFILE1);
     r = rt_store_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (r)
	  elog_die(FATAL, "[2] shouldn't have read access to file %s", 
		   TFILE1);
     r = rt_store_access(TURL1, NULL, TFILE1, ROUTE_WRITEOK);
     if (r)
	  elog_die(FATAL, "[2] shouldn't have write access to file %s", 
		   TFILE1);

     /* test 3: create a file */
     lld1 = rt_store_open(TURL1, "blah", NULL, 10, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[3] file %s wasn't created", TFILE1);
     r = rt_store_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (!r)
	  elog_die(FATAL, "[3] can't read access file %s", TFILE1);
     r = rt_store_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (!r)
	  elog_die(FATAL, "[3] can't write access file %s", TFILE1);

     rt_store_close(lld1);

     r = rt_store_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (!r)
	  elog_die(FATAL, "[3a] can't read access file %s", TFILE1);
     r = rt_store_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (!r)
	  elog_die(FATAL, "[3a] can't write access file %s", TFILE1);

     /* test 4: tell me file size on null file */
     lld1 = rt_store_open(TURL1, "blah", NULL, 0, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[4] can't open file %s", TFILE1);
     r = rt_store_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[4] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[4] bad kama! sequence is set to %d", seq1);
     if (size1 != 0)
	  elog_die(FATAL, "[4] bad kama! size is set to %d", size1);
     rt_store_close(lld1);

     /* test 5: write some data to the file */
     lld1 = rt_store_open(TURL1, "blah", NULL, 0, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[5] can't open file %s", TFILE1);
     r = rt_store_write(lld1, "tom, dick and harry", 19);
     if (r != 19)
	  elog_die(FATAL, "[5] wrote %d chars instead of 19", r);
     r = rt_store_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[5] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[5] bad kama! sequence is set to %d", seq1);
     if (size1 != 19)
	  elog_die(FATAL, "[5] bad kama! size is set to %d", size1);
     rt_store_close(lld1);

     /* test 6: read the data back */
     lld1 = rt_store_open(TURL1, "blah", NULL, 0, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[6] can't open file %s", TFILE1);
     chain = rt_store_read(lld1, 0, 0);
     if (itree_n(chain) != 1)
	  elog_die(FATAL, "[6] wrong number of buffers: %d", 
		   itree_n(chain));
     itree_first(chain);
     rtbuf = itree_get(chain);
     if (!rtbuf)
	  elog_die(FATAL, "[6] no buffer");
     if (rtbuf->buflen != 19)
	  elog_die(FATAL, "[6] buffer length is %d not 19", 
		   rtbuf->buflen);
     if (!rtbuf->buffer)
	  elog_die(FATAL, "[6] NULL buffer");
     if (strncmp(rtbuf->buffer, "tom, dick and harry", 19))
	  elog_die(FATAL, "[6] buffer is different: %s", rtbuf->buffer);
     route_free_routebuf(chain);

     r = rt_store_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[6] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[6] bad kama! sequence is set to %d", seq1);
     if (size1 != 19)
	  elog_die(FATAL, "[6] bad kama! size is set to %d", size1);
     rt_store_close(lld1);

     /* test 7: add some more data to it, which should append */
     lld1 = rt_store_open(TURL1, "blah", NULL, 0, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[7a] can't open file %s", TFILE1);
     rt_store_write(lld1, "\nrita, sue and bob too", 22);
     rt_store_close(lld1);
     lld1 = rt_store_open(TURL1, "blah", NULL, 0, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[7b] can't open file %s", TFILE1);
     chain = rt_store_read(lld1, 0, 0);
     if (itree_n(chain) != 1)
	  elog_die(FATAL, "[7] wrong number of buffers: %d", 
		   itree_n(chain));
     itree_first(chain);
     rtbuf = itree_get(chain);
     if (!rtbuf)
	  elog_die(FATAL, "[7] no buffer");
     if (rtbuf->buflen != 41)
	  elog_die(FATAL, "[7] buffer length is %d not 41", 
		   rtbuf->buflen);
     if (!rtbuf->buffer)
	  elog_die(FATAL, "[7] NULL buffer");
     if (strncmp(rtbuf->buffer, "tom, dick and harry\nrita, sue and bob too", 
		 19))
	  elog_die(FATAL, "[7] buffer is different: %s", rtbuf->buffer);
     route_free_routebuf(chain);

     r = rt_store_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[7] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[7] bad kama! sequence is set to %d", seq1);
     if (size1 != 41)
	  elog_die(FATAL, "[7] bad kama! size is set to %d", size1);
     rt_store_close(lld1);

     /* test 8: add some data in overwrite mode */
     lld1 = rt_store_open(TURL1, "blah", NULL, 0, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[8a] can't open file %s", TFILE1);
     r = rt_store_write(lld1, "there should be only the one line", 33);
     if (r != 33)
	  elog_die(FATAL, "[8] can't write");
     rt_store_close(lld1);
     lld1 = rt_store_open(TURL1, "blah", NULL, 0, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[8b] can't open file %s", TFILE1);
     chain = rt_store_read(lld1, 0, 0);
     if (itree_n(chain) != 1)
	  elog_die(FATAL, "[8] wrong number of buffers: %d", 
		   itree_n(chain));
     itree_first(chain);
     rtbuf = itree_get(chain);
     if (!rtbuf)
	  elog_die(FATAL, "[8] no buffer");
     if (rtbuf->buflen != 33)
	  elog_die(FATAL, "[8] buffer length is %d not 33", 
		   rtbuf->buflen);
     if (!rtbuf->buffer)
	  elog_die(FATAL, "[8] NULL buffer");
     if (strncmp(rtbuf->buffer, "there should be only the one line", 33))
	  elog_die(FATAL, "[8] buffer is different: %s", rtbuf->buffer);
     route_free_routebuf(chain);

     r = rt_store_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[8] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[8] bad kama! sequence is set to %d", seq1);
     if (size1 != 33)
	  elog_die(FATAL, "[8] bad kama! size is set to %d", size1);
     rt_store_close(lld1);

     cf_destroy(cf);
     rt_store_fini();
     return 0;
}

#endif /* TEST */
