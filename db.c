/*	$Id$ */
/*
 * Copyright (c) 2014--2016 Kristaps Dzonsons <kristaps@kcons.eu>
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
#include <sys/param.h>

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <bsd/stdlib.h>
#endif
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <kcgi.h>
#include <kcgijson.h>
#include <gmp.h>
#include <sqlite3.h>

#include "extern.h"

#define DB_STEP_CONSTRAINT 0x01
#define	PLAYER	"player.email,player.state,player.id,player.enabled," \
		"player.role,player.rseed,player.instr," \
		"player.finalrank,player.finalscore,player.autoadd," \
		"player.version,player.joined,player.answer," \
		"player.hitid,player.assignmentid,player.hash," \
		"player.mturkdone"
#define	GAME	"game.p1,game.p2,game.payoffs,game.name,game.id"

/*
 * The database, its location, and its statement (if any).
 * This is opened on-demand (see db_tryopen()) and closed automatically
 * on system exit (see the atexit(3) call).
 */
static sqlite3		*db;

/*
 * This should be called via atexit() or manually invoked.
 */
void
db_close(void)
{

	if (NULL == db)
		return;
	if (SQLITE_OK != sqlite3_close(db))
		WARNX("sqlite3_close: %s", sqlite3_errmsg(db));
	db = NULL;
}

static void
db_sleep(size_t attempt)
{

	if (attempt < 10)
		usleep(arc4random_uniform(100000));
	else
		usleep(arc4random_uniform(400000));
}

static void
db_tryopen(void)
{
	size_t	 attempt;
	int	 rc;

	if (NULL != db)
		return;

	/* Register exit hook for the destruction of the database. */
	if (-1 == atexit(db_close)) {
		WARN("atexit");
		exit(EXIT_FAILURE);
	}

	attempt = 0;
again:
	rc = sqlite3_open(DATADIR "/gamelab.db", &db);
	if (SQLITE_BUSY == rc) {
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		WARNX("sqlite3_step: %s", sqlite3_errmsg(db));
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_PROTOCOL == rc) {
		WARNX("sqlite3_step: %s", sqlite3_errmsg(db));
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_OK == rc) {
		sqlite3_busy_timeout(db, 500);
		return;
	} 

	WARNX("sqlite3_open: %s", sqlite3_errmsg(db));
	exit(EXIT_FAILURE);
}

static int
db_step(sqlite3_stmt *stmt, unsigned int flags)
{
	int	 rc;
	size_t	 attempt = 0;

	assert(NULL != stmt);
	assert(NULL != db);
again:
	rc = sqlite3_step(stmt);
	if (SQLITE_BUSY == rc) {
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		WARNX("sqlite3_step: %s", sqlite3_errmsg(db));
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_PROTOCOL == rc) {
		WARNX("sqlite3_step: %s", sqlite3_errmsg(db));
		db_sleep(attempt++);
		goto again;
	}

	if (SQLITE_DONE == rc || SQLITE_ROW == rc)
		return(rc);
	if (SQLITE_CONSTRAINT == rc && DB_STEP_CONSTRAINT & flags)
		return(rc);

	WARNX("sqlite3_step: %s", sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	exit(EXIT_FAILURE);
}

static sqlite3_stmt *
db_stmt(const char *sql)
{
	sqlite3_stmt	*stmt;
	size_t		 attempt = 0;
	int		 rc;

	db_tryopen();
again:
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

	if (SQLITE_BUSY == rc) {
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(db));
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_PROTOCOL == rc) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(db));
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_OK == rc)
		return(stmt);

	WARNX("sqlite3_prepare_v2: %s (%s)", sqlite3_errmsg(db), sql);
	sqlite3_finalize(stmt);
	exit(EXIT_FAILURE);
}

static void
db_bind_text(sqlite3_stmt *stmt, size_t pos, const char *val)
{

	assert(pos > 0);
	if (SQLITE_OK == sqlite3_bind_text
		(stmt, pos, val, -1, SQLITE_STATIC))
		return;
	WARNX("sqlite3_bind_text: %s", sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	exit(EXIT_FAILURE);
}

static void
db_bind_double(sqlite3_stmt *stmt, size_t pos, double val)
{

	assert(pos > 0);
	if (SQLITE_OK == sqlite3_bind_double(stmt, pos, val))
		return;
	WARNX("sqlite3_bind_double: %s", sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	exit(EXIT_FAILURE);
}

static void
db_bind_int(sqlite3_stmt *stmt, size_t pos, int64_t val)
{

	assert(pos > 0);
	if (SQLITE_OK == sqlite3_bind_int64(stmt, pos, val))
		return;
	WARNX("sqlite3_bind_int64: %s", sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	exit(EXIT_FAILURE);
}

static void
db_exec(const char *sql)
{
	size_t	attempt = 0;
	int	rc;

again:
	assert(NULL != db);
	rc = sqlite3_exec(db, sql, NULL, NULL, NULL);

	if (SQLITE_BUSY == rc) {
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		WARNX("sqlite3_exec: %s", sqlite3_errmsg(db));
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_PROTOCOL == rc) {
		WARNX("sqlite3_exec: %s", sqlite3_errmsg(db));
		db_sleep(attempt++);
		goto again;
	} else if (SQLITE_OK == rc)
		return;

	WARNX("sqlite3_exec: %s (%s)", sqlite3_errmsg(db), sql);
	exit(EXIT_FAILURE);
}

static void
db_trans_begin(int immediate)
{

	db_tryopen();
	if (0 == immediate)
		db_exec("BEGIN TRANSACTION");
	else 
		db_exec("BEGIN IMMEDIATE");
}

static void
db_trans_commit(void)
{

	db_exec("COMMIT TRANSACTION");
}

static void
db_trans_rollback(void)
{

	db_exec("ROLLBACK TRANSACTION");
}

static size_t
db_count_all(const char *p)
{
	sqlite3_stmt	*stmt;
	char		 buf[1024];
	int64_t		 count;

	(void)snprintf(buf, sizeof(buf),
		"SELECT count(*) FROM %s", p);
	stmt = db_stmt(buf);
	db_step(stmt, 0);
	count = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	assert(count >= 0 && (uint64_t)count < SIZE_MAX);
	return(count);
}

/*
 * We never really delete session records, as they contain valuable
 * information which we can later reference.
 * We simply zero the magic number associated with the session.
 */
void
db_sess_delete(int64_t id)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt
		("UPDATE sess SET cookie=0 WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
}

/*
 * See if the session for a player is valid.
 * We never accept cookies being zero, as it means that the cookie has
 * been invalidated.
 */
int
db_player_sess_valid(int64_t *playerid, int64_t id, int64_t cookie)
{
	int	 	 rc;
	sqlite3_stmt	*stmt;

	if (0 == cookie) {
		WARNX("Player login of %"
			PRId64 " without cookie", id);
		return(0);
	}

	stmt = db_stmt
		("SELECT playerid FROM sess "
		 "INNER JOIN player ON player.id=sess.playerid "
		 "WHERE sess.id=? AND sess.cookie=? AND "
		 "sess.playerid IS NOT NULL AND player.enabled=1");
	db_bind_int(stmt, 1, id);
	db_bind_int(stmt, 2, cookie);
	if (SQLITE_ROW == (rc = db_step(stmt, 0))) {
		*playerid = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
		db_player_set_state(*playerid, PSTATE_LOGGEDIN);
	} else
		sqlite3_finalize(stmt);

	return(SQLITE_ROW == rc);
}

int
db_admin_sess_valid(int64_t id, int64_t cookie)
{
	int	 	 rc;
	sqlite3_stmt	*stmt;

	if (0 == cookie) {
		WARNX("Administrator login of %" 
			PRId64 " without cookie", id);
		return(0);
	}

	stmt = db_stmt
		("SELECT * FROM sess "
		 "WHERE id=? AND cookie=? AND playerid IS NULL");
	db_bind_int(stmt, 1, id);
	db_bind_int(stmt, 2, cookie);
	rc = db_step(stmt, 0);
	sqlite3_finalize(stmt);
	return(SQLITE_ROW == rc);
}

struct sess *
db_player_sess_alloc(int64_t playerid, const char *useragent)
{
	sqlite3_stmt	*stmt;
	struct sess	*sess;

	sess = kcalloc(1, sizeof(struct sess));
	/* Never let the cookie be zero. */
	do 
		sess->cookie = arc4random();
	while (0 == sess->cookie);
	stmt = db_stmt("INSERT INTO sess "
		"(cookie,playerid,useragent,created) "
		"VALUES(?,?,?,?)");
	db_bind_int(stmt, 1, sess->cookie);
	db_bind_int(stmt, 2, playerid);
	db_bind_text(stmt, 3, useragent);
	db_bind_int(stmt, 4, time(NULL));
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	sess->id = sqlite3_last_insert_rowid(db);
	INFO("Player %" PRId64 " logged in, "
		"session %" PRId64, playerid, sess->id);
	return(sess);
}

struct sess *
db_admin_sess_alloc(const char *useragent)
{
	sqlite3_stmt	*stmt;
	struct sess	*sess;

	sess = kcalloc(1, sizeof(struct sess));
	/* Never let the cookie be zero. */
	do
		sess->cookie = arc4random();
	while (0 == sess->cookie);
	stmt = db_stmt("INSERT INTO sess "
		"(cookie,useragent,created) "
		"VALUES(?,?,?)");
	db_bind_int(stmt, 1, sess->cookie);
	db_bind_text(stmt, 2, useragent);
	db_bind_int(stmt, 3, time(NULL));
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	sess->id = sqlite3_last_insert_rowid(db);
	INFO("Administrator logged in, "
		"session %" PRId64, sess->id);
	return(sess);
}

void
db_sess_free(struct sess *sess)
{

	free(sess);
}

static char *
db_crypt_mkpass(void)
{
	char	*pass;
	size_t	 i;
	
	pass = kmalloc(9);
	for (i = 0; i < 8; i++) {
		if (i < 3) 
			pass[i] = (arc4random() % 26) + 97;
		else if (i == 3)
			pass[i] = '-';
		else
			pass[i] = (arc4random() % 10) + 48;
	}
	pass[i] = '\0';
	return(pass);
}

static int
db_crypt_check(const unsigned char *hash, const char *pass)
{

	return(0 == strcmp((char *)hash, pass));
}

void
db_admin_set_pass(const char *pass)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE admin SET hash=?,isset=isset|2");
	db_bind_text(stmt, 1, pass);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator set password");
}

void
db_winners_free(struct winner *win)
{

	if (NULL == win)
		return;
	free(win);
}

/*
 * Advance to the next round IFF the experiment has already started and
 * we're in a valid round already.
 */
void
db_expr_advanceend(void)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE experiment SET "
		"round=rounds,roundbegan=? WHERE "
		"round < rounds AND round >= 0");
	db_bind_int(stmt, 1, time(NULL));
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Round terminus (attempt) manually");
}

/*
 * Advance to the next round IFF the experiment has already started and
 * we're in a valid round already.
 */
void
db_expr_advancenext(void)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE experiment SET "
		"round=round + 1,roundbegan=? WHERE "
		"round < rounds AND round >= 0");
	db_bind_int(stmt, 1, time(NULL));
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Round advanced (attempt) manually");
}

/*
 * This is one of the more important functions.
 * It is called often (once per invocation) by both the administrator
 * and players to advance the round.
 * Rounds either advance according to time or the percentage of players
 * per role who have played all games.
 * Returns 1 if we advanced the round, 0 otherwise.
 */
