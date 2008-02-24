/*
 * General purpose utilities
 *
 * Nigel Stuckey, May 1998
 * Copyright System Garden Limited 1998-2001. All rights reserved
 */

#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "nmalloc.h"
#include "itree.h"
#include "route.h"
#include "elog.h"
#include "util.h"

/* ----- Text parsing and utility functions ----- */
/*
 * Parse the input route and return a vector of vectors: in this case, 
 * an ITREE of ITREEs. see util_parsetext() for details.
 * Returns the number of lines that contain data (not comments, not blanks), 
 * 0 for empty data or -1 if there was an error.
 */
int util_parseroute(char *route,	/* pseudo-url of input route */
		    char *sep,		/* separator characters */
		    char *magic,	/* magic string */
		    ITREE **retbuf	/* return: list of lists */ )
{
     char *text;
     int buflen, r;

     /* Read the configuration */
     text = route_read(route, NULL, &buflen);
     if (text == NULL || buflen == 0 ) {
          elog_printf(DIAG, "no data in %s", route);
	  return 0;
     }

     r = util_parsetext(text, sep, magic, retbuf);
     nfree(text);
     return r;
}


/* Parse a string buffer with quotes and comments and output a list of lists.
 * Outputs an ITREE in `retbuf', representing lines and indexed by line number.
 * The line data is a token list, indexed by token number. Each token data 
 * element contains a copy of a parsed input token, having been seperated 
 * by characters in `sep' string.
 * The line and word order is preserved, but each token now has a unique
 * index pair to reference it.
 * Comments are not parsed, not placed in the token lists, are 
 * introduced with '#' and terminated with a newline. Thus '#' is
 * special and you may not use it in tokens.
 * Quoted strings are handled, introduced by using the /"/ character if 
 * it immedeatly follows whitespace and is terminated either by
 * a /"/ followed immedeatly by whitespace or the end of line.
 * Multiline strings are not currently allowed [CHECK, I THINK IT IS NOW], 
 * but the string may contain the usual escape sequencies.
 * The magic string is a line at the start of the text buffer that must
 * match or -1 is returned and an elog ERROR is generated. Magic may be
 * NULL in which case no magic line is checked.
 * Use util_freeparse() to free the storage.
 * Returns the number of lines that contain data (not comments, not blanks), 
 * 0 for empty data or -1 if there was an error.
 */
int util_parsetext(char *buf,		/* null terminated buffer to parse */
		   char *sep,		/* separator characters */
		   char *magic,		/* magic string */
		   ITREE **retbuf	/* return: list of lists */ )
{
     int toksz, sepsz, j, i, ln=0, col=0, ncols=0;
     char *remain, gap[40], *errtxt, *tok;
     ITREE *lines, *cols;

     /* setup */
     strcpy(gap, sep);
     strcat(gap, "\n#");
     remain = buf;

     /*
      * Find magic line.
      * It is allowed to have trailing information as part of the line,
      * so we only check that the line begins with the string magic,
      * and then start processing after the \n.
      */
     if (magic == NULL)
	  j = 0;
     else
	  j = strlen(magic);
     if (j != 0) {
	  i = strncmp(remain, magic, j);
	  if (i != 0) {
	       /* no magic: error */
	       errtxt = xnmemdup(remain, j+1);
	       *(errtxt+j) = '\0';
	       elog_printf(ERROR, "magic mismatch: want %s got: %s", 
			   magic, errtxt);
	       nfree(errtxt);
	       return -1;
	  }
	  remain += strcspn(remain, "\n")+1;	/* Next line */
     }

     /* Create tree for lines */
     lines = itree_create();
     cols = itree_create();

     /* iterate over the entire buffer */
     while (*remain != '\0') {
          /* eat whitespace */
          if ( (sepsz = strspn(remain, sep)) ) {
	       remain += sepsz;
	       continue;
	  }

	  /* eat quoted token */
	  if (*remain == '"') {
	       remain++;
	       toksz = strcspn(remain, "\"");
	       tok = xnmemdup(remain, toksz+1);
	       *(tok+toksz) = '\0';
	       itree_append(cols, tok);
	       col++;
	       remain += toksz;
	       if (*remain == '"')
		   remain++;
	       continue;
	  }

	  /* eat token */
	  if ( (toksz = strcspn(remain, gap)) ) {
	       tok = xnmemdup(remain, toksz+1);
	       *(tok+toksz) = '\0';
	       itree_append(cols, tok);
	       col++;
	       remain += toksz;
	       continue;
	  }
 
	  /* eat end of line */
	  if (*remain == '\n') {
	       if (ln == 0)
		    ncols = col;
	       if (col) {
		    /* if columns have been recorded, save them in the
		     * line list */
		    itree_append(lines, cols);
		    cols = itree_create();
	       }
	       col = 0;
	       ln++;
	       remain++;
	       continue;
	  }

	  /* eat comment line (leaving \n) */
	  if (*remain == '#') {
	       remain += strcspn(remain, "\n");
	       continue;
	  }
     }

     if (ln == 0)
          ncols = col;

     /* close off lists */
     if (col)
          itree_append(lines, cols);	/* final line without \n */
     else {
          itree_destroy(cols);
	  cols = NULL;
     }
     ln = itree_n(lines);
     if (ln == 0) {
          itree_destroy(lines);
	  lines = NULL;
     }

     /* make returns */
     *retbuf = lines;
     return ln;
}

/* Destroy the parse storage returned by util_parseroute() */
void util_freeparse(ITREE *lol) {
     ITREE *collist;

     if ( ! lol )
          return;

     while ( ! itree_empty(lol)) {
          itree_first(lol);
          collist = itree_get(lol);
	  while ( ! itree_empty(collist)) {
	       itree_first(collist);
	       if (itree_get(collist))
		    nfree( itree_get(collist) );
	       itree_rm(collist);
	  }
	  itree_destroy(collist);
          itree_rm(lol);
     }
     itree_destroy(lol);
}

/* Destroy the parse storage returned by util_parseroute(), but leave the
 * data */
void util_freeparse_leavedata(ITREE *lol) {
     if ( ! lol )
          return;

     itree_traverse(lol)
          itree_destroy(itree_get(lol));
     itree_destroy(lol);
}

void util_parsedump(ITREE *buffer	/* parsed data */)
{
     ITREE *wordlist;
     int i,j;

     if (buffer == NULL) {
	  elog_send(DEBUG, "Parse list empty");
	  return;
     }

     elog_startprintf(DEBUG, "Dump of parse list: `%x' -----\n", buffer);

     /* Traverse tree (tree has state, which allows us to do this */
     i = 1;
     itree_traverse(buffer) {
          elog_contprintf(DEBUG, "(l%d): ", i++);
	  wordlist = itree_get(buffer);
	  j = 1;
	  itree_traverse(wordlist)
	       elog_contprintf(DEBUG, "(w%d) %s ", j++, itree_get(wordlist));
	  elog_contprintf(DEBUG, "\n");
     }

     elog_endprintf(DEBUG, "End of parse list ----------");

     return;
}


/*
 * Scans a buffer of text using configuration scanning rules (see below).
 * The buffer should be writable and will be altered to patch in nulls
 * after string tokens.
 * The result of the scan is a list of lists (read rows of word tokens)
 * which should be freed by util_scanfree().
 *
 * Optionally check a magic string at the start of the buffer and
 * if the match fails, return -1. If it matches, the string scanning starts
 * from the begining of the next line.
 * Magic may be NULL if no magic line checking is required.
 *
 * String seperators are specified by the arg string `sep', unless they
 * are contained in quotes ("), in which case the seperators are treated 
 * as part of the string. Closing quotes should have white space following,
 * otherwise it is considered part of the string. End of lines will also 
 * terminate strings. Non ascii characters (up to 8 bit) may be part of
 * the string by using standard escaping rules [introduced with backslash (\)].
 * Comments are introduced with hash (#) and are terminated by the end of line.
 *
 * The list of lists is returned is an ITREE, which is an ordered list 
 * of indexes for each line, keys start from 0.
 * Each line index is another ITREE, which indexes the discovered strings
 * in that line, keys start from 0.
 * Comments are not included in the index.
 * Lines and word/string ordering is maintained.
 *
 * Use util_scanfree() to free the storage.
 * Returns the number of lines that contain data (not comments, not blanks), 
 * 0 for empty data or -1 if there was an error. For 0 or -1 returns, 
 * retindex will return NULL.
 */
