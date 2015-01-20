/*	$Id$ */
/*
 * Copyright (c) 2014, 2015 Kristaps Dzonsons <kristaps@kcons.eu>
 */
#include "config.h" 

#include <sys/param.h>

#include <assert.h>
#include <inttypes.h>
#include <math.h>
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

void
db_close(void)
{

	if (NULL == db)
		return;
	if (SQLITE_OK != sqlite3_close(db))
		perror(sqlite3_errmsg(db));
	db = NULL;
}

void
db_tryopen(void)
{
	size_t	 attempt;
	int	 rc;

	if (NULL != db)
		return;

	/* Register exit hook for the destruction of the database. */
	if (-1 == atexit(db_close)) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	attempt = 0;
again:
	if (100 == attempt) {
		fprintf(stderr, "sqlite3_open: busy database\n");
		exit(EXIT_FAILURE);
	}

	rc = sqlite3_open(DATADIR "/gamelab.db", &db);
	if (SQLITE_BUSY == rc) {
		fprintf(stderr, "sqlite3_open: "
			"busy database (%zu)\n", attempt);
		usleep(arc4random_uniform(100000));
		attempt++;
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		fprintf(stderr, "sqlite3_open: "
			"locked database (%zu)\n", attempt);
		usleep(arc4random_uniform(100000));
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
	if (100 == attempt) {
		fprintf(stderr, "sqlite3_step: busy database\n");
		exit(EXIT_FAILURE);
	}

	rc = sqlite3_step(stmt);
	if (SQLITE_BUSY == rc) {
		fprintf(stderr, "sqlite3_step: "
			"busy database (%zu)\n", attempt);
		usleep(arc4random_uniform(100000));
		attempt++;
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		fprintf(stderr, "sqlite3_step: "
			"locked database (%zu)\n", attempt);
		usleep(arc4random_uniform(100000));
		attempt++;
		goto again;
	}

	if (SQLITE_DONE == rc || SQLITE_ROW == rc)
		return(rc);
	if (SQLITE_CONSTRAINT == rc && DB_STEP_CONSTRAINT & flags)
		return(rc);

	fprintf(stderr, "sqlite3_step: %s\n", sqlite3_errmsg(db));
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
	if (100 == attempt) {
		fprintf(stderr, "sqlite3_stmt: busy database\n");
		exit(EXIT_FAILURE);
	}

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

	if (SQLITE_BUSY == rc) {
		fprintf(stderr, "sqlite3_stmt: "
			"busy database (%zu)\n", attempt);
		usleep(arc4random_uniform(100000));
		attempt++;
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		fprintf(stderr, "sqlite3_stmt: "
			"locked database (%zu)\n", attempt);
		usleep(arc4random_uniform(100000));
		attempt++;
		goto again;
	} else if (SQLITE_OK == rc)
		return(stmt);

	fprintf(stderr, "sqlite3_stmt: %s\n", sqlite3_errmsg(db));
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
	fprintf(stderr, "sqlite3_bind_text: %s\n", sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	exit(EXIT_FAILURE);
}

static void
db_bind_int(sqlite3_stmt *stmt, size_t pos, int64_t val)
{

	assert(pos > 0);
	if (SQLITE_OK == sqlite3_bind_int64(stmt, pos, val))
		return;
	fprintf(stderr, "sqlite3_bind_int64: %s\n", sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	exit(EXIT_FAILURE);
}

static void
db_exec(const char *sql)
{
	size_t	attempt = 0;
	int	rc;

again:
	if (100 == attempt) {
		fprintf(stderr, "sqlite3_exec: busy database\n");
		exit(EXIT_FAILURE);
	}

	assert(NULL != db);
	rc = sqlite3_exec(db, sql, NULL, NULL, NULL);

	if (SQLITE_BUSY == rc) {
		fprintf(stderr, "sqlite3_exec: "
			"busy database (%zu)\n", attempt);
		usleep(arc4random_uniform(100000));
		attempt++;
		goto again;
	} else if (SQLITE_LOCKED == rc) {
		fprintf(stderr, "sqlite3_exec: "
			"locked database (%zu)\n", attempt);
		usleep(arc4random_uniform(100000));
		attempt++;
		goto again;
	} else if (SQLITE_OK == rc)
		return;

	fprintf(stderr, "sqlite3_exec: %s\n", sqlite3_errmsg(db));
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
	size_t		 count;

	(void)snprintf(buf, sizeof(buf),
		"SELECT count(*) FROM %s", p);
	stmt = db_stmt(buf);
	db_step(stmt, 0);
	count = (size_t)sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return(count);
}

/*
 * Assign the string value into "val".
 * NOTE: "val" is initialised here!
 */
static void
db_str2mpq_single(const unsigned char *v, mpq_t val)
{
	int	 rc;

	mpq_init(val);
	rc = mpq_set_str(val, (const char *)v, 10);
	assert(0 == rc);
	mpq_canonicalize(val);
}

static mpq_t *
db_str2mpq(const unsigned char *v, size_t sz)
{
	mpq_t	*p;
	size_t	 i;
	char	*buf, *tok, *sv;
	int	 rc;

	p = kcalloc(sz, sizeof(mpq_t));

	if (1 == sz) {
		mpq_init(p[0]);
		rc = mpq_set_str(p[0], (const char *)v, 10);
		assert(0 == rc);
		mpq_canonicalize(p[0]);
		return(p);
	}

	buf = sv = kstrdup((const char *)v);

	i = 0;
	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		assert(i < sz);
		mpq_init(p[i]);
		rc = mpq_set_str(p[i], tok, 10);
		assert(0 == rc);
		mpq_canonicalize(p[i]);
		i++;
	}

	free(sv);
	return(p);
}

void
db_sess_delete(int64_t id)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("DELETE FROM sess WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
}

int
db_player_sess_valid(int64_t *playerid, int64_t id, int64_t cookie)
{
	int	 	 rc;
	sqlite3_stmt	*stmt;

	stmt = db_stmt
		("SELECT playerid FROM sess "
		 "INNER JOIN player ON player.id=sess.playerid "
		 "WHERE sess.id=? AND sess.cookie=? AND "
		 "sess.playerid IS NOT NULL AND player.enabled=1");
	db_bind_int(stmt, 1, id);
	db_bind_int(stmt, 2, cookie);
	if (SQLITE_ROW == (rc = db_step(stmt, 0))) {
		*playerid = sqlite3_column_int(stmt, 0);
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
	sqlite3_finalize(stmt);
	sess->id = sqlite3_last_insert_rowid(db);
	fprintf(stderr, "Player %" PRId64 " logged in, "
		"session %" PRId64 "\n", playerid, sess->id);
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
	sqlite3_finalize(stmt);
	sess->id = sqlite3_last_insert_rowid(db);
	fprintf(stderr, "Administrator logged in, "
		"session %" PRId64 "\n", sess->id);
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
	sqlite3_finalize(stmt);
	fprintf(stderr, "Administrator set password\n");
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

	stmt = db_stmt("UPDATE admin SET email=?");
	db_bind_text(stmt, 1, email);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	fprintf(stderr, "Administrator set email: %s\n", email);
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
		fprintf(stderr, "Administrator failed login\n");
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

int
db_player_valid(int64_t *id, const char *mail, const char *pass)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("SELECT hash,id FROM player WHERE email=?");
	db_bind_text(stmt, 1, mail);

	if (SQLITE_ROW != db_step(stmt, 0)) {
		fprintf(stderr, "Player (%s) failed "
			"login: no such player\n", mail);
		sqlite3_finalize(stmt);
		return(0);
	} 

	*id = sqlite3_column_int(stmt, 1);

	if ( ! db_crypt_check(sqlite3_column_text(stmt, 0), pass)) {
		fprintf(stderr, "Player %" PRId64 " (%s) failed "
			"login: bad password\n", *id, mail);
		sqlite3_finalize(stmt);
		return(0);
	}

	sqlite3_finalize(stmt);
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
		sqlite3_finalize(stmt);
		return(0);
	} 

	if ( ! db_crypt_check(sqlite3_column_text(stmt, 0), pass)) {
		fprintf(stderr, "%s: not admin password\n", pass);
		sqlite3_finalize(stmt);
		return(0);
	}

	sqlite3_finalize(stmt);
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
	struct player	*player = NULL;

	stmt = db_stmt("SELECT email,state,id,enabled,"
		"role,rseed FROM player WHERE id=?");
	db_bind_int(stmt, 1, id);

	if (SQLITE_ROW == db_step(stmt, 0)) {
		player = kcalloc(1, sizeof(struct player));
		player->mail = kstrdup
			((char *)sqlite3_column_text(stmt, 0));
		player->state = sqlite3_column_int(stmt, 1);
		player->id = sqlite3_column_int(stmt, 2);
		player->enabled = sqlite3_column_int(stmt, 3);
		player->role = sqlite3_column_int(stmt, 4);
		player->rseed = sqlite3_column_int(stmt, 5);
	} 

	sqlite3_finalize(stmt);
	return(player);
}

void
db_player_load_all(playerf fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct player	 player;

	stmt = db_stmt("SELECT email,state,id,"
		"enabled,role FROM player");
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

	sqlite3_finalize(stmt);
}

/*
 * The "plays" values should be canonicalised from mpq_t prior to being
 * passed here.
 */
int
db_player_play(int64_t playerid, int64_t round, 
	int64_t gameid, mpq_t *plays, size_t sz)
{
	struct expr	*expr;
	time_t		 t;
	sqlite3_stmt	*stmt;
	int		 rc;
	char		*buf;

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
		fprintf(stderr, "Player %" PRId64 " tried playing "
			"game %" PRId64 " (round %" PRId64 "), but "
			"round invalid (max %" PRId64 ")\n", 
			playerid, gameid, round, expr->rounds);
		return(0);
	} else if (round != (t - expr->start) / (expr->minutes * 60)) {
		fprintf(stderr, "Player %" PRId64 " tried playing "
			"game %" PRId64 " (round %" PRId64 "), but "
			"round invalid (at %" PRId64 ")\n", 
			playerid, gameid, round, 
			(t - expr->start) / (expr->minutes * 60));
		db_expr_free(expr);
		return(0);
	}
	db_expr_free(expr);

	buf = mpq_mpq2str(plays, sz);
	stmt = db_stmt("INSERT INTO choice "
		"(round,playerid,gameid,strats) "
		"VALUES (?,?,?,?)");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, playerid);
	db_bind_int(stmt, 3, gameid);
	db_bind_text(stmt, 4, buf);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);
	free(buf);
	if (SQLITE_CONSTRAINT == rc) {
		fprintf(stderr, "Player %" PRId64 " tried "
			"playing game %" PRId64 " (round %" 
			PRId64 "), but has already played\n",
			playerid, gameid, round);
		return(0);
	}

	fprintf(stderr, "Player %" PRId64 " has played "
		"game %" PRId64 " (round %" PRId64 ")\n",
		playerid, gameid, round);

	stmt = db_stmt("INSERT INTO gameplay "
		"(round,playerid) VALUES (?,?)");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, playerid);
	db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);

	stmt = db_stmt("UPDATE gameplay "
		"SET choices=choices + 1 "
		"WHERE round=? AND playerid=?");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, playerid);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
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
		id = sqlite3_column_int(stmt, 4);
		if (db_game_check_player(playerid, round, id))
			continue;
		memset(&game, 0, sizeof(struct game));
		game.p1 = sqlite3_column_int(stmt, 1);
		game.p2 = sqlite3_column_int(stmt, 2);
		maxcount = game.p1 * game.p2 * 2;
		game.payoffs = db_str2mpq
			(sqlite3_column_text(stmt, 0), maxcount);
		game.name = strdup((char *)sqlite3_column_text(stmt, 3));
		assert(NULL != game.name);
		game.id = sqlite3_column_int(stmt, 4);
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
	result = (size_t)sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return(result);
}

