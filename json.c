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
#include <signal.h>
#include <stdarg.h>
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

/*
 * Key identifiers used when templating the instructions.
 * See khttp_template(3) and family.
 */
enum	tkey {
	TKEY_ADMIN_EMAIL,
	TKEY_CONVERSION,
	TKEY_GAMES,
	TKEY_LOTTERY,
	TKEY_ROUND_MINTIME_HOURS,
	TKEY_ROUND_MINTIME_MINUTES,
	TKEY_ROUND_PERCENT,
	TKEY_ROUND_TIME_HOURS,
	TKEY_ROUND_TIME_MINUTES,
	TKEY_ROUNDS,
	TKEY_TIME_TOTAL_HOURS,
	TKEY_TIME_TOTAL_MINUTES,
	TKEY__MAX
};

static	const char *const tkeys[TKEY__MAX] = {
	"gamelab-admin-email", /* TKEY_ADMIN_EMAIL */
	"gamelab-conversion", /* TKEY_CONVERSION */
	"gamelab-games", /* TKEY_GAMES */
	"gamelab-lottery", /* TKEY_LOTTERY */
	"gamelab-round-mintime-hours", /* TKEY_ROUND_MINTIME_HOURS */
	"gamelab-round-mintime-minutes", /* TKEY_ROUND_MINTIME_MINUTES */
	"gamelab-round-percent", /* TKEY_ROUND_PERCENT */
	"gamelab-round-time-hours", /* TKEY_ROUND_TIME_HOURS */
	"gamelab-round-time-minutes", /* TKEY_ROUND_TIME_MINUTES */
	"gamelab-rounds", /* TKEY_ADMIN_EMAIL */
	"gamelab-time-total-hours", /* TKEY_TIME_TOTAL_HOURS */
	"gamelab-time-total-minutes", /* TKEY_TIME_TOTAL_MINUTES */
};

struct	jsoncache {
	char	 	  *mail; /* administrator email */
	struct kjsonreq	  *r; /* JSON object */
	int64_t		   games; /* number of games */
	const struct expr *expr; /* experiment data */
};

struct	intvcache {
	struct interval	*intv; /* game roundups */
	struct kjsonreq	*req; /* JSON object */
	int		 admin; /* whether privileged */
};

static int
json_instructions(size_t key, void *arg)
{
	struct jsoncache *c = arg;

	switch (key) {
	case (TKEY_ADMIN_EMAIL):
		kjson_string_puts(c->r, c->mail);
		break;
	case (TKEY_CONVERSION):
		kjson_string_putdouble(c->r, 
			c->expr->awsconvert);
		break;
	case (TKEY_GAMES):
		kjson_string_putint(c->r, c->games);
		break;
	case (TKEY_LOTTERY):
		kjson_string_puts(c->r, c->expr->lottery);
		break;
	case (TKEY_ROUND_MINTIME_HOURS):
		kjson_string_putdouble(c->r, 
			c->expr->roundmin / 60.0);
		break;
	case (TKEY_ROUND_MINTIME_MINUTES):
		kjson_string_putint(c->r, 
			c->expr->roundmin);
		break;
	case (TKEY_ROUND_PERCENT):
		kjson_string_putdouble(c->r, 
			c->expr->roundpct * 100.0);
		break;
	case (TKEY_ROUND_TIME_HOURS):
		kjson_string_putdouble(c->r, 
			c->expr->minutes / 60.0);
		break;
	case (TKEY_ROUND_TIME_MINUTES):
		kjson_string_putint(c->r, 
			c->expr->minutes);
		break;
	case (TKEY_ROUNDS):
		kjson_string_putint(c->r, 
			c->expr->rounds);
		break;
	case (TKEY_TIME_TOTAL_HOURS):
		kjson_string_putdouble(c->r, 
			(c->expr->minutes / 60.0) * 
			c->expr->rounds);
		break;
	case (TKEY_TIME_TOTAL_MINUTES):
		kjson_string_putdouble(c->r, 
			c->expr->minutes * 
			c->expr->rounds);
		break;
	default:
		abort();
	}

	return(1);
}

/*
 * Serialise a game's metadata (actions, payoffs, etc.) and a full
 * history of play as encoded in the `roundups' object.
 */
