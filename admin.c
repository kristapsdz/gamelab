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

#include <gmp.h>
#include <kcgi.h>

#include "extern.h"

enum	page {
	PAGE_DOADDGAME,
	PAGE_DOADDPLAYERS,
	PAGE_DOCHANGEMAIL,
	PAGE_DOCHANGEPASS,
	PAGE_DOLOADGAMES,
	PAGE_DOLOGIN,
	PAGE_DOLOGOUT,
	PAGE_HOME,
	PAGE_INDEX,
	PAGE_LOGIN,
	PAGE_STYLE,
	PAGE__MAX
};

enum	cntt {
	CNTT_CSS_STYLE,
	CNTT_HTML_HOME,
	CNTT_HTML_LOGIN,
	CNTT__MAX
};

enum	key {
	KEY_EMAIL,
	KEY_EMAIL1,
	KEY_EMAIL2,
	KEY_EMAIL3,
	KEY_NAME,
	KEY_P1,
	KEY_P2,
	KEY_PASSWORD,
	KEY_PASSWORD1,
	KEY_PASSWORD2,
	KEY_PASSWORD3,
	KEY_PAYOFFS,
	KEY_PLAYERS,
	KEY_SESSCOOKIE,
	KEY_SESSID,
	KEY__MAX
};

enum	templ {
	TEMPL_CGIBIN,
	TEMPL_HTDOCS,
	TEMPL__MAX
};

static const char *const pages[PAGE__MAX] = {
	"doaddgame", /* PAGE_DOADDGAME */
	"doaddplayers", /* PAGE_DOADDPLAYERS */
	"dochangemail", /* PAGE_DOCHANGEMAIL */
	"dochangepass", /* PAGE_DOCHANGEPASS */
	"doloadgames", /* PAGE_DOLOADGAMES */
	"dologin", /* PAGE_DOLOGIN */
	"dologout", /* PAGE_DOLOGOUT */
	"home", /* PAGE_HOME */
	"index", /* PAGE_INDEX */
	"login", /* PAGE_LOGIN */
	"style", /* PAGE_STYLE */
};

static	const char *const templs[TEMPL__MAX] = {
	"cgibin",
	"htdocs"
};

static const char *const cntts[CNTT__MAX] = {
	"style.css", /* CNTT_CSS_STYLE */
	"adminhome.html", /* CNTT_HTML_HOME */
	"adminlogin.html", /* CNTT_HTML_LOGIN */
};

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_email, "email" }, /* KEY_EMAIL */
	{ kvalid_email, "email1" }, /* KEY_EMAIL1 */
	{ kvalid_email, "email2" }, /* KEY_EMAIL2 */
	{ kvalid_email, "email3" }, /* KEY_EMAIL3 */
	{ kvalid_stringne, "name" }, /* KEY_NAME */
	{ kvalid_int, "p1" }, /* KEY_P1 */
	{ kvalid_int, "p2" }, /* KEY_P2 */
	{ kvalid_stringne, "password" }, /* KEY_PASSWORD */
	{ kvalid_stringne, "password1" }, /* KEY_PASSWORD1 */
	{ kvalid_stringne, "password2" }, /* KEY_PASSWORD2 */
	{ kvalid_stringne, "password3" }, /* KEY_PASSWORD3 */
	{ kvalid_stringne, "payoffs" }, /* KEY_PAYOFFS */
	{ kvalid_stringne, "players" }, /* KEY_PLAYERS */
	{ kvalid_int, "sesscookie" }, /* KEY_SESSCOOKIE */
	{ kvalid_int, "sessid" }, /* KEY_SESSID */
};

static void
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

static void
json_putint(struct kreq *r, const char *key, int64_t val)
{
	char	buf[22];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);

	assert('\0' != *key);
	json_puts(r, key);
	khttp_puts(r, " : ");
	json_puts(r, buf);
}

static void
json_putstring(struct kreq *r, const char *key, const char *val)
{

	assert('\0' != *key);
	json_puts(r, key);
	khttp_puts(r, " : ");
	json_puts(r, val);
}

static void
json_putmpqs(struct kreq *r, const char *key, 
	const mpq_t *vals, int64_t p1, int64_t p2)
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

