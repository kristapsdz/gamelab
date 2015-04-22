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
#include <sys/param.h>

#include <assert.h>
#include <inttypes.h>
#include <math.h>
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

struct	intvstor {
	struct interval	*intv;
	struct kjsonreq	*req;
};

struct	poffstor {
	int64_t		 playerid;
	int64_t		 round;
	struct kjsonreq	*req;
};

/*
 * Unique pages under the CGI "directory".
 */
enum	page {
	PAGE_DOAUTOADD,
	PAGE_DOINSTR,
	PAGE_DOLOADEXPR,
	PAGE_DOLOGIN,
	PAGE_DOLOGOUT,
	PAGE_DOPLAY,
	PAGE_INDEX,
	PAGE__MAX
};

/*
 * Input form field names.
 */
enum	key {
	KEY_EMAIL,
	KEY_GAMEID,
	KEY_INSTR,
	KEY_PASSWORD,
	KEY_ROUND,
	KEY_SESSCOOKIE,
	KEY_SESSID,
	KEY__MAX
};

#define	PERM_LOGIN	0x01
#define	PERM_HTML	0x08
#define	PERM_JSON	0x10

static	unsigned int perms[PAGE__MAX] = {
	PERM_JSON, /* PAGE_DOAUTOADD */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOINSTR */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOLOADEXPR */
	PERM_JSON | PERM_HTML, /* PAGE_DOLOGIN */
	PERM_HTML | PERM_LOGIN, /* PAGE_DOLOGOUT */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOPLAY */
	PERM_HTML | PERM_LOGIN, /* PAGE_INDEX */
};

static const char *const pages[PAGE__MAX] = {
	"doautoadd", /* PAGE_DOAUTOADD */
	"doinstr", /* PAGE_DOINSTR */
	"doloadexpr", /* PAGE_DOLOADEXPR */
	"dologin", /* PAGE_DOLOGIN */
	"dologout", /* PAGE_DOLOGOUT */
	"doplay", /* PAGE_DOPLAY */
	"index", /* PAGE_INDEX */
};

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_email, "email" }, /* KEY_EMAIL */
	{ kvalid_int, "gid" }, /* KEY_GAMEID */
	{ NULL, "instr" }, /* KEY_INSTR */
	{ kvalid_stringne, "password" }, /* KEY_PASSWORD */
	{ kvalid_uint, "round" }, /* KEY_ROUND */
	{ kvalid_int, "sesscookie" }, /* KEY_SESSCOOKIE */
	{ kvalid_int, "sessid" }, /* KEY_SESSID */
};

/*
 * Check if the player session identified in the cookiemap is valid.
 * We must do so with every attempt to hit the site.
 */
static int
sess_valid(struct kreq *r, int64_t *id)
{

	if (NULL == r->cookiemap[KEY_SESSID] ||
		NULL == r->cookiemap[KEY_SESSCOOKIE]) 
		return(0);

	return(db_player_sess_valid
		(id,
		 r->cookiemap[KEY_SESSID]->parsed.i,
		 r->cookiemap[KEY_SESSCOOKIE]->parsed.i));
}

static int
kpairbad(struct kreq *r, enum key key)
{

	return(NULL == r->fieldmap[key] || NULL != r->fieldnmap[key]);
}

static int
player_valid(struct kreq *r, int64_t *playerid)
{

	fprintf(stderr, "email: %p\n", r->fieldmap[KEY_EMAIL]);
	fprintf(stderr, "pass: %p\n", r->fieldmap[KEY_PASSWORD]);

	if (kpairbad(r, KEY_EMAIL) || kpairbad(r, KEY_PASSWORD))
		return(0);

	return(db_player_valid
		(playerid,
		 r->fieldmap[KEY_EMAIL]->parsed.s,
		 r->fieldmap[KEY_PASSWORD]->parsed.s));
}

