/*	$Id$ */
/*
 * Copyright (c) 2014--2016 Kristaps Dzonsons <kristaps@kcons.eu>
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
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>
#include <gmp.h>
#include <sqlite3.h>
#include <kcgi.h>
#include <kcgijson.h>

#include "extern.h"

enum	instrs {
	INSTR_LOTTERY,
	INSTR_NOLOTTERY,
	INSTR_MTURK,
	INSTR_CUSTOM,
	INSTR__MAX
};

enum	page {
	PAGE_DOADDGAME,
	PAGE_DOADDPLAYERS,
	PAGE_DOADVANCE,
	PAGE_DOADVANCEEND,
	PAGE_DOBACKUP,
	PAGE_DOCHANGEMAIL,
	PAGE_DOCHANGEPASS,
	PAGE_DOCHANGESMTP,
	PAGE_DOCHECKROUND,
	PAGE_DOCHECKSMTP,
	PAGE_DOCLEARMTURK,
	PAGE_DODELETEGAME,
	PAGE_DODELETEPLAYER,
	PAGE_DODISABLEPLAYER,
	PAGE_DOENABLEPLAYER,
	PAGE_DOGETEXPR,
	PAGE_DOGETHIGHEST,
	PAGE_DOGETHISTORY,
	PAGE_DOLOADGAMES,
	PAGE_DOLOADPLAYERS,
	PAGE_DOLOGIN,
	PAGE_DOLOGOUT,
	PAGE_DOMTURKBONUSES,
	PAGE_DORESENDEMAIL,
	PAGE_DORESETPASSWORDS,
	PAGE_DOSETINSTR,
	PAGE_DOSETMTURK,
	PAGE_DOSTARTEXPR,
	PAGE_DOTESTSMTP,
	PAGE_DOWINNERS,
	PAGE_DOWIPE,
	PAGE_DOWIPEQUIET,
	PAGE_INDEX,
	PAGE__MAX
};

enum	key {
	KEY_AUTOADD,
	KEY_AUTOADDPRESERVE,
	KEY_AWSACCESSKEY,
	KEY_AWSDESC,
	KEY_AWSKEYWORDS,
	KEY_AWSNAME,
	KEY_AWSREWARD,
	KEY_AWSSANDBOX,
	KEY_AWSSECRETKEY,
	KEY_AWSWORKER_LOCALE,
	KEY_AWSWORKER_HITAPPRV,
	KEY_AWSWORKER_PCTAPPRV,
	KEY_AWSWORKERS,
	KEY_AWSCONVERSION,
	KEY_CUSTOMANSWER,
	KEY_CUSTOMCOUNT,
	KEY_CUSTOMQUESTION,
	KEY_DATE,
	KEY_TIME,
	KEY_ROUNDMIN,
	KEY_ROUNDPCT,
	KEY_ROUNDS,
	KEY_PROUNDS,
	KEY_MINUTES,
	KEY_EMAIL,
	KEY_EMAIL1,
	KEY_EMAIL2,
	KEY_EMAIL3,
	KEY_GAMEID,
	KEY_HISTORYFILE,
	KEY_HISTORYTRIM,
	KEY_INSTR,
	KEY_INSTRFILE,
	KEY_LOTTERY,
	KEY_MAILROUND,
	KEY_NAME,
	KEY_P1,
	KEY_P2,
	KEY_PASSWORD,
	KEY_PASSWORD1,
	KEY_PASSWORD2,
	KEY_PASSWORD3,
	KEY_PAYOFFS,
	KEY_PLAYERID,
	KEY_PLAYERMAX,
	KEY_PLAYERS,
	KEY_QUESTIONNAIRE,
	KEY_RELATIVE,
	KEY_SERVER,
	KEY_SESSCOOKIE,
	KEY_SESSID,
	KEY_SHOWHISTORY,
	KEY_SHUFFLE,
	KEY_URI,
	KEY_USER,
	KEY_WINNERS,
	KEY_WINSEED,
	KEY__MAX
};

/*
 * Used for managing the high scores, which require both the score
 * itself as well as the conversion rate for a currency calculation.
 */
struct	hghstor {
	struct kjsonreq	*req; /* may be NULL */
	struct kreq	*r;
	struct expr	*expr;
};

#define	PERM_LOGIN	0x01
#define	PERM_HTML	0x08
#define	PERM_JSON	0x10
#define PERM_CSV	0x02

static	unsigned int perms[PAGE__MAX] = {
	PERM_JSON | PERM_LOGIN, /* PAGE_DOADDGAME */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOADDPLAYERS */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOADVANCE */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOADVANCEEND */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOBACKUP */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOCHANGEMAIL */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOCHANGEPASS */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOCHANGESMTP */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOCHECKROUND */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOCHECKSMTP */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOCLEARMTURK */
	PERM_JSON | PERM_LOGIN, /* PAGE_DODELETEGAME */
	PERM_JSON | PERM_LOGIN, /* PAGE_DODELETEPLAYER */
	PERM_JSON | PERM_LOGIN, /* PAGE_DODISABLEPLAYER */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOENABLEPLAYER */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOGETEXPR */
	PERM_CSV | PERM_LOGIN, /* PAGE_DOGETHIGHEST */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOGETHISTORY */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOLOADGAMES */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOLOADPLAYERS */
	PERM_JSON, /* PAGE_DOLOGIN */
	PERM_HTML | PERM_LOGIN, /* PAGE_DOLOGOUT */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOMTURKBONUSES */
	PERM_JSON | PERM_LOGIN, /* PAGE_DORESENDEMAIL */
	PERM_JSON | PERM_LOGIN, /* PAGE_DORESETPASSWORDS */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOSETINSTR */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOSETMTURK */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOSTARTEXPR */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOTESTSMTP */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOWINNERS */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOWIPE */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOWIPEQUIET */
	PERM_HTML | PERM_LOGIN, /* PAGE_INDEX */
};

static const char *const pages[PAGE__MAX] = {
	"doaddgame", /* PAGE_DOADDGAME */
	"doaddplayers", /* PAGE_DOADDPLAYERS */
	"doadvance", /* PAGE_DOADVANCE */
	"doadvanceend", /* PAGE_DOADVANCEEND */
	"dobackup", /* PAGE_DOBACKUP */
	"dochangemail", /* PAGE_DOCHANGEMAIL */
	"dochangepass", /* PAGE_DOCHANGEPASS */
	"dochangesmtp", /* PAGE_DOCHANGESMTP */
	"docheckround", /* PAGE_DOCHECKROUND */
	"dochecksmtp", /* PAGE_DOCHECKSMTP */
	"doclearmturk", /* PAGE_DOCLEARMTURK */
	"dodeletegame", /* PAGE_DODELETEGAME */
	"dodeleteplayer", /* PAGE_DODELETEPLAYER */
	"dodisableplayer", /* PAGE_DODISABLEPLAYER */
	"doenableplayer", /* PAGE_DOENABLEPLAYER */
	"dogetexpr", /* PAGE_DOGETEXPR */
	"dogethighest", /* PAGE_DOGETHIGHEST */
	"dogethistory", /* PAGE_DOGETHISTORY */
	"doloadgames", /* PAGE_DOLOADGAMES */
	"doloadplayers", /* PAGE_DOLOADPLAYERS */
	"dologin", /* PAGE_DOLOGIN */
	"dologout", /* PAGE_DOLOGOUT */
	"domturkbonuses", /* PAGE_DOMTURKBONUSES */
	"doresendemail", /* PAGE_DORESENDEMAIL */
	"doresetpasswords", /* PAGE_DORESETPASSWORDS */
	"dosetinstr", /* PAGE_DOSETINSTR */
	"dosetmturk", /* PAGE_DOSETMTURK */
	"dostartexpr", /* PAGE_DOSTARTEXPR */
	"dotestsmtp", /* PAGE_DOTESTSMTP */
	"dowinners", /* PAGE_DOWINNERS */
	"dowipe", /* PAGE_DOWIPE */
	"dowipequiet", /* PAGE_DOWIPEQUIET */
	"index", /* PAGE_INDEX */
};

