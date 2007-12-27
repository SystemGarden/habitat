/*
 * iiab conversion routine
 *
 * This class contains code that converts foreign files to and from
 * native data formats.
 *
 * Nigel Stuckey, October 1999
 * Copyright System Garden Limited 1999-2001. All rights reserved
 */
#include "tablestore.h"

#define CONV_SARFILTER "| sed -e '/:/h' -e '/:/s/ .*//' -e '/:/x' " \
	"-e '/^        /G' -e '/^        /s/\\(.*\\)\\n\\(.*\\)/\\2\\1/' " \
	"-e '/proc-sz/,$s/\\/ */\\//g' -e '/proc-sz/,$s/\\/[^\t ]*//g' " \
	"-e 's/\t/ /g' -e 's/  */ /g' -e '/^ /s///' -e '/^$/d' " \
	"-e '/^Average/,$d' "
#define CONV_SARCMDS2D "ubycwaqmvpr"
#define CONV_SARCMDS3D "d"
#define CONV_SARCMDLEN 1024
#define CONV_TIMESTR "_time"
#define CONV_SEQSTR "_seq"
#define CONV_VALUESTR "value"
#define CONV_MAXROWS 10000

int conv_solsar2tab(char *sarfile, char *tabroute, char *ringprefix, 
		    char *fromdate, char *todate);
int conv_file2ring(char *infile, char *holname, int mode, char *ringname, 
		   char *description, char *password, int nslotsextra,
		   char *separator, int withcolnames, int hasruler,
		   int hastimecol, int hasseqcol, int hashostcol,
		   int hasringcol, int hasdurcol);
int conv_mem2ring(char *intext, char *holname, int mode, char *ringname, 
		  char *description, char *password, int nslotsextra,
		  char *separator, int withcolnames, int hasruler,
		  int hastimecol, int hasseqcol, int hashostcol,
		  int hasringcol, int hasdurcol);
int conv_ring2file(char *holname, char *ringname, char *password, 
		   char *outfile, char separator, int withtitle, 
		   int withruler, int withtimecol, char *dtformat,
		   int withseqcol, int withhostcol, int withringcol, 
		   int withdurcol, time_t from, time_t to);
char *conv_ring2mem(char *holname, char *ringname, char *password, 
		    char separator, int withcolnames, 
		    int withruler, int withtimecol, char *dtformat,
		    int withseqcol, int withhostcol, int withringcol, 
		    int withdurcol, time_t from, time_t to);