static void
http_open(struct kreq *r, enum khttp http)
{

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[http]);
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r->mime]);
	khttp_head(r, kresps[KRESP_CACHE_CONTROL], 
		"%s", "no-cache, no-store");
	khttp_head(r, kresps[KRESP_PRAGMA], 
		"%s", "no-cache");
}

static void
send404(struct kreq *r)
{

	http_open(r, KHTTP_404);
	khttp_body(r);
}

static void
send303(struct kreq *r, const char *pg, enum page dest, int st)
{
	char	*page, *full;

	if (st)
		http_open(r, KHTTP_303);

	if (NULL == pg) {
		page = kutil_urlpart(r, r->pname, 
			ksuffixes[r->mime], pages[dest], NULL);
		full = kutil_urlabs(r->scheme, r->host, r->port, page);
	} else {
		page = NULL;
		full = kutil_urlabs(r->scheme, r->host, r->port, pg);
	}

	khttp_head(r, kresps[KRESP_LOCATION], "%s", full);
	khttp_body(r);
	khttp_puts(r, "Redirecting...");
	free(full);
	free(page);
}

/*
 * Log in a player.
 * This depends on whether we're using JSON or HTML.
 * In the latter case, we need to format error messages; in the former
 * case, we can just return the right HTTP error code.
 * Returns error 409 if the experiment hasn't started (and JSON).
 * Returns error 303 if the experiment hasn't started (and HTML).
 * Returns error 303 if the player login is bad (and HTML).
 * Returns error 400 if the player login is bad (and JSON).
 * Returns error 200 otherwise.
 */
static void
senddologin(struct kreq *r)
{
	struct sess	*sess;
	int64_t	 	 playerid;
	struct expr	*expr;

	sess = NULL;

	if (NULL == (expr = db_expr_get(1))) {
		if (KMIME_TEXT_HTML == r->mime) {
			http_open(r, KHTTP_303);
			send303(r, HTURI "/playerlogin.html", PAGE__MAX, 0);
		} else {
			http_open(r, KHTTP_409);
			khttp_body(r);
		}
	} else if (player_valid(r, &playerid)) {
		sess = db_player_sess_alloc(playerid);
		assert(NULL != sess);
		if (KMIME_TEXT_HTML == r->mime) 
			http_open(r, KHTTP_303);
		else
			http_open(r, KHTTP_200);
		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRId64 "; path=/; expires=", 
			keys[KEY_SESSCOOKIE].name, sess->cookie);
		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRId64 "; path=/; expires=", 
			keys[KEY_SESSID].name, sess->id);
		if (KMIME_TEXT_HTML == r->mime)
			send303(r, HTURI "/playerhome.html", PAGE__MAX, 0);
		else
			khttp_body(r);
	} else {
		if (KMIME_TEXT_HTML == r->mime) {
			http_open(r, KHTTP_303);
			send303(r, HTURI "/playerlogin.html", PAGE__MAX, 0);
		} else {
			http_open(r, KHTTP_400);
			khttp_body(r);
		}
	}

	db_expr_free(expr);
	db_sess_free(sess);
}

static void
senddoloadplays(const struct game *game, void *arg)
{
	struct poffstor	*p = arg;
	struct kjsonreq	*req = p->req;
	mpq_t		*mpq,  poff;
	size_t		 i, sz;
	
	mpq = db_choices_get(p->round, p->playerid, game->id, &sz);
	if (NULL == mpq) {
		kjson_putnull(req);
		return;
	}

	kjson_obj_open(req);
	if (db_payoff_get(p->round, p->playerid, game->id, poff)) {
		kjson_putdoublep(req, "poff", mpq_get_d(poff));
		mpq_clear(poff);
	} else
		kjson_putintp(req, "poff", 0);

	kjson_arrayp_open(req, "strats");
	for (i = 0; i < sz; i++) {
		json_putmpq(req, mpq[i]);
		mpq_clear(mpq[i]);
	}
	kjson_array_close(req);
	kjson_obj_close(req);
	free(mpq);
}