#if 0
static int kvalid_history(struct kpair *);
#endif
static int kvalid_minutes(struct kpair *);
static int kvalid_rounds(struct kpair *);
static int kvalid_time(struct kpair *);

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_int, "autoadd" }, /* KEY_AUTOADD */
	{ kvalid_int, "autoaddpreserve" }, /* KEY_AUTOADDPRESERVE*/
	{ kvalid_stringne, "awsaccesskey" }, /* KEY_AWSACCESSKEY */
	{ kvalid_string, "awsdesc" }, /* KEY_AWSDESC */
	{ kvalid_string, "awskeywords" }, /* KEY_AWSKEYWORDS */
	{ kvalid_stringne, "awsname" }, /* KEY_AWSNAME */
	{ kvalid_double, "awsreward" }, /* KEY_AWSREWARD */
	{ NULL, "sandbox" }, /* KEY_AWSSANDBOX */
	{ kvalid_stringne, "awssecretkey" }, /* KEY_AWSSECRETKEY */
	{ kvalid_string, "workerlocale" }, /* KEY_AWSWORKER_LOCALE */
	{ kvalid_int, "workerhitapprv" }, /* KEY_AWSWORKER_HITAPPRV */
	{ kvalid_int, "workerpctapprv" }, /* KEY_AWSWORKER_PCTAPPRV */
	{ kvalid_uint, "workers" }, /* KEY_AWSWORKERS */
	{ kvalid_udouble, "conversion" }, /* KEY_AWSCONVERSION */
	{ kvalid_string, "customanswer" }, /* KEY_CUSTOMANSWER */
	{ kvalid_uint, "customcount" }, /* KEY_CUSTOMCOUNT */
	{ kvalid_string, "customquestion" }, /* KEY_CUSTOMQUESTION */
	{ kvalid_date, "date" }, /* KEY_DATE */
	{ kvalid_time, "time" }, /* KEY_TIME */
	{ kvalid_uint, "roundmin" }, /* KEY_ROUNDMIN */
	{ kvalid_uint, "roundpct" }, /* KEY_ROUNDPCT */
	{ kvalid_rounds, "rounds" }, /* KEY_ROUNDS */
	{ kvalid_uint, "prounds" }, /* KEY_PROUNDS */
	{ kvalid_minutes, "minutes" }, /* KEY_MINUTES */
	{ kvalid_email, "email" }, /* KEY_EMAIL */
	{ kvalid_email, "email1" }, /* KEY_EMAIL1 */
	{ kvalid_email, "email2" }, /* KEY_EMAIL2 */
	{ kvalid_email, "email3" }, /* KEY_EMAIL3 */
	{ kvalid_int, "gid" }, /* KEY_GAMEID */
	{ kvalid_string, "historyfile" }, /* KEY_HISTORYFILE */
	{ NULL, "trimhist" }, /* KEY_HISTORYTRIM */
	{ kvalid_stringne, "instr" }, /* KEY_INSTR */
	{ kvalid_stringne, "instrFile" }, /* KEY_INSTRFILE */
	{ kvalid_string, "lottery" }, /* KEY_LOTTERY */
	{ kvalid_int, "mailround" }, /* KEY_MAILROUND */
	{ kvalid_stringne, "name" }, /* KEY_NAME */
	{ kvalid_int, "p1" }, /* KEY_P1 */
	{ kvalid_int, "p2" }, /* KEY_P2 */
	{ kvalid_stringne, "password" }, /* KEY_PASSWORD */
	{ kvalid_stringne, "password1" }, /* KEY_PASSWORD1 */
	{ kvalid_stringne, "password2" }, /* KEY_PASSWORD2 */
	{ kvalid_stringne, "password3" }, /* KEY_PASSWORD3 */
	{ kvalid_stringne, "payoffs" }, /* KEY_PAYOFFS */
	{ kvalid_int, "pid" }, /* KEY_PLAYERID */
	{ kvalid_uint, "playermax" }, /* KEY_PLAYERMAX */
	{ kvalid_stringne, "players" }, /* KEY_PLAYERS */
	{ kvalid_int, "question" }, /* KEY_QUESTIONNAIRE */
	{ kvalid_int, "relative" }, /* KEY_RELATIVE */
	{ kvalid_stringne, "server" }, /* KEY_SERVER */
	{ kvalid_int, "sesscookie" }, /* KEY_SESSCOOKIE */
	{ kvalid_int, "sessid" }, /* KEY_SESSID */
	{ kvalid_int, "showhistory" }, /* KEY_SHOWHISTORY */
	{ kvalid_int, "shuffle" }, /* KEY_SHUFFLE */
	{ kvalid_stringne, "uri" }, /* KEY_URI */
	{ kvalid_stringne, "user" }, /* KEY_USER */
	{ kvalid_uint, "winners" }, /* KEY_WINNERS */
	{ kvalid_int, "winseed" }, /* KEY_WINSEED */
};

/*
 * This function simply wakes up every thirty seconds and checks the
 * current round.
 * If the round has advanced, it tries to mail players.
 * (See mail_roundadvance() for details.)
 * It exits if the round-daemon identifier has changed, the game has
 * ended, or any other number of conditions.
 */
static void
mailround(const char *uri)
{
	struct expr	*expr;
	pid_t		 pid;
	int64_t		 round;

	round = -1;
	pid = getpid();
	db_expr_setmailer(0, pid);
	expr = NULL;

	INFO("Round mailer starting: %u", pid);

	for (;;) {
		db_expr_free(expr);
		sleep(30);
		expr = db_expr_get(0);

		if (pid != expr->roundpid) {
			WARNX("Round mailer error: "
				"invoked with different pid: "
				"my %u != %" PRId64 " (exiting)", 
				pid, expr->roundpid);
			break;
		} else if (expr->state > ESTATE_STARTED ||
		           expr->round >= expr->rounds) {
			INFO("Round mailer exiting: "
				"experiment over: %u", pid);
			break;
		} else if (expr->round == round)
			continue;

		INFO("Round mailer is firing for round %" 
			PRId64 " (last saw %" PRId64 "): %u", 
			expr->round, round, pid);
		mail_roundadvance(uri, round, expr->round);
		round = expr->round;
		INFO("Round mailer has fired: %u", pid);
	}

	db_expr_free(expr);
	db_expr_setmailer(pid, 0);
	INFO("Round mailer exiting: %u", pid);
}

static int
sess_valid(struct kreq *r)
{

	if (NULL == r->cookiemap[KEY_SESSID] ||
	    NULL == r->cookiemap[KEY_SESSCOOKIE])
		return(0);
	return(db_admin_sess_valid
		(r->cookiemap[KEY_SESSID]->parsed.i,
		 r->cookiemap[KEY_SESSCOOKIE]->parsed.i));
}

static int
kpairbad(struct kreq *r, enum key key)
{

	return(NULL == r->fieldmap[key] || 
	       NULL != r->fieldnmap[key]);
}

static int
admin_valid(struct kreq *r)
{


	if (kpairbad(r, KEY_EMAIL) || 
	    kpairbad(r, KEY_PASSWORD))
		return(0);
	return(db_admin_valid
		(r->fieldmap[KEY_EMAIL]->parsed.s,
		 r->fieldmap[KEY_PASSWORD]->parsed.s));
}

static void
http_open(struct kreq *r, enum khttp http)
{

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[http]);
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r->mime]);
}