int util_scancftext(char *buf,		/* null terminated buffer to parse */
		    char *sep,		/* separator characters */
		    char *magic,	/* magic string */
		    ITREE **retindex	/* return: index list of lists */ )
{
     int toksz, sepsz, j, i, ln=0, col=0, ncols=0, isescaped;
     char *remain, gap[40], *errtxt, *tok;
     ITREE *lines, *cols;

     if (buf == NULL)
	  return -1;

     /* setup */
     strcpy(gap, sep);
     strcat(gap, "\n#");
     remain = buf;

     /*
      * Find magic line.
      * It is allowed to have trailing information as part of the line,
      * so we only check that the line begins with the string magic,
      * and then start processing after the \n.
      */
     if (magic == NULL)
	  j = 0;
     else
	  j = strlen(magic);
     if (j != 0) {
	  i = strncmp(remain, magic, j);
	  if (i != 0) {
	       /* no magic: error */
	       errtxt = xnmemdup(remain, j+1);
	       *(errtxt+j) = '\0';
	       elog_printf(ERROR, "magic mismatch: want %s got: %s", magic, 
			   errtxt);
	       nfree(errtxt);
	       return -1;
	  }
	  remain += strcspn(remain, "\n")+1;	/* Next line */
     }

     /* Create tree for lines */
     lines = itree_create();
     cols = itree_create();

     /* iterate over the entire buffer */
     while (*remain != '\0') {
          /* eat whitespace */
          if ( (sepsz = strspn(remain, sep)) ) {
	       memset(remain, 0, sepsz);	/* fill with nulls */
	       remain += sepsz;
	       continue;
	  }

	  /* eat quoted token */
	  if (*remain == '"') {
	       isescaped = 0;
	       remain++;
	       toksz = strcspn(remain, "\"");

	       /* save and patch with null */
	       itree_append(cols, remain);
	       col++;
	       tok = remain;
	       remain += toksz;
	       if (*remain == '"') {
		   *remain = '\0';		/* patch with null */
		   remain++;
	       }
	       /* transform any escaped quotes according to the unescape
		* table above */
	       while ( *tok != '\0' ) {
		    i = strcspn(tok, UTIL_ESCQUOTES);
		    if (i == 0)
			 break;
		    tok += i;
		    if (*tok == '\'')
			 *(tok++) = '"';
		    if (*tok == '\1')
			 *(tok++) = '\'';
		    if (*tok == '\2')
			 *(tok++) = '\1';
	       }
	       continue;
	  }

	  /* eat token */
	  if ( (toksz = strcspn(remain, gap)) ) {
	       itree_append(cols, remain);
	       col++;
	       remain += toksz;
	       continue;
	  }
 
	  /* eat end of line */
	  if (*remain == '\n') {
	       *remain = '\0';			/* patch with null */
	       if (ln == 0)
		    ncols = col;
	       if (col) {
		    /* if columns have been recorded, save them in the
		     * line list */
		    itree_append(lines, cols);
		    cols = itree_create();
	       }
	       col = 0;
	       ln++;
	       remain++;
	       continue;
	  }

	  /* eat comment line (leaving \n) */
	  if (*remain == '#') {
	       /* problem here if we want to keep comments */
	       *remain = '\0';			/* patch with null */
	       remain++;
	       remain += strcspn(remain, "\n");
	       continue;
	  }
     }

     if (ln == 0)
          ncols = col;

     /* close off lists */
     if (col)
          itree_append(lines, cols);	/* final line without \n */
     else {
          itree_destroy(cols);
	  cols = NULL;
     }
     ln = itree_n(lines);
     if (ln == 0) {
          itree_destroy(lines);
	  lines = NULL;
     }

     /* make returns */
     *retindex = lines;
     return ln;
}


/*
 * Scans a buffer of ASCII text, patches nulls into the buffer to 
 * terminate the strings that it finds and returns a structure that 
 * indexes the strings.
 * The index should be freed by util_scanfree().
 *
 * String seperators are specified by the arg string `sep', unless they
 * are contained in quotes ("), in which case the seperators are treated 
 * as part of the string. Quoted strings may also contain escaped 
 * charaters using standard C notation, which are not valid outside quotes.
 * For the quote character ("), use \". For a newline, use \n.
 * For non-ascii characters (up to 8 bit) use \yyy to specify the
 * octal code or \0xww for the hex code.
 * Multiple separators can be flagged to be treated as one.
 *
 * The index returned is an ITREE, which is an ordered list of indexes for 
 * each line, keys start from 0.
 * Each line index is another ITREE, which indexes the discovered strings
 * in that line, keys start from 0.
 * Comments are not supported, use util_scancftext() for that.
 * Lines and word/string ordering is maintained.
 *
 * Use util_scanfree() to free the storage.
 * Returns the number of lines that contain data (not comments, not blanks), 
 * 0 for empty data or -1 if there was an error. For 0 or -1 returns, 
 * retindex will return NULL.
 */
int util_scantext(char *buf,		/* null terminated buffer to parse */
		  char *sep,		/* separator characters */
		  int multisep,		/* treat multiple separators as one */
		  ITREE **retindex	/* return: index list of lists */ )
{
     int toksz, sepsz, i, eaten_sep, ln=0, col=0, ncols=0;
     char *remain, gap[40], *tok, *fmt;
     ITREE *lines, *cols;

     if (buf == NULL)
	  return -1;

     /* setup */
     strcpy(gap, sep);
     strcat(gap, "\n");
     remain = buf;

     /* Create tree for lines */
     lines = itree_create();
     cols = itree_create();

     /* iterate over the entire buffer */
     while (*remain != '\0') {

	  /* eat end of line */
	  if (*remain == '\n') {
	       /* If a delimiter preceeds the end of line, we need to 
	        * register an empty cell */
	       if ((!multisep) && eaten_sep) {
		    itree_append(cols, "");
		    col++;
	       }
	       *remain = '\0';			/* patch with null */
	       if (ln == 0)
		    ncols = col;
	       if (col) {
		    /* if columns have been recorded, save them in the
		     * line list */
		    itree_append(lines, cols);
		    cols = itree_create();
	       }
	       col = 0;
	       ln++;
	       remain++;
	       eaten_sep = 0;
	       continue;
	  }

	  /* eat quoted token */
	  if (*remain == '"') {
	       remain++;

	       /* find the terminating quote */
	       toksz = strcspn(remain, "\"");

	       /* save and patch with null */
	       remain[toksz] = '\0';
	       tok = remain;
	       itree_append(cols, remain);
	       remain += toksz +1;
	       col++;

	       /* translate special characters back to normal */
	       for (; *tok; tok++) {
		    if (*tok == '\001')
			 *tok = '"';
		    else if (*tok == '\002')
			 *tok = '\n';
	       }
	       
	       eaten_sep = 0;
	       continue;
	  }

	  /* eat unquoted token */
	  if ( (toksz = strcspn(remain, gap)) ) {
	       itree_append(cols, remain);
	       col++;
	       remain += toksz;
	       eaten_sep = 0;
	       continue;
	  }

          /* eat separators */
          if ( (sepsz = strspn(remain, sep)) ) {
	       memset(remain, 0, sepsz);	/* fill with nulls */
	       remain += sepsz;
	       if (!multisep) {
		    /* if multiple separators in single sep mode, they
		     * represent empty fields and thus inc the col counter 
		     * and push an empty field into the col list.
		     * If there is a leading value, the first separator
		     * doesn't count. */
		    for (i=(col ? 1 : 0); i < sepsz; i++) {
		         itree_append(cols, "");
			 col++;
		    }
	       }
	       eaten_sep = 1;
	  }
     }

     if (ln == 0)
          ncols = col;

     /* close off lists */
     if (col)
          itree_append(lines, cols);	/* final line without \n */
     else {
          itree_destroy(cols);
	  cols = NULL;
     }
     ln = itree_n(lines);
     if (ln == 0) {
          itree_destroy(lines);
	  lines = NULL;
     }

     /* make returns */
     *retindex = lines;
     return ln;
}


