/*
 * Mac OS X process probe for iiab
 * Nigel Stuckey, March 2010
 *
 * Copyright System Garden Ltd 2010. All rights reserved.
 */

#if __APPLE_CC__DONTWORK

#include <stdio.h>
#include "probe.h"

/* Linux specific includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

TREE *pmacnames_sysfiles = NULL;

/* table constants for system probe */
struct probe_sampletab pmacnames_cols[] = {
     {"name",	"", "str",  "abs", "", "1", "name"},
     {"vname",	"", "str",  "abs", "", "", "value name"},
     {"value",  "", "str",  "abs", "", "", "value"},
     PROBE_ENDSAMPLE
};

struct probe_rowdiff pmacnames_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *pmacnames_getcols()    {return pmacnames_cols;}
struct probe_rowdiff   *pmacnames_getrowdiff() {return pmacnames_diffs;}
char                  **pmacnames_getpub()     {return NULL;}

/*
 * Initialise probe for linux names information
 */
void pmacnames_init() {
}

/*
 * Linux specific routines
 */
void pmacnames_collect(TABLE tab) {
     char *basename;

     pmacnames_sysfiles = tree_create();
     pmacnames_readalldir("/proc/sys", pmacnames_sysfiles);
     tree_traverse(pmacnames_sysfiles) {
	  table_addemptyrow(tab);
	  table_replacecurrentcell(tab, "value", 
				   tree_get(pmacnames_sysfiles));
	  table_replacecurrentcell(tab, "name", 
				   tree_getkey(pmacnames_sysfiles));
	  basename = strrchr(tree_getkey(pmacnames_sysfiles), '/');
	  if (basename)
	       basename++;
	  else
	       basename = tree_getkey(pmacnames_sysfiles);
	  table_replacecurrentcell(tab, "vname", basename);
	  table_freeondestroy(tab, tree_getkey(pmacnames_sysfiles));
	  table_freeondestroy(tab, tree_get(pmacnames_sysfiles));
     }
     tree_destroy(pmacnames_sysfiles);
}


void pmacnames_fini()
{
     tree_clearoutandfree(pmacnames_sysfiles);
     tree_destroy(pmacnames_sysfiles);
}


/* recurse from the root point directory and add the file paths to list 
 * as keys and place the contents of the files as the bodies of the list.
 * Each call at the function will directly add the contents of the 
 * specified directory to the tree, then recurse for the subdirectories */
void pmacnames_readalldir(char *rootdir, TREE *list)
{
     DIR *dir;
     struct dirent *d;
     char filep[PATH_MAX];
     char *data;
     struct stat buf;
     int buflen;

     dir = opendir(rootdir);
     if ( ! dir ) {
	  elog_printf(ERROR, "can't open %s: %d %s", rootdir, errno, 
		      strerror(errno));
	  return;
     }

     while ((d = readdir(dir))) {
	  if (strcmp(d->d_name, ".") == 0)
	       continue;
	  if (strcmp(d->d_name, "..") == 0)
	       continue;

	  sprintf(filep, "%s/%s", rootdir, d->d_name);
	  stat(filep, &buf);

	  /* check to see if file is a directory and recurse if so */
	  if ( S_ISDIR( buf.st_mode ) ) {
	       pmacnames_readalldir(filep, list);
	       continue;
	  }

	  /* read file, strip trailing \n and add contents to list */
	  data = probe_readfile(filep);
	  if ( ! data )
	       continue;
	  buflen = strlen(data);
	  if ( *(data+buflen-1) == '\n' )
	       *(data+buflen-1) = '\0';
	  tree_add(list, xnstrdup(filep), data);
     }

     closedir(dir);
}

void pmacnames_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     pmacnames_init();
     pmacnames_collect();
     if (argc > 1)
	  buf = table_outtable(tab);
     else
	  buf = table_print(tab);
     puts(buf);
     nfree(buf);
     table_destroy(tab);
     exit(0);
}

#endif /* TEST */

#endif /* linux */
