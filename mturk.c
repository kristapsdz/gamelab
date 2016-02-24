/*	$Id$ */
/*
 * Copyright (c) 2015, 2016 Kristaps Dzonsons <kristaps@kcons.eu>
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
#define	SERVICE "AWSMechanicalTurkRequester"
#define EXTQUES	"<ExternalQuestion xmlns=\"" SCHEMA "\">" \
		  "<ExternalURL>" \
		    "https://%s" LABURI "/mturk.html" \
		  "</ExternalURL>" \
		  "<FrameHeight>400</FrameHeight>" \
		"</ExternalQuestion>"
#define REALURL	"mechanicalturk.amazonaws.com"
#define	SANDURL "mechanicalturk.sandbox.amazonaws.com"
#define	DEFNAME	"Gamelab Experiment"
#define DEFDESC "This is a gamelab experiment"
#define	MINPAY	0.01
#define	MINWORK	2
#define	MINMINS	1

/*
 * These are all the operations that we support.
 */
enum 	awstype {
	AWS_CREATE_HIT,
	AWS_GRANT_BONUS,
	AWS__MAX
};

/*
 * Parse state, that is, what [ancestor] node we're processing.
 */
enum	awsstate {
	AWSSTATE_ERRORS, /* errors */
	AWSSTATE_HITID, /* hitId */
	AWSSTATE__MAX
};

/*
 * Describes a parsed response from the AWS server.
 */
struct	aws {
	enum awstype	 type; /* type of response */
	char		*errorCodes; /* non-NULL means errors */
	char		*hitId; /* not-null ok AWS_CREATE_HIT */
};

/*
 * Parse status. 
 */
struct	state {
	int	 	 ok; /* parse syntax ok */
	struct aws	 aws; /* parsing aws object */
	enum awsstate	 state; /* state of parse */
	int		 useb; /* collecting text? */
	char		*b; /* text collector */
	size_t		 bsz; /* size of collected buffer */
};

static	const char *const awstypes[AWS__MAX] = {
	"CreateHIT", /* AWS_CREATE_HIT */
	"GrantBonus" /* AWS_GRANT_BONUS */
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
		if (AWSSTATE_ERRORS == st->state) {
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
	} else if (0 == strcasecmp(s, "grantbonusresult")) {
		st->aws.type = AWS_GRANT_BONUS;
	} else if (0 == strcasecmp(s, "errors")) {
		st->state = AWSSTATE_ERRORS;
	} else if (0 == strcasecmp(s, "message")) {
		if (AWSSTATE_ERRORS == st->state)
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
		WARNX("Response buffer length %zu failed "
			"with error code %d: %s", rsz, erc, 
			XML_ErrorString(erc));
		st->ok = 0;
	}
  	return(rsz);
}

static void
state_free(struct state *st)
{

	free(st->b);
	free(st->aws.errorCodes);
	free(st->aws.hitId);
}

static XML_Parser
state_alloc(struct state *st)
{
	XML_Parser	 p;

	memset(st, 0, sizeof(struct state));
	st->ok = 1;
	st->aws.type = AWS__MAX;
	if (NULL != (p = XML_ParserCreate(NULL))) {
		XML_SetUserData(p, st);
		XML_SetElementHandler(p, node_open, node_close);
		XML_SetCharacterDataHandler(p, node_text);
	} else
		WARNX("XML_ParserCreate");

	return(p);
}

static char *
mturk_init(const char *key, enum awstype type, 
	char *tstamp, size_t tstampsz)
{
	time_t	 	 tt;
	struct tm	*gmt;
	char		*sigprop, *pdigest, *enc;
	unsigned char	 digest[SHA1_DIGEST_LENGTH];

	/* Construct what we'll use for our signature. */
	tt = time(NULL);
	gmt = gmtime(&tt);
	strftime(tstamp, tstampsz, "%Y-%m-%dT%TZ", gmt);
	kasprintf(&sigprop, SERVICE "%s%s", awstypes[type], tstamp);

	/* Construct HMAC-SHA1 digest in base64. */
	hmac_sha1((unsigned char *)sigprop, strlen(sigprop), 
		  (unsigned char *)key, strlen(key), digest);
	pdigest = kmalloc(base64len(SHA1_DIGEST_LENGTH));
	base64buf(pdigest, (const char *)digest, SHA1_DIGEST_LENGTH);

	/* URL-encode result and return. */
	enc = kutil_urlencode(pdigest);
	free(sigprop);
	free(pdigest);
	return(enc);
}

