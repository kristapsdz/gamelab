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

#include "kcgi.h"

enum	page {
	PAGE_DOLOGIN,
	PAGE_HOME,
	PAGE_INDEX,
	PAGE_LOGIN,
	PAGE_LOGOUT,
	PAGE__MAX
};

enum	html {
	HTML_HOME,
	HTML_LOGIN,
	HTML__MAX
};

enum	key {
	KEY_EMAIL,
	KEY_PASSWORD,
	KEY_SESSCOOKIE,
	KEY_SESSID,
	KEY__MAX
};

static const char *const pages[PAGE__MAX] = {
	"dologin", /* PAGE_DOLOGIN */
	"home", /* PAGE_HOME */
	"index", /* PAGE_INDEX */
	"login", /* PAGE_LOGIN */
	"logout", /* PAGE_LOGOUT */
};

static const char *const htmls[HTML__MAX] = {
	"adminhome.html", /* HTML_HOME */
	"adminlogin.html", /* HTML_LOGIN */
};

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_email, "email" }, /* KEY_EMAIL */
	{ kvalid_stringne, "password" }, /* KEY_PASSWORD */
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

static void
sendhome(struct kreq *r)
{
	struct ktemplate t;

	memset(&t, 0, sizeof(struct ktemplate));

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r->mime]);
	khttp_body(r);
	khttp_template(r, &t, htmls[HTML_HOME]);
}

static void
sendlogin(struct kreq *r)
{
	struct ktemplate t;

	memset(&t, 0, sizeof(struct ktemplate));

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r->mime]);
	khttp_body(r);
	khttp_template(r, &t, htmls[HTML_LOGIN]);
}

static void
senddologin(struct kreq *r)
{

	if (NULL == r->fieldmap[KEY_EMAIL] ||
		NULL == r->fieldmap[KEY_PASSWORD] ||
		NULL != r->fieldnmap[KEY_EMAIL] ||
		NULL != r->fieldnmap[KEY_PASSWORD]) {
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
sendlogout(struct kreq *r)
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
		/* "Meta-page" that redirect us home or to login. */
		send303(&r, sessvalid(&r) ? PAGE_HOME : PAGE_LOGIN, 1);
		break;
	case (PAGE_HOME):
		/* Here we do all of our administration. */
		if ( ! sessvalid(&r)) 
			send303(&r, PAGE_LOGIN, 1);
		else
			sendhome(&r);
		break;
	case (PAGE_DOLOGIN):
		/* Accept the login form. */
		senddologin(&r);
		break;
	case (PAGE_LOGOUT):
		/* Log us out and redirect to index. */
		sendlogout(&r);
		break;
	case (PAGE_LOGIN):
		/* Post the login form. */
		sendlogin(&r);
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

