/*
 * Habitat Gtk GUI implementation
 *
 * Nigel Stuckey, November 1999
 * Copyright System Garden Limited 1999-2001. All rights reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include "misc.h"
#include "../iiab/iiab.h"
#include "../iiab/util.h"
#include "../iiab/nmalloc.h"

/*
 * If clockwork is running on this machine, this routine will return 
 * its PID, which will be above 0.
 * If clockwork is not running, 0 will be returned.
 * Warning, this may be fooled if clockwork has died and left its
 * marker file behind in /tmp.
 */
int is_clockwork_running(char **key, char **user, char **tty, char **datestr)
{
  if (key)
       *key = "clockwork";
  return iiab_getlockpid("clockwork", user, tty, datestr);
}

/* check if clockwork is availble to run. Returns 1 for yes, 0 for no */
int is_clockwork_runable()
{
     int noclockwork;
     char *path;

     path = util_strjoin(iiab_dir_bin, "/clockwork", NULL);
     elog_printf(INFO, "looking for %s to collect local data", path);
     noclockwork = access(path, R_OK | X_OK);
     nfree(path);
     if (noclockwork)
	  return 0;
     else
	  return 1;
}

