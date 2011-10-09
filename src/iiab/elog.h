/*
 * Event log class (elog)
 *
 * Implements a event logging mechanism on top of the route class.
 * All events are classified by their origin and severity.
 * Origin is set by the programmer and severity may be one of five levels:
 * debug, info, warning, error and fatal. Each level or groups of levels may 
 * directed to different places as allowed by the route class.
 * Elog also has the ability of overriding the programmed severity levels
 * by use of patterns. Thus events may be upgraded or down graded in severity.
 * Finally, the setting of severity routes and overrinding patterns may be
 * specified in a file and parsed by evelog, thus making the behaviour
 * of a program externally configurable.
 *
 * Nigel Stuckey, May 1998. 
 * Modified Jan 2000 to add the DIAG severity, 
 * Copyright System Garden Limited 1998-2001. All rights reserved
 */

#ifndef _ELOG_H_
#define _ELOG_H_

#include "cf.h"
#include "route.h"
#include "table.h"

#define ELOG_NSEVERITIES 7
#define ELOG_STRLEN 4096
#define ELOG_KEEPDEF 100
#define ELOG_CFPREFIX "elog"
#define ELOG_FMT1 "decdt: %s, unixdt: %s, shortdt: %s, epochtime: %d, sev: %s, sevl: %c, sevu: %c, shtpname: %s, lngpname: %s, pid: %d, tid: %d, file: %s, fn: %s, ln: %d, origin: %s, code: %d, text: %s"
#define ELOG_FMT2 "%7$c: %17$s"
#define ELOG_FMT3 "%5$s: %17$s"
#define ELOG_FMT4 "%5$7s %17$s (%12$s:%13$s:%14$d)"
#define ELOG_FMT5 "-%7$c- %3$s %8$s %12$s:%13$s:%14$d %17$s"
#define ELOG_FMT6 "%s %5$s %9$s %10$d %12$s:%13$s:s%14$d %15$s %16$d %17$s"
#define ELOG_FMT7 "\"%2$s\" %4$d %5$s \"%9$s\" %10$d %11$d \"%12$s\" %13$s %14$d \"%15$s\" %16$d \"%17$s\""
#define ELOG_FMT8 "%5$7s %12$-11s %14$4d %13$-18s %17$s"
#define ELOG_DEFFORMAT ELOG_FMT4
#define ELOG_MAXFMT 8

extern char *elog_stdfmt[];

/* Severity levels */
enum elog_severity {
     NOELOG=0,	/* no logging, used for checking error states */
     DEBUG,	/* debugging logs to aid the developers */
     DIAG,	/* diagnostic logs to aid support when deployed */
     INFO,	/* information logs to help the user */
     WARNING,	/* warnings for the user */
     ERROR,	/* errors for the user and support */
     FATAL	/* catastrophic errors causing the app to fail */
};

/* open routes, indexed by severity
 * if purl is NULL, ROUTE was passed directly and should not be freed
 * when finished with */
struct elog_destination {
     char *purl;
     ROUTE route;
     char *format;
} elog_opendest[ELOG_NSEVERITIES];

int elog_init(int debug, char *binname, CF_VALS cf);
void elog_checkinit();
void elog_fini();
void elog_setorigin(char *origin);
int elog_setallroutes(ROUTE route);
int elog_setallpurl(char *purl);
int elog_setsevroute(enum elog_severity severity, ROUTE route);
int elog_setsevpurl(enum elog_severity severity, char *purl);
int elog_setbelowroute(enum elog_severity severity, ROUTE route);
int elog_setbelowpurl(enum elog_severity severity, char *purl);
int elog_setaboveroute(enum elog_severity severity, ROUTE route);
int elog_setabovepurl(enum elog_severity severity, char *purl);
int elog_setformat(enum elog_severity, char *format);
int elog_setallformat(char *format);
int elog_setoverride(enum elog_severity severity, char *pattern);
int elog_rmoverride(char *pattern);
void  elog_configure(CF_VALS cf);
enum  elog_severity elog_lettertosev(char sevletter);
enum  elog_severity elog_strtosev(char *sevstring);
char *elog_sevtostr(enum elog_severity sev);
TABLE elog_getstatus();
ROUTE elog_getroute(enum elog_severity sev);
char *elog_getpurl(enum elog_severity sev);

/* public log raising functions */
int  elog_startsend(enum elog_severity severity, char *logtext);
void elog_contsend(enum elog_severity severity, char *logtext);
void elog_endsend(enum elog_severity severity, char *logtext);
int  elog_send(enum elog_severity severity, char *logtext);
int  elog_startprintf(enum elog_severity severity, const char *format, ...);
void elog_contprintf(enum elog_severity severity, const char *format, ...);
void elog_endprintf(enum elog_severity severity, const char *format, ...);
int  elog_printf(enum elog_severity severity, const char *format, ...);
void elog_die(enum elog_severity severity, const char *format, ...);
void elog_safeprintf(enum elog_severity severity, const char *format, ...);

/* underlying log raising functions */
int  elog_fstartsend(enum elog_severity severity, char *file, int line, 
		     const char *function, char *logtext);
void elog_fcontsend(enum elog_severity severity, char *logtext);
void elog_fendsend(enum elog_severity severity, char *logtext);
int  elog_fsend(enum elog_severity severity, char *file, int line, 
		const char *function, char *logtext);
int  elog_fstartprintf(enum elog_severity severity, 
		       char *file, int line, const char *function, 
		       const char *format, ...);
void elog_fcontprintf(enum elog_severity severity, const char *format, ...);
void elog_fendprintf(enum elog_severity severity, const char *format, ...);
int  elog_fprintf(enum elog_severity severity, char *file, int line, 
		  const char *function, const char *format, 
		  ...);
void elog_fdie(enum elog_severity severity, char *file, int line, 
	       const char *function, const char *format, ...);
void elog_fsafeprintf(enum elog_severity severity, char *file, int line, 
		      const char *function, const char *format, ...);

/* public calling points: macros to embed more information
 * WARNING: this is a gnu extension */
#define elog_startsend(s,t) \
	  elog_fstartsend(s,__FILE__,__LINE__,__PRETTY_FUNCTION__,t)
#define elog_contsend(s,t) elog_fcontsend(s,t)
#define elog_endsend(s,t) elog_fendsend(s,t)
#define elog_send(s,t) \
	  elog_fsend(s,__FILE__,__LINE__,__PRETTY_FUNCTION__,t)
#define elog_startprintf(s,f,r...) \
	  elog_fstartprintf(s,__FILE__,__LINE__,__PRETTY_FUNCTION__,f,## r)
#define elog_contprintf(s,f,r...) elog_fcontprintf(s,f,## r)
#define elog_endprintf(s,f,r...) elog_fendprintf(s,f,## r)
#define elog_printf(s,f,r...) \
	  elog_fprintf(s,__FILE__,__LINE__,__PRETTY_FUNCTION__,f,## r)
#define elog_die(s,f,r...) \
	  elog_fdie(s,__FILE__,__LINE__,__PRETTY_FUNCTION__,f,## r)
#define elog_safeprintf(s,f,r...) \
	  elog_fsafeprintf(s,__FILE__,__LINE__,__PRETTY_FUNCTION__,f,## r)

#endif /* _ELOG_H_ */
