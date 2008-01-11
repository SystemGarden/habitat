/*
 * Route driver for files based on holstore: holstore, timestore, tablestore,
 * versionstore
 *
 * Nigel Stuckey, July 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include "nmalloc.h"
#include "cf.h"
#include "elog.h"
#include "util.h"
#include "holstore.h"
#include "timestore.h"
#include "tablestore.h"
#include "versionstore.h"
#include "rt_store.h"

/* private functional prototypes */
RT_STORED rt_store_from_lld(RT_LLD lld);

/* private functional prototypes */
RT_STORED rt_storehol_from_lld(RT_LLD lld);
RT_STORED rt_storetime_from_lld(RT_LLD lld);
RT_STORED rt_storetab_from_lld(RT_LLD lld);
RT_STORED rt_storever_from_lld(RT_LLD lld);

const struct route_lowlevel rt_storehol_method = {
     rt_storehol_magic,   rt_storehol_prefix,   rt_storehol_description,
     rt_storehol_init,    rt_storehol_fini,     rt_storehol_access,
     rt_storehol_open,    rt_storehol_close,    rt_storehol_write,
     rt_storehol_twrite,  rt_storehol_tell,     rt_storehol_read,
     rt_storehol_tread 
};

const struct route_lowlevel rt_storetime_method = {
     rt_storetime_magic,  rt_storetime_prefix,  rt_storetime_description,
     rt_storetime_init,   rt_storetime_fini,    rt_storetime_access,
     rt_storetime_open,   rt_storetime_close,   rt_storetime_write,
     rt_storetime_twrite, rt_storetime_tell,    rt_storetime_read,
     rt_storetime_tread 
};

const struct route_lowlevel rt_storetab_method = {
     rt_storetab_magic,   rt_storetab_prefix,   rt_storetab_description,
     rt_storetab_init,    rt_storetab_fini,     rt_storetab_access,
     rt_storetab_open,    rt_storetab_close,    rt_storetab_write,
     rt_storetab_twrite,  rt_storetab_tell,     rt_storetab_read,
     rt_storetab_tread 
};

const struct route_lowlevel rt_storever_method = {
     rt_storever_magic,   rt_storever_prefix,   rt_storever_description,
     rt_storever_init,    rt_storever_fini,     rt_storever_access,
     rt_storever_open,    rt_storever_close,    rt_storever_write,
     rt_storever_twrite,  rt_storever_tell,     rt_storever_read,
     rt_storever_tread 
};

char *rt_storetime_schema[] = {"_time", "_seq", "data", NULL};

int rt_storehol_debug=0;
int rt_storetime_debug=0;
int rt_storetab_debug=0;
int rt_storever_debug=0;

int    rt_storehol_magic() { return RT_STOREHOL_LLD_MAGIC; }
char * rt_storehol_prefix() { return "hol"; }
char * rt_storehol_description() { return "holstore"; }
void   rt_storehol_init  (CF_VALS cf, int debug) {rt_storehol_debug=debug;}
void   rt_storehol_fini  () {}

/* Check accessability of the holstore file. Returns 1 for can access or 
 * 0 for no access */
int    rt_storehol_access(char *p_url, char *password, char *basename, 
			   int flag)
{
     HOLD id;
     char *file;

     if (!basename)
	  return 0;

     file = xnstrdup(basename);
     strtok(file, ",");
     if ((id = hol_open(file))) {
	  hol_close(id);
	  nfree(file);
	  return 1;	/* success */
     } else {
	  nfree(file);
	  return 0;	/* failure */
     }
}

/* open holstore, returning the descriptor for success or NULL for failure */
RT_LLD rt_storehol_open (char *p_url, char *comment, char *password, int keep,
			  char *basename)
{
     RT_STORED rt;
     HOLD id;
     char *file, *obj;

     if ( ! basename)
	  return NULL;

     file = xnstrdup(basename);
     if (strtok(file, ",") == NULL)
	  return NULL;		/* no fathomable object in address */
     obj = strtok(NULL, ",");

     id = hol_open(file);
     if ( !id ) {
	  if (!keep) {
	       nfree(file);
	       return NULL;	/* dont create */
	  }
	  id = hol_create(file, 0644);
	  if ( ! id) {
	       /* well... we tried */
	       if (rt_storehol_debug)
		    elog_printf(DEBUG, "Unable to "
				"open or create holstore `%s,%s'", 
				file, obj);
	       nfree(file);
	       return NULL;
	  }
     }

     rt = nmalloc(sizeof(struct rt_store_desc));
     rt->magic = rt_storehol_magic();
     rt->prefix = rt_storehol_prefix();
     rt->description = rt_storehol_description();
     rt->p_url = p_url;
     rt->filepath = file;
     rt->object = obj;
     rt->password = NULL;
     rt->hol_id = id;

     return rt;
}

