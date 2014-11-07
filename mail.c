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
#include <gmp.h>
#include <curl/curl.h>

#include "extern.h"

struct	mail {
	char		*to;
	char		*from;
	char		 date[27];
	char		*pass;
	char		*login;
};

struct 	buf {
	char		*buf;
	size_t		 cur;
	size_t		 sz;
	size_t		 maxsz;
};

enum	mailkey {
	MAILKEY_FROM,
	MAILKEY_TO,
	MAILKEY_DATE,
	MAILKEY_PASS,
	MAILKEY_LOGIN,
	MAILKEY__MAX
};

static	const char *const mailkeys[MAILKEY__MAX] = {
	"from",
	"to",
	"date",
	"pass",
	"login"
};

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

static void
buf_puts(const char *s, struct buf *b)
{
	size_t	 sz = strlen(s);

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
				buf_puts(mail->from, b);
				break;
			case (MAILKEY_TO):
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

static void
mail_template_close(char *buf, size_t sz, int fd)
{

	if (NULL == buf)
		return;
	munmap(buf, sz);
	close(fd);
}

void 
mail_players(const char *uri)
{
	CURL		  *curl;
	CURLcode 	   res;
	struct curl_slist *recipients = NULL;
	char		   fname[PATH_MAX];
	time_t		   t;
	struct tm	  *tm;
	struct buf	   buf;
	struct smtp	  *smtp;
	struct mail	   mail;
	int64_t		   id;
	char		  *mbuf, *encto, *encpass;
	size_t		   mbufsz;
	int		   mbuffd;

	snprintf(fname, sizeof(fname), "%s/%s",
		NULL == getenv("DB_DIR") ? "." : getenv("DB_DIR"),
		"addplayer.eml");

	if (NULL == (smtp = db_smtp_get())) {
		fprintf(stderr, "smtp not initialised\n");
		return;
	} else if (NULL == (curl = curl_easy_init())) {
		perror(NULL);
		db_smtp_free(smtp);
		return;
	} 

	memset(&mail, 0, sizeof(struct mail));
	memset(&buf, 0, sizeof(struct buf));
	
	mbuf = mail_template_open(fname, &mbuffd, &mbufsz);
	if (NULL == mbuf) {
		fprintf(stderr, "%s: invalid mail\n", fname);
		goto out;
	}

	fprintf(stderr, "mailing: from %s\n", smtp->from);
	fprintf(stderr, "mailing: server %s@%s\n", smtp->user, smtp->server);
	fprintf(stderr, "mailing: uri %s\n", uri);

	curl_easy_setopt(curl, CURLOPT_USERNAME, smtp->user);
	curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp->pass);
	curl_easy_setopt(curl, CURLOPT_URL, smtp->server);
	curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, smtp->from);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, dobuf);

	t = time(NULL);
	tm = gmtime(&t);
	asctime_r(tm, mail.date);
	mail.date[strlen(mail.date) - 1] = '\0';
	mail.from = smtp->from;

	while (NULL != (mail.to = db_player_next_new(&id, &mail.pass))) {
		assert(NULL != mail.pass);

		encto = kutil_urlencode(mail.to);
		encpass = kutil_urlencode(mail.pass);

		kasprintf(&mail.login, 
			"%s?email=%s&password=%s",
			uri, encto, encpass);

		buf.sz = buf.cur = 0;
		mail_template_buf(mbuf, mbufsz, &mail, &buf);

		recipients = NULL;
		recipients = curl_slist_append(recipients, mail.to);

		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
		curl_easy_setopt(curl, CURLOPT_READDATA, &buf);

		/* Send the message */ 
		if (CURLE_OK == (res = curl_easy_perform(curl))) {
			db_player_set_mailed(id, mail.pass);
			fprintf(stderr, "%s: new player\n", mail.to);
		} else
			fprintf(stderr, "%s: %s\n", 
				mail.to, curl_easy_strerror(res));

		curl_slist_free_all(recipients);
		recipients = NULL;
		/*
		 * TODO.
		 * explicit_bzero(buf.buf, buf.sz);
		 * explicit_bzero(pass, strlen(pass));
		 */
		free(mail.to);
		free(mail.pass);
		free(mail.login);
		free(encto);
		free(encpass);
	}

out:
	free(buf.buf);
	mail_template_close(mbuf, mbufsz, mbuffd);
	db_smtp_free(smtp);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}
