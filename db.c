/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include <sys/param.h>

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <kcgi.h>
#include <kcgijson.h>
#include <gmp.h>
#include <sqlite3.h>

#include "extern.h"

/*
 * The database, its location, and its statement (if any).
 * This is opened on-demand (see db_tryopen()) and closed automatically
 * on system exit (see the atexit(3) call).
 */
static sqlite3		*db;

static void
db_finalise(sqlite3_stmt *stmt)
{

	sqlite3_finalize(stmt);
}

void
db_close(void)
{

	if (NULL == db)
		return;
	if (SQLITE_OK != sqlite3_close(db))
		perror(sqlite3_errmsg(db));
	db = NULL;
}

/*
 * Check to see whether the database "db" has been initialised.
 * If so, return immediately.
 * If not, then first create the path to it, then try to open it.
 */
void
db_tryopen(void)
{
	size_t	 attempt;
	int	 rc;
	char	 dbpath[PATH_MAX];

	if (NULL != db)
		return;

	snprintf(dbpath, sizeof(dbpath), "%s/gamelab.db",
		NULL != getenv("DB_DIR") ? getenv("DB_DIR") : ".");

	/* Register exit hook for the destruction of the database. */
	if (-1 == atexit(db_close)) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	attempt = 0;
again:
	if (500 == attempt) {
		fprintf(stderr, "sqlite3_open: busy database\n");
		exit(EXIT_FAILURE);
	}

	rc = sqlite3_open(dbpath, &db);
	if (SQLITE_BUSY == rc) {
		fprintf(stderr, "sqlite3_open: "
			"busy database (%zu)\n", attempt);
		usleep(arc4random_uniform(10000));
		attempt++;
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		fprintf(stderr, "sqlite3_open: "
			"locked database (%zu)\n", attempt);
		usleep(arc4random_uniform(10000));
		attempt++;
		goto again;
	} else if (SQLITE_OK == rc) {
		sqlite3_busy_timeout(db, 500);
		return;
	} 

	fprintf(stderr, "sqlite3_open: %s\n", sqlite3_errmsg(db));
	exit(EXIT_FAILURE);
}

#define DB_STEP_CONSTRAINT 0x01

static int
db_step(sqlite3_stmt *stmt, unsigned int flags)
{
	int	 rc;
	size_t	 attempt = 0;

	assert(NULL != stmt);
	assert(NULL != db);
again:
	if (500 == attempt) {
		fprintf(stderr, "sqlite3_step: busy database\n");
		exit(EXIT_FAILURE);
	}

	rc = sqlite3_step(stmt);
	if (SQLITE_BUSY == rc) {
		fprintf(stderr, "sqlite3_step: "
			"busy database (%zu)\n", attempt);
		usleep(arc4random_uniform(10000));
		attempt++;
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		fprintf(stderr, "sqlite3_step: "
			"locked database (%zu)\n", attempt);
		usleep(arc4random_uniform(10000));
		attempt++;
		goto again;
	}

	if (SQLITE_DONE == rc || SQLITE_ROW == rc)
		return(rc);
	if (SQLITE_CONSTRAINT == rc && DB_STEP_CONSTRAINT & flags)
		return(rc);

	fprintf(stderr, "sqlite3_step: %s\n", sqlite3_errmsg(db));
	db_finalise(stmt);
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
	if (500 == attempt) {
		fprintf(stderr, "sqlite3_stmt: busy database\n");
		exit(EXIT_FAILURE);
	}

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

	if (SQLITE_BUSY == rc) {
		fprintf(stderr, "sqlite3_stmt: "
			"busy database (%zu)\n", attempt);
		usleep(arc4random_uniform(10000));
		attempt++;
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		fprintf(stderr, "sqlite3_stmt: "
			"locked database (%zu)\n", attempt);
		usleep(arc4random_uniform(10000));
		attempt++;
		goto again;
	} else if (SQLITE_OK == rc)
		return(stmt);

	fprintf(stderr, "sqlite3_stmt: %s\n", sqlite3_errmsg(db));
	db_finalise(stmt);
	exit(EXIT_FAILURE);
}

