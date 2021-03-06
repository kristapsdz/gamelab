/*
 * Adapted from hmac.c (HMAC-MD5) for use by SHA1.
 * by <mcr@sandelman.ottawa.on.ca>. Test cases from RFC2202.
 *
 */

/*
 ** Function: hmac_sha1
 */

#include <sys/param.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <kcgi.h>
#include <kcgijson.h>
#include <gmp.h>

#include "extern.h"

/*
   unsigned char*  text;                pointer to data stream
   int             text_len;            length of data stream
   unsigned char*  key;                 pointer to authentication key
   int             key_len;             length of authentication key
   unsigned char*  digest;              caller digest to be filled in
   */

void
hmac_sha1(const unsigned char *text, int text_len,
		const unsigned char *key, int key_len,
		unsigned char *digest)
{
	SHA1_CTX context;
	unsigned char k_ipad[65];    /* inner padding -
				      * key XORd with ipad
				      */
	unsigned char k_opad[65];    /* outer padding -
				      * key XORd with opad
				      */
	unsigned char tk[20];
	int i;
	/* if key is longer than 64 bytes reset it to key=SHA1(key) */
	if (key_len > 64) {

		SHA1_CTX      tctx;

		SHA1Init(&tctx);
		SHA1Update(&tctx, key, key_len);
		SHA1Final(tk, &tctx);

		key = tk;
		key_len = 20;
	}

	/*
	 * the HMAC_SHA1 transform looks like:
	 *
	 * SHA1(K XOR opad, SHA1(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times

	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	memset( k_ipad, 0, sizeof(k_ipad));
	memset( k_opad, 0, sizeof(k_opad));
	memcpy( k_ipad, key, key_len);
	memcpy( k_opad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}
	/*
	 * perform inner SHA1
	 */
	SHA1Init(&context);                   /* init context for 1st
					       * pass */
	SHA1Update(&context, k_ipad, 64);      /* start with inner pad */
	SHA1Update(&context, text, text_len); /* then text of datagram */
	SHA1Final(digest, &context);          /* finish up 1st pass */
	/*
	 * perform outer MD5
	 */
	SHA1Init(&context);                   /* init context for 2nd
					       * pass */
	SHA1Update(&context, k_opad, 64);     /* start with outer pad */
	SHA1Update(&context, digest, 20);     /* then results of 1st
					       * hash */
	SHA1Final(digest, &context);          /* finish up 2nd pass */
}

/*
   Test Vectors (Trailing '\0' of a character string not included in test):

   key =         "Jefe"
   data =        "what do ya want for nothing?"
   data_len =    28 bytes
   digest =

   key =         0xAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA

   key_len       16 bytes
   data =        0xDDDDDDDDDDDDDDDDDDDD...
   ..DDDDDDDDDDDDDDDDDDDD...
   ..DDDDDDDDDDDDDDDDDDDD...
   ..DDDDDDDDDDDDDDDDDDDD...
   ..DDDDDDDDDDDDDDDDDDDD
   data_len =    50 bytes
   digest =      0x56be34521d144c88dbb8c733f0e8b3f6
   */

#ifdef TESTING
/*
 *  cc -DTESTING -I ../include/ hmac.c md5.c -o hmac
 *
 *  ./hmac Jefe "what do ya want for nothing?"
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	unsigned char digest[20];
	char *key;
	int key_len;
	char *text;
	int text_len;
	int i;

	key = argv[1];
	key_len = strlen(key);

	text = argv[2];
	text_len = strlen(text);

	lrad_hmac_sha1(text, text_len, key, key_len, digest);

	for (i = 0; i < 20; i++) {
		printf("%02x", digest[i]);
	}
	printf("\n");

	exit(0);
	return 0;
}

#endif