static void
senddoloadhistory(const struct game *game, void *arg)
{
	struct intvstor	*p = arg;
	struct period	*per;
	struct kjsonreq	*req = p->req;
	size_t		 i;

	kjson_obj_open(req);
	kjson_putintp(req, "p1", game->p1);
	kjson_putintp(req, "p2", game->p2);
	kjson_putstringp(req, "name", game->name);
	json_putmpqs(req, "payoffs", 
		game->payoffs, game->p1, game->p2);
	kjson_putintp(req, "id", game->id);
	kjson_arrayp_open(req, "roundups");
	if (NULL == p->intv) {
		kjson_array_close(req);
		kjson_obj_close(req);
		return;
	}
	for (i = 0; i < p->intv->periodsz; i++) 
		if (game->id == p->intv->periods[i].gameid)
			break;
	assert(i < p->intv->periodsz);
	per = &p->intv->periods[i];
	for (i = 0; i < per->roundupsz; i++)
		json_putroundup(req, NULL, per->roundups[i]);
	kjson_array_close(req);
	kjson_obj_close(req);
}

static void
senddoloadgame(const struct game *game, int64_t round, void *arg)
{
	struct intvstor	*p = arg;
	struct kjsonreq	*req = p->req;
	struct period	*per;
	size_t		 i;

	if (NULL == game) {
		kjson_putnull(req);
		return;
	}
	kjson_obj_open(req);
	kjson_putintp(req, "p1", game->p1);
	kjson_putintp(req, "p2", game->p2);

	kjson_putstringp(req, "name", game->name);
	json_putmpqs(req, "payoffs", 
		game->payoffs, game->p1, game->p2);
	kjson_putintp(req, "id", game->id);

	kjson_arrayp_open(req, "p1order");
	kjson_array_close(req);

	if (NULL == p->intv) {
		kjson_putnullp(req, "roundup");
		kjson_obj_close(req);
		return;
	}

	for (i = 0; i < p->intv->periodsz; i++) 
		if (game->id == p->intv->periods[i].gameid)
			break;
	assert(i < p->intv->periodsz);
	per = &p->intv->periods[i];
	json_putroundup(req, "roundup", per->roundups[round - 1]);
	kjson_obj_close(req);
}

/*
 * Configure "auto-add" players: players enter the e-mail addresses in
 * the configuration phase.
 * Returns error 400 if the e-mail is bad.
 * Returns error 403 if the e-mail is already registered.
 * Returns error 404 if the experiment doesn't use auto-adding.
 * Returns error 409 if the experiment has started.
 * Otherwise returns error 200 and a JSON with the password and email.
 */
static void
senddoautoadd(struct kreq *r)
{
	int		 rc;
	char		*hash;
	struct kjsonreq	 req;

	if ( ! db_expr_getautoadd()) {
		http_open(r, KHTTP_404);
		khttp_body(r);
		return;
	} else if (NULL == r->fieldmap[KEY_EMAIL]) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} 

	rc = db_player_create
		(r->fieldmap[KEY_EMAIL]->parsed.s, &hash);

	if (rc < 0) {
		http_open(r, KHTTP_409);
		khttp_body(r);
	} else if (0 == rc) {
		http_open(r, KHTTP_403);
		khttp_body(r);
	} else {
		http_open(r, KHTTP_200);
		khttp_body(r);
		kjson_open(&req, r);
		kjson_obj_open(&req);
		kjson_putstringp(&req, "email", 
			r->fieldmap[KEY_EMAIL]->parsed.s);
		kjson_putstringp(&req, "password", hash);
		kjson_obj_close(&req);
		kjson_close(&req);
	}

	free(hash);
}

