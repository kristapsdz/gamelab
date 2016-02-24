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
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <kcgi.h>
#include <kcgijson.h>
#include <gmp.h>
#include <curl/curl.h>

#include "extern.h"

/*
 * This is the buffer created when templating a mail message.
 * We use it to read in and write out the buffer, hence having both "sz"
 * and "maxsz" (reading) and "cur" (writing).
 */
struct 	buf {
	char		*buf;
	size_t		 cur;
	size_t		 sz;
	size_t		 maxsz;
};

/*
 * This consists of data that we're going to template into the mail
 * message.
 * If any of the fields are NULL, they will be templated with an empty
 * string.
 * All non-const fields are free'd in mail_free().
 */
struct	mail {
	char		*to; /* recipient (not NULL) */
	char		*from; /* from (smtp-from) (not NULL) */
	char		 date[27]; /* current date string */
	char		*pass; /* new-user password */
	char		*login; /* new-user login url */
	const char	*uri; /* full login url (or NULL) */
	char		*fname; /* backup fname (or NULL) */
	struct buf	 b; /* buffer to read/write */
	int64_t		 round; /* current round */
	int64_t		 minutes; /* round time in minutes */
	int64_t		 minminutes; /* round time in minutes */
};

enum	mailkey {
	MAILKEY_FROM, /* "from" address */
	MAILKEY_TO, /* "to" address */
	MAILKEY_DATE, /* date (GMT) */
	MAILKEY_PASS, /* password (or NULL) */
	MAILKEY_LOGIN, /* login URL (or NULL) */
	MAILKEY_BACKUP, /* backup file (or empty) */
	MAILKEY_LABURL, /* URL of user lab */
	MAILKEY_ROUND, /* current round */
	MAILKEY_ROUNDMINTIME_HOURS, /* round min time */
	MAILKEY_ROUNDMINTIME_MINUTES, /* round min time */
	MAILKEY_ROUNDTIME_HOURS, /* round time */
	MAILKEY_ROUNDTIME_MINUTES, /* round time */
	MAILKEY__MAX
};

static	const char *const mailkeys[MAILKEY__MAX] = {
	"from", /* MAILKEY_FROM */
	"to", /* MAILKEY_TO */
	"date", /* MAILKEY_DATE */
	"pass", /* MAILKEY_PASS */
	"login", /* MAILKEY_LOGIN */
	"backup", /* MAILKEY_LOGIN */
	"uri", /* MAILKEY_LABURL */
	"round", /* MAILKEY_ROUND */
	"roundmintime-hours", /* MAILKEY_ROUNDMINTIME_HOURS */
	"roundmintime-minutes", /* MAILKEY_ROUNDMINTIME_MINUTES */
	"roundtime-hours", /* MAILKEY_ROUNDTIME_HOURS */
	"roundtime-minutes", /* MAILKEY_ROUNDTIME_MINUTES */
};

/*
 * Invoked by libcurl to actually serialise the message from the buffer
 * to the network.
 * This follows from their shoddy example.
 */
static size_t 
dobuf(void *ptr, size_t sz, size_t nm, void *arg)
{
	struct buf	*buf = arg;
	size_t		 rsz;

	/* Obviously, we don't care about zero-length. */
	if (0 == sz || 0 == nm || buf->cur == buf->sz) 
		return(0);

	/* Check for multiplication overflow. */
	if (SIZE_MAX / nm < sz) 
		return(0);

	rsz = sz * nm;

	if (rsz > buf->sz - buf->cur)
		rsz = buf->sz - buf->cur;

	memcpy(ptr, buf->buf + buf->cur, rsz);
	buf->cur += rsz;
	return(rsz);
}

/*
 * Template-driven call to put data into the mail message buffer.
 * We make sure to nil-terminate this buffer all of the time because
 * libcurl will work on a nil-terminated buffer.
 */
