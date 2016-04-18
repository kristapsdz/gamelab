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
#include <ctype.h>
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

#define	QUESTIONS 8

/*
 * This structure is used to bundle several things into a single pointer
 * as used by the JSON outputter.
 * Specifically, it's used to bundle both an interval structure (all
 * rounds' histories) and the JSON request object.
 */
struct	intvstor {
	struct interval	*intv;
	struct kjsonreq	*req;
};

/*
 * This structure is used to bundle several things into a single pointer
 * as used by the JSON outputter.
 * Specifically, it holds the current player and current round along
 * with the JSON request object.
 */
struct	poffstor {
	int64_t		 playerid;
	int64_t		 round;
	struct kjsonreq	*req;
};

/*
 * Unique pages under the CGI "directory".
 */
enum	page {
	PAGE_DOANSWER,
	PAGE_DOAUTOADD,
	PAGE_DOCHECKROUND,
	PAGE_DOINSTR,
	PAGE_DOLOADEXPR,
	PAGE_DOLOADQUESTIONS,
	PAGE_DOLOGIN,
	PAGE_DOLOGOUT,
	PAGE_DOPLAY,
	PAGE_INDEX,
	PAGE_MTURK,
	PAGE_MTURKFINISH,
	PAGE__MAX
};

/*
 * Input form field names.
 */
enum	key {
	KEY_ANSWER0,
	KEY_ANSWERID,
	KEY_ASSIGNMENTID,
	KEY_CHOICE0,
	KEY_CHOICE1,
	KEY_CHOICE2,
	KEY_CHOICE3,
	KEY_CHOICE4A,
	KEY_CHOICE4B,
	KEY_CHOICE5A,
	KEY_CHOICE5B,
	KEY_CHOICE6,
	KEY_CHOICE7,
	KEY_GAMEID,
	KEY_HITID,
	KEY_IDENTIFIER,
	KEY_INSTR,
	KEY_PASSWORD,
	KEY_ROUND,
	KEY_SESSCOOKIE,
	KEY_SESSID,
	KEY_WORKERID,
	KEY__MAX
};

/*
 * Bit permissions on each page.
 * This will always consist of PERM_HTML and/or PERM_JSON.
 */
#define	PERM_LOGIN	0x01 /* need login */
#define	PERM_HTML	0x08 /* HTML page */
#define	PERM_JSON	0x10 /* JSON page */

static	unsigned int perms[PAGE__MAX] = {
	PERM_JSON | PERM_LOGIN, /* PAGE_DOANSWER */
	PERM_JSON, /* PAGE_DOAUTOADD */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOCHECKROUND */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOINSTR */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOLOADEXPR */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOLOADQUESTIONS */
	PERM_JSON | PERM_HTML, /* PAGE_DOLOGIN */
	PERM_HTML | PERM_LOGIN, /* PAGE_DOLOGOUT */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOPLAY */
	PERM_HTML | PERM_LOGIN, /* PAGE_INDEX */
	PERM_HTML, /* PAGE_MTURK */
	PERM_JSON | PERM_LOGIN, /* PAGE_MTURKFINISH */
};

static const char *const pages[PAGE__MAX] = {
	"doanswer", /* PAGE_DOANSWER */
	"doautoadd", /* PAGE_DOAUTOADD */
	"docheckround", /* PAGE_DOCHECKROUND */
	"doinstr", /* PAGE_DOINSTR */
	"doloadexpr", /* PAGE_DOLOADEXPR */
	"doloadquestions", /* PAGE_DOLOADQUESTIONS */
	"dologin", /* PAGE_DOLOGIN */
	"dologout", /* PAGE_DOLOGOUT */
	"doplay", /* PAGE_DOPLAY */
	"index", /* PAGE_INDEX */
	"mturk", /* PAGE_MTURK */
	"mturkfinish", /* PAGE_MTURKFINISH */
};