static void
senddoinstr(struct kreq *r, int64_t playerid)
{

	db_player_set_instr(playerid, 
		NULL != r->fieldmap[KEY_INSTR]);
	http_open(r, KHTTP_200);
	khttp_body(r);
}

static void
senddowinrnums(const struct player *p, 
	const struct winner *w, void *arg)
{
	struct kjsonreq	*r = arg;

	kjson_putint(r, w->rnum);
}

static void
senddoloadexpr(struct kreq *r, int64_t playerid)
{
	struct expr	*expr;
	struct player	*player;
	struct intvstor	 stor;
	struct poffstor	 pstor;
	struct winner	*win;
	mpq_t		 cur, aggr;
	time_t		 t;
	int64_t	 	 i, round;
	size_t		 gamesz;
	struct kjsonreq	 req;

	/* All response have at least the following. */
	if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	}
	player = db_player_load(playerid);
	assert(NULL != player);
	memset(&stor, 0, sizeof(struct intvstor));
	memset(&pstor, 0, sizeof(struct poffstor));
	t = time(NULL);

	http_open(r, KHTTP_200);
	khttp_body(r);
	kjson_open(&req, r);
	kjson_obj_open(&req);

	/*
	 * If the experiment hasn't started yet, have it contain nothing
	 * other than the experiment data itself.
	 */
	if (t < expr->start) {
		json_putexpr(&req, expr);
		kjson_putnullp(&req, "winner");
		kjson_putintp(&req, "colour", playerid % 12);
		kjson_putintp(&req, "ocolour", (playerid + 6) % 12);
		kjson_putintp(&req, "rseed", player->rseed);
		kjson_putintp(&req, "role", player->role);
		kjson_putintp(&req, "instr", player->instr);
		kjson_putintp(&req, "finalrank", player->finalrank);
		kjson_putintp(&req, "finalscore", player->finalscore);
		goto out;
	}

	/* Compute current round. */
	round = t >= expr->end ?  expr->rounds :
		(t - expr->start) / (expr->minutes * 60);

	/*
	 * This invokes the underlying "roundup" machinery.
	 * When it completes, we'll have computed the payoffs for all
	 * prior games.
	 * It returns NULL if and only if we have no history.
	 */
	stor.req = &req;
	if (NULL != (stor.intv = db_interval_get(round - 1)))
		gamesz = stor.intv->periodsz;
	else
		gamesz = db_game_count_all();

	/* 
	 * If the experiment is over but we don't have a total yet,
	 * refresh the experiment object to reflect the total number of
	 * lottery tickets.
	 */
	if (t >= expr->end && expr->state < ESTATE_PREWIN) {
		db_expr_finish(&expr, gamesz);
		/* Reload with new final ranking... */
		db_player_free(player);
		player = db_player_load(playerid);
	}

	json_putexpr(&req, expr);
	if (ESTATE_POSTWIN == expr->state) {
		kjson_arrayp_open(&req, "winrnums");
		db_winners_load_all(&req, senddowinrnums);
		kjson_array_close(&req);
		if (NULL != (win = db_winners_get(playerid))) {
			kjson_putintp(&req, "winner", win->rank);
			kjson_putintp(&req, "winrnum", win->rnum);
			db_winners_free(win);
		} else {
			kjson_putintp(&req, "winner", -1);
			kjson_putdoublep(&req, "winrnum", 0.0);
		}
	} else
		kjson_putnullp(&req, "winner");

	kjson_putintp(&req, "finalrank", player->finalrank);
	kjson_putintp(&req, "finalscore", player->finalscore);
	kjson_putintp(&req, "colour", playerid % 12);
	kjson_putintp(&req, "ocolour", (playerid + 6) % 12);
	kjson_putintp(&req, "rseed", player->rseed);
	kjson_putintp(&req, "role", player->role);
	kjson_putintp(&req, "instr", player->instr);

	/*
	 * This invokes the underlying lottery computation.
	 * This will compute the lottery for (right now) only the last
	 * lottery sequence.
	 */
	if (db_player_lottery(round - 1, playerid, cur, aggr, gamesz)) {
		kjson_putdoublep(&req, "curlottery", mpq_get_d(cur));
		kjson_putdoublep(&req, "aggrlottery", mpq_get_d(aggr));
		mpq_clear(cur);
		mpq_clear(aggr);
	} else {
		kjson_putnullp(&req, "curlottery");
		kjson_putnullp(&req, "aggrlottery");
	}

	/* 
	 * Send the remaining games.
	 * This will not reference the games that we've already played.
	 * We use the games computed during roundup.
	 */
	kjson_putintp(&req, "gamesz", gamesz);
	kjson_arrayp_open(&req, "games");
	db_game_load_player(playerid, 
		round, senddoloadgame, &stor);
	kjson_array_close(&req);

	/*
	 * Send all games and all game histories.
	 * This exhaustively catalogues the game sequence.
	 */
	kjson_arrayp_open(&req, "history");
	db_game_load_all(senddoloadhistory, &stor);
	kjson_array_close(&req);

	pstor.playerid = playerid;
	pstor.req = &req;
	kjson_arrayp_open(&req, "lotteries");
	for (i = 0; i < round; i++) {
		db_player_lottery(i, playerid, cur, aggr, gamesz);
		kjson_obj_open(&req);
		kjson_putdoublep(&req, "curlottery", mpq_get_d(cur));
		kjson_putdoublep(&req, "aggrlottery", mpq_get_d(aggr));
		mpq_clear(cur);
		mpq_clear(aggr);
		kjson_arrayp_open(&req, "plays");
		pstor.round = i;
		db_game_load_all(senddoloadplays, &pstor);
		kjson_array_close(&req);
		kjson_obj_close(&req);
	}
	kjson_array_close(&req);

