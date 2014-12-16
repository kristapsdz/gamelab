/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include "config.h" 

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gmp.h>
#include <kcgi.h>
#include <kcgijson.h>

#include "extern.h"

enum	tkey {
	TKEY_ADMIN_EMAIL,
	TKEY__MAX
};

static	const char *const tkeys[TKEY__MAX] = {
	"gamelab-admin-email"
};

static int
json_instructions(size_t key, void *arg)
{
	struct kjsonreq	*r = arg;
	char		*cp = NULL;

	switch (key) {
	case (TKEY_ADMIN_EMAIL):
		cp = db_admin_get_mail();
		kjson_string_puts(r, cp);
		break;
	default:
		abort();
	}

	free(cp);
	return(1);
}


void
json_putexpr(struct kjsonreq *r, const struct expr *expr)
{
	double	 	 frac;
	time_t	 	 tt, tilstart, tilnext;
	int64_t	 	 round;
	struct ktemplate t;

	frac = 0.0;
	tilstart = tilnext = 0;

	/* 
	 * First compute the fraction of completion, the current round,
	 * time til start (if applicable), and time til next play (if
	 * applicable).
	 */
	if ((tt = time(NULL)) > expr->start) {
		round = (tt - expr->start) / (expr->minutes * 60);
		if (tt >= expr->end) {
			round = expr->rounds - 1;
			frac = 1.0;
			tilstart = tilnext = -1;
		} else {
			frac = (tt - expr->start) / 
				(double)(expr->end - expr->start);
			tilnext = ((round + 1) * 
				(expr->minutes * 60)) - 
				(tt - expr->start);
		}
	} else {
		round = -1;
		tilnext = tilstart = expr->start - tt;
	}

	kjson_objp_open(r, "expr");
	kjson_stringp_open(r, "instructions");
	memset(&t, 0, sizeof(struct ktemplate));
	t.key = tkeys;
	t.keysz = TKEY__MAX;
	t.arg = r;
	t.cb = json_instructions;
	khttp_templatex_buf(&t, expr->instructions, 
		strlen(expr->instructions), kjson_string_write, r);
	kjson_string_close(r);
	kjson_putintp(r, "start", (int64_t)expr->start);
	kjson_putintp(r, "end", (int64_t)expr->end);
	kjson_putintp(r, "rounds", expr->rounds);
	kjson_putdoublep(r, "progress", frac);
	kjson_putintp(r, "tilstart", (int64_t)tilstart);
	kjson_putintp(r, "tilnext", (int64_t)tilnext);
	kjson_putintp(r, "round", round);
	kjson_obj_close(r);
}

void
json_putmpqp(struct kjsonreq *r, const char *key, mpq_t val)
{
	char	*buf;

	gmp_asprintf(&buf, "%Qd", val);
	kjson_putstringp(r, key, buf);
	free(buf);
}

static void
json_putmpq(struct kjsonreq *r, mpq_t val)
{
	char	*buf;

	gmp_asprintf(&buf, "%Qd", val);
	kjson_putstring(r, buf);
	free(buf);
}

void
json_putroundup(struct kjsonreq *r, const char *name,
	const struct roundup *roundup)
{
	size_t	 i, j, k;

	if (NULL == roundup) {
		kjson_putnullp(r, name);
		return;
	}

	fprintf(stderr, "MOO: %zu, %zu\n", roundup->p1sz, roundup->p2sz);

	kjson_objp_open(r, name);
	kjson_arrayp_open(r, "avgp1");
	for (i = 0; i < roundup->p1sz; i++)
		json_putmpq(r, roundup->avgp1[i]);
	kjson_array_close(r);

	kjson_arrayp_open(r, "avgp2");
	for (i = 0; i < roundup->p2sz; i++)
		json_putmpq(r, roundup->avgp2[i]);
	kjson_array_close(r);

	kjson_arrayp_open(r, "avgs");
	for (k = i = 0; i < roundup->p1sz; i++) {
		kjson_array_open(r);
		for (j = 0; j < roundup->p2sz; j++, k++)
			json_putmpq(r, roundup->avg[k]);
		kjson_array_close(r);
	}
	kjson_array_close(r);
	kjson_obj_close(r);
}

/*
 * Put a quoted JSON string key and array of rational fractions.
 */
void
json_putmpqs(struct kjsonreq *r, const char *name,
	mpq_t *vals, int64_t p1, int64_t p2)
{
	int64_t		 i, j;

	kjson_arrayp_open(r, name);
	for (i = 0; i < p1; i++) {
		kjson_array_open(r);
		for (j = 0; j < p2; j++) {
			kjson_array_open(r);
			json_putmpq(r, vals[i * 
				(p2 * 2) + (j * 2)]);
			json_putmpq(r, vals[i * 
				(p2 * 2) + (j * 2) + 1]);
			kjson_array_close(r);
		}
		kjson_array_close(r);
	}
	kjson_array_close(r);
}