/*
 * The answers to each of our questions.
 * Note that 4 and 5 don't have specific answers: these are handled in
 * the answer code itself.
 */
static int kvalid_choice0(struct kpair *);
static int kvalid_choice1(struct kpair *);
static int kvalid_choice2(struct kpair *);
static int kvalid_choice3(struct kpair *);
static int kvalid_choice6(struct kpair *);
static int kvalid_choice7(struct kpair *);
static int kvalid_mstring(struct kpair *);
static int kvalid_rational(struct kpair *);

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_stringne, "answer0" }, /* KEY_ANSWER0 */
	{ kvalid_int, "aid" }, /* KEY_ANSWERID */
	{ kvalid_mstring, "assignmentId" }, /* KEY_ASSIGNMENTID */
	{ kvalid_choice0, "choice0" }, /* KEY_CHOICE0 */
	{ kvalid_choice1, "choice1" }, /* KEY_CHOICE1 */
	{ kvalid_choice2, "choice2" }, /* KEY_CHOICE2 */
	{ kvalid_choice3, "choice3" }, /* KEY_CHOICE3 */
	{ kvalid_rational, "choice4a" }, /* KEY_CHOICE4A */
	{ kvalid_rational, "choice4b" }, /* KEY_CHOICE4B */
	{ kvalid_rational, "choice5a" }, /* KEY_CHOICE5A */
	{ kvalid_rational, "choice5b" }, /* KEY_CHOICE5B */
	{ kvalid_choice6, "choice6" }, /* KEY_CHOICE6 */
	{ kvalid_choice7, "choice7" }, /* KEY_CHOICE7 */
	{ kvalid_int, "gid" }, /* KEY_GAMEID */
	{ kvalid_mstring, "hitId" }, /* KEY_HITID */
	{ kvalid_stringne, "ident" }, /* KEY_IDENTIFIER */
	{ NULL, "instr" }, /* KEY_INSTR */
	{ kvalid_stringne, "password" }, /* KEY_PASSWORD */
	{ kvalid_uint, "round" }, /* KEY_ROUND */
	{ kvalid_int, "sesscookie" }, /* KEY_SESSCOOKIE */
	{ kvalid_int, "sessid" }, /* KEY_SESSID */
	{ kvalid_mstring, "workerId" }, /* KEY_WORKERID */
};

/*
 * Check if the player session identified in the cookiemap is valid.
 * We must do so with every attempt to hit the site.
 */
static int
sess_valid(struct kreq *r, int64_t *id)
{
	int64_t	sid, scookie;

	if (NULL != r->fieldmap[KEY_SESSID] &&
	    NULL != r->fieldmap[KEY_SESSCOOKIE]) {
		sid = r->fieldmap[KEY_SESSID]->parsed.i;
		scookie = r->fieldmap[KEY_SESSCOOKIE]->parsed.i;
	} else if (NULL != r->cookiemap[KEY_SESSID] &&
		   NULL != r->cookiemap[KEY_SESSCOOKIE]) {
		sid = r->cookiemap[KEY_SESSID]->parsed.i;
		scookie = r->cookiemap[KEY_SESSCOOKIE]->parsed.i;
	} else
		return(0);

	return(db_player_sess_valid(id, sid, scookie));
}