size_t
db_game_round_count(int64_t gameid, int64_t round, int64_t role)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 result;

	stmt = db_stmt("SELECT count(*) FROM choice "
		"INNER JOIN player ON player.id = choice.playerid "
		"WHERE round=? AND gameid=? AND player.role = ?");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, gameid);
	db_bind_int(stmt, 3, role);
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);
	result = (size_t)sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return(result);
}

struct game *
db_game_load(int64_t gameid)
{
	sqlite3_stmt	*stmt;
	struct game	*game = NULL;

	stmt = db_stmt("SELECT payoffs,p1,p2,"
		"name,id FROM game WHERE id=?");
	db_bind_int(stmt, 1, gameid);

	if (SQLITE_ROW == db_step(stmt, 0)) {
		game = kcalloc(1, sizeof(struct game));
		game->p1 = sqlite3_column_int(stmt, 1);
		game->p2 = sqlite3_column_int(stmt, 2);
		game->payoffs = db_str2mpq
			(sqlite3_column_text(stmt, 0), 
			 game->p1 * game->p2 * 2);
		game->name = kstrdup((char *)
			sqlite3_column_text(stmt, 3));
		game->id = sqlite3_column_int(stmt, 4);
	}

	sqlite3_finalize(stmt);
	return(game);
}

