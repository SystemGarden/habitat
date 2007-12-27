/*
 * Simple hash algroithm
 * taken from http://burtleburtle.net/bob/hash/doobs.html
 */

/* functional prototypes */
unsigned long hash_block(char *k, unsigned long length, unsigned long initval);
unsigned long hash_str(char *str);
