/*
 * Program configuration class.
 * Scans the route sepcified for lines that match configuration patterns.
 *
 * The patterns are as follows:-
 *
 * <magic number>	   Magic: optional on first line only
 * <key> = <value>	   Scalar assignment: see <key> and <value> below
 * <key> <val1> <val2> ... List assignment: Key is assigned a list
 * [-]<key>		   Flag, - optional, sets value of key to -1
 * +<key>		   Opposite flag, sets value of key to +1
 * #			   Comment following # character
 *
 * <key> may be defined as [^ \t\n=#]. 
 * <value> is taken as the string of characters following =, running up 
 * to \n or #, with leading and trailing spaces removed. To get these 
 * characters, the string may be enclosed in a quotes thus: 
 * " hello, ###, world ".
 * <valN> is a space seperated token, which may include spaces and escaped 
 * characters by enclosing in quotes.
 *
 * The scan produces a tree of key value pairs, which may be scanned by key.
 * Values may be scalar ints or strings, or vectors in the form of lists.
 * A string subset function is also supported for searching.
 *
 * Once scanned in, it is also possible to amend the values, generally under
 * program control and save the values back to source once modified and
 * if the user has enough permission.
 *
 * Nigel Stuckey, December 1997
 * Major revision July 1999
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include "nmalloc.h"
#include "cf.h"
#include "route.h"
#include "elog.h"
#include "tree.h"
#include "util.h"
#include "table.h"

/* status table header string */
char *cf_colnames[] = {"name", "arg", "value", NULL};

/* Create an empty configuration list */
CF_VALS cf_create()
{
     return(tree_create());
}

/* Destroy a configuration list, possibly containing items */
void cf_destroy(CF_VALS cf)
{
     tree_traverse(cf) {
	  cf_entfree( tree_get(cf) );
	  nfree( tree_getkey(cf) );
     }
     tree_destroy(cf);
}

/*
 * Scan the route for configuration tokens, placing their key/value pairs
 * into the passed list. Also scan for a magic line (if magic is not NULL), 
 * which is a single line at the begining of the route's data. 
 * Use the scanning rules from util_scantext() for details.
 * If overwrite is set, details found will replace existing data; if false
 * scanned data will be ignored for that key.
 * cf_scan() always attempts to recover from parsing errors.
 * Returns 1 for success (may include no data being read) or 
 * 0 if a failure occurs (like the magic number did not match).
 */
int cf_scanroute(CF_VALS cf,		/* configuration list */
		 char *magic,		/* scanf format for magic line */
		 char *cfroute,		/* Route pseudo-url of config data */
		 int overwrite		/* Overwrite existing data? */ )
{
     ITREE *lol;
     int r;

     r = util_parseroute(cfroute, " \t=", magic, &lol);
     if (r < 0)
	  return 0;
     if (r == 0)
          return 1;

     r = cf_scan(cf, lol, overwrite);
     itree_destroy(lol);
     return r;
}


/*
 * Scan the text buffer for configuration tokens, placing their key/value pairs
 * into the passed list. Also scan for a magic line (if magic is not NULL), 
 * which is a single line at the begining of the buffer.
 * Use the scanning rules from util_scantext() for details.
 * If overwrite is set, details found will replace existing data; if false
 * scanned data will be ignored for that key.
 * cf_scan() always attempts to recover from parsing errors.
 * Returns 1 for success or 0 if a failure occurs, the magic number
 * did not match or cftext contained no information.
 */
int cf_scantext(CF_VALS cf,		/* configuration list */
		char *magic,		/* scanf format for magic line */
		char *cftext,		/* text buffer of config data */
		int overwrite		/* overwrite existing data? */ )
{
     ITREE *lol;
     int r;
     char *mycftext;

     mycftext = xnstrdup(cftext);
     if (util_parsetext(mycftext, " \t=", magic, &lol) <= 0)
	  return 0;

     r = cf_scan(cf, lol, overwrite);
     itree_destroy(lol);
     nfree(mycftext);
     return r;
}


/*
 * Follow the tree (lol) of scanned tokens for configuration rules.
 * Adopts the memory management of the passed tree (reparents it)
 * and will free the memory once it is finished with.
 * If overwrite is set, details found will replace existing data; 
 * if overwrite is false scanned data will be ignored for that key.
 * cf_scan() always attempts to recover from parsing errors.
 * Returns 1 for success or 0 if a failure occurs.
 */
