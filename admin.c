/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
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
	PAGE_DOADDGAME,
	PAGE_DOADDPLAYERS,
	PAGE_DOCHANGEMAIL,
	PAGE_DOCHANGEPASS,
	PAGE_DOCHANGESMTP,
	PAGE_DODISABLEPLAYER,
	PAGE_DOENABLEPLAYER,
	PAGE_DOLOADGAMES,
	PAGE_DOLOADPLAYERS,
	PAGE_DOLOGIN,
	PAGE_DOLOGOUT,
	PAGE_DOSTARTEXPR,
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
	CNTT_HTML_HOME_NEW,
	CNTT_HTML_HOME_STARTED,
	CNTT_HTML_LOGIN,
	CNTT__MAX
};

/*
 * Input form field names.
 */
enum	key {
	KEY_DATE,
	KEY_DAYS,
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
	KEY_PLAYERID,
	KEY_PLAYERS,
	KEY_SERVER,
	KEY_SESSCOOKIE,
	KEY_SESSID,
	KEY_USER,
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
	"doaddgame", /* PAGE_DOADDGAME */
	"doaddplayers", /* PAGE_DOADDPLAYERS */
	"dochangemail", /* PAGE_DOCHANGEMAIL */
	"dochangepass", /* PAGE_DOCHANGEPASS */
	"dochangesmtp", /* PAGE_DOCHANGESMTP */
	"dodisableplayer", /* PAGE_DODISABLEPLAYER */
	"doenableplayer", /* PAGE_DOENABLEPLAYER */
	"doloadgames", /* PAGE_DOLOADGAMES */
	"doloadplayers", /* PAGE_DOLOADPLAYERS */
	"dologin", /* PAGE_DOLOGIN */
	"dologout", /* PAGE_DOLOGOUT */
	"dostartexpr", /* PAGE_DOSTARTEXPR */
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
	"adminhome-new.html", /* CNTT_HTML_HOME_NEW */
	"adminhome-started.html", /* CNTT_HTML_HOME_STARTED */
	"adminlogin.html", /* CNTT_HTML_LOGIN */
};

static int kvalid_days(struct kpair *);
static int kvalid_futuredate(struct kpair *);

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_futuredate, "date" }, /* KEY_DATE */
	{ kvalid_days, "days" }, /* KEY_DAYS */
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
	{ kvalid_int, "pid" }, /* KEY_PLAYERID */
	{ kvalid_stringne, "players" }, /* KEY_PLAYERS */
	{ kvalid_stringne, "server" }, /* KEY_SERVER */
	{ kvalid_int, "sesscookie" }, /* KEY_SESSCOOKIE */
	{ kvalid_int, "sessid" }, /* KEY_SESSID */
	{ kvalid_stringne, "user" }, /* KEY_USER */
};

/*
 * Put a quoted JSON string into the output stream.
 */
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

/*
 * Put a quoted JSON string key and integral value pair.
 */
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

/*
 * Put a quoted JSON string key and string value pair.
 */
static void
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
static void
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

static int
sess_valid(struct kreq *r)
{

	if (NULL == r->cookiemap[KEY_SESSID] ||
		NULL == r->cookiemap[KEY_SESSCOOKIE]) 
		return(0);

	return(db_admin_sess_valid(r->cookiemap[KEY_SESSID]->parsed.i,
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

	if (kpairbad(r, KEY_EMAIL1) ||
		kpairbad(r, KEY_EMAIL2) ||
		kpairbad(r, KEY_EMAIL3) ||
		strcmp(r->fieldmap[KEY_EMAIL2]->parsed.s,
		       r->fieldmap[KEY_EMAIL3]->parsed.s) ||
		! db_admin_valid
			(r->fieldmap[KEY_EMAIL1]->parsed.s, NULL)) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	}

	db_sess_delete(r->cookiemap[KEY_SESSID]->parsed.i);
	db_admin_set_mail(r->fieldmap[KEY_EMAIL2]->parsed.s);
	http_open(r, KHTTP_200);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSCOOKIE].name);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSID].name);
	khttp_body(r);
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

	db_sess_delete(r->cookiemap[KEY_SESSID]->parsed.i);
	db_admin_set_pass(r->fieldmap[KEY_PASSWORD2]->parsed.s);
	http_open(r, KHTTP_200);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSCOOKIE].name);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSID].name);
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
senddoaddplayers(struct kreq *r)
{
	char	*tok, *buf, *mail, *sv;
	int	 first;

	http_open(r, KHTTP_200);
	khttp_body(r);
	khttp_putc(r, '[');

	if (NULL == r->fieldmap[KEY_PLAYERS]) {
		khttp_putc(r, ']');
		return;
	}

	buf = sv = kstrdup(r->fieldmap[KEY_PLAYERS]->parsed.s);
	first = 1;
	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		if (*tok == '\0')
			continue;
		if (NULL == (mail = valid_email(tok))) {
			fprintf(stderr, "%s: invalid e-mail\n", tok);
			continue;
		}
		if ( ! first)
			khttp_putc(r, ',');
		json_puts(r, mail);
		db_player_create(mail);
		first = 0;
	}
	free(sv);
	khttp_putc(r, ']');
}