static int
mail_write(const char *s, size_t sz, void *arg)
{
	struct mail	*m = arg;
	struct buf	*b = &m->b;

	if (NULL == s)
		return(1);

	if (b->sz + sz + 1 > b->maxsz) {
		b->maxsz = b->sz + sz + 1 + 1024;
		b->buf = krealloc(b->buf, b->maxsz);
	}

	memcpy(&b->buf[b->sz], s, sz);
	b->sz += sz;
	b->buf[b->sz] = '\0';
	return(1);
}

/*
 * Completely free up the mail context.
 * Nothing should remain after this call.
 */
static void
mail_free(struct mail *mail, CURL *c, struct curl_slist *r)
{

	free(mail->b.buf);
	free(mail->from);
	free(mail->to);
	free(mail->pass);
	free(mail->login);
	curl_slist_free_all(r);
	curl_easy_cleanup(c);
	curl_global_cleanup();
}

/*
 * Clear the mail context for the next invocation.
 * This should only clear the necessary bits.
 */
static void
mail_clear(struct mail *mail, struct curl_slist **rp)
{

	free(mail->to);
	free(mail->pass);
	free(mail->login);
	mail->b.sz = mail->b.cur = 0;
	mail->to = mail->pass = mail->login = NULL;
	curl_slist_free_all(*rp);
	*rp = NULL;
}

/*
 * String wrapper for mail_write().
 */
static void
mail_puts(const char *s, struct mail *m)
{

	mail_write(s, strlen(s), m);
}

static void
mail_putfile(const char *s, struct mail *m)
{
	FILE	*f;

	if (NULL == (f = fopen(s, "r"))) {
		WARN("fopen: s");
		return;
	}

	base64file(f, 72, mail_write, m);
	fclose(f);
}

static int
mail_template_buf(size_t key, void *arg)
{
	struct mail	*mail = arg;
	char		 buf[22];

	switch (key) {
	case (MAILKEY_FROM):
		assert(NULL != mail->from);
		mail_puts(mail->from, mail);
		break;
	case (MAILKEY_TO):
		assert(NULL != mail->to);
		mail_puts(mail->to, mail);
		break;
	case (MAILKEY_DATE):
		mail_puts(mail->date, mail);
		break;
	case (MAILKEY_PASS):
		mail_puts(mail->pass, mail);
		break;
	case (MAILKEY_LOGIN):
		mail_puts(mail->login, mail);
		break;
	case (MAILKEY_BACKUP):
		assert(NULL != mail->fname);
		mail_putfile(mail->fname, mail);
		break;
	case (MAILKEY_LABURL):
		assert(NULL != mail->uri);
		mail_puts(mail->uri, mail);
		break;
	case (MAILKEY_ROUND):
		snprintf(buf, sizeof(buf), "%" PRId64, mail->round);
		mail_puts(buf, mail);
		break;
	case (MAILKEY_ROUNDMINTIME_HOURS):
		snprintf(buf, sizeof(buf), "%g", mail->minminutes / 60.0);
		mail_puts(buf, mail);
		break;
	case (MAILKEY_ROUNDMINTIME_MINUTES):
		snprintf(buf, sizeof(buf), "%" PRId64, mail->minminutes);
		mail_puts(buf, mail);
		break;
	case (MAILKEY_ROUNDTIME_HOURS):
		snprintf(buf, sizeof(buf), "%g", mail->minutes / 60.0);
		mail_puts(buf, mail);
		break;
	case (MAILKEY_ROUNDTIME_MINUTES):
		snprintf(buf, sizeof(buf), "%" PRId64, mail->minutes);
		mail_puts(buf, mail);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(1);
}

/*
 * Initialise the CURL handle and all of the entries in the "mail"
 * structure that are set from the SMTP data in the database.
 * This can return NULL on failure, e.g., we haven't set any information
 * in our database yet.
 */
static CURL *
mail_init(struct mail *mail, struct ktemplate *t)
{
	struct smtp	*smtp;
	CURL		*curl;
	time_t		 tt;

	if (NULL == (smtp = db_smtp_get())) {
		INFO("Mail: no SMTP configured");
		return(NULL);
	} else if (NULL == (curl = curl_easy_init())) {
		WARNX("curl_easy_init");
		db_smtp_free(smtp);
		return(NULL);
	}

	memset(mail, 0, sizeof(struct mail));

	/* Initialise sending time. */
	tt = time(NULL);
	asctime_r(gmtime(&tt), mail->date);
	mail->date[strlen(mail->date) - 1] = '\0';
	/* Initialise from address. */
	mail->from = kstrdup(smtp->from);

	/* Initialise CURL object. */
	curl_easy_setopt(curl, CURLOPT_USERNAME, smtp->user);
	curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp->pass);
	curl_easy_setopt(curl, CURLOPT_URL, smtp->server);
	curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, smtp->from);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, dobuf);

	db_smtp_free(smtp);

	/* Initialise the template system itself. */
	memset(t, 0, sizeof(struct ktemplate));
	t->key = mailkeys;
	t->keysz = MAILKEY__MAX;
	t->arg = mail;
	t->cb = mail_template_buf;
	return(curl);
}

