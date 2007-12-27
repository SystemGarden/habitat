/*
 * Linux network probe for iiab
 * Nigel Stuckey, August 2001
 *
 * Copyright System Garden Ltd 2001. All rights reserved.
 */

#if linux

#include <stdio.h>
#include "probe.h"

/* Linux specific includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/* table constants for system probe */
struct probe_sampletab plinnet_cols[] = {
     {"device",	    "", "str", "cnt", "", "1","device name"},
     {"rx_bytes",   "", "u32", "cnt", "", "", "bytes received"},
     {"rx_pkts",    "", "u32", "cnt", "", "", "packets received"},
     {"rx_errs",    "", "u32", "cnt", "", "", "receive errors"},
     {"rx_drop",    "", "u32", "cnt", "", "", "receive dropped packets"},
     {"rx_fifo",    "", "u32", "cnt", "", "", "received fifo"},
     {"rx_frame",   "", "u32", "cnt", "", "", "receive frames"},
     {"rx_comp",    "", "u32", "cnt", "", "", "receive compressed"},
     {"rx_mcast",   "", "u32", "cnt", "", "", "received multicast"},
     {"tx_bytes",   "", "u32", "cnt", "", "", "bytes transmitted"},
     {"tx_pkts",    "", "u32", "cnt", "", "", "packets transmitted"},
     {"tx_errs",    "", "u32", "cnt", "", "", "transmit errors"},
     {"tx_drop",    "", "u32", "cnt", "", "", "transmit dropped packets"},
     {"tx_fifo",    "", "u32", "cnt", "", "", "transmit fifo"},
     {"tx_colls",   "", "u32", "cnt", "", "", "transmit collisions"},
     {"tx_carrier", "", "u32", "cnt", "", "", "transmit carriers"},
     {"tx_comp",    "", "u32", "cnt", "", "", "transmit compressed"},
     PROBE_ENDSAMPLE
};

/* List of colums to diff */
struct probe_rowdiff plinnet_diffs[] = {
     PROBE_ENDROWDIFF
};

/* static data return methods */
struct probe_sampletab *plinnet_getcols()    {return plinnet_cols;}
struct probe_rowdiff   *plinnet_getrowdiff() {return plinnet_diffs;}
char                  **plinnet_getpub()     {return NULL;}

/* linux version; assume 2.4 being the latest */
int plinnet_linuxversion=24;
int plinnet_ndev=1;

/*
 * Initialise probe for linux interrupt information
 */
void plinnet_init() {
     char *data, *vpt;

     /* we need to work out which version of linux we are running */
     data = probe_readfile("/proc/version");
     if (!data) {
          elog_printf(ERROR, "unable to find the linux kernel "
		      "version file");
	  return;
     }
     vpt = strstr(data, "version ");
     if (!vpt) {
          elog_printf(ERROR, "unable to find the linux kernel version");
	  nfree(data);
	  return;
     }
     vpt += 8;
     if (strncmp(vpt, "2.1.", 4) == 0 || strncmp(vpt, "2.2.", 4) == 0) {
          plinnet_linuxversion=22;
     } else if (strncmp(vpt, "2.3.", 4) == 0 || strncmp(vpt, "2.4.", 4) == 0) {
          plinnet_linuxversion=24;
     } else if (strncmp(vpt, "2.5.", 4) == 0 || strncmp(vpt, "2.6.", 4) == 0) {
          plinnet_linuxversion=26;
     } else {
          elog_printf(ERROR, "unsupported linux kernel version");
     }

     nfree(data);
     return;
}

/*
 * Linux specific routines
 */
void plinnet_collect(TABLE tab) {
     char *data;
     ITREE *lines;

     /* open and process the network stats file */
     data = probe_readfile("/proc/net/dev");
     if (data) {
	  /*elog_printf(DEBUG, "Read from /proc/net/dev: %s", data);*/
       util_scantext(data, ": |", UTIL_MULTISEP, &lines);	/* scan */
	  itree_find(lines, 2);			/* data starts at 3rd line */
	  while(!itree_isbeyondend(lines)) {
	       table_addemptyrow(tab);
	       plinnet_col_netdev(tab, itree_get(lines) );
	       itree_next(lines);
	  }

	  /* clear up */
	  util_scanfree(lines);
	  table_freeondestroy(tab, data);
     }
}

/* 
 * scans one line of network device information from /proc/net/dev 
 * provided as a tokenised list
 */
void plinnet_col_netdev(TABLE tab, ITREE *idata) {
     /* /proc/interrupts in versions 2.2, 2.4 & 2.6 have a layout similar 
      * to the one below:-
      *
      * Inter-|   Receive                                                |  Transmit
      *  face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
      *     lo:  262326     174    0    0    0     0          0         0   262326     174    0    0    0     0       0          0
      *
      * A two line header with one line per interface below it. This routine
      * is called with eacch interface line in turn, so it will be called 
      * with lo, the again with eth0 etc.
      */

     if (plinnet_linuxversion == 24 || plinnet_linuxversion == 22 || 
	 plinnet_linuxversion == 26) {
	  itree_first(idata);
	  table_replacecurrentcell(tab, "device", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "rx_bytes", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "rx_pkts", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "rx_errs", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "rx_drop", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "rx_fifo", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "rx_frame", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "rx_comp", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "rx_mcast", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "tx_bytes", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "tx_pkts", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "tx_errs", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "tx_drop", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "tx_fifo", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "tx_colls", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "tx_carrier", itree_get(idata));
	  itree_next(idata);
	  table_replacecurrentcell(tab, "tx_comp", itree_get(idata));
     }
}


void plinnet_derive(TABLE prev, TABLE cur) {}


#if TEST

/*
 * Main function
 */
main(int argc, char *argv[]) {
     char *buf;

     plinnet_init();
     plinnet_collect();
     if (argc > 1)
	  buf = table_outtable(plinnet_tab);
     else
	  buf = table_print(plinnet_tab);
     puts(buf);
     nfree(buf);
     table_destroy(plinnet_tab);
     exit(0);
}

#endif /* TEST */

#endif /* linux */
