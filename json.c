/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <gmp.h>
#include <kcgi.h>

#include "extern.h"

/*
 * Put a quoted JSON string into the output stream.
 */
void
json_puts(struct kreq *r, const char *cp)
{
	int	 c;

	khttp_putc(r, '"');

	while ('\0' != (c = *cp++))
		switch (c) {
		case ('"'):
		case ('\\'):
			khttp_putc(r, '\\');
			/* FALLTHROUGH */
		default:
			khttp_putc(r, c);
			break;
		}

	khttp_putc(r, '"');
}

/*
 * Put a quoted JSON string key and integral value pair.
 */
void
json_putdouble(struct kreq *r, const char *key, double val)
{
	char	buf[256];

	(void)snprintf(buf, sizeof(buf), "%g", val);

	assert('\0' != *key);
	json_puts(r, key);
	khttp_puts(r, " : ");
	json_puts(r, buf);
}

/*
 * Put a quoted JSON string key and integral value pair.
 */
void
json_putint(struct kreq *r, const char *key, int64_t val)
{
	char	buf[22];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);

	assert('\0' != *key);
	json_puts(r, key);
	khttp_puts(r, " : ");
	json_puts(r, buf);
}

/*
 * Put a quoted JSON string key and string value pair.
 */
void
json_putstring(struct kreq *r, const char *key, const char *val)
{

	assert('\0' != *key);
	json_puts(r, key);
	khttp_puts(r, " : ");
	json_puts(r, val);
}

/*
 * Put a quoted JSON string key and array of rational fractions.
 */
void
json_putmpqs(struct kreq *r, const char *key, 
	mpq_t *vals, int64_t p1, int64_t p2)
{
	int64_t		i, j, k;
	char		buf[128];

	assert('\0' != *key);
	json_puts(r, key);
	khttp_puts(r, " : ");
	khttp_putc(r, '[');

	for (k = i = 0; i < p1; i++) {
		if (i > 0)
			khttp_putc(r, ',');
		khttp_putc(r, '[');
		for (j = 0; j < p2; j++, k++) {
			if (j > 0)
				khttp_putc(r, ',');
			gmp_snprintf(buf, sizeof(buf), "%Qd", vals[k]);
			json_puts(r, buf);
		}
		khttp_putc(r, ']');
	}
	khttp_putc(r, ']');
}