static void
db_bind_text(sqlite3_stmt *stmt, size_t pos, const char *val)
{

	assert(pos > 0);
	if (SQLITE_OK == sqlite3_bind_text
		(stmt, pos, val, -1, SQLITE_STATIC))
		return;
	fprintf(stderr, "sqlite3_bind_text: %s\n", sqlite3_errmsg(db));
	db_finalise(stmt);
	exit(EXIT_FAILURE);
}

static void
db_bind_double(sqlite3_stmt *stmt, size_t pos, double val)
{

	assert(pos > 0);
	if (SQLITE_OK == sqlite3_bind_double(stmt, pos, val))
		return;
	fprintf(stderr, "sqlite3_bind_double: %s\n", sqlite3_errmsg(db));
	db_finalise(stmt);
	exit(EXIT_FAILURE);
}

static void
db_bind_int(sqlite3_stmt *stmt, size_t pos, int64_t val)
{

	assert(pos > 0);
	if (SQLITE_OK == sqlite3_bind_int64(stmt, pos, val))
		return;
	fprintf(stderr, "sqlite3_bind_int64: %s\n", sqlite3_errmsg(db));
	db_finalise(stmt);
	exit(EXIT_FAILURE);
}

static void
db_exec(const char *sql)
{
	size_t	attempt = 0;
	int	rc;

again:
	if (500 == attempt) {
		fprintf(stderr, "sqlite3_exec: busy database\n");
		exit(EXIT_FAILURE);
	}

	assert(NULL != db);
	rc = sqlite3_exec(db, sql, NULL, NULL, NULL);

	if (SQLITE_BUSY == rc) {
		fprintf(stderr, "sqlite3_exec: "
			"busy database (%zu)\n", attempt);
		usleep(arc4random_uniform(10000));
		attempt++;
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		fprintf(stderr, "sqlite3_exec: "
			"locked database (%zu)\n", attempt);
		usleep(arc4random_uniform(10000));
		attempt++;
		goto again;
	} else if (SQLITE_OK == rc)
		return;

	fprintf(stderr, "sqlite3_exec: %s\n", sqlite3_errmsg(db));
	exit(EXIT_FAILURE);
}

