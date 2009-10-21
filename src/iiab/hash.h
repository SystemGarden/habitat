/*
 * Simple hash algroithm
 * taken from http://burtleburtle.net/bob/hash/doobs.html
 */

/* functional prototypes */
unsigned int hash_block(char *k, unsigned int length, unsigned int initval);
unsigned int hash_str(char *str);