void
db_game_load_all(gamef fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct game	 game;
	size_t		 i;

	stmt = db_stmt("SELECT payoffs,p1,p2,"
		"name,id FROM game ORDER BY id");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		memset(&game, 0, sizeof(struct game));
		game.p1 = sqlite3_column_int(stmt, 1);
		game.p2 = sqlite3_column_int(stmt, 2);
		game.payoffs = db_str2mpq
			(sqlite3_column_text(stmt, 0), 
			 game.p1 * game.p2 * 2);
		game.name = kstrdup((char *)
			sqlite3_column_text(stmt, 3));
		game.id = sqlite3_column_int(stmt, 4);
		(*fp)(&game, arg);
		for (i = 0; i < (size_t)(2 * game.p1 * game.p2); i++)
			mpq_clear(game.payoffs[i]);
		free(game.payoffs);
		free(game.name);
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

	if (p2 && p1 > INT64_MAX / p2) {
		fprintf(stderr, "Game allocated with "
			"too many strategies: %" PRId64 
			", %" PRId64 "\n", p1, p2);
		return(NULL);
	}

	sv = buf = kstrdup(poffs);
	maxcount = p1 * p2 * 2;
	rops = kcalloc(maxcount, sizeof(mpq_t));
	count = 0;

	while (NULL != (tok = strsep(&buf, " \t\n\r")))
		if (count >= maxcount) {
			fprintf(stderr, "Game allocated with too "
				"many strategies: %zu >= %zu\n",
				count, maxcount);
			goto err;
		} else if ( ! mpq_str2mpq(tok, rops[count++])) {
			fprintf(stderr, "Game allocated with "
				"bad rational %s\n", tok);
			goto err;
		}

	free(sv);
	sv = NULL;

	if (count != (size_t)(p1 * p2 * 2))
		goto err;

	db_trans_begin(0);
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		fprintf(stderr, "Game allocated in bad state\n");
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
	fprintf(stderr, "Game %" PRId64 " created\n", game->id);
	return(game);
err:
	free(sv);
	for (i = 0; i < count; i++)
		mpq_clear(rops[i]);
	free(rops);
	return(NULL);
}