static int
kpairbad(struct kreq *r, enum key key)
{

	return(NULL == r->fieldmap[key] || NULL != r->fieldnmap[key]);
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
 * Log in a Mechanial Turk player.
 * This is only for use via HTML (i.e., the mturk interface).
 * Returns error 200 if the player is logged in.
 * If failure occurs, the player is 303'd to the captive registration page.
 */
static void
sendmturk(struct kreq *r)
{
	struct sess	*sess;
	struct expr	*expr;
	struct player	*player;
	int		 rc, needjoin;
	char		*hash;
	const char	*id, *hitid, *assid;
	char		 addr[1024];

	if (NULL != r->fieldmap[KEY_ASSIGNMENTID] && 
	    0 == strcmp("ASSIGNMENT_ID_NOT_AVAILABLE", 
		        r->fieldmap[KEY_ASSIGNMENTID]->parsed.s)) {
		http_open(r, KHTTP_303);
		send303(r, HTURI "/mturkpreview.html", PAGE__MAX, 0);
		return;
	}

	/* Get the experiment and workerId we're here to enter. */
	if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_303);
		send303(r, HTURI "/playerautoadd.html", PAGE__MAX, 0);
		return;
	} else if (NULL == r->fieldmap[KEY_WORKERID] ||
		   NULL == r->fieldmap[KEY_ASSIGNMENTID] ||
		   NULL == r->fieldmap[KEY_HITID]) {
		http_open(r, KHTTP_303);
		send303(r, HTURI "/playerautoadd.html", PAGE__MAX, 0);
		db_expr_free(expr);
		return;
	} 
	
	id = r->fieldmap[KEY_WORKERID]->parsed.s;
	hitid = r->fieldmap[KEY_HITID]->parsed.s;
	assid = r->fieldmap[KEY_ASSIGNMENTID]->parsed.s;
	
	if ('\0' == *expr->hitid) {
		INFO("Mechanical Turk player trying "
			"to play non-turk experiment");
		http_open(r, KHTTP_303);
		send303(r, HTURI "/playerautoadd.html", PAGE__MAX, 0);
		db_expr_free(expr);
		return;
	} else if (strcmp(expr->hitid, hitid)) {
		INFO("Mechanical Turk player trying "
			"to play with incorrect HIT identifier");
		http_open(r, KHTTP_303);
		send303(r, HTURI "/playerautoadd.html", PAGE__MAX, 0);
		db_expr_free(expr);
		return;
	}

	/* 
	 * If the player does not exist, then ok.
	 * If the player does exist, make sure that they entered with
	 * the same hitId else we route them to the login page.
	 */
	if (0 == (rc = db_player_create(id, &hash, hitid, assid)) &&
	    ! db_player_mturkvrfy(id, &hash, hitid, assid)) {
		http_open(r, KHTTP_303);
		send303(r, HTURI "/playerlogin.html", PAGE__MAX, 0);
		db_expr_free(expr);
		return;
	}

	player = db_player_valid(id, hash);
	assert(NULL != player);
	free(hash);

	sess = db_player_sess_alloc(player->id,
		 NULL != r->reqmap[KREQU_USER_AGENT] ?
		 r->reqmap[KREQU_USER_AGENT]->val : "");
	assert(NULL != sess);

	/* 
	 * If applicable, try to "join" us now.
	 * This just prevents us from bouncing the user around
	 * when they finally try to log in.
	 */
	needjoin = player->joined < 0;
	if (needjoin)
		needjoin = ! db_player_join(player, QUESTIONS);

	/*
	 * Mechanical Turk players have no cookie set at all, as they'll
	 * just use the query string for credentials.
	 */
#if 0
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=%" PRId64 "; path=/; expires=", 
		keys[KEY_SESSCOOKIE].name, sess->cookie);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=%" PRId64 "; path=/; expires=", 
		keys[KEY_SESSID].name, sess->id);
#endif
	snprintf(addr, sizeof(addr),
		HTURI "/%s.html?%s=%" PRId64 "&%s=%" PRId64,
		needjoin ? "playerlobby" : "playerhome",
		keys[KEY_SESSCOOKIE].name, sess->cookie,
		keys[KEY_SESSID].name, sess->id);
	send303(r, addr, PAGE__MAX, 1);

	db_expr_free(expr);
	db_sess_free(sess);
	db_player_free(player);
}

/*
 * This is sent by the front-end to indicate that a player (Mechanical
 * Turk player, we hope) has played through to the end of the
 * experiment and has responded back to the Mechanical Turk site saying
 * that they've finished.
 * We need to submit onward to the Mechanical Turk system.
 * The documents recommend doing this by way of the client, but modern
 * browsers won't allow that kind of behaviour.
 */
