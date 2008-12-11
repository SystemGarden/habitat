/*
 * Nigel's malloc
 * This is a memory tracing malloc and friends to help in fixing
 * memory leaks
 * Based on the public alloc routes.
 *
 * Nigel Stuckey, March 99
 * Copyright System Garden Limited 1999-2001. All rights reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "elog.h"
#include "nmalloc.h"

#if NMALLOC
/* This code implements the definitions for leak checking in nmalloc.
 * WARNING! non-reenterent */
#include <stdio.h>
#include "itree.h"
#include "cf.h"
#include "iiab.h"

ITREE *nm_used = NULL;	/* journal table, indexed by allocated 
			 * address */
/* corresponds with and indexed by enum nm_origin */
char *nm_names[] = { "nmalloc", "xnmalloc", "nrealloc", "xnrealloc",
		     "nfree",   "xnfree",   "nstrdup",  "xnstrdup",
		     "nmemdup", "xnmemdup", "adopted",  "forget" };
int nm_active = 1;	/* if nmalloc checking has been activated from
			 * iiab_cf config table. Set to true (active)
			 * by default so the first call goes through
			 * to initialise the class */


/* special version of itree_create for nmalloc only. only the create is 
 * needed as free will never be called */
ITREE *itree_createnocheck() {
	ITREE *t;

	t = malloc(sizeof(ITREE));
	t->node = t->root = make_rb();
	if (t->node)
		return t;
	else
		return NULL;
}

/*
 * Initialisation of nmalloc is a two stage process. If NMALLOC is set,
 * debug check is carried out. If and when the iiab_cf class is 
 * initialised, the following check may be carried out, which may
 * disable further checks
 */
void nm_deactivate()
{
     struct nm_userec *log;

     nm_active = 0;		/* nmalloc is inactive */
     if (nm_used) {
	  /* drop allocations if nmalloc is disabled */
	  itree_traverse(nm_used) {
	       log = itree_get(nm_used);
	       itree_rm(nm_used);
	       free(log->reqfile);
	       free(log->reqfunc);
	       free(log);
	  }
     }
}	  


/* add an allocation entry to the used table */
void nm_add(enum nm_origin meth, void *aloc, size_t sz, char *rfile, 
	    int rline, const char *rfunc) {
     struct nm_userec *log;

     if ( nm_used ) {
          if ( (log = itree_find(nm_used, (unsigned int) aloc)) != ITREE_NOVAL)
	       fprintf(stderr, "nm_add() allocation already in table\n"
		       "   asked - %s %p %d %s:%d:%s\n"
		       "  exists - %s %p %d %s:%d:%s (%ld)\n", 
		       nm_names[meth], aloc, sz, rfile, rline, rfunc, 
		       nm_names[log->method], log->addr, log->length, 
		       log->reqfile, log->reqline, log->reqfunc, 
		       (long) log->when);
     } else {
	  /* create table */
          nm_used = itree_createnocheck();
	  atexit(nm_rpt);
     }

     /* add new entry to use table */
     log = malloc(sizeof(struct nm_userec));	/* the leak checking
						 * has no leaks!! */
     log->when = time(NULL);
     log->method = meth;
     log->addr = aloc;
     log->length = sz;
     log->reqfile = strdup(rfile);
     log->reqline = rline;
     log->reqfunc = strdup(rfunc);
     itree_add(nm_used, (unsigned int) aloc, log);
}

/* remove an allocation entry from the table */
void nm_rm(enum nm_origin meth, void *aloc, char *rfile, int rline, 
	   const char *rfunc) {
     struct nm_userec *log;

     if ( ! nm_used ) {
          fprintf(stderr, "nm_rm() table does not exist - %s %p %s:%d:%s\n",
		  nm_names[meth], aloc, rfile, rline, rfunc);
	  abort();
     }
     if ( (log = itree_find(nm_used, (unsigned int) aloc)) == ITREE_NOVAL ) {
          fprintf(stderr, "nm_rm() allocation not in table - %s %p %s:%d:%s\n",
		  nm_names[meth], aloc, rfile, rline, rfunc);
	  abort();
     }

     itree_rm(nm_used);
     free(log->reqfile);
     free(log->reqfunc);
     free(log);
}

/* Return 1 if aloc has been allocated or 0 otherwise */
int nm_isalloc(void *aloc)
{
#if NMALLOC
     if ( ! nm_active )
	  return 0;

     if ( ! nm_used ) {
          fprintf(stderr, "nm_isalloc() no request for memory ever made\n");
	  return 0;
     }

     if (itree_find(nm_used, (unsigned int) aloc) == ITREE_NOVAL)
	  return 0;
     else
	  return 1;
#else
     return 0;
#endif		/* NMALLOC */
}

