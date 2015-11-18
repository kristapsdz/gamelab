/*	$Id$ */
#include <assert.h>
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

#define	EXTERNAL_SCHEMA	"http://mechanicalturk.amazonaws.com/" \
			"AWSMechanicalTurkDataSchemas/2006-07-14/" \
			"ExternalQuestion.xsd"

struct	state {
	int	 ok;
};

static size_t 
parse(void *contents, size_t len, size_t nm, void *arg)
{
	XML_Parser 	 parser = (XML_Parser) arg;
	size_t 		 rsz;
	struct state 	*state;
	int		 erc;

	/* Overflow check. */
	if (SIZE_MAX / nm < len) 
		return(0);

	rsz = nm * len;
	state = XML_GetUserData(parser);
	assert(NULL != state);
 
	/* Only parse if we're not already in a failure state. */ 
	if (state->ok && 0 == XML_Parse(parser, contents, rsz, 0)) {
		erc = XML_GetErrorCode(parser);
		WARN("Response buffer length %zu failed "
			"with error code %d: %s", rsz, erc, 
			XML_ErrorString(erc));
		state->ok = 0;
	}
  	return(rsz);
}

void
mturk(const char *aws, const char *key, 
	const char *name, const char *desc,
	int64_t workers, int64_t minutes, int sandbox,
	double reward)
{
	CURL		*c;
	CURLcode	 res;
	XML_Parser 	 parser;
	time_t		 tt;
	int		 erc;
	struct tm	*gmt;
	size_t		 i;
	char		 tstamp[64], digest[20], pdigest[20 * 2 + 1];
	struct state	 state;
	char		*url, *encques, *sigprop, *encname, *encdesc;

	/* Clamp input values. */
	if (workers < 2)
		workers = 2;
	if (reward < 0.01)
		reward = 0.01;

	assert(NULL != aws && '\0' != *aws &&
	       NULL != key && '\0' != *key);

	/* Initialise our response parser. */
	memset(&state, 0, sizeof(struct state));
	state.ok = 1;
	if (NULL == (parser = XML_ParserCreateNS(NULL, '\0'))) {
		perror(NULL);
		return;
	}
	XML_SetUserData(parser, &state);

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
		("<ExternalQuestion xmlns=\"" EXTERNAL_SCHEMA "\">"
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
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, parse);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)parser);

	if (CURLE_OK != (res = curl_easy_perform(c))) {
	      WARN("curl_easy_perform failed: %s", 
		   curl_easy_strerror(res));
	} else if (state.ok) {
		if (0 == XML_Parse(parser, NULL, 0, 1)) {
			erc = XML_GetErrorCode(parser);
			WARN("Response final buffer failed "
				"with error code %d: %s", erc, 
				XML_ErrorString(erc));
		}
	}

	curl_easy_cleanup(c);
	curl_global_cleanup();

	free(sigprop);
	free(encques);
	free(encname);
	free(encdesc);
	free(url);
	XML_ParserFree(parser);
}
