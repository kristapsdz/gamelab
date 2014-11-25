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
#include <kcgijson.h>

#include "extern.h"

/*
 * Unique pages under the CGI "directory".
 */
enum	page {
	PAGE_DOLOADEXPR,
	PAGE_DOLOGIN,
	PAGE_DOLOGOUT,
	PAGE_DOPLAY,
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
	KEY_GAMEID,
	KEY_PASSWORD,
	KEY_ROUND,
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

#define	PERM_LOGIN	0x01
#define	PERM_HTML	0x08
#define	PERM_JSON	0x10
#define	PERM_CSS	0x20
#define	PERM_JS		0x40

static	unsigned int perms[PAGE__MAX] = {
	PERM_JSON | PERM_LOGIN, /* PAGE_DOLOADEXPR */
	PERM_JSON | PERM_HTML, /* PAGE_DOLOGIN */
	PERM_HTML | PERM_LOGIN, /* PAGE_DOLOGOUT */
	PERM_JSON | PERM_LOGIN, /* PAGE_DOPLAY */
	PERM_JS | PERM_HTML | PERM_LOGIN, /* PAGE_HOME */
	PERM_JS | PERM_HTML | PERM_LOGIN, /* PAGE_INDEX */
	PERM_HTML, /* PAGE_LOGIN */
	PERM_CSS, /* PAGE_STYLE */
};

static const char *const pages[PAGE__MAX] = {
	"doloadexpr", /* PAGE_DOLOADEXPR */
	"dologin", /* PAGE_DOLOGIN */
	"dologout", /* PAGE_DOLOGOUT */
	"doplay", /* PAGE_DOPLAY */
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
	{ kvalid_int, "gid" }, /* KEY_GAMEID */
	{ kvalid_stringne, "password" }, /* KEY_PASSWORD */
	{ kvalid_uint, "round" }, /* KEY_ROUND */
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
send404(struct kreq *r)
{

	http_open(r, KHTTP_404);
	khttp_body(r);
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
	khttp_puts(r, "Redirecting...");
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
		khttp_puts(r, r->pname);
		break;
	case (TEMPL_HTDOCS):
		p = getenv("HTML_URI");
		khttp_puts(r, NULL != p ? p : "");
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
	struct kjsonreq	*r = arg;

	kjson_obj_open(r);
	kjson_putintp(r, "p1", game->p1);
	kjson_putintp(r, "p2", game->p2);
	kjson_putstringp(r, "name", game->name);
	json_putmpqs(r, game->payoffs, game->p1, game->p2);
	kjson_putintp(r, "id", game->id);
	kjson_obj_close(r);
}

static void
senddoloadexpr(struct kreq *r, int64_t playerid)
{
	struct expr	*expr;
	struct player	*player;
	time_t		 t;
	struct kjsonreq	 req;

	player = db_player_load(playerid);
	expr = db_expr_get();

	http_open(r, KHTTP_200);
	khttp_body(r);

	kjson_open(&req, r);
	kjson_obj_open(&req);
	if ((t = time(NULL)) < expr->end && t > expr->start) {
		kjson_putintp(&req, "role", player->role);
		kjson_arrayp_open(&req, "games");
		db_game_load_player(playerid, 
			(t - expr->start) / (expr->minutes * 60), 
			senddoloadgame, &req);
		kjson_array_close(&req);
	}
	json_putexpr(&req, expr);
	kjson_obj_close(&req);
	kjson_close(&req);
	db_expr_free(expr);
	db_player_free(player);
}

static void
senddoplay(struct kreq *r, int64_t playerid)
{
	struct player	*player;

	if (kpairbad(r, KEY_GAMEID) ||
		kpairbad(r, KEY_ROUND)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	}

	player = db_player_load(playerid);
	assert(NULL != player);
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
	unsigned int	 bit;
	
	if ( ! khttp_parse(&r, keys, KEY__MAX, 
			pages, PAGE__MAX, PAGE_INDEX))
		return(EXIT_FAILURE);

	/* 
	 * First, make sure that the page accepts the content type we've
	 * assigned to it.
	 * If it doesn't, then run an HTTP 404.
	 */
	switch (r.mime) {
	case (KMIME_TEXT_CSS):
		bit = PERM_CSS;
		break;
	case (KMIME_TEXT_HTML):
		bit = PERM_HTML;
		break;
	case (KMIME_APP_JSON):
		bit = PERM_JSON;
		break;
	case (KMIME_APP_JAVASCRIPT):
		bit = PERM_JS;
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
		send303(&r, PAGE_LOGIN, 1);
		goto out;
	}

	switch (r.page) {
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
	case (PAGE_HOME):
		if (KMIME_APP_JAVASCRIPT == r.mime)
			sendcontent(&r, CNTT_JS_HOME);
		else
			sendcontent(&r, CNTT_HTML_HOME);
		break;
	case (PAGE_INDEX):
		send303(&r, PAGE_HOME, 1);
		break;
	case (PAGE_LOGIN):
		sendcontent(&r, CNTT_HTML_LOGIN);
		break;
	case (PAGE_STYLE):
		sendcontent(&r, CNTT_CSS_STYLE);
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