static void
db_trans_begin(void)
{

	db_tryopen();
	db_exec("BEGIN TRANSACTION");
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

void
db_sess_delete(int64_t id)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt
		("DELETE FROM sess WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	db_finalise(stmt);
}

int
db_player_sess_valid(int64_t *playerid, int64_t id, int64_t cookie)
{
	int	 	 rc;
	sqlite3_stmt	*stmt;

	stmt = db_stmt
		("SELECT playerid FROM sess "
		 "WHERE id=? AND cookie=? AND playerid IS NOT NULL");
	db_bind_int(stmt, 1, id);
	db_bind_int(stmt, 2, cookie);
	if (SQLITE_ROW == (rc = db_step(stmt, 0))) {
		*playerid = sqlite3_column_int(stmt, 0);
		db_finalise(stmt);
		db_player_set_state(*playerid, PSTATE_LOGGEDIN);
	} else
		db_finalise(stmt);

	return(SQLITE_ROW == rc);
}

int
db_admin_sess_valid(int64_t id, int64_t cookie)
{
	int	 	 rc;
	sqlite3_stmt	*stmt;

	stmt = db_stmt
		("SELECT * FROM sess "
		 "WHERE id=? AND cookie=? AND playerid IS NULL");
	db_bind_int(stmt, 1, id);
	db_bind_int(stmt, 2, cookie);
	rc = db_step(stmt, 0);
	db_finalise(stmt);
	return(SQLITE_ROW == rc);
}

struct sess *
db_player_sess_alloc(int64_t playerid)
{
	sqlite3_stmt	*stmt;
	struct sess	*sess;

	sess = calloc(1, sizeof(struct sess));
	assert(NULL != sess);
	sess->cookie = arc4random();
	stmt = db_stmt("INSERT INTO sess "
		"(cookie,playerid) VALUES(?,?)");
	db_bind_int(stmt, 1, sess->cookie);
	db_bind_int(stmt, 2, playerid);
	db_step(stmt, 0);
	db_finalise(stmt);
	sess->id = sqlite3_last_insert_rowid(db);
	fprintf(stderr, "%" PRId64 ": new player session\n", sess->id);
	return(sess);
}

struct sess *
db_admin_sess_alloc(void)
{
	sqlite3_stmt	*stmt;
	struct sess	*sess;

	sess = calloc(1, sizeof(struct sess));
	assert(NULL != sess);
	sess->cookie = arc4random();
	stmt = db_stmt("INSERT INTO sess (cookie) VALUES(?)");
	db_bind_int(stmt, 1, sess->cookie);
	db_step(stmt, 0);
	db_finalise(stmt);
	sess->id = sqlite3_last_insert_rowid(db);
	fprintf(stderr, "%" PRId64 ": new admin session\n", sess->id);
	return(sess);
}

void
db_sess_free(struct sess *sess)
{

	free(sess);
}

static const char *
db_crypt_hash(const char *pass)
{

	return(pass);
}

static char *
db_crypt_mkpass(void)
{
	char	*pass;
	size_t	 i;
	
	pass = kmalloc(17);
	for (i = 0; i < 16; i++)
		pass[i] = (arc4random() % 26) + 97;
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

	stmt = db_stmt("UPDATE admin SET hash=?");
	db_bind_text(stmt, 1, db_crypt_hash(pass));
	db_step(stmt, 0);
	db_finalise(stmt);
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
	db_finalise(stmt);
	return(mail);
}

void
db_admin_set_mail(const char *email)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE admin SET email=?");
	db_bind_text(stmt, 1, email);
	db_step(stmt, 0);
	db_finalise(stmt);
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
		fprintf(stderr, "%s: not admin password\n", pass);
		db_finalise(stmt);
		return(0);
	}

	db_finalise(stmt);
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
	db_finalise(stmt);
	return(SQLITE_ROW == rc);
}

int
db_player_valid(int64_t *id, const char *mail, const char *pass)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("SELECT hash,id FROM player WHERE email=?");
	db_bind_text(stmt, 1, mail);

	if (SQLITE_ROW != db_step(stmt, 0)) {
		db_finalise(stmt);
		return(0);
	} 

	if ( ! db_crypt_check(sqlite3_column_text(stmt, 0), pass)) {
		db_finalise(stmt);
		return(0);
	}

	*id = sqlite3_column_int(stmt, 1);
	db_finalise(stmt);
	return(1);
}

int
db_admin_valid(const char *email, const char *pass)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("SELECT hash FROM admin WHERE email=?");
	db_bind_text(stmt, 1, email);

	if (SQLITE_ROW != db_step(stmt, 0)) {
		fprintf(stderr, "%s: not admin\n", email);
		db_finalise(stmt);
		return(0);
	} 

	if ( ! db_crypt_check(sqlite3_column_text(stmt, 0), pass)) {
		fprintf(stderr, "%s: not admin password\n", pass);
		db_finalise(stmt);
		return(0);
	}

	db_finalise(stmt);
	return(1);
}

void
db_game_free(struct game *game)
{
	size_t	 i;

	if (NULL == game)
		return;

	for (i = 0; i < (size_t)(game->p1 * game->p2 * 2); i++)
		mpq_clear(game->payoffs[i]);
	free(game->payoffs);
	free(game->name);
	free(game);
}

/*
 * This should only be called on rationals that are in the database,
 * i.e., those that have been canonicalised and validated.
 */
