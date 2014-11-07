/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include <sys/param.h>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gmp.h>
#include <kcgi.h>

#include "extern.h"

/*
 * Unique pages under the CGI "directory".
 */
enum	page {
	PAGE_DOLOADEXPR,
	PAGE_DOLOGIN,
	PAGE_DOLOGOUT,
	PAGE_HOME,
	PAGE_INDEX,
	PAGE_LOGIN,
	PAGE_STYLE,
	PAGE__MAX
};

/*
 * Content that we manage.
 * By "manage" I mean that we template them.
 */
enum	cntt {
	CNTT_CSS_STYLE,
	CNTT_HTML_HOME,
	CNTT_HTML_LOGIN,
	CNTT_JS_HOME,
	CNTT__MAX
};

/*
 * Input form field names.
 */
enum	key {
	KEY_EMAIL,
	KEY_PASSWORD,
	KEY_SESSCOOKIE,
	KEY_SESSID,
	KEY__MAX
};

/*
 * Values in our content that we're templating.
 */
enum	templ {
	TEMPL_CGIBIN,
	TEMPL_HTDOCS,
	TEMPL__MAX
};

static const char *const pages[PAGE__MAX] = {
	"doloadexpr", /* PAGE_DOLOADEXPR */
	"dologin", /* PAGE_DOLOGIN */
	"dologout", /* PAGE_DOLOGOUT */
	"home", /* PAGE_HOME */
	"index", /* PAGE_INDEX */
	"login", /* PAGE_LOGIN */
	"style", /* PAGE_STYLE */
};

static	const char *const templs[TEMPL__MAX] = {
	"cgibin", /* TEMPL_CGIBIN */
	"htdocs" /* TEMPL_HTDOCS */
};

static const char *const cntts[CNTT__MAX] = {
	"style.css", /* CNTT_CSS_STYLE */
	"playerhome.html", /* CNTT_HTML_HOME_NEW */
	"playerlogin.html", /* CNTT_HTML_LOGIN */
	"playerhome.js", /* CNTT_JS_HOME */
};

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_email, "email" }, /* KEY_EMAIL */
	{ kvalid_stringne, "password" }, /* KEY_PASSWORD */
	{ kvalid_int, "sesscookie" }, /* KEY_SESSCOOKIE */
	{ kvalid_int, "sessid" }, /* KEY_SESSID */
};

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
}

static void
send303(struct kreq *r, enum page dest, int dostatus)
{
	char	*page, *full;

	if (dostatus)
		http_open(r, KHTTP_303);
	page = kutil_urlpart(r, r->pname, 
		ksuffixes[r->mime], pages[dest], NULL);
	full = kutil_urlabs(KSCHEME_HTTP, r->host, r->port, page);
	khttp_head(r, kresps[KRESP_LOCATION], "%s", full);
	khttp_body(r);
	khtml_text(r, "Redirecting...");
	free(full);
	free(page);
}

static int
sendtempl(size_t key, void *arg)
{
	const char	*p;
	struct kreq	*r = arg;

	switch (key) {
	case (TEMPL_CGIBIN):
		khtml_text(r, r->pname);
		break;
	case (TEMPL_HTDOCS):
		p = getenv("HTML_URI");
		khtml_text(r, NULL != p ? p : "");
		break;
	default:
		break;
	}
	return(1);
}

static void
sendcontent(struct kreq *r, enum cntt cntt)
{
	struct ktemplate t;
	char		 fname[PATH_MAX];
	const char	*p;

	t.key = templs;
	t.keysz = TEMPL__MAX;
	t.arg = r;
	t.cb = sendtempl;

	p = getenv("DB_DIR");
	snprintf(fname, sizeof(fname), "%s/%s",
		NULL != p ? p : ".", cntts[cntt]);

	http_open(r, KHTTP_200);
	khttp_body(r);
	khttp_template(r, &t, fname);
}

