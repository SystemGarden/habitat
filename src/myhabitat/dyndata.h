/*
 * MyHabitat dynamic data getter's
 *
 * Nigel Stuckey, January 2011
 * Copyright System Garden Ltd 2010-11. All rights reserved
 */

#ifndef _DYNDATA_H_
#define _DYNDATA_H_

#include <time.h>
#include "../iiab/table.h"

TABLE dyndata_config(time_t from, time_t to);

#endif /* _DYNDATA_H_ */