static mpq_t *
db_payoff_to_mpq(char *buf, int64_t p1, int64_t p2)
{
	mpq_t	*rops;
	size_t	 i, count;
	int	 rc;
	char	*tok;

	rops = kcalloc(p1 * p2 * 2, sizeof(mpq_t));

	for (i = 0; i < (size_t)(p1 * p2 * 2); i++)
		mpq_init(rops[i]);

	count = 0;
	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		assert(count < (size_t)(p1 * p2 * 2));
		rc = mpq_set_str(rops[count], tok, 10);
		assert(0 == rc);
		mpq_canonicalize(rops[count]);
		assert(0 == rc);
		count++;
	}

	assert(count == (size_t)(p1 * p2 * 2));
	return(rops);
}

size_t
db_game_count_all(void)
{
	sqlite3_stmt	*stmt;
	size_t		 count;

	count = 0;
	stmt = db_stmt("SELECT count(*) FROM game");
	if (SQLITE_ROW != db_step(stmt, 0)) {
		db_finalise(stmt);
		return(0);
	}

	count = (size_t)sqlite3_column_int(stmt, 0);
	db_finalise(stmt);
	return(count);
}

size_t
db_player_count_all(void)
{
	sqlite3_stmt	*stmt;
	size_t		 count;

	count = 0;
	stmt = db_stmt("SELECT count(*) FROM player");
	if (SQLITE_ROW != db_step(stmt, 0)) {
		db_finalise(stmt);
		return(0);
	}

	count = (size_t)sqlite3_column_int(stmt, 0);
	db_finalise(stmt);
	return(count);
}

void
db_player_free(struct player *player)
{

	if (NULL == player)
		return;
	free(player->mail);
	free(player);
}

struct player *
db_player_load(int64_t id)
{
	sqlite3_stmt	*stmt;
	struct player	*player;

	stmt = db_stmt("SELECT email,state,id,enabled,role "
		"FROM player WHERE id=?");
	db_bind_int(stmt, 1, id);
	player = NULL;

	if (SQLITE_ROW == db_step(stmt, 0)) {
		player = kcalloc(1, sizeof(struct player));
		player->mail = kstrdup
			((char *)sqlite3_column_text(stmt, 0));
		player->state = sqlite3_column_int(stmt, 1);
		player->id = sqlite3_column_int(stmt, 2);
		player->enabled = sqlite3_column_int(stmt, 3);
		player->role = sqlite3_column_int(stmt, 4);
	} 

	db_finalise(stmt);
	return(player);
}

void
db_player_load_all(playerf fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct player	 player;

	stmt = db_stmt("SELECT email,state,id,enabled,role FROM player");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		memset(&player, 0, sizeof(struct player));
		player.mail = kstrdup
			((char *)sqlite3_column_text(stmt, 0));
		player.state = sqlite3_column_int(stmt, 1);
		player.id = sqlite3_column_int(stmt, 2);
		player.enabled = sqlite3_column_int(stmt, 3);
		player.role = sqlite3_column_int(stmt, 4);
		(*fp)(&player, arg);
		free(player.mail);
	}

	db_finalise(stmt);
}

/*
 * The "plays" values should be canonicalised from mpq_t prior to being
 * passed here.
 */
