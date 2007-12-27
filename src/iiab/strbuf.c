/*
 * Simple extendable string buffer for habitat
 * Copyright System Garden Ltd, 2004.
 * Nigel Stuckey, August 2004
 */

#include <string.h>
#include "nmalloc.h"
#include "strbuf.h"

STRBUF strbuf_init()
{
     STRBUF s;

     s = xnmalloc(sizeof(struct strbuf_inst));
     s->buffer = xnmalloc(STRBUF_INITIAL_LEN);
     s->buffer[0] = '\0';
     s->strlen = 0;
     s->blocklen = STRBUF_INITIAL_LEN;

     return s;
}

void   strbuf_append(STRBUF buf, char *text)
{
     int len;

     len = strlen(text);
     if (buf->strlen+len > buf->blocklen) {
          buf->blocklen += STRBUF_EXTEND_LEN;
          buf->buffer = xnrealloc(buf->buffer, buf->blocklen);
     }
     strcpy(buf->buffer + buf->strlen, text);
     buf->strlen += len;
}

void   strbuf_backspace(STRBUF buf)
{
     if (buf->strlen <= 0)
          return;
     buf->strlen--;
     buf->buffer[buf->strlen] = '\0';
}

/* string must be nfree()ed after use */
char * strbuf_string(STRBUF buf) { return buf->buffer; }

int    strbuf_strlen(STRBUF buf) { return buf->strlen; }

/* string buffer is not freed, only structure; 
 * must run nfree(strbuf_string(...)) also */
void   strbuf_fini(STRBUF buf) 
{
     nfree(buf);
}