out:
	kjson_obj_close(&req);
	kjson_close(&req);
	db_player_free(player);
	db_expr_free(expr);
	db_interval_free(stor.intv);
}

static void
senddoplay(struct kreq *r, int64_t playerid)
{
	struct player	 *player;
	struct game	 *game;
	size_t		  i, j, strats;
	char		  buf[128];
	mpq_t		  sum, one;
	mpq_t		 *mixes;

	player = NULL;
	game = NULL;
	mixes = NULL;
	strats = 0;
	mpq_init(one);
	mpq_init(sum);

	/* 
	 * First, make sure our basic parameters are there (game
	 * identifier, round, and the game itself).
	 */
	if (kpairbad(r, KEY_GAMEID) ||
		kpairbad(r, KEY_ROUND) ||
		NULL == (game = db_game_load
			(r->fieldmap[KEY_GAMEID]->parsed.i))) {
		http_open(r, KHTTP_400);
		goto out;
	}

	/* Get our player handle (for the role). */
	player = db_player_load(playerid);
	assert(NULL != player);

	/* 
	 * We'll need this number of strategies.
	 * Initialise the strategy array right now.
	 */
	strats = (0 == player->role) ? game->p1 : game->p2;
	mixes = kcalloc(strats, sizeof(mpq_t));

	/*
	 * Look up each strategy array in the field array.
	 * If we don't find it, or it's empty, then pretend that we've
	 * just entered zero.
	 * Otherwise, make sure it's a valid number and canonicalise it.
	 * If anything goes wrong, punt to KHTTP_400.
	 */
	for (i = 0; i < strats; i++) {
		(void)snprintf(buf, sizeof(buf), "index%zu", i);
		for (j = 0; j < r->fieldsz; j++)
			if (0 == strcmp(r->fields[j].key, buf))
				break;

		if (j == r->fieldsz) {
			mpq_init(mixes[i]);
			continue;
		} else if ( ! kvalid_stringne(&r->fields[j])) {
			mpq_init(mixes[i]);
			continue;
		}
		
		if ( ! mpq_str2mpqu(r->fields[j].val, mixes[i])) {
			http_open(r, KHTTP_400);
			for ( ; i < strats; i++)
				mpq_init(mixes[i]);
			goto out;
		}

		mpq_summation(sum, mixes[i]);
	}

	mpq_set_ui(one, 1, 1);
	mpq_canonicalize(one);

	if ( ! mpq_equal(one, sum)) {
		http_open(r, KHTTP_400);
		goto out;
	}

	/*
	 * Actually play the game by assigning an array of strategies to
	 * a given round, game, and player.
	 * This can fail for many reasons, in which case we kick to
	 * KHTTP_400 and depend on the frontend.
	 */
	if ( ! db_player_play
		(player->id, r->fieldmap[KEY_ROUND]->parsed.i,
		 game->id, mixes, strats))
		http_open(r, KHTTP_409);
	else
		http_open(r, KHTTP_200);

out:
	/*
	 * Clean up.
	 * Assume we've already opened our HTTP response with the code,
	 * so just pump our empty body and free memory.
	 */
	khttp_body(r);
	if (NULL != mixes)
		for (i = 0; i < strats; i++)
			mpq_clear(mixes[i]);
	free(mixes);
	mpq_clear(one);
	mpq_clear(sum);
	db_player_free(player);
	db_game_free(game);
}

