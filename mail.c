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
};

enum	mailkey {
	MAILKEY_FROM, /* "from" address */
	MAILKEY_TO, /* "to" address */
	MAILKEY_DATE, /* date (GMT) */
	MAILKEY_PASS, /* password (or NULL) */
	MAILKEY_LOGIN, /* login URL (or NULL) */
	MAILKEY_BACKUP,
	MAILKEY_LABURL,
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

static const char cb64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz0123456789+/";

static void
encodeblock(unsigned char *in, unsigned char *out, int len)
{

	out[0] = (unsigned char) cb64[ (int)(in[0] >> 2) ];
	out[1] = (unsigned char) cb64[ (int)(((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)) ];
	out[2] = (unsigned char) (len > 1 ? cb64[ (int)(((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)) ] : '=');
	out[3] = (unsigned char) (len > 2 ? cb64[ (int)(in[2] & 0x3f) ] : '=');
}

static int 
encode(FILE *infile, int linesize, struct mail *m)
{
	unsigned char in[3];
	unsigned char out[4];
	int i, len, blocksout = 0;
	int retcode = 0;

	*in = (unsigned char) 0;
	*out = (unsigned char) 0;

	while( feof( infile ) == 0 ) {
		len = 0;
		for( i = 0; i < 3; i++ ) {
			in[i] = (unsigned char) getc( infile );

			if( feof( infile ) == 0 ) {
				len++;
			}
			else {
				in[i] = (unsigned char) 0;
			}
		}
		if( len > 0 ) {
			encodeblock( in, out, len );
			for( i = 0; i < 4; i++ ) {
				mail_write( (char *)&out[i], 1, m);
			}
			blocksout++;
		}
		if( blocksout >= (linesize/4) || feof( infile ) != 0 ) {
			if( blocksout > 0 ) {
				mail_write( "\r\n", 2, m);
			}
			blocksout = 0;
		}
	}
	return( retcode );
}

static void
mail_putfile(const char *s, struct mail *m)
{
	FILE	*f;

	if (NULL == (f = fopen(s, "r"))) {
		perror(s);
		return;
	}

	encode(f, 72, m);

	fclose(f);
}

static int
mail_template_buf(size_t key, void *arg)
{
	struct mail	*mail = arg;

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
		fprintf(stderr, "mail: no smtp configured\n");
		return(NULL);
	} else if (NULL == (curl = curl_easy_init())) {
		perror(NULL);
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

/*
 * Mail new players a welcome message.
 * This will loop through all players with PSTATE_NEW and mail them
 * what's found in "addplayers.eml".
 * The state of these users is either PSTATE_MAILED or PSTATE_ERROR,
 * depending on the conditions.
 */
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

	if (NULL == (curl = mail_init(&m, &t)))
		return;

	m.uri = uri;

	while (NULL != (m.to = db_player_next_new(&id, &m.pass))) {
		assert(NULL != m.pass);
		encto = kutil_urlencode(m.to);
		encpass = kutil_urlencode(m.pass);
		kasprintf(&m.login, "%s?ident=%s&password=%s", 
			loginuri, encto, encpass);
		free(encto);
		free(encpass);

		rc = khttp_templatex(&t, DATADIR 
			"/addplayer.eml", mail_write, &m);
		if (0 == rc) {
			perror("khttp_templatex");
			break;
		}

		recpts = curl_slist_append(NULL, m.to);
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recpts);
		curl_easy_setopt(curl, CURLOPT_READDATA, &m.b);

		if (CURLE_OK != (res = curl_easy_perform(curl))) {
			fprintf(stderr, "%s: mail error: %s\n", 
				m.to, curl_easy_strerror(res));
			db_player_set_state(id, PSTATE_ERROR);
		} else
			db_player_set_mailed(id, m.pass);

		mail_clear(&m, &recpts);
	}

	mail_free(&m, curl, recpts);
}

/*
 * Send a test e-mail to the administrative e-mail address.
 * This does not set any error conditions or anything.
 */
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
		"/test.eml", mail_write, &m);

	if ( ! rc) {
		perror("khttp_templatex");
		goto out;
	}

	recpts = curl_slist_append(NULL, m.to);
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recpts);
	curl_easy_setopt(curl, CURLOPT_READDATA, &m.b);

	if (CURLE_OK == (res = curl_easy_perform(curl)))
		fprintf(stderr, "%s: mail test\n", m.to);
	else
		fprintf(stderr, "%s: mail error: %s\n", 
			m.to, curl_easy_strerror(res));

out:
	mail_free(&m, curl, recpts);
}

static void 
mail_backupfail(void)
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
		"/backupfail.eml", mail_write, &m);

	if ( ! rc) {
		perror("khttp_templatex");
		goto out;
	}

	recpts = curl_slist_append(NULL, m.to);
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recpts);
	curl_easy_setopt(curl, CURLOPT_READDATA, &m.b);

	if (CURLE_OK == (res = curl_easy_perform(curl)))
		fprintf(stderr, "%s: mail backpufail\n", m.to);
	else
		fprintf(stderr, "%s: mail backpufail: %s\n", 
			m.to, curl_easy_strerror(res));

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
		mail_backupfail();
		return;
	} 

	if (NULL == (curl = mail_init(&m, &t))) {
		remove(fname);
		return;
	}

	m.fname = fname;
	m.to = db_admin_get_mail();
	rc = khttp_templatex(&t, DATADIR 
		"/backupsuccess.eml", mail_write, &m);

	if ( ! rc) {
		perror("khttp_templatex");
		remove(fname);
		goto out;
	}

	recpts = curl_slist_append(NULL, m.to);
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recpts);
	curl_easy_setopt(curl, CURLOPT_READDATA, &m.b);

	if (CURLE_OK == (res = curl_easy_perform(curl)))
		fprintf(stderr, "%s: mail backup\n", m.to);
	else
		fprintf(stderr, "%s: mail backup: %s\n", 
			m.to, curl_easy_strerror(res));

	chmod(fname, 0);
out:
	mail_free(&m, curl, recpts);
}

void
mail_wipe(int backup)
{

	if (backup)
		mail_backup();
	db_expr_wipe();
}
