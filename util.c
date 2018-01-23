/*	$Id$ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@kcons.eu>
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
#include <sys/wait.h>

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

/*
 * The "double-fork" is a well-known technique to start a long-running
 * process.
 * A process is created that invokes daemon(3), which in turk forks
 * internally.
 * This returns -1 if it's in the caller process and something bad
 * happened, 0 if it's in the child "long-running" process, and 1 if
 * it's in the caller process.
 * The "middle" process never returns.
 */
int
doublefork(struct kreq *r)
{
	pid_t	 pid;

	db_close();
	if (-1 == (pid = fork())) {
		WARN("fork");
		return(-1);
	} else if (pid > 0) {
		if (-1 == waitpid(pid, NULL, 0)) {
			WARN("waitpid");
			return(-1);
		}
		return(1);
	}
	khttp_child_free(r);
	if (-1 == daemon(1, 1)) {
		WARN("daemon");
		exit(EXIT_SUCCESS);
	} 
	return(0);
}


