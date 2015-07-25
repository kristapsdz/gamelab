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
#include <sys/select.h>

#include <assert.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json-c/json.h>
#include <curl/curl.h>

/*
 * The phases of starting, registering, joining, etc.
 * This controls the FSM of gamer action.
 */
enum	phase {
	PHASE_REGISTER = 0, /* registering */
	PHASE_LOGIN, /* logging in */
	PHASE_LOADEXPR, /* load experiment for play */
	PHASE_PLAY, /* play against server */
	PHASE__MAX
};

/*
 * This will be filled in upon initialisation with the URLs accessed for
 * each gamer phase.
 */
static	char *urls[PHASE__MAX] = {
	NULL, /* PHASE_REGISTER */
	NULL, /* PHASE_LOGIN */
	NULL, /* PHASE_LOADEXPR */
	NULL, /* PHASE_PLAY */
};

/*
 * A resizable buffer `in the usual way'.
 * There are no constraints whatsoever (nil-terminated, etc.).
 */
struct	buf {
	char		*b; /* buffer (or NULL) */
	size_t		 bsz; /* length of buffer */
	size_t		 bcur; /* current (for serialising) */
	size_t		 bmax; /* allocated space */
};

/*
 * The entire game simulation.
 */
struct	game {
	size_t		 finished; /* how many have finished */
	int		 verbose; /* verbose operation */
	CURLM		*curl; /* the multi handle */
};

/*
 * A Netscape cookie. 
 * The fields are self-explanatory.
 * (We usually only care about key and val.) 
 * All non-integers are nil-terminated.
 */
struct 	cookie {
	char		*domain;
	int		 flag;
	char		*path;
	int		 secure;
	char		*expire;
	char		*key;
	char		*val;
};

/*
 * A single player in the simulation.
 */
struct	gamer {
	struct game		*game; /* the game itself */
	char			*email; /* our e-mail address */
	char			*password; /* our password */
	char			*sesscookie; /* our password */
	char			*sessid; /* our password */
	const char		*url; /* the URL */
	enum phase		 phase; /* what we're doing */
	CURL			*conn; /* my connection */
	struct buf		 post; /* buffer for posting */
	struct buf		 cookie; /* buffer for cookies */
	struct json_tokener 	*tok; /* download tokeniser */
	struct json_object	*parsed; /* parsed json object */
	int64_t			 lastround; /* last seen round */
};

/*
 * Overwrites the buffer at `buf' with the string formatted from `fmt'
 * and the variable arguments.
 * This will always nil-terminate the result, and may reallocate the
 * buffer to hold the output.
 * It returns zero on failure (memory allocation), non-zero otherwise.
 */
static int
buf_write(struct buf *buf, const char *fmt, ...)
{
	va_list	 ap, app;
	void	*p;
	int	 ret;

	va_start(ap, fmt);
	va_copy(app, ap);
	if (-1 == (ret = vsnprintf(NULL, 0, fmt, ap))) {
		perror("vsnprintf");
		return(0);
	}
	if ((size_t)ret + 1 > buf->bmax) {
		p = realloc(buf->b, (size_t)ret + 1);
		if (NULL == p) {
			perror(NULL);
			return(0);
		}
		buf->b = p;
		buf->bmax = ret + 1;
	}
	ret = vsnprintf(buf->b, buf->bmax, fmt, app);
	va_end(ap);
	assert(-1 != ret && (size_t)ret + 1 <= buf->bmax);
	buf->bsz = ret + 1;
	return(1);
}

/*
 * Parse a Netscape-style cookie string.
 * This will modified `cp' during the parse itself.
 * See the http://cookiecentral.com FAQ, section 3.5.
 */
