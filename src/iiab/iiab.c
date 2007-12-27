/*
 * habitat application helpers - infrastructure in a box - iiab
 *
 * This class contains application initialisation and management methods
 * to help reduce the code in client applications and to ensure that
 * iiab applications work in a standard, predictable way.
 *
 * Nigel Stuckey, July 1999
 * Copyright System Garden Limited 1999-2004. All rights reserved
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <pwd.h>
#include "tree.h"
#include "itree.h"
#include "table.h"
#include "nmalloc.h"
#include "route.h"
#include "elog.h"
#include "cf.h"
#include "iiab.h"
#include "util.h"
#include "callback.h"
#include "http.h"
#include "httpd.h"
#include "sig.h"
#include "rt_none.h"
#include "rt_file.h"
#include "rt_std.h"
#include "rt_http.h"
#include "rt_sqlrs.h"
#include "rt_rs.h"
/*#include "rt_store.h"*/

CF_VALS iiab_cf;		/* configuration parameters */
char   *iiab_cmdusage;		/* consoladated command line usage string */
char   *iiab_cmdopts;		/* consoladated command line options */
CF_VALS iiab_cmdarg;		/* command line arguments */
char   *iiab_havelock=NULL;	/* non-NULL if we have an exclusive lock */
char   *iiab_dir_etc=NULL;	/* config file directory */
char   *iiab_dir_bin=NULL;	/* executable directory */
char   *iiab_dir_lib=NULL;	/* library directory */
char   *iiab_dir_var=NULL;	/* data directory */
char   *iiab_dir_lock=NULL;	/* lock directory */
char   *iiab_std_bin_dirs[] = {"/usr/local/bin", "/bin", "/usr/bin", 
			       "/sbin", "/usr/sbin", NULL};

/*
 * Initialise standard routes, logging, configuration, interpret 
 * command line and change directory.
 *
 * A standard set of rules is followed to setup the correct
 * configuration from central places local files and the command line.
 * All the information is then available in the global CF_VALS: iiab_cf.
 * The command line config is available in iiab_cmdarg on its own and also
 * placed in iiab_cf.
 * There are some standard options implemented by this routine:-
 *     -c Configuration file (arg expected)
 *     -C Configuration option (arg expected)
 *     -d Diagnostic debug mode
 *     -D Developer debug mode
 *     -e 'off the shelf' error format (int arg expected 1-7)
 *     -h Help
 *     -v Version print and exit
 * Note:-
 * 1.  If the symbol defined by NM_CFNAME (nmalloc) is not defined or 
 *     set to 0, nmalloc leak checks are disabled
 */
