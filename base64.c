/*	$Id$ */
/*
 * Copyright (c) 2015 Kristaps Dzonsons <kristaps@kcons.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <kcgi.h>
#include <kcgijson.h>
#include <gmp.h>

#include "extern.h"

static const char b64[] = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

size_t 
base64len(size_t len)
{

	return((len + 2) / 3 * 4) + 1;
}

size_t 
base64buf(char *enc, const char *str, size_t len)
{
	size_t 	i, val;
	char 	*p;

	p = enc;

	for (i = 0; i < len - 2; i += 3) {
		val = (str[i] >> 2) & 0x3F;
		assert(val < sizeof(b64));
		*p++ = b64[val];

		val = ((str[i] & 0x3) << 4) | 
			((int)(str[i + 1] & 0xF0) >> 4);
		assert(val < sizeof(b64));
		*p++ = b64[val];

		val = ((str[i + 1] & 0xF) << 2) | 
			((int)(str[i + 2] & 0xC0) >> 6);
		assert(val < sizeof(b64));
		*p++ = b64[val];

		val = str[i + 2] & 0x3F;
		assert(val < sizeof(b64));
		*p++ = b64[val];
	}

	if (i < len) {
		val = (str[i] >> 2) & 0x3F;
		assert(val < sizeof(b64));
		*p++ = b64[val];

		if (i == (len - 1)) {
			val = ((str[i] & 0x3) << 4);
			assert(val < sizeof(b64));
			*p++ = b64[val];
			*p++ = '=';
		} else {
			val = ((str[i] & 0x3) << 4) |
				((int)(str[i + 1] & 0xF0) >> 4);
			assert(val < sizeof(b64));
			*p++ = b64[val];

			val = ((str[i + 1] & 0xF) << 2);
			assert(val < sizeof(b64));
			*p++ = b64[val];
		}
		*p++ = '=';
	}

	*p++ = '\0';
	return(p - enc);
}

static void
encodeblock(unsigned char *in, unsigned char *out, int len)
{
	int	 val;

	val = (int)(in[0] >> 2);
	assert(val >= 0 && (size_t)val < sizeof(b64));
	out[0] = b64[val];

	val = (int)(((in[0] & 0x03) << 4) | 
		((in[1] & 0xf0) >> 4));
	assert(val >= 0 && (size_t)val < sizeof(b64));
	out[1] = b64[val];

	if (len > 1) {
		val = (int)(((in[1] & 0x0f) << 2) | 
			((in[2] & 0xc0) >> 6));
		assert(val >= 0 && (size_t)val < sizeof(b64));
		out[2] = b64[val];
	} else
		out[2] = '=';

	if (len > 2) {
		val = (int)(in[2] & 0x3f);
		assert(val >= 0 && (size_t)val < sizeof(b64));
		out[3] = b64[val];
	} else
		out[3] = '=';
}

void
base64file(FILE *infile, size_t lsz, 
	int (*fp)(const char *, size_t, void *), void *arg)
{
	unsigned char	in[3];
	unsigned char	out[4];
	size_t		i, len, bout;

	bout = 0;
	*in = *out = '\0';

	while (0 == feof(infile)) {
		len = 0;
		for (i = 0; i < 3; i++) {
			in[i] = (unsigned char)getc(infile);
			if(0 == feof(infile)) 
				len++;
			else
				in[i] = '\0';
		}

		if(len > 0) {
			encodeblock(in, out, len);
			for (i = 0; i < 4; i++)
				fp((char *)&out[i], 1, arg);
			bout++;
		}
		if (bout >= (lsz / 4) || feof(infile)) {
			if (bout > 0)
				fp("\r\n", 2, arg);
			bout = 0;
		}
	}
}