int cf_scan(CF_VALS cf,		/* configuration list */
	    ITREE *lol,		/* scanned config data */
	    int overwrite	/* overwrite existing data? */ )
{
     struct cf_entval *entry;
     char *key;

     if (cf == NULL || lol == NULL)
	  return 0;

     /* scan the first arguments to set byname list */
     itree_traverse(lol) {
	  /* get line */
	  if ( ! itree_get(lol) )
	       continue;		/* empty line */

	  /* work out how to treat the line */
	  switch (itree_n( itree_get(lol) )) {
	  case 0:
	       /* empty tree -- odd but not a problem, just get rid of it */
	       itree_destroy( itree_get(lol) );
	       itree_put(lol, NULL);
	       continue;
	  case 1:
	       /* switch -- create entry and dispose of argument ITREE
		* values will be +1 */
	       entry = xnmalloc(sizeof(struct cf_entval));
	       entry->vector = 0;
	       itree_first( itree_get(lol) );
	       key = (char *) itree_get( itree_get(lol) );
	       switch ( *key ) {
	       case '-':
		    if ( ! overwrite && 
			 tree_find(cf, key+1) != TREE_NOVAL ) {

		         nfree(key);
		         itree_destroy( itree_get(lol) );
			 itree_put(lol, NULL);
			 nfree(entry);
			 continue;	/* don't overwrite existing data */
		    }
		    entry->data.arg = xnstrdup("-1");
		    /* dup the key name without the '-' to store in cf
		     * then free the '-' version */
		    cf_entreplace(cf, xnstrdup(key+1), entry);
		    nfree(key);
		    break;
	       case '+':
		    if ( ! overwrite && 
			 tree_find(cf, key+1) != TREE_NOVAL ) {

		         nfree(key);
		         itree_destroy( itree_get(lol) );
			 itree_put(lol, NULL);
			 nfree(entry);
			 continue;	/* don't overwrite existing data */
		    }
		    entry->data.arg = xnstrdup("+1");
		    /* dup the key name without the '+' to store in cf
		     * then free the '+' version */
		    cf_entreplace(cf, xnstrdup(key+1), entry);
		    nfree(key);
		    break;
	       default:
		    if ( ! overwrite && 
			 tree_find(cf, key) != TREE_NOVAL ) {

		         nfree(key);
		         itree_destroy( itree_get(lol) );
			 itree_put(lol, NULL);
			 nfree(entry);
			 continue;	/* don't overwrite existing data */
		    }
		    entry->data.arg = xnstrdup("+1");
		    cf_entreplace(cf, key, entry);
		    break;
	       }
	       itree_destroy( itree_get(lol) );
	       itree_put(lol, NULL);
	       break;
	  case 2:
	       /* value assignmant -- take over parenting of the two args
		* in the line */
	       if ( ! overwrite && 
		    tree_find(cf, itree_get(lol)) != TREE_NOVAL ) {

		    itree_clearoutandfree( itree_get(lol) );
		    itree_destroy( itree_get(lol) );
		    itree_put(lol, NULL);
		    continue;	/* don't overwrite existing data */
	       }
	       entry = xnmalloc(sizeof(struct cf_entval));
	       entry->vector = 0;
	       entry->data.arg = itree_find( itree_get(lol), 1 );
	       cf_entreplace(cf, itree_find( itree_get(lol), 0 ), entry);
	       itree_destroy( itree_get(lol) );
	       itree_put(lol, NULL);
	       break;
	  default:
	       /* value -- reparent all the args: it is a vector and should
		* be expressed as an ITREE */
	       if ( ! overwrite && 
		    tree_find(cf, itree_get(lol)) != TREE_NOVAL ) {

		    itree_clearoutandfree( itree_get(lol) );
		    itree_destroy( itree_get(lol) );
		    itree_put(lol, NULL);
		    continue;	/* don't overwrite existing data */
	       }
	       itree_first( itree_get(lol) );
	       entry = xnmalloc(sizeof(struct cf_entval));
	       entry->vector = 1;
	       entry->data.vec = itree_get(lol);
	       cf_entreplace(cf, itree_get(itree_get(lol)), entry);
	       itree_rm(itree_get(lol)); /* rm 1st arg which is the key */
	       itree_put(lol, NULL);
	       break;
	  }
     }

     return 1;
}



/*
 * Scan the command line (specified by argc and argv) for switches, 
 * using the option string `opt', which is the same format as getopt. 
 * If options are found, they are placed in the configuration list as 
 * a key/value pair. 
 * The key is the text of the command line option, the value either -1 for 
 * a switch or the value of the option (if specified by `opt').
 * Arguments not prefixed by `-' are placed in the list with their key
 * set to `argv<n>', where <n> is the order number.
 * If logroute is defined, cf_cmd() will open it as an output route
 * and send parsing errors to it, including any usage strings that are
 * passed in the function arguments. Logroute may be undefined by setting to
 * "" or NULL.
 * cf_cmd() always attemtps to recover from parsing errors.
 * Returns 1 on success or 0 if a switch failure occurs.
 */
int cf_cmd(CF_VALS cf,	/* configuration list */
	   char *opts,	/* command line option string as getopts(3) */
	   int argc,	/* number of arguments in command line */
	   char **argv,	/* array of argument strings */
	   char *usage	/* string describing usage */ )
{
     int i, error;
     struct cf_entval *entry;
     char tmp[20];

     /* Obtain the command name argv[0] */
     entry = xnmalloc(sizeof(struct cf_entval));
     entry->vector = 0;
     entry->data.arg = xnstrdup(argv[0]);
     tree_add(cf, xnstrdup("argv0"), entry);

     /* Process switches */
     error = 0;
     optind = 1;	/* reset index, in case getopt has been run before */
     while ((i = getopt(argc, argv, opts)) != EOF) {
	  switch (i) {
	  case ':':
	       elog_printf(ERROR, "missing option for switch %c", optopt);
	       error++;
	       break;
	  case '?':
	       elog_printf(ERROR, "switch not recognised: %c", optopt);
	       error++;
	       tmp[0] = optopt;
	       tmp[1] = '\0';
	       entry = xnmalloc(sizeof(struct cf_entval));
	       entry->vector = 0;
	       entry->data.arg = xnstrdup("-1");
	       cf_entreplace(cf, xnstrdup(tmp), entry);
	  default:
	       /* store switch */
	       tmp[0] = i;
	       tmp[1] = '\0';
	       entry = xnmalloc(sizeof(struct cf_entval));
	       entry->vector = 0;
	       if (optarg)
		    entry->data.arg = xnstrdup(optarg);
	       else
		    entry->data.arg = xnstrdup("-1");
	       cf_entreplace(cf, xnstrdup(tmp), entry);
	  }
     }

     /* Do a winge summary */
     if (error) {
          if (error == 1)
	       elog_printf(INFO, "there was a single error");
	  else
	       elog_printf(INFO, "there were %d errors", error);
	  if (usage)
	       elog_printf(INFO, "usage %s %s", argv[0], usage);
	  tree_clearoutandfree(cf);
	  tree_destroy(cf);
	  return 0;
     }

     /* Process arguments */
     sprintf(tmp, "%d", argc-optind+1);
     entry = xnmalloc(sizeof(struct cf_entval));
     entry->vector = 0;
     entry->data.arg = xnstrdup(tmp);
     cf_entreplace(cf, xnstrdup("argc"), entry);
     for (i=optind; i<argc; i++) {
	  sprintf(tmp, "argv%d", i-optind+1);
	  entry = xnmalloc(sizeof(struct cf_entval));
	  entry->vector = 0;
	  entry->data.arg = xnstrdup(argv[i]);
	  cf_entreplace(cf, xnstrdup(tmp), entry);
     }

     return 1;
}

