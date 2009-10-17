/*
 * Route driver for files
 *
 * Nigel Stuckey, July 2003
 * Copyright System Garden Ltd 2003. All rights reserved
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "nmalloc.h"
#include "cf.h"
#include "elog.h"
#include "util.h"
#include "rt_file.h"

/* private fiunctional prototypes */
RT_FILED rt_file_from_lld(RT_LLD lld);

const struct route_lowlevel rt_filea_method = {
     rt_filea_magic,     rt_filea_prefix,    rt_filea_description,
     rt_file_init,       rt_file_fini,       rt_file_access,
     rt_filea_open,      rt_file_close,      rt_file_write,
     rt_file_twrite,     rt_file_tell,       rt_file_read,
     rt_file_tread,      rt_file_status
};

const struct route_lowlevel rt_fileov_method = {
     rt_fileov_magic,    rt_fileov_prefix,   rt_fileov_description,
     rt_file_init,       rt_file_fini,       rt_file_access,
     rt_fileov_open,     rt_file_close,      rt_file_write,
     rt_file_twrite,     rt_file_tell,       rt_file_read,
     rt_file_tread,      rt_file_status
};

char *rt_file_tabschema[] = {"data", "_time", NULL};

int    rt_filea_magic() { return RT_FILEA_LLD_MAGIC; }
char * rt_filea_prefix() { return "file"; }
char * rt_filea_description() { return "local file system (append mode)"; }

int    rt_fileov_magic() { return RT_FILEOV_LLD_MAGIC; }
char * rt_fileov_prefix() { return "fileov"; }
char * rt_fileov_description() { return "local file system (overwrite mode)"; }

void   rt_file_init  (CF_VALS cf, int debug) {}

void   rt_file_fini  () {}

/* Check accessability of a file. Returns 1 for can access or 
 * 0 for no access */
int    rt_file_access(char *p_url, char *password, char *basename, int flag)
{
     if (!basename)
	  return 0;
     if ((flag & ROUTE_READOK) && (access(basename, R_OK) == 0))
	  return 1;
     else if ((flag & ROUTE_WRITEOK) && (access(basename, W_OK) == 0))
	  return 1;
     else
	  return 0;
}

/* Open file in append mode, returning the descriptor for success or NULL
 * for failure.
 * If keep <= 0, then the file will be opened in read-only mode for saftey
 */
RT_LLD rt_filea_open (char *p_url, char *comment, char *password, int keep,
		      char *basename)
{
     RT_FILED rt;
     int fd;

     if ( ! basename)
	  return NULL;

     if (keep > 0) {
          fd = open(basename, (O_RDWR | O_APPEND | O_CREAT | O_EXCL), 0644);
	  if (fd == -1) {
	       if (access(basename, (R_OK |W_OK)) == 0) {
		    fd = open(basename, (O_RDWR | O_APPEND), 0644);
		    if (fd == -1) {
		         elog_printf(DEBUG, "should be able to open "
				     "file %s for writing but can't", 
				     basename);
			 return NULL;
		    }
	       } else {
		    elog_printf(DEBUG, "can't open file %s for writing", 
				basename);
		    return NULL;
	       }
	  }
     } else {
          if (access(basename, R_OK) == 0) {
	       fd = open(basename, O_RDONLY);
	       if (fd == -1) {
		    elog_printf(DEBUG, "should be able to open "
				"file %s for reading but can't (reading in "
				"append method as keep <= 0)", basename);
		    return NULL;
	       }
	  } else if (access(basename, F_OK) == 0) {
	       elog_printf(DEBUG, "file % exists but no read permission "
			   "(reading in append method as keep <= 0)",basename);
	       return NULL;
	  } else {
	       elog_printf(DEBUG, "file %s does not exist for reading; "
			   "check permission of the leading path "
			   "(reading in append method as keep <= 0)",basename);
	       return NULL;
	  }
     }

     rt = nmalloc(sizeof(struct rt_file_desc));
     rt->magic = rt_filea_magic();
     rt->prefix = rt_filea_prefix();
     rt->description = rt_filea_description();
     rt->fd = fd;
     rt->p_url = p_url;
     rt->filepath = basename;

     return rt;
}

/* open file in overwite mode, returning the descriptor for success or NULL
 * for failure. Open in read only mode if keep is set to 0 */