void   rt_storehol_close (RT_LLD lld)
{
     RT_STORED rt;

     rt = rt_store_from_lld(lld);

     hol_close(rt->hol_id);
     rt->magic = 0;	/* don't use again */
     nfree(rt->filepath);
     nfree(rt);
}

/* write to holstore, return the number of charaters written or -1 for error */
int    rt_storehol_write (RT_LLD lld, const void *buf, int buflen)
{
     RT_STORED rt;

     rt = rt_store_from_lld(lld);

     if (hol_put(rt->hol_id, rt->object, (void *) buf, buflen))
	  return buflen;
     else
	  return -1;
}

/* write to holstore, return 1 for success or 0 for failure */
int    rt_storehol_twrite (RT_LLD lld, TABLE tab)
{
     RT_STORED rt;
     char *buf;
     int r;

     rt = rt_store_from_lld(lld);

     buf = table_outtable(tab);
     if ( ! buf)
       return 1;	/* empty table, successfully don't writing anything */
     r = hol_put(rt->hol_id, rt->object, (void *) buf, strlen(buf)+1);
     nfree(buf);
     if (r)
	  return 1;
     else
	  return 0;
}

/* Sets file size and modification time; sequence is set to -1
 * Returns 1 for success, 0 for failure */
int    rt_storehol_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     RT_STORED rt;

     rt = rt_store_from_lld(lld);

     *seq  = -1;
     *size = -1;
     *modt = 0;
     return 1;	/* success */
}

/* read file, starting at offset bytes from the start, returning data in
 * a list of ROUTE_BUF or NULL for failure or empty list */
ITREE *rt_storehol_read  (RT_LLD lld, int seq, int offset)
{
     RT_STORED rt;
     ITREE *buflist=NULL;
     ROUTE_BUF *storebuf;
     int len;
     char *buf;

     rt = rt_store_from_lld(lld);

     buf = hol_get(rt->hol_id, rt->object, &len);

     /* create the list */
     if (buf) {
	  buflist = itree_create();
	  storebuf = xnmalloc(sizeof(ROUTE_BUF));
	  itree_append(buflist, storebuf);
	  /* ensure termination of buffer and store in list slot */
	  if (buf[len-1] != '\0') {
	       buf = nrealloc(buf, len+1);
	       buf[len] = '\0';
	  }
	  storebuf->buffer = buf;
	  storebuf->buflen = len;
     }

     return buflist;
}



/* Read data as a table and return it as a TABLE data type.
 * Sequence and offset are ignored for a holstore: they are not applicable.
 * To read back as a table, the data should have been stored as a table
 * before [with rt_storehol_twrite() by writing the output of table_outtable()
 * or table_print()], specifically with headers, info and info separator.
 * Returns a TABLE if successful or a NULL otherwise.
 */
TABLE rt_storehol_tread  (RT_LLD lld, int seq, int offset)
{
     RT_STORED rt;
     int len;
     char *buf;
     TABLE tab;

     rt = rt_store_from_lld(lld);

     buf = hol_get(rt->hol_id, rt->object, &len);
     if (!buf)
	  return NULL;

     /* ensure termination of buffer and store in list slot */
     if (buf[len-1] != '\0') {
	  buf = nrealloc(buf, len+1);
	  buf[len] = '\0';
     }

     /* create the table, assuming headers exist */
     tab = table_create();
     table_scan(tab, buf, "\t", TABLE_SINGLESEP, TABLE_HASCOLNAMES, 
		TABLE_HASRULER);
     table_freeondestroy(tab, buf);

     return tab;
}



int    rt_storetime_magic() { return RT_STORETIME_LLD_MAGIC; }
char * rt_storetime_prefix() { return "ts"; }
char * rt_storetime_description() { return "timestore"; }
void   rt_storetime_init  (CF_VALS cf, int debug) {rt_storetime_debug=debug;}
void   rt_storetime_fini  () {}