int
db_expr_advance(void)
{
	struct expr	*expr;
	time_t		 t;
	int		 rc, advanced;
	size_t		 games, allplayers[2], roleplayers[2];
	double		 playerf[2];
	int64_t		 round, played, tmp;
	sqlite3_stmt	*stmt;

	/* 
	 * Do nothing if we've not started. 
	 */
	if (NULL == (expr = db_expr_get(1)))
		return(0);

	/* 
	 * Do nothing if we're not running or have finished. 
	 */
	if (expr->round >= expr->rounds) {
		db_expr_free(expr);
		return(0);
	} else if ((t = time(NULL)) < expr->start) {
		db_expr_free(expr);
		return(0);
	} 

	/*
	 * Optional round advancement according to the number of players
	 * per role who have played all games.
	 */
	if (expr->roundpct > 0.0 && 
		 expr->round >= 0 &&
		 t - expr->roundbegan > expr->roundmin * 60) {
		/*
		 * Determine how many players exist per role.
		 * This only works with players who are currently in the
		 * play role, not in the lobby (or finished).
		 */
		stmt = db_stmt("SELECT count(*) FROM "
			"player WHERE role=? AND "
			"joined >= 0 AND "
			"joined <= ? AND "
			"? < joined + ?");
		db_bind_int(stmt, 1, 0);
		db_bind_int(stmt, 2, expr->round);
		db_bind_int(stmt, 3, expr->round);
		db_bind_int(stmt, 4, expr->prounds);
		rc = db_step(stmt, 0);
		assert(SQLITE_ROW == rc);
		tmp = sqlite3_column_int64(stmt, 0);
		sqlite3_reset(stmt);
		assert(tmp >= 0 && (uint64_t)tmp < SIZE_MAX);
		if (0 == (allplayers[0] = tmp)) {
			sqlite3_finalize(stmt);
			goto fallthrough;
		}

		db_bind_int(stmt, 1, 1);
		db_bind_int(stmt, 2, expr->round);
		db_bind_int(stmt, 3, expr->round);
		db_bind_int(stmt, 4, expr->prounds);
		rc = db_step(stmt, 0);
		assert(SQLITE_ROW == rc);
		tmp = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
		assert(tmp >= 0 && (uint64_t)tmp < SIZE_MAX);
		/*
		 * FIXME: is this the right thing to do?
		 * Would we rather wait for people to join?
		 * Or should we have a "wait" timer as well?
		 */
		if (0 == (allplayers[1] = tmp))
			goto fallthrough;

		/*
		 * Compute the per-player-role number of players we have
		 * available to play in this round.
		 */
		games = db_game_count_all();
		stmt = db_stmt("SELECT player.role FROM player "
			"INNER JOIN gameplay ON "
			" player.id = gameplay.playerid "
			"WHERE gameplay.round=? AND "
			" gameplay.choices=?");
		db_bind_int(stmt, 1, expr->round);
		db_bind_int(stmt, 2, games);
		roleplayers[0] = roleplayers[1] = 0;
		while (SQLITE_ROW == db_step(stmt, 0)) {
			played = sqlite3_column_int64(stmt, 0);
			assert(played == 0 || played == 1);
			roleplayers[played]++;
		}
		sqlite3_finalize(stmt);

		playerf[0] = roleplayers[0] / (double)allplayers[0];
		playerf[1] = roleplayers[1] / (double)allplayers[1];

		if (playerf[0] >= expr->roundpct &&
			 playerf[1] >= expr->roundpct) {
			INFO("Round-advance: (at %" 
				PRId64 "): fraction exceeded "
				"(%g >= %g, %g >= %g)", expr->round, 
				playerf[0], expr->roundpct,
				playerf[1], expr->roundpct);
			round = expr->round + 1;
			goto advance;
		}
	} 
fallthrough:

	/*
	 * Round advancement according to the system time.
	 * (This is the fallback for the percentage case.)
	 * Obviously this is sensitive to time(2) returning the
	 * correct value!
	 * However, this won't allow rounds to regress.
	 */
	if (expr->round >= 0) 
		round = expr->round +
			(t - expr->roundbegan) / (expr->minutes * 60);
	else 
		round = (t - expr->start) / (expr->minutes * 60);

	if (round > expr->rounds)
		round = expr->rounds;

	if (round == expr->round) {
		db_expr_free(expr);
		return(0);
	} else if (t < expr->roundbegan) {
		WARNX("Round-advance: time warp!");
		db_expr_free(expr);
		return(0);
	} 


advance:
	db_expr_free(expr);
	/* 
	 * At this exact point, our computed round is ahead of the
	 * experiment's round.
	 * Try to increment it, not clobbering existing people.
	 */
	stmt = db_stmt("UPDATE experiment SET round=?,roundbegan=?");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, time(NULL));

	advanced = 0;
	db_trans_begin(1);
	expr = db_expr_get(1);
	assert(NULL != expr);
	if (round < expr->round) {
		db_trans_rollback();
		WARNX("Round-advance: time warp (commit): "
			"computed %" PRId64 " have %"
			PRId64, expr->round, round);
	} else if (round == expr->round) {
		db_trans_rollback();
		INFO("Round-advance: to %" 
			PRId64 " during break", round);
	} else {
		db_step(stmt, 0);
		db_trans_commit();
		advanced = 1;
		if (0 == round)
			INFO("Round-advance: start of experiment");
		else if (round == expr->rounds)
			INFO("Round-advance: end of experiment");
	}
	sqlite3_finalize(stmt);
	db_expr_free(expr);
	return(advanced);
}

struct winner *
db_winners_get(int64_t playerid)
{
	sqlite3_stmt	*stmt;
	struct winner	*win = NULL;

	stmt = db_stmt("SELECT winrank,rnum FROM "
		"winner WHERE playerid=? AND winner=1");
	db_bind_int(stmt, 1, playerid);
	if (SQLITE_ROW == db_step(stmt, 0))  {
		win = kcalloc(1, sizeof(struct winner));
		win->rank = sqlite3_column_int64(stmt, 0);
		win->rnum = sqlite3_column_int64(stmt, 1);
	}
	sqlite3_finalize(stmt);
	return(win);
}

void
db_winners_load_all(void *arg, winnerf fp)
{
	sqlite3_stmt	*stmt;
	int64_t		 id;
	struct player	*p;
	struct winner	 win;

	stmt = db_stmt("SELECT playerid,winrank,rnum "
		"FROM winner WHERE winner=1 "
		"ORDER BY winrank ASC");

	while (SQLITE_ROW == db_step(stmt, 0)) {
		memset(&win, 0, sizeof(struct winner));
		id = sqlite3_column_int64(stmt, 0);
		win.rank = sqlite3_column_int64(stmt, 1);
		win.rnum = sqlite3_column_int64(stmt, 2);
		p = db_player_load(id);
		assert(NULL != p);
		fp(p, &win, arg);
		db_player_free(p);
	}
	sqlite3_finalize(stmt);
}

/*
 * This computes the ranking and final score (points) awarded to
 * individuals who are playing the lottery; i.e., all players who are
 * not mechanical turk players.
 * Once finished, the experiment is set to EXPR_PREWIN.
 */
void
db_expr_finish(struct expr **expr, size_t count)
{
	sqlite3_stmt	*stmt;
	size_t		 i, j, players;
	int64_t		*pids;
	int64_t	 	 total, score, min;
	mpq_t		 sum, cmp;

again:
	/* 
	 * Reload the experiment and see if we've already tallied. 
	 * This might still be simultaneous, however.
	 * Check that we're after the last round, in which case we won't
	 * be joined by late-coming players.
	 */

	db_expr_free(*expr);
	*expr = db_expr_get(1);
	assert(NULL != *expr);
	if ((*expr)->round < (*expr)->rounds) {
		WARNX("Trying to compute lottery tickets "
		      "when rounds not completed");
		return;
	} else if ((*expr)->state >= ESTATE_PREWIN)
		return;

	mpq_init(sum);
	mpq_init(cmp);
	players = db_player_count_all(); /* max players */
	pids = kcalloc(players, sizeof(int64_t));

	/* 
	 * Start tallying all the players' payouts.
	 * Don't worry about simultaneous updates because the tally will
	 * be the same in each one.
	 * Use only Mechanical Turk players.
	 */

	stmt = db_stmt("SELECT id FROM player WHERE '' == hitid");
	for (i = j = 0; SQLITE_ROW == db_step(stmt, 0); i++, j++) {
		assert(i < players);
		pids[i] = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	assert(i <= players);
	players = j;
	
	if (0 == players)
		WARNX("No (non-mturk) lottery players for "
			"computing final scores or rankings");

	if (players) {
		/*
		 * First, compute the minimum lottery ticket.
		 * This will establish whether we need to offset
		 * negative values.
		 * To do so, we need to force all lottery values to be
		 * computed: to date, they might not be there.
		 */
		INFO("Forcing lottery computation at end of game...");
		for (i = 0; i < players; i++) {
			mpq_clear(cmp);
			mpq_clear(sum);
			db_player_lottery((*expr)->rounds - 1, 
				pids[i], cmp, sum, &score, count);
		}

		/* 
		 * Now get the minimum ticket.
		 * If we have a positive ticket, use the standard
		 * lottery method by zeroing the offset.
		 */

		stmt = db_stmt
			("SELECT MIN(aggrtickets) FROM lottery "
			 "INNER JOIN player ON player.id=playerid "
			 "WHERE '' == player.hitid "
			 "AND lottery.round = ?");
		db_bind_int(stmt, 1, (*expr)->rounds - 1);
		db_step(stmt, 0);
		min = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
		if (min >= 0)
			min = 0;
		INFO("Payoffs offset (check for negative "
			"payoffs) is at %" PRId64, min);
	} else
		min = 0;

	/*
	 * Set the rank of each player with respect to the total number
	 * of accumulated tickets, taking into account a negative offset
	 * if we're playing a negative lottery.
	 * So if we have 100 total tickets with 10 players, the first
	 * will have x, then the second x + y, then x + y + z, etc.
	 * If we have any negative tickets, offset all tickets by the
	 * minimum negative.
	 * NOTE: we're rounding up!
	 */

	stmt = db_stmt("UPDATE player SET "
		"finalrank=?,finalscore=?,version=version+1 "
	        "WHERE id=?");
	for (total = 0, i = 0; i < players; i++) {
		mpq_clear(cmp);
		mpq_clear(sum);
		db_player_lottery((*expr)->rounds - 1, 
			pids[i], cmp, sum, &score, count);
		/* Offset negative (or zero). */
		score -= min;
		db_bind_int(stmt, 1, total);
		assert(score >= 0);
		db_bind_int(stmt, 2, score);
		db_bind_int(stmt, 3, pids[i]);
		db_step(stmt, 0);
		sqlite3_reset(stmt);
		total += score;
	}
	sqlite3_finalize(stmt);

	/*
	 * Store that we've created our total but haven't yet computed
	 * the winners.
	 */

	INFO("Total lottery tickets: %" PRId64, total);
	stmt = db_stmt("UPDATE experiment SET state=?,total=?");
	db_bind_int(stmt, 1, ESTATE_PREWIN);
	db_bind_int(stmt, 2, total);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);

	mpq_clear(sum);
	mpq_clear(cmp);
	free(pids);
	goto again;
}

/*
 * Compute "winnersz" number of lottery winners, using "seed".
 * The number of games is "count".
 * This only selects from non-Mechanical Turk players.
 */
