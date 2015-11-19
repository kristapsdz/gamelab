/*	$Id$ */
/*
 * Copyright (c) 2015 Kristaps Dzonsons <kristaps@kcons.eu>
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
#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <expat.h>
#include <kcgi.h>
#include <kcgijson.h>
#include <gmp.h>
#include <curl/curl.h>

#include "extern.h"

#define	SCHEMA	"http://mechanicalturk.amazonaws.com/" \
		"AWSMechanicalTurkDataSchemas/2006-07-14/" \
		"ExternalQuestion.xsd"

enum 	awstype {
	AWS_CREATE_HIT,
	AWS__MAX
};

enum	awsstate {
	AWSSTATE_ERROR,
	AWSSTATE_HITID,
	AWSSTATE__MAX
};

struct	aws {
	enum awstype	 type;
	char		*errorCodes;
	char		*hitId;
};

struct	state {
	int	 	 ok;
	struct aws	 aws;
	enum awsstate	 state;
	int		 useb;
	char		*b;
	size_t		 bsz;
};

static void
node_text(void *arg, const XML_Char *s, int len)
{
	struct state 	*st = arg;

	assert(NULL != st);
	if (0 == st->ok || 0 == st->useb)
		return;

	st->b = krealloc(st->b, st->bsz + len + 1);
	memcpy(st->b + st->bsz, s, len);
	st->b[st->bsz + len] = '\0';
	st->bsz += len;
}

static void
node_close(void *arg, const XML_Char *s)
{
	struct state 	*st = arg;

	assert(NULL != st);
	if (0 == st->ok)
		return;

	if (0 == strcasecmp(s, "errors")) {
		st->state = AWSSTATE__MAX;
	} else if (0 == strcasecmp(s, "message")) {
		if (AWSSTATE_ERROR == st->state) {
			free(st->aws.errorCodes);
			st->aws.errorCodes = st->b;
		}
		st->bsz = 0;
		st->b = NULL;
		st->useb = 0;
	} else if (0 == strcasecmp(s, "hitid")) {
		free(st->aws.hitId);
		st->aws.hitId = st->b;
		st->bsz = 0;
		st->b = NULL;
		st->useb = 0;
		st->state = AWSSTATE__MAX;
	}
}

static void
node_open(void *arg, const XML_Char *s, const XML_Char **atts)
{
	struct state 	*st = arg;

	assert(NULL != st);
	if (0 == st->ok)
		return;

	if (0 == strcasecmp(s, "createhitresponse")) {
		st->aws.type = AWS_CREATE_HIT;
	} else if (0 == strcasecmp(s, "errors")) {
		st->state = AWSSTATE_ERROR;
	} else if (0 == strcasecmp(s, "message")) {
		if (AWSSTATE_ERROR == st->state)
			st->useb = 1;
	} else if (0 == strcasecmp(s, "hitid")) {
		st->state = AWSSTATE_HITID;
		st->useb = 1;
	}
}

static size_t 
node_parse(void *contents, size_t len, size_t nm, void *arg)
{
	XML_Parser 	 parser = (XML_Parser)arg;
	size_t 		 rsz;
	struct state 	*st;
	int		 erc;

	st = XML_GetUserData(parser);
	assert(NULL != st);
	/* Overflow check. */
	if (0 == st->ok || SIZE_MAX / nm < len) 
		return(0);

	rsz = nm * len;

	if (0 == XML_Parse(parser, contents, (int)rsz, 0)) {
		erc = XML_GetErrorCode(parser);
		WARN("Response buffer length %zu failed "
			"with error code %d: %s", rsz, erc, 
			XML_ErrorString(erc));
		st->ok = 0;
	}
  	return(rsz);
}