static void
sendmturkfinish(struct kreq *r, int64_t playerid)
{
	struct player	*player;
	struct expr	*expr;

	if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	}

	player = db_player_load(playerid);
	assert(NULL != player);
	db_player_mturkdone(playerid);
	http_open(r, KHTTP_200);
	khttp_body(r);
	if (player->mturkdone)
		WARNX("Player %" PRId64 "re-submitting "
		      "MTurk finish query", playerid);
	db_expr_free(expr);
	db_player_free(player);
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
	struct expr	*expr;
	struct player	*player;
	int		 needjoin;
	struct kjsonreq	 req;
	time_t		 t;
	struct tm	*tm;
	char		 buf[64];

	if (NULL == (expr = db_expr_get(1))) {
		if (KMIME_TEXT_HTML == r->mime) {
			http_open(r, KHTTP_303);
			send303(r, HTURI "/playerlogin.html", PAGE__MAX, 0);
		} else {
			http_open(r, KHTTP_409);
			khttp_body(r);
		}
		return;
	}

	/* Now we know that the experiment is running. */

	player = NULL;
	if ( ! kpairbad(r, KEY_IDENTIFIER) && ! kpairbad(r, KEY_PASSWORD)) 
		player = db_player_valid
			(r->fieldmap[KEY_IDENTIFIER]->parsed.s,
			 r->fieldmap[KEY_PASSWORD]->parsed.s);

	sess = NULL;
	if (NULL != player) {
		/* Allocate our session, first. */
		sess = db_player_sess_alloc
			(player->id,
			 NULL != r->reqmap[KREQU_USER_AGENT] ?
			 r->reqmap[KREQU_USER_AGENT]->val : "");
		assert(NULL != sess);

		/* 
		 * If applicable, try to "join" us now.
		 * This just prevents us from bouncing the user around
		 * when they finally try to log in.
		 */
		if ((needjoin = player->joined < 0))
			needjoin = ! db_player_join(player, QUESTIONS);

		if (KMIME_TEXT_HTML == r->mime) 
			http_open(r, KHTTP_303);
		else
			http_open(r, KHTTP_200);

		t = time(NULL) + 60 * 60 * 24 * 365;
		tm = gmtime(&t);
		strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm);

		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRId64 "; path=/; expires=%s", 
			keys[KEY_SESSCOOKIE].name, sess->cookie, buf);
		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRId64 "; path=/; expires=%s", 
			keys[KEY_SESSID].name, sess->id, buf);

		/*
		 * If we're JSON, then indicate whether we should
		 * redirect to the join page or the home page.
		 * We do this in a JSON object so as not to overload the
		 * HTTP codes toooo much.
		 */
		if (KMIME_TEXT_HTML != r->mime) {
			khttp_body(r);
			kjson_open(&req, r);
			kjson_obj_open(&req);
			kjson_putintp(&req, "needjoin", needjoin);
			kjson_obj_close(&req);
			kjson_close(&req);
		} else 
			send303(r, needjoin ?
				HTURI "/playerlobby.html" :
				HTURI "/playerhome.html", PAGE__MAX, 0);
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
	db_player_free(player);
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

	kjson_arrayp_open(req, "stratsd");
	for (i = 0; i < sz; i++)
		kjson_putdouble(req, mpq_get_d(mpq[i]));
	kjson_array_close(req);

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
	json_putroundup(req, "roundup", per->roundups[round - 1], 0);
	kjson_obj_close(req);
}

static int
kvalid_mstring(struct kpair *kp)
{
	char		*cp;
	unsigned char	 c;

	if ( ! kvalid_stringne(kp) || kp->valsz > 64)
		return(0);
	for (cp = kp->val; '\0' != *cp; cp++) {
		c = (unsigned char)*cp;
		if ('_' == c || '-' == c)
			continue;
		if ( ! isalnum(c))
			return(0);
		if (isalpha(c) && islower(c))
			return(0);
	}
	return(1);
}