static int
cookie(struct cookie *c, char *cp)
{
	char	*tok;

	memset(c, 0, sizeof(struct cookie));

	/* Relevant domain. */
	if (NULL == (tok = strsep(&cp, "\t")))
		return(0);
	c->domain = tok;

	/* All-domain flag. */
	if (NULL == (tok = strsep(&cp, "\t")))
		return(0);
	if (0 == strcasecmp(tok, "TRUE"))
		c->flag = 1;
	else if (0 == strcasecmp(tok, "FALSE"))
		c->flag = 0;
	else
		return(0);

	/* Access path. */
	if (NULL == (tok = strsep(&cp, "\t")))
		return(0);
	c->path = tok;

	/* Secure only (HTTPS). */
	if (NULL == (tok = strsep(&cp, "\t")))
		return(0);
	if (0 == strcasecmp(tok, "TRUE"))
		c->secure = 1;
	else if (0 == strcasecmp(tok, "FALSE"))
		c->secure = 0;
	else
		return(0);

	/* Expiration date. */
	if (NULL == (tok = strsep(&cp, "\t")))
		return(0);
	c->expire = tok;

	/* Name. */
	if (NULL == (tok = strsep(&cp, "\t")))
		return(0);
	c->key = tok;

	/* Value. */
	if (NULL == (tok = strsep(&cp, "\t")))
		return(0);
	c->val = tok;

	return(1);
}

static int
json_check(const struct gamer *gamer)
{

	if (NULL == gamer->parsed) {
		fprintf(stderr, "%s: missing JSON: %s\n",
			gamer->email, gamer->url);
		return(0);
	}
	if (json_type_object != json_object_get_type(gamer->parsed)) {
		fprintf(stderr, "%s: bad JSON type: %s\n",
			gamer->email, gamer->url);
		return(0);
	}
	return(1);
}

static int
json_getobj(const struct gamer *gamer, 
	json_object *parent, json_object **obj, const char *name)
{

	if ( ! json_object_object_get_ex(parent, name, obj)) {
		fprintf(stderr, "%s: no JSON `%s\': %s\n",
			gamer->email, name, gamer->url);
		return(0);
	} else if (json_type_object != json_object_get_type(*obj)) {
		fprintf(stderr, "%s: bad JSON `%s' type: %s\n",
			gamer->email, name, gamer->url);
		return(0);
	}
	return(1);
}

static int
json_getint(const struct gamer *gamer, 
	json_object *parent, int64_t *val, const char *name)
{
	json_object	*obj;

	/* Verify that the email is sane. */
	if ( ! json_object_object_get_ex(parent, name, &obj)) {
		fprintf(stderr, "%s: no JSON `%s\': %s\n",
			gamer->email, name, gamer->url);
		return(0);
	} else if (json_type_int != json_object_get_type(obj)) {
		fprintf(stderr, "%s: bad JSON `%s' type: %s\n",
			gamer->email, name, gamer->url);
		return(0);
	}
	*val = json_object_get_int64(obj);
	return(1);
}

static int
json_getstring(const struct gamer *gamer, 
	json_object *parent, const char **val, const char *name)
{
	json_object	*obj;

	/* Verify that the email is sane. */
	if ( ! json_object_object_get_ex(parent, name, &obj)) {
		fprintf(stderr, "%s: no JSON `%s\': %s\n",
			gamer->email, name, gamer->url);
		return(0);
	} else if (json_type_string != json_object_get_type(obj)) {
		fprintf(stderr, "%s: bad JSON `%s' type: %s\n",
			gamer->email, name, gamer->url);
		return(0);
	}
	*val = json_object_get_string(obj);
	return(1);
}

/* 
 * Write to a gamer's buffer.
 */
static size_t
gamer_write(void *ptr, size_t sz, size_t nm, void *arg)
{
	struct gamer	*gamer = arg;
	enum json_tokener_error er;

	/* Obviously, we don't care about zero-length. */
	if (0 == sz || 0 == nm)
		return(0);

	/* Check for multiplication overflow. */
	if (SIZE_MAX / nm < sz) {
		fputs("multiplication overflow\n", stderr);
		return(0);
	}

	/* Do we already have an object? */
	if (NULL != gamer->parsed) {
		fputs("data after complete JSON\n", stderr);
		return(0);
	}

	/* 
	 * This is pretty awkward.
	 * The function returns an object when it's done, which can
	 * happen at any time whilst reading.
	 * Simply record it and do the best we can.
	 */
	gamer->parsed = json_tokener_parse_ex
		(gamer->tok, ptr, nm * sz);
	er = json_tokener_get_error(gamer->tok);
	if (er == json_tokener_success || 
	    er == json_tokener_continue)
		return(sz * nm);

	fprintf(stderr, "json_tokener_parse_ex: %s\n",
		json_tokener_error_desc(er));
	return(0);
}