/*
 * Read input route, scan it using util_scantext() and return an ITREE
 * structure containing the line indexes.
 * See util_scantext() for details.
 * Use util_freescan() to free the index structure.
 * Returns the number of lines that contain data (not comments, not blanks), 
 * 0 for empty data or -1 if there was an error.
 */
int util_scancfroute(char *route,	/* pseudo-url of input route */
		     char *sep,		/* separator characters */
		     char *magic,	/* magic string */
		     ITREE **retindex,	/* return: index list of lists */
		     char **retbuffer	/* return: data buffer */ )
{
     char *text;
     int buflen, r;

     /* Read the configuration */
     text = route_read(route, NULL, &buflen);
     if (text == NULL || buflen == 0 ) {
          elog_printf(INFO, "no data in %s", route);
	  return 0;
     }

     r = util_scancftext(text, sep, magic, retindex);
     *retbuffer = text;
     return r;
}


/* Free the structure created by util_scantext(); don't touch the data */
void   util_scanfree(ITREE *index	/* scanned index list of lists */ )
{
     itree_traverse(index)
          itree_destroy( itree_get(index) );
     itree_destroy(index);
}

/* Dump the contents refered to by SCANBUF to elog's DEBUG route */
void   util_scandump(ITREE *index	/* scanned index list of lists */ )
{
     ITREE *wordlist;
     int i,j;

     elog_startsend(DEBUG, "Dump of scan list ---------");

     /* Traverse tree (tree has state, which allows us to do this) */
     i = 1;
     itree_traverse(index) {
          elog_contprintf(DEBUG, "(l%d): ", i++);
	  wordlist = itree_get(index);
	  j = 1;
	  itree_traverse(wordlist)
	       elog_contprintf(DEBUG, "(w%d) %s ", j++, itree_get(wordlist));
	  elog_contprintf(DEBUG, "\n");
     }

     elog_endprintf(DEBUG, "End of scan list ----------");

     return;
}

/* ----- String utilities ----- */

/* Return a string in quotes, escaping any quote characters in string.
 * For performance, escaped quote characters are represented by 
 * one of sevaral single characters, depending on the depth of the escape. 
 * The following table shows the transformation sequence that takes place.
 *
 *        " => '    implemented
 *        ' => 0x1  implemented
 *      0x1 => 0x2  implemented
 *      0x2 => 0x3  not yet implemented
 *      0x3 => 0x4  not yet implemented
 *      0x4 => 0x4  not yet implemented
 *
 * Transformations not implemented are due to performance reasons
 */
char *util_escapestr(char *s) {
     static char buf[UTIL_ESCSTRLEN];
     char *spn, *bufpt;
     int i;

     /* write spans of charcters until the end of string.
      * Each span is a sub string of charcters terminated by \0 or a quote */
     spn = s;
     bufpt = buf;

     *(bufpt++) = '"';
     /* copy span of data, which may have been quoted before */
     while ( *spn != '\0' ) {
	  i = strcspn(spn, UTIL_ESCQUOTES);
/*	  if (i == 0)
	  break;*/
	  if (bufpt+i-buf > UTIL_ESCSTRLEN-5) {
	       elog_printf(ERROR, "string too big to be escaped "
			   "(unescaped %d bytes, max escaped %d)", strlen(s), 
			   UTIL_ESCSTRLEN);
	       break;		/* failure, but return what we have */
	  }
	  strncpy(bufpt, spn, i);
	  bufpt += i;
	  spn += i;

	  /* emit escaped quote if found, depending on level */
	  if (*spn == '"') {
	       *(bufpt++) = '\'';
	       spn++;
	  }
	  if (*spn == '\'') {
	       *(bufpt++) = 0x1;
	       spn++;
	  }
	  if (*spn == '\1') {
	       *(bufpt++) = 0x2;
	       spn++;
	  }
     }

     *(bufpt++) = '"';
     *bufpt = '\0';
     return buf;
}


/*
 * Return a string which will always be a scanned as a single token.
 * Empty strings will be returned as "" or -, strings with spaces in will
 * be surrounded by quotes and strings with previous quotes will be have
 * them transformed in to different quote styles, which may not be printable.
 */
char *util_strtoken(char *s)
{
     char *pt;

     if (s == NULL || *s == '\0')
	  return "-";
     for (pt=s; *pt; pt++)
	  if (  isspace(*pt) || *pt == '"' )
	       return util_escapestr(s);
     return s;
}


/* Copy 'str' to 'buf' and return the address. If 'str' contains any 
 * character from the string 'escape', then put quotes around it.
 * If there is insufficient space, return the string "(too big)",
 * which will not be placed in the buffer, thus allowing an error check,
 * although the buffer will be partially used.
 * Quotes and newlines are escaped if the string becomes quoted to 
 * allow for string embedding in text on a single line. 
 * This is an encoding that can be reversed using util_scantext()
 */
char  *util_quotestr(char *str, char *escape, char *buf, int buflen)
{
     char *bufpt, *strpt, *bufendpt;
     int len, slen;

     /* match empty strings */
     if (str == NULL || *str == '\0')
	  return strcpy(buf, "\"\"");

     slen = strlen(str);
     if (buflen < slen+3)
	  return "(too big)";

     /* detect escaped characters */
     if (strpbrk(str, escape)) {
	  *buf = '"';
	  strcpy(buf+1, str);
	  for (bufpt = buf+1; *bufpt; bufpt++) {
	       if (*bufpt == '"')
		    *bufpt = '\001';
	       else if (*bufpt == '\n')
		    *bufpt = '\002';
	  }
	  *bufpt++ = '"';
	  *bufpt = '\0';
     } else {
	  /* no escaped characters in string */
	  if (slen+1 > buflen)
	       return "(too big)";
	  strcpy(buf, str);
     }

     return buf;
}


/* If the string has spaces in it, put quotes around it and return 
 * an nmalloc'ed string with the quotes, otherwise return a copy of
 * the input string. Free storage in every case with nfree().
 */
char  *util_mquotestr(char *str)
{
     char *retstr, *pt;

     if (str == NULL || *str == '\0')
          return xnstrdup("\"\"");

     for (pt = str; *pt; pt++)
	  if (isspace(*pt)) {
	       /* space found in the string */
	       retstr = xnmalloc(strlen(str)+3);
	       sprintf(retstr, "\"%s\"", str);
	       return retstr;
	  }
     return xnstrdup(str);
}


/* Append a character to the end of the result string if it fits.  */
#define btsappend(c)	(void)((m < max-1) && (str[m++] = (c)))

/*
 * Convert a block of binary data into a string, using C string escape 
 * notation to represent non-printable characters.
 * Specify the input binary data with binblock and its size with n.
 * Returns an nmalloc()ed string, of no greater size than max including \0.
 * Free with nmfree().
 */