/* Check accessability of a holstore file containing timestore structures.
 * Returns 1 for can access or 0 for no access */
int    rt_storetime_access(char *p_url, char *password, char *basename, 
			    int flag)
{
     TS_RING id;
     char *file, *obj;

     if (!basename)
	  return 0;

     file = xnstrdup(basename);
     strtok(file, ",");
     obj = strtok(NULL, ",");
     
     if ((id = ts_open(file, obj, password))) {
	  ts_close(id);
	  nfree(file);
	  return 1;	/* success */
     } else {
	  nfree(file);
	  return 0;	/* failure */
     }
}

/* open timestore, returning the descriptor for success or NULL for failure */
RT_LLD rt_storetime_open (char *p_url, char *comment, char *password, 
			   int keep, char *basename)
{
     RT_STORED rt;
     TS_RING id;
     char *file, *obj;

     if ( ! basename)
	  return NULL;

     file = xnstrdup(basename);
     strtok(file, ",");
     obj = strtok(NULL, ",");

     id = ts_open(file, obj, password);
     if ( ! id) {
	  if (keep > 0)
	       id = ts_create(file, 0644, obj, comment, password, keep);
	  if ( ! id) {
	       /* well... we tried */
	       if (rt_storetime_debug)
		    elog_printf(DEBUG, "Unable to open or create "
				"timestore `%s,%s'", file, obj);
	       nfree(file);
	       return NULL;
	  }
     }

     rt = nmalloc(sizeof(struct rt_store_desc));
     rt->magic = rt_storetime_magic();
     rt->prefix = rt_storetime_prefix();
     rt->description = rt_storetime_description();
     rt->p_url = p_url;
     rt->filepath = file;
     rt->object = obj;
     rt->password = password ? xnstrdup(password) : NULL;
     rt->ts_id = id;

     return rt;
}

void   rt_storetime_close (RT_LLD lld)
{
     RT_STORED rt;

     rt = rt_store_from_lld(lld);

     ts_close(rt->ts_id);
     rt->magic = 0;	/* don't use again */
     nfree(rt->filepath);
     if (rt->password)
	  nfree(rt->password);
     nfree(rt);
}

/* write to holstore, return the number of charaters written or -1 for error */
int    rt_storetime_write (RT_LLD lld, const void *buf, int buflen)
{
     RT_STORED rt;

     rt = rt_store_from_lld(lld);

     if (ts_put(rt->ts_id, (void *) buf, buflen) != -1)
	  return buflen;
     else
	  return -1;
}

/* write to holstore, return 1 for success or 0 for failure */
int    rt_storetime_twrite (RT_LLD lld, TABLE tab)
{
     RT_STORED rt;
     char *buf;

     rt = rt_store_from_lld(lld);

     buf = table_outtable(tab);
     if ( ! buf)
       return 1;	/* empty table, successfully don't writing anything */
     if (ts_put(rt->ts_id, (void *) buf, strlen(buf)) != -1)
	  return 1;
     else
	  return 0;
}

/* Sets file size and modification time; sequence is set to -1
 * Returns 1 for success, 0 for failure */
int    rt_storetime_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     RT_STORED rt;
     int len;
     char *buf;

     rt = rt_store_from_lld(lld);

     ts_jumpyoungest(rt->ts_id);
     ts_jump(rt->ts_id, -1);
     buf = ts_get(rt->ts_id, &len, modt, seq);
     if (buf == NULL && len == -1) {
	  /* failure */
	  elog_printf(ERROR, "unable to get last datum of %s", 
		      rt->p_url);
	  return 0;
     }

     /* success */
     nfree(buf);
     *size = -1;

     return 1;	/* success */
}

/* read file, starting at offset bytes from the start, returning data in
 * a list of ROUTE_BUF or NULL for failure */