/*
 * Reset the connection for a new run.
 * This means releasing us from the multi handle, resetting the easy
 * connection, then re-adding us to the multi handle.
 */
static int
gamer_reset(struct gamer *gamer)
{
	CURLMcode	 cm;

	cm = curl_multi_remove_handle(gamer->game->curl, gamer->conn);
	if (CURLM_OK != cm) {
		fprintf(stderr, "curl_multi_remove_handle: %s\n",
			curl_multi_strerror(cm));
		return(0);
	}
	curl_easy_reset(gamer->conn);
	cm = curl_multi_add_handle(gamer->game->curl, gamer->conn);
	if (CURLM_OK != cm) {
		fprintf(stderr, "curl_multi_remove_handle: %s\n",
			curl_multi_strerror(cm));
		return(0);
	}
	return(1);
}

/*
 * Generic initialisation for writing a server request.
 * We accept the gamer, to whom we'll assign the given URL, and whether
 * or not it's a POST request.
 * This will also set the receiver function for JSON data coming from
 * the server as a response.
 */
static int
gamer_init(struct gamer *gamer, const char *url, int post)
{
	CURLcode	 cc;

	gamer->url = url;
	cc = curl_easy_setopt(gamer->conn, CURLOPT_URL, url);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_URL: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}

	cc = curl_easy_setopt(gamer->conn, 
		CURLOPT_WRITEFUNCTION, gamer_write);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_WRITEFUNCTION: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}

	cc = curl_easy_setopt(gamer->conn, 
		CURLOPT_WRITEDATA, gamer);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_WRITEDATA: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}

	cc = curl_easy_setopt(gamer->conn, 
		post ? CURLOPT_POST : CURLOPT_HTTPGET, 1L);

	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_HTTPGET/POST: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}

	return(1);
}

static int
gamer_init_play(struct gamer *gamer, int64_t round)
{
	int	 	 rc;
	CURLcode	 cc;

	gamer->phase = PHASE_PLAY;

	/* Start with the URL for the captive portal. */
	if ( ! gamer_init(gamer, urls[PHASE_PLAY], 1)) {
		fputs("gamer_init", stderr);
		return(0);
	}

	rc = buf_write(&gamer->cookie, 
		"sessid=%s;sesscookie=%s", 
		gamer->sessid, gamer->sesscookie);
	if ( ! rc) {
		fputs("buf_write\n", stderr);
		return(0);
	}
	cc = curl_easy_setopt(gamer->conn, 
		CURLOPT_COOKIE, gamer->cookie.b);
        if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_COOKIE: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}

	/* Our POST consists only of our email address. */
	rc = buf_write(&gamer->post, 
		"round=%" PRId64 "&gid=1&index0=1", round);
	if ( ! rc) {
		fputs("buf_write\n", stderr);
		return(0);
	}

	/* Add the POST field to the request. */
	cc = curl_easy_setopt(gamer->conn, 
		CURLOPT_POSTFIELDS, gamer->post.b);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_POSTFIELDS: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}

	gamer->lastround = round;
	return(1);
}

static int
gamer_init_loadexpr(struct gamer *gamer)
{
	int	 	 rc;
	CURLcode	 cc;

	gamer->phase = PHASE_LOADEXPR;

	/* Start with the URL for the captive portal. */
	if ( ! gamer_init(gamer, urls[PHASE_LOADEXPR], 0)) {
		fputs("gamer_init", stderr);
		return(0);
	}

	rc = buf_write(&gamer->cookie, 
		"sessid=%s;sesscookie=%s", 
		gamer->sessid, gamer->sesscookie);
	if ( ! rc) {
		fputs("buf_write\n", stderr);
		return(0);
	}
	cc = curl_easy_setopt(gamer->conn, 
		CURLOPT_COOKIE, gamer->cookie.b);
        if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_COOKIE: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}

	return(1);
}