int
db_player_create(const char *email)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 id;

	db_trans_begin(0);
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		fprintf(stderr, "Player (%s) could not be "
			"created: bad state\n", email);
		return(0);
	}

	stmt = db_stmt("INSERT INTO player "
		"(email,rseed) VALUES (?,?)");
	db_bind_text(stmt, 1, email);
	db_bind_int(stmt, 2, arc4random_uniform(INT32_MAX) + 1);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);
	if (SQLITE_DONE == rc) {
		id = sqlite3_last_insert_rowid(db);
		fprintf(stderr, "Player %" PRId64 " "
			"created: %s\n", id, email);
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
	sqlite3_finalize(stmt);
	return(rc == SQLITE_ROW);
}

int64_t
db_expr_winner(double winval, int64_t round)
{
	mpq_t	 	 mpq;
	double		 v;
	int64_t	 	 id;
	sqlite3_stmt	*stmt;

	assert(winval >= 0.0);
	assert(winval <= 1.0);

	stmt = db_stmt("SELECT player.id,lottery.aggrpayoff FROM "
		"lottery INNER JOIN player ON lottery.playerid=player.id "
		"WHERE lottery.round=? ORDER BY player.rank ASC");
	db_bind_int(stmt, 1, round);
	mpq_init(mpq);
	while (SQLITE_ROW == db_step(stmt, 0)) {
		id = sqlite3_column_int(stmt, 0);
		mpq_summation_str(mpq, sqlite3_column_text(stmt, 1));
		v = mpq_get_d(mpq);
		assert(0.0 == v || isnormal(v));
		/* TODO... */
	}
	sqlite3_finalize(stmt);
	mpq_clear(mpq);
	return(id);
}

int
db_expr_start(int64_t date, int64_t rounds, int64_t minutes, 
	const char *instructions, const char *uri)
{
	sqlite3_stmt	*stmt, *stmt2;
	int64_t		 id;
	size_t		 i;

	db_trans_begin(0);
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		fprintf(stderr, "Experiment could not "
			"be started: bad state\n");
		return(0);
	}

	stmt = db_stmt("UPDATE experiment SET "
		"state=1,start=?,rounds=?,minutes=?,"
		"loginuri=?,instructions=?");
	db_bind_int(stmt, 1, date);
	db_bind_int(stmt, 2, rounds);
	db_bind_int(stmt, 3, minutes);
	db_bind_text(stmt, 4, uri);
	db_bind_text(stmt, 5, instructions);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	db_trans_commit();
	fprintf(stderr, "Experiment started!\n");

	i = 0;
	stmt = db_stmt("SELECT id from player ORDER BY rseed ASC, id ASC");
	stmt2 = db_stmt("UPDATE player SET role=?,rank=? WHERE id=?");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		id = sqlite3_column_int(stmt, 0);
		db_bind_int(stmt2, 1, i % 2);
		db_bind_int(stmt2, 2, i++);
		db_bind_int(stmt2, 3, id);
		db_step(stmt2, 0);
		sqlite3_reset(stmt2);
	}
	sqlite3_finalize(stmt);
	sqlite3_finalize(stmt2);
	return(1);
}