void iiab_start(char *opts,	/* Command line option string as getopts(3) */
		int argc,	/* Number of arguments in command line */
		char **argv,	/* Array of argument strings */
		char *usage,	/* String describing usage */
		char *appcf	/* Application configuration string */ )
{
     int r;
     char iiablaunchdir[PATH_MAX];
     int elogfmt;

     if ( !(opts && usage) )
	  elog_die(FATAL, "opts or usage not set");

     /* save our launch directory & work out the standard places */
     getcwd(iiablaunchdir, PATH_MAX);
     iiab_dir_locations(argv[0]);

     /* consolidated command line options and usage */
     /* -- setup global variables -- */
     iiab_cmdopts  = util_strjoin(IIAB_DEFOPTS, opts, NULL);
     iiab_cmdusage = util_strjoin(IIAB_DEFUSAGE, usage, usage?"\n":"", 
				  IIAB_DEFWHERE, NULL);
     iiab_cmdarg   = cf_create();
     iiab_cf       = cf_create();

     /* initialise common IIAB classes: callbacks, error logging, 
      * i/o and old storage routes */
     callback_init();
     iiab_init_routes();     /* initialise new route mechanism */
     elog_init(1, argv[0], NULL);
     rs_init();

     /* collect command line arguments and place in special config list
      * which is use for generating the main config list
      */
     r = cf_cmd(iiab_cmdarg, iiab_cmdopts, argc, argv, iiab_cmdusage);
     if ( ! r ) {
          nm_deactivate();
	  elog_send(FATAL, "incorrect command line");
	  exit(1);
     }

     /* help! print out help before we do anything else and send it
      * to stderr as the user needs to read it */
     if (cf_defined(iiab_cmdarg, "h")) {
          nm_deactivate();
	  fprintf(stderr, "usage %s %s", 
		  cf_getstr(iiab_cmdarg,"argv0"), iiab_cmdusage);
	  exit(1);
     }

     /* load the configuration from the standard places, goverened by 
      * the command line. Also load the app's dir locations */
     iiab_cf_load(iiab_cf, iiab_cmdarg, iiab_cmdusage, appcf);
     iiab_dir_setcf(iiab_cf);

     /***********************************************
      * (d) Carry out common configuration actions
      * 1. configure event logging
      * 2. memory checking 
      * 3. version switch
      * 4. adapt logging for -d and -D special debug cases
      * 5. adapt logging format for -e
      * 6. final initialisation for subsystems needing configuration
      * 7. license checking: go/no-go
      ***********************************************/

     /* configure event logging */
     elog_configure(iiab_cf);

     /*
      * nmalloc deactivation check.
      * Deactivate nmalloc checking if NM_CFNAME is set to 0 in config
      */
     if ( !cf_defined(iiab_cf, NM_CFNAME) ||
	  cf_getint(iiab_cf, NM_CFNAME) == 0 )
	  nm_deactivate();

     /* command line switches (excluding -c and -C) */

     /* -v flag: version print and exit */
     if (cf_defined(iiab_cmdarg, "v")) {
	  fprintf(stderr, "Version of %s is %s\n", 
		  cf_getstr(iiab_cmdarg,"argv0"), VERSION);
	  exit(0);
     }
     /* if -d diag flag specified, override supplied event 
      * configuration and instead route DIAG and above to stderr */
     if (cf_defined(iiab_cmdarg, "d")) {
	  elog_setallpurl("none:");
	  elog_setabovepurl(DIAG, "stderr:");
	  elog_printf(DIAG, "event configuration overidden: diagnosis "
		      "to stderr");
     }
     /* -D debug flag: as above but route everything to stderr */
     if (cf_defined(iiab_cmdarg, "D")) {
	  elog_setabovepurl(DEBUG, "stderr:");
	  elog_printf(DEBUG, "event configuration overidden: debug "
		      "to stderr");
     }

     /* -e flag: use precanned elog formats */
     if (cf_defined(iiab_cmdarg, "e")) {
	  elogfmt = cf_getint(iiab_cmdarg, "e");
	  if (elogfmt < 0 || elogfmt > ELOG_MAXFMT)
	       elog_printf(ERROR, "standard error format out of range "
			   "(0-%d), using default", ELOG_MAXFMT);
	  else
	       elog_setallformat(elog_stdfmt[elogfmt]);
     }


     /*  diagnostics: config and dirs */
     cf_dump(iiab_cf);		/* dump config to diag */
     iiab_dir_dump();		/* dump dir locations to diag */

     /* final set of initialisations that require configurations */
     http_init();

#if 0
     /* (d6) self destruction due to snapshot time out, licenses etc */
     if ( ! cf_defined(iiab_cf, IIAB_LICNAME) )
	  if (time(NULL) > 993884454)	/* HACK ALERT, some time in jul 2001 */
	       elog_printf(FATAL, "This is development software "
			   "and almost certainly out of date.\n"
			   "You should arrange an update from system garden "
			   "by e-mailing help@systemgarden.com\n"
			   "or checking the web site "
			   "http://www.systemgarden.com.\n"
			   "This software will run unaffected in its current "
			   "form but you will be nagged from now on");
#endif

#if 0
     /* return to the launch dir */
     chdir(iiablaunchdir);
#endif
}



void iiab_stop()
{
     /* remove lock file if applicable */
     if (iiab_havelock) {
	  unlink(iiab_havelock);
	  nfree(iiab_havelock);
     }

     /* finalise and free iiab class data */
     nfree(iiab_cmdopts);
     nfree(iiab_cmdusage);
     cf_destroy(iiab_cmdarg);
     cf_destroy(iiab_cf);

     /* finalise co-operative clases co-ordinated by iiab */
     rs_fini();
     elog_fini();
     route_fini();
     callback_fini();
     iiab_free_dir_locations();
}