static void
send403(struct kreq *r)
{

	http_open(r, KHTTP_403);
	khttp_body(r);
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

static void
sendhighestcsv(const struct player *p, 
	double poff, int64_t score, void *arg)
{
	struct hghstor	*stor = arg;
	char		 buf[64];

	snprintf(buf, sizeof(buf), "%" PRId64, score);
	khttp_puts(stor->r, buf);
	khttp_putc(stor->r, ',');
	snprintf(buf, sizeof(buf), "%g", poff);
	khttp_puts(stor->r, buf);
	khttp_putc(stor->r, ',');
	khttp_puts(stor->r, p->mail);
	khttp_putc(stor->r, ',');
	snprintf(buf, sizeof(buf), "%g", 
		score * stor->expr->awsconvert);
	khttp_puts(stor->r, buf);
	khttp_putc(stor->r, '\n');
}

static void
sendhighest(const struct player *p, 
	double poff, int64_t score, void *arg)
{
	struct hghstor	*stor = arg;

	kjson_obj_open(stor->req);
	kjson_putintp(stor->req, "score", score);
	kjson_putdoublep(stor->req, "payoff", poff);
	kjson_putdoublep(stor->req, "payout", 
		score * stor->expr->awsconvert);
	json_putplayer(stor->req, p);
	kjson_obj_close(stor->req);
}

static void
sendwinners(const struct player *p, 
	const struct winner *winner, void *arg)
{
	struct kjsonreq	*req = arg;

	kjson_obj_open(req);
	kjson_putstringp(req, "email", p->mail);
	kjson_putintp(req, "rank", winner->rank);
	kjson_putintp(req, "winrank", p->finalrank);
	kjson_putintp(req, "winscore", p->finalscore);
	kjson_putintp(req, "winnum", winner->rnum);
	kjson_obj_close(req);
}

static void
senddogethistory(struct kreq *r)
{
	struct expr	*expr;
	struct kjsonreq	 req;

	if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	}

	http_open(r, KHTTP_200);
	khttp_body(r);
	kjson_open(&req, r);
	kjson_obj_open(&req);
	json_puthistory(&req, 0, expr, NULL);
	kjson_obj_close(&req);
	kjson_close(&req);
	db_expr_free(expr);
}

static void
senddomturkbonuses(struct kreq *r)
{
	struct expr	 *expr;
	size_t		  i, psz;
	struct player	**ps;
	int64_t		 *scores;

	http_open(r, KHTTP_200);
	khttp_body(r);

	expr = db_expr_get(0);
	if (expr->round < expr->rounds ||
		'\0' == *expr->awssecretkey ||
		'\0' == *expr->awsaccesskey) {
		WARNX("Ignoring MTurk bonus request: experiment "
			"not finished or not MTurk-enabled");
		db_expr_free(expr);
		return;
	}

	ps = db_player_load_bonuses(&scores, &psz);

	/*
	 * We want to send our bonuses behind the scenes: with many
	 * players, it may take some time as each one is separate.
	 * Close out our current connection.
	 */
	if (0 == doublefork(r)) {
		for (i = 0; i < psz; i++) {
			mturk_bonus(expr, ps[i], scores[i]);
			db_player_free(ps[i]);
		}
		free(ps);
		free(scores);
		db_expr_free(expr);
		exit(EXIT_SUCCESS);
	}

	db_expr_free(expr);
	for (i = 0; i < psz; i++) 
		db_player_free(ps[i]);
	free(ps);
	free(scores);
}

static void
senddogethighest(struct kreq *r)
{
	struct hghstor	 stor;

	memset(&stor, 0, sizeof(struct hghstor));
	http_open(r, KHTTP_200);
	khttp_body(r);
	stor.r = r;
	stor.expr = db_expr_get(0);
	db_player_load_highest(sendhighestcsv, &stor, 0);
	db_expr_free(stor.expr);
}

static void
senddogetexpr(struct kreq *r)
{
	struct expr	*expr;
	struct kjsonreq	 req;
	struct hghstor	 hgh;
	size_t		 gamesz, round;

	http_open(r, KHTTP_200);
	khttp_body(r);

	expr = db_expr_get(0);
	assert(NULL != expr);

	if (ESTATE_NEW == expr->state) {
		/*
		 * If the experiment is brand new, we don't need to get
		 * anything for it.
		 * FIXME: technically, all we should be sending out is
		 * the experiment state (expr.state): the rest is bleed.
		 */
		kjson_open(&req, r);
		kjson_obj_open(&req);
		json_putexpr(r, &req, expr, 1);
		kjson_putintp(&req, "adminflags", 
			db_admin_get_flags());
		kjson_obj_close(&req);
		kjson_close(&req);
		db_expr_free(expr);
		return;
	}

	/*
	 * The experiment has been started and is thus associated with
	 * all of its data.
	 * The remainder switches on whether the experiment has started
	 * the play sequence (expr.round >= 0) or has finished.
	 */
	memset(&hgh, 0, sizeof(struct hghstor));

	kjson_open(&req, r);
	kjson_obj_open(&req);

	json_putexpr(r, &req, expr, 1);
	kjson_putintp(&req, "adminflags", db_admin_get_flags());

	if (expr->round >= 0)
		json_puthistory(&req, 1, expr, NULL);

	if (expr->round >= 0 && expr->round < expr->rounds) {
		gamesz = db_game_count_all();
		round = expr->round;
		kjson_putintp(&req, "frow",
			db_game_round_count_done(round, 0, gamesz));
		kjson_putintp(&req, "frowmax",
			db_expr_round_count(expr, round, 0));
		kjson_putintp(&req, "fcol",
			db_game_round_count_done(round, 1, gamesz));
		kjson_putintp(&req, "fcolmax",
			db_expr_round_count(expr, round, 1));
	}

	kjson_putintp(&req, "lobbysize", db_expr_lobbysize());

	kjson_arrayp_open(&req, "highest");
	if (expr->round > 0) {
		hgh.req = &req;
		hgh.r = r;
		hgh.expr = expr;
		db_player_load_highest(sendhighest, &hgh, 5);
	}
	kjson_array_close(&req);

	if (ESTATE_POSTWIN == expr->state) {
		kjson_arrayp_open(&req, "winners");
		db_winners_load_all(&req, sendwinners);
		kjson_array_close(&req);
	} else
		kjson_putnullp(&req, "winners");

	kjson_obj_close(&req);
	kjson_close(&req);
	db_expr_free(expr);
}

static void
senddoenableplayer(struct kreq *r)
{

	if ( ! kpairbad(r, KEY_PLAYERID)) {
		db_player_enable(r->fieldmap[KEY_PLAYERID]->parsed.i);
		http_open(r, KHTTP_200);
	} else
		http_open(r, KHTTP_400);

	khttp_body(r);
}

static void
senddodeletegame(struct kreq *r)
{

	if ( ! kpairbad(r, KEY_GAMEID)) {
		db_game_delete(r->fieldmap[KEY_GAMEID]->parsed.i);
		http_open(r, KHTTP_200);
		http_open(r, KHTTP_200);
	} else
		http_open(r, KHTTP_400);

	khttp_body(r);
}

static void
senddodeleteplayer(struct kreq *r)
{

	if ( ! kpairbad(r, KEY_PLAYERID)) {
		db_player_delete(r->fieldmap[KEY_PLAYERID]->parsed.i);
		http_open(r, KHTTP_200);
		http_open(r, KHTTP_200);
	} else
		http_open(r, KHTTP_400);

	khttp_body(r);
}

static void
senddodisableplayer(struct kreq *r)
{

	if ( ! kpairbad(r, KEY_PLAYERID)) {
		db_player_disable(r->fieldmap[KEY_PLAYERID]->parsed.i);
		http_open(r, KHTTP_200);
	} else
		http_open(r, KHTTP_400);

	khttp_body(r);
}

