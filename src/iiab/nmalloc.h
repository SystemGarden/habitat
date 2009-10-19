/*
 * Nigel's malloc
 * This is a memory tracing malloc and friends to help in fixing
 * memory leaks
 * Based on the public alloc routes.
 *
 * Nigel Stuckey, March 99
 * Copyright System Garden Limited 1999-2001. All rights reserved.
 */

#ifndef _NMALLOC_H_
#define _NMALLOC_H_

#include <stddef.h>		/* need size_t */

#if NMALLOC

#include <time.h>
#include "ptree.h"

/* this code implements the definitions for leak checking in nmalloc */
enum nm_origin {
	NM_NMALLOC,	/* orgin is nmalloc() */ 
	NM_XNMALLOC, 	/* orgin is xnmalloc() */ 
	NM_NREALLOC,	/* orgin is nrealloc() */ 
	NM_XNREALLOC,	/* orgin is xnrealloc() */ 
	NM_NFREE,	/* orgin is nfree() */ 
	NM_XNFREE,	/* orgin is xnfree() */ 
	NM_NSTRDUP,	/* orgin is nstrdup() */ 
	NM_XNSTRDUP,	/* orgin is xnstrdup() */ 
	NM_NMEMDUP,	/* orgin is nmemdup() */ 
	NM_XNMEMDUP,	/* orgin is xnmemdup() */ 
	NM_FORGET,	/* orgin is anonymous and memory is to be fogotten */ 
	NM_ADOPT	/* orgin is anonymous and memory is to be adopted */ 
};

#define NM_CFNAME "nmalloc"

struct nm_userec {
     time_t when;
     enum nm_origin method;
     void *addr;
     size_t length;
     char *reqfile;
     int reqline;
     char *reqfunc;
};

PTREE *ptree_createnocheck();
void nm_deactivate();
void nm_add(enum nm_origin meth, void *aloc, size_t sz, char *rfile, 
	    int rline, const char *rfunc);
void nm_rm(enum nm_origin meth, void *aloc, char *rfile, int rline, 
	   const char *rfunc);
int  nm_isalloc(void *aloc);
void nm_rpt();
#endif /* NMALLOC */

/* underlying function prototypes */
void *nm_nmalloc(size_t, char *rfile, int rline, const char *rfunc);
void *nm_xnmalloc(size_t, char *rfile, int rline, const char *rfunc);
void *nm_nrealloc(void *, size_t, char *rfile, int rline, const char *rfunc);
void *nm_xnrealloc(void *, size_t, char *rfile, int rline, const char *rfunc);
void  nm_nfree(void *, char *rfile, int rline, const char *rfunc);
void  nm_xnfree(void *, char *rfile, int rline, const char *rfunc);
char *nm_nstrdup(const char *, char *rfile, int rline, const char *rfunc);
char *nm_xnstrdup(const char *, char *rfile, int rline, const char *rfunc);
char *nm_nstrndup(const char *s, size_t max, char *rfile, int rline, 
		  const char *rfunc);
char *nm_xnstrndup(const char *s, size_t max, char *rfile, int rline, 
		   const char *rfunc);
void *nm_nmemdup(const void *, size_t, char *rfile, int rline, 
		 const char *rfunc);
void *nm_xnmemdup(const void *, size_t, char *rfile, int rline, 
		  const char *rfunc);
void *nm_memdup(const void *, size_t, char *rfile, int rline, 
		const char *rfunc);
void *nm_xmemdup(const void *, size_t, char *rfile, int rline, 
		 const char *rfunc);
void  nm_forget(void *, char *rfile, int rline, const char *rfunc);
void  nm_adopt(void *, char *rfile, int rline, const char *rfunc);

/* macro prototypes */
void *nmalloc(size_t);
void *xnmalloc(size_t);
void *nrealloc(void *, size_t);
void *xnrealloc(void *, size_t);
void  nfree(void *);
void  xnfree(void *);
char *nstrdup(const char *);
char *xnstrdup(const char *);
char *nstrndup(const char *, size_t);
char *xnstrndup(const char *, size_t);
void *nmemdup(const void *, size_t);
void *xnmemdup(const void *, size_t);
void *memdup(const void *, size_t);
void *xmemdup(const void *, size_t);
void  nforget(void *);
void  nadopt(void *);

/* macros themselves from interface to underlying function */
#define nmalloc(a)	nm_nmalloc(a,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define xnmalloc(a)	nm_xnmalloc(a,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define nrealloc(a,b)	nm_nrealloc(a,b,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define xnrealloc(a,b)	nm_xnrealloc(a,b,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define nfree(a)	nm_nfree(a,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define xnfree(a)	nm_xnfree(a,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define nstrdup(a)	nm_nstrdup(a,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define xnstrdup(a)	nm_xnstrdup(a,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define nstrndup(a,b)	nm_nstrndup(a,b,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define xnstrndup(a,b)	nm_xnstrndup(a,b,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define nmemdup(a,b)	nm_nmemdup(a,b,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define xnmemdup(a,b)	nm_xnmemdup(a,b,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define memdup(a,b)	nm_memdup(a,b,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define xmemdup(a,b)	nm_xmemdup(a,b,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define nforget(a)	nm_forget(a,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define nadopt(a)	nm_adopt(a,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#endif /* _NMALLOC_H_ */