void
db_player_enable(int64_t id)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET enabled=1 WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	fprintf(stderr, "Player %" PRId64 " enabled\n", id);
}

void
db_player_reset_error(void)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET state=0 WHERE state=3");
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	fprintf(stderr, "Updated player error states\n");
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
	db_bind_text(stmt, 2, db_crypt_hash(pass));
	db_bind_int(stmt, 3, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	fprintf(stderr, "Player %" PRId64 " mailed password\n", id);
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
		fprintf(stderr, "Game %" PRId64 " not "
			"deleted: bad game state\n", id);
		return(0);
	}

	stmt = db_stmt("DELETE FROM game WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	db_trans_commit();
	fprintf(stderr, "Game %" PRId64 " deleted\n", id);
	return(1);
}

int
db_player_delete(int64_t id)
{
	sqlite3_stmt	*stmt;

	db_trans_begin(0);
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		fprintf(stderr, "Player %" PRId64 " not "
			"deleted: bad game state\n", id);
		return(0);
	}

	stmt = db_stmt("DELETE FROM player WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	db_trans_commit();
	fprintf(stderr, "Player %" PRId64 " deleted\n", id);
	return(1);
}

void
db_player_disable(int64_t id)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET enabled=0 WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	sqlite3_finalize(stmt);
	fprintf(stderr, "Player %" PRId64 " disabled\n", id);
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
	fprintf(stderr, "SMTP server %s, user %s, "
		"from %s set\n", server, user, from);
}

/*
 * Given the strategy mixes played for a given roundup, compute the
 * per-strategy-pair matrix.
 */
static struct roundup *
db_roundup_round(struct roundup *r)
{
	size_t	 i, j, k;
	mpq_t	 tmp, sum;

	r->avg = kcalloc(r->p1sz * r->p2sz, sizeof(double));
	r->avgp1 = kcalloc(r->p1sz, sizeof(double));
	r->avgp2 = kcalloc(r->p2sz, sizeof(double));

	if (0 == r->roundcount)
		return(r);

	mpq_init(tmp);
	mpq_init(sum);

	for (j = 1, i = 0; i < r->roundcount; i++, j *= 2) {
		mpq_set_ui(tmp, 1, j);
		mpq_canonicalize(tmp);
		mpq_summation(sum, tmp);
	}

	for (i = 0; i < r->p1sz; i++) {
		mpq_div(tmp, r->aggrp1[i], sum);
		r->avgp1[i] = mpq_get_d(tmp);
	}
	for (i = 0; i < r->p2sz; i++)  {
		mpq_div(tmp, r->aggrp2[i], sum);
		r->avgp2[i] = mpq_get_d(tmp);
	}

	for (i = k = 0; i < r->p1sz; i++) 
		for (j = 0; j < r->p2sz; j++, k++)
			r->avg[k] = r->avgp1[i] * r->avgp2[j];

	mpq_clear(tmp);
	mpq_clear(sum);

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
db_player_lottery(int64_t round, 
	int64_t pid, mpq_t cur, mpq_t aggr, size_t count)
{
	sqlite3_stmt	*stmt;
	size_t		 i;
	int		 rc;
	char		*curstr, *aggrstr;
	mpq_t		 prevcur, prevaggr;

	if (round < 0)
		return(0);

	stmt = db_stmt("SELECT aggrpayoff,curpayoff FROM "
		"lottery WHERE playerid=? AND round=?");
	db_bind_int(stmt, 1, pid);
	db_bind_int(stmt, 2, round);
	if (SQLITE_ROW == (rc = db_step(stmt, 0))) {
		db_str2mpq_single(sqlite3_column_text(stmt, 0), aggr);
		db_str2mpq_single(sqlite3_column_text(stmt, 1), cur);
	}
	sqlite3_finalize(stmt);

	if (SQLITE_ROW == rc)
		return(1);
again:
	fprintf(stderr, "Lottery: player %" 
		PRId64 ", round %" PRId64 "\n", pid, round);

	if ( ! db_player_lottery(round - 1, pid, prevcur, prevaggr, count)) 
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

	/* Convert the current and last to strings. */
	gmp_asprintf(&curstr, "%Qd", cur);
	gmp_asprintf(&aggrstr, "%Qd", aggr);

	/* Record both in database. */
	stmt = db_stmt("INSERT INTO lottery (aggrpayoff,"
		"curpayoff,playerid,round) VALUES (?,?,?,?)");
	db_bind_text(stmt, 1, aggrstr);
	db_bind_text(stmt, 2, curstr);
	db_bind_int(stmt, 3, pid);
	db_bind_int(stmt, 4, round);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);

	free(aggrstr);
	free(curstr);
	mpq_clear(prevaggr);

	if (SQLITE_CONSTRAINT == rc) {
		db_trans_rollback();
		fprintf(stderr, "Lottery concurrent: player %" 
			PRId64 ", round %" PRId64 "\n", pid, round);
		mpq_clear(cur);
		mpq_clear(aggr);
		goto again;
	} 

	db_trans_commit();
	fprintf(stderr, "Lottery complete: player %" 
		PRId64 ", round %" PRId64 "\n", pid, round);
	return(1);
}

