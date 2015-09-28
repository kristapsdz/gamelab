/*	$Id$ */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
	fprintf(stderr, "[info] %s:%zu: ", file, line);
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
	fprintf(stderr, "[warn] %s:%zu: ", file, line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
}
