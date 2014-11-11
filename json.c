/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
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

void
json_putexpr(struct kreq *r, const struct expr *expr)
{
	double	 frac;
	time_t	 t, tilstart, tilnext;
	int64_t	 round;

	frac = 0.0;
	tilstart = tilnext = 0;

	/* 
	 * First compute the fraction of completion, the current round,
	 * time til start (if applicable), and time til next play (if
	 * applicable).
	 */
	if ((t = time(NULL)) > expr->start) {
		round = (t - expr->start) / (expr->minutes * 60);
		if (t >= expr->end) {
			round = expr->rounds - 1;
			frac = 1.0;
			tilstart = tilnext = -1;
		} else {
			frac = (t - expr->start) / (double)(expr->end - expr->start);
			tilnext = ((round + 1) * (expr->minutes * 60)) - (t - expr->start);
		}
	} else {
		round = -1;
		tilnext = tilstart = expr->start - t;
	}

	json_puts(r, "expr");
	khttp_putc(r, ':');
	khttp_putc(r, '{');
	json_putint(r, "start", (int64_t)expr->start);
	khttp_putc(r, ',');
	json_putint(r, "end", (int64_t)expr->end);
	khttp_putc(r, ',');
	json_putint(r, "rounds", expr->rounds);
	khttp_putc(r, ',');
	json_putdouble(r, "progress", frac);
	khttp_putc(r, ',');
	json_putint(r, "tilstart", (int64_t)tilstart);
	khttp_putc(r, ',');
	json_putint(r, "tilnext", (int64_t)tilnext);
	khttp_putc(r, ',');
	json_putint(r, "round", round);
	khttp_putc(r, '}');
}

/*
 * Put a quoted JSON string key and array of rational fractions.
 */
void
json_putmpqs(struct kreq *r, const char *key, 
	mpq_t *vals, int64_t p1, int64_t p2)
{
	int64_t		i, j;
	char		buf[128];

	assert('\0' != *key);
	json_puts(r, key);
	khttp_puts(r, " : ");
	khttp_putc(r, '[');

	for (i = 0; i < p1; i++) {
		if (i > 0)
			khttp_putc(r, ',');
		khttp_putc(r, '[');
		for (j = 0; j < p2; j++) {
			if (j > 0)
				khttp_putc(r, ',');
			khttp_putc(r, '[');
			gmp_snprintf(buf, sizeof(buf), "%Qd", vals[i * (p2 * 2) + (j * 2)]);
			json_puts(r, buf);
			khttp_putc(r, ',');
			gmp_snprintf(buf, sizeof(buf), "%Qd", vals[i * (p2 * 2) + (j * 2) + 1]);
			json_puts(r, buf);
			khttp_putc(r, ']');
		}
		khttp_putc(r, ']');
	}
	khttp_putc(r, ']');
}