void
mturk(const char *aws, const char *key, const char *name, 
	const char *desc, int64_t workers, int64_t minutes, 
	int sandbox, double reward)
{
	CURL		*c;
	CURLcode	 res;
	XML_Parser 	 parser;
	time_t		 tt;
	int		 erc;
	struct tm	*gmt;
	size_t		 i;
	char		 tstamp[64], digest[20], pdigest[20 * 2 + 1];
	struct state	 st;
	char		*url, *encques, *sigprop, *encname, *encdesc;

	/* Clamp input values. */
	if (workers < 2)
		workers = 2;
	if (reward < 0.01)
		reward = 0.01;
	if (minutes < 1)
		minutes = 1;
	if ('\0' == *desc)
		desc = "gamelab";
	if ('\0' == *name)
		name = "gamelab";

	assert(NULL != aws && '\0' != *aws &&
	       NULL != key && '\0' != *key);

	/* Initialise our response parser. */
	memset(&st, 0, sizeof(struct state));
	st.ok = 1;
	st.aws.type = AWS__MAX;
	if (NULL == (parser = XML_ParserCreate(NULL))) {
		perror(NULL);
		return;
	}
	XML_SetUserData(parser, &st);
	XML_SetElementHandler(parser, node_open, node_close);
	XML_SetCharacterDataHandler(parser, node_text);

	/* Initialise the CURL object. */
	if (NULL == (c = curl_easy_init())) {
		perror(NULL);
		XML_ParserFree(parser);
		return;
	}
	
	/* Input for AWS signature. */
	tt = time(NULL);
	gmt = gmtime(&tt);
	strftime(tstamp, sizeof(tstamp), "%Y-%m-%dT%TZ", gmt);
	kasprintf(&sigprop, "%s%s%s", 
		"AWSMechanicalTurkRequester",
		"CreateHIT", tstamp);

	/* Construct signature HMAC-SHA1 digest. */
	hmac_sha1((unsigned char *)sigprop, strlen(sigprop), 
		  (unsigned char *)key, strlen(key), 
		  (unsigned char *)digest);

	/* Convert digest to hex string. */
	for (i = 0; i < sizeof(digest); i++)
		snprintf(&pdigest[i * 2], 3, "%02x", digest[i]);

	/* URL-encode all unknown inputs. */
	encname = kutil_urlencode(name);
	encdesc = kutil_urlencode(desc);
	encques = kutil_urlencode
		("<ExternalQuestion xmlns=\"" SCHEMA "\">"
			"<ExternalURL>"
				"https://" LABURI "/mturk.html"
			"</ExternalURL>"
			"<FrameHeight>400</FrameHeight>"
		 "</ExternalQuestion>");

	/* Actually construct the full URL. */
	kasprintf(&url, "https://%s"
		"?Service=AWSMechanicalTurkRequester"
		"&AWSAccessKeyId=%s"
		"&Operation=CreateHIT"
		"&Signature=%s"
		"&Timestamp=%s"
		"&Title=%s"
		"&Description=%s"
		"&Reward.1.Amount=%.2f"
		"&Reward.1.CurrencyCode=USD"
		"&Question=%s"
		"&LifetimeInSeconds=%" PRId64,
		sandbox ? 
		 "mechanicalturk.sandbox.amazonaws.com" :
		 "mechanicalturk.amazonaws.com",
		aws, pdigest, tstamp, encname, 
		encdesc, reward, encques, minutes * 60);

	/* Initialise CURL object. */
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, node_parse);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)parser);

	INFO("Preparing mturk enquiry: %s",
		sandbox ? 
		 "mechanicalturk.sandbox.amazonaws.com" :
		 "mechanicalturk.amazonaws.com");

	if (CURLE_OK != (res = curl_easy_perform(c))) {
	      WARN("curl_easy_perform failed: %s", 
		   curl_easy_strerror(res));
	} else if (st.ok) {
		if (0 == XML_Parse(parser, NULL, 0, 1)) {
			erc = XML_GetErrorCode(parser);
			WARN("Response final buffer failed "
				"with error code %d: %s", erc, 
				XML_ErrorString(erc));
			st.ok = 0;
		} 
	}


	if (st.ok) {
		INFO("Mturk enquiry finished: received");
		if (NULL != st.aws.errorCodes) {
			WARN("Mturk enquiry has an error code");
			db_expr_mturk(NULL, st.aws.errorCodes);
		} else if (NULL != st.aws.hitId) {
			INFO("Mturk enquiry success");
			db_expr_mturk(st.aws.hitId, NULL);
		} else {
			INFO("Mturk enquiry... not-success?");
			db_expr_mturk(NULL, "Ambiguous outcome");
		}
	} else {
		WARN("Mturk enquiry finished: bad communication");
		db_expr_mturk(NULL, "Error in transmission");
	}

	curl_easy_cleanup(c);
	curl_global_cleanup();

	free(sigprop);
	free(encques);
	free(encname);
	free(encdesc);
	free(url);
	free(st.b);
	free(st.aws.errorCodes);
	free(st.aws.hitId);
	XML_ParserFree(parser);
}