ITREE *rt_storetime_read  (RT_LLD lld, int seq, int offset)
{
     RT_STORED rt;
     ITREE *buflist;
     ROUTE_BUF *storebuf;
     int rlen, rseq, i;
     time_t rmodt;
     char *buf;

     rt = rt_store_from_lld(lld);

     buflist = itree_create();
     ts_setjump(rt->ts_id, seq-1);
     i = 0;
     while ( (buf=ts_get(rt->ts_id,&rlen,&rmodt,&rseq)) ) {
	  /* make list entry */
	  storebuf = xnmalloc(sizeof(ROUTE_BUF));
	  itree_append(buflist, storebuf);

	  /* ensure termination of buffer and store in list slot */
	  if (buf[rlen-1] != '\0') {
	       buf = nrealloc(buf, rlen+1);
	       buf[rlen] = '\0';
	  }
	  storebuf->buffer = buf;
	  storebuf->buflen = rlen;

	  i++;
     }

     return buflist;
}


/* read file, starting at offset bytes from the start, returning data in
 * a TABLE () or NULL for failure. */
TABLE rt_storetime_tread  (RT_LLD lld, int seq, int offset)
{
     RT_STORED rt;
     int rlen, rseq;
     time_t rmodt;
     char *buf;
     TABLE tab;

     rt = rt_store_from_lld(lld);

     tab = table_create_a(rt_storetime_schema);
     ts_setjump(rt->ts_id, seq-1);
     while ( (buf=ts_get(rt->ts_id,&rlen,&rmodt,&rseq)) ) {
	  /* ensure termination of buffer and store in TABLE */
	  if (buf[rlen-1] != '\0') {
	       buf = nrealloc(buf, rlen+1);
	       buf[rlen] = '\0';
	  }

	  /* create the list */
	  table_addemptyrow(tab);
	  table_replacecurrentcell_alloc(tab, "_time", util_i32toa(rmodt));
	  table_replacecurrentcell_alloc(tab, "_seq",  util_i32toa(rseq));
	  table_replacecurrentcell(tab, "data", buf);
	  table_freeondestroy(tab, buf);
     }

     return tab;
}



int    rt_storetab_magic() { return RT_STORETAB_LLD_MAGIC; }
char * rt_storetab_prefix() { return "tab"; }
char * rt_storetab_description() { return "tablestore"; }
void   rt_storetab_init  (CF_VALS cf, int debug) {rt_storetab_debug=debug;}
void   rt_storetab_fini  () {}

/* Check accessability of a holstore file containing timestore structures.
 * Returns 1 for can access or 0 for no access */
int    rt_storetab_access(char *p_url, char *password, char *basename, 
			    int flag)
{
     TAB_RING id;
     char *file, *obj;

     if (!basename)
	  return 0;

     file = xnstrdup(basename);
     strtok(file, ",");
     obj = strtok(NULL, ",");
     
     if ((id = tab_open(file, obj, password))) {
	  tab_close(id);
	  nfree(file);
	  return 1;	/* success */
     } else {
	  nfree(file);
	  return 0;	/* failure */
     }
}

/* open timestore, returning the descriptor for success or NULL for failure */
RT_LLD rt_storetab_open (char *p_url, char *comment, char *password, 
			   int keep, char *basename)
{
     RT_STORED rt;
     TAB_RING id;
     char *file, *obj;

     if ( ! basename)
	  return NULL;

     file = xnstrdup(basename);
     strtok(file, ",");
     obj = strtok(NULL, ",");

     id = tab_open(file, obj, password);
     if ( ! id) {
	  if (keep > 0)
	       id = tab_create(file, 0644, obj, comment, password, keep);
	  if ( ! id) {
	       /* well... we tried */
	       if (rt_storetab_debug)
		    elog_printf(DEBUG, "Unable to open or create "
				"tablestore `%s,%s'", file, obj);
	       nfree(file);
	       return NULL;
	  }
     }

     rt = nmalloc(sizeof(struct rt_store_desc));
     rt->magic = rt_storetab_magic();
     rt->prefix = rt_storetab_prefix();
     rt->description = rt_storetab_description();
     rt->p_url = p_url;
     rt->filepath = file;
     rt->object = obj;
     rt->password = password ? xnstrdup(password) : NULL;
     rt->tab_id = id;

     return rt;
}

void   rt_storetab_close (RT_LLD lld)
{
     RT_STORED rt;

     rt = rt_store_from_lld(lld);

     tab_close(rt->tab_id);
     rt->magic = 0;	/* don't use again */
     nfree(rt->filepath);
     if (rt->password)
	  nfree(rt->password);
     nfree(rt);
}