int
db_player_play(int64_t playerid, int64_t round, 
	int64_t gameid, const char *const *plays, 
	size_t sz, size_t choice, double rr)
{
	struct expr	*expr;
	time_t		 t;
	sqlite3_stmt	*stmt;
	size_t		 i;
	int		 rc;

	/*
	 * Check this outside of a transaction.
	 * The values needn't be completely "on time" to the
	 * microsecond and this doesn't change in the database.
	 */
	t = time(NULL);
	expr = db_expr_get();
	assert(NULL != expr);
	if (round > expr->rounds) {
		db_expr_free(expr);
		return(0);
	} else if (round != (t - expr->start) / (expr->minutes * 60)) {
		db_expr_free(expr);
		return(0);
	}
	db_expr_free(expr);

	/*
	 * Uniquely create our choice.
	 * We do this before setting the play choices, because we've
	 * basically "locked" what follows into the original player.
	 */
	stmt = db_stmt("INSERT INTO choice "
		"(round,playerid,gameid,strategy,randr) "
		"VALUES (?,?,?,?,?)");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, playerid);
	db_bind_int(stmt, 3, gameid);
	db_bind_int(stmt, 4, choice);
	db_bind_double(stmt, 5, rr);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	db_finalise(stmt);
	if (SQLITE_CONSTRAINT == rc)
		return(0);

	/*
	 * Insert all of our play choices.
	 */
	stmt = db_stmt("INSERT into play "
		"(fraction, round, strategy, playerid, gameid) "
		"VALUES (?, ?, ?, ?, ?)");
	for (i = 0; i < sz; i++) {
		db_bind_text(stmt, 1, plays[i]);
		db_bind_int(stmt, 2, round);
		db_bind_int(stmt, 3, i);
		db_bind_int(stmt, 4, playerid);
		db_bind_int(stmt, 5, gameid);
		db_step(stmt, 0);
		sqlite3_reset(stmt);
	}
	db_finalise(stmt);
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
	db_finalise(stmt);
	return(rc == SQLITE_ROW);
}

void
db_game_load_player(int64_t playerid, int64_t round, gamef fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct game	 game;
	int64_t		 id;
	size_t		 i;
	char		*sv;

	stmt = db_stmt("SELECT payoffs,p1,p2,name,id FROM game");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		id = sqlite3_column_int(stmt, 4);
		if (db_game_check_player(playerid, round, id))
			continue;
		memset(&game, 0, sizeof(struct game));
		sv = strdup((char *)sqlite3_column_text(stmt, 0));
		assert(NULL != sv);
		game.p1 = sqlite3_column_int(stmt, 1);
		game.p2 = sqlite3_column_int(stmt, 2);
		game.payoffs = db_payoff_to_mpq(sv, game.p1, game.p2);
		assert(NULL != game.payoffs);
		game.name = strdup((char *)sqlite3_column_text(stmt, 3));
		assert(NULL != game.name);
		game.id = sqlite3_column_int(stmt, 4);
		(*fp)(&game, arg);
		for (i = 0; i < (size_t)(2 * game.p1 * game.p2); i++)
			mpq_clear(game.payoffs[i]);
		free(game.payoffs);
		free(game.name);
		free(sv);
	}

	db_finalise(stmt);
}

struct game *
db_game_load(int64_t gameid)
{
	sqlite3_stmt	*stmt;
	struct game	*game;
	char		*sv;

	game = NULL;
	stmt = db_stmt("SELECT payoffs,p1,p2,"
		"name,id FROM game WHERE id=?");
	db_bind_int(stmt, 1, gameid);

	if (SQLITE_ROW == db_step(stmt, 0)) {
		game = kcalloc(1, sizeof(struct game));
		sv = kstrdup((char *)sqlite3_column_text(stmt, 0));
		game->p1 = sqlite3_column_int(stmt, 1);
		game->p2 = sqlite3_column_int(stmt, 2);
		game->payoffs = db_payoff_to_mpq(sv, game->p1, game->p2);
		game->name = kstrdup((char *)sqlite3_column_text(stmt, 3));
		game->id = sqlite3_column_int(stmt, 4);
		free(sv);
	}

	db_finalise(stmt);
	return(game);
}

void
db_game_load_all(gamef fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct game	 game;
	size_t		 i;
	char		*sv;

	stmt = db_stmt("SELECT payoffs,p1,p2,name,id FROM game");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		memset(&game, 0, sizeof(struct game));
		sv = kstrdup((char *)sqlite3_column_text(stmt, 0));
		game.p1 = sqlite3_column_int(stmt, 1);
		game.p2 = sqlite3_column_int(stmt, 2);
		game.payoffs = db_payoff_to_mpq(sv, game.p1, game.p2);
		game.name = kstrdup((char *)sqlite3_column_text(stmt, 3));
		game.id = sqlite3_column_int(stmt, 4);
		(*fp)(&game, arg);
		for (i = 0; i < (size_t)(2 * game.p1 * game.p2); i++)
			mpq_clear(game.payoffs[i]);
		free(game.payoffs);
		free(game.name);
		free(sv);
	}

	db_finalise(stmt);
}