static void
senddoloadplayer(const struct player *player, size_t count, void *arg)
{
	struct kreq	*r = arg;

	if (count > 0)
		khttp_putc(r, ',');
	khttp_putc(r, '{');
	json_putstring(r, "mail", player->mail);
	khttp_putc(r, ',');
	json_putint(r, "id", player->id);
	khttp_putc(r, ',');
	json_putint(r, "enabled", player->enabled);
	khttp_putc(r, '}');
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
senddoloadplayers(struct kreq *r)
{

	http_open(r, KHTTP_200);
	khttp_body(r);
	khttp_putc(r, '[');
	db_player_load_all(senddoloadplayer, r);
	khttp_puts(r, "]\n");
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
		sess = db_admin_sess_alloc();
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
senddostartexpr(struct kreq *r)
{
	pid_t	 pid;

	if (kpairbad(r, KEY_DATE) ||
		kpairbad(r, KEY_DAYS) ||
		db_player_count_all() < 2 ||
		db_game_count_all() < 1) {
		http_open(r, KHTTP_400);
		khttp_body(r);
		return;
	} 

	db_expr_start
		(r->fieldmap[KEY_DATE]->parsed.i,
		 r->fieldmap[KEY_DAYS]->parsed.i);
	http_open(r, KHTTP_200);
	khttp_body(r);

	db_close();
	if (-1 == (pid = fork())) {
		fprintf(stderr, "cannot fork!\n");
	} else if (0 == pid) {
		khttp_child_free(r);
		if (0 == (pid = fork())) {
			mail_players();
			db_close();
		} else if (pid < 0) 
			fprintf(stderr, "cannot double-fork!\n");
		_exit(EXIT_SUCCESS);
	} else 
		waitpid(pid, NULL, 0);
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

static int
kvalid_futuredate(struct kpair *kp)
{

	if ( ! kvalid_date(kp))
		return(0);
	return(kp->parsed.i > time(NULL));
}


static int
kvalid_days(struct kpair *kp)
{

	if ( ! kvalid_uint(kp))
		return(0);
	return(kp->parsed.i > 0);
}

int
main(void)
{
	struct kreq	 r;
	
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
	if ( ! sess_valid(&r)) {
		send303(&r, PAGE_LOGIN, 1);
		khttp_free(&r);
		return(EXIT_SUCCESS);
	}

	switch (r.page) {
	case (PAGE_DOADDGAME):
	case (PAGE_DOADDPLAYERS):
	case (PAGE_DODISABLEPLAYER):
	case (PAGE_DOENABLEPLAYER):
	case (PAGE_DOSTARTEXPR):
		if (db_expr_checkstate(ESTATE_NEW))
			break;
		fprintf(stderr, "ignoring request: "
			"experiment already started\n");
		http_open(&r, KHTTP_409);
		khttp_body(&r);
		khttp_free(&r);
		return(EXIT_SUCCESS);
	default:
		break;
	}

	switch (r.page) {
	case (PAGE_INDEX):
		send303(&r, PAGE_HOME, 1);
		break;
	case (PAGE_HOME):
		if ( ! db_expr_checkstate(ESTATE_NEW))
			sendcontent(&r, CNTT_HTML_HOME_STARTED);
		else
			sendcontent(&r, CNTT_HTML_HOME_NEW);
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
	case (PAGE_DOCHANGESMTP):
		senddochangesmtp(&r);
		break;
	case (PAGE_DODISABLEPLAYER):
		senddodisableplayer(&r);
		break;
	case (PAGE_DOENABLEPLAYER):
		senddoenableplayer(&r);
		break;
	case (PAGE_DOLOADGAMES):
		senddoloadgames(&r);
		break;
	case (PAGE_DOLOADPLAYERS):
		senddoloadplayers(&r);
		break;
	case (PAGE_DOLOGOUT):
		senddologout(&r);
		break;
	case (PAGE_DOSTARTEXPR):
		senddostartexpr(&r);
		break;
	default:
		http_open(&r, KHTTP_404);
		khttp_body(&r);
		break;
	}

	khttp_free(&r);
	return(EXIT_SUCCESS);
}