int
db_winners(struct expr **expr, size_t winnersz, int64_t seed, size_t count)
{
	enum estate	 state;
	sqlite3_stmt	*stmt;
	size_t		 i, j, players;
	int64_t		 id, sum, top, score;
	int64_t		*pids, *winners, *rnums;
	mpq_t		 div, aggr;

	/* Compute the total count of players. */

	db_expr_finish(expr, count);

	/* See if we already have winners computed. */

	db_trans_begin(0);

	stmt = db_stmt("SELECT state FROM experiment");
	db_step(stmt, 0);
	state = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	if (ESTATE_POSTWIN == state) {
		INFO("Win request when experiment already winnered");
		db_trans_rollback();
		return(1);
	} else if ((*expr)->total <= 0) {
		INFO("Win request when experiment has <=0 "
			"tickets: %" PRId64, (*expr)->total);
		db_trans_rollback();
		return(0);
	}

	/* Disallow more winners than players. */

	players = db_player_count_all();
	winners = kcalloc(players, sizeof(int64_t));
	rnums = kcalloc(winnersz, sizeof(int64_t));
	pids = kcalloc(players, sizeof(int64_t));
	mpq_init(div);
	mpq_init(aggr);

	/* 
	 * Assign player identifiers.
	 * Only draw from non-Mechanical Turk players.
	 * Then reset our number of players to match the non-Mechanical
	 * turk players and the winners to be less than that.
	 */

	stmt = db_stmt("SELECT id FROM player WHERE '' == hitid");
	for (i = j = 0; SQLITE_ROW == db_step(stmt, 0); i++, j++) {
		assert(i < players);
		pids[i] = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	players = j;
	if (winnersz > players)
		winnersz = players;

	/*
	 * Here we're going to use random(), which is deterministic,
	 * with the seed supplied by the administrator.
	 * This yields repeatable results when given the same random
	 * seed, which is exactly what we want.
	 */
	srandom(seed);
	stmt = db_stmt("SELECT id,finalrank,finalscore FROM player "
		"WHERE '' == hitid ORDER BY rseed ASC, id ASC ");
	for (i = 0; i < winnersz; i++) {
		/* coverity[dont_call] */
		top = random() % (*expr)->total;
		INFO("Winning ticket: %" PRId64, top);
		id = 0;
		while (SQLITE_ROW == db_step(stmt, 0)) {
			id = sqlite3_column_int64(stmt, 0);
			sum = sqlite3_column_int64(stmt, 1);
			score = sqlite3_column_int64(stmt, 2);
			assert(score >= 0);
			if (top >= sum && top < sum + score)
				break;
		}
		sqlite3_reset(stmt);
		assert(0 != id);
		for (j = 0; j < i; j++)
			if (winners[j] == id)
				break;
		if (j < i) {
			i--;
		} else {
			winners[i] = id;
			rnums[i] = top;
			INFO("Winner: player %" PRId64, id);
		}
	}
	sqlite3_finalize(stmt);

	stmt = db_stmt("INSERT INTO winner "
		"(playerid,winner,winrank,rnum) VALUES (?,?,?,?)");
	for (i = 0; i < players; i++) {
		db_bind_int(stmt, 1, pids[i]);
		for (j = 0; j < winnersz; j++)
			if (winners[j] == pids[i])
				break;
		if (j < winnersz) {
			db_bind_int(stmt, 2, 1);
			db_bind_int(stmt, 3, j);
			db_bind_int(stmt, 4, rnums[j]);
		} else {
			db_bind_int(stmt, 2, 0);
			db_bind_int(stmt, 3, 0);
			db_bind_int(stmt, 4, 0);
		}
		db_step(stmt, DB_STEP_CONSTRAINT);
		sqlite3_reset(stmt);
	}
	sqlite3_finalize(stmt);

	stmt = db_stmt("UPDATE experiment SET state=?");
	db_bind_int(stmt, 1, ESTATE_POSTWIN);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);

	INFO("All lottery winners computed!");

	db_trans_commit();
	mpq_clear(div);
	mpq_clear(aggr);
	free(pids);
	free(winners);
	free(rnums);
	return(1);
}

int64_t
db_admin_get_flags(void)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 v;

	stmt = db_stmt("SELECT isset FROM admin");
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);
	v = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	return(v);
}

char *
db_admin_get_mail(void)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	char		*mail;

	stmt = db_stmt("SELECT email FROM admin");
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);
	mail = kstrdup((char *)sqlite3_column_text(stmt, 0));
	sqlite3_finalize(stmt);
	return(mail);
}

void
db_admin_set_mail(const char *email)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE admin SET email=?,isset=isset|1");
	db_bind_text(stmt, 1, email);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator set email: %s", email);
}

int
db_admin_valid_pass(const char *pass)
{
	sqlite3_stmt	*stmt;
	int		 rc;

	stmt = db_stmt("SELECT hash FROM admin");
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);

	if ( ! db_crypt_check(sqlite3_column_text(stmt, 0), pass)) {
		INFO("Administrator failed login");
		sqlite3_finalize(stmt);
		return(0);
	}

	sqlite3_finalize(stmt);
	return(1);
}

int
db_admin_valid_email(const char *email)
{
	sqlite3_stmt	*stmt;
	int		 rc;

	stmt = db_stmt("SELECT * FROM admin WHERE email=?");
	db_bind_text(stmt, 1, email);
	rc = db_step(stmt, 0);
	sqlite3_finalize(stmt);
	return(SQLITE_ROW == rc);
}

struct player *
db_player_valid(const char *mail, const char *pass)
{
	sqlite3_stmt	*stmt;
	int64_t		 id;

	stmt = db_stmt("SELECT hash,id FROM player WHERE email=?");
	db_bind_text(stmt, 1, mail);
	if (SQLITE_ROW != db_step(stmt, 0)) {
		sqlite3_finalize(stmt);
		return(NULL);
	} 
	if (NULL != pass &&
	    ! db_crypt_check(sqlite3_column_text(stmt, 0), pass)) {
		sqlite3_finalize(stmt);
		return(NULL);
	}
	id = sqlite3_column_int64(stmt, 1);
	sqlite3_finalize(stmt);
	return(db_player_load(id));
}

int
db_admin_valid(const char *email, const char *pass)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("SELECT hash FROM admin WHERE email=?");
	db_bind_text(stmt, 1, email);

	if (SQLITE_ROW != db_step(stmt, 0)) {
		INFO("Admin login with incorrect email: %s", email);
		sqlite3_finalize(stmt);
		return(0);
	} 

	if ( ! db_crypt_check(sqlite3_column_text(stmt, 0), pass)) {
		INFO("Admin login with incorrect password");
		sqlite3_finalize(stmt);
		return(0);
	}

	sqlite3_finalize(stmt);
	return(1);
}

size_t
db_game_count_all(void)
{

	return(db_count_all("game"));
}

size_t
db_player_count_all(void)
{

	return(db_count_all("player"));
}

size_t
db_player_count_plays(int64_t round, int64_t playerid)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 val;

	stmt = db_stmt("SELECT choices FROM gameplay "
		"WHERE playerid=? AND round=?");
	db_bind_int(stmt, 1, playerid);
	db_bind_int(stmt, 2, round);
	rc = db_step(stmt, 0);
	if (SQLITE_DONE == rc) 
		val = 0;
	else
		val = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	assert(val >= 0);
	return(val);
}

static void
db_player_clear(struct player *player)
{

	if (NULL == player)
		return;
	free(player->mail);
	free(player->hitid);
	free(player->assignmentid);
	free(player->hash);
}

void
db_player_free(struct player *player)
{

	db_player_clear(player);
	free(player);
}

void
db_player_set_answered(int64_t player, int64_t answer)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET "
		"answer=CASE WHEN answer=? THEN "
		"answer+1 ELSE answer END WHERE id=?");
	db_bind_int(stmt, 1, answer);
	db_bind_int(stmt, 2, player);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
}

/*
 * Set Boolean "instr" whether want to see instructions on load.
 */
void
db_player_set_instr(int64_t player, int64_t instr)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt
		("UPDATE player SET "
		"instr=?,version=version+1 WHERE id=?");
	db_bind_int(stmt, 1, instr);
	db_bind_int(stmt, 2, player);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
}

static void
db_game_fill(struct game *p, size_t *pos, sqlite3_stmt *s)
{
	size_t	 i = 0, sz;

	memset(p, 0, sizeof(struct game));
	if (NULL == pos)
		pos = &i;
	p->p1 = sqlite3_column_int64(s, (*pos)++);
	p->p2 = sqlite3_column_int64(s, (*pos)++);
	sz = 2 * p->p1 * p->p2;
	p->payoffs = mpq_str2mpqsinit
		(sqlite3_column_text(s, (*pos)++), sz);
	p->name = kstrdup((char *)
		sqlite3_column_text(s, (*pos)++));
	p->id = sqlite3_column_int64(s, (*pos)++);
}

static void
db_game_unfill(struct game *p)
{
	size_t	 i, sz;

	if (NULL == p)
		return;
	sz = 2 * p->p1 * p->p2;
	for (i = 0; i < sz; i++)
		mpq_clear(p->payoffs[i]);
	free(p->payoffs);
	free(p->name);
}

void
db_game_free_array(struct game *game, size_t sz)
{
	size_t	 i;

	for (i = 0; i < sz; i++)
		db_game_unfill(&game[i]);
	free(game);
}

void
db_game_free(struct game *game)
{

	db_game_unfill(game);
	free(game);
}

static void
db_player_fill(struct player *p, size_t *pos, sqlite3_stmt *s)
{
	size_t 	i = 0;
	
	memset(p, 0, sizeof(struct player));
	if (NULL == pos)
		pos = &i;

	p->mail = kstrdup((char *)
		sqlite3_column_text(s, (*pos)++));
	p->state = sqlite3_column_int64(s, (*pos)++);
	p->id = sqlite3_column_int64(s, (*pos)++);
	p->enabled = sqlite3_column_int64(s, (*pos)++);
	p->role = sqlite3_column_int64(s, (*pos)++);
	p->rseed = sqlite3_column_int64(s, (*pos)++);
	p->instr = sqlite3_column_int64(s, (*pos)++);
	p->finalrank = sqlite3_column_int64(s, (*pos)++);
	p->finalscore = sqlite3_column_int64(s, (*pos)++);
	p->autoadd = sqlite3_column_int64(s, (*pos)++);
	p->version = sqlite3_column_int64(s, (*pos)++);
	p->joined = sqlite3_column_int64(s, (*pos)++);
	p->answer = sqlite3_column_int64(s, (*pos)++);
	p->hitid = kstrdup((char *)
		sqlite3_column_text(s, (*pos)++));
	p->assignmentid = kstrdup((char *)
		sqlite3_column_text(s, (*pos)++));
	p->hash = kstrdup((char *)
		sqlite3_column_text(s, (*pos)++));
	p->mturkdone = sqlite3_column_int64(s, (*pos)++);
}

/*
 * Load a single player by their unique identifier.
 * Return NULL if the player was not found.
 */
struct player *
db_player_load(int64_t id)
{
	sqlite3_stmt	*stmt;
	struct player	*player;

	stmt = db_stmt("SELECT " PLAYER " FROM player WHERE id=?");
	db_bind_int(stmt, 1, id);
	if (SQLITE_ROW != db_step(stmt, 0)) {
		sqlite3_finalize(stmt);
		return(NULL);
	}
	player = kmalloc(sizeof(struct player));
	db_player_fill(player, NULL, stmt);
	sqlite3_finalize(stmt);
	return(player);
}

/*
 * Sort the players by their aggregate tickets sorted by count.
 * If "limit" is set to zero, this will return all of the sorted
 * players.
 * Otherwise, it will act as a limiter.
 */
void
db_player_load_highest(playerscorefp fp, void *arg, size_t limit)
{
	sqlite3_stmt	*stmt;
	struct player	*player;
	mpq_t		 aggr;

	if (limit > 0) {
		stmt = db_stmt("SELECT playerid,max(aggrtickets),"
			"aggrpayoff FROM lottery "
			"GROUP BY playerid "
			"ORDER BY aggrtickets DESC LIMIT ?");
		db_bind_int(stmt, 1, limit);
	} else
		stmt = db_stmt("SELECT playerid,max(aggrtickets),"
			"aggrpayoff FROM lottery "
			"GROUP BY playerid "
			"ORDER BY aggrtickets DESC");

	while (SQLITE_ROW == db_step(stmt, 0)) {
		player = db_player_load
			(sqlite3_column_int64(stmt, 0));
		assert(NULL != player);
		mpq_str2mpqinit(sqlite3_column_text(stmt, 2), aggr);
		fp(player, mpq_get_d(aggr),
			sqlite3_column_int64(stmt, 1), arg);
		db_player_free(player);
		mpq_clear(aggr);
	}

	sqlite3_finalize(stmt);
}

/*
 * Load all Mechanical Turk players who have finished their sequence of
 * play and need to receive bonuses.
 */
struct player **
db_player_load_bonuses(int64_t **scores, size_t *sz)
{
	sqlite3_stmt	 *stmt;
	struct player	**player;
	size_t		  i;

	*sz = 0;
	stmt = db_stmt("SELECT " PLAYER ",max(lottery.aggrtickets) "
		"FROM lottery "
		"INNER JOIN player ON player.id=playerid "
		"WHERE player.assignmentid != \'\' AND "
		"player.mturkdone=1 "
		"GROUP BY playerid");

	player = NULL;
	*scores = NULL;

	while (SQLITE_ROW == db_step(stmt, 0)) {
		player = kreallocarray
			(player, *sz + 1,
			 sizeof(struct player *));
		*scores = kreallocarray
			(*scores, *sz + 1,
			 sizeof(int64_t));
		player[*sz] = kmalloc(sizeof(struct player));
		i = 0;
		db_player_fill(player[*sz], &i, stmt);
		(*scores)[*sz] = sqlite3_column_int64(stmt, i);
		(*sz)++;
	}
	sqlite3_finalize(stmt);

	return(player);
}

