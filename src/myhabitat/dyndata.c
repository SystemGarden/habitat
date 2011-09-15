/*
 * Habitat dynamic data for visualisations
 *
 * Nigel Stuckey, September 2011
 * Copyright System Garden Limited 2011. All rights reserved.
 */

#include <time.h>
#include "main.h"
#include "dyndata.h"
#include "../iiab/table.h"
#include "../iiab/cf.h"
#include "../iiab/elog.h"
#include "../iiab/iiab.h"
#include "../iiab/table.h"


/* Return configuration as a TABLE.
 * Time is ignored and the caller will free the TABLE storage
 * NULL is returned if there is no data to show; this routine will
 * take care of specific errors to users */
TABLE dyndata_config(time_t from, time_t to)
{
     TABLE tab;
     int r;

     tab = cf_getstatus ( iiab_cf );
     if ( ! tab ) {
          elog_printf(FATAL, "No configuration at all, please check that "
		      "Habitat is installed correctly");
     } else {
          r = table_nrows(tab);
          if ( r == 0) {
	       elog_printf(ERROR, "Empty configuration. Please check that "
			   "Habitat is installed correctly");
	       return NULL;
	  } else if (r < 0) {
	       elog_printf(ERROR, "Unable to get configuration rows. Please "
			   "check that Habitat is installed correctly");
	       return NULL;
	  }
     }

     return tab;
}