/*
 * Initialise a registration sequence.
 * This sets us up to try to `auto-add' to the simulation.
 * It uses our existing e-mail address.
 */
static int
gamer_init_register(struct gamer *gamer)
{
	CURLcode	 cc;

	gamer->phase = PHASE_REGISTER;

	/* Start with the URL for the captive portal. */
	if ( ! gamer_init(gamer, urls[PHASE_REGISTER], 1)) {
		fputs("gamer_init", stderr);
		return(0);
	}

	/* Our POST consists only of our email address. */
	if ( ! buf_write(&gamer->post, "email=%s", gamer->email)) {
		fputs("buf_write\n", stderr);
		return(0);
	}

	/* Add the POST field to the request. */
	cc = curl_easy_setopt(gamer->conn, 
		CURLOPT_POSTFIELDS, gamer->post.b);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_POSTFIELDS: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}
	return(1);
}

/*
 * Initialise a login sequence.
 * This tries to log us into the system given the registration
 * information as early initialised in gamer_init_register().
 */
static int
gamer_init_login(struct gamer *gamer)
{
	int	 	 c;
	CURLcode	 cc;

	gamer->phase = PHASE_LOGIN;

	if ( ! gamer_init(gamer, urls[PHASE_LOGIN], 1)) {
		fputs("gamer_init", stderr);
		return(0);
	}

	/* 
	 * Our POST consists of email and password. 
	 * TODO: make sure about URL encoding?
	 */
	c = buf_write(&gamer->post, "email=%s&password=%s", 
		gamer->email, gamer->password);
	if ( ! c) {
		fputs("buf_write\n", stderr);
		return(0);
	}

	/* Attach buffer to POST request. */
	cc = curl_easy_setopt(gamer->conn, 
		CURLOPT_POSTFIELDS, gamer->post.b);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_POSTFIELDS: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}

	/* We want to accept cookies for this coming request. */
	cc = curl_easy_setopt(gamer->conn, 
		CURLOPT_COOKIEFILE, "");
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_setopt: "
			"CURLOPT_COOKIEFILE: %s\n",
			curl_easy_strerror(cc));
		return(0);
	}

	return(1);
}

static int
gamer_phase_play(struct gamer *gamer)
{
	CURLcode	    cc;
	long		    code;

	/* First make sure we have HTTP code 200. */
	cc = curl_easy_getinfo(gamer->conn, 
		CURLINFO_RESPONSE_CODE, &code);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_getinfo: "
			"CURLINFO_RESPONSE_CODE: %s\n",
			curl_easy_strerror(cc));
		return(0);
	} else if (200 != code) {
		fprintf(stderr, "%s: bad response: %s -> %ld\n", 
			gamer->email, gamer->url, code);
		return(0);
	} else if (NULL != gamer->parsed) {
		fprintf(stderr, "%s: unexpected JSON: %s\n", 
			gamer->email, gamer->url);
		return(0);
	} else if ( ! gamer_reset(gamer)) {
		fputs("gamer_reset\n", stderr);
		return(0);
	} else if ( ! gamer_init_loadexpr(gamer)) {
		fputs("game_init_loadexpr", stderr);
		return(0);
	}

	if (gamer->game->verbose)
		fprintf(stderr, "%s: played %" PRId64 "\n",
			gamer->email, gamer->lastround);
	return(1);
}

