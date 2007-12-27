/*
 * Timeline class, which implements an adaptive timeline for a graph.
 * Given two dates, it will return a list containing the labels to draw 
 * and where they should be drawn, having worked out what scale it is 
 * reasonable to display at.
 *
 * Nigel Stuckey, 1999
 * Copyright System Garden Ltd 1999-2001. All rights reserved.
 */

#include <time.h>
#include "itree.h"

#define TIMELINE_SHORTSTR 20

enum timeline_units {NOUNIT, 
		     SECS, 
		     MINS, 
		     HOURS, 
		     WEEKDAYS, 
		     DAYS, 
		     MDAYS,
		     MONTHS, 
		     YEARS};

enum timeline_ticktype {TIMELINE_MAJOR, 
			TIMELINE_MINOR, 
			TIMELINE_NONE };

struct timeline_tick {
     enum timeline_ticktype type;
     char *label;
};

void   timeline_setoffset(time_t offset);
ITREE *timeline_calc(time_t min, time_t max, time_t dispdiff);
void   timeline_free(ITREE *);
void   timeline_rounddown(struct tm *t_tm, enum timeline_units);