char  *util_bintostr(size_t max,		/* max string returned */
		     const void *binblock,	/* binary data */
		     size_t n			/* size of binary data */)
{
     static const char print[] = "'\"?\\abfnrtv";
     static const char unprint[] = "'\"?\\\a\b\f\n\r\t\v";
     int i, thishex, prevhex;
     const char *p, *data;
     size_t m;
     char *str;

     if (max <= 0)
          elog_die(FATAL, "max <= 0");
     if (binblock == NULL)
          elog_die(FATAL, "binblock == NULL");

     str = nmalloc(max);
     data = binblock;
     m = 0;
     prevhex = 0;
     while (m < max-1 && n-- > 0) {
          thishex = 0;
	     if (*data == '\0') {
	          btsappend('\\');
		  btsappend('0');
		  if (isdigit(data[1])) {
		       btsappend('0');
		       btsappend('0');
		  }
		  ++data;
	     } else if ((p = strchr(unprint, *data)) != NULL) {
	          btsappend('\\');
		  btsappend(print[p-unprint]);
		  ++data;
	     } else if (isprint(*data) && !(prevhex && isxdigit(*data)))
	          btsappend(*data++);
	     else {
	          btsappend('\\');
		  btsappend('x');
		  i = (CHAR_BIT/4+1)*4-4;
		  while (m < max-1 && i >= 0) {
		       btsappend("0123456789abcdef"[(*data & (0xf <<i)) >> i]);
		       i -= 4;
		  }
		  thishex = 1;
		  ++data;
	     }
	     prevhex = thishex;
     }
     btsappend('\0');

     return str;
}

/* Append a character to the end of the data block, if it fits. */
#define stbappend(c)	(void)(m < max && (data[m++] = (c)))

/* Convert hexadecimal character to its corresponding integer value.
   Assume that the character is valid.  */
#define hextoint(c)	hexvalues[strchr(hexdigits, (c))-hexdigits];

/*
 * Convert a string containing C string escape sequencies into binary data.
 * The destination and the maximum size is specified by binblock and max.
 * Returns the number of bytes converted from string & written into 
 * binblock.
 */
int util_strtobin(void *binblock,	/* binary data */
		  const char *str, 	/* string */
		  size_t max		/* max bytes to convert */ )
{
     static const char print[] = "'\"?\\abfnrtv";
     static const char unprint[] = "'\"?\\\a\b\f\n\r\t\v";
     static const char hexdigits[] = "0123456789abcdefABCDEF";
     static const char hexvalues[] = "\0\1\2\3\4\5\6\7\x8\x9"
					"\xa\xb\xc\xd\xe\xf"
					"\xA\xB\xC\xD\xE\xF";
     char *p, *data;
     unsigned c;
     size_t m;
     int i;

     if (str == NULL)
          elog_die(FATAL, "str == NULL");
     if (binblock == NULL)
          elog_die(FATAL, "binblock == NULL");

     data = binblock;
     m = 0;
     while (m < max && *str != '\0') {
          if (*str != '\\')		/* printable character? */
	       stbappend(*str++);
	  else if (str[1] == 'x') {	/* hex escape sequence? */
	       str += 2;
	       c = 0;
	       while (isxdigit(*str)) {
		    c = (c << 4) | hextoint(*str);
		    ++str;
	       }
	       stbappend(c);
	  } else if (isdigit(str[1])) {	/* octal escape sequence? */
	       ++str;
	       c = i = 0;
	       while (++i < 3 && isdigit(*str))
		    c = (c << 3) | hextoint(*str++);
	       stbappend(c);
	  } else if ((p = strchr(print, str[1])) != NULL) {
	       stbappend(unprint[p-print]); /* simple esc sequence */
	       str += 2;
	  } else {			/* undefined sequence! */
	       stbappend('\\');
	       stbappend(str[1]);
	       str += 2;
	  }
     }

     return m;
}


/* Remove characters from the begining of the string */
char *util_strdel(char *s, size_t n) {
     size_t len;

     if (s == NULL)
	  elog_die(FATAL, "s == NULL");

     len = strlen(s);
     if (n > len)
	  n = len;
     memmove(s, s+n, len+1 - n);
     return s;
}


/* Remove trailing whitespace from a string */
char *util_strrtrim(char *s) {
     char *t, *tt;

     if (s == NULL)
          elog_die(FATAL, "s == NULL");

     for (tt = t = s; *t != '\0'; ++t)
          if (!isspace(*t))
	       tt = t+1;
     *tt = '\0';

     return s;
}


/* Remove leading whitespace from string */
char *util_strltrim(char *s) {
	char *t;

     if (s == NULL)
          elog_die(FATAL, "s == NULL");
     for (t = s; isspace(*t); ++t)
          continue;
     memmove(s, t, strlen(t)+1);	/* +1 so that '\0' is moved too */
     return s;
}


/* Remove leading and trailing blanks from string */
char *util_strtrim(char *s) {
     if (s == NULL)
          elog_die(FATAL, "s == NULL");

     util_strrtrim(s);
     util_strltrim(s);
     return s;
}


/*
 * Substitute all occurences of pattern with another string.
 * If there isn't enough space for the substitutions, then -1
 * is returned.
 */
int util_strgsub(char *str, 		/* string */
		 const char *pat, 	/* pattern to find */
		 const char *sub, 	/* substring to substitute */
		 size_t max		/* maximum length string can grow */)
{
     size_t lenpat, lensub;
     const char *p;
     int n;

     if (str == NULL)
          elog_die(FATAL, "str == NULL");
     if (pat == NULL)
          elog_die(FATAL, "pat == NULL");
     if (*pat == '\0')
          elog_die(FATAL, "*pat == '\0'");
     if (sub == NULL)
          elog_die(FATAL, "sub == NULL");
     if (max < strlen(str)+1)
          elog_die(FATAL, "max < strlen(str)+1");

     /*
      * Check that the all substitutions will fit.
      */
     lenpat = strlen(pat);
     lensub = strlen(sub);
     if (lenpat < lensub) {
	  for (n = 0, p = str; (p = strstr(p, pat)) != NULL; p += lenpat)
	       ++n;
	  if (strlen(str)+1 + n*(lensub-lenpat) > max)
	       return -1;
     }

     /*
      * Substitute.
      */
     for (n = 0; (str = util_strsub(str, pat, sub)) != NULL; ++n)
	  continue;
     return n;
}


/*
 * Substitute first occurence of pattern with another string.
 * There should be enough space for str to grow to take any new
 * values.
 */
char *util_strsub(char *str, 		/* string */
		  const char *pat,	/* pattern to find */
		  const char *sub	/* substring to substitute */ )
{
     size_t lenpat, lensub, lenstr;

     if (str == NULL)
          elog_die(FATAL, "str == NULL");
     if (pat == NULL)
          elog_die(FATAL, "pat == NULL");
     if (*pat == '\0')
          elog_die(FATAL, "*pat == '\0'");
     if (sub == NULL)
          elog_die(FATAL, "sub == NULL");

     str = strstr(str, pat);
     if (str == NULL)
	  return NULL;

     lenstr = strlen(str);
     lenpat = strlen(pat);
     lensub = strlen(sub);

     /* make room for substituted string, or remove slack after it */
     if (lensub != lenpat)
	  memmove(str + lensub, str + lenpat, lenstr + 1 - lenpat);

     memcpy(str, sub, lensub);
     return str + lensub;
}



/* Concatinate multiple strings (a la varargs) into a single one.
 * The final argument must be a NULL to terminate the concatination.
 * If the frst argument is NULL, so will be the return value!
 * Returns a buffer which should be nfree()ed after use.
 */