#if 0
int64_t
db_player_set_bonus(int64_t id)
{
       sqlite3_stmt    *stmt;
       int64_t		bonus;

       bonus = arc4random();

       stmt = db_stmt("UPDATE player SET bonus=? WHERE id=?");
       db_bind_int(stmt, 1, bonus);
       db_bind_int(stmt, 2, id);
       db_step(stmt, 0);
       sqlite3_finalize(stmt);
       INFO("Player %" PRId64 " setting bonus: %" PRId64, id, bonus);
       return(bonus);
}
#endif

/*
 * Load all of the players who are currently playing.
 * This means that they are joined to the experiment and are still
 * within the maximum number of allotted rounds.
 */
void
db_player_load_playing(const struct expr *expr, playerf fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct player	 player;

	stmt = db_stmt("SELECT " PLAYER " FROM player WHERE "
		"CASE WHEN joined >= 0 THEN "
		"joined <= ? AND ? < JOINED + ? ELSE 1 END");
	db_bind_int(stmt, 1, expr->round);
	db_bind_int(stmt, 2, expr->round);
	db_bind_int(stmt, 3, expr->prounds);
	while (SQLITE_ROW == db_step(stmt, 0)) {
		db_player_fill(&player, NULL, stmt);
		(*fp)(&player, arg);
		db_player_clear(&player);
	}
	sqlite3_finalize(stmt);
}

/*
 * Load all players registered in the system.
 * Orders them by e-mail address (identifier).
 */
void
db_player_load_all(playerf fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct player	 player;

	stmt = db_stmt("SELECT " PLAYER " FROM player "
		"ORDER BY email ASC");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		db_player_fill(&player, NULL, stmt);
		(*fp)(&player, arg);
		db_player_clear(&player);
	}
	sqlite3_finalize(stmt);
}

void
db_player_mturkdone(int64_t playerid)
{
       sqlite3_stmt    *stmt;

       stmt = db_stmt("UPDATE player SET mturkdone=1 WHERE id=?");
       db_bind_int(stmt, 1, playerid);
       db_step(stmt, 0);
       sqlite3_finalize(stmt);
       INFO("Player %" PRId64 " finished mturk", playerid);
}

/*
 * This should be invoked for players who are playing.
 * `Plays' values MUST be canonicalised prior to being passed here, and
 * must sum to one.
 * If the player is allowed to play (correct round, etc.), then create a
 * `choice' with their choice and increment the `game-play' counter.
 * This returns zero if we're not in the correct state: player has exceeded
 * their maximum number of plays, game has ended, etc.
 * Otherwise it returns non-zero.
 */
int
db_player_play(const struct player *p, int64_t sessid, 
	int64_t round, int64_t gameid, mpq_t *plays, size_t sz)
{
	struct expr	*expr;
	sqlite3_stmt	*stmt;
	int		 rc;
	char		*buf;

	db_trans_begin(1);

	/*
	 * Safety checks: make sure we're not playing in an experiment
	 * that hasn't started, an experiment that has ended, or if
	 * we're not playing for the current round.
	 */
	if (NULL == (expr = db_expr_get(1))) {
		db_trans_rollback();
		INFO("Player %" PRId64 " tried playing "
			"game %" PRId64 " (round %" PRId64 "), but "
			"not started", p->id, gameid, round);
		return(0);
	} else if (round != expr->round) {
		db_trans_rollback();
		INFO("Player %" PRId64 " tried playing "
			"game %" PRId64 " (round %" PRId64 "), but "
			"round invalid (at %" PRId64 ")", 
			p->id, gameid, round, expr->round);
		db_expr_free(expr);
		return(0);
	} else if (round >= expr->rounds) {
		db_trans_rollback();
		INFO("Player %" PRId64 " tried playing "
			"game %" PRId64 " (round %" PRId64 "), but "
			"round invalid (max %" PRId64 ")", 
			p->id, gameid, round, expr->rounds);
		db_expr_free(expr);
		return(0);
	} else if (round < p->joined) {
		db_trans_rollback();
		INFO("Player %" PRId64 " tried playing "
			"game %" PRId64 " (round %" PRId64 "), but "
			"hasn't been admitted (slated %" PRId64 ")",
			p->id, gameid, round, p->joined);
		db_expr_free(expr);
		return(0);
	} else if (round >= p->joined + expr->prounds) {
		db_trans_rollback();
		INFO("Player %" PRId64 " tried playing "
			"game %" PRId64 " (round %" PRId64 "), but "
			"has exceeded player max (%" PRId64 ")",
			p->id, gameid, round, 
			p->joined + expr->prounds);
		db_expr_free(expr);
		return(0);
	}
	db_expr_free(expr);

	/*
	 * Create our `choice': the mixture of strategies for this given
	 * game and this given round.
	 * Tie that choice to our session (for records-keeping).
	 */
	buf = mpq_mpq2str(plays, sz);
	stmt = db_stmt("INSERT INTO choice "
		"(round,playerid,gameid,strats,stratsz,created,sessid) "
		"VALUES (?,?,?,?,?,?,?)");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, p->id);
	db_bind_int(stmt, 3, gameid);
	db_bind_text(stmt, 4, buf);
	db_bind_int(stmt, 5, sz);
	db_bind_int(stmt, 6, time(NULL));
	db_bind_int(stmt, 7, sessid);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);
	free(buf);
	if (SQLITE_CONSTRAINT == rc) {
		db_trans_rollback();
		INFO("Player %" PRId64 " tried "
			"playing game %" PRId64 " (round %" 
			PRId64 "), but has already played",
			p->id, gameid, round);
		return(0);
	}

	/*
	 * First create (this is a no-op if it has already been created)
	 * then update the record of how many `choice' fields we've made
	 * for this round.
	 */
	stmt = db_stmt("INSERT INTO gameplay "
		"(round,playerid) VALUES (?,?)");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, p->id);
	db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);
	stmt = db_stmt("UPDATE gameplay "
		"SET choices=choices + 1 "
		"WHERE round=? AND playerid=?");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, p->id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	db_trans_commit();
	return(1);
}

int
db_game_check_player(int64_t playerid, int64_t round, int64_t game)
{
	sqlite3_stmt	*stmt;
	int		 rc;

	stmt = db_stmt("SELECT * FROM choice "
		"WHERE playerid=? AND round=? AND gameid=?");
	db_bind_int(stmt, 1, playerid);
	db_bind_int(stmt, 2, round);
	db_bind_int(stmt, 3, game);
	rc = db_step(stmt, 0);
	sqlite3_finalize(stmt);
	return(rc == SQLITE_ROW);
}

void
db_game_load_player(int64_t playerid, 
	int64_t round, gameroundf fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct game	 game;
	int64_t		 id;
	size_t		 i, maxcount;

	stmt = db_stmt("SELECT payoffs,p1,p2,name,id FROM game");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		id = sqlite3_column_int64(stmt, 4);
		if (db_game_check_player(playerid, round, id)) {
			(*fp)(NULL, round, arg);
			continue;
		}
		memset(&game, 0, sizeof(struct game));
		game.p1 = sqlite3_column_int64(stmt, 1);
		game.p2 = sqlite3_column_int64(stmt, 2);
		maxcount = game.p1 * game.p2 * 2;
		game.payoffs = mpq_str2mpqsinit
			(sqlite3_column_text(stmt, 0), maxcount);
		game.name = strdup((char *)sqlite3_column_text(stmt, 3));
		assert(NULL != game.name);
		game.id = sqlite3_column_int64(stmt, 4);
		(*fp)(&game, round, arg);
		for (i = 0; i < maxcount; i++)
			mpq_clear(game.payoffs[i]);
		free(game.payoffs);
		free(game.name);
	}

	sqlite3_finalize(stmt);
}

size_t
db_game_round_count_done(int64_t round, int64_t role, size_t gamesz)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 result;

	stmt = db_stmt("SELECT count(*) FROM gameplay "
		"INNER JOIN player ON player.id = gameplay.playerid "
		"WHERE round=? AND choices=? AND player.role = ?");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, gamesz);
	db_bind_int(stmt, 3, role);
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);
	result = (size_t)sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	return(result);
}

/*
 * Count the number of current players in role "role" for the current
 * (as described by "expr") round of the experiment.
 */
size_t
db_expr_round_count(const struct expr *expr, int64_t round, int64_t role)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 result;

	stmt = db_stmt
		("SELECT count(*) from player "
		 "WHERE player.joined > -1 AND "
		 "player.joined <= ?1 AND "
		 "?1 < player.joined + ?2 AND "
		 "player.role = ?3"); 
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, expr->prounds);
	db_bind_int(stmt, 3, role);
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);
	result = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	assert(result >= 0 && (uint64_t)result < SIZE_MAX);
	return(result);
}

struct game *
db_game_load(int64_t gameid)
{
	sqlite3_stmt	*stmt;
	struct game	*game = NULL;

	stmt = db_stmt("SELECT " GAME " FROM game WHERE id=?");
	db_bind_int(stmt, 1, gameid);
	if (SQLITE_ROW == db_step(stmt, 0)) {
		game = kcalloc(1, sizeof(struct game));
		db_game_fill(game, NULL, stmt);
	}

	sqlite3_finalize(stmt);
	return(game);
}

struct game *
db_game_load_all_array(size_t *sz)
{
	sqlite3_stmt	*stmt;
	struct game	*games = NULL;
	size_t		 i = 0;

	stmt = db_stmt("SELECT " GAME " FROM game ORDER BY id");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		games = kreallocarray
			(games, i + 1, sizeof(struct game));
		db_game_fill(&games[i++], NULL, stmt);
	}

	sqlite3_finalize(stmt);
	*sz = i;
	return(games);
}

size_t
db_customq_count(void)
{

	return(db_count_all("customquestion"));
}

int
db_customq_verify(int64_t rank, const char *answer)
{
	sqlite3_stmt	*stmt;
	int		 rc;

	stmt = db_stmt("SELECT * FROM customquestion "
		"WHERE rank=? AND answer=?");
	db_bind_int(stmt, 1, rank);
	db_bind_text(stmt, 2, answer);
	rc = db_step(stmt, 0);
	sqlite3_finalize(stmt);
	return(SQLITE_ROW == rc);
}

void
db_customq_load_all(customqf fp, void *arg)
{
	sqlite3_stmt	*stmt;
	char		*ques, *ans;

	stmt = db_stmt("SELECT rank,question,answer "
		"FROM customquestion ORDER BY rank ASC");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		ques = kstrdup((char *)
			sqlite3_column_text(stmt, 1));
		ans = kstrdup((char *)
			sqlite3_column_text(stmt, 2));
		(*fp)(ques, ans, arg);
		free(ques);
		free(ans);
	}
	sqlite3_finalize(stmt);
}

void
db_game_load_all(gamef fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct game	 game;

	stmt = db_stmt("SELECT " GAME " FROM game ORDER BY id");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		db_game_fill(&game, NULL, stmt);
		(*fp)(&game, arg);
		db_game_unfill(&game);
	}
	sqlite3_finalize(stmt);
}

struct game *
db_game_alloc(const char *poffs, 
	const char *name, int64_t p1, int64_t p2)
{
	char		*buf, *tok, *sv;
	mpq_t		*rops;
	size_t		 i, count, maxcount;
	sqlite3_stmt	*stmt;
	struct game	*game;

	if (p2 && p1 > INT64_MAX / p2)
		return(NULL);

	sv = buf = kstrdup(poffs);
	maxcount = p1 * p2 * 2;
	rops = kcalloc(maxcount, sizeof(mpq_t));
	count = 0;

	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		if ('\0' == *tok)
			continue;
		if (count >= maxcount)
			goto err;
		else if ( ! mpq_str2mpq(tok, rops[count]))
			goto err;
		count++;
	}

	free(sv);
	sv = NULL;

	if (count != (size_t)(p1 * p2 * 2))
		goto err;

	db_trans_begin(0);
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		goto err;
	}

	game = kcalloc(1, sizeof(struct game));
	game->payoffs = rops;
	game->p1 = p1;
	game->p2 = p2;
	game->name = kstrdup(name);
	sv = mpq_mpq2str(rops, maxcount);
	stmt = db_stmt("INSERT INTO game "
		"(payoffs, p1, p2, name) VALUES(?,?,?,?)");
	db_bind_text(stmt, 1, sv);
	db_bind_int(stmt, 2, game->p1);
	db_bind_int(stmt, 3, game->p2);
	db_bind_text(stmt, 4, game->name);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	game->id = sqlite3_last_insert_rowid(db);
	db_trans_commit();

	free(sv);
	INFO("Administrator created game %" PRId64, game->id);
	return(game);