static void
db_roundup_players(int64_t round, const struct roundup *r, 
	size_t gamesz, const struct game *game)
{
	mpq_t		*qs, *opponent;
	sqlite3_stmt	*stmt, *stmt2;
	size_t		 i, j, sz;
	mpq_t		 tmp, sum, div, mul;
	char		*buf;
	int64_t	 	 playerid;

	assert(round >= 0);

	mpq_init(tmp);
	mpq_init(sum);
	mpq_init(div);
	mpq_init(mul);

	/* We need a buffer for both strategy vectors. */

	sz = r->p1sz > r->p2sz ? r->p1sz : r->p2sz;
	opponent = kcalloc(sz, sizeof(mpq_t));
	for (i = 0; i < sz; i++)
		mpq_init(opponent[i]);

	/* Establish the common divisor for roundup aggregates. */

	for (i = 0; i < r->roundcount; i++) {
		mpq_set_ui(div, i + 1, r->roundcount);
		mpq_canonicalize(div);
		mpq_summation(sum, div);
	}

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
			mpq_set(tmp, r->curp2[j]);
			mpq_mul(mul, div, tmp);
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
		qs = db_str2mpq
			(sqlite3_column_text(stmt, 0), r->p1sz);
		playerid = sqlite3_column_int(stmt, 1);
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
			mpq_set(tmp, r->curp1[j]);
			mpq_mul(mul, div, tmp);
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
		qs = db_str2mpq
			(sqlite3_column_text(stmt, 0), r->p2sz);
		playerid = sqlite3_column_int(stmt, 1);
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
	mpq_clear(div);
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

	if (NULL != p->aggrp1)
		for (i = 0; i < p->p1sz; i++)
			mpq_clear(p->aggrp1[i]);
	if (NULL != p->curp1)
		for (i = 0; i < p->p1sz; i++)
			mpq_clear(p->curp1[i]);
	if (NULL != p->aggrp2)
		for (i = 0; i < p->p2sz; i++)
			mpq_clear(p->aggrp2[i]);
	if (NULL != p->curp2)
		for (i = 0; i < p->p2sz; i++)
			mpq_clear(p->curp2[i]);

	free(p->avg);
	free(p->avgp1);
	free(p->avgp2);
	free(p->aggrp1);
	free(p->aggrp2);
	free(p->curp1);
	free(p->curp2);
	free(p);
}

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

	stmt = db_stmt("SELECT averagesp1,averagesp2,skip,"
		"roundcount,currentsp1,currentsp2 FROM past "
		"WHERE round=? AND gameid=?");

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, r->gameid);

	if (SQLITE_ROW == db_step(stmt, 0)) {
		r->aggrp1 = db_str2mpq
			(sqlite3_column_text
			 (stmt, 0), r->p1sz);
		r->aggrp2 = db_str2mpq
			(sqlite3_column_text
			 (stmt, 1), r->p2sz);
		r->skip = sqlite3_column_int(stmt, 2);
		r->roundcount = sqlite3_column_int(stmt, 3);
		r->curp1 = db_str2mpq
			(sqlite3_column_text
			 (stmt, 4), r->p1sz);
		r->curp2 = db_str2mpq
			(sqlite3_column_text
			 (stmt, 5), r->p2sz);
		sqlite3_finalize(stmt);
		return(db_roundup_round(r));
	}

	sqlite3_finalize(stmt);
	free(r);
	return(NULL);
}

