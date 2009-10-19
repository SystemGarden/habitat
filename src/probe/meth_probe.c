/*
 * Probe methods
 * Interface between iiab's meth and the standard probes in this directory.
 *
 * Nigel Stuckey, August 1999, November 2003
 * Copyright System Garden Ltd 1999-2003. All rights reserved.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "probe.h"

struct meth_info probe_cbinfo = {
     probe_id,
     probe_info,
     probe_type,
     probe_init,	/* beforerun */
     NULL,		/* preaction */
     probe_action,
     probe_fini,	/* afterrun */
     NULL		/* name of shared library */
};

/* List of all probe data (struct probe_datainfo), keyed by
 * output route */
PTREE *probe_data=NULL;

/* initialise the table with headers for the specific probe and
 * include the info lines require for probe plotting */
TABLE probe_tabinit(struct probe_sampletab *hd /* col defs */ ) 
{
     TABLE tab;
     struct probe_sampletab *p;

     /* create a table dynamically, which may not be the best performance */
     tab = table_create();

     /* add columns */
     for (p=hd; p->name != NULL; p++) {
	  if (table_addcol(tab, p->name, NULL) == -1)
	       elog_die(FATAL, "unable to add column %s", p->name);
     }

     /* add info rows */
     table_addemptyinfo(tab, "type");
     table_addemptyinfo(tab, "sense");
     table_addemptyinfo(tab, "max");
     table_addemptyinfo(tab, "key");
     table_addemptyinfo(tab, "name");
     table_addemptyinfo(tab, "info");

     /* add info data */
     for (p=hd; p->name != NULL; p++) {
	  if ( ! table_replaceinfocell(tab, "type", p->name, p->type) )
	       elog_die(FATAL, "unable to add info @%s,%s=%s", "type", 
			p->name, p->type);
	  if ( ! table_replaceinfocell(tab, "sense", p->name, p->sense))
	       elog_die(FATAL, "unable to add info @%s,%s=%s", "sense",
			p->name, p->sense);
	  if ( ! table_replaceinfocell(tab, "max", p->name, p->max))
	       elog_die(FATAL, "unable to add info @%s,%s=%s", "max", 
			p->name, p->max);
	  if ( ! table_replaceinfocell(tab, "key", p->name, p->key))
	       elog_die(FATAL, "unable to add info @%s,%s=%s", "key", 
			p->name, p->key);
	  if ( ! table_replaceinfocell(tab, "name", p->name, p->rname) )
	       elog_die(FATAL, "unable to add info @%s,%s=%s", "rname", 
			p->name, p->info);
	  if ( ! table_replaceinfocell(tab, "info", p->name, p->info) )
	       elog_die(FATAL, "unable to add info @%s,%s=%s", "info", 
			p->name, p->info);
     }

     return tab;
}

/* initialise all probe functions, returning -1 if there was a problem  */
int probe_init(char *command, 		/* command line */
	       ROUTE output, 		/* output route */
	       ROUTE error, 		/* error route */
	       struct meth_runset *rset	/* runset structure */ )
{
     struct probe_datainfo *dinfo;
     char *probename, *probeargs;
     int pnlen;

     /* separate the command line into a probe name and an optional
      * set of arguments that some probes may need. nmalloc() memory
      * for this so strtok() can be used. */
     probename = xnstrdup(command);
     pnlen = strcspn(command, " ");
     if (probename[pnlen] == '\0')
	  probeargs = probename+pnlen;		/* only command */
     else {
	  probename[pnlen] = '\0';
	  probeargs = probename+pnlen+1;	/* command + args */
     }