struct game *
db_game_alloc(const char *poffs, 
	const char *name, int64_t p1, int64_t p2)
{
	char		*buf, *tok, *sv;
	mpq_t		*rops;
	size_t		 i, count;
	sqlite3_stmt	*stmt;
	struct game	*game;

	if (p2 && p1 > INT64_MAX / p2) {
		fprintf(stderr, "%s: integer overflow: %"
			PRId64 ", %" PRId64 "\n", name, p1, p2);
		return(NULL);
	}

	sv = buf = kstrdup(poffs);
	rops = kcalloc(p1 * p2 * 2, sizeof(mpq_t));

	for (i = 0; i < (size_t)(p1 * p2 * 2); i++)
		mpq_init(rops[i]);

	count = 0;
	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		if (count >= (size_t)(p1 * p2 * 2)) {
			fprintf(stderr, "%s: matrix too big", name);
			free(sv);
			free(rops);
			return(NULL);
		} else if (-1 != mpq_set_str(rops[count], tok, 10)) {
			mpq_canonicalize(rops[count++]);
			continue;
		}
		fprintf(stderr, "%s: bad payoff: %s\n", name, tok);
		free(sv);
		free(rops);
		return(NULL);
	}
	free(sv);

	if (count != (size_t)(p1 * p2 * 2)) {
		fprintf(stderr, "%s: matrix size: %zu != %" 
			PRId64 "\n", name, count, 2 * p1 * p2);
		free(rops);
		return(NULL);
	}

	db_trans_begin();
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		return(NULL);
	}

	game = kcalloc(1, sizeof(struct game));
	game->payoffs = rops;
	game->p1 = p1;
	game->p2 = p2;
	game->name = kstrdup(name);

	stmt = db_stmt("INSERT INTO game "
		"(payoffs, p1, p2, name) VALUES(?,?,?,?)");
	db_bind_text(stmt, 1, poffs);
	db_bind_int(stmt, 2, game->p1);
	db_bind_int(stmt, 3, game->p2);
	db_bind_text(stmt, 4, game->name);
	db_step(stmt, 0);
	db_finalise(stmt);
	game->id = sqlite3_last_insert_rowid(db);
	db_trans_commit();

	fprintf(stderr, "%" PRId64 ": new game: %s\n", 
		game->id, game->name);
	return(game);
}

int
db_player_create(const char *email)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 id;

	db_trans_begin();
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		return(0);
	}

	stmt = db_stmt("INSERT INTO player "
		"(email,role) VALUES (?,?)");
	db_bind_text(stmt, 1, email);
	db_bind_int(stmt, 2, arc4random_uniform(2));
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	db_finalise(stmt);

	if (SQLITE_DONE == rc) {
		id = sqlite3_last_insert_rowid(db);
		fprintf(stderr, "%" PRId64 ": new player: %s\n",
			sqlite3_last_insert_rowid(db), email);
	}

	db_trans_commit();
	return(1);
}

int
db_expr_checkstate(enum estate state)
{
	sqlite3_stmt	*stmt;
	int		 rc;

	stmt = db_stmt("SELECT * FROM experiment WHERE state=?");
	db_bind_int(stmt, 1, state);
	rc = db_step(stmt, 0);
	db_finalise(stmt);

	return(rc == SQLITE_ROW);
}