static void
db_roundup_game(const struct game *game, struct period *period, int64_t round, int64_t gamesz)
{
	struct roundup	*r;
	size_t		 i, count;
	sqlite3_stmt	*stmt;
	mpq_t		 tmp, sum;
	mpq_t		*aggrp1, *aggrp2;
	char		*aggrsp1, *aggrsp2, *cursp1, *cursp2;
	int		 rc;

	if (round < 0)
		return;
	db_roundup_game(game, period, round - 1, gamesz);

again:
	period->roundups[round] = db_roundup_get(round, game);
	if (NULL != period->roundups[round])
		return;

	fprintf(stderr, "Roundup: game %" PRId64 
		", round %" PRId64 "\n", game->id, round);

	r = kcalloc(1, sizeof(struct roundup));
	r->round = round;
	r->p1sz = game->p1;
	r->p2sz = game->p2;
	r->gameid = game->id;
	r->aggrp1 = kcalloc(r->p1sz, sizeof(mpq_t));
	r->aggrp2 = kcalloc(r->p2sz, sizeof(mpq_t));
	r->curp1 = kcalloc(r->p1sz, sizeof(mpq_t));
	r->curp2 = kcalloc(r->p2sz, sizeof(mpq_t));
	r->skip = 1;

	for (i = 0; i < r->p1sz; i++) {
		mpq_init(r->aggrp1[i]);
		mpq_init(r->curp1[i]);
	}
	for (i = 0; i < r->p2sz; i++) {
		mpq_init(r->aggrp2[i]);
		mpq_init(r->curp2[i]);
	}

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

	for (count = 0; SQLITE_ROW == db_step(stmt, 0); count++)
		mpq_summation_strvec(r->curp1, 
			sqlite3_column_text(stmt, 0), r->p1sz);

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
	 * If we should NOT skip it, then divide it by half.
	 */
	r->roundcount = 0;
	aggrp1 = aggrp2 = NULL;

	if (round > 0 && NULL != period->roundups[round - 1]) {
		r->roundcount = period->roundups[round - 1]->roundcount;
		aggrp1 = kcalloc(r->p1sz, sizeof(mpq_t));
		aggrp2 = kcalloc(r->p2sz, sizeof(mpq_t));
		for (i = 0; i < r->p1sz; i++) 
			mpq_set(aggrp1[i], period->roundups
				[round - 1]->aggrp1[i]);
		for (i = 0; i < r->p2sz; i++) 
			mpq_set(aggrp2[i], period->roundups
				[round - 1]->aggrp2[i]);
	}

	r->roundcount += r->skip ? 0 : 1;

	for (i = 0; i < r->p1sz; i++) 
		mpq_set(r->aggrp1[i], r->curp1[i]);
	for (i = 0; i < r->p2sz; i++) 
		mpq_set(r->aggrp2[i], r->curp2[i]);

	if (NULL != aggrp1 && 0 == r->skip) {
		assert(NULL != aggrp2);
		mpq_set_ui(tmp, 1, 2);
		mpq_canonicalize(tmp);
		for (i = 0; i < r->p1sz; i++) {
			mpq_set(sum, aggrp1[i]);
			mpq_mul(aggrp1[i], tmp, sum);
		}
		for (i = 0; i < r->p2sz; i++) {
			mpq_set(sum, aggrp2[i]);
			mpq_mul(aggrp2[i], tmp, sum);
		}
	}

	/*
	 * Next, accumulate the last round into our own.
	 * If we're marked to skip it, we still incorporate it into our
	 * own.
	 */
	if (NULL != aggrp1) {
		assert(NULL != aggrp2);
		for (i = 0; i < r->p1sz; i++) {
			mpq_summation(r->aggrp1[i], aggrp1[i]);
			mpq_clear(aggrp1[i]);
		}
		for (i = 0; i < r->p2sz; i++) {
			mpq_summation(r->aggrp2[i], aggrp2[i]);
			mpq_clear(aggrp2[i]);
		}
	}

	aggrsp1 = mpq_mpq2str(r->aggrp1, r->p1sz);
	aggrsp2 = mpq_mpq2str(r->aggrp2, r->p2sz);
	cursp1 = mpq_mpq2str(r->curp1, r->p1sz);
	cursp2 = mpq_mpq2str(r->curp2, r->p2sz);

	stmt = db_stmt("INSERT INTO past (round,averagesp1,"
		"averagesp2,gameid,skip,roundcount,currentsp1,"
		"currentsp2) VALUES (?,?,?,?,?,?,?,?)");

	db_bind_int(stmt, 1, r->round);
	db_bind_text(stmt, 2, aggrsp1);
	db_bind_text(stmt, 3, aggrsp2);
	db_bind_int(stmt, 4, game->id);
	db_bind_int(stmt, 5, r->skip);
	db_bind_int(stmt, 6, r->roundcount);
	db_bind_text(stmt, 7, cursp1);
	db_bind_text(stmt, 8, cursp2);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	sqlite3_finalize(stmt);

	if (SQLITE_CONSTRAINT == rc) { 
		db_trans_rollback();
		fprintf(stderr, "Roundup concurrent: "
			"round %" PRId64 ", game %" 
			PRId64 "\n", round, game->id);
	} else {
		db_roundup_players(round, r, gamesz, game);
		db_trans_commit();
		fprintf(stderr, "Roundup complete: "
			"round %" PRId64 ", game %" 
			PRId64 "\n", round, game->id);
	}

	free(aggrp1);
	free(aggrp2);
	free(aggrsp1);
	free(aggrsp2);
	free(cursp1);
	free(cursp2);
	mpq_clear(sum);
	mpq_clear(tmp);
	db_roundup_free(r);
	goto again;
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

struct interval *
db_interval_get(int64_t round)
{
	struct interval	*intv;
	sqlite3_stmt	*stmt;
	size_t		 i;
	struct game	*game;

	if (round < 0) 
		return(NULL);

	intv = kcalloc(1, sizeof(struct interval));
	intv->periodsz = db_game_count_all();
	intv->periods = kcalloc
		(intv->periodsz, sizeof(struct period));
	
	i = 0;
	stmt = db_stmt("SELECT id from game");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		assert(i < intv->periodsz);
		intv->periods[i].roundupsz = (size_t)round + 1;
		intv->periods[i].roundups = kcalloc
			(intv->periods[i].roundupsz, 
			 sizeof(struct roundup *));
		intv->periods[i++].gameid = 
			sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);

	assert(i == intv->periodsz);
	for (i = 0; i < intv->periodsz; i++) {
		game = db_game_load(intv->periods[i].gameid);
		db_roundup_game(game, &intv->periods[i], round, intv->periodsz);
		db_game_free(game);
	}

	return(intv);
}

