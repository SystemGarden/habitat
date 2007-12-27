/*
 * Simple extendable string buffer for habitat
 * Copyright System Garden Ltd, 2004.
 * Nigel Stuckey, August 2004
 */

#ifndef _STRBUF_H_
#define _STRBUF_H_


#define STRBUF_INITIAL_LEN  128
#define STRBUF_EXTEND_LEN  1024

struct strbuf_inst {
     char *buffer;
     int strlen;
     int blocklen;
};
typedef struct strbuf_inst * STRBUF;

STRBUF strbuf_init();
void   strbuf_append(STRBUF buf, char *text);
void   strbuf_backspace(STRBUF buf);
char * strbuf_string(STRBUF buf);
int    strbuf_strlen(STRBUF buf);
void   strbuf_fini(STRBUF buf);

#endif /* _STRBUF_H_ */