/* 
 * Extracts a value from tree and treats it as an integer 
 * Returns the integer or CF_UNDEF of the key is not present in the
 * list.
 */
int cf_getint(CF_VALS cf, 	/* Parsed configuration values */
	      char *key		/* Key of the value to be returned */ )
{
     struct cf_entval *entry;

     entry = tree_find(cf, key);
     if (entry == TREE_NOVAL)
	  return CF_UNDEF;
     return strtol(entry->data.arg, (char**)NULL, 10);
}

/* Extracts a string from the tree. Does NOT malloc any storage.
 * Returns NULL if no key exists. */
char *cf_getstr(CF_VALS cf, 	/* Parsed configuration values */
		char *key	/* Key of the value to be returned */ )
{
     struct cf_entval *entry;

     entry = tree_find(cf, key);
     if (entry == TREE_NOVAL )
	  return NULL;
     return entry->data.arg;
}

/* Extracts a vector from the tree in the form of an ITREE. 
 * Does NOT nmalloc any storage so please to not alter any data. 
 * Returns NULL if no key exists or the value is not a vector */
ITREE *cf_getvec(CF_VALS cf, 	/* Parsed configuration values */
		 char *key	/* Key of the value to be returned */ )
{
     struct cf_entval *entry;

     entry = tree_find(cf, key);
     if (entry == TREE_NOVAL)
	  return NULL;
     if (entry->vector)
	  return entry->data.vec;
     else
	  return NULL;
}


/* 
 * Add or replace the value with an int of newval.
 * The caller is responsible for key's memory.
 * The whole list is removed with cf_freeparse().
 */
void cf_putint(CF_VALS cf, char *key, int newval)
{
     struct cf_entval *entry;

     entry = xnmalloc(sizeof(struct cf_entval));
     entry->vector = 0;
     entry->data.arg = xnstrdup(util_i32toa(newval)); /* TBD */
     cf_entreplace(cf, xnstrdup(key), entry);
}


/*
 * Add or replace the value with string newval. 
 * The caller is responsible for key and newval's memory.
 * The entry is removed with cf_entfree().
 */
void cf_putstr(CF_VALS cf, char *key, char *newval)
{
     struct cf_entval *entry;

     entry = xnmalloc(sizeof(struct cf_entval));
     entry->vector = 0;
     entry->data.arg = xnstrdup(newval);
     cf_entreplace(cf, xnstrdup(key), entry);
}

/*
 * Add or replace the value with an ITREE newval, which is always list 
 * of strings
 * The caller is responsible for key and newval's memory, this routine
 * will carry out a deep copy of newval so the cf class can look after
 * its own DB.
 * The entry is removed with cf_entfree().
 */
void cf_putvec(CF_VALS cf, char *key, ITREE *newval)
{
     struct cf_entval *entry;

     entry = xnmalloc(sizeof(struct cf_entval));
     entry->vector = 1;
     entry->data.vec = itree_copystr(newval);
     cf_entreplace(cf, xnstrdup(key), entry);
}

/* 
 * Check list for the presence of ALL the keys from key[].
 * Array list is terminated by an element containing NULL.
 * Returns 1 if all keys were in tree or 0 if some or none were present.
 * The entry is removed with cf_entfree().
 */
int cf_check(CF_VALS cf,	/* Parsed configuration values */
	     char *key[]	/* List of keys that must be present */ )
{
     int i;

     for (i=0; key[i] != NULL; i++) {
	  if (tree_find(cf, key[i]) == TREE_NOVAL)
	       return 0;
     }
     return 1;
}

/*
 * Load default values into a configuration list created by cf_scan().
 * The key/value pairs from the default list are loaded into the 
 * configuration list, unless the key already exists in the confguration
 * tree. If the key pre-exists, then the default is not needed, and the
 * next value wil be tried.
 * Format of default list should be a string array: 
 * 	"key1" "value1" "key2" "value2" "key3" "value3", etc...
 * The list is terminated by a NULL in the key value.
 * Data is copied so caller's storage is not needed after the call.
 * Returns the number of insertions made.
 */
int cf_default(CF_VALS cf,	/* Parsed configuration values */
	       char *defaults[]	/* Default list of key-value pairs */ )
{
     int i, insert;
     struct cf_entval *entry;

     for (i=0, insert=0; defaults[i] != NULL; i+=2) {
	  if (tree_find(cf, defaults[i]) == TREE_NOVAL) {
	       entry = xnmalloc(sizeof(struct cf_entval));
	       entry->vector = 0;
	       entry->data.arg = xnstrdup(defaults[i+1]);
	       tree_add(cf, xnstrdup(defaults[i]), entry);
	       insert++;
	  }
     }

     return insert;
}


/*
 * Load default values from one configuration list into another one, 
 * whilst not replacing data that is already there.
 * The same as cf_default(), but using a different source type
 * Returns the number of insertions made.
 */
int cf_defaultcf(CF_VALS cf,		/* Destination configuration values */
		 CF_VALS defaults	/* Default list of key-value pairs */ )
{
     int insert;
     struct cf_entval *entry, *origentry;

     insert = 0;
     tree_traverse(defaults) {
          if (tree_find(cf, tree_getkey(defaults)) == TREE_NOVAL) {
	       /* default not in cf, so enter it */
	       origentry = tree_get(defaults);
	       entry = xnmalloc(sizeof(struct cf_entval));
	       entry->vector = origentry->vector;
	       if (entry->vector) {
		    /* vector */
		    entry->data.vec = itree_create();
		    itree_traverse(origentry->data.vec)
		         itree_append(entry->data.vec,
				   xnstrdup(itree_get(origentry->data.vec)));
	       } else {
		    /* scaler */
		    entry->data.arg = xnstrdup(origentry->data.arg);
	       }
	    
	       tree_add(cf, xnstrdup(tree_getkey(defaults)), entry);
	       insert++;
	  }
     }

     return insert;
}