RT_LLD rt_fileov_open (char *p_url, char *comment, char *password, int keep,
		       char *basename)
{
     RT_FILED rt;
     int fd;

     if ( ! basename)
	  return NULL;

     if (keep > 0) {
          fd = open(basename, (O_RDWR | O_TRUNC | O_CREAT | O_EXCL), 0644);
	  if (fd == -1) {
	       if (access(basename, (R_OK |W_OK)) == 0) {
		    fd = open(basename, (O_RDWR | O_TRUNC), 0644);
		    if (fd == -1) {
		         elog_printf(DEBUG, "should be able to open "
				     "file %s for writing but can't", 
				     basename);
			 return NULL;
		    }
	       } else {
		    elog_printf(DEBUG, "can't open file %s for writing", 
				basename);
		    return NULL;
	       }
	  }
     } else {
          if (access(basename, R_OK) == 0) {
	       fd = open(basename, O_RDONLY);
	       if (fd == -1) {
		    elog_printf(DEBUG, "should be able to open "
				"file %s for reading but can't", basename);
		    return NULL;
	       }
	  } else if (access(basename, F_OK) == 0) {
	       elog_printf(DEBUG, "file %s accessable but no read "
			   "permission ", basename);
	       return NULL;
	  } else {
	       elog_printf(DEBUG, "file %s inaccessable; check permission "
			   "of the leading path", basename);
	       return NULL;
	  }
     }

     rt = nmalloc(sizeof(struct rt_file_desc));
     rt->magic = rt_fileov_magic();
     rt->prefix = rt_fileov_prefix();
     rt->description = rt_fileov_description();
     rt->fd = fd;
     rt->p_url = p_url;
     rt->filepath = basename;

     return rt;
}

void   rt_file_close (RT_LLD lld)
{
     RT_FILED rt;

     rt = rt_file_from_lld(lld);

     close(rt->fd);
     rt->magic = 0;	/* don't use again */
     nfree(rt);
}

int    rt_file_write (RT_LLD lld, const void *buf, int buflen)
{
     int n;
     RT_FILED rt;

     rt = rt_file_from_lld(lld);

     n = write(rt->fd, buf, buflen);
     if (n == -1)
	  elog_printf(ERROR, "write() system call returns -1: %s",
		      strerror(errno));
     return n;
}

int    rt_file_twrite (RT_LLD lld, TABLE tab)
{
     RT_FILED rt;
     char *buf;
     int n;

     rt = rt_file_from_lld(lld);

     buf = table_outtable(tab);
     if ( ! buf)
       return 1;	/* empty table, successfully don't writing anything */

     n = write(rt->fd, buf, strlen(buf));
     nfree(buf);
     if (n == -1) {
	  elog_printf(ERROR, "write() system call returns -1: %s",
		      strerror(errno));
	  return 0;
     }
     return 1;
}

/* Sets file size and modification time of current reading point; 
 * sequence is set to -1
 * Returns 1 for success, 0 for failure */
int    rt_file_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
{
     struct stat sbuf;
     int r;
     RT_FILED rt;

     rt = rt_file_from_lld(lld);

     r = fstat(rt->fd, &sbuf);
     if (r == -1) {
	  elog_printf(ERROR, "unable to stat %s", rt->p_url);
	  return 0;	/* failure */
     } else {
	  *seq  = -1;
	  *size = sbuf.st_size;
	  *modt = sbuf.st_mtime;
	  return 1;	/* success */
     }
}


/* Read file, starting at offset bytes from the start, returning data in
 * a list of ROUTE_BUF or NULL for failure.
 * For files, a single element ROUTE_BUF list is returned if there is 
 * text to read; the text will be null terminated for safty. 
 * If there is not text to read, an empty list is returned */