     /* initialise data collection structure */
     dinfo = xnmalloc(sizeof(struct probe_datainfo));
     dinfo->old = NULL;
     dinfo->new = NULL;
#if __svr4__
     if (strstr(probename, "intr")) {
	  psolintr_init();
	  dinfo->rowdiff = psolintr_getrowdiff();
	  dinfo->pub     = psolintr_getpub();
	  dinfo->derive  = psolintr_derive;
     } else if (strstr(probename, "io")) {
	  psolio_init();
	  dinfo->rowdiff = psolio_getrowdiff();
	  dinfo->pub     = psolio_getpub();
	  dinfo->derive  = psolio_derive;
     } else if (strstr(probename, "names")) {
	  psolnames_init();
	  dinfo->rowdiff = psolnames_getrowdiff();
	  dinfo->pub     = psolnames_getpub();
	  dinfo->derive  = psolnames_derive;
     } else if (strstr(probename, "ps")) {
	  psolps_init(probeargs);
	  dinfo->rowdiff = psolps_getrowdiff();
	  dinfo->pub     = psolps_getpub();
	  dinfo->derive  = psolps_derive;
     } else if (strstr(probename, "sys")) {
	  psolsys_init();
	  dinfo->rowdiff = psolsys_getrowdiff();
	  dinfo->pub     = psolsys_getpub();
	  dinfo->derive  = psolsys_derive;
     } else if (strstr(probename, "timer")) {
	  psoltimer_init();
	  dinfo->rowdiff = psoltimer_getrowdiff();
	  dinfo->pub     = psoltimer_getpub();
	  dinfo->derive  = psoltimer_derive;
     } else if (strstr(probename, "up")) {
	  psolup_init();
	  dinfo->rowdiff = psolup_getrowdiff();
	  dinfo->pub     = psolup_getpub();
	  dinfo->derive  = psolup_derive;
     } else if (strstr(probename, "down")) {
	  psoldown_init(probeargs);
	  dinfo->rowdiff = psoldown_getrowdiff();
	  dinfo->pub     = psoldown_getpub();
	  dinfo->derive  = psoldown_derive;
     } else if (strstr(probename, "net")) {
	  elog_printf(ERROR, "%s not supported under solaris", command);
	  nfree(probename);
	  return -1;
     } else {
	  elog_printf(ERROR, "unknown solaris probe: %s", command);
	  nfree(probename);
	  return -1;
     }
#elif linux
     if (strstr(probename, "intr")) {
	  plinintr_init();
	  dinfo->rowdiff = plinintr_getrowdiff();
	  dinfo->pub     = plinintr_getpub();
	  dinfo->derive  = plinintr_derive;
     } else if (strstr(probename, "io")) {
	  plinio_init();
	  dinfo->rowdiff = plinio_getrowdiff();
	  dinfo->pub     = plinio_getpub();
	  dinfo->derive  = plinio_derive;
     } else if (strstr(probename, "names")) {
	  plinnames_init();
	  dinfo->rowdiff = plinnames_getrowdiff();
	  dinfo->pub     = plinnames_getpub();
	  dinfo->derive  = plinnames_derive;
     } else if (strstr(probename, "ps")) {
	  plinps_init(probeargs);
	  dinfo->rowdiff = plinps_getrowdiff();
	  dinfo->pub     = plinps_getpub();
	  dinfo->derive  = plinps_derive;
     } else if (strstr(probename, "sys")) {
	  plinsys_init();
	  dinfo->rowdiff = plinsys_getrowdiff();
	  dinfo->pub     = plinsys_getpub();
	  dinfo->derive  = plinsys_derive;
     } else if (strstr(probename, "timer")) {
	  elog_printf(ERROR, "%s not supported under linux", command);
	  nfree(probename);
	  return -1;
     } else if (strstr(probename, "net")) {
	  plinnet_init();
	  dinfo->rowdiff = plinnet_getrowdiff();
	  dinfo->pub     = plinnet_getpub();
	  dinfo->derive  = plinnet_derive;
     } else if (strstr(probename, "up")) {
	  plinup_init();
	  dinfo->rowdiff = plinup_getrowdiff();
	  dinfo->pub     = plinup_getpub();
	  dinfo->derive  = plinup_derive;
     } else if (strstr(probename, "down")) {
	  plindown_init(probeargs);
	  dinfo->rowdiff = plindown_getrowdiff();
	  dinfo->pub     = plindown_getpub();
	  dinfo->derive  = plindown_derive;
     } else {
	  elog_printf(ERROR, "unknown linux probe: %s", command);
	  nfree(probename);
	  return -1;
     }
#else
     elog_printf(ERROR, "platform not supported: %s", command);
     nfree(probename);
     return -1;
#endif

