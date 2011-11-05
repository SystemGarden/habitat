/*
 * Simple hash algorithm, 
 * taken from http://burtleburtle.net/bob/hash/doobs.html
 */

#include <string.h>
#include "hash.h"
#include "elog.h"

typedef  unsigned int  ub4;   /* unsigned 4-byte quantities in 32 & 64 bit 
			       * systems */
typedef  unsigned char ub1;   /* unsigned 1-byte quantities */

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/*
--------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.
For every delta with one or two bits set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
* If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
* If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
mix() was built out of 36 single-cycle latency instructions in a 
  structure that could supported 2x parallelism, like so:
      a -= b; 
      a -= c; x = (c>>13);
      b -= c; a ^= x;
      b -= a; x = (a<<8);
      c -= a; b ^= x;
      c -= b; x = (b>>13);
      ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage 
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
--------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  len     : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 6*len+35 instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

unsigned int hash_block(char *k, 		/* key */
			unsigned int length,	/* length of the key */
			unsigned int initval	/* previous hash or an 
						 * arbitrary value */ )
{
     unsigned int a,b,c,len;
     char *orig_k;

     /* Set up the internal state */
     len = length;
     a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
     c = initval;         /* the previous hash value */
     orig_k = k;

     /*--------------------- handle most of the key (set of 12 bytes) */
     while (len >= 12)
     {
	  a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
	  b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
	  c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
	  mix(a,b,c);
	  k += 12; len -= 12;
     }

     /*------------------------------------- handle the last 11 bytes */
     c += length;
     switch(len)              /* all the case statements fall through */
     {
     case 11: c+=((ub4)k[10]<<24);
     case 10: c+=((ub4)k[9]<<16);
     case 9 : c+=((ub4)k[8]<<8);
	  /* the first byte of c is reserved for the length */
     case 8 : b+=((ub4)k[7]<<24);
     case 7 : b+=((ub4)k[6]<<16);
     case 6 : b+=((ub4)k[5]<<8);
     case 5 : b+=k[4];
     case 4 : a+=((ub4)k[3]<<24);
     case 3 : a+=((ub4)k[2]<<16);
     case 2 : a+=((ub4)k[1]<<8);
     case 1 : a+=k[0];
	  /* case 0: nothing left to add */
     }
     mix(a,b,c);

     /*-------------------------------------------- report the result */
     /*elog_printf(DEBUG, "HASH %s => %u (0x%x)", orig_k, c, c);*/
     elog_printf(DEBUG, "computed hash %u (0x%x)", c, c);

     return c;
}


unsigned int hash_str(char *str) {
     return hash_block(str, strlen(str), 685570);
}


#if TEST

#include <stdio.h>
#include <stdlib.h>

int main() {
     unsigned int l1, l2;

     printf("%u %u %u %u\n", 
	    hash_str("tom dick harry"), hash_str("tom harry dick"), 
	    hash_str("dick tom harry"), hash_str("tom dick haryr"));

     /* test 1 */
     l1 = hash_str("tom dick harry");
     l2 = hash_str("tom dick harry");
     if (l1 != l2)
	  fprintf(stderr, "[1] hash is not repeatable");

     /* test 2 */
     l2 = hash_str("tom dick haryr");
     if (l1 == l2)
	  fprintf(stderr, "[2] shouldn't be the same");

     exit(0);
}

#endif /* TEST */