static void
senddologin(struct kreq *r)
{
	struct sess	*sess;
	int64_t	 	 playerid;

	if (player_valid(r, &playerid)) {
		sess = db_player_sess_alloc(playerid);
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
			send303(r, PAGE_HOME, 0);
		else
			khttp_body(r);
		db_sess_free(sess);
	} else {
		http_open(r, KHTTP_400);
		if (KMIME_TEXT_HTML == r->mime) {
			http_open(r, KHTTP_303);
			send303(r, PAGE_LOGIN, 0);
		} else {
			http_open(r, KHTTP_400);
			khttp_body(r);
		}
	}

}

static void
senddoloadgame(const struct game *game, size_t count, void *arg)
{
	struct kreq	*r = arg;

	if (count > 0)
		khttp_putc(r, ',');
	khttp_putc(r, '{');
	json_putint(r, "p1", game->p1);
	khttp_putc(r, ',');
	json_putint(r, "p2", game->p2);
	khttp_putc(r, ',');
	json_putstring(r, "name", game->name);
	khttp_putc(r, ',');
	json_putmpqs(r, "payoffs", game->payoffs, game->p1, game->p2);
	khttp_putc(r, '}');
}

static void
senddoloadexpr(struct kreq *r, int64_t playerid)
{
	struct expr	*expr;
	struct player	*player;
	time_t		 t;

	player = db_player_load(playerid);
	
	expr = db_expr_get();
	assert(NULL != expr);
	http_open(r, KHTTP_200);
	khttp_body(r);

	if (expr->start > (t = time(NULL))) {
		khttp_putc(r, '{');
		json_putint(r, "tilstart", (int64_t)(expr->start - t));
		khttp_putc(r, '}');
	} else {
		khttp_putc(r, '{');
		json_putstring(r, "tilstart", "0");
		khttp_putc(r, ',');
		json_putint(r, "role", player->role);
		khttp_putc(r, ',');
		json_puts(r, "games");
		khttp_putc(r, ':');
		khttp_putc(r, '[');
		db_game_load_all(senddoloadgame, r);
		khttp_putc(r, ']');
		khttp_putc(r, '}');
	}

	db_expr_free(expr);
	db_player_free(player);
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
	send303(r, PAGE_LOGIN, 0);
}

int
main(void)
{
	struct kreq	 r;
	int64_t		 id;
	
	if ( ! khttp_parse(&r, keys, KEY__MAX, 
			pages, PAGE__MAX, PAGE_INDEX))
		return(EXIT_FAILURE);

	/* 
	 * First handle pages that don't require login.
	 * This consists only of static (but templated) pages and the
	 * login page and its POST receiver.
	 */
	switch (r.page) {
	case (PAGE_DOLOGIN):
		senddologin(&r);
		khttp_free(&r);
		return(EXIT_SUCCESS);
	case (PAGE_STYLE):
		sendcontent(&r, CNTT_CSS_STYLE);
		khttp_free(&r);
		return(EXIT_SUCCESS);
	case (PAGE_LOGIN):
		sendcontent(&r, CNTT_HTML_LOGIN);
		khttp_free(&r);
		return(EXIT_SUCCESS);
	default:
		break;
	}

	/* Everything now must have a login. */
	if ( ! sess_valid(&r, &id)) {
		send303(&r, PAGE_LOGIN, 1);
		khttp_free(&r);
		return(EXIT_SUCCESS);
	}

	switch (r.page) {
	case (PAGE_INDEX):
		send303(&r, PAGE_HOME, 1);
		break;
	case (PAGE_HOME):
		if (KMIME_APP_JAVASCRIPT == r.mime)
			sendcontent(&r, CNTT_JS_HOME);
		else
			sendcontent(&r, CNTT_HTML_HOME);
		break;
	case (PAGE_DOLOADEXPR):
		senddoloadexpr(&r, id);
		break;
	case (PAGE_DOLOGOUT):
		senddologout(&r);
		break;
	default:
		http_open(&r, KHTTP_404);
		khttp_body(&r);
		break;
	}

	khttp_free(&r);
	return(EXIT_SUCCESS);
}