#define UTIL_STRJOIN_N 64
char *util_strjoin(const char *s, ...) {
	va_list args;
	char *q, *block;
	const char *p;
	size_t size, len;
	size_t lentab[UTIL_STRJOIN_N];
	int n, i;

	if (s == NULL)
	     return NULL;

	/*
	 * Compute the amount of memory needed for the target string.
	 * (I could use realloc to make it larger if an initial guess
	 * at its size would be too little, but this way we avoid doing
	 * calling realloc many times, which is a win, because it can
	 * be pretty slow.  However, I haven't actually tested that
	 * this is faster. :-( ).
	 *
	 * I use another untested speed hack (but this one should be
	 * obvious -- famous last words): to avoid having to compute the
	 * length of each string twice, I store the lengths in an array,
	 * lentab.  If there are more strings than will fit into lentab,
	 * then the rest will still have their lengths computed twice.
	 * UTIL_STRJOIN_N, the length of lentab, should be made large 
	 * enough that it seldom happens, and small enough that there 
	 * is not significant memory loss.
	 */
	n = 1;
	lentab[0] = strlen(s);
	size = 1 + lentab[0];
	va_start(args, s);
	while ((p = va_arg(args, char *)) != NULL) {
		len = strlen(p);
		size += len;
		if (n < UTIL_STRJOIN_N)
			lentab[n++] = len;
	}
	va_end(args);

	/*
	 * Allocate the block.
	 */
	block = xnmalloc(size);

	/*
	 * Concatenate the strings to the allocated block.
	 */
	memcpy(block, s, lentab[0]);
	q = block + lentab[0];
	i = 1;
	va_start(args, s);
	while ((p = va_arg(args, const char *)) != NULL) {
		len = (i < n) ? lentab[i++] : strlen(p);
		memcpy(q, p, len);
		q += len;
	}
	va_end(args);

	*(block + size-1) = '\0';

	return block;
}


/* convert a signed long (32 bits)  to a string */
char *util_i32toa(long src)
{
     static char buf[UTIL_U32STRLEN];
     snprintf(buf, UTIL_U32STRLEN, "%ld", src);
     return buf;
}


/* convert an unsigned long (32 bits) to a string */
char *util_u32toa(unsigned long src)
{
     static char buf[UTIL_U32STRLEN];
     snprintf(buf, UTIL_U32STRLEN, "%lu", src);
     return buf;
}


/* convert a signed long long (64 bits) to a string */
char *util_i64toa(long long src)
{
     static char buf[UTIL_U64STRLEN];
     snprintf(buf, UTIL_U64STRLEN, "%lld", src);
     return buf;
}


/* convert an unsigned long long (64 bits) to a string */
char *util_u64toa(unsigned long long src)
{
     static char buf[UTIL_U64STRLEN];
     snprintf(buf, UTIL_U64STRLEN, "%llu", src);
     return buf;
}


/* convert a float to a string */
char  *util_ftoa(float src)
{
     static char buf[UTIL_FLOATSTRLEN];
     snprintf(buf, UTIL_FLOATSTRLEN, "%.2f", src);
     return buf;
}


/* convert an unsigned long (32 bits) to an octal string */
char *util_u32toaoct(unsigned long src)
{
     static char buf[UTIL_U32STRLEN];
     snprintf(buf, UTIL_U32STRLEN, "%o", src);
     return buf;
}


#if __svr4__
/* convert a high resolution timer to a string */
char  *util_hrttoa(hrtime_t src)
{
     static char buf[UTIL_NANOSTRLEN];
     int r, i;

     r = snprintf(buf, UTIL_NANOSTRLEN, "%u.%u", 
		  src / 1000000000LL, src % 1000000000LL);
     /* strip off trailing zeros */
     if (r > 0)
	  for (i = r-1 ; i > 1 ; i--)
	       if (buf[i] == '0' && buf[i-1] != '.')
		    buf[i] = '\0';
	       else
		    break;
     return buf;
}


/* convert a timestruct to a string, solaris version */
char  *util_tstoa(timestruc_t *src)
{
     static char buf[UTIL_NANOSTRLEN];
     int r, i;

     r = snprintf(buf,UTIL_NANOSTRLEN,"%lu.%.9li", src->tv_sec, src->tv_nsec);
     /* strip off trailing zeros */
     if (r > 0)
	  for (i = r-1 ; i > 1 ; i--)
	       if (buf[i] == '0' && buf[i-1] != '.')
		    buf[i] = '\0';
	       else
		    break;
     return buf;
}
#elif linux
/* convert a timestruct to a string, linux version */
char  *util_tstoa(struct timespec *src)
{
     static char buf[UTIL_NANOSTRLEN];
     int r, i;

     r = snprintf(buf,UTIL_NANOSTRLEN,"%lu.%.9li", src->tv_sec, src->tv_nsec);
     /* strip off trailing zeros */
     if (r > 0) {
	  for (i = r-1 ; i > 1 ; i--) {
	       if (buf[i] == '0' && buf[i-1] != '.')
		    buf[i] = '\0';
	       else
		    break;
	  }
     }
     return buf;
}
#else
#endif /* __svr4__ */


/* converts linux's jiffy format into seconds. input and output are strings */
char  *util_jiffytoa(long long jiffies)
{
     static char buf[UTIL_NANOSTRLEN];

     /* a jiffy is 1/100th of a second in linux */
     sprintf(buf, "%lu.%lu", jiffies / 100, jiffies % 100);

     return buf;
}



/* Get the basename from path. Returns a pointer into the existing string */
char  *util_basename(char *path)
{
     char *pt;

     pt = strrchr(path, '/');
     if (pt)
          pt++;
     else
          pt = path;

     return pt;
}


/* 
 * Inspect the string for NULL or zero lengh strings. Returns a pointer 
 * to "-" if so or the original string pointer otherwise.
 */
char *util_nonull(char *str)
{
     if ( (! str) || (! *str) )
          return UTIL_BLANKREPSTR;
     else
          return str;
}


/* ----- Date and time functions ----- */

/*
 * Generate an ascii representation of time
 * Warning: single threaded function 
 */
char *util_decdatetime(time_t t)
{
     static char buf[UTIL_SHORTSTR];
     struct tm *time;

     time = localtime(&t);
     strftime(buf, UTIL_SHORTSTR, "%d-%b-%y %I:%M:%S %p", time);
     /* %c */
     /* also %d-%b-%y %I:%M:%S %p */

     return buf;
}

/*
 * Generate a short ascii representation of time, adapting its format
 * to the distance of the event: the further away in time, the more
 * approximate the representation.
 */
/* Warning: single threaded function */
char *util_shortadaptdatetime(time_t t) {
     static char buf[UTIL_SHORTSTR];
     struct tm entry, now;
     time_t now_t;

     now_t = time(NULL);
     memcpy(&entry, localtime(&t), sizeof(struct tm));
     memcpy(&now, localtime(&now_t), sizeof(struct tm));

     strftime(buf, UTIL_SHORTSTR, "%c", &entry); /* also %d-%b-%y %I:%M:%S %p*/
     /* Or adaptive  recent: %I:%M:%S */
     /* longer term: %d%b */

     if (entry.tm_year != now.tm_year)
	  /* not this year */
	  if ( abs(entry.tm_year - now.tm_year) <= 1 &&
	       abs(entry.tm_mon  - now.tm_mon) < 6 )
	       /* ...but less than six months adjacent, give date */
	       strftime(buf, UTIL_SHORTSTR, "%d %b", &entry);
	  else
	       /* all other times, give year */
	       strftime(buf, UTIL_SHORTSTR, " %Y ", &entry);
     else
	  /* this year */
	  if (entry.tm_yday != now.tm_yday)
	       /* not today */
	       if ( abs(entry.tm_yday - now.tm_yday) <= 1 && 
		    abs(entry.tm_hour - now.tm_hour) <= 12 )
		    /* ...but less than 12 hours adjacent, give hours */
		    strftime(buf, UTIL_SHORTSTR, "%H:%M:%S", &entry);
	       else
		    /* Older than 12 hours, give date */
		    strftime(buf, UTIL_SHORTSTR, "%d %b", &entry);
	  else
	       /* Today */
	       strftime(buf, UTIL_SHORTSTR, "%H:%M:%S", &entry);

     return buf;
}


/*
 * Copy a file from one place to another using mmap.
 * It will write over any existing files.
 * Returns 0 for failure or 1 for success
 */