     /* add to global list keyed by runset */
     if (probe_data == NULL)
	  probe_data = ptree_create();
     ptree_add(probe_data, rset, dinfo);

     nfree(probename);
     return 0;
}

char *        probe_id()   { return "probe"; }
char *        probe_info() { return "Standard data collection probes"; }
enum exectype probe_type() { return METH_SOURCE; }

/* 
 * Run the probe that corresponds to the string present in command,
 * which can be:
 *
 *     intr, io, names, ps, sys, timer, net, up, down
 *
 * Returns -1 if there was an error with the exec.
 */
int probe_action(char *command,  		/* command line */
		 ROUTE output,  		/* output route */
		 ROUTE error,  			/* error route */
		 struct meth_runset *rset	/* runset structure */ )
{
     struct probe_datainfo *dinfo;
     char *probename, *probeargs;
     int pnlen, i;
     ITREE *origcols, *newcols;

     /* separate the command line into a probe name and an optional
      * set of arguments that some probes may need. nmalloc() memory
      * for this so strtok() can be used to tokenise. */
     probename = xnstrdup(command);
     pnlen = strcspn(command, " ");
     probename[pnlen] = '\0';
     probeargs = probename+pnlen+1;

     /* fetch probe data structure */
     if ( ! probe_data )
	  elog_die(FATAL, "probe_data not initialised");
     dinfo = ptree_find(probe_data, rset);
     if (dinfo == PTREE_NOVAL) {
	  elog_printf(ERROR, "can't find details - method: %s command: %s", 
		      "probe", command);
	  nfree(probename);
	  return -1;
     }

     /* Maintain two tables of data in the probe structure: old and new.
      * This is to allow some differencing 'services' to be performed.
      * Remove the old table and replace with new one */
     if (dinfo->old)
	  table_destroy(dinfo->old);
     dinfo->old = dinfo->new;