static int
sess_valid(struct kreq *r)
{

	if (NULL == r->cookiemap[KEY_SESSID] ||
		NULL == r->cookiemap[KEY_SESSCOOKIE]) 
		return(0);

	return(db_sess_valid(r->cookiemap[KEY_SESSID]->parsed.i,
		r->cookiemap[KEY_SESSCOOKIE]->parsed.i));
}

static int
kpairbad(struct kreq *r, enum key key)
{

	return(NULL == r->fieldmap[key] || NULL != r->fieldnmap[key]);
}

static int
admin_valid(struct kreq *r)
{

	if (kpairbad(r, KEY_EMAIL) || kpairbad(r, KEY_PASSWORD))
		return(0);

	return(db_admin_valid(r->fieldmap[KEY_EMAIL]->parsed.s,
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
senddochangemail(struct kreq *r)
{

	if (kpairbad(r, KEY_EMAIL1) ||
		kpairbad(r, KEY_EMAIL2) ||
		kpairbad(r, KEY_EMAIL3))
		http_open(r, KHTTP_400);
	else 
		http_open(r, KHTTP_200);

	khttp_body(r);
}

static void
senddochangepass(struct kreq *r)
{

	if (kpairbad(r, KEY_PASSWORD1) ||
		kpairbad(r, KEY_PASSWORD2) ||
		kpairbad(r, KEY_PASSWORD3))
		http_open(r, KHTTP_400);
	else
		http_open(r, KHTTP_200);

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

	if (NULL == game) 
		http_open(r, KHTTP_400);
	else
		http_open(r, KHTTP_200);

	khttp_body(r);
	db_game_free(game);
}

static void
senddoaddplayers(struct kreq *r)
{

	http_open(r, KHTTP_200);
	khttp_body(r);
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
senddoloadgames(struct kreq *r)
{

	http_open(r, KHTTP_200);
	khttp_body(r);
	khttp_putc(r, '[');
	db_game_load_all(senddoloadgame, r);
	khttp_puts(r, "]\n");
}

static void
senddologin(struct kreq *r)
{
	struct sess	*sess;

	if (admin_valid(r)) {
		sess = db_sess_alloc();
		http_open(r, KHTTP_200);
		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRId64 "; path=/; expires=", 
			keys[KEY_SESSCOOKIE].name, sess->cookie);
		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRId64 "; path=/; expires=", 
			keys[KEY_SESSID].name, sess->id);
		db_sess_free(sess);
	} else
		http_open(r, KHTTP_400);

	khttp_body(r);
}

static void
senddologout(struct kreq *r)
{

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
	
	if ( ! khttp_parse(&r, keys, KEY__MAX, 
			pages, PAGE__MAX, PAGE_INDEX))
		return(EXIT_FAILURE);

	switch (r.page) {
	case (PAGE_INDEX):
		send303(&r, sess_valid(&r) ? PAGE_HOME : PAGE_LOGIN, 1);
		break;
	case (PAGE_HOME):
		if ( ! sess_valid(&r)) 
			send303(&r, PAGE_LOGIN, 1);
		else
			sendcontent(&r, CNTT_HTML_HOME);
		break;
	case (PAGE_DOADDGAME):
		senddoaddgame(&r);
		break;
	case (PAGE_DOADDPLAYERS):
		senddoaddplayers(&r);
		break;
	case (PAGE_DOCHANGEMAIL):
		senddochangemail(&r);
		break;
	case (PAGE_DOCHANGEPASS):
		senddochangepass(&r);
		break;
	case (PAGE_DOLOADGAMES):
		senddoloadgames(&r);
		break;
	case (PAGE_DOLOGIN):
		senddologin(&r);
		break;
	case (PAGE_DOLOGOUT):
		senddologout(&r);
		break;
	case (PAGE_LOGIN):
		sendcontent(&r, CNTT_HTML_LOGIN);
		break;
	case (PAGE_STYLE):
		sendcontent(&r, CNTT_CSS_STYLE);
		break;
	default:
		/* Page not found... */
		khttp_head(&r, "Status", "%s", khttps[KHTTP_404]);
		khttp_head(&r, "Content-Type", "%s", kmimetypes[r.mime]);
		khttp_body(&r);
		break;
	}

	khttp_free(&r);
	return(EXIT_SUCCESS);
}