/* report on the items in the table to stderr */
void nm_rpt() {
     struct nm_userec *log;
     int nleak;

     if ( ! nm_active )
	  return;

     if ( ! nm_used ) {
          fprintf(stderr, "nm_rpt() no request for memory ever made\n");
	  return;
     }

     nleak = itree_n(nm_used);

     fprintf(stderr, "nm_rpt() %d leaks detected\n", nleak);

#if 1
     if ( ! nleak )
          return;
#endif

     fprintf(stderr, "TIME         METHOD    ALLOTED  SIZE     "
		     "FILE  LINE FUNCTION\n");
     itree_traverse(nm_used) {
          log = itree_get(nm_used);
          fprintf(stderr, "%ld %9s %10p %5d %8s:%5d:%s\n", (long) log->when, 
		  nm_names[log->method], log->addr, log->length, 
		  log->reqfile, log->reqline, log->reqfunc);
     }
}

#endif /* NMALLOC */


/* As malloc(3) but with parameter checking and leak safeguards */
void *nm_nmalloc(size_t n, char *rfile, int rline, const char *rfunc) {
	char *q;

	if (n <= 0)
	     elog_die(FATAL, "n <= 0 at %s:%d:%s",
		      rfile, rline, rfunc);

	q = malloc(n);

#if NMALLOC
	if (nm_active)
	     nm_add(NM_NMALLOC, q, n, rfile, rline, rfunc);
#endif

	return q;
}

/* As nmalloc above, but will abort application if not enough memory */
void *nm_xnmalloc(size_t n, char *rfile, int rline, const char *rfunc) {
	char *q;

	if (n <= 0)
	     elog_die(FATAL, "n <= 0 at %s:%d:%s",
		      rfile, rline, rfunc);

	q = malloc(n);
	if (q == NULL)
	     elog_die(FATAL, "malloc failed (%d) at %s:%d:%s",
		      n, rfile, rline, rfunc);;

#if NMALLOC
	if (nm_active)
	     nm_add(NM_XNMALLOC, q, n, rfile, rline, rfunc);
#endif

	return q;
}

/* As realloc(3) but with parameter checking and leak safeguards */
void *nm_nrealloc(void *p, size_t n, char *rfile, int rline, 
		  const char *rfunc) {
	char *q;

	if (n <= 0)
	     elog_die(FATAL, "n <= 0 at %s:%d:%s",
		      rfile, rline, rfunc);

	if (p == NULL) {
		q = malloc(n);
	} else {
#if NMALLOC
	     if (nm_active)
		  nm_rm(NM_NREALLOC, p, rfile, rline, rfunc);
#endif
	     q = realloc(p, n);
	}

#if NMALLOC
	if (nm_active)
	     nm_add(NM_NREALLOC, q, n, rfile, rline, rfunc);
#endif

	return q;
}

/* As nrealloc above, but will abort application if not enough memory */
void *nm_xnrealloc(void *p, size_t n, char *rfile, int rline, 
		   const char *rfunc) {
	char *q;

	if (n <= 0)
	     elog_die(FATAL, "n <= 0 at %s:%d:%s",
		      rfile, rline, rfunc);

	if (p == NULL) {
		q = malloc(n);
	} else {
#if NMALLOC
	     if (nm_active)
		  nm_rm(NM_NREALLOC, p, rfile, rline, rfunc);
#endif
	     q = realloc(p, n);
	}
	if (q == NULL)
	     elog_die(FATAL, 
		      "realloc failed (%d -> %d) at %s:%d:%s",
		      p, n, rfile, rline, rfunc);

#if NMALLOC
	if (nm_active)
	     nm_add(NM_XNREALLOC, q, n, rfile, rline, rfunc);
#endif

	return q;
}

/* As free(3) but with parameter checking and leak safeguards */
void nm_nfree(void *p, char *rfile, int rline, const char *rfunc) {
	if (p == NULL)
	     return;

#if NMALLOC
	if (nm_active)
	     nm_rm(NM_NFREE, p, rfile, rline, rfunc);
#endif

	free(p);
}

/* As nfree above */
void nm_xnfree(void *p, char *rfile, int rline, const char *rfunc) {
     if (p == NULL)
          return;

#if NMALLOC
     if (nm_active)
	  nm_rm(NM_XNFREE, p, rfile, rline, rfunc);
#endif

     free(p);
}

/* As strdup(3) but with parameter checking and leak safeguards */
char *nm_nstrdup(const char *s, char *rfile, int rline, const char *rfunc) {
	char *p;

	if (s == NULL)
	     elog_die(FATAL, "s == NULL at %s:%d:%s",
		      rfile, rline, rfunc);

	p = strdup(s);
#if NMALLOC
	if (nm_active)
	     nm_add(NM_NSTRDUP, p, strlen(s)+1, rfile, rline, rfunc);
#endif
	return p;
}