static void
senddochangemail(struct kreq *r)
{
	time_t	 	 t;
	struct tm	*tm;
	char		 buf[64];

	if (kpairbad(r, KEY_EMAIL1) ||
	    kpairbad(r, KEY_EMAIL2) ||
	    kpairbad(r, KEY_EMAIL3) ||
	    strcmp(r->fieldmap[KEY_EMAIL2]->parsed.s,
		   r->fieldmap[KEY_EMAIL3]->parsed.s) ||
	    ! db_admin_valid_email
	     (r->fieldmap[KEY_EMAIL1]->parsed.s)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	}

	assert(NULL != r->cookiemap[KEY_SESSID]);
	db_sess_delete(r->cookiemap[KEY_SESSID]->parsed.i);

	t = time(NULL) + 60 * 60 * 24 * 365;
	tm = gmtime(&t);
	strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm);

	db_admin_set_mail(r->fieldmap[KEY_EMAIL2]->parsed.s);
	http_open(r, KHTTP_200);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=%s", 
		keys[KEY_SESSCOOKIE].name, buf);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=%s", 
		keys[KEY_SESSID].name, buf);
	khttp_body(r);
}

static void
senddotestsmtp(struct kreq *r)
{

	http_open(r, KHTTP_200);
	khttp_body(r);
	if (0 == doublefork(r)) {
		mail_test();
		exit(EXIT_SUCCESS);
	}
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
senddoclearmturk(struct kreq *r)
{

	http_open(r, KHTTP_200);
	khttp_body(r);
	db_expr_clearmturk();
}

static void
senddochecksmtp(struct kreq *r)
{
	struct smtp	*smtp;
	struct kjsonreq	 req;

	if (NULL == (smtp = db_smtp_get())) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} 

	http_open(r, KHTTP_200);
	khttp_body(r);
	kjson_open(&req, r);
	kjson_obj_open(&req);
	kjson_putstringp(&req, "server", smtp->server);
	kjson_putstringp(&req, "mail", smtp->from);
	kjson_putstringp(&req, "user", smtp->user);
	kjson_obj_close(&req);
	kjson_close(&req);
	db_smtp_free(smtp);
}

static void
senddochangesmtp(struct kreq *r)
{

	if (kpairbad(r, KEY_PASSWORD) ||
		kpairbad(r, KEY_USER) ||
		kpairbad(r, KEY_SERVER) ||
		kpairbad(r, KEY_EMAIL)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} 

	db_smtp_set
		(r->fieldmap[KEY_USER]->parsed.s,
		 r->fieldmap[KEY_SERVER]->parsed.s,
		 r->fieldmap[KEY_EMAIL]->parsed.s,
		 r->fieldmap[KEY_PASSWORD]->parsed.s);
	http_open(r, KHTTP_200);
	khttp_body(r);
}

static void
senddochangepass(struct kreq *r)
{
	time_t		 t;
	struct tm	*tm;
	char		 buf[64];

	if (kpairbad(r, KEY_PASSWORD1) ||
		kpairbad(r, KEY_PASSWORD2) ||
		kpairbad(r, KEY_PASSWORD3) ||
		strcmp(r->fieldmap[KEY_PASSWORD2]->parsed.s,
		       r->fieldmap[KEY_PASSWORD3]->parsed.s) ||
		! db_admin_valid_pass
			(r->fieldmap[KEY_PASSWORD1]->parsed.s)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} 

	t = time(NULL) + 60 * 60 * 24 * 365;
	tm = gmtime(&t);
	strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm);

	db_sess_delete(r->cookiemap[KEY_SESSID]->parsed.i);
	db_admin_set_pass(r->fieldmap[KEY_PASSWORD2]->parsed.s);
	http_open(r, KHTTP_200);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=%s", 
		keys[KEY_SESSCOOKIE].name, buf);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=%s", 
		keys[KEY_SESSID].name, buf);
	khttp_body(r);
}

static void
senddoaddgame(struct kreq *r)
{
	struct game	*game;

	if (kpairbad(r, KEY_NAME) ||
		kpairbad(r, KEY_PAYOFFS) ||
		kpairbad(r, KEY_P1) ||
		kpairbad(r, KEY_P2)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} 

	game = db_game_alloc
		(r->fieldmap[KEY_PAYOFFS]->parsed.s,
		 r->fieldmap[KEY_NAME]->parsed.s,
		 r->fieldmap[KEY_P1]->parsed.i,
		 r->fieldmap[KEY_P2]->parsed.i);

	/* FIXME: this can also be KHTTP_409. */
	if (NULL == game) 
		http_open(r, KHTTP_400);
	else
		http_open(r, KHTTP_200);

	khttp_body(r);
	db_game_free(game);
}

static char *
trim(char *val)
{
	char		*cp;

	if ('\0' == *val)
		return(val);

	cp = val + strlen(val) - 1;
	while (cp > val && isspace((unsigned char)*cp))
		*cp-- = '\0';

	cp = val;
	while (isspace((unsigned char)*cp))
		cp++;

	return(cp);
}


static char *
valid_email(char *p)
{
	char		*domain, *cp, *start;
	size_t		 i, sz;

	cp = start = trim(p);

	if ((sz = strlen(cp)) < 5 || sz > 254)
		return(NULL);
	if (NULL == (domain = strchr(cp, '@')))
		return(NULL);
	if ((sz = domain - cp) < 1 || sz > 64)
		return(NULL);

	for (i = 0; i < sz; i++) {
		if (isalnum((unsigned char)cp[i]))
			continue;
		if (NULL == strchr("!#$%&'*+-/=?^_`{|}~.", cp[i]))
			return(NULL);
	}

	assert('@' == cp[i]);
	cp = &cp[++i];
	if ((sz = strlen(cp)) < 4 || sz > 254)
		return(NULL);

	for (i = 0; i < sz; i++) 
		if ( ! isalnum((unsigned char)cp[i]))
			if (NULL == strchr("-.", cp[i]))
				return(NULL);

	for (cp = start; '\0' != *cp; cp++)
		*cp = tolower((unsigned char)*cp);

	return(start);
}

static void
senddobackup(struct kreq *r)
{

	http_open(r, KHTTP_200);
	khttp_body(r);
	db_close();

	if (0 == doublefork(r)) {
		mail_backup();
		exit(EXIT_SUCCESS);
	}
}

static void
senddoadvanceend(struct kreq *r)
{

	db_expr_advanceend();
	http_open(r, KHTTP_200);
	khttp_body(r);
}

static void
senddoadvance(struct kreq *r)
{

	db_expr_advancenext();
	http_open(r, KHTTP_200);
	khttp_body(r);
}

static void
senddoaddplayers(struct kreq *r)
{
	char	*tok, *buf, *mail, *sv;

	http_open(r, KHTTP_200);
	khttp_body(r);

	db_expr_setautoadd
		(NULL != r->fieldmap[KEY_AUTOADD] ?
		 r->fieldmap[KEY_AUTOADD]->parsed.i : 0,
		 NULL != r->fieldmap[KEY_AUTOADDPRESERVE] ?
		 r->fieldmap[KEY_AUTOADDPRESERVE]->parsed.i : 0);

	if (NULL == r->fieldmap[KEY_PLAYERS])
		return;

	buf = sv = kstrdup(r->fieldmap[KEY_PLAYERS]->parsed.s);

	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		if (*tok == '\0')
			continue;
		if (NULL == (mail = valid_email(tok)))
			continue;
		 db_player_create(mail, NULL, NULL, NULL);
	}

	free(sv);
}

static void
senddoloadplayer(const struct player *player, void *arg)
{
	struct kjsonreq	*r = arg;

	kjson_obj_open(r);
	json_putplayer(r, player);
	kjson_obj_close(r);
}