     /* for each command, initialise new table and fill with new data */
     if (strstr(probename, "intr")) {
#if __svr4__
	  dinfo->new = probe_tabinit( psolintr_getcols() );
	  psolintr_collect( dinfo->new );
#elif linux
	  dinfo->new = probe_tabinit( plinintr_getcols() );
	  plinintr_collect( dinfo->new );
#else
#endif
     } else if (strstr(probename, "io")) {
#if __svr4__
	  dinfo->new = probe_tabinit( psolio_getcols() );
	  psolio_collect( dinfo->new );
#elif linux
	  dinfo->new = probe_tabinit( plinio_getcols() );
	  plinio_collect( dinfo->new );
#else
#endif
     } else if (strstr(probename, "names")) {
#if __svr4__
	  dinfo->new = probe_tabinit( psolnames_getcols() );
	  psolnames_collect( dinfo->new );
#elif linux
	  dinfo->new = probe_tabinit( plinnames_getcols() );
	  plinnames_collect( dinfo->new );
#else
#endif
     } else if (strstr(probename, "ps")) {
#if __svr4__
	  dinfo->new = probe_tabinit( psolps_getcols() );
	  psolps_collect( dinfo->new );
#elif linux
	  dinfo->new = probe_tabinit( plinps_getcols() );
	  plinps_collect( dinfo->new );
#else
#endif
     } else if (strstr(probename, "sys")) {
#if __svr4__
	  dinfo->new = probe_tabinit( psolsys_getcols() );
	  psolsys_collect( dinfo->new );
#elif linux
	  dinfo->new = probe_tabinit( plinsys_getcols() );
	  plinsys_collect( dinfo->new );
#else
#endif
     } else if (strstr(probename, "timer")) {
#if __svr4__
	  dinfo->new = probe_tabinit( psoltimer_getcols() );
	  psoltimer_collect( dinfo->new );
#elif linux
	  elog_printf(ERROR, "unknown linux probe: %s", command);
	  nfree(probename);
	  return -1;
#else
#endif
     } else if (strstr(probename, "net")) {
#if __svr4__
	  elog_printf(ERROR, "unknown solaris probe: %s", command);
	  nfree(probename);
	  return -1;
#elif linux
	  dinfo->new = probe_tabinit( plinnet_getcols() );
	  plinnet_collect( dinfo->new );
#else
#endif
     } else if (strstr(probename, "up")) {
#if __svr4__
	  dinfo->new = probe_tabinit( psolup_getcols() );
	  /	  psolup_collect( dinfo->new );
#elif linux
	  dinfo->new = probe_tabinit( plinup_getcols() );
	  plinup_collect( dinfo->new );
#else
#endif
     } else if (strstr(probename, "down")) {
#if __svr4__
	  dinfo->new = probe_tabinit( psoldown_getcols() );
	  psoldown_collect( dinfo->new );
#elif linux
	  dinfo->new = probe_tabinit( plindown_getcols() );
	  plindown_collect( dinfo->new );
#else
#endif
     } else {
	  /* send error */
	  elog_printf(ERROR, "unknown probe: %s", command);
	  nfree(probename);
	  return -1;
     }

     /* Return error if probes return NULL */
     if ( ! dinfo->new ) {
	  nfree(probename);
	  return -1;
     }

     /* carry out all differences between samples */
     probe_rundiff(dinfo);

     /* output table in scannable format and destroy after each action.
      * If the publish list is set, print only those columns listed */
     if (dinfo->pub && dinfo->pub[0]) {
	  newcols = itree_create();
	  for (i=0; dinfo->pub[i]; i++) {
	       itree_append(newcols, dinfo->pub[i]);
	  }
	  origcols = table_setcolorder(dinfo->new, newcols);
	  route_twrite(output, dinfo->new);
	  table_setcolorder(dinfo->new, origcols);
	  itree_destroy(newcols);
     } else {
	  route_twrite(output, dinfo->new);
     }

     nfree(probename);
     return 0;
}


/* calculate differences */

/* shut down probes at the end of the run, returning -1 for error */
int probe_fini(char *command,  		/* command line */
	       ROUTE output,  		/* output route */
	       ROUTE error,  		/* error route */
	       struct meth_runset *rset	/* runset structure */ ) 
{
     struct probe_datainfo *dinfo;
     char *probename, *probeargs;
     int pnlen;

     /* separate the command line into a probe name and an optional
      * set of arguments that some probes may need. nmalloc() memory
      * for this so strtok() can be used. */
     probename = xnstrdup(command);
     pnlen = strcspn(command, " ");
     probename[pnlen] = '\0';
     probeargs = probename+pnlen+1;

     /* fetch probe data entry */
     if ( ! probe_data )
	  elog_die(FATAL, "probe_data not initialised");
     dinfo = ptree_find(probe_data, rset);
     if (dinfo == PTREE_NOVAL) {
	  elog_printf(ERROR, "can't find details - method: %s "
		      "command: %s", "probe", command);
	  nfree(probename);
	  return -1;
     }

     /* clear up data for the probe instance */
     if (dinfo->old)
	  table_destroy(dinfo->old);
     if (dinfo->new)
	  table_destroy(dinfo->new);
     nfree(dinfo);

