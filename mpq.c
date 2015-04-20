/*	$Id$ */
/*
 * Copyright (c) 2014--2015 Kristaps Dzonsons <kristaps@kcons.eu>
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
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <bsd/stdlib.h>
#include <bsd/string.h>
#endif
#include <time.h>

#include <gmp.h>
#include <kcgi.h>
#include <kcgijson.h>

#include "extern.h"

#ifdef __APPLE__
/*	$OpenBSD$	*/
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
#include <errno.h>

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

void
mpq_str2mpqinit(const unsigned char *v, mpq_t val)
{
	int	 rc;

	mpq_init(val);
	rc = mpq_set_str(val, (const char *)v, 10);
	assert(0 == rc);
	mpq_canonicalize(val);
}

mpq_t *
mpq_str2mpqsinit(const unsigned char *v, size_t sz)
{
	mpq_t	*p;
	size_t	 i;
	char	*buf, *tok, *sv;
	int	 rc;

	p = kcalloc(sz, sizeof(mpq_t));

	if (1 == sz) {
		mpq_init(p[0]);
		rc = mpq_set_str(p[0], (const char *)v, 10);
		assert(0 == rc);
		mpq_canonicalize(p[0]);
		return(p);
	}

	buf = sv = kstrdup((const char *)v);

	i = 0;
	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		assert(i < sz);
		mpq_init(p[i]);
		rc = mpq_set_str(p[i], tok, 10);
		assert(0 == rc);
		mpq_canonicalize(p[i]);
		i++;
	}

	free(sv);
	return(p);
}

int
mpq_str2mpq(const char *v, mpq_t q)
{
	char		 buf[64];
	const char	*cp, *er;
	unsigned long	 den, num;
	int		 neg;
	size_t		 i, sz, precision;
	mpq_t		 tmp, stor;

	/* First pass: MPQ all non-dotted numbers. */
	if (NULL == (cp = strchr(v, '.'))) {
		mpq_init(q);
		if (-1 == mpq_set_str(q, v, 10)) {
			mpq_clear(q);
			return(0);
		}
		mpq_canonicalize(q);
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
	if (sz > sizeof(buf) - 1)
		return(0);

	/*
	 * Test to make sure the characters are valid.
	 * We also add a check for negation, here.
	 */
	for (neg = 0, i = 0; i < (size_t)(cp - v); i++) 
		if ( ! isdigit(v[i])) {
			if (0 == i && '-' == v[i]) {
				neg = 1;
				continue;
			}
			return(0);
		}

	for (i = (size_t)(cp - v) + 1; i < sz; i++) 
		if ( ! isdigit(v[i]))
			return(0);

	/* If the dot's at the end, ignore it and MPQ it. */
	if (cp == v + sz - 1) {
		memcpy(buf, v, sz - 1);
		buf[sz - 1] = '\0';
		mpq_init(q);
		if (-1 == mpq_set_str(q, buf, 10)) {
			mpq_clear(q);
			return(0);
		}
		mpq_canonicalize(q);
		return(1);
	}

	/* Copy in the left of the decimal. */
	memcpy(buf, v, cp - v);
	buf[cp - v] = '\0';
	(void)strtonum(buf, LONG_MIN, LONG_MAX, &er);
	if (NULL != er)
		return(0);

	mpq_init(tmp);
	if (mpq_set_str(tmp, buf, 10) < 0) {
		mpq_clear(tmp);
		return(0);
	}

	mpq_canonicalize(tmp);

	/* Copy in the decimal part. */
	memcpy(buf, cp + 1, sz - (cp - v) - 1);
	buf[sz - (cp - v) - 1] = '\0';
	num = strtonum(buf, 0, LONG_MAX, &er);
	if (NULL != er) {
		mpq_clear(tmp);
		return(0);
	}

	/* Maximum precision: 4 (thousandths). */
	if ((precision = strlen(buf)) > 4) {
		mpq_clear(tmp);
		return(0);
	}

	/* Compute our precision-based denominator. */
	for (den = 1, i = 0; i < precision; i++)
		den *= 10;

	mpq_init(q);
	mpq_set_ui(q, num, den);
	mpq_canonicalize(q);

	mpq_init(stor);
	mpq_set(stor, q);

	/* Negate or accumulate. */
	if (neg)
		mpq_sub(q, tmp, stor);
	else
		mpq_add(q, stor, tmp);

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
mpq_summation_strvec(mpq_t *ops, const unsigned char *v, size_t sz)
{
	char	*buf, *sv, *tok;
	size_t	 i;

	buf = sv = kstrdup((const char *)v);

	i = 0;
	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		assert(i < sz);
		mpq_summation_str(ops[i], (unsigned char *)tok);
		i++;
	}

	free(sv);
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

char *
mpq_mpq2str(mpq_t *v, size_t sz)
{
	char	*p, *tmp;
	size_t	 i, psz;

	for (p = NULL, psz = i = 0; i < sz; i++) {
		gmp_asprintf(&tmp, "%Qd", v[i]);
		psz += strlen(tmp) + 2;
		p = krealloc(p, psz);
		if (i > 0)
			(void)strlcat(p, " ", psz);
		else
			p[0] = '\0';
		(void)strlcat(p, tmp, psz);
		free(tmp);
	}

	return(p);
}