static void
senddoloadgame(const struct game *game, void *arg)
{
	struct kjsonreq	*r = arg;

	kjson_obj_open(r);
	kjson_putintp(r, "id", game->id);
	kjson_putintp(r, "p1", game->p1);
	kjson_putintp(r, "p2", game->p2);
	kjson_putstringp(r, "name", game->name);
	json_putmpqs(r, "payoffs", 
		game->payoffs, game->p1, game->p2);
	kjson_obj_close(r);
}

static void
senddoloadplayers(struct kreq *r)
{
	struct kjsonreq	 req;
	struct expr	*expr;

	expr = db_expr_get(0);
	assert(NULL != expr);
	http_open(r, KHTTP_200);
	khttp_body(r);
	kjson_open(&req, r);
	kjson_obj_open(&req);
	kjson_putintp(&req, "autoadd", expr->autoadd);
	kjson_putintp(&req, "prounds", expr->prounds);
	kjson_putintp(&req, "round", expr->round);
	kjson_putintp(&req, "autoaddpreserve", expr->autoaddpreserve);
	kjson_arrayp_open(&req, "players");
	db_player_load_all(senddoloadplayer, &req);
	kjson_array_close(&req);
	kjson_obj_close(&req);
	kjson_close(&req);
	db_expr_free(expr);
}

static void
senddoloadgames(struct kreq *r)
{
	struct kjsonreq	 req;

	http_open(r, KHTTP_200);
	khttp_body(r);
	kjson_open(&req, r);
	kjson_array_open(&req);
	db_game_load_all(senddoloadgame, &req);
	kjson_array_close(&req);
	kjson_close(&req);
}

static void
senddologin(struct kreq *r)
{
	struct sess	*sess;
	time_t		 t;
	struct tm	*tm;
	char		 buf[64];

	if (admin_valid(r)) {
		t = time(NULL) + 60 * 60 * 24 * 365;
		tm = gmtime(&t);
		strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm);
		sess = db_admin_sess_alloc
			(NULL != r->reqmap[KREQU_USER_AGENT] ?
			 r->reqmap[KREQU_USER_AGENT]->val : "");
		http_open(r, KHTTP_200);
		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRId64 "; path=/; expires=%s", 
			keys[KEY_SESSCOOKIE].name, sess->cookie, buf);
		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRId64 "; path=/; expires=%s", 
			keys[KEY_SESSID].name, sess->id, buf);
		db_sess_free(sess);
	} else
		http_open(r, KHTTP_400);

	khttp_body(r);
}

static void
senddosetinstr(struct kreq *r)
{
	struct expr	*expr;

	if (kpairbad(r, KEY_INSTR)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} else if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	}

	if (expr->round < expr->rounds) {
		db_expr_setinstr(r->fieldmap[KEY_INSTR]->parsed.s);
		http_open(r, KHTTP_200);
	} else
		http_open(r, KHTTP_409);

	khttp_body(r);
	db_expr_free(expr);
}

static void
senddoresetpasswordss(struct kreq *r)
{
	pid_t		 pid;
	char		*loginuri, *uri;
	struct expr	*expr;

	if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	}

	if (NULL != r->fieldmap[KEY_PLAYERID])
		db_player_set_state
			(r->fieldmap[KEY_PLAYERID]->parsed.i, 
			 PSTATE_NEW);
	else
		db_player_reset_all();

	http_open(r, KHTTP_200);
	khttp_body(r);
	loginuri = kstrdup(expr->loginuri);
	kasprintf(&uri, "%s://%s" HTURI 
		"/playerlogin.html", 
		kschemes[r->scheme], r->host);
	db_expr_free(expr);
	db_close();

	if (-1 == (pid = fork())) {
		WARN("fork");
		return;
	} else if (pid > 0) {
		if (-1 == waitpid(pid, NULL, 0))
			WARN("waitpid");
		free(loginuri);
		free(uri);
		return;
	}

	khttp_child_free(r);
	if (daemon(1, 1) < 0) 
		WARN("daemon");
	else
		mail_players(uri, loginuri);

	free(loginuri);
	free(uri);
	exit(EXIT_SUCCESS);
}

static void
senddoresendmail(struct kreq *r)
{
	char		*loginuri, *uri;
	struct expr	*expr;

	if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	}

	db_player_reset_error();

	http_open(r, KHTTP_200);
	khttp_body(r);
	loginuri = kstrdup(expr->loginuri);
	kasprintf(&uri, "%s://%s" HTURI 
		"/playerlogin.html", 
		kschemes[r->scheme], r->host);
	db_expr_free(expr);

	if (0 == doublefork(r)) {
		mail_players(uri, loginuri);
		free(loginuri);
		free(uri);
		exit(EXIT_SUCCESS);
	}

	free(loginuri);
	free(uri);
}

static void
senddosetmturk(struct kreq *r)
{

	if (kpairbad(r, KEY_AWSREWARD) ||
	    kpairbad(r, KEY_AWSWORKER_HITAPPRV) ||
	    kpairbad(r, KEY_AWSWORKER_PCTAPPRV) ||
	    kpairbad(r, KEY_AWSACCESSKEY) ||
	    kpairbad(r, KEY_AWSSECRETKEY) ||
	    kpairbad(r, KEY_AWSNAME) ||
	    kpairbad(r, KEY_AWSDESC) ||
	    kpairbad(r, KEY_AWSKEYWORDS)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	}

	db_expr_setmturk
		(r->fieldmap[KEY_AWSACCESSKEY]->parsed.s,
	 	 r->fieldmap[KEY_AWSSECRETKEY]->parsed.s,
	 	 r->fieldmap[KEY_AWSWORKERS]->parsed.i,
	 	 r->fieldmap[KEY_AWSNAME]->parsed.s,
	 	 r->fieldmap[KEY_AWSDESC]->parsed.s,
	 	 r->fieldmap[KEY_AWSKEYWORDS]->parsed.s,
	 	 NULL != r->fieldmap[KEY_AWSSANDBOX],
	 	 r->fieldmap[KEY_AWSCONVERSION]->parsed.d,
	 	 r->fieldmap[KEY_AWSREWARD]->parsed.d,
 	 	 r->fieldmap[KEY_AWSWORKER_LOCALE]->parsed.s,
	 	 r->fieldmap[KEY_AWSWORKER_HITAPPRV]->parsed.i,
	 	 r->fieldmap[KEY_AWSWORKER_PCTAPPRV]->parsed.i);
	http_open(r, KHTTP_200);
	khttp_body(r);
}

static char *
historytrim(struct kreq *r)
{
	struct json_object	*obj, *h, *game, *rs, *ru, *skip;
	struct array_list	*al;
	size_t			 i, gsz, rsz;
	int			 j, hiskip;
	char			*v;
	struct game		*gs;

	gs = db_game_load_all_array(&gsz);
	assert(NULL != gs);
	obj = json_tokener_parse
		(r->fieldmap[KEY_HISTORYFILE]->parsed.s);
	assert(NULL != obj);
	json_object_object_get_ex(obj, "history", &h);
	assert(NULL != h);
	hiskip = 0;

	/* Make sure number of actions match. */
	for (i = 0; i < gsz; i++) {
		game = json_object_array_get_idx(h, i);
		assert(json_type_object == json_object_get_type(obj));
		json_object_object_get_ex(game, "roundups", &rs);
		assert(NULL != rs);
		assert(json_type_array == json_object_get_type(rs));
		rsz = json_object_array_length(rs);
		for (j = rsz - 1; j >= 0; j--) {
			ru = json_object_array_get_idx(rs, j);
			assert(NULL != ru);
			json_object_object_get_ex(ru, "skip", &skip);
			assert(NULL != skip);
			assert(json_type_int == json_object_get_type(skip));
			if (0 == json_object_get_int(skip))
				break;
		}
#if 0
		WARNX("high[%zu] = %d", i, j);
#endif
		if (j + 1 > hiskip)
			hiskip = j + 1;
	}

#if 0
	WARNX("high = %d", hiskip);
#endif

	for (i = 0; i < gsz; i++) {
		game = json_object_array_get_idx(h, i);
		assert(json_type_object == json_object_get_type(obj));
		json_object_object_get_ex(game, "roundups", &rs);
		assert(NULL != rs);
		assert(json_type_array == json_object_get_type(rs));
		al = json_object_get_array(rs);
		al->length = hiskip;
	}

	v = kstrdup(json_object_to_json_string(obj));
	json_object_put(obj);
	db_game_free_array(gs, gsz);
	return(v);
}