int    util_filecopy(char *src, char *dst) {
     struct stat buf;
     int r, src_fd, dst_fd;
     void *data;

     /* stat and open source */
     r = stat(src, &buf);
     if (r == -1) {
	  elog_printf(DIAG, "can't stat source file %s, %s (%d)", 
		      src, strerror(errno), errno);
	  return 0;
     }
     src_fd = open(src, O_RDONLY);
     if (src_fd == -1) {
	  elog_printf(DIAG, "can't read source file %s, %s (%d)", 
		      src, strerror(errno), errno);
	  return 0;
     }

     /* attempt to create destination */
     dst_fd = creat(dst, 0644);
     if (dst_fd == -1) {
	  elog_printf(DIAG, "can't create destination file %s, %s (%d)", 
		      dst, strerror(errno), errno);
	  close(src_fd);
	  return 0;
     }

     /* map source and write data to destination */
     data = mmap((caddr_t) 0, buf.st_size, PROT_READ, MAP_SHARED, src_fd, 0);
     if (data == MAP_FAILED) {
	  elog_printf(DIAG, "can't map source file %s, %s (%d)", 
		      src, strerror(errno), errno);
	  close(src_fd);
	  close(dst_fd);
	  return 0;
     }
     r = write(dst_fd, data, buf.st_size);
     if (r == -1) {
	  elog_printf(DIAG, "can't write to destination file %s, %s (%d)", 
		      dst, strerror(errno), errno);
	  close(src_fd);
	  close(dst_fd);
	  return 0;
     }
     if (r != buf.st_size) {
	  elog_printf(DIAG, "could only write %d to destination file %s", 
		      r, dst);
	  close(src_fd);
	  close(dst_fd);
	  return 0;
     }

     /* completed, goodbye!! */
     close(src_fd);
     close(dst_fd);

     return 1;
}



/*
 * Does the string only contain printable characters? 
 * Returns 1 for success or 0 for failure
 */
int    util_is_str_printable(char *str)
{
     char *s;
     for (s = str; *s; s++) {
	  if ( ! isprint(*s) )
	       return 0;
     }
     return 1;
}


/*
 * Does the string only contain whitespace characters? 
 * Returns 1 for success or 0 for failure
 */
int    util_is_str_whitespace(char *str)
{
     char *s;
     for (s = str; *s; s++) {
	  if ( ! isspace(*s) )
	       return 0;
     }
     return 1;
}


/*
 * Search the directory list `dirlst' for a file `file', returning the 
 * nmalloc()ed path if successful or NULL for failure. The return value 
 * should be nfree()d after use. Dirlst should be a colon seperated list
 * of files, such as you might find in PATH, MANPATH or CDPATH environment
 * variables.
 */
char * util_whichdir(char *file, char *dirlst)
{
     char *dir, path[PATH_MAX];
     int dirlen;

     dir = dirlst;
     while (*dir) {
	  /* skip colon seperator */
	  if (*dir == ':')
	       dir++;

	  /* find next dir token */
	  dirlen = strcspn(dir, ":");
	  if (dirlen == 0) {
	       /* zero length token (::) current dir */
	       strcpy(path, file);
	  } else {
	       strncpy(path, dir, dirlen);
	       *(path+dirlen) = '/';
	       strcpy(path+dirlen+1, file);
	  }

	  if (access(path, F_OK) == 0) {
	       /* match found */
	       return xnstrdup(path);
	  }

	  dir += dirlen;
     }

     return NULL;
}



/* Base-64 encoding.  This encodes binary data as printable ASCII characters.
** Three 8-bit binary bytes are turned into four 6-bit values, like so:
**
**   [11111111]  [22222222]  [33333333]
**
**   [111111] [112222] [222233] [333333]
**
** Then the 6-bit values are represented using the characters "A-Za-z0-9+/".
*/

static char b64_encode_table[64] = {
     'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',  /* 0-7 */
     'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',  /* 8-15 */
     'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',  /* 16-23 */
     'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',  /* 24-31 */
     'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',  /* 32-39 */
     'o', 'p', 'q', 'r', 's', 't', 'u', 'v',  /* 40-47 */
     'w', 'x', 'y', 'z', '0', '1', '2', '3',  /* 48-55 */
     '4', '5', '6', '7', '8', '9', '+', '/'   /* 56-63 */
};

static int b64_decode_table[256] = {
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
     52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
     -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
     15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
     -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
     41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
};


/* Do base-64 encoding on a hunk of bytes.   Return the actual number of
** bytes generated.  Base-64 encoding takes up 4/3 the space of the original,
** plus a bit for end-padding.  3/2+5 gives a safe margin.
*/
int util_b64_encode( unsigned char* ptr, int len, char* space, int size )
{
     int ptr_idx, space_idx, phase;
     char c;
     
     space_idx = 0;
     phase = 0;
     for ( ptr_idx = 0; ptr_idx < len; ++ptr_idx )
     {
	  switch ( phase )
	  {
	  case 0:
	       c = b64_encode_table[ptr[ptr_idx] >> 2];
	       if ( space_idx < size )
		    space[space_idx++] = c;
	       c = b64_encode_table[( ptr[ptr_idx] & 0x3 ) << 4];
	       if ( space_idx < size )
		    space[space_idx++] = c;
	       ++phase;
	       break;
	  case 1:
	       space[space_idx - 1] =
		    b64_encode_table[
			 b64_decode_table[(int)space[space_idx - 1]] |
			 ( ptr[ptr_idx] >> 4 ) ];
	       c = b64_encode_table[( ptr[ptr_idx] & 0xf ) << 2];
	       if ( space_idx < size )
		    space[space_idx++] = c;
	       ++phase;
	       break;
	  case 2:
	       space[space_idx - 1] =
		    b64_encode_table[
			 b64_decode_table[(int)space[space_idx - 1]] |
			 ( ptr[ptr_idx] >> 6 ) ];
	       c = b64_encode_table[ptr[ptr_idx] & 0x3f];
	       if ( space_idx < size )
		    space[space_idx++] = c;
	       phase = 0;
	       break;
	  }
     }
     /* Pad with ='s. */
     while ( phase++ < 3 )
	  if ( space_idx < size )
	       space[space_idx++] = '=';
     return space_idx;
}



/* Do base-64 decoding on a string.  Ignore any non-base64 bytes.
** Return the actual number of bytes generated.  The decoded size will
** be at most 3/4 the size of the encoded, and may be smaller if there
** are padding characters (blanks, newlines).
*/
int util_b64_decode( const char* str, unsigned char* space, int size )
{
     const char* cp;
     int space_idx, phase;
     int d, prev_d=0;
     unsigned char c;

     space_idx = 0;
     phase = 0;
     for ( cp = str; *cp != '\0'; ++cp )
     {
	  d = b64_decode_table[(int)*cp];
	  if ( d != -1 )
	  {
	       switch ( phase )
	       {
	       case 0:
		    ++phase;
		    break;
	       case 1:
		    c = ( ( prev_d << 2 ) | ( ( d & 0x30 ) >> 4 ) );
		    if ( space_idx < size )
			 space[space_idx++] = c;
		    ++phase;
		    break;
	       case 2:
		    c = ( ( ( prev_d & 0xf ) << 4 ) | ( ( d & 0x3c ) >> 2 ) );
		    if ( space_idx < size )
			 space[space_idx++] = c;
		    ++phase;
		    break;
	       case 3:
		    c = ( ( ( prev_d & 0x03 ) << 6 ) | d );
		    if ( space_idx < size )
			 space[space_idx++] = c;
		    phase = 0;
		    break;
	       }
	       prev_d = d;
	  }
     }
     return space_idx;
}



void util_strencode( char* to, int tosize, char* from )
{
     int tolen;
     
     for ( tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from )
     {
	  if ( isalnum(*from) || strchr( "/_.", *from ) != (char*) 0 )
	  {
	       *to = *from;
	       ++to;
	       ++tolen;
	  }
	  else
	  {
	       sprintf( to, "%c%02x", '%', *from );
	       to += 3;
	       tolen += 3;
	  }
     }
     *to = '\0';
}



