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
#include <errno.h>
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

#include "extern.h"

void
dbg_info(const char *file, size_t line, const char *fmt, ...)
{
	va_list	 ap;
#ifdef LOGTIME
	char	 buf[32];
	time_t	 t;
	size_t	 sz;
#endif
#ifdef	LOGTIME
	t = time(NULL);
	ctime_r(&t, buf);
	sz = strlen(buf);
	buf[sz - 1] = '\0';
	fprintf(stderr, "[%s] ", buf);
#endif
	fprintf(stderr, "[gamelab-info] %s:%zu: ", file, line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
}

void
dbg_warn(const char *file, size_t line, const char *fmt, ...)
{
	va_list	 ap;
#ifdef LOGTIME
	char	 buf[32];
	time_t	 t;
	size_t	 sz;
#endif
#ifdef	LOGTIME
	t = time(NULL);
	ctime_r(&t, buf);
	sz = strlen(buf);
	buf[sz - 1] = '\0';
	fprintf(stderr, "[%s] ", buf);
#endif
	fprintf(stderr, "[gamelab-WARN] %s:%zu: ", file, line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
}

void
dbg_warnx(const char *file, size_t line, const char *fmt, ...)
{
	va_list	 ap;
	int	 er = errno;
#ifdef LOGTIME
	char	 buf[32];
	time_t	 t;
	size_t	 sz;
#endif
#ifdef	LOGTIME
	t = time(NULL);
	ctime_r(&t, buf);
	sz = strlen(buf);
	buf[sz - 1] = '\0';
	fprintf(stderr, "[%s] ", buf);
#endif
	fprintf(stderr, "[gamelab-WARN] %s:%zu: ", file, line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(er));
	fflush(stderr);
}