/*
 * Make sure that, if a history file was uploaded, it matches the
 * current number of games and strategies per game.
 * XXX: since json-c can't run in a validator (it access /dev/urandom),
 * we need to check against an invalid file and an empty one.
 * We require a KEY_HISTORYFILE for this, but it can be an empty string
 * in which case we return false (not bad).
 */
static int
historybad(struct kreq *r)
{
	struct json_object	*obj, *h, *game, *p1, *p2, *rs, *ru, *skip;
	size_t			 i, j, gsz, rsz;
	ssize_t			 grsz = -1;
	int			 rc = 1;
	struct game		*gs;

	if (0 == r->fieldmap[KEY_HISTORYFILE]->valsz)
		return(0);

	/* Get top-level "history" object. */
	obj = json_tokener_parse
		(r->fieldmap[KEY_HISTORYFILE]->parsed.s);
	if (NULL == obj)
		return(1);
	assert(NULL != obj);

	gs = db_game_load_all_array(&gsz);
	assert(NULL != gs);

	/* Make sure top-level object is array with correct size. */
	if (json_type_object != json_object_get_type(obj)) {
		WARNX("history: top-level JSON is not object");
		goto out;
	} else if ( ! json_object_object_get_ex(obj, "history", &h)) {
		WARNX("history: no top-level history array");
		goto out;
	} else if (json_type_array != json_object_get_type(h)) {
		WARNX("history: top-level history JSON not array");
		goto out;
	} else if (gsz != json_object_array_length(h)) {
		WARNX("history: different number of games");
		goto out;
	}

	/* Make sure number of actions match. */
	for (i = 0; i < gsz; i++) {
		game = json_object_array_get_idx(h, i);
		if (json_type_object != json_object_get_type(obj)) {
			WARNX("history: game JSON is not object");
			goto out;
		} else if ( ! json_object_object_get_ex(game, "p1", &p1)) {
			WARNX("history: no player 1 strategy size");
			goto out;
		} else if (json_type_int != json_object_get_type(p1)) {
			WARNX("history: player 1 size not int");
			goto out;
		} else if ((int)gs[i].p1 != json_object_get_int(p1)) {
			WARNX("history: player 1 action mismatch");
			goto out;
		} else if ( ! json_object_object_get_ex(game, "p2", &p2)) {
			WARNX("history: no player 2 strategy size");
			goto out;
		} else if (json_type_int != json_object_get_type(p2)) {
			WARNX("history: player 2 size not int");
			goto out;
		} else if ((int)gs[i].p2 != json_object_get_int(p2)) {
			WARNX("history: player 2 action mismatch");
			goto out;
		} else if ( ! json_object_object_get_ex(game, "roundups", &rs)) {
			WARNX("history: no roundups");
			goto out;
		} else if (json_type_array != json_object_get_type(rs)) {
			WARNX("history: roundups not JSON array");
			goto out;
		}
		rsz = json_object_array_length(h);
		if (grsz < 0)
			grsz = rsz;
		if ((size_t)grsz != rsz) {
			WARNX("history: inconsistent roundup count");
			goto out;
		}
		for (j = 0; j < rsz; j++) {
			ru = json_object_array_get_idx(rs, j);
			if (json_type_object != json_object_get_type(ru)) {
				WARNX("history: roundup JSON is not object");
				goto out;
			} else if ( ! json_object_object_get_ex(ru, "skip", &skip)) {
				WARNX("history: no skip");
				goto out;
			} else if (json_type_int != json_object_get_type(skip)) {
				WARNX("history: skip not int");
				goto out;
			}
		}
	}
	rc = 0;
out:
	json_object_put(obj);
	db_game_free_array(gs, gsz);
	return(rc);
}

