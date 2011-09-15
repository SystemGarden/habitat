/*
 * Graph conversion functions
 *
 * Nigel Stuckey, September 1999, refactored October 2010
 * Copyright System Garden Limited 1999-2001, 2010. All rights reserved.
 */

#ifndef _GCONV_H_
#define _GCONV_H_

#include "../iiab/table.h"
#include "graphdbox.h"

#if 0
int  gconv_resdat2arrays(GRAPHDBOX *g, RESDAT rdat, char *colname,
			 char *keycol, char *keyval, 
			 float **xvals, float **yvals);
#endif

int  gconv_table2arrays(GRAPHDBOX *g, TABLE rdat, 
			time_t oldest_t, time_t youngest_t,
			char *colname, char *keycol, char *keyval, 
			float **xvals, float **yvals);

#endif /* _GCONV_H_ */