     /* finalise data collecion */
#if __svr4__
     if (strstr(probename, "intr")) {
     } else if (strstr(probename, "io")) {
     } else if (strstr(probename, "names")) {
     } else if (strstr(probename, "ps")) {
	  psolps_fini();
     } else if (strstr(probename, "sys")) {
	  psolsys_fini();
     } else if (strstr(probename, "timer")) {
     } else if (strstr(probename, "net")) {
     } else if (strstr(probename, "up")) {
     } else if (strstr(probename, "down")) {
	  psoldown_fini();
     } else {
	  elog_printf(ERROR, "unknown solaris probe: %s", command);
	  nfree(probename);
	  return -1;
     }
#elif linux
     if (strstr(probename, "intr")) {
     } else if (strstr(probename, "io")) {
     } else if (strstr(probename, "names")) {
     } else if (strstr(probename, "ps")) {
	  plinps_fini();
     } else if (strstr(probename, "sys")) {
	  plinsys_fini();
     } else if (strstr(probename, "net")) {
     } else if (strstr(probename, "up")) {
     } else if (strstr(probename, "down")) {
	  plindown_fini();
     } else {
	  elog_printf(ERROR, "unknown linux probe: %s", command);
	  nfree(probename);
	  return -1;
     }
#else
     elog_printf(ERROR, "platform not supported: %s", command);
     nfree(probename);
     return -1;
#endif

     nfree(probename);
     return 0;
}



/*
 * Calculate row differences, both generic and specific
 *
 * In each probe_datainfo structure, there are two sample tables (old and new)
 * and a probe_rowdiff array. Rowdiff holds details of the column names that
 * need to be differenced between the two samples and the name of 
 * result column. Sample TABLE columns should have their types specified
 * to allow the calculation to take place.
 *
 * After the generic differencing and for iteration 2 onwards, run probe 
 * specific processing with dinfo->derive hook.
 */
void probe_rundiff(struct probe_datainfo *dinfo) {
     int haskey=0, newrowid=0, oldrowid=0;
     char *keycol;
     TREE *inforow, *keyvals;

     /* derive historic calculations for iteration 2 onwards */
     if (dinfo->old != NULL) {

          /* if row differences are set, calculate them */
          if (dinfo->rowdiff && dinfo->rowdiff->source) {
	       /* find keys that may exist in the new table.
		* If they do exist, set haskey and compile the instance names 
		* into keyvals */
	       inforow = table_getinforow(dinfo->new, "key");
	       if (inforow) {
		    keycol = tree_search(inforow, "1", 2);
		    if (keycol) {
		         keyvals = table_uniqcolvals(dinfo->new, keycol, NULL);
			 if (keyvals) {
			      haskey++;
			 } else {
			      nfree(inforow);
			 }
		    }
	       }

	       /* traverse the new table by instance name, match rows with
		* the old table then calculate a difference */
	       if ( haskey ) {
		    /* multi instance samples */
		    tree_traverse(keyvals) {
		         /* current rows of each table to the same key */
		         newrowid = table_search(dinfo->new, keycol, 
						 tree_getkey(keyvals));
			 oldrowid = table_search(dinfo->old, keycol, 
						 tree_getkey(keyvals));
			 probe_rowdiff(dinfo);
		    }
	       } else {
		    /* single instance samples */

		    /* Reset current row of both tables */
		    table_first(dinfo->new);
		    table_first(dinfo->old);

		    probe_rowdiff(dinfo);
	       }
	  }

	  /* calculate probe specific, special metrics */
	  if (dinfo->derive)
	       dinfo->derive(dinfo->old, dinfo->new);
     }

     /* remove the key data */
     if (haskey) {
          tree_destroy(keyvals);
	  tree_destroy(inforow);
     }

}