static int
gamer_phase_loadexpr(struct gamer *gamer)
{
	CURLcode	    cc;
	long		    code;
	int64_t		    round, rounds;
	struct json_object *expr;

	/* First make sure we have HTTP code 200. */
	cc = curl_easy_getinfo(gamer->conn, 
		CURLINFO_RESPONSE_CODE, &code);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_getinfo: "
			"CURLINFO_RESPONSE_CODE: %s\n",
			curl_easy_strerror(cc));
		return(0);
	} else if (200 != code) {
		fprintf(stderr, "%s: bad response: %s -> %ld\n", 
			gamer->email, gamer->url, code);
		return(0);
	} else if ( ! json_check(gamer)) {
		fputs("json_check\n", stderr);
		return(0);
	}
	
	/*
	 * Examine the result.
	 * If we have the same round number as we did before, then keep
	 * `getting' the experiment again.
	 * If it's a new round, then try to play it.
	 */
	if ( ! json_getobj(gamer, gamer->parsed, &expr, "expr")) {
		fputs("json_getobj\n", stderr);
		return(0);
	} else if ( ! json_getint(gamer, expr, &round, "round")) {
		fputs("json_getint\n", stderr);
		return(0);
	} else if ( ! json_getint(gamer, expr, &rounds, "rounds")) {
		fputs("json_getint\n", stderr);
		return(0);
	}

	if (round < 0) {
		/* Experiment hasn't begun yet. */
		if ( ! gamer_reset(gamer)) {
			fputs("gamer_reset\n", stderr);
			return(0);
		} else if ( ! gamer_init_loadexpr(gamer)) {
			fputs("gamer_init_loadexpr", stderr);
			return(0);
		}
		return(1);
	} else if (round == rounds) {
		/* Experiment has finished. */
		assert(gamer->lastround < round);
		gamer->game->finished++;
		gamer->phase = PHASE__MAX;
		if (gamer->game->verbose)
			fprintf(stderr, "%s: done\n", gamer->email);
		return(1);
	} else if (gamer->lastround > round) {
		/* We played a round in the future...? */
		fprintf(stderr, "%s: time-warp: %s -> %" 
			PRId64 " > %" PRId64 "\n", gamer->email, 
			gamer->url, gamer->lastround, round);
		return(0);
	} else if (gamer->lastround < round) {
		/* Play the round. */
		if ( ! gamer_reset(gamer)) {
			fputs("gamer_reset\n", stderr);
			return(0);
		} else if ( ! gamer_init_play(gamer, round)) {
			fputs("game_init_play", stderr);
			return(0);
		}
		if (gamer->game->verbose)
			fprintf(stderr, "%s: entering round %" 
				PRId64 "\n", gamer->email, round);
		return(1);
	} 

	assert(round == gamer->lastround);
	assert(round >= 0 && round < rounds);
	/* We've already played the round. */
	if ( ! gamer_reset(gamer)) {
		fputs("gamer_reset\n", stderr);
		return(0);
	} else if ( ! gamer_init_loadexpr(gamer)) {
		fputs("gamer_init_loadexpr", stderr);
		return(0);
	}
	return(1);
}

/*
 * We've now logged in.
 * Accept our cookies.
 */
static int
gamer_phase_login(struct gamer *gamer)
{
	CURLcode	   cc;
	long		   code;
	struct curl_slist *cookies, *nc;
	struct cookie	   c;

	/* First make sure we have HTTP code 200. */
	cc = curl_easy_getinfo(gamer->conn, 
		CURLINFO_RESPONSE_CODE, &code);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_getinfo: "
			"CURLINFO_RESPONSE_CODE: %s\n",
			curl_easy_strerror(cc));
		return(0);
	} else if (409 == code) {
		if ( ! gamer_reset(gamer)) {
			fputs("gamer_reset\n", stderr);
			return(0);
		} else if ( ! gamer_init_login(gamer)) {
			fputs("gamer_init_login", stderr);
			return(0);
		}
		return(1);
	} else if (200 != code) {
		fprintf(stderr, "%s: bad response: %s -> %ld\n", 
			gamer->email, gamer->url, code);
		return(0);
	} else if (NULL != gamer->parsed) {
		fprintf(stderr, "%s: unexpected JSON: %s\n", 
			gamer->email, gamer->url);
		return(0);
	}

	/* Extract the Netscape-style cookie list. */
	cc = curl_easy_getinfo(gamer->conn,
		CURLINFO_COOKIELIST, &cookies);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_getinfo: "
			"CURLINFO_COOKIELIST: %s\n",
			curl_easy_strerror(cc));
		return(0);
	} 

	/*
	 * Loop through the cookies we've downloaded.
	 * Extract our session identifier and magic cookie value.
	 */
	for (nc = cookies; NULL != nc; nc = nc->next) {
		if ( ! cookie(&c, nc->data)) {
			fputs("cookie\n", stderr);
			return(0);
		}
		if (0 == strcmp(c.key, "sesscookie")) {
			free(gamer->sesscookie);
			gamer->sesscookie = strdup(c.val);
			if (NULL == gamer->sesscookie) {
				perror(NULL);
				curl_slist_free_all(cookies);
				return(0);
			}
		} else if (0 == strcmp(c.key, "sessid")) {
			free(gamer->sessid);
			gamer->sessid = strdup(c.val);
			if (NULL == gamer->sessid) {
				perror(NULL);
				curl_slist_free_all(cookies);
				return(0);
			}
		}
	}
	curl_slist_free_all(cookies);
	if (NULL == gamer->sesscookie || NULL == gamer->sessid) {
		fprintf(stderr, "%s: no session cookie "
			"or identifier\n", gamer->email);
		return(0);
	}
	
	/*
	 * Now, reset our connection, start us on the experiment itself,
	 * and enter the experiment phase.
	 */

	if ( ! gamer_reset(gamer)) {
		fputs("gamer_reset\n", stderr);
		return(0);
	} else if ( ! gamer_init_loadexpr(gamer)) {
		fputs("gamer_init_loadexpr", stderr);
		return(0);
	}
	if (gamer->game->verbose)
		fprintf(stderr, "%s: playing (session %s, "
			"cookie %s)\n", gamer->email,
			gamer->sessid, gamer->sesscookie);
	return(1);
}