err:
	free(sv);
	for (i = 0; i < count; i++)
		mpq_clear(rops[i]);
	free(rops);
	return(NULL);
}

/*
 * Mturk player logging back in (db_player_create() returned 0).
 * Verify that their hitId is the same as when they initially tried to
 * log in so that they can continue playing.
 */
int
db_player_mturkvrfy(const char *email, char **pass, 
	const char *hitid, const char *assid)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 id;

	stmt = db_stmt("SELECT hash,id FROM player "
		"WHERE email=? AND hitid=? AND assignmentid=?");
	db_bind_text(stmt, 1, email);
	db_bind_text(stmt, 2, hitid);
	db_bind_text(stmt, 3, assid);
	if (SQLITE_ROW == (rc = db_step(stmt, 0))) {
		*pass = kstrdup((char *)
			sqlite3_column_text(stmt, 0));
		id = sqlite3_column_int64(stmt, 1);
		INFO("Player %" PRId64 " re-entering "
			"with same HIT ID %s and assignment ID: %s", 
			id, hitid, assid);
	} else
		INFO("Player tried re-entering "
			"with different IDs: %s", email);

	sqlite3_finalize(stmt);
	return(SQLITE_ROW == rc);
}

/*
 * Create a player, automatically setting a password for them.
 * Returns zero if the player already exists (nothing changed), and
 * nonzero on new player creation.
 * Fills in the "pass" pointer (if set) with the player's password or
 * NULL if errors have occurred.
 */
int
db_player_create(const char *email, char **pass, 
	const char *hitid, const char *assid)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	char		*hash;

	if (NULL != pass)
		*pass = NULL;

	hash = db_crypt_mkpass();
	stmt = db_stmt("INSERT INTO player "
		"(email,rseed,hash,autoadd,hitid,assignmentid) "
		"VALUES (?,?,?,?,?,?)");
	db_bind_text(stmt, 1, email);
	db_bind_int(stmt, 2, arc4random_uniform(INT32_MAX) + 1);
	db_bind_text(stmt, 3, hash);
	db_bind_int(stmt, 4, NULL != pass);
	db_bind_text(stmt, 5, NULL == hitid ? "" : hitid);
	db_bind_text(stmt, 6, NULL == assid ? "" : assid);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);
	if (SQLITE_DONE == rc) {
		INFO("%sPlayer %" PRId64 " created: %s", 
			NULL != hitid ? "Mechanical Turk " : "",
			sqlite3_last_insert_rowid(db), email);
		if (NULL != pass)
			*pass = hash;
	}
	if (NULL == pass)
		free(hash);
	return(SQLITE_DONE == rc);
}

int
db_expr_checkstate(enum estate state)
{
	sqlite3_stmt	*stmt;
	int		 rc;

	stmt = db_stmt("SELECT * FROM experiment WHERE state=?");
	db_bind_int(stmt, 1, state);
	rc = db_step(stmt, 0);
	sqlite3_finalize(stmt);
	return(rc == SQLITE_ROW);
}

/*
 * Get the number of players in the lobby, i.e., those who haven't yet
 * joined the experiment.
 */
size_t
db_expr_lobbysize(void)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 result;

	stmt = db_stmt
		("SELECT count(*) from player "
		 "WHERE player.joined < 0");
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);
	result = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	assert(result >= 0 && (uint64_t)result < SIZE_MAX);
	return(result);
}

void
db_expr_setmailer(int64_t old, int64_t new)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE experiment SET "
		"roundpid=? WHERE roundpid=?");
	db_bind_int(stmt, 1, new);
	db_bind_int(stmt, 2, old);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
}

/*
 * Set the "auto-add" (captive mode) facility.
 * Also set whether we're going to continue inheriting this facility
 * after the experiment has started.
 * (Not doing so is a security measure to cut off the number of players
 * when the experiment begins.)
 */
void
db_expr_setautoadd(int64_t autoadd, int64_t preserve)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE experiment SET "
		"autoadd=?,autoaddpreserve=?");
	db_bind_int(stmt, 1, autoadd ? 1 : 0);
	db_bind_int(stmt, 2, preserve ? 1 : 0);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator %s captive: %s preserve",
		autoadd ? "enabled" : "disabled",
		preserve ? "do" : "do not");
}

void
db_expr_setinstr(const char *instr)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE experiment SET instr=?");
	db_bind_text(stmt, 1, instr);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator changed instructions");
}

/*
 * Decide whether to let player "player" join the game in the next
 * round.
 * The player must have a join value of -1, i.e., she has not yet joined
 * for game-play.
 * This works by atomically fetching the experiment, calculating the
 * number of players playing per role in the next round, and seeing if
 * we can fit in the current player.
 * If so, we schedule her to play.
 */
int
db_player_join(const struct player *player, int64_t answers)
{
	sqlite3_stmt	*stmt;
	int64_t		 count, count0, count1, role;
	struct expr	*expr;

	assert(-1 == player->joined);
	db_trans_begin(1);
	expr = db_expr_get(0);
	assert(NULL != expr);

	if (expr->round + 1 >= expr->rounds) {
		db_trans_rollback();
		INFO("Player %" PRId64 " asked to join "
			"after end of experiment", player->id);
		db_expr_free(expr);
		return(0);
	}

	if (expr->questionnaire && answers != player->answer) {
		db_trans_rollback();
		db_expr_free(expr);
		return(0);
	}

	/* Count the number of player roles in the next. */
	count0 = db_expr_round_count(expr, expr->round + 1, 0);
	count1 = db_expr_round_count(expr, expr->round + 1, 1);
	/* Assign our player role to the minimum. */
	if (count0 < count1) {
		role = 0;
		count = count0;
	} else {
		role = 1;
		count = count1;
	}
	if (expr->playermax > 0 && count >= expr->playermax) {
		/*
		 * We've exceeded the maximum number of acceptable
		 * players in this role at this time.
		 */
		db_trans_rollback();
		INFO("Player %" PRId64 " asked to join "
			"round %" PRId64 " in role %" PRId64 " but it "
			"already has maximum players: %" 
			PRId64, player->id, expr->round + 1,
			role, count);
		return(0);
	}
	/* Number of players ok: join! */
	stmt = db_stmt("UPDATE player SET joined=?,role=? WHERE id=?");
	db_bind_int(stmt, 1, expr->round + 1);
	db_bind_int(stmt, 2, role);
	db_bind_int(stmt, 3, player->id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	db_trans_commit();
	INFO("Next round (%" PRId64 ") will have %" PRId64 " "
		"players (max %" PRId64 " per role, role %" PRId64 
		", had %" PRId64 "): scheduling player %" PRId64 
		" as well", expr->round + 1, count, 
		expr->playermax, role, count, player->id);
	db_expr_free(expr);
	return(1);
}

/*
 * Set that an experiment will start at the given time.
 * This doesn't necessarily mean that the experiment is accepting plays,
 * which depends upon `date' and db_expr_advance().
 */
int
db_expr_start(int64_t date, int64_t roundpct, int64_t roundmin, 
	int64_t rounds, int64_t prounds, int64_t minutes, 
	int64_t playermax, const char *instr, const char *uri,
	const char *historyfile, const char *lottery, int64_t ques,
	int64_t flags, const char **qs, const char **as, size_t qsz)
{
	sqlite3_stmt	*stmt, *stmt2;
	int64_t		 id;
	size_t		 i;
	time_t		 t;
	int64_t		 players[2];

	db_trans_begin(0);

	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		return(0);
	}

	if (roundpct < 0)
		roundpct = 0;
	if (roundpct > 100)
		roundpct = 100;
	if (0 == prounds)
		prounds = rounds;
	if (date < (t = time(NULL)))
		date = t;

	stmt = db_stmt("UPDATE experiment SET "
		"start=?,rounds=?,minutes=?,"
		"loginuri=?,instr=?,state=?,"
		"roundpct=?,prounds=?,playermax=?,"
		"prounds=?,history=?,lottery=?,questionnaire=?,"
		"autoadd=CASE WHEN autoaddpreserve=1 "
			"THEN autoadd ELSE 0 END,"
		"roundmin=?,flags=?");
	db_bind_int(stmt, 1, date);
	db_bind_int(stmt, 2, rounds);
	db_bind_int(stmt, 3, minutes);
	db_bind_text(stmt, 4, uri);
	db_bind_text(stmt, 5, instr);
	db_bind_int(stmt, 6, ESTATE_STARTED);
	db_bind_double(stmt, 7, roundpct / 100.0);
	db_bind_int(stmt, 8, rounds);
	db_bind_int(stmt, 9, playermax);
	db_bind_int(stmt, 10, prounds);
	db_bind_text(stmt, 11, NULL == historyfile ? "" : historyfile);
	db_bind_text(stmt, 12, lottery);
	db_bind_int(stmt, 13, ques);
	db_bind_int(stmt, 14, roundmin);
	db_bind_int(stmt, 15, flags);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);

	INFO("Started experiment: %" PRId64 " rounds, "
		"%" PRId64 " per player (%" PRId64 " simultaneously), "
		"%" PRId64 " minutes (%" PRId64 " min)", 
		rounds, prounds, playermax, minutes, roundmin);
	INFO("Started experiment: %" PRId64 "%%, "
		"with%s a lottery, with%s questions, "
		"with%s a pre-history", roundpct, 
		lottery && '\0' != *lottery ? "" : "out",
		ques ? "" : "out",
		NULL != historyfile ? "" : "out");
	INFO("Started experiment: "
		"%sshow history, "
		"%sshuffle matrices, "
		"%sshow relative rounds",
		EXPR_NOHISTORY & flags ? "don't " : "",
		EXPR_NOSHUFFLE & flags ? "don't " : "",
		EXPR_RELATIVE & flags ? "don't " : "");

	if ( ! ques) {
		/*
		 * If we're not running a questionnaire, then assign
		 * people to play immediately.
		 * Loop through assigning players to the zeroth round.
		 * We obey the maximum number of players per role, so
		 * make sure we keep track.
		 * Assignment order is random.
		 */
		i = 0;
		players[0] = players[1] = 0;
		stmt = db_stmt("SELECT id from player "
			"ORDER BY rseed ASC, id ASC");
		stmt2 = db_stmt("UPDATE player "
			"SET role=?,joined=0 WHERE id=?");
		while (SQLITE_ROW == db_step(stmt, 0)) {
			id = sqlite3_column_int64(stmt, 0);
			db_bind_int(stmt2, 1, i % 2);
			db_bind_int(stmt2, 2, id);
			db_step(stmt2, 0);
			sqlite3_reset(stmt2);
			players[i % 2]++;
			if (playermax > 0 &&
				 players[0] >= playermax && 
				 players[1] >= playermax)
				break;
			i++;
		}
		sqlite3_finalize(stmt);
		sqlite3_finalize(stmt2);
		INFO("Started experiment: reset "
			"roles, random seeds, join status");
	}

	stmt = db_stmt("INSERT INTO customquestion "
		"(question,answer,rank) VALUES (?,?,?)");
	for (i = 0; i < qsz; i++) {
		db_bind_text(stmt, 1, qs[i]);
		db_bind_text(stmt, 2, as[i]);
		db_bind_int(stmt, 3, i);
		db_step(stmt, 0);
		sqlite3_reset(stmt);
	}
	sqlite3_finalize(stmt);
	INFO("Added %zu custom questions to experiment", qsz);

	db_trans_commit();
	return(1);
}

void
db_player_enable(int64_t id)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET "
		"enabled=1,version=version+1 WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator enabled player %" PRId64, id);
}

void
db_player_reset_all(void)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET state=0");
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator reset players\' error states");
}

void
db_player_reset_error(void)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET state=0 WHERE state=3");
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator reset players\' "
		"(with error) error states");
}

void
db_player_set_state(int64_t id, enum pstate state)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET state=? WHERE id=?");
	db_bind_int(stmt, 1, state);
	db_bind_int(stmt, 2, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
}