static int
kvalid_choice7(struct kpair *kp)
{

	if ( ! kvalid_uint(kp))
		return(0);
	return(1 == kp->parsed.i);
}

static int
kvalid_choice6(struct kpair *kp)
{

	if ( ! kvalid_uint(kp))
		return(0);
	return(2 == kp->parsed.i);
}

static int
kvalid_rational(struct kpair *kp)
{
	mpq_t	 mpq;
	int	 rc;

	if (0 == kp->valsz)
		return(1);

	mpq_init(mpq);
	if ((rc = mpq_str2mpqu(kp->val, mpq)))
		mpq_clear(mpq);
	return(rc);
}

static int
kvalid_choice3(struct kpair *kp)
{

	if ( ! kvalid_uint(kp))
		return(0);
	return(2 == kp->parsed.i);
}

static int
kvalid_choice2(struct kpair *kp)
{

	if ( ! kvalid_uint(kp))
		return(0);
	return(4 == kp->parsed.i);
}

static int
kvalid_choice1(struct kpair *kp)
{

	if ( ! kvalid_uint(kp))
		return(0);
	return(1 == kp->parsed.i);
}

static int
kvalid_choice0(struct kpair *kp)
{

	if ( ! kvalid_uint(kp))
		return(0);
	return(3 == kp->parsed.i);
}

static void
senddocheckround(struct kreq *r)
{
	struct expr	*expr;
	struct kjsonreq	 req;

	if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} 

	http_open(r, KHTTP_200);
	khttp_body(r);
	kjson_open(&req, r);
	kjson_obj_open(&req);
	kjson_putintp(&req, "round", expr->round);
	kjson_obj_close(&req);
	kjson_close(&req);
	db_expr_free(expr);
}

