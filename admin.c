/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include <sys/types.h>

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <kcgi.h>

#include "extern.h"

enum	page {
	PAGE_DOADDGAME,
	PAGE_DOADDPLAYERS,
	PAGE_DOCHANGEMAIL,
	PAGE_DOCHANGEPASS,
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
	KEY_PASSWORD,
	KEY_PASSWORD1,
	KEY_PASSWORD2,
	KEY_PASSWORD3,
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
	{ kvalid_stringne, "password" }, /* KEY_PASSWORD */
	{ kvalid_stringne, "password1" }, /* KEY_PASSWORD1 */
	{ kvalid_stringne, "password2" }, /* KEY_PASSWORD2 */
	{ kvalid_stringne, "password3" }, /* KEY_PASSWORD3 */
	{ kvalid_stringne, "players" }, /* KEY_PLAYERS */
	{ kvalid_int, "sesscookie" }, /* KEY_SESSCOOKIE */
	{ kvalid_int, "sessid" }, /* KEY_SESSID */
};

static void
send303(struct kreq *r, enum page dest, int dostatus)
{
	char	*page, *full;

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_303]);
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r->mime]);
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

static int
kpairbad(struct kreq *r, enum key key)
{

	return(NULL == r->fieldmap[key] || NULL != r->fieldnmap[key]);
}

static void
sendcontent(struct kreq *r, enum cntt cntt)
{
	struct ktemplate t;

	t.key = templs;
	t.keysz = TEMPL__MAX;
	t.arg = r;
	t.cb = sendtempl;

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r->mime]);
	khttp_body(r);
	khttp_template(r, &t, cntts[cntt]);
}

static void
senddochangemail(struct kreq *r)
{

	if (kpairbad(r, KEY_EMAIL1) ||
		kpairbad(r, KEY_EMAIL2) ||
		kpairbad(r, KEY_EMAIL3)) {
		khttp_head(r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_400]);
		khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[r->mime]);
	} else {
		khttp_head(r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[r->mime]);
	}
	khttp_body(r);
}

static void
senddochangepass(struct kreq *r)
{

	if (kpairbad(r, KEY_PASSWORD1) ||
		kpairbad(r, KEY_PASSWORD2) ||
		kpairbad(r, KEY_PASSWORD3)) {
		khttp_head(r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_400]);
		khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[r->mime]);
	} else {
		khttp_head(r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[r->mime]);
	}
	khttp_body(r);
}

static void
senddoaddgame(struct kreq *r)
{
}

static void
senddoaddplayers(struct kreq *r)
{

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r->mime]);
	khttp_body(r);
}

static void
senddologin(struct kreq *r)
{

	if (kpairbad(r, KEY_EMAIL) || kpairbad(r, KEY_PASSWORD)) {
		khttp_head(r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_400]);
		khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[r->mime]);
	} else {
		khttp_head(r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[r->mime]);
		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRIu32 "; path=/; expires=", 
			keys[KEY_SESSCOOKIE].name, arc4random());
		khttp_head(r, kresps[KRESP_SET_COOKIE],
			"%s=%" PRIu32 "; path=/; expires=", 
			keys[KEY_SESSID].name, arc4random());
	}
	khttp_body(r);
}

static void
senddologout(struct kreq *r)
{

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_303]);
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r->mime]);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSCOOKIE].name);
	khttp_head(r, kresps[KRESP_SET_COOKIE],
		"%s=; path=/; expires=", 
		keys[KEY_SESSID].name);
	send303(r, PAGE_LOGIN, 0);
}

static int
sessvalid(struct kreq *r)
{

	if (NULL == r->cookiemap[KEY_SESSID] ||
		NULL == r->cookiemap[KEY_SESSCOOKIE]) 
		return(0);

	return(1);
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
		send303(&r, sessvalid(&r) ? PAGE_HOME : PAGE_LOGIN, 1);
		break;
	case (PAGE_HOME):
		if ( ! sessvalid(&r)) 
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