static void
senddostartexpr(struct kreq *r)
{
	pid_t		  pid;
	char		 *loginuri, *uri, *map, *server, *json, *start;
	struct kpair	 *kpques, *kpans;
	struct expr	 *expr;
	const char	 *instdat;
	const char	**questions, **answers;
	enum instrs	  inst;
	int		  fd;
	int64_t		  flags;
	struct stat	  st;
	size_t		  i, sz, qsz;

	if (kpairbad(r, KEY_DATE) ||
	    kpairbad(r, KEY_INSTR) ||
	    kpairbad(r, KEY_LOTTERY) ||
	    kpairbad(r, KEY_MAILROUND) ||
	    kpairbad(r, KEY_MINUTES) ||
	    kpairbad(r, KEY_PLAYERMAX) ||
	    kpairbad(r, KEY_PROUNDS) ||
	    kpairbad(r, KEY_QUESTIONNAIRE) ||
	    kpairbad(r, KEY_CUSTOMCOUNT) ||
	    kpairbad(r, KEY_RELATIVE) ||
	    kpairbad(r, KEY_ROUNDMIN) ||
	    kpairbad(r, KEY_ROUNDPCT) ||
	    kpairbad(r, KEY_ROUNDS) ||
	    kpairbad(r, KEY_SHOWHISTORY) ||
	    kpairbad(r, KEY_HISTORYFILE) ||
	    kpairbad(r, KEY_SHUFFLE) ||
	    kpairbad(r, KEY_TIME) ||
	    kpairbad(r, KEY_URI) ||
	    db_game_count_all() < 1 ||
	    historybad(r)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} 

	/* 
	 * Check that we have all custom questions and answers. 
	 * We only do this if we've both indicated that we want
	 * questions *and* that the administrator has given us some.
	 */

	questions = answers = NULL;
	qsz = 0;

	if (r->fieldmap[KEY_QUESTIONNAIRE]->parsed.i &&
	    r->fieldmap[KEY_CUSTOMCOUNT]->parsed.i) {
		qsz = r->fieldmap[KEY_CUSTOMCOUNT]->parsed.i;
		questions = kcalloc(qsz, sizeof(char *));
		answers = kcalloc(qsz, sizeof(char *));
		kpques = r->fieldmap[KEY_CUSTOMQUESTION];
		kpans = r->fieldmap[KEY_CUSTOMANSWER];
		for (i = 0; i < qsz; i++) {
			if (NULL == kpques || NULL == kpans)
				break;
			if ( ! kvalid_stringne(kpques) ||
			     ! kvalid_stringne(kpans))
				break;
			questions[qsz - i - 1] = kpques->parsed.s;
			answers[qsz - i - 1] = kpans->parsed.s;
			kpques = kpques->next;
			kpans = kpans->next;
		}
		if (i < qsz) {
			http_open(r, KHTTP_400);
			khttp_body(r);
			free(questions);
			free(answers);
			return;
		}
	}

	/* 
	 * Determine what kind of instructions to use. 
	 * This is important because we're later going to load the
	 * instructions either from the given string or defaults on our
	 * disc.
	 */

	inst = INSTR__MAX;
	if (0 == strcmp("custom", r->fieldmap[KEY_INSTR]->parsed.s))
		inst = kpairbad(r, KEY_INSTRFILE) ?  
			INSTR__MAX : INSTR_CUSTOM;
	if (0 == strcmp("mturk", r->fieldmap[KEY_INSTR]->parsed.s))
		inst = INSTR_MTURK;
	if (0 == strcmp("lottery", r->fieldmap[KEY_INSTR]->parsed.s))
		inst = INSTR_LOTTERY;
	if (0 == strcmp("nolottery", r->fieldmap[KEY_INSTR]->parsed.s))
		inst = INSTR_NOLOTTERY;

	if (INSTR__MAX == inst) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		free(questions);
		free(answers);
		return;
	} 

	sz = 0;
	map = NULL;
	fd = -1;

	flags = 0;
	json = NULL;

	/*
	 * If we have pre-supplied instructions, then read them now.
	 * We do this by simply mmap()ing the entire file into memory.
	 * If we don't have pre-supplied instructions, we point to the
	 * memory held by the uploaded instruction buffer.
	 */

	if (INSTR_LOTTERY == inst || 
	    INSTR_NOLOTTERY == inst ||
	    INSTR_MTURK == inst) {
		fd = open(INSTR_LOTTERY == inst ?
			  DATADIR "/instructions-lottery.xml" :
			  (INSTR_NOLOTTERY == inst ?
			   DATADIR "/instructions-nolottery.xml" :
			   DATADIR "/instructions-mturk.xml"),
			  O_RDONLY, 0);
		if (-1 == fd || -1 == fstat(fd, &st)) {
			WARN("open");
			http_open(r, KHTTP_400);
			khttp_body(r);
			if (-1 != fd)
				close(fd);
			free(questions);
			free(answers);
			return;
		}
		sz = (size_t)st.st_size;
		map = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
		if (MAP_FAILED == map) {
			WARN("mmap");
			http_open(r, KHTTP_400);
			khttp_body(r);
			close(fd);
			free(questions);
			free(answers);
			return;
		}
		instdat = map;
	} else 
		instdat = r->fieldmap[KEY_INSTRFILE]->parsed.s;

	if (r->fieldmap[KEY_SHOWHISTORY]->parsed.i)
		flags |= EXPR_NOHISTORY;
	if (r->fieldmap[KEY_SHUFFLE]->parsed.i)
		flags |= EXPR_NOSHUFFLE;
	if (r->fieldmap[KEY_RELATIVE]->parsed.i)
		flags |= EXPR_RELATIVE;

	if (NULL != r->fieldmap[KEY_HISTORYTRIM] &&
	    r->fieldmap[KEY_HISTORYFILE]->valsz)
		json = historytrim(r);
	else if (r->fieldmap[KEY_HISTORYFILE]->valsz) 
		json = kstrdup(r->fieldmap[KEY_HISTORYFILE]->parsed.s);

	/* 
	 * In order to use the JSON file the user gives us, we need to
	 * trim out the outer `object' components, since the JSON file
	 * was (presumably) directly downloaded from the browser window.
	 */

	if (NULL != json) {
		start = json;
		sz = strlen(start);
		while (isspace((int)*start)) {
			start++;
			sz--;
		}
		if ('{' == *start) {
			start++;
			sz--;
		}
		while (sz && isspace((int)start[sz - 1]))
			sz--;
		if ('}' == start[sz - 1])
			sz--;
		start[sz] = '\0';
	} else
		start = NULL;
	
	/*
	 * Actually start the experiment.
	 * This only fails if the experiment has already been started in
	 * the meantime.
	 */

	if ( ! db_expr_start
		(r->fieldmap[KEY_DATE]->parsed.i +
		 r->fieldmap[KEY_TIME]->parsed.i,
		 r->fieldmap[KEY_ROUNDPCT]->parsed.i,
		 r->fieldmap[KEY_ROUNDMIN]->parsed.i,
		 r->fieldmap[KEY_ROUNDS]->parsed.i,
		 r->fieldmap[KEY_PROUNDS]->parsed.i,
		 r->fieldmap[KEY_MINUTES]->parsed.i,
		 r->fieldmap[KEY_PLAYERMAX]->parsed.i,
		 instdat,
		 r->fieldmap[KEY_URI]->parsed.s,
		 start,
		 r->fieldmap[KEY_LOTTERY]->parsed.s,
		 r->fieldmap[KEY_QUESTIONNAIRE]->parsed.i,
		 flags, questions, answers, qsz)) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		if (INSTR_LOTTERY == inst || 
		    INSTR_NOLOTTERY == inst ||
		    INSTR_MTURK == inst) {
			munmap(map, sz);
			close(fd);
		}
		free(json);
		free(questions);
		free(answers);
		return;
	} 
	
	if (INSTR_LOTTERY == inst || 
	    INSTR_NOLOTTERY == inst ||
	    INSTR_MTURK == inst) {
		munmap(map, sz);
		close(fd);
	}
	free(json);
	free(questions);
	free(answers);

	http_open(r, KHTTP_200);
	khttp_body(r);

	loginuri = kstrdup(r->fieldmap[KEY_URI]->parsed.s);
	kasprintf(&uri, "%s://%s" HTURI 
		"/playerlogin.html", kschemes[r->scheme], r->host);

	/*
	 * The experiment has been started!
	 * Begin everything by preparing the round mailer.
	 * This will fire periodically to mail out notices that the
	 * round has advanced.
	 */

	if (r->fieldmap[KEY_MAILROUND]->parsed.i) {
		db_close();
		if (-1 == (pid = fork())) {
			WARN("fork");
			return;
		} else if (0 == pid) {
			khttp_child_free(r);
			if (daemon(1, 1) < 0)
				WARN("daemon");
			else
				mailround(uri);
			free(loginuri);
			free(uri);
			exit(EXIT_SUCCESS);
		} else if (-1 == waitpid(pid, NULL, 0))
			WARN("waitpid");
	}

	expr = db_expr_get(0);
	assert(NULL != expr);
	if ('\0' != *expr->awssecretkey &&
	    '\0' != *expr->awsaccesskey) {
		/* 
		 * Contacting the Mechanical Turk server happens within
		 * a protected child, so close out now.
		 * Since this is a long-running process, we double-fork
		 * in the usual way and daemonise.
		 */
		db_close();
		if (-1 == (pid = fork())) {
			WARN("fork");
			return;
		} else if (0 == pid) {
			/*
			 * We're in the child.
			 * Free the HTTP request resources: we're not
			 * going to use them any more.
			 */
			server = kstrdup(r->host);
			khttp_child_free(r);
			if (daemon(1, 1) < 0)
				WARN("daemon");
			else
				mturk_create(expr, server);
			db_expr_free(expr);
			free(server);
			exit(EXIT_SUCCESS);
		} else if (-1 == waitpid(pid, NULL, 0))
			WARN("waitpid");
	}
	db_expr_free(expr);

	/*
	 * Now mail out to each of the players to inform them of being
	 * added to the experiment.
	 */

	db_close();
	if (-1 == (pid = fork())) {
		WARN("fork");
		return;
	} else if (pid > 0) {
		if (-1 == waitpid(pid, NULL, 0))
			WARN("waitpid");
		free(loginuri);
		free(uri);
		return;
	}

	khttp_child_free(r);
	if (daemon(1, 1) < 0) 
		WARN("daemon");
	else
		mail_players(uri, loginuri);

	free(loginuri);
	free(uri);
	exit(EXIT_SUCCESS);
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
	send303(r, HTURI "/adminlogin.html", PAGE__MAX, 0);
}