void 
mail_players(const char *uri, const char *loginuri)
{
	CURL		  *curl;
	CURLcode 	   res;
	struct curl_slist *recpts = NULL;
	struct mail	   m;
	int64_t		   id;
	char		  *encto, *encpass;
	struct ktemplate   t;
	int		   rc;

	curl = mail_init(&m, &t);
	/* Can be NULL: we want to set the statuses. */
	m.uri = uri;

	while (NULL != (m.to = db_player_next_new(&id, &m.pass))) {
		if (NULL == curl) {
			db_player_set_mailed(id, m.pass);
			free(m.to);
			free(m.pass);
			continue;
		}

		assert(NULL != m.pass);
		encto = kutil_urlencode(m.to);
		encpass = kutil_urlencode(m.pass);
		kasprintf(&m.login, "%s?ident=%s&password=%s", 
			loginuri, encto, encpass);
		free(encto);
		free(encpass);

		rc = khttp_templatex(&t, DATADIR 
			"/mail-addplayer.eml", mail_write, &m);
		if (0 == rc) {
			WARNX("khttp_templatex");
			break;
		}

		recpts = curl_slist_append(NULL, m.to);
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recpts);
		curl_easy_setopt(curl, CURLOPT_READDATA, &m.b);

		if (CURLE_OK != (res = curl_easy_perform(curl))) {
			WARNX("Mail error: %s", curl_easy_strerror(res));
			db_player_set_state(id, PSTATE_ERROR);
		} else
			db_player_set_mailed(id, m.pass);

		mail_clear(&m, &recpts);
	}

	if (NULL != curl)
		mail_free(&m, curl, recpts);
}

void 
mail_test(void)
{
	CURL		  *curl;
	CURLcode 	   res;
	struct curl_slist *recpts = NULL;
	struct mail	   m;
	struct ktemplate   t;
	int		   rc;

	if (NULL == (curl = mail_init(&m, &t)))
		return;

	m.to = db_admin_get_mail();
	rc = khttp_templatex(&t, DATADIR 
		"/mail-test.eml", mail_write, &m);

	if (0 == rc) {
		WARNX("khttp_templatex");
		goto out;
	}

	recpts = curl_slist_append(NULL, m.to);
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recpts);
	curl_easy_setopt(curl, CURLOPT_READDATA, &m.b);

	if (CURLE_OK == (res = curl_easy_perform(curl)))
		goto out;
	WARNX("Mail error: %s", curl_easy_strerror(res));
out:
	mail_free(&m, curl, recpts);
}