struct expr *
db_expr_get(void)
{
	sqlite3_stmt	*stmt;
	struct expr	*expr;
	int		 rc;

	stmt = db_stmt("SELECT start,rounds,minutes,"
		"loginuri,state,instructions FROM experiment");
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);
	if (ESTATE_STARTED != sqlite3_column_int(stmt, 4)) {
		sqlite3_finalize(stmt);
		return(NULL);
	}

	expr = kcalloc(1, sizeof(struct expr));
	expr->start = (time_t)sqlite3_column_int(stmt, 0);
	expr->rounds = sqlite3_column_int(stmt, 1);
	expr->minutes = sqlite3_column_int(stmt, 2);
	expr->loginuri = kstrdup((char *)sqlite3_column_text(stmt, 3));
	expr->instructions = kstrdup((char *)sqlite3_column_text(stmt, 5));
	expr->end = expr->start + (expr->rounds * expr->minutes * 60);
	sqlite3_finalize(stmt);
	return(expr);
}

void
db_expr_free(struct expr *expr)
{
	if (NULL == expr)
		return;

	free(expr->loginuri);
	free(expr->instructions);
	free(expr);
}

void
db_expr_wipe(void)
{
	sqlite3_stmt	*stmt, *stmt2;

	fprintf(stderr, "Database being wiped!\n");
	db_exec("DELETE FROM gameplay");
	db_exec("DELETE FROM sess WHERE playerid IS NOT NULL");
	db_exec("DELETE FROM payoff");
	db_exec("DELETE FROM choice");
	db_exec("DELETE FROM past");
	db_exec("DELETE FROM lottery");
	db_exec("DELETE FROM tickets");
	db_exec("UPDATE player SET state=0,enabled=1");
	db_exec("UPDATE experiment SET state=0,rounds=0,"
		"minutes=0,loginuri=\'\',instructions=\'\'");
	stmt = db_stmt("SELECT id FROM player");
	stmt2 = db_stmt("UPDATE player SET rseed=? WHERE id=?");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		db_bind_int(stmt2, 1, arc4random_uniform(INT32_MAX) + 1);
		db_bind_int(stmt2, 2, sqlite3_column_int(stmt, 0));
		db_step(stmt2, 0);
		sqlite3_reset(stmt2);
	}
	sqlite3_finalize(stmt);
	sqlite3_finalize(stmt2);
}