static void
senddowinners(struct kreq *r)
{
	struct expr	*expr;
	struct interval	*intv;
	size_t		 gamesz;
	int		 rc;

	if (kpairbad(r, KEY_WINNERS) || 
	    kpairbad(r, KEY_WINSEED)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} else if (NULL == (expr = db_expr_get(1))) {
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	} else if (expr->round < expr->rounds) {
		db_expr_free(expr);
		http_open(r, KHTTP_409);
		khttp_body(r);
		return;
	}

	/* Make sure we're all round up. */
	intv = db_interval_get(expr->rounds - 1);
	assert(NULL != intv);
	gamesz = intv->periodsz;
	db_interval_free(intv);

	/* Compute winners from that. */
	rc = db_winners(&expr,
		r->fieldmap[KEY_WINNERS]->parsed.i, 
		r->fieldmap[KEY_WINSEED]->parsed.i, 
		gamesz);
	http_open(r, 0 == rc ? KHTTP_409 : KHTTP_200);
	khttp_body(r);
	db_expr_free(expr);
}

static void
senddowipe(struct kreq *r, int mail)
{

	http_open(r, KHTTP_200);
	khttp_body(r);
	db_close();

	if (0 == doublefork(r)) {
		mail_wipe(mail);
		exit(EXIT_SUCCESS);
	}
}

#if 0
/*
 * For now, we need to disable this: the json library will call out to
 * /dev/urandom and that will cause this to crash.
 */
static int
kvalid_history(struct kpair *kp)
{
	struct json_object	*obj;

	if ( ! kvalid_stringne(kp))
		return(0);
	obj = json_tokener_parse(kp->parsed.s);
	if (NULL == obj)
		return(0);
	json_object_put(obj);
	return(1);
}
#endif

static int
kvalid_rounds(struct kpair *kp)
{

	if ( ! kvalid_uint(kp))
		return(0);
	return(kp->parsed.i > 0);
}

static int
kvalid_time(struct kpair *kp)
{

	int		 hr, min;

	if ( ! kvalid_stringne(kp))
		return(0);
	else if (kp->valsz != 5 && kp->valsz != 4)
		return(0);
	else if (':' != kp->val[1] && ':' != kp->val[2])
		return(0);
	else if (':' == kp->val[1] &&
		( ! isdigit(kp->val[0]) ||
	  	  ! isdigit(kp->val[2]) ||
	  	  ! isdigit(kp->val[3]))) 
		return(0);
	else if (':' == kp->val[2] &&
		( ! isdigit(kp->val[0]) ||
	  	  ! isdigit(kp->val[1]) ||
	  	  ! isdigit(kp->val[3]) ||
	  	  ! isdigit(kp->val[4])))
		return(0);

	if (':' == kp->val[1]) {
		hr = atoi(&kp->val[0]);
		min = atoi(&kp->val[2]);
	} else {
		hr = atoi(&kp->val[0]);
		min = atoi(&kp->val[3]);
	}

	kp->parsed.i = hr * 60 * 60 + min * 60;
	kp->type = KPAIR_INTEGER;
	return(1);
}

static int
kvalid_minutes(struct kpair *kp)
{

	if ( ! kvalid_uint(kp))
		return(0);
	return(kp->parsed.i > 0 && kp->parsed.i <= 1440);
}

#if 0
static void 
errLogCallback(void *pArg, int iErrCode, const char *zMsg)
{

	WARNX("sqlite3: %s (%d)", zMsg, iErrCode);
}
#endif

int
main(void)
{
	struct kreq	 r;
	unsigned int	 bit;

	/*
	 * Open our own log file.
	 * This is because we might double-fork, and doing so will cause
	 * problems with FastCGI implementations that wait on stderr
	 * before seeing a channel as closed.
	 */
	freopen(LOGFILE, "a", stderr);
	setlinebuf(stderr);
#if 0
	sqlite3_config(SQLITE_CONFIG_LOG, errLogCallback, NULL);
#endif

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
	/* 
	 * First, make sure that the page accepts the content type we've
	 * assigned to it.
	 * If it doesn't, then run an HTTP 404.
	 */
	switch (r.mime) {
	case (KMIME_TEXT_HTML):
		bit = PERM_HTML;
		break;
	case (KMIME_APP_JSON):
		bit = PERM_JSON;
		break;
	case (KMIME_TEXT_CSV):
		bit = PERM_CSV;
		break;
	default:
		send404(&r);
		goto out;
	}

	if ( ! (perms[r.page] & bit)) {
		send404(&r);
		goto out;
	}

	/*
	 * Next, make sure that all pages that require a login are
	 * attached to a valid administrative session.
	 */
	if ((perms[r.page] & PERM_LOGIN) && ! sess_valid(&r)) {
		if (KMIME_APP_JSON == r.mime) {
			send403(&r);
			goto out;
		}
		send303(&r, HTURI "/adminlogin.html", PAGE__MAX, 1);
		goto out;
	}
	
	db_expr_advance();

	switch (r.page) {
	case (PAGE_DOADDGAME):
		senddoaddgame(&r);
		break;
	case (PAGE_DOADDPLAYERS):
		senddoaddplayers(&r);
		break;
	case (PAGE_DOADVANCE):
		senddoadvance(&r);
		break;
	case (PAGE_DOADVANCEEND):
		senddoadvanceend(&r);
		break;
	case (PAGE_DOBACKUP):
		senddobackup(&r);
		break;
	case (PAGE_DOCHANGEMAIL):
		senddochangemail(&r);
		break;
	case (PAGE_DOCHANGEPASS):
		senddochangepass(&r);
		break;
	case (PAGE_DOCHANGESMTP):
		senddochangesmtp(&r);
		break;
	case (PAGE_DOCHECKROUND):
		senddocheckround(&r);
		break;
	case (PAGE_DOCHECKSMTP):
		senddochecksmtp(&r);
		break;
	case (PAGE_DOCLEARMTURK):
		senddoclearmturk(&r);
		break;
	case (PAGE_DODELETEGAME):
		senddodeletegame(&r);
		break;
	case (PAGE_DODELETEPLAYER):
		senddodeleteplayer(&r);
		break;
	case (PAGE_DODISABLEPLAYER):
		senddodisableplayer(&r);
		break;
	case (PAGE_DOENABLEPLAYER):
		senddoenableplayer(&r);
		break;
	case (PAGE_DOGETHIGHEST):
		senddogethighest(&r);
		break;
	case (PAGE_DOGETEXPR):
		senddogetexpr(&r);
		break;
	case (PAGE_DOGETHISTORY):
		senddogethistory(&r);
		break;
	case (PAGE_DOLOADGAMES):
		senddoloadgames(&r);
		break;
	case (PAGE_DOLOADPLAYERS):
		senddoloadplayers(&r);
		break;
	case (PAGE_DOLOGIN):
		senddologin(&r);
		break;
	case (PAGE_DOLOGOUT):
		senddologout(&r);
		break;
	case (PAGE_DOMTURKBONUSES):
		senddomturkbonuses(&r);
		break;
	case (PAGE_DOSTARTEXPR):
		senddostartexpr(&r);
		break;
	case (PAGE_DORESENDEMAIL):
		senddoresendmail(&r);
		break;
	case (PAGE_DORESETPASSWORDS):
		senddoresetpasswordss(&r);
		break;
	case (PAGE_DOSETINSTR):
		senddosetinstr(&r);
		break;
	case (PAGE_DOSETMTURK):
		senddosetmturk(&r);
		break;
	case (PAGE_DOTESTSMTP):
		senddotestsmtp(&r);
		break;
	case (PAGE_DOWINNERS):
		senddowinners(&r);
		break;
	case (PAGE_DOWIPE):
		senddowipe(&r, 1);
		break;
	case (PAGE_DOWIPEQUIET):
		senddowipe(&r, 0);
		break;
	case (PAGE_INDEX):
		send303(&r, HTURI "/adminlogin.html", PAGE__MAX, 1);
		break;
	default:
		send404(&r);
		break;
	}

out:
	khttp_free(&r);
	return(EXIT_SUCCESS);
}

