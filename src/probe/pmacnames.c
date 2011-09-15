/*
 * Mac OS X name probe for iiab
 * Nigel Stuckey, June 2011
 *
 * Copyright System Garden Ltd 2011. All rights reserved.
 */

#if __APPLE_CC__

#include <stdio.h>
#include "probe.h"

/* MacOSX specific includes */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <string.h>
#include <stdio.h>

struct ctlname pmacnames_kernnames[] = CTL_KERN_NAMES;
struct ctlname pmacnames_hwnames[] = CTL_HW_NAMES;
struct ctlname pmacnames_usernames[] = CTL_USER_NAMES;

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
 * Mac specific routines
 */
void pmacnames_collect(TABLE tab) {

     pmacnames_readkern("kern", tab);
     pmacnames_readhw("hw", tab);
     pmacnames_readuser("user", tab);
}


void pmacnames_fini()
{
}


/* Read all the keys from CTL_KERN namespace within sysctl() and insert
 * in the given table using the namespace rootns.
 * Currently reads from kern.* with no recursion.
 * kern.ipc, kern.sysv, kern.exec, kern.lctx are not read
 */
void pmacnames_readkern(char *rootns, TABLE tab)
{
     int i, r, mib[4];
     char kvalue_str[128];
     int kvalue_int;
     long long kvalue_quad;
     size_t len;
     char name[64], value[128];

     mib[0] = CTL_KERN;
     for (i = 0; i < KERN_MAXID; i++) {
          mib[1] = i;
	  sprintf(name, "%s.%s", rootns, pmacnames_kernnames[i].ctl_name);
	  if ( ! pmacnames_kernnames[i].ctl_name )
	       continue;

	  switch(pmacnames_kernnames[i].ctl_type) {
	  case CTLTYPE_INT:
	       len = sizeof(kvalue_int);
	       r = sysctl(mib, 2, &kvalue_int, &len, NULL, 0);
	       break;
	  case CTLTYPE_STRING:
	       len = 128;
	       r = sysctl(mib, 2, &kvalue_str, &len, NULL, 0);
	       break;
	  case CTLTYPE_QUAD:
	       len = sizeof(kvalue_quad);
	       r = sysctl(mib, 2, &kvalue_quad, &len, NULL, 0);
	       break;
	  default:
#if 0
	       elog_printf(ERROR, "can't read %s: type %d int %d", name,
			   pmacnames_kernnames[i].ctl_type, i);
#endif
	       continue;
	  }

	  if (r == -1) {
#if 0
	       elog_printf(ERROR, "can't read %s: %d %s", name, errno, 
			   strerror(errno));
#endif
	       continue;
	  }

	  if (len > 0) {
	       /* save data */
	       switch(pmacnames_kernnames[i].ctl_type) {
	       case CTLTYPE_INT:
		    sprintf(value, "%d", kvalue_int);
		    break;
	       case CTLTYPE_STRING:
		    sprintf(value, "%s", kvalue_str);
		    break;
	       case CTLTYPE_QUAD:
		    sprintf(value, "%llu", kvalue_quad);
		    break;
	       default:
#if 0
		    elog_printf(ERROR, "can't convert %s", name);
#endif
		    break;
	       }
	  }

	  /* add the three column row into the table */
	  table_addemptyrow(tab);
	  table_replacecurrentcell_alloc(tab, "name",  name);
	  table_replacecurrentcell_alloc(tab, "vname", 
					 pmacnames_kernnames[i].ctl_name);
	  table_replacecurrentcell_alloc(tab, "value", value);
     }
}

/* Read all the keys from CTL_HW namespace within sysctl() and insert
 * in the given table using the namespace rootns.
 * Currently reads from hw.* with no recursion.
 */