/*
 * Turn a normal running process into a detached daemon!!
 * Running this routine will fork the process into a different pid
 * parented by init, become part of a new session or process group,
 * change the default umask for created files and block off
 * tty signals.
 * It does not close file descriptors or redirect them.
 */
void iiab_daemonise()
{
     int i;

     if (getppid() == 1)
	  return;				/* already a daemon */
     i = fork();
     if (i < 0)
	  elog_die(FATAL, "unable to fork");	/* fork error */
     if (i > 0) {
	  iiab_stop();
	  _exit(0);				/* parent */
     }

     /* Child process (the novice daemon) continues */
     setsid();					/* obtain new process group */
#if 0
     for (i = getdtablesize(); i >= 0; --i)
	  close(i);				/* close all fds */
     i = open("/dev/null", O_RDWR);		/* open stdio on /dev/null */
     dup(i);
     dup(i);
#endif
     umask(022);				/* security esp root */
     sig_blocktty();
}



/*
 * Initialise nroute and register all the possible destinations.
 * Expect iiab_cf to be initialised and all the callbacks used in
 * the register to be available (eg rt_filea_method, etc)
 */
void iiab_init_routes() {
     route_init(iiab_cf, 0);
     route_register(&rt_none_method);
     route_register(&rt_filea_method);
     route_register(&rt_fileov_method);
     route_register(&rt_stdin_method);
     route_register(&rt_stdout_method);
     route_register(&rt_stderr_method);
     route_register(&rt_http_method);
     route_register(&rt_https_method);
     route_register(&rt_sqlrs_method);
#if 0
     route_register(&rt_storehol_method);
     route_register(&rt_storetime_method);
     route_register(&rt_storetab_method);
     route_register(&rt_storever_method);
#endif
     route_register(&rt_rs_method);
}


/*
 * Find the application's directory locations.
 * As iiab based apps can be tree based or linux location based 
 * (RPM standards), this routine finds where the important locations
 * are. Sets the following globals:-
 *   iiab_dir_etc - config directory
 *   iiab_dir_bin - executables
 *   iiab_dir_lib - library files
 *   iiab_dir_var - data files
 * Called by iiab_start(), this finction can also be called 
 * explicitly, before iiab_start(), to find the values of directories early.
 * when called for the second or subsequent time, the values will not 
 * be overwritten. To reread, iiab_free_dir_locations() should be called
 * before hand.
 */
void iiab_dir_locations(char *argv0)
{
     char cwd[PATH_MAX], **path, *eot;
     int stdplaces=0;

     if (iiab_dir_bin)		/* don't reinitialise */
	  return;

     /* get cwd as default */
     getcwd(cwd, PATH_MAX);

     /* find absolute location of executable */
     iiab_dir_bin = iiab_getbinpath(argv0);
     eot = strrchr(iiab_dir_bin, '/');	/* bin dir */
     if (eot)
	  *eot = '\0';			/* just dir part: chop at '/' */

     /* if executable is in a standard place (/usr/local/bin, /bin, /usr/bin,
      * /sbin, /usr/sbin) then standard locations are assumed for the
      * support directories */
     for (path=iiab_std_bin_dirs; *path; path++) {
	  if (strncmp(iiab_dir_bin, *path, strlen(*path)) == 0) {
	       stdplaces++;
	       break;
	  }
     }
     if (stdplaces) {
	  /*elog_printf(DIAG, "system installation detected: argv0 %s, %s", 
	    argv0, iiab_dir_bin);*/
	  iiab_dir_etc  = xnstrdup(IIAB_STD_DIR_ETC);
	  iiab_dir_lib  = xnstrdup(IIAB_STD_DIR_LIB);
	  iiab_dir_var  = xnstrdup(IIAB_STD_DIR_VAR);
	  iiab_dir_lock = xnstrdup(IIAB_STD_DIR_LOCK);
     } else {
	  /* elog_printf(DIAG, "standalone installation detected: "
	     "argv0 %s, %s", argv0, iiab_dir_bin);*/
	  iiab_dir_etc  = util_strjoin(iiab_dir_bin, "/../etc", NULL);
	  iiab_dir_lib  = util_strjoin(iiab_dir_bin, "/../lib", NULL);
	  iiab_dir_var  = util_strjoin(iiab_dir_bin, "/../var", NULL);
	  iiab_dir_lock = xnstrdup("/tmp");

	  /* check that the directories exist: if not, use the cwd */
	  if (access(iiab_dir_etc, R_OK)) {
	       nfree(iiab_dir_etc);
	       iiab_dir_etc = nstrdup(cwd);
	  }
	  if (access(iiab_dir_lib, R_OK)) {
	       nfree(iiab_dir_lib);
	       iiab_dir_lib = nstrdup(cwd);
	  }
	  if (access(iiab_dir_var, R_OK)) {
	       nfree(iiab_dir_var);
	       iiab_dir_var = nstrdup(cwd);
	  }
     }
}