/* write to holstore, return the number of charaters written or -1 for error */
int    rt_storetab_write (RT_LLD lld, const void *buf, int buflen)
{
     RT_STORED rt;

     rt = rt_store_from_lld(lld);

     if (tab_puttext(rt->tab_id, buf) != -1)
	  return buflen;
     else
	  return -1;
}

/* write to holstore, return 1 for success or 0 for failue.
 * If _time is present in table, its value in the first row will be
 * used as the sample's time */
int    rt_storetab_twrite (RT_LLD lld, TABLE tab)
{
     RT_STORED rt;
     time_t samptime;
     int r;

     rt = rt_store_from_lld(lld);

     if (table_hascol(tab, "_time")) {
	  table_first(tab);
	  samptime = strtol(table_getcurrentcell(tab, "_time"), NULL, 10);
	  r = tab_put_withtime(rt->tab_id, tab, samptime);
     } else {
	  r = tab_put(rt->tab_id, tab);
     }
     if (r == -1)
	  return 0;
     else
	  return 1;
}

/* Sets file size and modification time; sequence is set to -1
 * Returns 1 for success, 0 for failure */
int    rt_storetab_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     RT_STORED rt;
     int len;
     char *buf;

     rt = rt_store_from_lld(lld);

     tab_jumpyoungest(rt->tab_id);
     tab_jump(rt->tab_id, -1);
     buf = tab_getraw(rt->tab_id, &len, modt, seq);
     if (buf == NULL && len == -1) {
	  /* failure */
	  elog_printf(ERROR, "unable to get last datum of %s", 
		      rt->p_url);
	  return 0;
     }

     /* success */
     nfree(buf);
     *size = -1;

     return 1;	/* success */
}

/* read file, starting at offset bytes from the start, returning data in
 * a list of ROUTE_BUF or NULL for failure */
ITREE *rt_storetab_read  (RT_LLD lld, int seq, int offset)
{
     RT_STORED rt;
     ITREE *buflist;
     ROUTE_BUF *storebuf;
     int rlen, rseq, i;
     time_t rmodt;
     char *buf;

     rt = rt_store_from_lld(lld);

     buflist = itree_create();
     tab_setjump(rt->tab_id, seq-1);
     i = 0;
     while ( (buf=tab_getraw(rt->tab_id, &rlen, &rmodt, &rseq)) ) {
	  /* make list entry */
	  storebuf = xnmalloc(sizeof(ROUTE_BUF));
	  itree_append(buflist, storebuf);

	  /* ensure termination of buffer and store in list slot */
	  if (buf[rlen-1] != '\0') {
	       buf = nrealloc(buf, rlen+1);
	       buf[rlen] = '\0';
	  }
	  storebuf->buffer = buf;
	  storebuf->buflen = rlen;

	  i++;
     }

     return buflist;
}


/* read file, starting at offset bytes from the start, returning data in
 * a list of ROUTE_BUF or NULL for failure */
TABLE rt_storetab_tread  (RT_LLD lld, int seq, int offset)
{
     RT_STORED rt;
     int rseq;
     time_t rmodt;
     TABLE tab, seqtab;

     rt = rt_store_from_lld(lld);

     tab = table_create();
     tab_setjump(rt->tab_id, seq-1);
     while ( (seqtab=tab_get(rt->tab_id, &rmodt, &rseq)) ) {
	  table_addcol(seqtab, "_seq", NULL);
	  table_addcol(seqtab, "_time", NULL);
	  table_traverse(seqtab) {
	       table_replacecurrentcell_alloc(seqtab, "_seq", 
					      util_i32toa(rseq));
	       table_replacecurrentcell_alloc(seqtab, "_time", 
					      util_i32toa(rmodt));
	  }
	  table_addtable(tab, seqtab, 1);
	  table_destroy(seqtab);
     }

     return tab;
}




int    rt_storever_magic() { return RT_STOREVER_LLD_MAGIC; }
char * rt_storever_prefix() { return "vs"; }
char * rt_storever_description() { return "versionstore"; }
void   rt_storever_init  (CF_VALS cf, int debug) {rt_storever_debug=debug;}
void   rt_storever_fini  () {}

/* Check accessability of a holstore file containing timestore structures.
 * Returns 1 for can access or 0 for no access */