/*
 * Copy the configuration from one CF_VALS to another, overwriting/clobbering
 * existing entries.
 * Returns the number of entries copied
 */
int cf_copycf(CF_VALS dst,	/* Destination list */
	      CF_VALS src	/* Source list */ )
{
     int insert;
     struct cf_entval *entry, *origentry;

     insert = 0;
     tree_traverse(src) {
          /* deep copy of entry */
          origentry = tree_get(src);
	  entry = xnmalloc(sizeof(struct cf_entval));
	  entry->vector = origentry->vector;
	  if (entry->vector) {
	       /* vector */
	       entry->data.vec = itree_create();
	       itree_traverse(origentry->data.vec)
		 itree_append(entry->data.vec,
			      xnstrdup(itree_get(origentry->data.vec)));
	  } else {
	       /* scaler */
	       entry->data.arg = xnstrdup(origentry->data.arg);
	  }

	  cf_entreplace(dst, xnstrdup(tree_getkey(src)), entry);
	  insert++;
     }

     return insert;
}

/* Returns 1 if key is defined in cf, 0 otherwise */
int cf_defined(TREE *cf,	/* Parsed configuration values */
	       char *key	/* Key of the value to be returned */ )
{
     return(tree_find(cf, key) == TREE_NOVAL ? 0 : 1);
}


/* Returns 1 if key's value is a vector, 0 otherwise */
int cf_isvector(TREE *cf,	/* Parsed configuration values */
		char *key	/* Key of the value to be returned */ )
{
     struct cf_entval *entry;

     entry = tree_find(cf, key);
     if (entry != TREE_NOVAL)
	  return entry->vector;
     else 
	  return 0;
}


/* dumps the contents of the parsed configuration table as a DIAG elog */
void cf_dump(CF_VALS cf		/* Parsed configuration values */ )
{
     struct cf_entval *entry;

     elog_startprintf(DIAG, "Dump of configuration list ----------\n");

     /* Traverse tree (tree has state, which allows us to do this */
     tree_traverse(cf) {
	  entry = tree_get(cf);
	  if (entry->vector) {
	       elog_contprintf(DIAG, "%s = ", tree_getkey(cf));
	       itree_traverse( entry->data.vec )
		    elog_contprintf(DIAG, "%d:[%s] ", 
				    itree_getkey(entry->data.vec), 
				    (char *) itree_get(entry->data.vec));
	       elog_contprintf(DIAG, "\n");
	  } else {
	       elog_contprintf(DIAG, "%s = %s\n", tree_getkey(cf), 
			       entry->data.arg);
	  }
     }

     elog_endprintf(DIAG, "End of configuration list -----------");

     return;
}


/* Generates a table of configuration values in a normalised form of
 * three columns which handles vectors in constant columns. 
 * The three columns are name, argument number and value:
 * scalar values have blank argument numbers, vectors have their arguments 
 * split over several lines with the same value column, but their argument 
 * number to make the composite key of (name,argument) unique. */
TABLE cf_getstatus(CF_VALS cf		/* Parsed configuration values */ )
{
     struct cf_entval *entry;
     char tmp[LINELEN], *tmpcpy;
     TABLE tab;
     TREE *row;

     /* create row */
     row = tree_create();
     tree_add(row, "name", NULL);
     tree_add(row, "value", NULL);
     tree_add(row, "arg", NULL);

     /* create table and add rows to it, made from the configuration table */
     tab = table_create_a(cf_colnames);
     tree_traverse(cf) {
	  entry = tree_get(cf);
	  if (entry->vector) {
	       tree_find(row, "name");
	       tree_put(row, tree_getkey(cf));

	       itree_traverse( entry->data.vec ) {
		    tree_find(row, "value");
		    tree_put(row, itree_get(entry->data.vec));
		    tree_find(row, "arg");
		    sprintf(tmp, "%d", itree_getkey(entry->data.vec));
		    tmpcpy = xnstrdup(tmp);
		    tree_put(row, tmpcpy);
		    table_addrow_alloc(tab, row);
		    table_freeondestroy(tab, tmpcpy);
	       }
	  } else {
	       tree_find(row, "name");
	       tree_put(row, tree_getkey(cf));
	       tree_find(row, "value");
	       tree_put(row, entry->data.arg);
	       tree_find(row, "arg");
	       tree_put(row, NULL);
	       table_addrow_alloc(tab, row);
	  }
     }

     tree_destroy(row);

     return tab;
}


/* Generates a TREE list of configuration values as key-value.
 * The normalised form of cf_getstatus() is followed with the exception
 * of vectors, whose values are concatenated with tabs as delimiters.
 * Returned tree should have its data freed
 */
TREE *cf_gettree(CF_VALS cf		/* Parsed configuration values */ )
{
     struct cf_entval *entry;
     char tmp[LINELEN];
     TREE *list;
     int n;

     /* create list */
     list = tree_create();

     tree_traverse(cf) {
	  entry = tree_get(cf);
	  if (entry->vector) {
	       n = 0;
	       itree_traverse( entry->data.vec ) {
		    n += snprintf(tmp+n, LINELEN-n, "%d\t", 
				  itree_getkey(entry->data.vec));
	       }
	       tmp[n] = '\0';
	       tree_add(list, nstrdup(tree_getkey(cf)), nstrdup(tmp));
	  } else {
	       tree_add(list, nstrdup(tree_getkey(cf)), 
			nstrdup(entry->data.arg));
	  }
     }

     return list;
}