void  iiab_dir_dump()
{
     elog_startsend(DIAG, "Dump of directory locations ---------");
     elog_contprintf(DIAG, "\nbin  = %s\n", iiab_dir_bin);
     elog_contprintf(DIAG, "var  = %s\n", iiab_dir_var);
     elog_contprintf(DIAG, "lib  = %s\n", iiab_dir_lib);
     elog_contprintf(DIAG, "etc  = %s\n", iiab_dir_etc);
     elog_contprintf(DIAG, "lock = %s\n", iiab_dir_lock);
     elog_endprintf(DIAG, "End of directory locations ----------");
}


/*
 * Takes the current settings for the application's directory locations
 * have saves them in the given config class
 * Sets the following globals:-
 *   iiab.dir.etc - config directory
 *   iiab.dir.bin - executables
 *   iiab.dir.lib - library files
 *   iiab.dir.var - data files
 */
void iiab_dir_setcf(CF_VALS cf		/* destination config */ )
{
     cf_addstr(cf, "iiab.dir.etc",  iiab_dir_etc);
     cf_addstr(cf, "iiab.dir.bin",  iiab_dir_bin);
     cf_addstr(cf, "iiab.dir.lib",  iiab_dir_lib);
     cf_addstr(cf, "iiab.dir.var",  iiab_dir_var);
     cf_addstr(cf, "iiab.dir.lock", iiab_dir_lock);
}


void iiab_free_dir_locations() 
{
     nfree(iiab_dir_bin);
     nfree(iiab_dir_etc);
     nfree(iiab_dir_lib);
     nfree(iiab_dir_var);
     nfree(iiab_dir_lock);
}


/* Load configuration values into 'cf' from every where.
 * The configuration is compiled from various sources (in order):-
 * 1. Application defaults
 * 2. The command line
 * 3. User configuration
 * 4. Configuration in distribution files
 * 5. Other standard config locations [like web or file servers]
 *    (which are usually boot strapped from (3) above)
 * Requires cf to have been set up and the route & elog mechanism 
 * to be working.
 */