static void 
mail_backupfail(const char *fname)
{
	CURL		  *curl;
	CURLcode 	   res;
	struct curl_slist *recpts = NULL;
	struct mail	   m;
	struct ktemplate   t;
	int		   rc;

	if (NULL == (curl = mail_init(&m, &t)))
		return;

	m.to = db_admin_get_mail();
	rc = khttp_templatex(&t, DATADIR 
		"/mail-backupfail.eml", mail_write, &m);

	if (0 == rc) {
		WARNX("khttp_templatex");
		goto out;
	}

	recpts = curl_slist_append(NULL, m.to);
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recpts);
	curl_easy_setopt(curl, CURLOPT_READDATA, &m.b);

	if (CURLE_OK == (res = curl_easy_perform(curl)))
		goto out;
	WARNX("Mail error: %s", curl_easy_strerror(res));
out:
	mail_free(&m, curl, recpts);
}

void
mail_backup(void)
{
	CURL		  *curl;
	CURLcode 	   res;
	struct curl_slist *recpts = NULL;
	struct mail	   m;
	struct ktemplate   t;
	int		   rc;
	char		   fname[PATH_MAX], date[27];
	char	   	  *cp;
	time_t		   tt;

	tt = time(NULL);
	asctime_r(gmtime(&tt), date);
	date[strlen(date) - 1] = '\0';
	for (cp = date; '\0' != *cp; cp++) {
		if (isspace(*cp) || ',' == *cp)
			*cp = '_';
		else if (*cp == ':')
			*cp = '-';
	}

	(void)snprintf(fname, sizeof(fname), 
		"%s/backup-%s.db", DATADIR, date);

	if ( ! db_backup(fname)) {
		mail_backupfail(fname);
		return;
	} else if (NULL == (curl = mail_init(&m, &t))) {
		chmod(fname, 0);
		return;
	}

	m.fname = fname;
	m.to = db_admin_get_mail();
	rc = khttp_templatex(&t, DATADIR 
		"/mail-backupsuccess.eml", mail_write, &m);

	if (0 == rc) {
		WARNX("khttp_templatex");
		goto out;
	}

	recpts = curl_slist_append(NULL, m.to);
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recpts);
	curl_easy_setopt(curl, CURLOPT_READDATA, &m.b);

	if (CURLE_OK == (res = curl_easy_perform(curl)))
		goto out;
	WARNX("Mail error: %s", curl_easy_strerror(res));
out:
	if (-1 == chmod(fname, 0))
		WARN("chmod: %s", fname);
	mail_free(&m, curl, recpts);
}

void
mail_wipe(int backup)
{

	if (backup)
		mail_backup();
	db_expr_wipe();
}

static void
mail_appendplayer(const struct player *p, void *arg)
{
	struct curl_slist **recpts = arg;

	if (p->autoadd)
		return;
	*recpts = curl_slist_append(*recpts, p->mail);
}

void 
mail_roundadvance(const char *uri, int64_t last, int64_t round)
{
	CURL		  *curl;
	CURLcode 	   res;
	struct curl_slist *recpts = NULL;
	struct mail	   m;
	struct ktemplate   t;
	int		   rc;
	struct expr	  *expr;

	if (NULL == (curl = mail_init(&m, &t))) 
		return;
	if (NULL == (expr = db_expr_get(1)))
		goto out;

	m.round = expr->round + 1;
	m.minutes = expr->minutes;
	m.minminutes = expr->roundmin;
	m.uri = uri;

	if (-1 == last)
		db_player_load_all(mail_appendplayer, &recpts);
	else
		db_player_load_playing(expr, 
			mail_appendplayer, &recpts);

	rc = khttp_templatex(&t, -1 == last ?
		DATADIR "/mail-roundfirst.eml" : 
		DATADIR "/mail-roundadvance.eml",
		mail_write, &m);

	if (0 == rc) {
		WARNX("khttp_templatex");
		goto out;
	}

	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recpts);
	curl_easy_setopt(curl, CURLOPT_READDATA, &m.b);

	if (CURLE_OK == (res = curl_easy_perform(curl)))
		goto out;
	WARNX("Mail error: %s", curl_easy_strerror(res));
out:
	mail_free(&m, curl, recpts);
	db_expr_free(expr);
}
