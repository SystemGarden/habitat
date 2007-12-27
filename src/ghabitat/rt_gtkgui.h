/*
 * Route driver for ghabitat and Gtk GUIS, where data is added to the GUI
 *
 * Nigel Stuckey, June 2004
 * Copyright System Garden Ltd 2004. All rights reserved
 */

#ifndef _RT_GTKGUI_H_
#define _RT_GTKGUI_H_

#include <time.h>
#include "../iiab/cf.h"
#include "../iiab/route.h"

/* General definitions */
#define RT_GTKGUI_LLD_MAGIC  1152194

extern const struct route_lowlevel rt_gtkgui_method;

int    rt_gtkgui_magic();
char * rt_gtkgui_prefix();
char * rt_gtkgui_description();
void   rt_gtkgui_init   (CF_VALS cf, int debug);
void   rt_gtkgui_fini   ();
int    rt_gtkgui_access (char *p_url, char *password, char *basename,int flag);
RT_LLD rt_gtkgui_open   (char *p_url, char *comment, char *password, int keep,
			 char *basename);
void   rt_gtkgui_close  (RT_LLD lld);
int    rt_gtkgui_write  (RT_LLD lld, const void *buf, int buflen);
int    rt_gtkgui_twrite (RT_LLD lld, TABLE tab);
int    rt_gtkgui_tell   (RT_LLD lld, int *seq, int *size, time_t *modt);
ITREE *rt_gtkgui_read   (RT_LLD lld, int seq, int offset);
TABLE  rt_gtkgui_tread  (RT_LLD lld, int seq, int offset);


#endif /* _RT_GTKGUI_H_ */