void
db_player_set_mailed(int64_t id, const char *pass)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET state=?,hash=? WHERE id=?");
	db_bind_int(stmt, 1, PSTATE_MAILED);
	db_bind_text(stmt, 2, pass);
	db_bind_int(stmt, 3, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator mailed player %" PRId64, id);
}

char *
db_player_next_new(int64_t *id, char **pass)
{
	sqlite3_stmt	*stmt;
	char		*email;

	email = *pass = NULL;
	stmt = db_stmt("SELECT email,id FROM player WHERE "
		"state=0 AND enabled=1 AND autoadd=0 LIMIT 1");

	if (SQLITE_ROW == db_step(stmt, 0)) {
		email = kstrdup((char *)
			sqlite3_column_text(stmt, 0));
		*id = sqlite3_column_int64(stmt, 1);
		*pass = db_crypt_mkpass();
	} 

	sqlite3_finalize(stmt);
	return(email);
}

int
db_game_delete(int64_t id)
{
	sqlite3_stmt	*stmt;

	db_trans_begin(0);
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		return(0);
	}

	stmt = db_stmt("DELETE FROM game WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	db_trans_commit();
	INFO("Administrator deleted game %" PRId64, id);
	return(1);
}

int
db_player_delete(int64_t id)
{
	sqlite3_stmt	*stmt;

	db_trans_begin(0);
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		return(0);
	}

	stmt = db_stmt("DELETE FROM player WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	db_trans_commit();
	INFO("Administrator deleted player %" PRId64, id);
	return(1);
}

void
db_player_disable(int64_t id)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET "
		"enabled=0,version=version+1 WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator disabled player %" PRId64, id);
}

struct smtp *
db_smtp_get(void)
{
	struct smtp	*smtp = NULL;
	sqlite3_stmt	*stmt;

	stmt = db_stmt("SELECT user,pass,server,email "
		"FROM smtp WHERE isset=1");

	if (SQLITE_ROW == db_step(stmt, 0)) {
		smtp = kcalloc(1, sizeof(struct smtp));
		smtp->user = kstrdup
			((char *)sqlite3_column_text(stmt, 0));
		smtp->pass = kstrdup
			((char *)sqlite3_column_text(stmt, 1));
		smtp->server = kstrdup
			((char *)sqlite3_column_text(stmt, 2));
		smtp->from = kstrdup
			((char *)sqlite3_column_text(stmt, 3));
	}

	sqlite3_finalize(stmt);
	return(smtp);
}

void
db_smtp_free(struct smtp *smtp)
{

	if (NULL == smtp)
		return;

	free(smtp->user);
	free(smtp->pass);
	free(smtp->server);
	free(smtp->from);
	free(smtp);
}

void
db_smtp_set(const char *user, const char *server,
	const char *from, const char *pass)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE smtp SET user=?,"
		"pass=?,server=?,isset=1,email=?");
	db_bind_text(stmt, 1, user);
	db_bind_text(stmt, 2, pass);
	db_bind_text(stmt, 3, server);
	db_bind_text(stmt, 4, from);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator set SMTP server %s, user %s, "
		"from %s set", server, user, from);
}

/*
 * Given the strategy mixes played for a given roundup, compute the
 * per-strategy-pair matrices.
 * In other words, here we create, for a given game and round, the
 * matrices that show the average of the previous round for each
 * strategy combination.
 */
static struct roundup *
db_roundup_round(struct roundup *r)
{
	size_t	 i, j, k;

	r->navg = kcalloc(r->p1sz * r->p2sz, sizeof(double));
	r->navgp1 = kcalloc(r->p1sz, sizeof(double));
	r->navgp2 = kcalloc(r->p2sz, sizeof(double));

	if (0 == r->roundcount)
		return(r);

	for (i = 0; i < r->p1sz; i++)
		r->navgp1[i] = mpq_get_d(r->curp1[i]);
	for (i = 0; i < r->p2sz; i++) 
		r->navgp2[i] = mpq_get_d(r->curp2[i]);

	for (i = k = 0; i < r->p1sz; i++) 
		for (j = 0; j < r->p2sz; j++, k++)
			r->navg[k] = r->navgp1[i] * r->navgp2[j];

	return(r);
}

/*
 * Compute lottery tickets on-demand for the noted round and all prior
 * rounds recursively.
 * Set "cur" to be an mpq (initialising it!) of the current lottery
 * ticket amount and "aggr" (also initialising in the proess!) to be the
 * accumulated (inclusive).
 */
int
db_player_lottery(int64_t round, int64_t pid, 
	mpq_t cur, mpq_t aggr, int64_t *tics, size_t count)
{
	sqlite3_stmt	*stmt;
	size_t		 i;
	int		 rc;
	char		*curstr, *aggrstr;
	int64_t		 prevtics;
	mpq_t		 prevcur, prevaggr;

	if (round < 0)
		return(0);

again:
	stmt = db_stmt("SELECT aggrpayoff,curpayoff FROM "
		"lottery WHERE playerid=? AND round=?");
	db_bind_int(stmt, 1, pid);
	db_bind_int(stmt, 2, round);
	if (SQLITE_ROW == (rc = db_step(stmt, 0))) {
		mpq_str2mpqinit(sqlite3_column_text(stmt, 0), aggr);
		mpq_str2mpqinit(sqlite3_column_text(stmt, 1), cur);
		*tics = ceil(mpq_get_d(aggr));
	}
	sqlite3_finalize(stmt);

	if (SQLITE_ROW == rc)
		return(1);

	if ( ! db_player_lottery(round - 1, pid, 
		 prevcur, prevaggr, &prevtics, count)) 
		mpq_init(prevaggr);
	else
		mpq_clear(prevcur);

	db_trans_begin(1);

	mpq_init(cur);
	mpq_init(aggr);

	stmt = db_stmt("SELECT payoff FROM payoff "
		"WHERE playerid=? AND round=?");
	db_bind_int(stmt, 1, pid);
	db_bind_int(stmt, 2, round);
	for (i = 0; SQLITE_ROW == db_step(stmt, 0); i++)
		mpq_summation_str(cur, 
			sqlite3_column_text(stmt, 0));
	sqlite3_finalize(stmt);

	/* If not enough plays, set lottery to zero. */
	if (i < count) {
		mpq_set_ui(cur, 0, 1);
		mpq_canonicalize(cur);
	} else
		assert(i == count);

	mpq_add(aggr, cur, prevaggr);
	*tics = ceil(mpq_get_d(aggr));

	/* Convert the current and last to strings. */
	gmp_asprintf(&curstr, "%Qd", cur);
	gmp_asprintf(&aggrstr, "%Qd", aggr);

	/* Record both in database. */
	stmt = db_stmt("INSERT INTO lottery "
		"(aggrpayoff,aggrtickets,curpayoff,playerid,round) "
		"VALUES (?,?,?,?,?)");
	db_bind_text(stmt, 1, aggrstr);
	db_bind_int(stmt, 2, *tics);
	db_bind_text(stmt, 3, curstr);
	db_bind_int(stmt, 4, pid);
	db_bind_int(stmt, 5, round);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);

	free(aggrstr);
	free(curstr);
	mpq_clear(prevaggr);

	if (SQLITE_CONSTRAINT == rc) {
		db_trans_rollback();
		mpq_clear(cur);
		mpq_clear(aggr);
		goto again;
	} 

	db_trans_commit();
	return(1);
}

int
db_payoff_get(int64_t round, 
	int64_t playerid, int64_t gameid, mpq_t mpq)
{
	sqlite3_stmt	*stmt;
	int		 rc;

	stmt = db_stmt("SELECT payoff FROM payoff "
		"WHERE playerid=? AND round=? AND gameid=?");
	db_bind_int(stmt, 1, playerid);
	db_bind_int(stmt, 2, round);
	db_bind_int(stmt, 3, gameid);
	if (SQLITE_ROW == (rc = db_step(stmt, 0)))
		mpq_str2mpqinit(sqlite3_column_text(stmt, 0), mpq);
	sqlite3_finalize(stmt);
	return(SQLITE_ROW == rc);
}

mpq_t *
db_choices_get(int64_t round, 
	int64_t playerid, int64_t gameid, size_t *sz)
{
	sqlite3_stmt	*stmt;
	mpq_t		*mpq = NULL;
	int64_t	 	 result;

	stmt = db_stmt("SELECT stratsz,strats FROM choice "
		"WHERE round=? AND playerid=? AND gameid=?");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, playerid);
	db_bind_int(stmt, 3, gameid);
	if (SQLITE_ROW == db_step(stmt, 0)) {
		result = sqlite3_column_int64(stmt, 0);
		assert(result >= 0 && (uint64_t)result < SIZE_MAX);
		*sz = result;
		mpq = mpq_str2mpqsinit
			(sqlite3_column_text(stmt, 1), *sz);
	}
	sqlite3_finalize(stmt);
	return(mpq);
}

static void
db_roundup_players(int64_t round, const struct roundup *r, 
	size_t gamesz, const struct game *game)
{
	mpq_t		*qs, *opponent;
	sqlite3_stmt	*stmt, *stmt2;
	size_t		 i, j, sz;
	mpq_t		 tmp, sum, mul;
	char		*buf;
	int64_t	 	 playerid;

	assert(round >= 0);

	mpq_init(tmp);
	mpq_init(sum);
	mpq_init(mul);

	/* We need a buffer for both strategy vectors. */

	sz = r->p1sz > r->p2sz ? r->p1sz : r->p2sz;
	opponent = kcalloc(sz, sizeof(mpq_t));
	for (i = 0; i < sz; i++)
		mpq_init(opponent[i]);

	stmt = db_stmt("SELECT choice.strats,choice.playerid FROM choice "
		"INNER JOIN player ON player.id=choice.playerid "
		"INNER JOIN gameplay ON "
		"(gameplay.round=choice.round AND "
		" gameplay.playerid=choice.playerid AND "
		" gameplay.choices=?4) "
		"WHERE choice.round=?1 AND choice.gameid=?2 "
		"AND gameplay.round=?1 AND player.role=?3");
	stmt2 = db_stmt("INSERT INTO payoff (round,playerid,"
		"gameid,payoff) VALUES (?,?,?,?)");

	/* First, handle the row player. */

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, game->id);
	db_bind_int(stmt, 3, 0);
	db_bind_int(stmt, 4, gamesz);

	for (i = 0; i < r->p1sz; i++) {
		mpq_set_ui(opponent[i], 0, 1);
		mpq_canonicalize(opponent[i]);
		for (j = 0; j < r->p2sz; j++) {
			/* Normalise aggregate. */
			mpq_set(mul, r->curp2[j]);
			/* Multiply norm by payoff. */
			mpq_mul(sum, mul, 
				game->payoffs[j * 2 + 
				i * (r->p2sz * 2)]);
			mpq_set(tmp, opponent[i]);
			/* Add to current row total. */
			mpq_add(opponent[i], tmp, sum);
		}
	}

	/*
	 * For each player, get her strategies, then multiply each
	 * strategy probability by the opponent's sum for that strategy.
	 * Insert that as the payoff for the given player.
	 */
	while (SQLITE_ROW == db_step(stmt, 0)) {
		qs = mpq_str2mpqsinit
			(sqlite3_column_text(stmt, 0), r->p1sz);
		playerid = sqlite3_column_int64(stmt, 1);
		mpq_set_ui(sum, 0, 1);
		mpq_canonicalize(sum);
		for (i = 0; i < r->p1sz; i++) {
			mpq_set(tmp, qs[i]);
			mpq_mul(mul, tmp, opponent[i]);
			mpq_set(tmp, sum);
			mpq_add(sum, tmp, mul);
		}
		gmp_asprintf(&buf, "%Qd", sum);
		sqlite3_reset(stmt2);
		db_bind_int(stmt2, 1, r->round);
		db_bind_int(stmt2, 2, playerid);
		db_bind_int(stmt2, 3, game->id);
		db_bind_text(stmt2, 4, buf);
		db_step(stmt2, DB_STEP_CONSTRAINT);
		free(buf);
		for (i = 0; i < r->p1sz; i++)
			mpq_clear(qs[i]);
		free(qs);
	}

	/* Now, handle the column player. */

	sqlite3_reset(stmt);

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, game->id);
	db_bind_int(stmt, 3, 1);
	db_bind_int(stmt, 4, gamesz);

	for (i = 0; i < r->p2sz; i++) {
		mpq_set_ui(opponent[i], 0, 1);
		mpq_canonicalize(opponent[i]);
		for (j = 0; j < r->p1sz; j++) {
			/* Normalise aggregate. */
			mpq_set(mul, r->curp1[j]);
			/* Multiply norm by payoff. */
			mpq_mul(sum, mul, 
				game->payoffs[(i * 2) + 1 + 
				j * (r->p2sz * 2)]);
			mpq_set(tmp, opponent[i]);
			/* Add to current row total. */
			mpq_add(opponent[i], tmp, sum);
		}
	}

	/*
	 * For each player, get her strategies, then multiply each
	 * strategy probability by the opponent's sum for that strategy.
	 * Insert that as the payoff for the given player.
	 */
	while (SQLITE_ROW == db_step(stmt, 0)) {
		qs = mpq_str2mpqsinit
			(sqlite3_column_text(stmt, 0), r->p2sz);
		playerid = sqlite3_column_int64(stmt, 1);
		mpq_set_ui(sum, 0, 1);
		mpq_canonicalize(sum);
		for (i = 0; i < r->p2sz; i++) {
			mpq_set(tmp, qs[i]);
			mpq_mul(mul, tmp, opponent[i]);
			mpq_set(tmp, sum);
			mpq_add(sum, tmp, mul);
		}
		gmp_asprintf(&buf, "%Qd", sum);
		sqlite3_reset(stmt2);
		db_bind_int(stmt2, 1, r->round);
		db_bind_int(stmt2, 2, playerid);
		db_bind_int(stmt2, 3, game->id);
		db_bind_text(stmt2, 4, buf);
		db_step(stmt2, DB_STEP_CONSTRAINT);
		free(buf);
		for (i = 0; i < r->p2sz; i++)
			mpq_clear(qs[i]);
		free(qs);
	}

	sqlite3_finalize(stmt);
	sqlite3_finalize(stmt2);

	mpq_clear(tmp);
	mpq_clear(sum);
	mpq_clear(mul);

	for (i = 0; i < sz; i++)
		mpq_clear(opponent[i]);
	free(opponent);
}

