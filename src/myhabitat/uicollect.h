/*
 * GHabitat collection control and GUI elements for start, stop and status
 *
 * Nigel Stuckey, May 2010
 * Copyright System Garden Ltd 2010. All rights reserved
 */

#ifndef _UICOLLECT_H_
#define _UICOLLECT_H_

void uicollect_askclockwork();
void uicollect_startclockwork();
void uicollect_stopclockwork();
void uicollect_showstopclockwork();
int  uicollect_is_clockwork_running(char **key, char **user, char **tty, 
				    char **datestr, int giveerror);
int  uicollect_is_clockwork_runable();
void uicollect_set_status_not_collecting();
void uicollect_set_status_collecting();


#endif /* _UICOLLECT_H_ */