/* As strdup(3) but with a maximum number of characters,  parameter checking
 * and leak safeguards. The string will always be terminated */
char *nm_nstrndup(const char *s, size_t max, char *rfile, int rline, 
		  const char *rfunc) {
	char *p;

	if (s == NULL)
	     elog_die(FATAL, "s == NULL at %s:%d:%s",
		      rfile, rline, rfunc);

	p = nmalloc(max);
	strncpy(p, s, max);

	return p;
}

/* As nstrndup above, but will abort application if not enough memory */
char *nm_xnstrndup(const char *s, size_t max, char *rfile, int rline, 
		   const char *rfunc) {
	char *p;

	if (s == NULL)
	     elog_die(FATAL, "s == NULL at %s:%d:%s",
		      rfile, rline, rfunc);

	p = xnmalloc(max);
	strncpy(p, s, max);

	return p;
}

/* As nstrdup above, but will abort application if not enough memory */
char *nm_xnstrdup(const char *s, char *rfile, int rline, const char *rfunc) {
	char *p;

	if (s == NULL)
	     elog_die(FATAL, "s == NULL at %s:%d:%s",
		      rfile, rline, rfunc);

	p = strdup(s);
	if (p == NULL)
	     elog_die(FATAL, "strdup failed at %s:%d:%s",
		      rfile, rline, rfunc);

#if NMALLOC
	if (nm_active)
	     nm_add(NM_XNSTRDUP, p, strlen(s)+1, rfile, rline, rfunc);
#endif
	return p;
}

/* As memdup(3) but with parameter checking and leak safeguards */
void *nm_nmemdup(const void *p, size_t n, char *rfile, int rline, 
		 const char *rfunc) {
	void *q;

	if (p == NULL)
	     elog_die(FATAL, "p == NULL at %s:%d:%s",
		      rfile, rline, rfunc);

	if (n <= 0)
	     elog_die(FATAL, "n <= 0 at %s:%d:%s",
		      rfile, rline, rfunc);

	q = malloc(n);
	if (q != NULL)
		memcpy(q, p, n);
#if NMALLOC
	if (nm_active)
	     nm_add(NM_NMEMDUP, q, n, rfile, rline, rfunc);
#endif
	return q;
}

/* As nmemdup above, but will abort application if not enough memory */
void *nm_xnmemdup(const void *p, size_t n, char *rfile, int rline, 
		  const char *rfunc) {
	char *q;

	if (p == NULL)
	     elog_die(FATAL, "p == NULL at %s:%d:%s",
		      rfile, rline, rfunc);
	if (n <= 0)
	     elog_die(FATAL, "n <= 0 at %s:%d:%s",
		      rfile, rline, rfunc);

	q = malloc(n);
	if (q == NULL)
	     elog_die(FATAL, "malloc failed");
	else
		memcpy(q, p, n);
#if NMALLOC
	if (nm_active)
	     nm_add(NM_XNMEMDUP, q, n, rfile, rline, rfunc);
#endif
	return q;
}

/* memdup, non adopting memory duplication  */
void *nm_memdup(const void *p, size_t n, char *rfile, int rline, 
		const char *rfunc) {
	void *q;

	if (p == NULL)
	     elog_die(FATAL, "p == NULL at %s:%d:%s",
		      rfile, rline, rfunc);

	if (n <= 0)
	     elog_die(FATAL, "n <= 0 at %s:%d:%s",
		      rfile, rline, rfunc);

	q = malloc(n);
	if (q != NULL)
		memcpy(q, p, n);

	return q;
}

/* As memdup above, non adopting memory duplication.
 * Aborts application if not enough memory */
void *nm_xmemdup(const void *p, size_t n, char *rfile, int rline, 
		 const  char *rfunc) {
	char *q;

	if (p == NULL)
	     elog_die(FATAL, "p == NULL at %s:%d:%s",
		      rfile, rline, rfunc);
	if (n <= 0)
	     elog_die(FATAL, "n <= 0 at %s:%d:%s",
		      rfile, rline, rfunc);

	q = malloc(n);
	if (q == NULL)
	     elog_die(FATAL, "malloc failed");
	else
		memcpy(q, p, n);

	return q;
}

/* forget a location that may have been allocated in the past. 
 * If the location is not present, don't complain! */
void  nm_forget(void *p, char *rfile, int rline, const char *rfunc) {
#if NMALLOC
	if (p && nm_active)
	     nm_rm(NM_FORGET, p, rfile, rline, rfunc);
#endif
}

/* adopt malloc()'ed into the nm table as if it was our own */
void  nm_adopt(void *p, char *rfile, int rline, const char *rfunc) {
#if NMALLOC
	if (p && nm_active)
	     nm_add(NM_ADOPT, p, 0, rfile, rline, rfunc);
#endif
}