/* Copies and decodes a string.  It's ok for from and to to be the
** same string as the destination will never be larger than the source.
*/
void util_strdecode( char* to, char* from )
{
     for ( ; *from != '\0'; ++to, ++from )
     {
	  if ( from[0] == '%' && isxdigit( from[1] ) && isxdigit( from[2] ) )
	  {
	       *to = util_hexit( from[1] ) * 16 + util_hexit( from[2] );
	       from += 2;
	  }
	  else
	       *to = *from;
     }
     *to = '\0';
}


int util_hexit( char c )
{
     if ( c >= '0' && c <= '9' )
	  return c - '0';
     if ( c >= 'a' && c <= 'f' )
	  return c - 'a' + 10;
     if ( c >= 'A' && c <= 'F' )
	  return c - 'A' + 10;
     return 0;           /* shouldn't happen, we're guarded by isxdigit() */
}



/* as strtok in libc, but only consumes a single character separator 
 * for each call */
char * util_strtok_sc(char *str, char *s)
{
     static char *nexttok = NULL;
     char *sep, *tok;

     if (str) {
	  /* first search */
	  sep = strstr(str, s);
	  if (sep) {
	       *sep = '\0';
	       nexttok = sep + 1;
	  } else
	       nexttok = NULL;
	  return str;
     } else {
	  /* successive searches */
	  if (nexttok) {
	       tok = nexttok;
	       sep = strstr(tok, s);
	       if (sep) {
		    *sep = '\0';
		    nexttok = sep + 1;
	       } else
		    nexttok = NULL;
	       return tok;
	  } else
	       return NULL;
     }
}


/* Return the host name */
char  util_hostname_t[UTIL_HOSTLEN] = {'\0'};
char *util_hostname() {
     if (util_hostname_t[0])
	  return util_hostname_t;
     if (gethostname(util_hostname_t, UTIL_HOSTLEN) != 0)
	  return NULL;
     strtok(util_hostname_t, ".");
     return util_hostname_t;
}

/* Return the domain name */
char  util_domainname_t[UTIL_DOMAINLEN] = {'\0'};
char *util_domainname() {
     if (util_domainname_t[0])
	  return util_domainname_t;
     if (getdomainname(util_domainname_t, UTIL_DOMAINLEN) != 0)
	  return NULL;
     else
	  return util_domainname_t;
}

/* Return the host name */
char  util_fqhostname_t[UTIL_FQHOSTLEN] = {'\0'};
char *util_fqhostname() {
     if (util_fqhostname_t[0])
	  return util_fqhostname_t;
     if (gethostname(util_fqhostname_t, UTIL_FQHOSTLEN) != 0)
	  return NULL;
     else
	  return util_fqhostname_t;
}



#if defined(TEST)
#define TEST_TEXT1 "This is line one\n"
"and line two\n"
"line three\n"
"four\n"
"line five, below is line six, which is blank\n"
"\n"
"seven\n"
"line eight (this one) is    very    very    long              with all sorts of spaces between the words to confuse the poor little parser.\n"
"line nine and thats your lot."
#define TEST_FILE1 "util.t1.txt"
#define TEST_PURL1 "file:util.t1.txt"
#define TEST_TEXT2 "magic line 1\n"
"this will test strings\n"
"there should be n\"o strings re\"cognised in this line\n"
"but this \"line should\" have one\n"
"this \"line should have\" two strings \"with some words in\"\n"
"this line should have a \"\" null string\n"
"and this one \" should finish a token in mid\"word, just to show it can be done\n"
"this line will \"start with a string that ends at the newline\"\n"
"\"\" another empty one?\n"
"and one \"\\n that inclues \\t some escape chars\"\n"
"the end"

#define TEST_FILE2 "util.t1.txt"
#define TEST_PURL2 "file:util.t1.txt"
#define TEST_BIGPURL1 "file:/etc/termcap"
#define TEST_BIGPURL2 "file:/etc/passwd"
#define TEST_TEXT3 "one two three"
#define TEST_TEXT4 "one two three\n"
#define TEST_TEXT5 "one two three\nfour"
#define TEST_TEXT6 "one two three\n# four"
#define TEST_TEXT7 "one two three\n# one two three"
#define TEST_TEXT8 "one two three\n   one   two   three"
#define TEST_TEXT9 "one two three\n   two   three"
#define TEST_TEXT10 "one two three\none two three\none two three"
#define TEST_TEXT11 "one two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three\none two three"
#define TEST_TEXT12 "one \"two\" three"
#define TEST_TEXT13 "\"one\" \"two\" three"
#define TEST_TEXT14 "one \"two\" \"three\""
#define TEST_TEXT15 "one \"two\" \"three\"\n"
#define TEST_TEXT16 "one \"two\" \"three\"\n#"
#define TEST_TEXT17 "one \"two\" \"three\"\n#\n"
#define TEST_TEXT18 "one \"two two and a half\" \"three\"\n#\n"
#define TEST_TEXT19 "one \"two two and a half\" \"three\"\n#\n\n\n#bollocks\none \"two two and a half\" \"three\"\n"
#define TEST_TEXT20 ""
#define TEST_TEXT21 "\n"
#define TEST_TEXT22 "one\none"
#define TEST_TEXT23 "one two\none two"
#define TEST_TEXT24 "one two three\none two three"
#define TEST_TEXT25 "one two three four\none two three four"
#define TEST_TEXT26 "one two three four five\none two three four five"
#define TEST_TEXT27 "one two three four five six\none two three four five six"
#define TEST_TEXT28 "4461 4461 \"load1 load5 load15 runque nprocs lastproc mem_tot mem_used mem_free mem_shared mem_buf mem_cache swap_tot swap_used swap_free cpu_user cpu_nice cpu_system cpu_idle vm_pgpgin vm_pgpgout vm_pgswpin vm_pgswpout nintr ncontext nforks uptime idletime\n'1 minute load average' '5 minute load average' '15 minute load average' 'num runnable procs' 'num of procs' 'last proc run' 'total memory' 'memory used' 'memory free' 'used memory shared' 'buffer memory' 'cache memory' 'total swap space' 'swap space used' 'swap space free' 'secs cpu spent in user space' 'secs cpu spent in nice user space' 'secs cpu spent in kernel' 'secs cpu was idle' 'npages paged in' 'npages paged out' 'npages swapped in' 'npages swapped out' 'total nuymber of interrupts' 'number of context switches' 'number of forks' 'secs system has been up' 'secs system has been idle' info\nabs abs abs abs abs abs abs abs abs abs abs abs abs abs abs cnt cnt cnt cnt cnt cnt cnt cnt cnt cnt cnt cnt cnt sense\nnano nano nano u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 u32 nano nano type\"\n"