static void
db_roundup_free(struct roundup *p)
{
	size_t		i;

	if (NULL == p)
		return;

	if (NULL != p->curp1)
		for (i = 0; i < p->p1sz; i++)
			mpq_clear(p->curp1[i]);
	if (NULL != p->curp2)
		for (i = 0; i < p->p2sz; i++)
			mpq_clear(p->curp2[i]);

	free(p->navg);
	free(p->navgp1);
	free(p->navgp2);
	free(p->curp1);
	free(p->curp2);
	free(p);
}

/*
 * Try to fetch the roundup for a given round and game.
 * If it doesn't exist (i.e., hasn't been built yet, or we're at a
 * negative round), then return a NULL pointer.
 */
static struct roundup *
db_roundup_get(int64_t round, const struct game *game)
{
	struct roundup	*r;
	sqlite3_stmt	*stmt;

	if (round < 0)
		return(NULL);

	r = kcalloc(1, sizeof(struct roundup));
	r->round = round;
	r->p1sz = game->p1;
	r->p2sz = game->p2;
	r->gameid = game->id;

	stmt = db_stmt("SELECT skip,roundcount,"
		"currentsp1,currentsp2,plays FROM past "
		"WHERE round=? AND gameid=?");

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, r->gameid);

	if (SQLITE_ROW == db_step(stmt, 0)) {
		r->skip = sqlite3_column_int64(stmt, 0);
		r->roundcount = sqlite3_column_int64(stmt, 1);
		r->curp1 = mpq_str2mpqsinit
			(sqlite3_column_text
			 (stmt, 2), r->p1sz);
		r->curp2 = mpq_str2mpqsinit
			(sqlite3_column_text
			 (stmt, 3), r->p2sz);
		r->plays = sqlite3_column_int64(stmt, 4);
		sqlite3_finalize(stmt);
		return(db_roundup_round(r));
	} 

	sqlite3_finalize(stmt);
	free(r);
	return(NULL);
}

/*
 * For a given "game", recursively round up the full history of play up
 * to and including "round".
 * This will compute the average strategy plays for a given player role
 * in all of the rounds of this game.
 * NOTE: the database is NOT locked during this time: this function
 * should be invoked without any state.
 */
static void
db_roundup_game(const struct game *game, 
	struct period *period, int64_t round, int64_t gamesz)
{
	struct roundup	*r;
	size_t		 i, count, fullcount;
	sqlite3_stmt	*stmt;
	mpq_t		 tmp, sum;
	char		*cursp1, *cursp2;
	int		 rc;

	if (round < 0)
		return;
	/* Recursive step. */
	db_roundup_game(game, period, round - 1, gamesz);
again:
	/* If the roundup exists, stop now. */
	period->roundups[round] = db_roundup_get(round, game);
	if (NULL != period->roundups[round])
		return;

	r = kcalloc(1, sizeof(struct roundup));
	r->round = round;
	r->p1sz = game->p1;
	r->p2sz = game->p2;
	r->gameid = game->id;
	r->curp1 = kcalloc(r->p1sz, sizeof(mpq_t));
	r->curp2 = kcalloc(r->p2sz, sizeof(mpq_t));
	r->skip = 1;

	for (i = 0; i < r->p1sz; i++)
		mpq_init(r->curp1[i]);
	for (i = 0; i < r->p2sz; i++)
		mpq_init(r->curp2[i]);

	mpq_init(sum);
	mpq_init(tmp);

	db_trans_begin(1);

	/*
	 * Accumulate all the mixtures from all players.
	 * Then average the accumulated mixtures.
	 * If a player has had no plays, then mark that this round
	 * should be skipped in computing the next round: we'll simply
	 * inherit directly from the last round.
	 */
	stmt = db_stmt("SELECT strats from choice "
		"INNER JOIN player ON player.id=choice.playerid "
		"INNER JOIN gameplay ON "
		"(gameplay.round=choice.round AND "
		" gameplay.playerid=choice.playerid AND "
		" gameplay.choices=?4) "
		"WHERE choice.round=?1 AND "
		"choice.gameid=?2 AND player.role=?3");

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, game->id);
	db_bind_int(stmt, 3, 0);
	db_bind_int(stmt, 4, gamesz);

	fullcount = 0;
	for (count = 0; SQLITE_ROW == db_step(stmt, 0); count++)
		mpq_summation_strvec(r->curp1, 
			sqlite3_column_text(stmt, 0), r->p1sz);

	fullcount += count;
	if (0 == count)
		goto aggregate;

	for (i = 0; i < r->p1sz; i++) {
		mpq_set_ui(tmp, count, 1);
		mpq_canonicalize(tmp);
		mpq_set(sum, r->curp1[i]);
		mpq_div(r->curp1[i], sum, tmp);
	} 

	sqlite3_reset(stmt);

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, game->id);
	db_bind_int(stmt, 3, 1);
	db_bind_int(stmt, 4, gamesz);

	for (count = 0; SQLITE_ROW == db_step(stmt, 0); count++) 
		mpq_summation_strvec(r->curp2,
			sqlite3_column_text(stmt, 0), r->p2sz);

	fullcount += count;
	if (0 == count) {
		/* Zero out first strategy. */
		mpq_set_ui(tmp, 0, 1);
		mpq_canonicalize(tmp);
		for (i = 0; i < r->p1sz; i++)
			mpq_set(r->curp1[i], tmp);
		goto aggregate;
	} 

	for (i = 0; i < r->p2sz; i++) {
		mpq_set_ui(tmp, count, 1);
		mpq_canonicalize(tmp);
		mpq_set(sum, r->curp2[i]);
		mpq_div(r->curp2[i], sum, tmp);
	} 

	r->skip = 0;

aggregate:
	sqlite3_finalize(stmt);

	/*
	 * Ok, now we want to make our adjustments for history.
	 * First, we see if we should NOT skip the last round.
	 */
	r->roundcount = 0;
	if (round > 0 && NULL != period->roundups[round - 1])
		r->roundcount = 
			period->roundups[round - 1]->roundcount;
	r->roundcount += r->skip ? 0 : 1;

	cursp1 = mpq_mpq2str(r->curp1, r->p1sz);
	cursp2 = mpq_mpq2str(r->curp2, r->p2sz);

	stmt = db_stmt("INSERT INTO past (round,"
		"gameid,skip,roundcount,currentsp1,"
		"currentsp2,plays) VALUES (?,?,?,?,?,?,?)");

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, game->id);
	db_bind_int(stmt, 3, r->skip);
	db_bind_int(stmt, 4, r->roundcount);
	db_bind_text(stmt, 5, cursp1);
	db_bind_text(stmt, 6, cursp2);
	db_bind_int(stmt, 7, fullcount);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);

	if (SQLITE_CONSTRAINT == rc) { 
		db_trans_rollback();
	} else {
		db_roundup_players(round, r, gamesz, game);
		db_trans_commit();
	}

	free(cursp1);
	free(cursp2);
	mpq_clear(sum);
	mpq_clear(tmp);
	db_roundup_free(r);
	/* 
	 * We've either entered the new roundup or another process beat
	 * us to it.
	 * Either way, run the whole sequence again to pull it again.
	 */
	goto again;
}

void
db_expr_mturk(const char *hitid, const char *error)
{
	sqlite3_stmt	*stmt;

	if (NULL != hitid) {
		stmt = db_stmt("UPDATE experiment SET hitid=?");
		db_bind_text(stmt, 1, hitid);
	} else if (NULL != error) {
		stmt = db_stmt("UPDATE experiment SET awserror=?");
		db_bind_text(stmt, 1, error);
	} else
		return;

	db_step(stmt, 0);
	sqlite3_finalize(stmt);
}

void
db_interval_free(struct interval *intv)
{
	size_t	 i, j;

	if (NULL == intv)
		return;

	for (i = 0; i < intv->periodsz; i++) {
		for (j = 0; j < intv->periods[i].roundupsz; j++)
			db_roundup_free(intv->periods[i].roundups[j]);
		free(intv->periods[i].roundups);
	}

	free(intv->periods);
	free(intv);
}

/*
 * This is the main driver behind incrementing rounds.
 * For each game in the system, we round up all of the plays for that
 * round, creating a history of play and the averages against which
 * individuals will play their strategies for payoffs.
 */
struct interval *
db_interval_get(int64_t round)
{
	struct interval	*intv;
	sqlite3_stmt	*stmt;
	size_t		 i;
	struct game	*game;

	if (round < 0) 
		return(NULL);

	/*
	 * To prevent locking at this level, pull our game identifiers
	 * into an array so that we can query them individually.
	 * Each game will get a history for each round (inclusive),
	 * which we also allocate.
	 */
	intv = kcalloc(1, sizeof(struct interval));
	intv->periodsz = db_game_count_all();
	intv->periods = kcalloc
		(intv->periodsz, sizeof(struct period));

	stmt = db_stmt("SELECT id from game");
	for (i = 0; SQLITE_ROW == db_step(stmt, 0); i++) {
		assert(i < intv->periodsz);
		intv->periods[i].roundupsz = (size_t)round + 1;
		intv->periods[i].roundups = kcalloc
			(intv->periods[i].roundupsz, 
			 sizeof(struct roundup *));
		intv->periods[i].gameid = 
			sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	assert(i == intv->periodsz);

	/* Perform the round up itself for each game. */
	for (i = 0; i < intv->periodsz; i++) {
		game = db_game_load(intv->periods[i].gameid);
		db_roundup_game(game, &intv->periods[i], 
			round, intv->periodsz);
		db_game_free(game);
	}

	return(intv);
}

void
db_expr_clearmturk(void)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE experiment SET "
		"awsaccesskey='',"
		"awssecretkey='',"
		"awsworkers=2,"
		"awsname='',"
		"awsdesc='',"
		"awserror='',"
		"awskeys='',"
		"awssandbox=1,"
		"awsconvert=1,"
		"awsreward=0.01,"
		"awslocale='',"
		"awswhitappr=-1,"
		"awswpctappr=-1 "
		"WHERE state=?");
	db_bind_int(stmt, 1, ESTATE_NEW);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator unset AWS configuration");
}