/* Calculate row differences between current rows of two rows: old and new.
 *
 * In each probe_datainfo structure, there are two sample tables (old and new)
 * and a probe_rowdiff array. Rowdiff holds details of the column names that
 * need to be differenced between the two samples and the name of 
 * result column. Sample TABLE columns should have their types specified
 * to allow the calculation to take place.
 *
 * After the generic differencing and for iteration 2 onwards, run probe 
 * specific processing with dinfo->derive hook.
 */
void probe_rowdiff(struct probe_datainfo *dinfo) {
     struct probe_rowdiff *rdiff;
     char *difftype;
     long idiff;
     unsigned long udiff;
     long long lldiff;
     unsigned long long ulldiff;

     /* Each probe nominates a set of columns on which to calculate 
      * differences. */
     for (rdiff = dinfo->rowdiff; 
	  rdiff != NULL && rdiff->source != NULL; 
	  rdiff++) {

          /* Extract type from info */
          difftype = table_getinfocell(dinfo->new, "type", rdiff->source);
	  if (strcmp(difftype, "i32")) {

	       /* signed 32 bit integer */
	       idiff = strtol(table_getcurrentcell(dinfo->new,rdiff->source),
			      NULL, 10) - 
		       strtol(table_getcurrentcell(dinfo->old, rdiff->source), 
			      NULL, 10);
	       table_replacecurrentcell_alloc(dinfo->new, rdiff->result,
					      util_i32toa(idiff));

	  } else if (strcmp(difftype, "u32")) {

	       /* unsigned 32 bit integer */
	       udiff = strtoul(table_getcurrentcell(dinfo->new,rdiff->source), 
			       NULL, 10) - 
		       strtoul(table_getcurrentcell(dinfo->old,rdiff->source),
			       NULL, 10);
	       table_replacecurrentcell_alloc(dinfo->new, rdiff->result,
					      util_u32toa(udiff));

	  } else if (strcmp(difftype, "i64")) {

	       /* signed 64 bit integer */
	       lldiff = strtol(table_getcurrentcell(dinfo->new,rdiff->source), 
			       NULL, 10) - 
		        strtol(table_getcurrentcell(dinfo->old,rdiff->source), 
			       NULL, 10);
	       table_replacecurrentcell_alloc(dinfo->new, rdiff->result,
					      util_i64toa(lldiff));

	  } else if (strcmp(difftype, "u64")) {

	       /* unsigned 64 bit integer */
	       ulldiff=strtoull(table_getcurrentcell(dinfo->new,rdiff->source),
				NULL, 10) - 
		       strtoull(table_getcurrentcell(dinfo->old,rdiff->source),
				NULL, 10);
	       table_replacecurrentcell_alloc(dinfo->new, rdiff->result,
					      util_u64toa(ulldiff));
	  }
     }

}



/* read file into memory and return address
 * free allocated memory with nfree().
 * if there were errors, NULL is returned */
char *probe_readfile(char *fname) {
     struct stat buf;
     int fd, nread, maxread;
     char *data;

     fd = open(fname, O_RDONLY);
     if (fd < 0)
	  return NULL;
  
     if (fstat(fd, &buf)) {
	  elog_printf(ERROR, "unable to fstat: %s: %d %s", 
		      fname, errno, strerror(errno));
	  return NULL;
     }

     /* Solaris correctly reports the size of the file to be read in
      * stat(), whereas Linux does not */
     if (buf.st_size == 0) {
#if __svr4__
	  elog_printf(ERROR, "null file: %s: %d %s", 
		      fname, errno, strerror(errno));
#endif
	  maxread = PROBE_STATSZ;
     } else {
	  maxread = buf.st_size + 1;	/* make larger to stop wingeing */
     }

     /* read the file */
     data = xnmalloc(maxread+1);
     nread = read(fd, data, maxread);
     if (nread < maxread )
	  *(data+nread) = '\0';
     else {
	  elog_printf(WARNING, "read maximum stat %d bytes: %s",
		      maxread, fname);
	  *(data+nread-1) = '\0';
     }

     close(fd);
     return data;
}