void pmacnames_readhw(char *rootns, TABLE tab)
{
     int i, r, mib[4];
     char value_str[128];
     int value_int;
     long long value_quad;
     size_t len;
     char name[64], value[128];

     mib[0] = CTL_HW;
     for (i = 0; i < HW_MAXID; i++) {
          mib[1] = i;
	  sprintf(name, "%s.%s", rootns, pmacnames_hwnames[i].ctl_name);
	  if ( ! pmacnames_hwnames[i].ctl_name )
	       continue;

	  switch(pmacnames_hwnames[i].ctl_type) {
	  case CTLTYPE_INT:
	       len = sizeof(value_int);
	       r = sysctl(mib, 2, &value_int, &len, NULL, 0);
	       break;
	  case CTLTYPE_STRING:
	       len = 128;
	       r = sysctl(mib, 2, &value_str, &len, NULL, 0);
	       break;
	  case CTLTYPE_QUAD:
	       len = sizeof(value_quad);
	       r = sysctl(mib, 2, &value_quad, &len, NULL, 0);
	       break;
	  default:
#if 0
	       elog_printf(ERROR, "can't read %s: type %d int %d", name,
			   pmacnames_hwnames[i].ctl_type, i);
#endif
	       continue;
	  }

	  if (r == -1) {
#if 0
	       elog_printf(ERROR, "can't read %s: %d %s", name, errno, 
			   strerror(errno));
#endif
	       continue;
	  }

	  if (len > 0) {
	       /* save data */
	       switch(pmacnames_hwnames[i].ctl_type) {
	       case CTLTYPE_INT:
		    sprintf(value, "%d", value_int);
		    break;
	       case CTLTYPE_STRING:
		    sprintf(value, "%s", value_str);
		    break;
	       case CTLTYPE_QUAD:
		    sprintf(value, "%llu", value_quad);
		    break;
	       default:
#if 0
		    elog_printf(ERROR, "can't convert %s", name);
#endif
		    break;
	       }
	  }

	  /* add the three column row into the table */
	  table_addemptyrow(tab);
	  table_replacecurrentcell_alloc(tab, "name",  name);
	  table_replacecurrentcell_alloc(tab, "vname", 
					 pmacnames_hwnames[i].ctl_name);
	  table_replacecurrentcell_alloc(tab, "value", value);
     }
}

/* Read all the keys from CTL_USER namespace within sysctl() and insert
 * in the given table using the namespace rootns.
 * Currently reads from user.* with no recursion.
 */
void pmacnames_readuser(char *rootns, TABLE tab)
{
     int i, r, mib[4];
     char value_str[128];
     int value_int;
     long long value_quad;
     size_t len;
     char name[64], value[128];

     mib[0] = CTL_USER;
     for (i = 0; i < USER_MAXID; i++) {
          mib[1] = i;
	  sprintf(name, "%s.%s", rootns, pmacnames_usernames[i].ctl_name);
	  if ( ! pmacnames_usernames[i].ctl_name )
	       continue;

	  switch(pmacnames_usernames[i].ctl_type) {
	  case CTLTYPE_INT:
	       len = sizeof(value_int);
	       r = sysctl(mib, 2, &value_int, &len, NULL, 0);
	       break;
	  case CTLTYPE_STRING:
	       len = 128;
	       r = sysctl(mib, 2, &value_str, &len, NULL, 0);
	       break;
	  case CTLTYPE_QUAD:
	       len = sizeof(value_quad);
	       r = sysctl(mib, 2, &value_quad, &len, NULL, 0);
	       break;
	  default:
#if 0
	       elog_printf(ERROR, "can't read %s: type %d int %d", name,
			   pmacnames_usernames[i].ctl_type, i);
#endif
	       continue;
	  }

	  if (r == -1) {
#if 0
	       elog_printf(ERROR, "can't read %s: %d %s", name, errno, 
			   strerror(errno));
#endif
	       continue;
	  }

	  if (len > 0) {
	       /* save data */
	       switch(pmacnames_usernames[i].ctl_type) {
	       case CTLTYPE_INT:
		    sprintf(value, "%d", value_int);
		    break;
	       case CTLTYPE_STRING:
		    sprintf(value, "%s", value_str);
		    break;
	       case CTLTYPE_QUAD:
		    sprintf(value, "%llu", value_quad);
		    break;
	       default:
#if 0
		    elog_printf(ERROR, "can't convert %s", name);
#endif
		    break;
	       }
	  }

	  /* add the three column row into the table */
	  table_addemptyrow(tab);
	  table_replacecurrentcell_alloc(tab, "name",  name);
	  table_replacecurrentcell_alloc(tab, "vname", 
					 pmacnames_usernames[i].ctl_name);
	  table_replacecurrentcell_alloc(tab, "value", value);
     }
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