/*
 * We've reached the captive portal registration site.
 * Now we process the response to see if we're in.
 */
static int
gamer_phase_register(struct gamer *gamer)
{
	CURLcode	    cc;
	long		    code;
	const char	   *mail, *pass;

	/* First make sure we have HTTP code 200. */
	cc = curl_easy_getinfo(gamer->conn, 
		CURLINFO_RESPONSE_CODE, &code);
	if (CURLE_OK != cc) {
		fprintf(stderr, "curl_easy_getinfo: "
			"CURLINFO_RESPONSE_CODE: %s\n",
			curl_easy_strerror(cc));
		return(0);
	} else if (200 != code) {
		fprintf(stderr, "%s: bad response: %s -> %ld\n", 
			gamer->email, gamer->url, code);
		return(0);
	} else if ( ! json_check(gamer)) {
		fputs("json_check\n", stderr);
		return(0);
	} 

	/* Make sure our email addresses match. */
	if ( ! json_getstring(gamer, gamer->parsed, &mail, "email")) {
		fputs("json_getstring\n", stderr);
		return(0);
	} else if (strcmp(mail, gamer->email)) {
		fprintf(stderr, "%s: email mismatch: %s\n",
			gamer->email, gamer->url);
		return(0);
	}

	/* Grok our password. */
	if ( ! json_getstring(gamer, gamer->parsed, &pass, "password")) {
		fputs("json_getstring\n", stderr);
		return(0);
	} else if (NULL == (gamer->password = strdup(pass))) {
		perror(NULL);
		return(0);
	}

	if ( ! gamer_reset(gamer)) {
		fputs("gamer_reset\n", stderr);
		return(0);
	} else if ( ! gamer_init_login(gamer)) {
		fputs("gamer_init_login", stderr);
		return(0);
	}

	/* Lastly, transfer us to the login phase. */
	if (gamer->game->verbose)
		fprintf(stderr, "%s: logged in\n", gamer->email);

	return(1);
}

/*
 * A gamer has had a connection end.
 * We switch on the gamer's phase to see what should happen.
 */
