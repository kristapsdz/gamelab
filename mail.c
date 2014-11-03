/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <kcgi.h>
#include <gmp.h>
#include <curl/curl.h>

#include "extern.h"

struct buf {
	char		*buf;
	size_t		 cur;
	size_t		 sz;
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

void 
mail_players(void)
{
	CURL		  *curl;
	CURLcode 	   res = CURLE_OK;
	struct curl_slist *recipients = NULL;
	time_t		   t;
	struct tm	  *tm;
	struct buf	   buf;
	char		  *email, *pass, timebuf[27];
	struct smtp	  *smtp;

	if (NULL == (smtp = db_smtp_get())) {
		fprintf(stderr, "smtp not initialised\n");
		return;
	} else if (NULL == (curl = curl_easy_init())) {
		perror(NULL);
		db_smtp_free(smtp);
		return;
	}

	curl_easy_setopt(curl, CURLOPT_USERNAME, smtp->user);
	curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp->pass);
	curl_easy_setopt(curl, CURLOPT_URL, smtp->server);
	curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, smtp->from);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, dobuf);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

	fprintf(stderr, "mailing from %s on %s@%s\n",
		smtp->from, smtp->user, smtp->server);

	while (NULL != (email = db_player_next_new(&pass))) {
		assert(NULL != pass);
		t = time(NULL);
		tm = gmtime(&t);
		asctime_r(tm, timebuf);
		timebuf[strlen(timebuf) - 1] = '\0';
		memset(&buf, 0, sizeof(struct buf));
		kasprintf(&buf.buf,
			 "Date: %s GMT\r\n"
			 "To: %s\r\n"
			 "From: %s\r\n"
			 "Subject: Gamelab Password\r\n"
			 "\r\n"
			 "Login with the following password: %s\r\n"
			 "\r\n"
			 "Thanks,\r\n"
			 "\r\n"
			 "Gamelab Administrator\r\n"
			 "\r\n",
			 timebuf, email, smtp->from, pass);
		buf.sz = strlen(buf.buf);

		recipients = NULL;
		recipients = curl_slist_append(recipients, "");
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
		curl_easy_setopt(curl, CURLOPT_READDATA, &buf);
		/* Send the message */ 
		if (CURLE_OK != (res = curl_easy_perform(curl)))
			fprintf(stderr, "mail: %s\n", 
				curl_easy_strerror(res));

		fprintf(stderr, "%s: emailed\n", email);

		curl_slist_free_all(recipients);
		recipients = NULL;
		/*explicit_bzero(buf.buf, buf.sz);
		explicit_bzero(pass, strlen(pass));*/
		free(buf.buf);
		free(email);
		free(pass);
	}

	db_smtp_free(smtp);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}