main(int argc, char **argv)
{
     ITREE *l1, *l2, *l3;
     /*     SCANBUF*/ ITREE *sb1;
     rtinf rt, err;
     int r;
     char *buffer;

     /* Write the buffer out to a route, so that we may read it back in */
     unlink(TEST_FILE1);
     route_init("stderr", 0, NULL);
     err = route_open("stderr", NULL, NULL, 10);
     elog_init(err, 0, "util test", NULL);
     rt = route_open(TEST_PURL1, NULL, NULL, 10);
     route_printf(rt, TEST_TEXT1);
     route_close(rt);

     /* test 1: controlled test */
     r = util_parseroute(TEST_PURL1, " ", NULL, &l1);
     if (r != 8)
	  route_die(err, "[1] parsed %d data lines, not 9\n", r);
     util_parsedump(l1);
     util_freeparse(l1);

     /* test 2: quote test */
     unlink(TEST_FILE2);
     rt = route_open(TEST_PURL2, NULL, NULL, 10);
     route_printf(rt, TEST_TEXT2);
     route_close(rt);
     r = util_parseroute(TEST_PURL2, " ", "magic line 1", &l1);
     if (r != 10)
	  route_die(err, "[2] parsed %d data lines, not 10\n", r);
     util_parsedump(l1);
     util_freeparse(l1);

     /* test 3: get it read the termcap and watch it explode */
     r = util_parseroute(TEST_BIGPURL1, " :", NULL, &l1);
     if (!r) {
	  r = util_parseroute(TEST_BIGPURL2, " :", NULL, &l1);
	  if (!r)
	       route_die(err, "[3] parsed no data\n");
     }
     route_printf(err, "[3] parsed %d data lines\n", r);
     util_parsedump(l1);
     util_freeparse(l1);

#if 0
     /* test 4: count columns */
     if ( (r=util_countcols(TEST_TEXT3, " ")) != 3 )
          route_die(err, "[4t3] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT4, " ")) != 3 )
          route_die(err, "[4t4] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT5, " ")) != -1 )
          route_die(err, "[4t5] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT6, " ")) != 3 )
          route_die(err, "[4t6] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT7, " ")) != 3 )
          route_die(err, "[4t7] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT8, " ")) != 3 )
          route_die(err, "[4t8] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT9, " ")) != -1 )
          route_die(err, "[4t9] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT10, " ")) != 3 )
          route_die(err, "[4t10] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT11, " ")) != 3 )
          route_die(err, "[4t11] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT12, " ")) != 3 )
          route_die(err, "[4t12] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT13, " ")) != 3 )
          route_die(err, "[4t13] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT14, " ")) != 3 )
          route_die(err, "[4t14] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT15, " ")) != 3 )
          route_die(err, "[4t15] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT16, " ")) != 3 )
          route_die(err, "[4t16] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT17, " ")) != 3 )
          route_die(err, "[4t17] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT18, " ")) != 3 )
          route_die(err, "[4t18] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT19, " ")) != 3 )
          route_die(err, "[4t19] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT20, " ")) != 0 )
          route_die(err, "[4t20] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT21, " ")) != 0 )
          route_die(err, "[4t21] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT22, " ")) != 1 )
          route_die(err, "[4t22] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT23, " ")) != 2 )
          route_die(err, "[4t23] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT24, " ")) != 3 )
          route_die(err, "[4t24] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT25, " ")) != 4 )
          route_die(err, "[4t25] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT26, " ")) != 5 )
          route_die(err, "[4t26] cant count columns (%d)", r);
     if ( (r=util_countcols(TEST_TEXT27, " ")) != 6 )
          route_die(err, "[4t27] cant count columns (%d)", r);
#endif

     /* test 5: extract columns from tables */
     l1 = itree_create();
     itree_add(l1, 2, NULL);
     l2 = util_extractcols(TEST_TEXT24, " ", l1);
     if (l2 == NULL)
          route_die(err, "[5a] cant get list of lists\n");
     if (itree_n(l2) != 1)
          route_die(err, "[5a] single column list not returned\n");
     itree_first(l2);
     l3 = itree_get(l2);
     if (l3 == NULL)
          route_die(err, "[5a] cant get col 2 list");
     if (itree_n(l3) != 2)
          route_die(err, "[5a] two elements not returned in col 2 list\n");
     itree_traverse(l3)
          if ( strcmp("two", itree_get(l3)) )
	       route_die(err, "[5a] col 2 key %d is not \"two\"\n", 
			 itree_getkey(l3));
     util_freeextract(l2);
     		/* quoted 2nd col */
     l2 = util_extractcols(TEST_TEXT19, " ", l1);
     if (l2 == NULL)
          route_die(err, "[5b] cant get list of lists\n");
     if (itree_n(l2) != 1)
          route_die(err, "[5b] single column list not returned\n");
     itree_first(l2);
     l3 = itree_get(l2);
     if (l3 == NULL)
          route_die(err, "[5b] cant get col 2 list");
     if (itree_n(l3) != 2)
          route_die(err, "[5b] two elements not returned in col 2 list\n");
     itree_traverse(l3)
          if ( strcmp("two two and a half", itree_get(l3)) )
	       route_die(err, "[5b] col 2 key %d is not \"two two and a "
			 "half\"\n", itree_getkey(l3));
     util_freeextract(l2);
     		/* 2 cols */
     itree_add(l1, 1, NULL);
     l2 = util_extractcols(TEST_TEXT19, " ", l1);
     if (l2 == NULL)
          route_die(err, "[5c] cant get list of lists\n");
     if (itree_n(l2) != 2)
          route_die(err, "[5c] two column lists not returned\n");
     itree_first(l2);
     l3 = itree_get(l2);
     if (l3 == NULL)
          route_die(err, "[5c] cant get col 1 list");
     if (itree_n(l3) != 2)
          route_die(err, "[5c] two elements not returned in col 1 list\n");
     itree_traverse(l3)
          if ( strcmp("one", itree_get(l3)) )
	       route_die(err, "[5b] col 2 key %d is not one", 
			 itree_getkey(l3));
     itree_next(l2);
     l3 = itree_get(l2);
     if (l3 == NULL)
          route_die(err, "[5c] cant get col 2 list");
     if (itree_n(l3) != 2)
          route_die(err, "[5c] two elements not returned in col 2 list\n");
     itree_traverse(l3)
          if ( strcmp("two two and a half", itree_get(l3)) )
	       route_die(err, "[5b] col 2 key %d is not \"two two and a "
			 "half\"\n", itree_getkey(l3));
     util_freeextract(l2);
     itree_destroy(l1);

     /* test 6: hunt columns */
     r = util_huntcol(TEST_TEXT3, " ", "three");
     if (r != 3)
          route_die(err, "[6a] did not hunt name properly (%d)\n", r);
     r = util_huntcol(TEST_TEXT18, " ", "two two and a half");
     if (r != 2)
          route_die(err, "[6b] did not hunt name properly (%d)\n", r);
     r = util_huntcol(TEST_TEXT18, " ", "two");
     if (r != -1)
          route_die(err, "[6c] shouldn't find this\n");

     /* Write the buffer out to a route, so that we may read it back in */
     unlink(TEST_FILE1);
     rt = route_open(TEST_PURL1, NULL, NULL, 10);
     route_printf(rt, TEST_TEXT1);
     route_close(rt);

     /* test 7: controlled test using scan* */
     r = util_scancfroute(TEST_PURL1, " ", NULL, &sb1, &buffer);
     if (r != 8)
	  route_die(err, "[7] scanned %d data lines, not 8\n", r);
     util_scandump(sb1);
     util_scanfree(sb1);
     nfree(buffer);

     /* test 8: quote test using scan* */
     unlink(TEST_FILE2);
     rt = route_open(TEST_PURL2, NULL, NULL, 10);
     route_printf(rt, TEST_TEXT2);
     route_close(rt);
     r = util_scancfroute(TEST_PURL2, " ", "magic line 1", &sb1, &buffer);
     if (r != 10)
	  route_die(err, "[8] scanned %d data lines, not 10\n", r);
     util_scandump(sb1);
     util_scanfree(sb1);
     nfree(buffer);

#if 1
     /* test 9: get it read the termcap and watch it explode */
     r = util_scancfroute(TEST_BIGPURL1, " :", NULL, &sb1, &buffer);
     if (!r) {
	  r = util_scancfroute(TEST_BIGPURL2, " :", NULL, &sb1, &buffer);
	  if (!r)
	       route_die(err, "[9] scanned no data\n");
     }
     route_printf(err, "[9] scanned %d data lines\n", r);
     util_scandump(sb1);
     util_scanfree(sb1);
     nfree(buffer);
#endif

     /* test 10: escaped or embedded quote test using scan* */
     unlink(TEST_FILE2);
     rt = route_open(TEST_PURL2, NULL, NULL, 10);
     route_printf(rt, TEST_TEXT28);
     route_close(rt);
     r = util_scancfroute(TEST_PURL2, " ", NULL, &sb1, &buffer);
     if (r != 1)
	  route_die(err, "[10] scanned %d data lines, not 1\n", r);
     util_scandump(sb1);
     util_scanfree(sb1);
     nfree(buffer);


     /* finalise */
     elog_fini();
     route_close(err);
     route_fini();

     unlink(TEST_FILE1);
     fprintf(stderr, "%s: tests finished\n", argv[0]);
     return 0;
}
#endif /* TEST */