/* Add a string to the configuration list. Makes a copy of the input */
void cf_addstr(CF_VALS cf, char *name, char *value) 
{
     struct cf_entval *entry;

     entry = xnmalloc(sizeof(struct cf_entval));
     entry->vector = 0;
     entry->data.arg = xnstrdup(value);
     tree_add(cf, xnstrdup(name), entry);
}


/*
 * Add the key data pair to the tree, overwriting existing data
 * if it was there. If it displaces data from the tree, the displaced
 * data will be freed with cf_entfree(), the key with nfree().
 * Both key and data are reparented by the cf tree, the caller should
 * not free the arguments individually as memory management passes to this
 * routine which will not fail.
 */
void cf_entreplace(CF_VALS cf, char *key, struct cf_entval *data) {
     struct cf_entval *entry;

     if ( ! cf )
	  elog_die(FATAL, "cf_entreplace()", 0, "cf == NULL");

     if ( (entry = tree_find(cf, key)) == TREE_NOVAL) {
	  tree_add(cf, key, data);
     } else {
	  nfree(key);
	  tree_put(cf, data);
	  cf_entfree(entry);
     }
}

/* Removes the configuration entry and frees the data. If not there, then 
 * 0 is returned and the list in unaffected. If the key is present, then
 * 1 is returned after removal. */
int     cf_rm(CF_VALS cf, char *key)
{
     struct cf_entval *entry;

     if ( (entry = tree_find(cf, key)) == TREE_NOVAL) {
          return 0;
     } else {
	  nfree(key);
	  cf_entfree(entry);
     }

     return 1;
}

/* Frees data in entry structure */
void cf_entfree(struct cf_entval *entry)
{
     if (entry->vector) {
	  itree_clearoutandfree(entry->data.vec);
	  itree_destroy(entry->data.vec);
     } else {
	  nfree(entry->data.arg);
     }
     nfree(entry);
}


/*
 * Write a single directive line to the passed buffer
 * The format of the line will be one of the following:
 *
 *   TYPE       FOMRMAT
 *   scaler     <name> = <value>
 *   vector     <name> <value1> <value2> <value3> ...
 *   -1         <name>
 *   1          +<name>
 *
 * The last two represent scalers with values of -1 or 1, which are
 * assumed to be switches. A new line is appended.
 * Returns the number of characters written into the buffer or -1
 * if there was an error.
 */
int cf_directive(char *key, struct cf_entval *entry, char *buffer, 
		 int buflen)
{
     int len;
     char str[LINELEN];

     if (key == NULL || *key == '\0' || entry == NULL)
	  return -1;

     len = 0;
     if (entry->vector) {
	  len += snprintf(buffer, buflen, "%s ", 
			  util_quotestr (key, "\t", str, LINELEN));
	  itree_traverse( entry->data.vec )
	       len += snprintf(buffer + len, buflen - len, "%s ", 
			       util_quotestr (
				    (char *) itree_get(entry->data.vec),
				    "\t", str, LINELEN));
	  buffer[len-1] = '\n';		/* kill trailing space */
     } else {
	  if (strcmp(entry->data.arg, "-1") == 0)
	       len += snprintf(buffer, buflen, "%s\n", key);
	  else if (strcmp(entry->data.arg, "1") == 0)
	       len += snprintf(buffer, buflen, "+%s\n", key);
	  else
	       len += snprintf(buffer, buflen, "%s=%s\n", key, 
			       util_quotestr(entry->data.arg, "\t", str, 
					     LINELEN));
     }

     return len;
}



/*
 * Write the configuration table in cf to a route.
 * See cf_writetext() for formatting details of output text.
 * The route is not flushed or closed, that is down to the caller.
 * Returns the number of characters if successful or -1 for failure. .
 */
int     cf_writeroute(CF_VALS cf, char *magic, ROUTE route)
{
     char *buf;
     int r;

     buf = cf_writetext(cf, magic);
     if (buf) {
	  r = route_write(route, buf, strlen(buf));
	  nfree(buf);
     } else {
	  return -1;
     }

     return r;
}



/*
 * Write the configuration table present in cf into a text buffer and
 * return. The format of the file with values set to respectively 
 * a scaler, a vector, special value of -1 and special value of +1 (both 
 * flags) is as follows:-
 *
 *        <magic string if present>
 *        # Configuration file id text and date
 *        <name> = <value>
 *        <name> <value1> <value2> <value3> ...
 *        <name>
 *        +<name>
 *
 * If magic is non-NULL, then the magic string is written to the buffer 
 * first; if NULL, nothing will be written.
 * Following the magic line, a comment line is written containing a date
 * stamp. All lines (including the list) have newlines appended to them.
 * The buffer should be nfree()ed when finished.
 */
char *  cf_writetext(CF_VALS cf, char *magic)
{
     char *buf, *pt;
     int allocated, magiclen;

     /* work out the rough size of the buffer and allocate the space
      * based on the number of rows and a set guess of row size */
     if (magic)
	  magiclen = strlen(magic)+1;
     else
	  magiclen = 0;
     allocated = (tree_n(cf)+1) * CF_TEXTLINESIZE + magiclen;
     buf = xnmalloc(allocated);

     /* write magic and header */
     pt = buf;
     if (magic)
	  pt += snprintf(pt, allocated, "%s\n", magic);
     pt += snprintf(pt, allocated - magiclen,
		    "# Configuration file saved automatically: %s\n",
		    util_decdatetime( time(NULL) ) );

     /* iterate over the values and write text representations of them */
     tree_traverse(cf)
	  pt += cf_directive(tree_getkey(cf), tree_get(cf), pt, 
			     allocated - (pt - buf));

     return buf;
}