static int
gamer_event(struct gamer *gamer)
{
	int	 rc;

	switch (gamer->phase) {
	case (PHASE_REGISTER):
		rc = gamer_phase_register(gamer);
		break;
	case (PHASE_LOGIN):
		rc = gamer_phase_login(gamer);
		break;
	case (PHASE_LOADEXPR):
		rc = gamer_phase_loadexpr(gamer);
		break;
	case (PHASE_PLAY):
		rc = gamer_phase_play(gamer);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	if (NULL != gamer->parsed) {
		json_object_put(gamer->parsed);
		gamer->parsed = NULL;
	}
	json_tokener_reset(gamer->tok);
	return(rc);
}

int
main(int argc, char *argv[])
{
	int	 	 c, rc, maxfd, run, postrun;
	CURLMsg		*msg;
	struct timeval	 tv;
	CURLMcode	 cm;
	size_t		 i, sz, players;
	long		 timeo;
	char	 	*url;
	struct gamer	*ctx;
	struct game	 game;
	fd_set		 rset, wset, eset;

	rc = 0;
	players = 2;
	memset(&game, 0, sizeof(struct game));

	while (-1 != (c = getopt(argc, argv, "n:v"))) 
		switch (c) {
		case ('v'):
			game.verbose = 1;
			break;
		case ('n'):
			players = atoi(optarg);
			break;
		default:
			goto usage;
		}

	if (players < 2) {
		fputs("-n: need a value >2\n", stderr);
		goto usage;
	}

	argc -= optind;
	argv += optind;

	if (0 == argc)
		goto usage;

	url = *argv++;
	argc--;

	if (0 == (sz = strlen(url))) {
		fputs("zero-length url\n", stderr);
		goto usage;
	} else if ('/' == url[sz - 1])
		url[sz - 1] = '\0';

	/*
	 * Create the URLs used for each phase of the simulation.
	 * These are created from the base URL.
	 */
	c = asprintf(&urls[PHASE_REGISTER], "%s/doautoadd.json", url);
	if (c < 0) {
		perror(NULL);
		return(EXIT_FAILURE);
	}
	c = asprintf(&urls[PHASE_LOGIN], "%s/dologin.json", url);
	if (c < 0) {
		perror(NULL);
		free(urls[PHASE_REGISTER]);
		return(EXIT_FAILURE);
	}
	c = asprintf(&urls[PHASE_LOADEXPR], "%s/doloadexpr.json", url);
	if (c < 0) {
		perror(NULL);
		free(urls[PHASE_REGISTER]);
		free(urls[PHASE_LOGIN]);
		return(EXIT_FAILURE);
	}
	c = asprintf(&urls[PHASE_PLAY], "%s/doplay.json", url);
	if (c < 0) {
		perror(NULL);
		free(urls[PHASE_REGISTER]);
		free(urls[PHASE_LOGIN]);
		free(urls[PHASE_LOADEXPR]);
		return(EXIT_FAILURE);
	}

	/*
	 * Begin by initialising the whole system.
	 * We won't have enough state to use the `out' label, so free
	 * memory and state as we go.
	 */
	if (NULL == (ctx = calloc(players, sizeof(struct gamer)))) {
		perror(NULL);
		return(EXIT_FAILURE);
	} else if (CURLE_OK != curl_global_init(CURL_GLOBAL_ALL)) {
		fputs("curl_global_init\n", stderr);
		free(ctx);
		return(EXIT_FAILURE);
	} else if (NULL == (game.curl = curl_multi_init())) {
		fputs("curl_global_init\n", stderr);
		curl_global_cleanup();
		free(ctx);
		return(EXIT_FAILURE);
	}

	/*
	 * Now initialise each handle.
	 * We have one handle per `player' in the `game'.
	 * This will allow players to act independently of one another.
	 */
	for (i = 0; i < players; i++) {
		if (NULL == (ctx[i].conn = curl_easy_init())) {
			fputs("curl_easy_init\n", stderr);
			goto out;
		}
		cm = curl_multi_add_handle(game.curl, ctx[i].conn);
		if (CURLM_OK != cm) {
			fprintf(stderr, "curl_multi_add_handle: %s\n",
				curl_multi_strerror(cm));
			curl_easy_cleanup(ctx[i].conn);
			ctx[i].conn = NULL;
			goto out;
		} else if (NULL == (ctx[i].tok = json_tokener_new())) {
			fputs("json_tokener_new", stderr);
			goto out;
		}

		ctx[i].lastround = -1;
		ctx[i].game = &game;

		/* E-mail address: pXX@foo.com. */
		if (asprintf(&ctx[i].email, "p%zu@foo.com", i) < 0) {
			perror(NULL);
			goto out;
		}

		/* Start with the URL for the captive portal. */
		if ( ! gamer_init_register(&ctx[i])) {
			fputs("gamer_init_register", stderr);
			goto out;
		}

		if (game.verbose)
			fprintf(stderr, "%s: trying to "
				"log in\n", ctx[i].email);
	}

	run = -1;
	/*
	 * Event loop.
	 * Prime our file descriptors for active connections then poll
	 * the server for data to be transferred.
	 * When a connection has been completed for any given player, we
	 * pass that into gamer_event() for processing.
	 */
	do {
		FD_ZERO(&eset);
		FD_ZERO(&rset);
		FD_ZERO(&wset);

		/* 
		 * Get file descriptors. 
		 * These will be maxfd=-1 when we're still preparing
		 * connections in the very beginning!
		 */
		cm = curl_multi_fdset(game.curl, 
			&rset, &wset, &eset, &maxfd);
		if (CURLM_OK != cm) {
			fprintf(stderr, "curl_multi_fdset: %s\n",
				curl_multi_strerror(cm));
			goto out;
		} 

		/* 
		 * How long should we wait?
		 * If we're told to wait infinitely, then give us a
		 * hard-coded wait time of five seconds.
		 */
		cm = curl_multi_timeout(game.curl, &timeo);
		if (CURLM_OK != cm) {
			fprintf(stderr, "curl_multi_timeout: %s\n",
				curl_multi_strerror(cm));
			goto out;
		} else if (-1 == timeo) 
			timeo = 5000;

		/*
		 * Poll on file descriptors.
		 * We don't do this if we have no file descriptors
		 * (happens during lookups, etc.) or when we have a
		 * timeout of zero, which means we need to read data
		 * now.
		 */
		if (-1 != maxfd && timeo > 0) {
			maxfd++;
			memset(&tv, 0, sizeof(struct timeval));
			tv.tv_sec = timeo / 1000;
			timeo -= tv.tv_sec * 1000;
			tv.tv_usec = timeo <= 0 ? 0 : timeo * 1000;
			c = select(maxfd, &rset, &wset, &eset, &tv);
			if (c < 0) {
				perror("select");
				goto out;
			}
		}

		/* Actually transfer the data. */
		do {
			cm = curl_multi_perform(game.curl, &postrun);
			if (CURLM_CALL_MULTI_PERFORM == cm ||
			    CURLM_OK == cm) 
				continue;
			fprintf(stderr, "curl_multi_perform: %s\n", 
				curl_multi_strerror(cm));
			goto out;
		} while (CURLM_CALL_MULTI_PERFORM == cm);

		/* No connections have completed. */
		if (-1 != run && run == postrun) 
			continue;

		/* 
		 * Connections have completed. 
		 * Loop through to find the connection, map the
		 * connection to a gamer, then process.
		 */
		for (;;) {
			msg = curl_multi_info_read(game.curl, &c);
			if (NULL == msg)
				break;
			for (i = 0; i < players; i++)
				if (ctx[i].conn == msg->easy_handle)
					break;
			assert(i < players);
			if (CURLMSG_DONE != msg->msg) {
				fputs("transfer error\n", stderr);
				goto out;
			} else if ( ! gamer_event(&ctx[i])) {
				fputs("gamer_event\n", stderr);
				goto out;
			}
		}
		run = postrun;
	} while (game.finished < players);

	rc = 1;
out:
	/* Memory cleanups and exiting. */
	for (i = 0; i < players; i++) {
		if (NULL != ctx[i].conn) {
			curl_multi_remove_handle(game.curl, ctx[i].conn);
			curl_easy_cleanup(ctx[i].conn);
		}
		free(ctx[i].email);
		free(ctx[i].password);
		free(ctx[i].sessid);
		free(ctx[i].sesscookie);
		free(ctx[i].post.b);
		free(ctx[i].cookie.b);
		json_tokener_free(ctx[i].tok);
		if (NULL != ctx[i].parsed)
			json_object_put(ctx[i].parsed);
	}

	for (i = 0; i < PHASE__MAX; i++)
		free(urls[i]);

	free(ctx);
	curl_multi_cleanup(game.curl);
	curl_global_cleanup();
	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);
usage:
	fprintf(stderr, "usage: %s [-v] url\n", getprogname());
	return(EXIT_FAILURE);
}
