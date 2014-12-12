/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gmp.h>
#include <kcgi.h>
#include <kcgijson.h>

#include "extern.h"

#ifdef __APPLE__
/*
 * Copyright (c) 2004 Ted Unangst and Todd Miller
 * All rights reserved.
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
#include <sys/cdefs.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#define INVALID 	1
#define TOOSMALL 	2
#define TOOLARGE 	3

static long long
strtonum(const char *numstr, long long minval, 
	long long maxval, const char **errstrp)
{
	long long ll = 0;
	char *ep;
	int error = 0;
	struct errval {
		const char *errstr;
		int err;
	} ev[4] = {
		{ NULL,		0 },
		{ "invalid",	EINVAL },
		{ "too small",	ERANGE },
		{ "too large",	ERANGE },
	};

	ev[0].err = errno;
	errno = 0;
	if (minval > maxval)
		error = INVALID;
	else {
		ll = strtoll(numstr, &ep, 10);
		if (numstr == ep || *ep != '\0')
			error = INVALID;
		else if ((ll == LLONG_MIN && errno == ERANGE) || ll < minval)
			error = TOOSMALL;
		else if ((ll == LLONG_MAX && errno == ERANGE) || ll > maxval)
			error = TOOLARGE;
	}
	if (errstrp != NULL)
		*errstrp = ev[error].errstr;
	errno = ev[error].err;
	if (error)
		ll = 0;

	return (ll);
}
#endif

int
mpq_str2mpq(const char *v, mpq_t q)
{
	char		 buf[64];
	const char	*cp, *er;
	unsigned long	 den, num;
	int		 neg;
	long		 mul;
	size_t		 i, sz, precision;
	mpq_t		 tmp, stor;

	/* First pass: MPQ all non-dotted numbers. */
	if (NULL == (cp = strchr(v, '.'))) {
		mpq_init(q);
		if (-1 == mpq_set_str(q, v, 10)) {
			fprintf(stderr, "MPQ didn't like: %s\n", v);
			mpq_clear(q);
			return(0);
		}
		mpq_canonicalize(q);
		fprintf(stderr, "Number clear: %s\n", v);
		return(1);
	} 
	
	/* Trim whitespace. */
	while (isspace(*v))
		v++;
	if ('\0' == *v)
		return(0);
	if (0 == (sz = strlen(v)))
		return(0);
	while (sz > 0 && isspace(v[sz - 1]))
		sz--;

	/* Test size. */
	if (sz > sizeof(buf) - 1) {
		fprintf(stderr, "Number too long: %zu\n", sz);
		return(0);
	}

	for (neg = 0, i = 0; i < (size_t)(cp - v); i++) {
		if ( ! isdigit(v[i])) {
			if (0 == i && '-' == v[i]) {
				neg = 1;
				continue;
			}
			fprintf(stderr, "Contains non-digits: %s (%c)\n", v, v[i]);
			return(0);
		}
	}
	for (i = (size_t)(cp - v) + 1; i < sz; i++) {
		if ( ! isdigit(v[i])) {
			fprintf(stderr, "Contains non-digits: %s (%c)\n", v, v[i]);
			return(0);
		}
	}

	/* If the dot's at the end, ignore it and MPQ it. */
	if (cp == v + sz - 1) {
		memcpy(buf, v, sz - 1);
		buf[sz - 1] = '\0';
		mpq_init(q);
		if (-1 == mpq_set_str(q, buf, 10)) {
			mpq_clear(q);
			fprintf(stderr, "MPQ didn't like: %s\n", buf);
			return(0);
		}
		fprintf(stderr, "Number clear: %s\n", buf);
		mpq_canonicalize(q);
		return(1);
	}

	/* Copy in the left of the decimal. */
	memcpy(buf, v, cp - v);
	buf[cp - v] = '\0';
	mul = strtonum(buf, LONG_MIN, LONG_MAX, &er);
	if (NULL != er) {
		fprintf(stderr, "Not a number: %s\n", buf);
		return(0);
	}

	mpq_init(tmp);
	if (mpq_set_str(tmp, buf, 10) < 0) {
		mpq_clear(tmp);
		fprintf(stderr, "MPQ didn't like: %s\n", buf);
	}
	mpq_canonicalize(tmp);
	gmp_fprintf(stderr, "Mul-check: %Qd\n", tmp);

	/* Copy in the decimal part. */
	memcpy(buf, cp + 1, sz - (cp - v) - 1);
	buf[sz - (cp - v) - 1] = '\0';
	num = strtonum(buf, 0, LONG_MAX, &er);
	if (NULL != er) {
		fprintf(stderr, "Not a number: %s\n", buf);
		mpq_clear(tmp);
		return(0);
	}

	/* Maximum precision: 4 (thousandths). */
	if ((precision = strlen(buf)) > 4) {
		fprintf(stderr, "Too big precision: %zu\n", precision);
		mpq_clear(tmp);
		return(0);
	}

	den = 1;
	for (i = 0; i < precision; i++)
		den *= 10;

	mpq_init(q);
	fprintf(stderr, "Trying: %lu/%lu\n", num, den);
	mpq_set_ui(q, num, den);
	mpq_canonicalize(q);

	mpq_init(stor);
	mpq_set(stor, q);
	if (neg)
		mpq_sub(q, tmp, stor);
	else
		mpq_add(q, stor, tmp);

	gmp_fprintf(stderr, "Check: %Qd\n", q);
	mpq_clear(tmp);
	mpq_clear(stor);
	return(1);
}

int
mpq_str2mpqu(const char *v, mpq_t q)
{

	if ( ! mpq_str2mpq(v, q))
		return(0);
	return(mpq_sgn(q) >= 0);
}

void
mpq_summation(mpq_t op, const mpq_t rop)
{
	mpq_t	 tmp;

	mpq_init(tmp);
	mpq_set(tmp, op);
	mpq_add(op, tmp, rop);
	mpq_clear(tmp);
}

void
mpq_summation_str(mpq_t op, const unsigned char *v)
{
	int	 rc;
	mpq_t	 tmp;

	mpq_init(tmp);
	rc = mpq_set_str(tmp, (const char *)v, 10);
	assert(0 == rc);
	mpq_canonicalize(tmp);
	mpq_summation(op, tmp);
	mpq_clear(tmp);
}