/*
 * This function updates a single line from an existing configuration route.
 * The route data is read and the line containing the key is replaced 
 * with one containing the current value if it has changed. The result
 * is saved back to the route. This is done with regular expressions.
 * The advantage with this is that comments and file structure are 
 * preserved, good for what is a hand edited file.
 * Will create a new file if one does not exist and uses magic
 * for the header line string. If magic is NULL, no magic string is provided.
 * If key does not exist in the config list, then the matching line
 * will be removed from the file.
 * Returns the number of characters if successful or -1 for failure.
 */
int     cf_updateline(CF_VALS cf, char *key, char *cfroute, char *magic)
{
     char directive[LINELEN], key_text[TOKLEN], errbuf[LINELEN];
     regex_t key_pattern;
     regmatch_t substr[1];
     char *buf, *newbuf, *npt;
     int len, newbuflen, dirlen, r=0;
     ROUTE rt;

     /* find entry */
     if (tree_find(cf, key) == TREE_NOVAL) {
	  /* remove line */
	  dirlen = 0;
     } else {
	  /* found: make the directive line to be inserted */
	  dirlen = cf_directive(tree_getkey(cf), tree_get(cf), directive, 
				LINELEN);
     }

     /* read the file into memory */
     buf = route_read(cfroute, NULL, &len);
     if (buf == NULL) {
	  /* no file */

	  if (dirlen == 0) {
	       /* no file & nothing to write */
	       newbuflen = 0;
	       newbuf = NULL;
	  } else {
	       /* no file, create a new one */
	       if (magic)
		    newbuf = util_strjoin(magic, "\n# Configuration file "
					  "saved automatically: ",
					  util_decdatetime( time(NULL) ), 
					  "\n", directive, NULL);
	       else
		    newbuf = util_strjoin("\n# Configuration file saved "
					  "automatically: ",
					  util_decdatetime( time(NULL) ), 
					  "\n", directive, NULL);
	       newbuflen = strlen(newbuf);
	  }
     } else {
	  /* found a file */

	  /* compile regular expression */
	  snprintf(key_text, TOKLEN, "[ \t-]*%s[ \t]*[ \t=]*.*\n", key);
	  if ((r = regcomp(&key_pattern, key_text, REG_NEWLINE))) {
	       regerror(r, &key_pattern, errbuf, LINELEN);
	       elog_printf(ERROR, "problem with key pattern: %s, error is %s", 
			   key_pattern, errbuf);
	       nfree(buf);
	       return -1;
	  }

	  /* find key pattern */
	  if (regexec(&key_pattern, buf, 1, substr, 0) == 0) {
	       /* found the key!! */
	       if (substr[0].rm_so == -1) {
		    /* error */
		    nfree(buf);
		    regfree(&key_pattern);
		    return -1;
	       } else {
		    /* patch line if changed */
		    newbuflen =   len 
			        - (substr[0].rm_eo - substr[0].rm_so) 
			        + dirlen;
		    npt = newbuf = nmalloc(newbuflen+1);
		    strncpy(npt, buf, substr[0].rm_so+1);
		    npt += substr[0].rm_so;
		    strncpy(npt, directive, dirlen+1);
		    npt += dirlen;
		    strncpy(npt, buf + substr[0].rm_eo, 
			    len - substr[0].rm_eo + 1);

		    nfree(buf);	/* remove old buffer */
	       }
	  } else {
	       /* key is not present in the file: append directive to buffer */
	       newbuflen = len + dirlen;
	       newbuf = xnrealloc(buf, newbuflen+1);
	       strncpy(newbuf + len, directive, dirlen+1);
	  }
	  /* clean up regexp */
	  regfree(&key_pattern);
     }

     /* save data back to route */
     if (newbuflen > 0) {
	  rt = route_open(cfroute, "user configuration", NULL, 10);
	  r = route_write(rt, newbuf, newbuflen);
	  route_close(rt);
     }

     /* clean up */
     nfree(newbuf);

     return r;
}



/*
 * Save selected keys from a config list (CF_VALS) to an existing config 
 * file by careful statement replacement. This preserves comments and 
 * other formatting that may exist, good for hand edited files.
 * Similar to cf_updateline() but works more efficently on many values.
 * Only the keys are used in the savekeys list.
 * 
 * The config data is read from its route and the lines containing keys 
 * in savekeys are replaced with the current values using regular expressions.
 * The result is saved back to the route.
 *
 * Will create a new config file if one does not exist and uses magic for 
 * the header line string. If magic is NULL, no magic string is provided 
 * in the saved route file.
 * If a key from savekeys does not exist in cf but does in the file, then 
 * the matching line will be removed from the file.
 * Returns the number of characters if successful or -1 for failure.
 */