ITREE *rt_file_read  (RT_LLD lld, int seq, int offset)
{
     ROUTE_BUF rtbuf, *storebuf;
     struct stat sbuf;
     int r;
     ITREE *buflist;
     RT_FILED rt;

     rt = rt_file_from_lld(lld);

     /* File -> Read partial file into one element */
     r = fstat(rt->fd, &sbuf);
     if (r == -1) {
          elog_printf(ERROR, "unable to stat %s", rt->p_url);
          return NULL;
     }

     buflist = itree_create();
     if (offset >= sbuf.st_size)
          return buflist;

     lseek(rt->fd, offset, SEEK_SET);
     rtbuf.buffer = nmalloc(sbuf.st_size - offset + 1); /* +1 for NULL */
     if (rtbuf.buffer == NULL) {
          elog_printf(ERROR, "unable to nmalloc(%d) for %s data\n",
                      sbuf.st_size - offset + 1, rt->p_url);
          itree_destroy(buflist);
          return NULL;
     }

     rtbuf.buflen = sbuf.st_size - offset;
     if (read(rt->fd, rtbuf.buffer, rtbuf.buflen) == -1) {
          elog_printf(ERROR, "unable to read(%d, %p, %d) from %s\n",
                      rt->fd, rtbuf.buffer, rtbuf.buflen, rt->p_url);
          itree_destroy(buflist);
          nfree(rtbuf.buffer);
          return NULL;
     }
     rtbuf.buffer[rtbuf.buflen] = '\0';

     /* create the list */
     storebuf = xnmalloc(sizeof(ROUTE_BUF));
     storebuf->buffer = rtbuf.buffer;
     storebuf->buflen = rtbuf.buflen;
     itree_append(buflist, storebuf);

     return buflist;
}


/* read file, starting at offset bytes from the start, returning data in
 * a table or NULL for failure. The data is in a column called 'data' and
 * a timestamp is in a column called '_time' */
TABLE rt_file_tread  (RT_LLD lld, int seq, int offset)
{
     RT_FILED rt;
     struct stat sbuf;
     TABLE tab;
     char *buf;
     int r, buflen;

     rt = rt_file_from_lld(lld);

     /* File -> Read partial file into table */
     r = fstat(rt->fd, &sbuf);
     if (r == -1) {
	  elog_printf(ERROR, "unable to stat %s", rt->p_url);
	  return NULL;
     }

     tab = table_create_a(rt_file_tabschema);
     if (offset >= sbuf.st_size)
	  return tab;

     lseek(rt->fd, offset, SEEK_SET);
     buf = nmalloc(sbuf.st_size - offset + 1);
     if (buf == NULL) {
	  elog_printf(ERROR, "unable to nmalloc(%d) for %s data\n", 
		      sbuf.st_size - offset + 1, rt->p_url);
	  table_destroy(tab);
	  return NULL;
     }
     
     buflen = sbuf.st_size - offset;
     if (read(rt->fd, buf, buflen) == -1) {
	  elog_printf(ERROR, "unable to read(%d, %p, %d) from %s\n",
		      rt->fd,buf, buflen, rt->p_url);
	  table_destroy(tab);
	  nfree(buf);
	  return NULL;
     }
     
     /* create the table */
     table_addemptyrow(tab);
     table_replacecurrentcell(tab, "data", buf);
     table_replacecurrentcell(tab, "_time", util_i32toa(sbuf.st_mtime));
     table_freeondestroy(tab, buf);

     return tab;
}


/*
 * Return the status of an open FILE descriptor.
 * Free the data from status and info with nfree() if non NULL.
 * If no data is available, either or both status and info may return NULL
 */
void   rt_file_status(RT_LLD lld, char **status, char **info) {
     if (status)
          *status = NULL;
     if (info)
          *info = NULL;
}


/* --------------- Private routines ----------------- */


