/*	$Id$ */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <kcgi.h>
#include <kcgijson.h>
#include <gmp.h>
#include <curl/curl.h>

#include "extern.h"

void
mturk(const char *aws, const char *key, 
	const char *name, const char *desc,
	int64_t workers, int64_t minutes, int sandbox,
	double reward)
{
#if 0
	CURL		*c;
#endif
	time_t		 tt;
	struct tm	*gmt;
	size_t		 i;
	char		 tstamp[64], digest[20], pdigest[20 * 2 + 1];
	char		*url, *encques, *sigprop, *encname, *encdesc;

	if (workers < 2)
		workers = 2;

#if 0
	if (NULL == (c = curl_easy_init())) {
		perror(NULL);
		return;
	}
#endif
	
	tt = time(NULL);
	gmt = gmtime(&tt);
	strftime(tstamp, sizeof(tstamp), "%Y-%m-%dT%TZ", gmt);

	kasprintf(&sigprop, "AWSMechanicalTurkRequesterCreateHIT%s", tstamp);
	hmac_sha1((unsigned char *)sigprop, strlen(sigprop), 
		  (unsigned char *)key, strlen(key), 
		  (unsigned char *)digest);
	for (i = 0; i < sizeof(digest); i++)
		snprintf(&pdigest[i * 2], 3, "%02x", digest[i]);

	encname = kutil_urlencode(name);
	encdesc = kutil_urlencode(desc);
	encques = kutil_urlencode
		("<ExternalQuestion xmlns=\"http://mechanicalturk.amazonaws.com/"
				"AWSMechanicalTurkDataSchemas/2006-07-14/"
				"ExternalQuestion.xsd\">"
			"<ExternalURL>https://" LABURI "/mturk.html</ExternalURL>"
			"<FrameHeight>400</FrameHeight>"
		 "</ExternalQuestion>");
	kasprintf(&url, 
		"https://%s"
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
#if 0
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);

	curl_easy_cleanup(c);
	curl_global_cleanup();
#endif

	INFO("url = %s", url);

	free(sigprop);
	free(encques);
	free(encname);
	free(encdesc);
	free(url);
}