int     cf_updatelines(CF_VALS cf, TREE *savekeys, char *cfroute, char *magic)
{
     char directive[LINELEN], key_text[TOKLEN], errbuf[LINELEN];
     regex_t key_pattern;
     regmatch_t substr[1];
     char *buf, *newbuf, *npt;
     ROUTE rt;
     CF_VALS new_cf;
     int len, newbuflen, dirlen, nchanges=0, r=0;

     if (tree_n(savekeys) == 0)
          return 0; /* empty list, not an error */

     /* read the file into memory or create a new one */
     buf = route_read(cfroute, NULL, &len);
     if (buf == NULL) {

          /* no existing file -- create a new one */
	  rt = route_open(cfroute, "user configuration", NULL, 10);
	  if (rt == NULL) {
	       elog_printf(ERROR, "unable to open %s to save configuration",
			   cfroute);
	       return -1;
	  }

	  /* if cf and savekeys are the same, then we can save time by
	   * writing it all. Also, you can't have both the same as they
	   * are stateful!! */
	  if (cf == savekeys) {
	       r = cf_writeroute(cf, magic, rt);
	  } else {
	       /* create a new config structure to feed cf_writeroute() */
	       new_cf = tree_create();
	       tree_traverse(savekeys) {
		    if (cf_defined(cf, tree_getkey(savekeys))) {
		         /* treat CF_VALS as a TREE* for simplicity, just copy
			  * the refs to key and value -- a shallow copy */
		         tree_find(cf, tree_getkey(savekeys));
			 tree_add(new_cf, tree_getkey(cf), tree_get(cf));
		    }
	       }

	       /* now save the new config structure */
	       r = cf_writeroute(new_cf, magic, rt);

	       /* clear up using a challow tree destructor as we dont want
		* a deep destroy to affect cf */
	       tree_destroy(new_cf);
	  }

	  /* close and return */
	  route_close(rt);
	  return r;
     }

     /* iterate over the savekeys list */
     tree_traverse(savekeys) {

          /* find entry from cf */
          if (cf != savekeys &&
	      tree_find(cf, tree_getkey(savekeys)) == TREE_NOVAL) {
	       /* no entry - remove line */
	       dirlen = 0;
	  } else {
	       /* found entry: make the directive line to be inserted */
	       dirlen = cf_directive(tree_getkey(cf), tree_get(cf), 
				     directive, LINELEN);
	  }

	  /* compile regular expression to search for the existing key */
	  snprintf(key_text, TOKLEN, "^[ \t-]*%s[ \t]*[ \t=]*.*\n", 
		   tree_getkey(savekeys));
	  if ((r = regcomp(&key_pattern, key_text, REG_NEWLINE))) {
	       regerror(r, &key_pattern, errbuf, LINELEN);
	       elog_printf(ERROR, "problem with key pattern: %s, error is %s", 
			   key_pattern, errbuf);
	       nfree(buf);
	       return -1;
	  }

	  /* find key pattern in the existing config buffer */
	  if (regexec(&key_pattern, buf, 1, substr, 0) == 0) {
	       /* found the key!! */
	       if (substr[0].rm_so == -1) {
		    /* error */
		    nfree(buf);
		    regfree(&key_pattern);
		    return -1;
	       } else {
		    /* patch line if changed */
		    newbuflen =   len 
			        - (substr[0].rm_eo - substr[0].rm_so) 
			        + dirlen;
		    npt = newbuf = nmalloc(newbuflen+1);
		    strncpy(npt, buf, substr[0].rm_so+1);
		    npt += substr[0].rm_so;
		    strncpy(npt, directive, dirlen+1);
		    npt += dirlen;
		    strncpy(npt, buf + substr[0].rm_eo, 
			    len - substr[0].rm_eo + 1);

		    nfree(buf);	/* remove old buffer */
		    nchanges++;
	       }
	  } else {
	       /* key is not present in the file to overwrite, so append 
		* directive to buffer */
	       newbuflen = len + dirlen;
	       newbuf = xnrealloc(buf, newbuflen+1);
	       strncpy(newbuf + len, directive, dirlen+1);
	       nchanges++;
	  }

	  /* clean up regexp & arrage buffers for next iteration */
	  regfree(&key_pattern);
	  buf = newbuf;
	  len = newbuflen;
     }

     /* save data back to route */
     if (nchanges) {
	  rt = route_open(cfroute, "user configuration", NULL, 10);
	  r = route_write(rt, buf, len);
	  route_close(rt);
     } else
          r = 0;

     /* clean up */
     nfree(buf);

     return r;
}



#if TEST

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "iiab.h"

char *cfdefaults = "debug -1 \n"
                   "nmalloc -1";
char *musthave1[] = {"jobs", "method", "results", "errors", "log", "loglevel",
		     "logkeep", NULL};
char *default1[]  = {"tom",	"42",
		     "dick",	"Mine's a double",
		     "harry",	"groovy",
		     NULL };
char *myargv1[]   = {"bollocks", "-t", "-he", "-lazy", "dog", "-j", "-umped",
		     "argument1", "argument2" };
char *checkargv1[]= {"t", "h", "e", "l", "a", "z", "y", "j", "u", "m", "p", 
		     "e", "d", "argc", "argv0", "argv1", "argv2", NULL};

#define FILENAME1 "t.cf.1.dat"
#define FILEPURL1 "file:t.cf.1.dat"
#define FILENAME2 "t.cf.2.dat"
#define FILEPURL2 "fileov:t.cf.2.dat"
#define FILEHEAD  "dispatchcf 0\n"
#define FILETEXT1 FILEHEAD "# A test \n" \
"jobs=file:job.dat \n" \
"method=./sh.so \n" \
"results=holstore:RES,%j \n" \
"#results=stdout \n" \
"errors=stderr \n" \
"log=file:LOG \n" \
"#log=stderr \n" \
"# main=1 disp=2 meth=4 q=8 job=16 cf=32 msg=64 tree=128 \n" \
"loglevel=17 \n" \
"logkeep=13 \n"
#define FILETEXT2 FILEHEAD "tom dick harry\nmary mungo midge"