int    rt_storever_access(char *p_url, char *password, char *basename, 
			    int flag)
{
     VS id;
     char *file, *obj;

     if (!basename)
	  return 0;

     file = xnstrdup(basename);
     strtok(file, ",");
     obj = strtok(NULL, ",");
     
     if ((id = vers_open(file, obj, password))) {
	  vers_close(id);
	  nfree(file);
	  return 1;	/* success */
     } else {
	  nfree(file);
	  return 0;	/* failure */
     }
}

/* open timestore, returning the descriptor for success or NULL for failure */
RT_LLD rt_storever_open (char *p_url, char *comment, char *password, 
			   int keep, char *basename)
{
     RT_STORED rt;
     VS id;
     char *file, *obj;

     if ( ! basename)
	  return NULL;

     file = xnstrdup(basename);
     strtok(file, ",");
     obj = strtok(NULL, ",");

     id = vers_open(file, obj, password);
     if ( ! id) {
	  if (keep > 0)
	       id = vers_create(file, 0644, obj, password, comment);
	  if ( ! id) {
	       /* well... we tried */
	       if (rt_storever_debug)
		    elog_printf(DEBUG, "Unable to open or create "
				"tablestore `%s,%s'", file, obj);
	       nfree(file);
	       return NULL;
	  }
     }

     rt = nmalloc(sizeof(struct rt_store_desc));
     rt->magic = rt_storever_magic();
     rt->prefix = rt_storever_prefix();
     rt->description = rt_storever_description();
     rt->p_url = p_url;
     rt->filepath = file;
     rt->object = obj;
     rt->password = password ? xnstrdup(password) : NULL;
     rt->vs_id = id;

     return rt;
}

void   rt_storever_close (RT_LLD lld)
{
     RT_STORED rt;

     rt = rt_store_from_lld(lld);

     vers_close(rt->vs_id);
     rt->magic = 0;	/* don't use again */
     nfree(rt->filepath);
     if (rt->password)
	  nfree(rt->password);
     nfree(rt);
}

/* write to versionstore, return the number of charaters written or -1 
 * for error */
int    rt_storever_write (RT_LLD lld, const void *buf, int buflen)
{
     RT_STORED rt;
     char *startcmt, *newbuf=NULL;
     int datalen, r;

     rt = rt_store_from_lld(lld);

     /* the buffer is presented  as <buffer>[\001<comment>]
      * where the comment is optional */
     startcmt = memchr(buf, 1, buflen);
     if (startcmt == NULL) {
	  startcmt = "From a route";
	  datalen = buflen;
     } else {
	  /* find comment */
	  datalen = startcmt - (char *) buf;
	  startcmt++;
	  /* null terminate comment */
	  if (memchr(startcmt, 0, buflen - (datalen+1)) == NULL) {
	       /* not null terminated - make it so! */
	       newbuf = xnmemdup(buf, buflen+1);
	       startcmt = newbuf + datalen +1;
	       *(newbuf + buflen) = '\0';
	  }
     }

     r = vers_new(rt->vs_id, newbuf ? newbuf : (char *)buf, datalen,
		  getpwuid(getuid())->pw_name, startcmt);
     if (newbuf)
	  nfree(newbuf);

     if (r != -1) {
	  return buflen;	/* success */
     } else {
	  return -1;
     }
}

/* Write TABLE to versionstore, return 1 for success or 0 for failure
 * The following columns should be present:- author, comment, data */
int    rt_storever_twrite (RT_LLD lld, TABLE tab)
{
#if 0
     RT_STORED rt;
     char *startcmt, *newbuf=NULL;
     int datalen, r;
#endif

     return 0;	/* failure */

#if 0
     rt = rt_store_from_lld(lld);

     /* the buffer is presented  as <buffer>[\001<comment>]
      * where the comment is optional */
     startcmt = memchr(buf, 1, buflen);
     if (startcmt == NULL) {
	  startcmt = "From a route";
	  datalen = buflen;
     } else {
	  /* find comment */
	  datalen = startcmt - (char *) buf;
	  startcmt++;
	  /* null terminate comment */
	  if (memchr(startcmt, 0, buflen - (datalen+1)) == NULL) {
	       /* not null terminated - make it so! */
	       newbuf = xnmemdup(buf, buflen+1);
	       startcmt = newbuf + datalen +1;
	       *(newbuf + buflen) = '\0';
	  }
     }

     r = vers_new(rt->vs_id, newbuf ? newbuf : (char *)buf, datalen,
		  getpwuid(getuid())->pw_name, startcmt);
     if (newbuf)
	  nfree(newbuf);

     if (r != -1) {
	  return buflen;	/* success */
     } else {
	  return -1;
     }
#endif
}

