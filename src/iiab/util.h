/*
 * General purpose utilities
 *
 * Nigel Stuckey, May 1998
 * Copyright System Garden Limited 1998-2001. All rights reserved
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include "itree.h"
#include "route.h"

#define UTIL_SHORTSTR    80
#define UTIL_U32STRLEN   12
#define UTIL_U64STRLEN   25
#define UTIL_FLOATSTRLEN 16
#define UTIL_NANOSTRLEN  50
#define UTIL_ESCSTRLEN   8192
#define UTIL_BLANKREPSTR "-"
#define UTIL_ESCQUOTES   "\"'\1\2"
#define UTIL_HOSTLEN     100
#define UTIL_DOMAINLEN   100
#define UTIL_FQHOSTLEN   100
#define UTIL_SINGLESEP   0
#define UTIL_MULTISEP    1

#define util_abs(x) ((x) >= 0 ? (x) : -(x))
#define util_dabs(x) (doublereal)abs(x)
#define util_min(a,b) ((a) <= (b) ? (a) : (b))
#define util_max(a,b) ((a) >= (b) ? (a) : (b))
#define util_dmin(a,b) (doublereal)min(a,b)
#define util_dmax(a,b) (doublereal)max(a,b)
#define util_bit_test(a,b)   ((a) >> (b) & 1)
#define util_bit_clear(a,b)  ((a) & ~((uinteger)1 << (b)))
#define util_bit_set(a,b)    ((a) |  ((uinteger)1 << (b)))

struct util_scanbuf {
     ITREE *lines;
     char *buffer;
};
typedef struct util_scanbuf * SCANBUF;


#if 1
int    util_parseroute(char *route, char *sep, char *magic, ITREE **retbuf);
int    util_parsetext(char *buffer, char *sep, char *magic, ITREE **retbuf);
void   util_freeparse(ITREE *lol);
void   util_freeparse_leavedata(ITREE *lol);
void   util_parsedump(ITREE *buffer);
#endif
int    util_scancftext(char *buffer, char *sep, char *magic, ITREE **retindex);
int    util_scantext(char *buffer, char *sep, int multisep, ITREE **retindex);
int    util_scancfroute(char *route, char *sep, char *magic, ITREE **retindex, 
			char **retbuffer);
void   util_scanfree(ITREE *index);
void   util_scandump(ITREE *index);
char  *util_escapestr(char *s);
char  *util_strtoken(char *s);
char  *util_quotestr(char *s, char *escape, char *buf, int buflen);
char  *util_mquotestr(char *s);
char  *util_bintostr(size_t max, const void *binblock, size_t n);
int    util_strtobin(void *binblock, const char *str, size_t max);
char  *util_strdel(char *s, size_t n);
char  *util_strrtrim(char *s);
char  *util_strltrim(char *s);
char  *util_strtrim(char *s);
int    util_strgsub(char *str, const char *pat, const char *sub, size_t max);
char  *util_strsub(char *str, const char *pat, const char *sub);
char  *util_strjoin(const char *s, ...);
char  *util_i32toa(long src);
char  *util_u32toa(unsigned long src);
char  *util_i64toa(long long src);
char  *util_u64toa(unsigned long long src);
char  *util_ftoa(float src);
char  *util_u32toaoct(unsigned long src);
#if __svr4__
  char  *util_hrttoa(hrtime_t src);
  char  *util_tstoa(timestruc_t *src);
#else
  char  *util_tstoa(struct timespec *src);
#endif /* __svr4__ */
char  *util_jiffytoa(unsigned long long jiffies);
char  *util_decdatetime(time_t t);
char  *util_shortadaptdatetime(time_t t);
char  *util_basename(char *path);
char  *util_nonull(char *str);
int    util_filecopy(char *src, char *dst);
int    util_is_str_printable(char *str);
int    util_is_str_whitespace(char *str);
char * util_whichdir(char *file, char *dirlst);
int    util_b64_encode(unsigned char* ptr, int len, char* space, int size);
int    util_b64_decode(const char* str, unsigned char* space, int size);
void   util_strencode(char *to, int tosize, char *from);
void   util_strdecode(char* to, char* from);
int    util_hexit(char c);
char * util_strtok_sc(char *str, char *s);
char * util_hostname();
char * util_domainname();
char * util_fqhostname();
void   util_html2text(char *);

#endif /* _UTIL_H_ */