int main(int argc, char **argv)
{
     TREE *cf1, *cf2, *cf3, *cf4, *cf5;
     ROUTE save;
     int fd, r, len;
     ITREE *args;
     TABLE tab;
     char *str;

     iiab_start("", argc, argv, "", cfdefaults);

     /* generate text files */
     unlink(FILENAME1);
     unlink(FILENAME2);
     fd = creat(FILENAME1, 0644);
     write(fd, FILETEXT1, strlen(FILETEXT1));
     close(fd);

     /* Scan, treating the magic number as a flag */
     cf1 = cf_create();
     r = cf_scanroute(cf1, NULL, FILEPURL1, CF_CAPITULATE);
     if (r == 0) {
	  fprintf(stderr, "test 1: unable to scan\n");
	  exit(1);
     }

     cf_default(cf1, default1);
     if (cf_getint(cf1, "tom") != 42) {
	  fprintf(stderr, "test 1a: tom != 42\n");
	  exit(1);
     }
     if (strcmp(cf_getstr(cf1, "dick"), "Mine's a double")) {
	  fprintf(stderr, "test 1b: dick not the same\n");
	  exit(1);
     }
     if (strcmp(cf_getstr(cf1, "harry"), "groovy")) {
	  fprintf(stderr, "test 1c: harry not groovy\n");
	  exit(1);
     }
     cf_destroy(cf1);

     /* As above, but passing a NULL magic number in a different way */
     cf2 = cf_create();
     r = cf_scanroute(cf2, "", FILEPURL1, CF_CAPITULATE);
     if (r == 0) {
	  fprintf(stderr, "test 2: unable to scan\n");
	  exit(1);
     }

     /* Do a check for values */
     if (!cf_check(cf2, musthave1)) {
	  fprintf(stderr, "test 2a: some keys are missing\n");
	  exit(1);
     }
     cf_destroy(cf2);

     /* Scan, but parsing the magic number */
     cf3 = cf_create();
     r = cf_scanroute(cf3, FILEHEAD, FILEPURL1, CF_CAPITULATE);
     if (r == 0) {
	  fprintf(stderr, "test 3: unable to scan\n");
	  exit(1);
     }
     /* Do a check for values again */
     if (!cf_check(cf3, musthave1)) {
	  fprintf(stderr, "test 2a: some keys are missing\n");
	  exit(1);
     }
     cf_destroy(cf3);

     fprintf(stderr, "You should see an error between the lines----------\n");
     cf4 = cf_create();
     r = cf_scanroute(cf4, "dispatchcf 1", FILEPURL1, CF_CAPITULATE);
     fprintf(stderr, "---------------------------------------------------\n");
     fprintf(stderr, "continuing...\n");
     if (r != 0) {
	  fprintf(stderr, "test 4: bad karma! should'nt be able to scan!\n");
	  exit(1);
     }
     cf_destroy(cf4);

     cf5 = cf_create();
     r = cf_cmd(cf5, "telazy:odgjmhpude", 9, myargv1, "Twat!");
     if (r == 0) {
	  fprintf(stderr, "test 5: command line not parsed\n");
	  exit(1);
     }
     if (!cf_check(cf5, checkargv1)) {
	  fprintf(stderr, "test 5a: command line does not check\n");
	  exit(1);
     }

     fprintf(stderr, "Example of one dump----------\n");
     cf_dump(cf5);
     cf_destroy(cf5);

     /* generate text file containing vectors */
     unlink(FILENAME1);
     fd = creat(FILENAME1, 0644);
     write(fd, FILETEXT2, strlen(FILETEXT2));
     close(fd);

     /* Scan, treating the magic number as a flag */
     cf1 = cf_create();
     r = cf_scanroute(cf1, NULL, FILEPURL1, CF_CAPITULATE);
     if (r== 0) {
	  fprintf(stderr, "test 6: unable to scan\n");
	  exit(1);
     }
     if ( ! cf_defined(cf1, "tom") ) {
	  fprintf(stderr, "test 6: tom does not exist\n");
	  exit(1);
     }
     if ( ! cf_defined(cf1, "mary") ) {
	  fprintf(stderr, "test 6: mary does not exist\n");
	  exit(1);
     }
     if ( ! cf_isvector(cf1, "tom") ) {
	  fprintf(stderr, "test 6: tom is not a vector\n");
	  exit(1);
     }
     if ( ! cf_isvector(cf1, "mary") ) {
	  fprintf(stderr, "test 6: mary is not a vector\n");
	  exit(1);
     }
     args = cf_getvec(cf1, "tom");
     if ( ! args ) {
	  fprintf(stderr, "test 6: tom does not return a vector\n");
	  exit(1);
     }
     if (strcmp(itree_find(args, 1), "dick")) {
	  fprintf(stderr, "test 6: dick arg 1 is wrong (%s)\n", 
		  (char *) itree_find(args, 1));
	  exit(1);
     }
     if (strcmp(itree_find(args, 2), "harry")) {
	  fprintf(stderr, "test 6: dick arg 2 is wrong (%s)\n", 
		  (char *) itree_find(args, 2));
	  exit(1);
     }
     args = cf_getvec(cf1, "mary");
     if ( ! args ) {
	  fprintf(stderr, "test 6: mary does not return a vector\n");
	  exit(1);
     }
     if (strcmp(itree_find(args, 1), "mungo")) {
	  fprintf(stderr, "test 6: mary arg 1 is wrong (%s)\n", 
		  (char *) itree_find(args, 1));
	  exit(1);
     }
     if (strcmp(itree_find(args, 2), "midge")) {
	  fprintf(stderr, "test 6: mary arg 2 is wrong (%s)\n", 
		  (char *) itree_find(args, 2));
	  exit(1);
     }

     /* test 7: print the status out */
     tab = cf_getstatus(cf1);
     str = table_printcols_a(tab, cf_colnames);
     printf("test 7:\n%s\n", str);
     table_destroy(tab);
     nfree(str);
     cf_destroy(cf1);

     /* test 8: rescan */
     cf1 = cf_create();
     r = cf_scanroute(cf1, FILEHEAD, FILEPURL1, CF_CAPITULATE);
     /* print the text representation out */
     str = cf_writetext(cf1, NULL);
     printf("%s\n", str);
     nfree(str);

     /* save the text representation as a file with no header & read back */
     save = route_open(FILEPURL2, "test 8", NULL, 10);
     cf_writeroute(cf1, NULL, save);
     route_close(save);
     str = route_read(FILEPURL2, NULL, &len);
     if (str == NULL) {
	  fprintf(stderr, "test 8: unable to save file\n");
	  exit(1);
     }

     /* update a value and patch file */
     cf_putstr(cf1, "mary", "dog mouse");
     if (cf_updateline(cf1, "mary", FILEPURL2, NULL) == -1) {
	  fprintf(stderr, "test 8: unable to patch file\n");
	  exit(1);
     }

     cf_destroy(cf1);
     nfree(str);

     iiab_stop();

     fprintf(stderr, "%s: tests finished\n", argv[0]);
     return 0;
}
#endif /* TEST */
