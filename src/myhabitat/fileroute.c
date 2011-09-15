/*
 * Fileroute, part of MyHabitat
 * A layer over some aspects of the route system to copy with file formats
 * encoded in a ROUTE.
 *
 * Nigel Stuckey August 2011
 * Copyright System Garden Ltd, 2011. All rights reserved.
 */

#include "fileroute.h"
#include "../iiab/table.h"
#include "../iiab/route.h"
#include "../iiab/nmalloc.h"

/* Open and read the route, returning a TABLE. 
 * The content type hint helps read and scan the route and successfully 
 * return it in a table. 
 * Typically helps data from 'file:' routes structure into a table where 
 * possible.
 * Will return a table with the 'data' column if the content is unparsable.
 */
TABLE fileroute_tread(char *purl, FILEROUTE_TYPE type)
{
     TABLE tab;

     /* Get the data.
      * Note: tread on a file: will not attempt to scan its contents:
      * this is the whole reason for this fileroute_ clsss. It will
      * try a series of deductions to see if the file is structured */
     tab = route_tread(purl, NULL);
     if ( ! tab )
          return NULL;

     /* if the type structured: RS, GRS etc then just return the table as 
      * we _TRUST_ it will have scanned */
     if ( type == FILEROUTE_TYPE_RS || 
	  type == FILEROUTE_TYPE_GRS )
       return tab;

     /* if the type free-form: TEXT etc then just return the table as 
      * we _TRUST_ it has no format */
     if ( type == FILEROUTE_TYPE_TEXT )
       return tab;


     /* carry out trial scanning to see if there is a table encoded in the
      * data column of a text table of a 'file:' route */

     /* Attempt to scan non structured data into a table */
     if (table_ncols(tab) <= 2 && table_hascol(tab, "data")) {
          char *buf;
	  TABLE newtab;
	  int r;

	  table_first(tab);
	  buf = nstrdup(table_getcurrentcell(tab, "data"));
	  newtab = table_create();

	  switch (type) {
	  case FILEROUTE_TYPE_CSV:
	       /* with ruler+info, then without */
	       r = table_scan(newtab, buf, ",", TABLE_SINGLESEP, 
			      TABLE_HASCOLNAMES, TABLE_HASRULER);
	       if (r == -1) {
		    nfree(buf);
		    buf = nstrdup(table_getcurrentcell(tab, "data"));
		    r = table_scan(newtab, buf, ",", TABLE_SINGLESEP, 
				   TABLE_HASCOLNAMES, TABLE_NORULER);
	       }
	       break;
	  case FILEROUTE_TYPE_TSV:
	       /* with ruler+info, then without */
	       r = table_scan(newtab, buf, "\t", TABLE_SINGLESEP, 
			      TABLE_HASCOLNAMES, TABLE_HASRULER);
	       if (r == -1) {
		    nfree(buf);
		    buf = nstrdup(table_getcurrentcell(tab, "data"));
		    r = table_scan(newtab, buf, "\t", TABLE_SINGLESEP, 
				   TABLE_HASCOLNAMES, TABLE_NORULER);
	       }
	       break;
	  case FILEROUTE_TYPE_SSV:
	       /* with ruler+info, then without */
	       r = table_scan(newtab, buf, " ", TABLE_MULTISEP, 
			      TABLE_HASCOLNAMES, TABLE_HASRULER);
	       if (r == -1) {
		    nfree(buf);
		    buf = nstrdup(table_getcurrentcell(tab, "data"));
		    r = table_scan(newtab, buf, " ", TABLE_MULTISEP, 
				   TABLE_HASCOLNAMES, TABLE_NORULER);
	       }

	       break;
	  case FILEROUTE_TYPE_PSV:
	       /* with ruler+info, then without */
	       r = table_scan(newtab, buf, "|", TABLE_SINGLESEP, 
			      TABLE_HASCOLNAMES, TABLE_HASRULER);
	       if (r == -1) {
		    nfree(buf);
		    buf = nstrdup(table_getcurrentcell(tab, "data"));
		    r = table_scan(newtab, buf, "|", TABLE_SINGLESEP, 
				   TABLE_HASCOLNAMES, TABLE_NORULER);
	       }
	       break;
	  case FILEROUTE_TYPE_FHA:
	       /* only rulers acceptable */
	       r = table_scan(newtab, buf, "\t", TABLE_SINGLESEP, 
			      TABLE_HASCOLNAMES, TABLE_HASRULER);
	       break;
	  case FILEROUTE_TYPE_UNKNOWN:
	       /* try FHA, TSV, CSV, PSV in order. dont try SSV */
	       r = table_scan(newtab, buf, "\t", TABLE_SINGLESEP, 
			      TABLE_HASCOLNAMES, TABLE_HASRULER);
	       if (r == -1) {
		    nfree(buf);
		    buf = nstrdup(table_getcurrentcell(tab, "data"));
		    r = table_scan(newtab, buf, "\t", TABLE_SINGLESEP, 
				   TABLE_HASCOLNAMES, TABLE_NORULER);
	       }
	       if (r == -1) {
		    nfree(buf);
		    buf = nstrdup(table_getcurrentcell(tab, "data"));
		    r = table_scan(newtab, buf, ",", TABLE_SINGLESEP, 
				   TABLE_HASCOLNAMES, TABLE_HASRULER);
	       }
	       if (r == -1) {
		    nfree(buf);
		    buf = nstrdup(table_getcurrentcell(tab, "data"));
		    r = table_scan(newtab, buf, ",", TABLE_SINGLESEP, 
				   TABLE_HASCOLNAMES, TABLE_NORULER);
	       }
	       if (r == -1) {
		    nfree(buf);
		    buf = nstrdup(table_getcurrentcell(tab, "data"));
		    r = table_scan(newtab, buf, "|", TABLE_SINGLESEP, 
				   TABLE_HASCOLNAMES, TABLE_HASRULER);
	       }
	       if (r == -1) {
		    nfree(buf);
		    buf = nstrdup(table_getcurrentcell(tab, "data"));
		    r = table_scan(newtab, buf, "|", TABLE_SINGLESEP, 
				   TABLE_HASCOLNAMES, TABLE_NORULER);
	       }
	       break;
	  default:
	       break;
	  }

	  if (r == -1) {
	       /* scan did not work, we need to return the default data */
	       table_destroy(newtab);
	       nfree(buf);
	       return tab;
	  }

	  /* The conversion into a table worked */
	  table_destroy(tab);
	  table_freeondestroy(newtab, buf);
	  return newtab;

     } else {

          return tab;

     }
}