void iiab_cf_load(CF_VALS cf,		/* destination config */
		  CF_VALS cmdarg,	/* command line switches */
		  char *cmdusage,	/* command usage text */
		  char *appcf		/* application default config*/ )
{
     char *usercf, *etccf;
     int r;

     /* check command line for user config file override */
     if (cf_defined(cmdarg, "c")) {
	  if ( ! route_access(cf_getstr(cmdarg, "c"), NULL, 0) )
	       elog_die(FATAL, 
			"configuration data does not exist (%s)\n%s: %s", 
			cf_getstr(cmdarg, "c"), cf_getstr(cmdarg,"argv0"), 
			iiab_cmdusage);
     } else  {
	  /* no override -- use default for user config */
	  usercf = util_strjoin(IIAB_CFUSERMETH, getenv("HOME"), "/", 
				IIAB_CFUSERFNAME, NULL);
	  cf_addstr(cf, "c", usercf);
	  nfree(usercf);
     }

     /* set up route names for multi level configuration in cf
      *   c         user file
      *   cfuser    user file (copy of c)
      *   cfetc     distribution config file (in iiab_dir_etc)
      *   cfsys     system config route
      *   cfregion  regional config route
      *   cfglobal  global config route
      */
     cf_addstr(cf, IIAB_CFUSERKEY, cf_getstr(cf, "c"));
     etccf = util_strjoin(IIAB_CFETCMETH, iiab_dir_etc, "/",
			  IIAB_CFETCFNAME, NULL);
     cf_addstr(cf, IIAB_CFETCKEY, etccf);
     nfree(etccf);
     /* no default for sys, region or global */


     /* Start reading in the actual values in order */

     /* 1. default app config */
     if (appcf) {
	  cf_scantext(cf, NULL, appcf, CF_CAPITULATE);
     }

     /* 2a. cmd line: switches as key */
     cf_defaultcf(cf, cmdarg);

     /* 2b. cmd line: -C contains key-value */
     if (cf_defined(cmdarg, "C")) {
	  if ( ! cf_scantext(cf,NULL,cf_getstr(cmdarg, "C"),CF_OVERWRITE) ) {
	       elog_die(FATAL, 
			"in-line configuration contains errors\n%s: %s", 
			cf_getstr(cmdarg,"argv0"), cmdusage);
	  }
     }

     /* 3. user configuration route, generally a file */
     if (route_access(cf_getstr(cf, IIAB_CFUSERKEY), NULL, ROUTE_READOK)) {
	  r = cf_scanroute(cf, IIAB_CFUSERMAGIC, 
			   cf_getstr(cf, IIAB_CFUSERKEY), CF_CAPITULATE);
	  if ( ! r )
	       elog_die(FATAL, "problem with user configuration file %s; "
			"can't continue",
			cf_getstr(cf, IIAB_CFUSERKEY));
     }

     /* 4. distribution config file, normally in /etc somewhere */
     if (route_access(cf_getstr(cf, IIAB_CFETCKEY), NULL, ROUTE_READOK)) {
	  r = cf_scanroute(cf, IIAB_CFETCMAGIC, 
			   cf_getstr(cf, IIAB_CFETCKEY), CF_CAPITULATE);
	  if ( ! r )
	       elog_die(FATAL, "problem with distribution configuration "
			"file %s; can't continue",
			cf_getstr(cf, IIAB_CFETCKEY));
     }

     /* 5. other standard config locations */
     /* to be complted */
}



/*
 * Routine to get the absolute path name of this binary.
 * Returns an nmalloc()ed string, which should be nfreed after use.
 */
char *iiab_getbinpath(char *argv0)
{
     char swd[PATH_MAX], cwd[PATH_MAX], *path, *binpath, *dir;
     struct stat statbuf;
     int len;

     /*cmd = getexecname();*/
     if (*argv0 == '/') {
	  /* a nice helpful absolute path */
	  binpath = xnstrdup( argv0 );
	  len = strlen(binpath);
	  util_strgsub(binpath, "//", "/", len+1);	/* making smaller */
	  util_strgsub(binpath, "/./", "/", len+1);	/* making smaller */
     } else {
	  /* unhelpfully, there is no absolute path given to us */

	  /* is it relative? (contains a dir path) */
	  if ((path = strchr(argv0, '/'))) {
	       /* relative path: change to the containging dir
		* and find its absoloute location */
	       dir = xnstrdup(argv0);
	       dir[argv0-path] = '\0';
	       getcwd(swd, PATH_MAX);
	       chdir(dir);
	       getcwd(cwd, PATH_MAX);
	       chdir(swd);
	       binpath = util_strjoin( cwd, "/", path+1, NULL );
	       nfree(dir);
	       len = strlen(binpath);
	       util_strgsub(binpath, "//", "/", len+1);
	       util_strgsub(binpath, "/./", "/", len+1);
	       return binpath;
	  }

	  /* So, its just a name then... lets see if its present 
	   * in any dir in $PATH */
          getcwd(cwd, PATH_MAX);
	  path = xnstrdup(getenv("PATH"));
	  dir = strtok(path, ":");
	  while (dir) {
	       if (dir[0] == '/') {
		    binpath = util_strjoin( dir, "/", argv0, NULL );
	       } else {
		    binpath = util_strjoin( cwd, "/", dir, "/", argv0, NULL );
	       }
	       len = strlen(binpath);
	       util_strgsub(binpath, "//", "/", len+1);
	       util_strgsub(binpath, "/./", "/", len+1);
	       if (access(binpath, X_OK) == 0) {
		    /* found one -- but is it a file or a dir? */
		    stat(binpath, &statbuf);
		    if (S_ISREG(statbuf.st_mode) &&
			! S_ISDIR(statbuf.st_mode))
		    {
			 nfree(path);
			 return binpath;
		    }
	       }
	       nfree(binpath);
	       dir = strtok(NULL, ":");
	  }
	  nfree(path);

	  /* getting a bit desperate now, so assume that it is based 
	   * from the current dir */
	  getcwd(cwd, PATH_MAX);
	  binpath = util_strjoin( cwd, "/", argv0, NULL );
     }


     return binpath;
}


