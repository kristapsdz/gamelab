/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <kcgi.h>
#include <kcgijson.h>
#include <gmp.h>
#include <curl/curl.h>

#include "extern.h"

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
};

/*
 * This is the buffer created when templating a mail message.
 * It's intended to be run multiple times, hence that the allocated
 * buffer is saved and re-used in subsequent runs.
 */
struct 	buf {
	char		*buf;
	size_t		 cur;
	size_t		 sz;
	size_t		 maxsz;
};

enum	mailkey {
	MAILKEY_FROM, /* "from" address */
	MAILKEY_TO, /* "to" address */
	MAILKEY_DATE, /* date (GMT) */
	MAILKEY_PASS, /* password (or NULL) */
	MAILKEY_LOGIN, /* login URL (or NULL) */
	MAILKEY__MAX
};

static	const char *const mailkeys[MAILKEY__MAX] = {
	"from", /* MAILKEY_FROM */
	"to", /* MAILKEY_TO */
	"date", /* MAILKEY_DATE */
	"pass", /* MAILKEY_PASS */
	"login" /* MAILKEY_LOGIN */
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

	/*fprintf(stderr, "[%.*s]\n", (int)rsz, buf->buf + buf->cur);*/

	memcpy(ptr, buf->buf + buf->cur, rsz);
	buf->cur += rsz;
	return(rsz);
}

static void
buf_puts(const char *s, struct buf *b)
{
	size_t	 sz;

	if (NULL == s)
		s = "";

	sz = strlen(s);

	if (b->sz + sz + 1 > b->maxsz) {
		b->maxsz = b->sz + sz + 1 + 1024;
		b->buf = krealloc(b->buf, b->maxsz);
	}
	memcpy(&b->buf[b->sz], s, sz);
	b->sz += sz;
	b->buf[b->sz] = '\0';
}

static void
buf_putc(char c, struct buf *b)
{

	if (b->sz + 2 > b->maxsz) {
		b->maxsz = b->sz + 2 + 1024;
		b->buf = krealloc(b->buf, b->maxsz);
	}
	b->buf[b->sz++] = c;
	b->buf[b->sz] = '\0';
}

static void
mail_free(struct mail *mail)
{

	free(mail->from);
}

static void
mail_clear(struct mail *mail)
{

	free(mail->to);
	free(mail->pass);
	free(mail->login);
}

static void
mail_template_buf(const char *buf, size_t sz, 
	const struct mail *mail, struct buf *b)
{
	size_t		 i, j, len, start, end;

	for (i = 0; i < sz - 1; i++) {
		/* Look for the starting "@@" marker. */
		if ('@' != buf[i]) {
			buf_putc(buf[i], b);
			continue;
		} else if ('@' != buf[i + 1]) {
			buf_putc(buf[i], b);
			continue;
		} 

		/* Seek to find the end "@@" marker. */
		start = i + 2;
		for (end = start + 2; end < sz - 1; end++)
			if ('@' == buf[end] && '@' == buf[end + 1])
				break;

		/* Continue printing if not found of 0-length. */
		if (end == sz - 1 || end == start) {
			buf_putc(buf[i], b);
			continue;
		}

		/* Look for a matching key. */
		for (j = 0; j < MAILKEY__MAX; j++) {
			len = strlen(mailkeys[j]);
			if (len != end - start)
				continue;
			else if (memcmp(&buf[start], mailkeys[j], len))
				continue;
			switch (j) {
			case (MAILKEY_FROM):
				assert(NULL != mail->from);
				buf_puts(mail->from, b);
				break;
			case (MAILKEY_TO):
				assert(NULL != mail->to);
				buf_puts(mail->to, b);
				break;
			case (MAILKEY_DATE):
				buf_puts(mail->date, b);
				break;
			case (MAILKEY_PASS):
				buf_puts(mail->pass, b);
				break;
			case (MAILKEY_LOGIN):
				buf_puts(mail->login, b);
				break;
			default:
				abort();
			}
			break;
		}

		/* Didn't find it... */
		if (j == MAILKEY__MAX)
			buf_putc(buf[i], b);
		else
			i = end + 1;
	}

	if (i < sz)
		buf_putc(buf[i], b);
}

/*
 * Memory-map the file at "fname".
 * This is used as the read-only template source.
 * It must be a real file, obviously.
 */