/*
 * Submit bonuses to Mechanical Turk players.
 * The players must have a valid AssignmentID and the experiment must
 * have its AWS credentials intact.
 */
void
mturk_bonus(const struct expr *expr, 
	const struct player *p, int64_t score)
{
	CURL		*c;
	CURLcode	 res;
	XML_Parser 	 parser;
	char		 t[64];
	struct state	 st;
	char		*url, *pdigest, *encdate, *post;
	double		 reward;

	assert(NULL != expr->awsaccesskey && 
	       '\0' != *expr->awsaccesskey &&
	       NULL != expr->awssecretkey && 
	       '\0' != *expr->awssecretkey &&
	       NULL != p->assignmentid &&
	       '\0' != *p->assignmentid);

	if (NULL == (parser = state_alloc(&st)))
		return;
	if (NULL == (c = curl_easy_init())) {
		WARNX("curl_easy_init");
		XML_ParserFree(parser);
		return;
	}

	/* URL-encode necessary inputs. */
	pdigest = mturk_init
		(expr->awssecretkey, 
		 AWS_GRANT_BONUS, t, sizeof(t));
	encdate = kutil_urlencode(t);
	reward = score * expr->conversion;
	if (reward < 0.0)
		reward = 0.0;

	/* Construct request URL. */
	kasprintf(&url, "https://%s", 
		EXPR_SANDBOX & expr->flags ? 
		SANDURL : REALURL);
	kasprintf(&post, 
		"Service=" SERVICE
		"&AWSAccessKeyId=%s"
		"&Operation=%s"
		"&Signature=%s"
		"&Timestamp=%s"
		"&WorkerId=%s"
		"&Reason=Gamelab%%20participation"
		"&BonusAmount.1.Amount=%g"
		"&BonusAmount.1.CurrencyCode=US"
		"&UniqueRequestToken=" PRId64
		"&AssignmentId=%s",
		expr->awsaccesskey,  /* access key ID */
		awstypes[AWS_GRANT_BONUS], /* operation */
		pdigest, /* request signature */
		encdate, /* used for signature */
		p->mail, /* identifier of player */
		reward, /* >0 amount of reward */
		p->rseed, /* bonus identifier */
		p->assignmentid);

	INFO("%s: preparing", url);
	INFO("%s: posting", post);
#if 0
	/* Initialise CURL object. */
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, node_parse);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)parser);
	curl_easy_setopt(c, CURLOPT_POST, 1L);
	curl_easy_setopt(c, CURLOPT_POSTFIELDS, post);

	if (CURLE_OK != (res = curl_easy_perform(c))) {
	      WARNX("curl_easy_perform failed: %s", 
		   curl_easy_strerror(res));
	} else if (st.ok) {
		if (0 == XML_Parse(parser, NULL, 0, 1)) {
			WARNX("XML_Parse: %s", 
				XML_ErrorString
				(XML_GetErrorCode(parser)));
			st.ok = 0;
		} 
	}

	if (st.ok)
		INFO("%s: success", url);
	else
		WARNX("%s: error", url);

	curl_easy_cleanup(c);
	curl_global_cleanup();

#endif
	free(url);
	free(post);
	state_free(&st);
	XML_ParserFree(parser);
}