static void
senddoanswer(struct kreq *r, int64_t id)
{
	struct player	*player;
	int		 ok;
	int64_t		 rank;
	mpq_t		 one, sum, v1, v2;
	size_t		 c1, c2;

	if (NULL == r->fieldmap[KEY_ANSWERID]) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	}

	rank = r->fieldmap[KEY_ANSWERID]->parsed.i;
	db_player_questionnaire(id, rank);
	
	switch (rank) {
	case (0):
		ok = NULL != r->fieldmap[KEY_CHOICE0];
		break;
	case (1):
		ok = NULL != r->fieldmap[KEY_CHOICE1];
		break;
	case (2):
		ok = NULL != r->fieldmap[KEY_CHOICE2];
		break;
	case (3):
		ok = NULL != r->fieldmap[KEY_CHOICE3];
		break;
	case (4):
		/* FALLTHROUGH */
	case (5):
		c1 = 4 == rank ? KEY_CHOICE4A : KEY_CHOICE5A;
		c2 = 4 == rank ? KEY_CHOICE4B : KEY_CHOICE5B;
		ok = NULL != r->fieldmap[c1] &&
		     NULL != r->fieldmap[c2];
		if ( ! ok)
			break;
		if (5 == rank) {
			ok = r->fieldmap[c1]->valsz &&
			     r->fieldmap[c2]->valsz;
			if ( ! ok)
				break;
		}
		mpq_init(one);
		mpq_init(sum);
		mpq_init(v1);
		mpq_init(v2);
		if (r->fieldmap[c1]->valsz)
			mpq_str2mpqu(r->fieldmap[c1]->val, v1);
		if (r->fieldmap[c2]->valsz)
			mpq_str2mpqu(r->fieldmap[c2]->val, v2);
		mpq_summation(sum, v1);
		mpq_summation(sum, v2);
		mpq_set_ui(one, 1, 1);
		mpq_canonicalize(one);
		ok = mpq_equal(one, sum);
		mpq_clear(sum);
		mpq_clear(one);
		mpq_clear(v1);
		mpq_clear(v2);
		break;
	case (6):
		ok = NULL != r->fieldmap[KEY_CHOICE6];
		break;
	case (7):
		ok = NULL != r->fieldmap[KEY_CHOICE7];
		break;
	default:
		ok = 0;
		break;
	}

	if (ok) {
		db_player_set_answered(id, 
			r->fieldmap[KEY_ANSWERID]->parsed.i);
		player = db_player_load(id);
		db_player_join(player, QUESTIONS);
		db_player_free(player);
		http_open(r, KHTTP_200);
	} else
		http_open(r, KHTTP_400);

	khttp_body(r);
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
	int64_t		 autoadd;
	char		*hash;
	struct kjsonreq	 req;
	struct expr	*expr;

	expr = db_expr_get(0);
	assert(NULL != expr);
	autoadd = expr->autoadd;
	db_expr_free(expr);

	if (0 == autoadd) {
		http_open(r, KHTTP_404);
		khttp_body(r);
		return;
	} else if (NULL == r->fieldmap[KEY_IDENTIFIER]) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} 

	rc = db_player_create
		(r->fieldmap[KEY_IDENTIFIER]->parsed.s, 
		 &hash, NULL, NULL);

	if (0 == rc) {
		http_open(r, KHTTP_403);
		khttp_body(r);
	} else {
		INFO("Player created from captive portal: %s", 
			r->fieldmap[KEY_IDENTIFIER]->parsed.s);
		http_open(r, KHTTP_200);
		khttp_body(r);
		kjson_open(&req, r);
		kjson_obj_open(&req);
		kjson_putstringp(&req, keys[KEY_IDENTIFIER].name,
			r->fieldmap[KEY_IDENTIFIER]->parsed.s);
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
senddoloadquestions(struct kreq *r, int64_t playerid)
{
	struct expr	*expr;
	struct kjsonreq	 req;
	struct player	*player;

	if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	} 
	
	player = db_player_load(playerid);
	assert(NULL != player);

	http_open(r, KHTTP_200);
	khttp_body(r);
	kjson_open(&req, r);
	kjson_obj_open(&req);
	kjson_putintp(&req, "questionnaire", expr->questionnaire);
	kjson_putintp(&req, "answered", player->answer);
	kjson_obj_close(&req);
	kjson_close(&req);
	db_expr_free(expr);
	db_player_free(player);
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
	int64_t	 	 i, tics;
	size_t		 gamesz;
	struct kjsonreq	 req;
	char		 buf[22];
	const char	*cp;

again:
	/* All response have at least the following. */
	if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	}

	player = db_player_load(playerid);
	assert(NULL != player);

	/*
	 * If we're currently in the player lobby, then see if we can be
	 * added in the next round.
	 */
	if (player->joined < 0) {
		db_expr_free(expr);
		if (db_player_join(player, QUESTIONS)) {
			db_player_free(player);
			goto again;
		}
		db_player_free(player);
		http_open(r, KHTTP_429);
		khttp_body(r);
		return;
	}

	/*
	 * Cache optimisation.
	 * If we're at the same round as the last time we asked for
	 * data, then simply 304 the request and let the browser use the
	 * cached version.
	 * This saves (untested) on our time digging around the db.
	 */
	if (expr->round >= 0 && NULL != r->reqmap[KREQU_IF_NONE_MATCH]) {
		snprintf(buf, sizeof(buf), 
			"\"%" PRIu64 "-%" PRId64 "-%" PRId64 "-%zu\"", 
			expr->round, player->id, player->version,
			db_player_count_plays(expr->round, playerid));
		cp = r->reqmap[KREQU_IF_NONE_MATCH]->val;
		if (0 == strcmp(buf, cp)) {
			khttp_head(r, kresps[KRESP_STATUS], 
				"%s", khttps[KHTTP_304]);
			khttp_body(r);
			db_expr_free(expr);
			db_player_free(player);
			return;
		}
	}

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r->mime]);
	khttp_head(r, kresps[KRESP_CACHE_CONTROL], 
		"%s", "no-cache, no-store");
	khttp_head(r, kresps[KRESP_PRAGMA], 
		"%s", "no-cache");
	khttp_head(r, kresps[KRESP_EXPIRES], 
		"%s", "-1");

	/*
	 * If the game has completed, then disallow the cache: the
	 * amdinistrator can grant winnings whenever.
	 */
	if (expr->round < expr->rounds) 
		khttp_head(r, kresps[KRESP_ETAG], 
			"\"%" PRIu64 "-%" PRId64 "-%" PRId64 "-%zu\"", 
			expr->round, player->id, player->version,
			db_player_count_plays(expr->round, playerid));

	khttp_body(r);
	kjson_open(&req, r);
	kjson_obj_open(&req);

	memset(&stor, 0, sizeof(struct intvstor));
	memset(&pstor, 0, sizeof(struct poffstor));

	/*
	 * If the experiment hasn't started yet, have it contain nothing
	 * other than the experiment data itself.
	 */
	if (expr->round < 0) {
		json_putexpr(&req, expr, 0);
		kjson_putnullp(&req, "win");
		kjson_putnullp(&req, "history");
		json_putplayer(&req, player);
		kjson_putnullp(&req, "curlottery");
		kjson_putnullp(&req, "aggrlottery");
		kjson_putnullp(&req, "aggrtickets");
		goto out;
	}

	/*
	 * This invokes the underlying "roundup" machinery.
	 * When it completes, we'll have computed the payoffs for all
	 * prior games.
	 * It returns NULL if and only if we have no history.
	 */
	stor.req = &req;
	if (NULL != (stor.intv = db_interval_get(expr->round - 1)))
		gamesz = stor.intv->periodsz;
	else
		gamesz = db_game_count_all();

	/*
	 * If we're not expressing our history, explicitly disable it
	 * here, which continues getting (and thus computing) our
	 * roundups and such, but not displaying them.
	 */
	if (EXPR_NOHISTORY & expr->flags) {
		db_interval_free(stor.intv);
		stor.intv = NULL;
	}

	/* 
	 * If the experiment is over but we don't have a total yet,
	 * refresh the experiment object to reflect the total number of
	 * lottery tickets.
	 */
	if (expr->round >= expr->rounds && 
	    expr->state < ESTATE_PREWIN) {
		db_expr_finish(&expr, gamesz);
		/* Reload with new final ranking... */
		db_player_free(player);
		player = db_player_load(playerid);
	}

	json_putexpr(&req, expr, 0);

	if (ESTATE_POSTWIN == expr->state) {
		kjson_objp_open(&req, "win");
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
		kjson_obj_close(&req);
	} else 
		kjson_putnullp(&req, "win");

	json_putplayer(&req, player);

	/*
	 * This invokes the underlying lottery computation.
	 * This will compute the lottery for (right now) only the last
	 * lottery sequence.
	 */
	if (db_player_lottery(expr->round - 1, 
			playerid, cur, aggr, &tics, gamesz)) {
		kjson_putdoublep(&req, "curlottery", mpq_get_d(cur));
		kjson_putdoublep(&req, "aggrlottery", mpq_get_d(aggr));
		kjson_putintp(&req, "aggrtickets", tics);
		mpq_clear(cur);
		mpq_clear(aggr);
	} else {
		kjson_putnullp(&req, "curlottery");
		kjson_putnullp(&req, "aggrlottery");
		kjson_putnullp(&req, "aggrtickets");
	}

	/* 
	 * Send the remaining games.
	 * This will not reference the games that we've already played.
	 * We use the games computed during roundup.
	 */
	kjson_putintp(&req, "gamesz", gamesz);
	kjson_arrayp_open(&req, "games");
	db_game_load_player(playerid, 
		expr->round, senddoloadgame, &stor);
	kjson_array_close(&req);

	json_puthistory(&req, 0, expr, stor.intv);

	pstor.playerid = playerid;
	pstor.req = &req;
	kjson_arrayp_open(&req, "lotteries");
	for (i = 0; i < expr->round; i++) {
		db_player_lottery(i, playerid, 
			cur, aggr, &tics, gamesz);
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
	assert(NULL != r->cookiemap[KEY_SESSID] ||
	       NULL != r->fieldmap[KEY_SESSID]);

	if (0 ==  db_player_play
		(player, 
		 NULL != r->cookiemap[KEY_SESSID] ?
		 r->cookiemap[KEY_SESSID]->parsed.i :
		 r->fieldmap[KEY_SESSID]->parsed.i,
		 r->fieldmap[KEY_ROUND]->parsed.i,
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

	if (NULL != r->cookiemap[KEY_SESSID])
		db_sess_delete(r->cookiemap[KEY_SESSID]->parsed.i);
	else if (NULL != r->fieldmap[KEY_SESSID])
		db_sess_delete(r->fieldmap[KEY_SESSID]->parsed.i);

	http_open(r, KHTTP_303);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSCOOKIE].name);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSID].name);
	send303(r, HTURI "/playerlogin.html", PAGE__MAX, 0);
}

