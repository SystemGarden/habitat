
/*
 * iiab application helpers
 *
 * This class contains application initialisation and management methods
 * to help reduce the code in client applications and to ensure that
 * iiab applications work in a standard, predictable way.
 *
 * Nigel Stuckey, July 1999
 * Copyright System Garden Limitied 1999, All rights reserved
 */

#ifndef _IIAB_H_
#define _IIAB_H_

#include "tree.h"
#include "itree.h"
#include "table.h"
#include "route.h"
#include "elog.h"
#include "cf.h"

#define IIAB_STD_DIR_ETC  "/etc"
#define IIAB_STD_DIR_LIB  "/usr/lib/habitat"
#define IIAB_STD_DIR_VAR  "/var/lib/habitat"
#define IIAB_STD_DIR_LOCK "/var/lib/habitat"
#define IIAB_DEFERRPURL	"stderr"
#define IIAB_CFNAME	"hab."
#define IIAB_LICNAME    IIAB_CFNAME "lic"
#define IIAB_HOST       IIAB_CFNAME "hostname"
#define IIAB_DOMAIN     IIAB_CFNAME "domainname"
#define IIAB_FQHOST     IIAB_CFNAME "fqhostname"
#define IIAB_HOSTLEN    100
#define IIAB_DEFOPTS	":c:C:dDe:hv"
#define IIAB_DEFUSAGE   "[-c <purl>] [-C <cfcmd>] [-e <fmt>] [-dDhv] "
#define IIAB_DEFWHERE \
"      -c <purl>   configuration route\n" \
"      -C <cfcmd>  configuration directive in-line\n" \
"      -d          diagnostic debug messages\n" \
"      -D          developer debug messages (expert use)\n" \
"      -e <fmt>    log using a predefined format <fmt>=[0-7]\n" \
"      -h          help\n" \
"      -v          print version and exit\n"
#define IIAB_CFUSERKEY    IIAB_CFNAME "cfuser"
#define IIAB_CFETCKEY     IIAB_CFNAME "cfetc"
#define IIAB_CFSYSKEY     IIAB_CFNAME "cfsys"
#define IIAB_CFREGIONKEY  IIAB_CFNAME "cfregion"
#define IIAB_CFGLOBALKEY  IIAB_CFNAME "cfglobal"
#define IIAB_CFUSERFNAME  ".habrc"
#define IIAB_CFUSERMETH   "fileov:"
#define IIAB_CFUSERMAGIC  "habitat 1"
#define IIAB_CFETCFNAME   "habitat.conf"
#define IIAB_CFETCMETH    "fileov:"
#define IIAB_CFETCMAGIC   "habitat 1"

extern CF_VALS iiab_cf;		/* configuration parameters */
extern char   *iiab_cmdusage;	/* consolidated command line usage string */
extern char   *iiab_cmdopts;	/* consolidated command line options */
extern CF_VALS iiab_cmdarg;	/* command line arguments */
extern char   *iiab_dir_etc;	/* config file directory */
extern char   *iiab_dir_bin;	/* executable directory */
extern char   *iiab_dir_lib;	/* library directory */
extern char   *iiab_dir_var;	/* data directory */
extern char   *iiab_dir_lock;	/* lock directory */

void  iiab_start(char *opts, int argc, char **argv, char *usage, char *appcf);
void  iiab_stop();
void  iiab_daemonise();
void  iiab_init_routes();
void  iiab_dir_locations(char *argv0);
void  iiab_dir_dump();
void  iiab_dir_setcf(CF_VALS cf);
void  iiab_free_dir_locations();
void  iiab_cf_load(CF_VALS cf, CF_VALS cmdarg, char *cmdusage, char *appcf);
char *iiab_getbinpath(char *argv0);
void  iiab_cdtoiiabdir(char *argv0);
int   iiab_lockordie(char *key);
int   iiab_getlockpid(char *key, char **pw, char **tty, char **date);
int   iiab_ispidrunning(int pid);
int   iiab_iscmdopt(char *opts, int argc, char **argv);
int   iiab_usercfsave(CF_VALS cf, char *key);

#endif /* _IIAB_H_ */