void
mturk_create(const char *aws, const char *key, const char *name, 
	const char *desc, int64_t workers, int64_t minutes, 
	int sandbox, double reward, const char *keywords,
	const char *server)
{
	CURL		*c;
	CURLcode	 res;
	XML_Parser 	 parser;
	int		 erc;
	char		 t[64];
	struct state	 st;
	char		*url, *encques, *encname, *encdesc,
			*pdigest, *encdate, *post, *enckeys, *lurl;

	/* 
	 * Clamp input values and sanitise everything going to the
	 * server.
	 * We need to provide a minimum amount of sanity because gamelab
	 * itself might be discouraged by shit submissions.
	 */
	if (workers < MINWORK)
		workers = MINWORK;
	if (reward < MINPAY)
		reward = MINPAY;
	if (minutes < MINMINS)
		minutes = MINMINS;
	if ('\0' == *desc)
		desc = DEFDESC;
	if ('\0' == *name)
		name = DEFNAME;

	assert(NULL != aws && '\0' != *aws &&
	       NULL != key && '\0' != *key);

	if (NULL == (parser = state_alloc(&st)))
		return;
	if (NULL == (c = curl_easy_init())) {
		WARNX("curl_easy_init");
		XML_ParserFree(parser);
		return;
	}

	kasprintf(&lurl, EXTQUES, server);

	/* URL-encode all inputs. */
	pdigest = mturk_init(key, AWS_CREATE_HIT, t, sizeof(t));
	encdate = kutil_urlencode(t);
	encname = kutil_urlencode(name);
	encdesc = kutil_urlencode(desc);
	enckeys = kutil_urlencode(keywords);
	encques = kutil_urlencode(lurl);

	/* 
	 * Actually construct the URL and post fields.
	 * We use the user-supplied information except for the currency
	 * code (AWS only allows USD anyway) and the qualifications.
	 */
	kasprintf(&url, "https://%s", sandbox ? SANDURL : REALURL);
	kasprintf(&post, 
		"Service=" SERVICE
		"&AWSAccessKeyId=%s"
		"&Operation=%s"
		"&Signature=%s"
		"&Timestamp=%s"
		"&Title=%s"
		"&Description=%s"
		"&Reward.1.Amount=%.2f"
		"&Reward.1.CurrencyCode=USD"
		"&Question=%s"
		"&AssignmentDurationInSeconds=%" PRId64
		"&LifetimeInSeconds=%" PRId64
		"&Keywords=%s"
		"&MaxAssignments=%" PRId64
		"&QualificationRequirement.1."
			"QualificationTypeId=000000000000000000L0"
		"&QualificationRequirement.1."
			"Comparator=GreaterThan"
		"&QualificationRequirement.1."
			"IntegerValue.1=95"
		"&QualificationRequirement.2."
			"QualificationTypeId=00000000000000000040"
		"&QualificationRequirement.2."
			"Comparator=GreaterThan"
		"&QualificationRequirement.2."
			"IntegerValue.1=500"
		"&QualificationRequirement.3."
			"QualificationTypeId=00000000000000000071"
		"&QualificationRequirement.3."
			"Comparator=EqualTo"
		"&QualificationRequirement.3."
			"LocaleValue.1.Country=US",
		aws,  /* access key ID */
		awstypes[AWS_CREATE_HIT], /* operation */
		pdigest, /* request signature */
		encdate, /* used for signature */
		encname, /* name of experment (URL encode) */
		encdesc, /* description (URL encode) */
		reward,  /* reward for participants */
		encques,  /* question data */
		minutes * 60, /* assignment duration */
		minutes * 60, /* when it's valid */
		enckeys, /* keywords */
		workers); /* number of workers */

	/* Initialise CURL object. */
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, node_parse);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)parser);
	curl_easy_setopt(c, CURLOPT_POST, 1L);
	curl_easy_setopt(c, CURLOPT_POSTFIELDS, post);

	INFO("Preparing mturk to %s: %s", url, aws);

	if (CURLE_OK != (res = curl_easy_perform(c))) {
	      WARNX("curl_easy_perform failed: %s", 
		   curl_easy_strerror(res));
	} else if (st.ok) {
		if (0 == XML_Parse(parser, NULL, 0, 1)) {
			erc = XML_GetErrorCode(parser);
			WARNX("Response final buffer failed "
				"with error code %d: %s", erc, 
				XML_ErrorString(erc));
			st.ok = 0;
		} 
	}

	if (st.ok) {
		INFO("Mturk enquiry finished: received");
		if (NULL != st.aws.errorCodes) {
			WARNX("Mturk enquiry has an error code");
			db_expr_mturk(NULL, st.aws.errorCodes);
		} else if (NULL != st.aws.hitId) {
			INFO("Mturk enquiry success");
			db_expr_mturk(st.aws.hitId, NULL);
		} else {
			INFO("Mturk enquiry... not-success?");
			db_expr_mturk(NULL, "Ambiguous outcome");
		}
	} else {
		WARNX("Mturk enquiry finished: bad communication");
		db_expr_mturk(NULL, "Error in transmission");
	}

	curl_easy_cleanup(c);
	curl_global_cleanup();

	free(encques);
	free(encname);
	free(encdesc);
	free(url);
	free(post);
	free(lurl);
	state_free(&st);
	XML_ParserFree(parser);
}