static void
json_putgamehistory(const struct game *g, void *arg)
{
	struct intvcache *p = arg;
	struct period	 *per;
	struct kjsonreq	 *req = p->req;
	size_t		  i;

	kjson_obj_open(req);
	kjson_putintp(req, "p1", g->p1);
	kjson_putintp(req, "p2", g->p2);
	kjson_putstringp(req, "name", g->name);
	json_putmpqs(req, "payoffs", g->payoffs, g->p1, g->p2);
	kjson_putintp(req, "id", g->id);
	kjson_arrayp_open(req, "roundups");

	if (NULL == p->intv) {
		kjson_array_close(req);
		kjson_obj_close(req);
		return;
	}

	for (i = 0; i < p->intv->periodsz; i++) 
		if (g->id == p->intv->periods[i].gameid)
			break;

	assert(i < p->intv->periodsz);
	per = &p->intv->periods[i];
	for (i = 0; i < per->roundupsz; i++)
		json_putroundup(req, NULL, per->roundups[i], p->admin);
	kjson_array_close(req);
	kjson_obj_close(req);
}

/*
 * Put a per-game array consisting of that game's metadata and full
 * history (if available).
 * If `expr' is NULL, puts a zero-length array.
 * If `intv' is NULL, it will fetch it from the database (and free it),
 * otherwise it is not freed.
 * If the game's history is NULL (i.e., we're on the first round), puts
 * a zero-length array of game-play history.
 */
void
json_puthistory(struct kjsonreq *r, int admin,
	const struct expr *expr, struct interval *intv)
{
	struct intvcache p;

	if (NULL == expr) {
		kjson_arrayp_open(r, "history");
		kjson_array_close(r);
		return;
	}

	/* 
	 * Fetch the full history record now. 
	 * Note: if history-showing has been disabled experiment-wide,
	 * then don't do it at all.
	 */
	if (NULL == intv && ! (EXPR_NOHISTORY & expr->flags))
		p.intv = db_interval_get(expr->round - 1);
	else
		p.intv = intv;

	p.req = r;
	p.admin = admin;

	kjson_arrayp_open(r, "history");
	db_game_load_all(json_putgamehistory, &p);
	kjson_array_close(r);

	if (NULL == intv)
		db_interval_free(p.intv);
}

void
json_putplayer(struct kjsonreq *r, const struct player *p)

{
	kjson_objp_open(r, "player");
	if ('\0' == *p->hitid)
		kjson_putnullp(r, "hitid");
	else
		kjson_putstringp(r, "hitid", p->hitid);
	if ('\0' == *p->assignmentid)
		kjson_putnullp(r, "assignmentid");
	else
		kjson_putstringp(r, "assignmentid", p->assignmentid);
	kjson_putstringp(r, "mail", p->mail);
	kjson_putintp(r, "rseed", p->rseed);
	kjson_putintp(r, "role", p->role);
	kjson_putintp(r, "instr", p->instr);
	kjson_putintp(r, "id", p->id);
	kjson_putintp(r, "mturkdone", p->mturkdone);
	kjson_putintp(r, "autoadd", p->autoadd);
	kjson_putintp(r, "joined", p->joined);
	kjson_putintp(r, "status", p->state);
	kjson_putintp(r, "enabled", p->enabled);
	kjson_putintp(r, "finalrank", p->finalrank);
	kjson_putintp(r, "finalscore", p->finalscore);
	kjson_putintp(r, "answer", p->answer);
	kjson_obj_close(r);
}

static int
json_writer(const char *cp, size_t sz, void *arg)
{
	return(KCGI_OK == kjson_string_write(cp, sz, arg));
}

void
json_putexpr(struct kreq *req, 
	struct kjsonreq *r, const struct expr *expr, int admin)
{
	double	 	  frac;
	time_t	 	  tt;
	struct ktemplate  t;
	struct ktemplatex tx;
	struct jsoncache  c;

	frac = 0.0;

	/* 
	 * First compute the fraction of completion, the current round,
	 * time til start (if applicable), and time til next play (if
	 * applicable).
	 */
	tt = time(NULL);
	if (expr->round >= 0 && expr->round < expr->rounds)
		frac = (expr->round / (double)expr->rounds) +
			(1.0 / (double)expr->rounds) * 
			((tt - expr->roundbegan) / 
			 (double)(expr->minutes * 60));
	else if (expr->round >= 0)
		frac = 1.0;