RT_FILED rt_file_from_lld(RT_LLD lld	/* typeless low level data */)
{
     if (!lld)
	  elog_die(FATAL, "passed NULL low level descriptor");
     if ( (((RT_FILED)lld)->magic != RT_FILEA_LLD_MAGIC) &&
	  (((RT_FILED)lld)->magic != RT_FILEOV_LLD_MAGIC) )
	  elog_die(FATAL, "magic type mismatch: we were given "
		   "%s (%s) but can only handle either %s (%s) or %s (%s)", 
		   ((RT_FILED)lld)->prefix, 
		   ((RT_FILED)lld)->description,
		   rt_filea_prefix(), rt_filea_description(),
		   rt_fileov_prefix(), rt_fileov_description() );

     return (RT_FILED) lld;
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
     rt_file_init(cf, 1);
     unlink(TFILE1);

     /* test 1: is it there? */
     r = rt_file_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have read access to file %s", 
		   TFILE1);
     r = rt_file_access(TURL1, NULL, TFILE1, ROUTE_WRITEOK);
     if (r)
	  elog_die(FATAL, "[1] shouldn't have write access to file %s", 
		   TFILE1);

     /* test 2: open for read only should not create file */
     elog_printf(DEBUG, "[2] expect 1 error below --->");
     lld1 = rt_filea_open(TURL1, "blah", NULL, 0, TFILE1);
     if (lld1)
	  elog_die(FATAL, "[2] shouldn't be able to open file %s", 
		   TFILE1);
     r = rt_file_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (r)
	  elog_die(FATAL, "[2] shouldn't have read access to file %s", 
		   TFILE1);
     r = rt_file_access(TURL1, NULL, TFILE1, ROUTE_WRITEOK);
     if (r)
	  elog_die(FATAL, "[2] shouldn't have write access to file %s", 
		   TFILE1);

     /* test 3: create a file */
     lld1 = rt_filea_open(TURL1, "blah", NULL, 10, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[3] file %s wasn't created", TFILE1);
     r = rt_file_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (!r)
	  elog_die(FATAL, "[3] can't read access file %s", TFILE1);
     r = rt_file_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (!r)
	  elog_die(FATAL, "[3] can't write access file %s", TFILE1);

     rt_file_close(lld1);

     r = rt_file_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (!r)
	  elog_die(FATAL, "[3a] can't read access file %s", TFILE1);
     r = rt_file_access(TURL1, NULL, TFILE1, ROUTE_READOK);
     if (!r)
	  elog_die(FATAL, "[3a] can't write access file %s", TFILE1);

     /* test 4: tell me file size on null file */
     lld1 = rt_filea_open(TURL1, "blah", NULL, 0, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[4] can't open file %s", TFILE1);
     r = rt_file_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[4] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[4] bad kama! sequence is set to %d", seq1);
     if (size1 != 0)
	  elog_die(FATAL, "[4] bad kama! size is set to %d", size1);
     rt_file_close(lld1);

     /* test 5: write some data to the file */
     lld1 = rt_filea_open(TURL1, "blah", NULL, 10, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[5] can't open file %s", TFILE1);
     r = rt_file_write(lld1, "tom, dick and harry", 19);
     if (r != 19)
	  elog_die(FATAL, "[5] wrote %d chars instead of 19", r);
     r = rt_file_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[5] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[5] bad kama! sequence is set to %d", seq1);
     if (size1 != 19)
	  elog_die(FATAL, "[5] bad kama! size is set to %d", size1);
     rt_file_close(lld1);

     /* test 6: read the data back */
     lld1 = rt_filea_open(TURL1, "blah", NULL, 0, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[6] can't open file %s", TFILE1);
     chain = rt_file_read(lld1, 0, 0);
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

     r = rt_file_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[6] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[6] bad kama! sequence is set to %d", seq1);
     if (size1 != 19)
	  elog_die(FATAL, "[6] bad kama! size is set to %d", size1);
     rt_file_close(lld1);

     /* test 7: add some more data to it, which should append */
     lld1 = rt_filea_open(TURL1, "blah", NULL, 10, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[7a] can't open file %s", TFILE1);
     rt_file_write(lld1, "\nrita, sue and bob too", 22);
     rt_file_close(lld1);
     lld1 = rt_filea_open(TURL1, "blah", NULL, 10, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[7b] can't open file %s", TFILE1);
     chain = rt_file_read(lld1, 0, 0);
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

     r = rt_file_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[7] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[7] bad kama! sequence is set to %d", seq1);
     if (size1 != 41)
	  elog_die(FATAL, "[7] bad kama! size is set to %d", size1);
     rt_file_close(lld1);

     /* test 8: add some data in overwrite mode */
     lld1 = rt_fileov_open(TURL1, "blah", NULL, 10, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[8a] can't open file %s", TFILE1);
     r = rt_file_write(lld1, "there should be only the one line", 33);
     if (r != 33)
	  elog_die(FATAL, "[8] can't write");
     rt_file_close(lld1);
     lld1 = rt_filea_open(TURL1, "blah", NULL, 10, TFILE1);
     if (!lld1)
	  elog_die(FATAL, "[8b] can't open file %s", TFILE1);
     chain = rt_file_read(lld1, 0, 0);
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

     r = rt_file_tell(lld1, &seq1, &size1, &time1);
     if (!r)
	  elog_die(FATAL, "[8] unable to tell file %s", TFILE1);
     if (seq1 != -1)
	  elog_die(FATAL, "[8] bad kama! sequence is set to %d", seq1);
     if (size1 != 33)
	  elog_die(FATAL, "[8] bad kama! size is set to %d", size1);
     rt_file_close(lld1);

     cf_destroy(cf);
     rt_file_fini();
     return 0;
}

#endif /* TEST */