static void
doreq(struct kreq *r)
{
	int64_t		 id;
	unsigned int	 bit;
	
	switch (r->method) {
	case (KMETHOD_GET):
	case (KMETHOD_POST):
		break;
	case (KMETHOD_OPTIONS):
		khttp_head(r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_head(r, kresps[KRESP_ALLOW],
			"GET POST OPTIONS");
		khttp_body(r);
		return;
	default:
		http_open(r, KHTTP_405);
		khttp_body(r);
		return;
	}

	switch (r->mime) {
	case (KMIME_TEXT_HTML):
		bit = PERM_HTML;
		break;
	case (KMIME_APP_JSON):
		bit = PERM_JSON;
		break;
	default:
		send404(r);
		return;
	}

	if ( ! (perms[r->page] & bit)) {
		send404(r);
		return;
	}

	id = -1;
	if ((perms[r->page] & PERM_LOGIN) && ! sess_valid(r, &id)) {
		if (KMIME_TEXT_HTML != r->mime) {
			http_open(r, KHTTP_403);
			khttp_body(r);
		} else
			send303(r, HTURI "/playerlogin.html", 
				PAGE__MAX, 1);
		return;
	}

	db_expr_advance();

	switch (r->page) {
	case (PAGE_DOAUTOADD):
		senddoautoadd(r);
		break;
	case (PAGE_DOANSWER):
		senddoanswer(r, id);
		break;
	case (PAGE_DOCHECKROUND):
		senddocheckround(r);
		break;
	case (PAGE_DOINSTR):
		senddoinstr(r, id);
		break;
	case (PAGE_DOLOADEXPR):
		senddoloadexpr(r, id);
		break;
	case (PAGE_DOLOADQUESTIONS):
		senddoloadquestions(r, id);
		break;
	case (PAGE_DOLOGIN):
		senddologin(r);
		break;
	case (PAGE_DOLOGOUT):
		senddologout(r);
		break;
	case (PAGE_DOPLAY):
		senddoplay(r, id);
		break;
	case (PAGE_MTURK):
		sendmturk(r);
		break;
	case (PAGE_MTURKFINISH):
		sendmturkfinish(r, id);
		break;
	case (PAGE_INDEX):
		send303(r, HTURI "/playerhome.html", PAGE__MAX, 1);
		break;
	default:
		http_open(r, KHTTP_404);
		khttp_body(r);
		break;
	}
}

int
main(void)
{
	struct kreq	 r;
	enum kcgi_err	 er;

	er = khttp_parse(&r, keys, KEY__MAX, 
		pages, PAGE__MAX, PAGE_INDEX);
	if (KCGI_OK == er) {
		doreq(&r);
		khttp_free(&r);
	} else 
		WARNX("khttp_parse: error %d", er);

	return(EXIT_SUCCESS);
}