	kjson_objp_open(r, "expr");
	kjson_stringp_open(r, "instr");
	memset(&t, 0, sizeof(struct ktemplate));
	memset(&tx, 0, sizeof(struct ktemplatex));
	memset(&c, 0, sizeof(struct jsoncache));
	c.r = r;
	c.mail = db_admin_get_mail();
	c.expr = expr;
	c.games = db_game_count_all(); /* FIXME */
	t.key = tkeys;
	t.keysz = TKEY__MAX;
	t.arg = &c;
	t.cb = json_instructions;
	tx.writer = json_writer;
	khttp_templatex_buf(&t, expr->instr, 
		strlen(expr->instr), &tx, r);
	kjson_string_close(r);
	kjson_putintp(r, "maxtickets", expr->total);
	kjson_putstringp(r, "admin", c.mail);
	free(c.mail);
	kjson_putintp(r, "state", expr->state);
	kjson_putintp(r, "start", (int64_t)expr->start);
	kjson_putintp(r, "rounds", expr->rounds);
	kjson_putintp(r, "prounds", expr->prounds);
	kjson_putintp(r, "nohistory", EXPR_NOHISTORY & expr->flags);
	kjson_putintp(r, "noshuffle", EXPR_NOSHUFFLE & expr->flags);
	kjson_putintp(r, "absoluteround", ! (EXPR_RELATIVE & expr->flags));
	kjson_putintp(r, "sandbox", expr->awssandbox);
	kjson_putintp(r, "awssandbox", expr->awssandbox);
	kjson_putstringp(r, "lottery", expr->lottery);
	kjson_putdoublep(r, "conversion", expr->awsconvert);
	kjson_putdoublep(r, "awsconvert", expr->awsconvert);
	kjson_putdoublep(r, "awsreward", expr->awsreward);
	if (admin) {
		kjson_putstringp(r, "hitid", expr->hitid);
		kjson_putstringp(r, "awsaccesskey", expr->awsaccesskey);
		kjson_putstringp(r, "awssecretkey", expr->awssecretkey);
		kjson_putstringp(r, "awserror", expr->awserror);
		kjson_putstringp(r, "awsname", expr->awsname);
		kjson_putstringp(r, "awsdesc", expr->awsdesc);
		kjson_putstringp(r, "awskeys", expr->awskeys);
		kjson_putstringp(r, "awslocale", expr->awslocale);
		kjson_putintp(r, "awsworkers", expr->awsworkers);
		kjson_putintp(r, "awswhitappr", expr->awswhitappr);
		kjson_putintp(r, "awswpctappr", expr->awswpctappr);
	}
	kjson_putintp(r, "minutes", expr->minutes);
	kjson_putstringp(r, "loginURI", expr->loginuri);
	kjson_putdoublep(r, "progress", frac);
	kjson_putintp(r, "postwin", ESTATE_POSTWIN == expr->state);
	kjson_putintp(r, "round", expr->round);
	kjson_putdoublep(r, "roundpct", expr->roundpct * 100.0);
	kjson_putintp(r, "roundmin", expr->roundmin);
	kjson_putintp(r, "roundpid", expr->roundpid);
	kjson_putintp(r, "roundpidok", 
		expr->roundpid <= 0 ? -1 :
		kill(expr->roundpid, 0) < 0 ? 0 : 1);
	kjson_putintp(r, "roundbegan", expr->roundbegan);
	kjson_putintp(r, "autoadd", expr->autoadd);

	if ('\0' != *expr->history) {
		khttp_putc(req, ',');
		khttp_puts(req, expr->history);
	} else 
		kjson_putnullp(r, "history");

	kjson_putintp(r, "autoaddpreserve", expr->autoaddpreserve);
	kjson_obj_close(r);
}

void
json_putmpqp(struct kjsonreq *r, const char *key, const mpq_t val)
{
	char	*buf;

	gmp_asprintf(&buf, "%Qd", val);
	kjson_putstringp(r, key, buf);
	free(buf);
}

void
json_putmpq(struct kjsonreq *r, mpq_t val)
{
	char	*buf;

	gmp_asprintf(&buf, "%Qd", val);
	kjson_putstring(r, buf);
	free(buf);
}

void
json_putroundup(struct kjsonreq *r, const char *name,
	const struct roundup *roundup, int admin)
{
	size_t	 i, j, k;

	if (NULL == roundup) {
		kjson_putnullp(r, name);
		return;
	}

	kjson_objp_open(r, name);
	if (admin)
		kjson_putintp(r, "plays", roundup->plays);
	kjson_putintp(r, "skip", roundup->skip);
	kjson_arrayp_open(r, "navgp1");
	for (i = 0; i < roundup->p1sz; i++)
		kjson_putdouble(r, roundup->navgp1[i]);
	kjson_array_close(r);
	kjson_arrayp_open(r, "navgp2");
	for (i = 0; i < roundup->p2sz; i++)
		kjson_putdouble(r, roundup->navgp2[i]);
	kjson_array_close(r);
	kjson_arrayp_open(r, "navgs");
	for (k = i = 0; i < roundup->p1sz; i++) {
		kjson_array_open(r);
		for (j = 0; j < roundup->p2sz; j++, k++)
			kjson_putdouble(r, roundup->navg[k]);
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