void
db_expr_setmturk(const char *accesskey,
	const char *secretkey, int64_t workers,
	const char *name, const char *desc, const char *keys,
	int64_t sandbox, double convert, double reward,
	const char *locale, int64_t whit, int64_t wpct)
{
	size_t	 	 i = 1;
	sqlite3_stmt	*stmt;

	/* Clamp values. */
	if (reward < 0.01)
		reward = 0.01;
	if (whit < 0)
		whit = -1;
	if (wpct < 0)
		wpct = -1;
	if (wpct > 100)
		wpct = 100;
	if (workers < 1)
		workers = 1;

	stmt = db_stmt("UPDATE experiment SET "
		"awsaccesskey=?,awssecretkey=?,"
		"awsworkers=?,awsname=?,awsdesc=?,awskeys=?,"
		"awssandbox=?,awsconvert=?,awsreward=?,"
		"awslocale=?,awswhitappr=?,awswpctappr=? "
		"WHERE state=?");
	db_bind_text(stmt, i++, accesskey);
	db_bind_text(stmt, i++, secretkey);
	db_bind_int(stmt, i++, workers);
	db_bind_text(stmt, i++, name);
	db_bind_text(stmt, i++, desc);
	db_bind_text(stmt, i++, keys);
	db_bind_int(stmt, i++, sandbox);
	db_bind_double(stmt, i++, convert);
	db_bind_double(stmt, i++, reward);
	db_bind_text(stmt, i++, locale);
	db_bind_int(stmt, i++, whit);
	db_bind_int(stmt, i++, wpct);
	db_bind_int(stmt, i++, ESTATE_NEW);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	INFO("Administrator set AWS access key: %s", accesskey);
	INFO("Administrator set AWS name: %s", name);
	INFO("Administrator set AWS desc: %s", desc);
	INFO("Administrator set AWS keys: %s", keys);
	INFO("Administrator set AWS sandbox: %s", 
		sandbox ? "true " : "false");
	INFO("Administrator set AWS conversion: %g", convert);
	INFO("Administrator set AWS reward: %g", reward);
	INFO("Administrator set AWS locale: %s", 
		'\0' == *locale ? "(none)" : locale);
	INFO("Administrator set AWS worker HIT: %" PRId64, whit);
	INFO("Administrator set AWS worker accept: %" PRId64, wpct);
}

/*
 * Get a configured experiment.
 * If "only_started" is specified, This will return NULL if the
 * experiment has not been started (i.e., is in ESTATE_NEW).
 * Call db_expr_free() with the returned structure.
 */
struct expr *
db_expr_get(int only_started)
{
	sqlite3_stmt	*stmt;
	struct expr	*expr;
	int		 rc;
	size_t		 i = 0;

	stmt = db_stmt("SELECT start,rounds,minutes,"
		"loginuri,state,instr,total,"
		"autoadd,round,roundbegan,roundpct,"
		"roundmin,prounds,playermax,autoaddpreserve,"
		"history,lottery,questionnaire,hitid,"
		"roundpid,flags,awsaccesskey,"
		"awssecretkey,awserror,awsworkers,awsname,"
		"awsdesc,awskeys,awssandbox,awsconvert,"
		"awsreward,awslocale,awswhitappr,awswpctappr "
		"FROM experiment");
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);

	if (only_started && 
	    ESTATE_NEW == sqlite3_column_int64(stmt, 4)) {
		sqlite3_finalize(stmt);
		return(NULL);
	}

	expr = kcalloc(1, sizeof(struct expr));
	expr->start = (time_t)sqlite3_column_int64(stmt, i++);
	expr->rounds = sqlite3_column_int64(stmt, i++);
	expr->minutes = sqlite3_column_int64(stmt, i++);
	expr->loginuri = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->state = sqlite3_column_int64(stmt, i++);
	expr->instr = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->total = sqlite3_column_int64(stmt, i++);
	expr->autoadd = sqlite3_column_int64(stmt, i++);
	expr->round = sqlite3_column_int64(stmt, i++);
	expr->roundbegan = sqlite3_column_int64(stmt, i++);
	expr->roundpct = sqlite3_column_double(stmt, i++);
	expr->roundmin = sqlite3_column_int64(stmt, i++);
	expr->prounds = sqlite3_column_int64(stmt, i++);
	expr->playermax = sqlite3_column_int64(stmt, i++);
	expr->autoaddpreserve = sqlite3_column_int64(stmt, i++);
	expr->history = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->lottery = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->questionnaire = sqlite3_column_int64(stmt, i++);
	expr->hitid = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->roundpid = sqlite3_column_int64(stmt, i++);
	expr->flags = sqlite3_column_int64(stmt, i++);
	expr->awsaccesskey = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->awssecretkey = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->awserror = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->awsworkers = sqlite3_column_int64(stmt, i++);
	expr->awsname = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->awsdesc = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->awskeys = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->awssandbox = sqlite3_column_int64(stmt, i++);
	expr->awsconvert = sqlite3_column_double(stmt, i++);
	expr->awsreward = sqlite3_column_double(stmt, i++);
	expr->awslocale = kstrdup((char *)sqlite3_column_text(stmt, i++));
	expr->awswhitappr = sqlite3_column_int64(stmt, i++);
	expr->awswpctappr = sqlite3_column_int64(stmt, i++);
	sqlite3_finalize(stmt);
	return(expr);
}

void
db_expr_free(struct expr *expr)
{
	if (NULL == expr)
		return;
	free(expr->loginuri);
	free(expr->hitid);
	free(expr->instr);
	free(expr->history);
	free(expr->awsaccesskey);
	free(expr->awssecretkey);
	free(expr->awserror);
	free(expr->lottery);
	free(expr);
}

void
db_player_questionnaire(int64_t playerid, int64_t rank)
{
	sqlite3_stmt	*stmt;

	/*
	 * We only allow the first answer to be set.
	 * The "tries" field is a holdover from an earlier version of
	 * gamelab and will be removed.
	 */
	stmt = db_stmt("INSERT INTO questionnaire "
		"(playerid,rank,first) VALUES (?,?,?)");
	db_bind_int(stmt, 1, playerid);
	db_bind_int(stmt, 2, rank);
	db_bind_int(stmt, 3, time(NULL));
	db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);
	
	stmt = db_stmt("UPDATE questionnaire "
		"SET tries=tries+1 WHERE "
		"playerid=? AND rank=?");
	db_bind_int(stmt, 1, playerid);
	db_bind_int(stmt, 2, rank);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
}

/*
 * Wipe a database, effectively putting us in the initial state.
 * We essentially remove all state as well as "captive" players.
 * While here, we also scrub the random seed attached to each
 * (remaining) player.
 */
void
db_expr_wipe(void)
{
	sqlite3_stmt	*stmt, *stmt2;

	INFO("Administrator wiping database");
	db_trans_begin(1);
	db_exec("DELETE FROM gameplay");
	db_exec("DELETE FROM sess WHERE playerid IS NOT NULL");
	db_exec("DELETE FROM payoff");
	db_exec("DELETE FROM choice");
	db_exec("DELETE FROM past");
	db_exec("DELETE FROM lottery");
	db_exec("DELETE FROM customquestion");
	db_exec("DELETE FROM winner");
	db_exec("DELETE FROM player WHERE autoadd=1");
	db_exec("UPDATE player SET version=0,instr=1,state=0,"
		"enabled=1,finalrank=0,finalscore=0,hash='',"
		"joined=-1,answer=0,assignmentid='',hitid='',"
		"mturkdone=0");
	db_exec("UPDATE experiment SET "
		"autoadd=0,hitid='',autoaddpreserve=0,"
		"state=0,total=0,round=-1,rounds=0,playermax=0,"
		"prounds=0,roundbegan=0,roundpct=0.0,minutes=0,"
		"roundmin=0,lottery='',questionnaire=0,"
		"roundpid=0,history='',flags=0");
	db_expr_clearmturk();
	stmt = db_stmt("SELECT id FROM player");
	stmt2 = db_stmt("UPDATE player SET rseed=? WHERE id=?");
	/* 
	 * Re-assign the random seed of each player.
	 * This was created when the player was created, and we don't
	 * want it to be inherited between experiment runs.
	 */
	while (SQLITE_ROW == db_step(stmt, 0)) {
		db_bind_int(stmt2, 1, arc4random_uniform(INT32_MAX) + 1);
		db_bind_int(stmt2, 2, sqlite3_column_int64(stmt, 0));
		db_step(stmt2, 0);
		sqlite3_reset(stmt2);
	}
	sqlite3_finalize(stmt);
	sqlite3_finalize(stmt2);
	db_trans_commit();
	INFO("Administrator wiped database");
}

/*
 * This follows almost exactly from the sqlite3 example.
 * Basically, open a new database and backup the existing database page
 * by page.
 * This (apparently) handles consistency issues.
 * We dont' really care about performance: this is only going to be run
 * after a game has completed.
 */
int 
db_backup(const char *zfile)
{
	int		 rc;
	sqlite3		*pf;
	sqlite3_backup	*pBackup;
	sqlite3_stmt	*stmt;

	INFO("Administrator backing up database: %s", zfile);

	db_tryopen();

	rc = sqlite3_open(zfile, &pf);
	if (SQLITE_OK != rc) {
		WARNX("sqlite3_open: %s", sqlite3_errmsg(pf));
		goto err;
	}

	pBackup = sqlite3_backup_init(pf, "main", db, "main");
	if (NULL == pBackup) {
		WARNX("sqlite3_backup_init: %s", sqlite3_errmsg(pf));
		goto err;
	}

	do {
		rc = sqlite3_backup_step(pBackup, 5);
		switch (rc) {
		case (SQLITE_OK):
		case (SQLITE_BUSY):
		case (SQLITE_LOCKED):
			sqlite3_sleep(250);
			break;
		default:
			break;
		}
	} while (rc == SQLITE_OK || 
		 rc == SQLITE_BUSY || 
		 rc == SQLITE_LOCKED);

	if (SQLITE_DONE != rc) {
		WARNX("sqlite3_backup_step: %s", sqlite3_errmsg(pf));
		goto err;
	}

	if (SQLITE_OK != sqlite3_backup_finish(pBackup)) {
		WARNX("sqlite3_backup_finish: %s", sqlite3_errmsg(pf));
		goto err;
	}

	/*
	 * Before we exit, let's scrub the database a little.
	 * Redact all password hashes (just in case), the SMTP stored
	 * password, and the administrator password.
	 */
	rc = sqlite3_prepare_v2(pf, 
		"UPDATE player SET hash=''", 
		-1, &stmt, NULL);

	if (SQLITE_OK != rc) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(pf));
		sqlite3_finalize(stmt);
		goto err;
	} else if (SQLITE_DONE != sqlite3_step(stmt)) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(pf));
		sqlite3_finalize(stmt);
		goto err;
	}
	sqlite3_finalize(stmt);

	rc = sqlite3_prepare_v2(pf, 
		"UPDATE admin SET hash=''", 
		-1, &stmt, NULL);

	if (SQLITE_OK != rc) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(pf));
		sqlite3_finalize(stmt);
		goto err;
	} else if (SQLITE_DONE != sqlite3_step(stmt)) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(pf));
		sqlite3_finalize(stmt);
		goto err;
	}
	sqlite3_finalize(stmt);

	rc = sqlite3_prepare_v2(pf, 
		"UPDATE smtp SET pass=''", 
		-1, &stmt, NULL);

	if (SQLITE_OK != rc) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(pf));
		sqlite3_finalize(stmt);
		goto err;
	} else if (SQLITE_DONE != sqlite3_step(stmt)) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(pf));
		sqlite3_finalize(stmt);
		goto err;
	}
	sqlite3_finalize(stmt);

	rc = sqlite3_prepare_v2(pf, 
		"UPDATE experiment SET awssecretkey=''", 
		-1, &stmt, NULL);

	if (SQLITE_OK != rc) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(pf));
		sqlite3_finalize(stmt);
		goto err;
	} else if (SQLITE_DONE != sqlite3_step(stmt)) {
		WARNX("sqlite3_prepare_v2: %s", sqlite3_errmsg(pf));
		sqlite3_finalize(stmt);
		goto err;
	}
	sqlite3_finalize(stmt);

	sqlite3_close(pf);
	INFO("Administrator backed up database: %s", zfile);
	return(1);
err:
	sqlite3_close(pf);
	if (-1 == remove(zfile))
		WARN("remove: %s", zfile);
	return(0);
}
