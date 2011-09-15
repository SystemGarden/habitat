/*
 * IIAB Route driver for myhabitat and Gtk GUIS, primarily used for elog 
 * messages to be added to the GUI
 *
 * Nigel Stuckey, June 2004, May 2010
 * Copyright System Garden Ltd 2004,2010. All rights reserved
 */

#include "../iiab/cf.h"
#include "rt_gtkgui.h"
#include "uilog.h"

const struct route_lowlevel rt_gtkgui_method = {
     rt_gtkgui_magic,    rt_gtkgui_prefix,   rt_gtkgui_description,
     rt_gtkgui_init,     rt_gtkgui_fini,     rt_gtkgui_access,
     rt_gtkgui_open,     rt_gtkgui_close,    rt_gtkgui_write,
     rt_gtkgui_twrite,   rt_gtkgui_tell,     rt_gtkgui_read,
     rt_gtkgui_tread 
};

int    rt_gtkgui_magic()  { return RT_GTKGUI_LLD_MAGIC; }
char * rt_gtkgui_prefix() { return "gtkgui"; }
char * rt_gtkgui_description() { return "MyHabitat GTk+ graphical interface"; }
void   rt_gtkgui_init  (CF_VALS cf, int debug) {}
void   rt_gtkgui_fini  () {}
int    rt_gtkgui_access(char *p_url, char *password, char *basename, int flag)
                       {return 1;}
RT_LLD rt_gtkgui_open (char *p_url, char *comment, char *password, int keep,
		       char *basename)  {return (RT_LLD) 1;}
void   rt_gtkgui_close (RT_LLD lld) {}


/* send data to the gui */
int    rt_gtkgui_write (RT_LLD lld, const void *buf, int buflen)
{
     /* treat all information over this route as elog text and send to the
      * gtk callback for processing */
     uilog_elog_raise(buf, buflen);
     return buflen;
}


int    rt_gtkgui_twrite(RT_LLD lld, TABLE tab) {return 1;}
int    rt_gtkgui_tell  (RT_LLD lld, int *seq, int *size, time_t *modt)
                       {return 0;}
ITREE *rt_gtkgui_read  (RT_LLD lld, int seq, int offset) {return NULL;}
TABLE  rt_gtkgui_tread (RT_LLD lld, int seq, int offset) {return NULL;}