static void
senddologout(struct kreq *r)
{

	db_sess_delete(r->cookiemap[KEY_SESSID]->parsed.i);
	http_open(r, KHTTP_303);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSCOOKIE].name);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSID].name);
	send303(r, HTURI "/playerlogin.html", PAGE__MAX, 0);
}

int
main(void)
{
	struct kreq	 r;
	int64_t		 id;
	unsigned int	 bit;
	
	if (KCGI_OK != khttp_parse(&r, keys, KEY__MAX, 
			pages, PAGE__MAX, PAGE_INDEX))
		return(EXIT_FAILURE);

	switch (r.method) {
	case (KMETHOD_GET):
	case (KMETHOD_POST):
		break;
	case (KMETHOD_OPTIONS):
		khttp_head(&r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_head(&r, kresps[KRESP_ALLOW],
			"GET POST OPTIONS");
		khttp_body(&r);
		goto out;
	default:
		http_open(&r, KHTTP_405);
		khttp_body(&r);
		goto out;
	}

	switch (r.mime) {
	case (KMIME_TEXT_HTML):
		bit = PERM_HTML;
		break;
	case (KMIME_APP_JSON):
		bit = PERM_JSON;
		break;
	default:
		send404(&r);
		goto out;
	}

	if ( ! (perms[r.page] & bit)) {
		send404(&r);
		goto out;
	}

	if ((perms[r.page] & PERM_LOGIN) && ! sess_valid(&r, &id)) {
		if (KMIME_TEXT_HTML != r.mime) {
			http_open(&r, KHTTP_403);
			khttp_body(&r);
		} else
			send303(&r, HTURI "/playerlogin.html", 
				PAGE__MAX, 1);
		goto out;
	}

	switch (r.page) {
	case (PAGE_DOAUTOADD):
		senddoautoadd(&r);
		break;
	case (PAGE_DOINSTR):
		senddoinstr(&r, id);
		break;
	case (PAGE_DOLOADEXPR):
		senddoloadexpr(&r, id);
		break;
	case (PAGE_DOLOGIN):
		senddologin(&r);
		break;
	case (PAGE_DOLOGOUT):
		senddologout(&r);
		break;
	case (PAGE_DOPLAY):
		senddoplay(&r, id);
		break;
	case (PAGE_INDEX):
		send303(&r, HTURI "/playerhome.html", PAGE__MAX, 1);
		break;
	default:
		http_open(&r, KHTTP_404);
		khttp_body(&r);
		break;
	}

out:
	khttp_free(&r);
	return(EXIT_SUCCESS);
}

