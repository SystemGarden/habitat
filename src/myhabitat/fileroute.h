/*
 * Fileroute, part of MyHabitat
 * A layer over some aspects of the route system to copy with file formats
 * encoded in a ROUTE.
 *
 * Nigel Stuckey August 2011
 * Copyright System Garden Ltd, 2011. All rights reserved.
 */

#ifndef _FILEROUTE_H_
#define _FILEROUTE_H_

#include "../iiab/table.h"

/* File type definitions */
enum  fileroute_file_type {
     FILEROUTE_TYPE_UNKNOWN,
     FILEROUTE_TYPE_GRS,
     FILEROUTE_TYPE_RS,
     FILEROUTE_TYPE_CSV,
     FILEROUTE_TYPE_TSV,
     FILEROUTE_TYPE_SSV,
     FILEROUTE_TYPE_PSV,
     FILEROUTE_TYPE_FHA,
     FILEROUTE_TYPE_TEXT
};

typedef enum fileroute_file_type FILEROUTE_TYPE;

TABLE fileroute_tread(char *purl, FILEROUTE_TYPE type);


#endif /* _FILEROUTE_H_ */