static char *
mail_template_open(const char *fname, int *fd, size_t *sz)
{
	struct stat 	 st;
	char		*buf;

	if (-1 == (*fd = open(fname, O_RDONLY, 0))) {
		fprintf(stderr, "open: %s\n", fname);
		return(NULL);
	} else if (-1 == fstat(*fd, &st)) {
		fprintf(stderr, "fstat: %s\n", fname);
		close(*fd);
		return(NULL);
	} else if (st.st_size >= (1U << 31)) {
		fprintf(stderr, "size overflow: %s\n", fname);
		close(*fd);
		return(NULL);
	} else if (0 == st.st_size) {
		fprintf(stderr, "zero-length: %s\n", fname);
		close(*fd);
		return(NULL);
	}

	*sz = (size_t)st.st_size;
	buf = mmap(NULL, *sz, PROT_READ, MAP_SHARED, *fd, 0);

	if (MAP_FAILED == buf) {
		fprintf(stderr, "mmap: %s", fname);
		close(*fd);
		buf = NULL;
	}

	return(buf);
}

/*
 * Close the mail template source mmap.
 */
static void
mail_template_close(char *buf, size_t sz, int fd)
{

	if (NULL == buf)
		return;
	munmap(buf, sz);
	close(fd);
}

/*
 * Initialise the CURL handle and all of the entries in the "mail"
 * structure that are set from the SMTP data in the database.
 * This can return NULL on failure, e.g., we haven't set any information
 * in our database yet.
 */
static CURL *
mail_init(struct mail *mail)
{
	struct smtp	*smtp;
	CURL		*curl;
	time_t		 t;

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
	t = time(NULL);
	asctime_r(gmtime(&t), mail->date);
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
mail_players(const char *uri)
{
	CURL		  *curl;
	CURLcode 	   res;
	struct curl_slist *recipients = NULL;
	struct buf	   buf;
	struct mail	   mail;
	int64_t		   id;
	char		  *mbuf, *encto, *encpass;
	size_t		   mbufsz;
	int		   mbuffd;

	mbuf = mail_template_open
		(DATADIR "/addplayer.eml", &mbuffd, &mbufsz);

	if (NULL == mbuf) {
		fprintf(stderr, "addplayer.eml: invalid mail\n");
		return;
	}

	/* Initialise mail system. */
	if (NULL == (curl = mail_init(&mail))) {
		mail_template_close(mbuf, mbufsz, mbuffd);
		return;
	}

	/*
	 * Hit the database for a new recipient until there are none
	 * remaining, re-use the same CURL connection as much as we can.
	 * Each access results in a database update: to set the user as
	 * having been mailed, or that an error occured.
	 */
	memset(&buf, 0, sizeof(struct buf));
	while (NULL != (mail.to = db_player_next_new(&id, &mail.pass))) {
		assert(NULL != mail.pass);
		buf.sz = buf.cur = 0;
		recipients = NULL;

		encto = kutil_urlencode(mail.to);
		encpass = kutil_urlencode(mail.pass);
		kasprintf(&mail.login, "%s?email=%s&password=%s",
			uri, encto, encpass);
		free(encto);
		free(encpass);

		mail_template_buf(mbuf, mbufsz, &mail, &buf);
		recipients = curl_slist_append(recipients, mail.to);
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
		curl_easy_setopt(curl, CURLOPT_READDATA, &buf);

		/* Set the pstate or loop forever on PSTATE_NEW!! */
		if (CURLE_OK == (res = curl_easy_perform(curl))) {
			db_player_set_mailed(id, mail.pass);
			fprintf(stderr, "%s: mail new\n", mail.to);
		} else {
			db_player_set_state(id, PSTATE_ERROR);
			fprintf(stderr, "%s: mail error: %s\n", 
				mail.to, curl_easy_strerror(res));
		}

		curl_slist_free_all(recipients);
		mail_clear(&mail);
	}

	free(buf.buf);
	mail_template_close(mbuf, mbufsz, mbuffd);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	mail_free(&mail);
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
	struct curl_slist *recipients = NULL;
	struct buf	   buf;
	struct mail	   mail;
	char		  *mbuf;
	size_t		   mbufsz;
	int		   mbuffd;

	mbuf = mail_template_open
		(DATADIR "/test.eml", &mbuffd, &mbufsz);

	if (NULL == mbuf) {
		fprintf(stderr, "test.eml: invalid mail\n");
		return;
	}

	if (NULL == (curl = mail_init(&mail))) {
		mail_template_close(mbuf, mbufsz, mbuffd);
		return;
	}

	mail.to = db_admin_get_mail();

	memset(&buf, 0, sizeof(struct buf));
	mail_template_buf(mbuf, mbufsz, &mail, &buf);

	recipients = curl_slist_append(NULL, mail.to);
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
	curl_easy_setopt(curl, CURLOPT_READDATA, &buf);

	/* TODO: record error somewhere? */
	if (CURLE_OK == (res = curl_easy_perform(curl)))
		fprintf(stderr, "%s: mail test\n", mail.to);
	else
		fprintf(stderr, "%s: mail error: %s\n", 
			mail.to, curl_easy_strerror(res));

	curl_slist_free_all(recipients);
	free(buf.buf);
	mail_template_close(mbuf, mbufsz, mbuffd);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	mail_clear(&mail);
	mail_free(&mail);
}