/*
 * Test to see if an iiab program with the same key is running on the
 * same machine as this program. It does three things depending on what 
 * it finds :-
 * 1. If there is another then iiab_stop() is run to shutdown this 
 *    application, a message is printed to stderr and exit(1) is called.
 * 2. If the program has crashed (run file exists but no process) then the
 *    run file is updated and 1 is returned
 * 3. If the program is not running or crashed then our claim to run is 
 *    established and this method returns 0.
 * The key is registered if successful and the lock will be removed when
 * iiab_stop() is run.
 */
int    iiab_lockordie(char *key	/* identifier to make exclusive */ )
{
     char *keyfname;
     char rundetails[100], *tty, pwname[20], ttytxt[25], datestr[25];
     int fd, n, pid, ret;
     struct passwd *pw;

     keyfname = util_strjoin(iiab_dir_lock, "/", key, ".run", NULL);
     fd = open(keyfname, O_WRONLY | O_CREAT | O_EXCL, 
	       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
     if (fd == -1) {
	  /* another instance found, but is it still a running process? */
	  fd = open(keyfname, O_RDONLY);
	  if (fd != -1) {
	       n = read(fd, rundetails, 100);
	       close(fd);
	       *(rundetails + n) = '\0';
	       sscanf(rundetails, "%d %s %s %[^~]", &pid, pwname, ttytxt, 
		      datestr);
	       if (iiab_ispidrunning(pid)) {
		    /* another process is running as the file said */
		    fprintf(stderr,
	"Unable to start as another process has taken out a lock,\n"
	"preventing us from running. The details are:-\n"
	"  process id: %d\n"
	"  user name:  %s\n"
	"  terminal:   %s\n"
	"  started:    %s\n"
	"To stop the process, use the command 'killclock' as user '%s'.\n"
	"If 'clockwork' still does not start, the previous process can be\n"
	"stopped with the command `kill -9 %d' as user '%s' or root.\n"
	"If the process doesn't exist anymore but clockwork is unable to\n"
	"run, clear the lock with the command `rm %s'.\n",
			    pid, pwname, ttytxt, datestr, 
			    pwname, pid, pwname, keyfname);
		    iiab_stop();
		    nfree(keyfname);
		    exit(1);
	       } else {
		    /* the other process has crashed */
		    elog_printf(DIAG, "previous process crashed: "
				"key %s pid %d user %s term %s started %s", 
				key, pid, pwname, ttytxt, datestr);
		    ret=1;	/* ok, but previous crashed */
		    unlink(keyfname);
		    fd = open(keyfname, O_WRONLY | O_CREAT | O_EXCL, 
			      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		    goto oktorun;
	       }
	  } else {
	       /* don't have write permission on run file: stop. */
	       fprintf(stderr,
		       "Unable to create lock file, although no "
		       "running instance of clockwork can be found.");
	       iiab_stop();
	       nfree(keyfname);
	       exit(1);
	  }
     } else {
          /* no crash, normal start */
          ret = 0;
     }

 oktorun:
     /* exclusive use of key: we should be the only thing running on this
      * machine using the same key. write a summary of this instance to 
      * the newly acquired file */
     pw = getpwuid( getuid() );
     tty = ttyname(2);	/* stderr least likely to be redirected */
     if (tty == NULL)
	  tty = "daemon";
     n = snprintf(rundetails, 100, "%d %s %s %s\n", getpid(), pw->pw_name, 
		  tty, util_decdatetime(time(NULL)) );
     write(fd, rundetails, n);
     close(fd);

     /* set a flag to iiab_stop() to remove lock when shutting down.
      * It is set to the lock file */
     iiab_havelock = keyfname;

     return ret;
}



/*
 * Return the pid of the process responsible for taking out an iiab 
 * application lock using the given key. Also return the username, 
 * starting terminal and date using pass by reference. If any of these 
 * are NULL the routine will not be used. However, if an address is passed, 
 * the location will be set to a static address.
 */
int  iiab_getlockpid(char *key, char **pw, char **tty, char **date) {
     char *keyfname;
     char rundetails[100];
     static char pwname[20], ttytxt[25], datestr[25];
     int fd, n, pid;

     keyfname = util_strjoin(iiab_dir_lock, "/", key, ".run", NULL);
     fd = open(keyfname, O_RDONLY);
     if (fd == -1) {
	  return 0;	/* no lock */
     } else {
	  /* read lock file contents, format:-
	   *    <pid> <username> <locking tty> <time locked> */
	  n = read(fd, rundetails, 100);
	  close(fd);
	  *(rundetails + n) = '\0';
	  sscanf(rundetails, "%d %s %s %[^~\n]", &pid, pwname, ttytxt, 
		 datestr);
	  if (pw)
	       *pw = pwname;
	  if (tty)
	       *tty = ttytxt;
	  if (date)
	       *date = datestr;
	  return pid;	/* pid of locking process */
     }
}


/*
 * Check to see if the pid is a running process.
 * Returns 1 for yes, 0 for no
 */
int  iiab_ispidrunning(int pid) {
     /*
      * this code uses the /proc filesystem and works on linuc and solaris only 
      */
     char fname[20];

     sprintf(fname, "/proc/%d", pid);
     if (access(fname, F_OK) == -1)
          return 0;
     else
          return 1;
}



/*
 * Check to see if the options are all present on a command line,  
 * returning 1 for yes, 0 for no.
 */
int   iiab_iscmdopt(char *opts, int argc, char **argv)
{
     int i, found=0;

     opterr = 0;	/* prevent error messages */
     optind = 1;	/* reset index, in case getopt has been run before */
     /* Process switches */
     while ((i = getopt(argc, argv, opts)) != EOF) {
	  switch (i) {
	  case ':':
	       found++;
	       elog_printf(WARNING, "missing option for switch %c", optopt);
	       break;
	  case '?':
	       /* found nothing */
	       break;
	  default:
	       /* found an option */
	       found++;
	       break;
	  }
     }
     opterr++;			/* restore original behaviour */
     if (found == strlen(opts))
          return 1;
     else
          return 0;
}



/* Save or update the attribute identified by key in the user 
 * configuration file. Find this file and the attribute from cf
 * Returns the number of characters read or -1 fo error */
int  iiab_usercfsave(CF_VALS cf, char *key)
{
     char *purl;
     int r;

     /* find the user configuration p-url from cf */
     if ( ! cf_defined(cf, IIAB_CFUSERKEY) )
	  return -1;

     purl = cf_getstr(cf, IIAB_CFUSERKEY);
     r = cf_updateline(cf, key, purl, IIAB_CFUSERMAGIC);
     return r;
}





#if TEST

char *test_cf = "nmalloc -1\n"
                "\"question 1\" \"answer 1\"\n"
                "\"question 2\" \"answer 2\"";

int main(int argc, char **argv)
{
     TABLE tab1;
     char *buf1;

     iiab_start("", argc, argv, "", test_cf);
     tab1 = cf_getstatus(iiab_cf);
     buf1 = table_print(tab1);
     printf("%s\n", buf1);
     nfree(buf1);
     table_destroy(tab1);
     iiab_stop();

     exit(0);
}

#endif