/* Sets file size and modification time; sequence is set to -1
 * Returns 1 for success, 0 for failure */
int    rt_storever_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     RT_STORED rt;
     int len, r;
     char *buf, *author, *comment;

     rt = rt_store_from_lld(lld);

     r = vers_getlatest(rt->vs_id, &buf, &len, &author, &comment, 
			modt, seq);
     if (r == 1) {
	  nfree(buf);
	  nfree(author);
	  nfree(comment);
	  return 1;
     } else
	  return 0;
}

/* read file, starting at offset bytes from the start, returning data in
 * a list of ROUTE_BUF or NULL for failure */
ITREE *rt_storever_read  (RT_LLD lld, int seq, int offset)
{
     RT_STORED rt;
     ITREE *buflist;
     ROUTE_BUF *storebuf;
     int rlen, i, top;
     time_t rmodt;
     char *buf, *author, *comment;

     rt = rt_store_from_lld(lld);

     buflist = itree_create();
     top = vers_nversions(rt->vs_id);
     for (i=0; i < top; i++) {
	  /* make list entry */
	  storebuf = xnmalloc(sizeof(ROUTE_BUF));
	  itree_append(buflist, storebuf);

	  vers_getversion(rt->vs_id, i, &buf, &rlen,&author,&comment,&rmodt);

	  /* ensure termination of buffer and store in list slot */
	  if (buf[rlen-1] != '\0') {
	       buf = nrealloc(buf, rlen+1);
	       buf[rlen] = '\0';
	  }
	  storebuf->buffer = buf;
	  storebuf->buflen = rlen;
	  nfree(author);
	  nfree(comment);
     }

     return buflist;
}

/* read file, starting at offset bytes from the start, returning data in
 * a list of ROUTE_BUF or NULL for failure */
TABLE rt_storever_tread  (RT_LLD lld, int seq, int offset)
{
     return NULL;	/* failure */
#if 0
     RT_STORED rt;
     ITREE *buflist;
     ROUTE_BUF *storebuf;
     int rlen, i, top;
     time_t rmodt;
     char *buf, *author, *comment;

     rt = rt_store_from_lld(lld);

     buflist = itree_create();
     top = vers_nversions(rt->vs_id);
     for (i=0; i < top; i++) {
	  /* make list entry */
	  storebuf = xnmalloc(sizeof(ROUTE_BUF));
	  itree_append(buflist, storebuf);

	  vers_getversion(rt->vs_id, i, &buf, &rlen,&author,&comment,&rmodt);

	  /* ensure termination of buffer and store in list slot */
	  if (buf[rlen-1] != '\0') {
	       buf = nrealloc(buf, rlen+1);
	       buf[rlen] = '\0';
	  }
	  storebuf->buffer = buf;
	  storebuf->buflen = rlen;
	  nfree(author);
	  nfree(comment);
     }

     return buflist;
#endif
}




/* --------------- Private routines ----------------- */


RT_STORED rt_store_from_lld(RT_LLD lld	/* typeless low level data */)
{
     if (!lld)
	  elog_die(FATAL, "passed NULL low level descriptor");
     if ( (((RT_STORED)lld)->magic != RT_STOREHOL_LLD_MAGIC) &&
	  (((RT_STORED)lld)->magic != RT_STORETIME_LLD_MAGIC) &&
	  (((RT_STORED)lld)->magic != RT_STORETAB_LLD_MAGIC) &&
	  (((RT_STORED)lld)->magic != RT_STOREVER_LLD_MAGIC) )
	  elog_die(FATAL, "magic type mismatch: we were given "
		   "%s (%s) but can only handle either %s (%s), %s (%s), "
		   "%s (%s) or %s (%s)", 
		   ((RT_STORED)lld)->prefix, 
		   ((RT_STORED)lld)->description,
		   rt_storehol_prefix(),  rt_storehol_description(),
		   rt_storetime_prefix(), rt_storetime_description(),
		   rt_storetab_prefix(),  rt_storetab_description(),
		   rt_storever_prefix(),  rt_storever_description() );

     return (RT_STORED) lld;
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
