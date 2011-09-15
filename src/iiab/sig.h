/*
 * class to handle the signal requirements of the iiab library
 *
 * Nigel Stuckey, January 1998
 * Copyright System Garden Ltd 1996-2001. All rights reserved.
 */

#ifndef _SIG_H_
#define _SIG_H_

void sig_init();
void sig_setchild(void (*handler)(int));
void sig_setalarm(void (*handler)(int));
void sig_setexit(void (*handler)(int));
void sig_blocktty();
void sig_off();
void sig_on();

#endif /* _SIG_H_ */

