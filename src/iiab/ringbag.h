/*
 * Class to manipulate and manage a pool of timestore rings.
 *
 * Nigel Stuckey, March 1998
 * Copyright System Garden Limitied 1998-2001. All rights reserved.
 */

#ifndef _RINGBAG_H_
#define _RINGBAG_H_

#include "timestore.h"
#include "tree.h"

#define RINGBAG_MGETBATCH 200

struct ringbag_ringent {
     char *tsname;
     char *ringname;
     char *description;
     char *password;
     int   seen;
     int   available;
     ITREE *summary;	/* list of strings indexed by sequence */
};

void ringbag_init();
void ringbag_fini();
int  ringbag_addts(char *tsname);
int  ringbag_rmts(char *tsname);
int  ringbag_rmallts();
void ringbag_rmallrings();
int  ringbag_getallrings();
int  ringbag_setring(char *compound, char *password);
void ringbag_unsetring();
int  ringbag_scan(int beforescope, int afterscope, int seq, 
		  char * (*summaryfunc)(ntsbuf *));
int  ringbag_update(int maxkeep, char * (*summaryfunc)(ntsbuf *));
int  ringbag_firstseq();
int  ringbag_lastseq();
struct ringbag_ringent *ringbag_getents();
TS_RING ringbag_getts();
TREE *ringbag_getrings();
TREE *ringbag_gettsnames();

#endif /* _RINGBAG_H_ */