int
db_expr_start(int64_t date, int64_t rounds, 
	int64_t minutes, const char *uri)
{
	sqlite3_stmt	*stmt;

	db_trans_begin();
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		return(0);
	}

	stmt = db_stmt("UPDATE experiment SET "
		"state=1,start=?,rounds=?,minutes=?,loginuri=?");
	db_bind_int(stmt, 1, date);
	db_bind_int(stmt, 2, rounds);
	db_bind_int(stmt, 3, minutes);
	db_bind_text(stmt, 4, uri);
	db_step(stmt, 0);
	db_finalise(stmt);
	db_trans_commit();
	fprintf(stderr, "experiment starting\n");
	return(1);
}

void
db_player_enable(int64_t id)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET enabled=1 WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	db_finalise(stmt);
	fprintf(stderr, "%" PRId64 ": player enabled\n", id);
}

void
db_player_reset_error(void)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET state=0 WHERE state=3");
	db_step(stmt, 0);
	db_finalise(stmt);
}

void
db_player_set_state(int64_t id, enum pstate state)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET state=? WHERE id=?");
	db_bind_int(stmt, 1, state);
	db_bind_int(stmt, 2, id);
	db_step(stmt, 0);
	db_finalise(stmt);
}

void
db_player_set_mailed(int64_t id, const char *pass)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET state=?,hash=? WHERE id=?");
	db_bind_int(stmt, 1, PSTATE_MAILED);
	db_bind_text(stmt, 2, db_crypt_hash(pass));
	db_bind_int(stmt, 3, id);
	db_step(stmt, 0);
	db_finalise(stmt);
}

char *
db_player_next_new(int64_t *id, char **pass)
{
	sqlite3_stmt	*stmt;
	char		*email;

	email = *pass = NULL;
	stmt = db_stmt("SELECT email,id FROM player "
		"WHERE state=0 AND enabled=1 LIMIT 1");

	if (SQLITE_ROW == db_step(stmt, 0)) {
		email = kstrdup((char *)sqlite3_column_text(stmt, 0));
		*id = sqlite3_column_int64(stmt, 1);
		*pass = db_crypt_mkpass();
	} 

	db_finalise(stmt);
	return(email);
}

int
db_game_delete(int64_t id)
{
	sqlite3_stmt	*stmt;

	db_trans_begin();
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		return(0);
	}

	stmt = db_stmt("DELETE FROM game WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	db_finalise(stmt);
	db_trans_commit();
	fprintf(stderr, "%" PRId64 ": game deleted\n", id);
	return(1);
}

int
db_player_delete(int64_t id)
{
	sqlite3_stmt	*stmt;

	db_trans_begin();
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		return(0);
	}

	stmt = db_stmt("DELETE FROM player WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	db_finalise(stmt);
	db_trans_commit();
	fprintf(stderr, "%" PRId64 ": player deleted\n", id);
	return(1);
}

void
db_player_disable(int64_t id)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET enabled=0 WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	db_finalise(stmt);
	fprintf(stderr, "%" PRId64 ": player disabled\n", id);
}

struct smtp *
db_smtp_get(void)
{
	struct smtp	*smtp;
	sqlite3_stmt	*stmt;

	smtp = NULL;
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

	db_finalise(stmt);
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
	db_finalise(stmt);
}

struct expr *
db_expr_get(void)
{
	sqlite3_stmt	*stmt;
	struct expr	*expr;
	int		 rc;

	/* FIXME: check that game has started. */
	expr = kcalloc(1, sizeof(struct expr));
	stmt = db_stmt("SELECT start,rounds,minutes,loginuri "
		"FROM experiment");
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);
	expr->start = (time_t)sqlite3_column_int(stmt, 0);
	expr->rounds = sqlite3_column_int(stmt, 1);
	expr->minutes = sqlite3_column_int(stmt, 2);
	expr->loginuri = kstrdup((char *)sqlite3_column_text(stmt, 3));
	expr->end = expr->start + (expr->rounds * expr->minutes * 60);

	db_finalise(stmt);
	return(expr);
}

void
db_expr_free(struct expr *expr)
{
	if (NULL == expr)
		return;

	free(expr->loginuri);
	free(expr);
}
