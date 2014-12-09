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
	db_finalise(stmt);
	return(count);
}

static char *
db_mpq2str(const mpq_t *v, size_t sz)
{
	char	*p, *tmp;
	size_t	 i, psz;

	for (p = NULL, psz = i = 0; i < sz; i++) {
		gmp_asprintf(&tmp, "%Qd", v[i]);
		psz += strlen(tmp) + 2;
		p = krealloc(p, psz);
		if (i > 0)
			(void)strlcat(p, " ", psz);
		else
			p[0] = '\0';
		(void)strlcat(p, tmp, psz);
		free(tmp);
	}

	return(p);
}

static void
db_str2mpq_add(const unsigned char *v, size_t sz, mpq_t *p)
{
	size_t	 i;
	char	*buf, *tok, *sv;
	int	 rc;
	mpq_t	 tmp, sum;

	mpq_init(tmp);
	mpq_init(sum);

	if (1 == sz) {
		rc = mpq_set_str(tmp, (const char *)v, 10);
		assert(0 == rc);
		mpq_canonicalize(tmp);
		mpq_set(sum, *p);
		mpq_add(*p, sum, tmp);
		mpq_clear(tmp);
		mpq_clear(sum);
		return;
	}

	buf = sv = kstrdup((const char *)v);

	i = 0;
	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		assert(i < sz);
		rc = mpq_set_str(tmp, tok, 10);
		assert(0 == rc);
		mpq_canonicalize(tmp);
		mpq_set(sum, p[i]);
		mpq_add(p[i], sum, tmp);
		i++;
	}

	free(sv);
	mpq_clear(tmp);
	mpq_clear(sum);
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
	db_finalise(stmt);
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
	db_finalise(stmt);
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
		fprintf(stderr, "Player (%s) failed "
			"login: no such player\n", mail);
		db_finalise(stmt);
		return(0);
	} 

	*id = sqlite3_column_int(stmt, 1);

	if ( ! db_crypt_check(sqlite3_column_text(stmt, 0), pass)) {
		fprintf(stderr, "Player %" PRId64 " (%s) failed "
			"login: bad password\n", *id, mail);
		db_finalise(stmt);
		return(0);
	}

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

	stmt = db_stmt("SELECT email,state,id,"
		"enabled,role FROM player WHERE id=?");
	db_bind_int(stmt, 1, id);

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

	db_finalise(stmt);
}

/*
 * The "plays" values should be canonicalised from mpq_t prior to being
 * passed here.
 */
int
db_player_play(int64_t playerid, int64_t round, 
	int64_t gameid, const mpq_t *plays, size_t sz)
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

	buf = db_mpq2str(plays, sz);
	stmt = db_stmt("INSERT INTO choice "
		"(round,playerid,gameid,strats) "
		"VALUES (?,?,?,?)");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, playerid);
	db_bind_int(stmt, 3, gameid);
	db_bind_text(stmt, 4, buf);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	db_finalise(stmt);
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
	db_finalise(stmt);

	stmt = db_stmt("UPDATE gameplay "
		"SET choices=choices + 1 "
		"WHERE round=? AND playerid=?");
	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, playerid);
	db_step(stmt, 0);
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
db_game_load_player(int64_t playerid, 
	int64_t round, gameroundf fp, void *arg)
{
	sqlite3_stmt	*stmt;
	struct game	 game;
	int64_t		 id;
	size_t		 i;

	stmt = db_stmt("SELECT payoffs,p1,p2,name,id FROM game");
	while (SQLITE_ROW == db_step(stmt, 0)) {
		id = sqlite3_column_int(stmt, 4);
		if (db_game_check_player(playerid, round, id))
			continue;
		memset(&game, 0, sizeof(struct game));
		game.p1 = sqlite3_column_int(stmt, 1);
		game.p2 = sqlite3_column_int(stmt, 2);
		game.payoffs = db_str2mpq
			(sqlite3_column_text(stmt, 0), 
			 game.p1 * game.p2 * 2);
		game.name = strdup((char *)sqlite3_column_text(stmt, 3));
		assert(NULL != game.name);
		game.id = sqlite3_column_int(stmt, 4);
		(*fp)(&game, round, arg);
		for (i = 0; i < (size_t)(2 * game.p1 * game.p2); i++)
			mpq_clear(game.payoffs[i]);
		free(game.payoffs);
		free(game.name);
	}

	db_finalise(stmt);
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
	db_finalise(stmt);
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
	db_finalise(stmt);
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

	db_finalise(stmt);
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
		fprintf(stderr, "Game allocated with "
			"too many strategies: %" PRId64 
			", %" PRId64 "\n", p1, p2);
		return(NULL);
	}

	sv = buf = kstrdup(poffs);
	rops = kcalloc(p1 * p2 * 2, sizeof(mpq_t));
	for (i = 0; i < (size_t)(p1 * p2 * 2); i++)
		mpq_init(rops[i]);

	count = 0;
	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		if (count >= (size_t)(p1 * p2 * 2))
			goto err;
		if (-1 == mpq_set_str(rops[count], tok, 10))
			goto err;
		mpq_canonicalize(rops[count++]);
	}
	free(sv);
	sv = NULL;

	if (count != (size_t)(p1 * p2 * 2))
		goto err;

	db_trans_begin();
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		goto err;
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

	fprintf(stderr, "Game %" PRId64 " created\n", game->id);
	return(game);
err:
	free(sv);
	for (i = 0; i < (size_t)(p1 * p2 * 2); i++)
		mpq_clear(rops[i]);
	free(rops);
	return(NULL);
}

int
db_player_create(const char *email, size_t role)
{
	sqlite3_stmt	*stmt;
	int		 rc;
	int64_t		 id;

	db_trans_begin();
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		fprintf(stderr, "Player (%s) could not be "
			"created: bad state\n", email);
		return(0);
	}

	stmt = db_stmt("INSERT INTO player "
		"(email,role) VALUES (?,?)");
	db_bind_text(stmt, 1, email);
	db_bind_int(stmt, 2, role);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	db_finalise(stmt);

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
		fprintf(stderr, "Experiment could not "
			"be started: bad state\n");
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
	fprintf(stderr, "Experiment started!\n");
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
	fprintf(stderr, "Player %" PRId64 " enabled\n", id);
}

void
db_player_reset_error(void)
{
	sqlite3_stmt	*stmt;

	stmt = db_stmt("UPDATE player SET state=0 WHERE state=3");
	db_step(stmt, 0);
	db_finalise(stmt);
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
	db_finalise(stmt);
	fprintf(stderr, "Player %" PRId64 
		" state updated: %d\n", id, state);
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
		fprintf(stderr, "Game %" PRId64 " not "
			"deleted: bad game state\n", id);
		return(0);
	}

	stmt = db_stmt("DELETE FROM game WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	db_finalise(stmt);
	db_trans_commit();
	fprintf(stderr, "Game %" PRId64 " deleted\n", id);
	return(1);
}

int
db_player_delete(int64_t id)
{
	sqlite3_stmt	*stmt;

	db_trans_begin();
	if ( ! db_expr_checkstate(ESTATE_NEW)) {
		db_trans_rollback();
		fprintf(stderr, "Player %" PRId64 " not "
			"deleted: bad game state\n", id);
		return(0);
	}

	stmt = db_stmt("DELETE FROM player WHERE id=?");
	db_bind_int(stmt, 1, id);
	db_step(stmt, 0);
	db_finalise(stmt);
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
	db_finalise(stmt);
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
	fprintf(stderr, "SMTP server %s, user %s, "
		"from %s set\n", server, user, from);
}

static struct roundup *
db_roundup_round(struct roundup *r)
{
	size_t	 i, j, k;
	mpq_t	 tmp, div, sum, row, col;

	fprintf(stderr, "Roundup for "
		"round %" PRId64 "\n", r->round);

	r->avg = kcalloc(r->p1sz * r->p2sz, sizeof(mpq_t));
	for (i = 0; i < r->p1sz * r->p2sz; i++)
		mpq_init(r->avg[i]);

	if (0 == r->roundcount) {
		fprintf(stderr, "Roundup: roundcount "
			"zero for round %" PRId64 "\n", r->round);
		return(r);
	}

	mpq_init(tmp);
	mpq_init(sum);
	mpq_init(div);
	mpq_init(row);
	mpq_init(col);

	for (i = 0; i < r->roundcount; i++) {
		mpq_set_ui(div, i + 1, r->roundcount);
		mpq_canonicalize(div);
		mpq_set(tmp, sum);
		mpq_add(sum, div, tmp);
	}

	for (i = k = 0; i < r->p1sz; i++) {
		mpq_set(tmp, r->avgp1[i]);
		mpq_mul(row, div, tmp);
		for (j = 0; j < r->p2sz; j++, k++) {
			mpq_set(tmp, r->avgp2[j]);
			mpq_mul(col, div, tmp);
			mpq_mul(r->avg[k], col, row);
		}
	}

	mpq_clear(tmp);
	mpq_clear(div);
	mpq_clear(sum);
	mpq_clear(row);
	mpq_clear(col);

	fprintf(stderr, "Roundup: complete for "
		"round %" PRId64 "\n", r->round);
	return(r);
}

mpq_t *
db_player_payoff(int64_t round, int64_t playerid)
{
	sqlite3_stmt	*stmt;
	mpq_t		*qp;
	size_t		 i, count;
	char		*sv;

	qp = NULL;
	stmt = db_stmt("SELECT payoff FROM lottery "
		"WHERE playerid=? AND round=?");
	db_bind_int(stmt, 1, playerid);
	db_bind_int(stmt, 2, round);
	if (SQLITE_ROW == db_step(stmt, 0))
		qp = db_str2mpq(sqlite3_column_text(stmt, 0), 1);
	db_finalise(stmt);
	
	if (round < 0) {
		fprintf(stderr, "Ignoring first-round for "
			"player %" PRId64 " lottery\n", playerid);
		return(NULL);
	}

	if (NULL != qp) {
		fprintf(stderr, "Player %" PRId64 ", round %" 
			PRId64 ", cached lottery\n", playerid, round);
		return(qp);
	}

	fprintf(stderr, "Player %" PRId64 ", round %" 
		PRId64 ", new lottery\n", playerid, round);

	qp = kcalloc(1, sizeof(mpq_t));
	mpq_init(*qp);

	stmt = db_stmt("SELECT payoff FROM payoff "
		"WHERE playerid=? AND round=?");
	db_bind_int(stmt, 1, playerid);
	db_bind_int(stmt, 2, round);
	i = 0;
	while (SQLITE_ROW == db_step(stmt, 0)) {
		db_str2mpq_add(sqlite3_column_text(stmt, 0), 1, qp);
		i++;
	}
	db_finalise(stmt);

	if (i != (count = db_game_count_all())) {
		fprintf(stderr, "Player %" PRId64 ", round %" 
			PRId64 ", not enough plays for lotter "
			"(%zu < %zu)\n", playerid, round, i, count);
		mpq_set_ui(*qp, 0, 1);
		mpq_canonicalize(*qp);
	}

	sv = db_mpq2str(qp, 1);
	assert(NULL != sv);

	stmt = db_stmt("INSERT INTO lottery "
		"(payoff,playerid,round) VALUES (?,?,?)");
	db_bind_text(stmt, 1, sv);
	db_bind_int(stmt, 2, playerid);
	db_bind_int(stmt, 3, round);
	if (SQLITE_CONSTRAINT == db_step(stmt, DB_STEP_CONSTRAINT))
		fprintf(stderr, "Player %" PRId64 ", round %" 
			PRId64 ", snuck in lottery\n", playerid, round);
	db_finalise(stmt);
	free(sv);
	return(qp);
}

static void
db_roundup_players(int64_t round, 
	const struct roundup *r, 
	const struct game *game)
{
	mpq_t		*qs, *opponent;
	sqlite3_stmt	*stmt, *stmt2;
	size_t		 i, j, sz;
	mpq_t		 tmp, sum, div, mul;
	char		*buf;
	int64_t	 	 playerid;

	if (round < 0) {
		fprintf(stderr, "No player roundup: no rounds\n");
		return;
	}

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
		mpq_set(tmp, sum);
		mpq_add(sum, div, tmp);
	}

	stmt = db_stmt("SELECT strats,playerid FROM choice "
		"INNER JOIN player ON player.id=choice.playerid "
		"WHERE round=? AND gameid=? AND player.role=?");
	stmt2 = db_stmt("INSERT INTO payoff (round,playerid,"
		"gameid,payoff) VALUES (?,?,?,?)");

	/* First, handle the row player. */

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, game->id);
	db_bind_int(stmt, 3, 0);

	for (i = 0; i < r->p1sz; i++) {
		mpq_set_ui(opponent[i], 0, 1);
		mpq_canonicalize(opponent[i]);
		for (j = 0; j < r->p2sz; j++) {
			/* Normalise aggregate. */
			mpq_set(tmp, r->avgp2[j]);
			mpq_mul(mul, div, tmp);
			/* Multiply norm by payoff. */
			mpq_mul(sum, mul, 
				game->payoffs[j * 2 + 
				i * (r->p2sz * 2)]);
			mpq_set(tmp, opponent[i]);
			/* Add to current row total. */
			mpq_add(opponent[i], tmp, sum);
			gmp_fprintf(stderr, "Row player opponent[%zu] %zu = %Qd\n", i, j, opponent[i]);
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
		fprintf(stderr, "Player %" PRId64 " rounding up\n", playerid);
		mpq_set_ui(sum, 0, 1);
		mpq_canonicalize(sum);
		for (i = 0; i < r->p1sz; i++) {
			mpq_set(tmp, qs[i]);
			mpq_mul(mul, tmp, opponent[i]);
			mpq_set(tmp, sum);
			mpq_add(sum, tmp, mul);
			gmp_fprintf(stderr, "Player %" PRId64 " aggregate: %Qd\n", playerid, sum);
		}
		gmp_asprintf(&buf, "%Qd", sum);
		sqlite3_reset(stmt2);
		db_bind_int(stmt2, 1, r->round);
		db_bind_int(stmt2, 2, playerid);
		db_bind_int(stmt2, 3, game->id);
		db_bind_text(stmt2, 4, buf);
		if (SQLITE_CONSTRAINT == db_step(stmt2, DB_STEP_CONSTRAINT)) {
			fprintf(stderr, "Player %" PRId64 " already rounded up\n", playerid);
		}
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

	for (i = 0; i < r->p2sz; i++) {
		mpq_set_ui(opponent[i], 0, 1);
		mpq_canonicalize(opponent[i]);
		for (j = 0; j < r->p1sz; j++) {
			/* Normalise aggregate. */
			mpq_set(tmp, r->avgp1[j]);
			mpq_mul(mul, div, tmp);
			/* Multiply norm by payoff. */
			mpq_mul(sum, mul, 
				game->payoffs[(i * 2) + 1 + 
				j * (r->p2sz * 2)]);
			mpq_set(tmp, opponent[i]);
			/* Add to current row total. */
			mpq_add(opponent[i], tmp, sum);
			gmp_fprintf(stderr, "Column player opponent[%zu] %zu = %Qd\n", i, j, opponent[i]);
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
		fprintf(stderr, "Player %" PRId64 " rounding up\n", playerid);
		mpq_set_ui(sum, 0, 1);
		mpq_canonicalize(sum);
		for (i = 0; i < r->p2sz; i++) {
			mpq_set(tmp, qs[i]);
			mpq_mul(mul, tmp, opponent[i]);
			mpq_set(tmp, sum);
			mpq_add(sum, tmp, mul);
			gmp_fprintf(stderr, "Player %" PRId64 " aggregate: %Qd\n", playerid, sum);
		}
		gmp_asprintf(&buf, "%Qd", sum);
		sqlite3_reset(stmt2);
		db_bind_int(stmt2, 1, r->round);
		db_bind_int(stmt2, 2, playerid);
		db_bind_int(stmt2, 3, game->id);
		db_bind_text(stmt2, 4, buf);
		if (SQLITE_CONSTRAINT == db_step(stmt2, DB_STEP_CONSTRAINT))
			fprintf(stderr, "Player %" PRId64 " already rounded up\n", playerid);
		free(buf);
		for (i = 0; i < r->p2sz; i++)
			mpq_clear(qs[i]);
		free(qs);
	}

	db_finalise(stmt);
	db_finalise(stmt2);

	mpq_clear(tmp);
	mpq_clear(sum);
	mpq_clear(div);
	mpq_clear(mul);

	for (i = 0; i < sz; i++)
		mpq_clear(opponent[i]);
	free(opponent);
}

struct roundup *
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
		"roundcount FROM past WHERE round=? AND gameid=?");

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, r->gameid);

	if (SQLITE_ROW == db_step(stmt, 0)) {
		r->avgp1 = db_str2mpq
			(sqlite3_column_text
			 (stmt, 0), r->p1sz);
		r->avgp2 = db_str2mpq
			(sqlite3_column_text
			 (stmt, 1), r->p2sz);
		r->skip = sqlite3_column_int(stmt, 3);
		r->roundcount = sqlite3_column_int(stmt, 3);
		db_finalise(stmt);
		return(db_roundup_round(r));
	}

	db_finalise(stmt);
	return(NULL);
}

static void
db_roundup_game(const struct game *game, void *arg)
{
	int64_t		 round;
	struct roundup	*r;
	size_t		 i, count;
	sqlite3_stmt	*stmt;
	mpq_t		 tmp, sum, swap;
	char		*avgsp1, *avgsp2;
	int		 rc;
	struct roundup	*prev;
	
	if ((round = *(int64_t *)arg) < 0)
		return;

	/*
	 * Try to look up the round in the previously-cached roundup for
	 * the game and round.
	 * If we find it, we're good: just exit.
	 */
	stmt = db_stmt("SELECT * FROM past "
		"WHERE round=? AND gameid=?");

	db_bind_int(stmt, 1, round);
	db_bind_int(stmt, 2, game->id);
	rc = db_step(stmt, 0);
	db_finalise(stmt);

	if (SQLITE_ROW == rc)
		return;

	/* Recursively make sure all priors rounds are accounted. */

	db_roundup(round - 1);

	r = kcalloc(1, sizeof(struct roundup));
	r->round = round;
	r->p1sz = game->p1;
	r->p2sz = game->p2;
	r->gameid = game->id;

	fprintf(stderr, "Roundup: game %" PRId64 
		", round %" PRId64 "\n", game->id, round);

	/* Allocate and zero our internal structures. */

	r->avgp1 = kcalloc(r->p1sz, sizeof(mpq_t));
	r->avgp2 = kcalloc(r->p2sz, sizeof(mpq_t));

	for (i = 0; i < r->p1sz; i++)
		mpq_init(r->avgp1[i]);
	for (i = 0; i < r->p2sz; i++)
		mpq_init(r->avgp2[i]);

	mpq_init(sum);
	mpq_init(swap);
	mpq_init(tmp);

	r->skip = 1;

	/*
	 * Accumulate all the mixtures from all players.
	 * Then average the accumulated mixtures.
	 * If a player has had no plays, then mark that this round
	 * should be skipped in computing the next round: we'll simply
	 * inherit directly from the last round.
	 */
	stmt = db_stmt("SELECT strats from choice "
		"INNER JOIN player ON player.id=choice.playerid "
		"WHERE round=? AND gameid=? AND player.role=?");

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, game->id);
	db_bind_int(stmt, 3, 0);

	for (count = 0; SQLITE_ROW == db_step(stmt, 0); count++)
		db_str2mpq_add(sqlite3_column_text
			(stmt, 0), r->p1sz, r->avgp1);

	if (0 == count) {
		fprintf(stderr, "Roundup round %" PRId64 
			": no plays for row player\n", r->round);
		goto aggregate;
	}

	fprintf(stderr, "Roundup round %" PRId64 
		": %zu plays for column player\n", r->round, count);

	for (i = 0; i < r->p1sz; i++) {
		mpq_set_ui(tmp, count, 1);
		mpq_canonicalize(tmp);
		mpq_set(sum, r->avgp1[i]);
		mpq_div(r->avgp1[i], sum, tmp);
	} 

	sqlite3_reset(stmt);

	db_bind_int(stmt, 1, r->round);
	db_bind_int(stmt, 2, game->id);
	db_bind_int(stmt, 3, 1);
	for (count = 0; SQLITE_ROW == db_step(stmt, 0); count++) 
		db_str2mpq_add(sqlite3_column_text
			(stmt, 0), r->p2sz, r->avgp2);

	if (0 == count) {
		/* Zero out first strategy. */
		mpq_set_ui(tmp, 0, 1);
		mpq_canonicalize(tmp);
		for (i = 0; i < r->p1sz; i++)
			mpq_set(r->avgp1[i], tmp);
		fprintf(stderr, "Roundup round %" PRId64 
			": no plays for column player\n", r->round);
		goto aggregate;
	}

	fprintf(stderr, "Roundup round %" PRId64 
		": %zu plays for column player\n", r->round, count);

	for (i = 0; i < r->p2sz; i++) {
		mpq_set_ui(tmp, count, 1);
		mpq_canonicalize(tmp);
		mpq_set(sum, r->avgp2[i]);
		mpq_div(r->avgp2[i], sum, tmp);
	} 

	r->skip = 0;

aggregate:
	db_finalise(stmt);

	r->roundcount = 0;
	if (NULL != (prev = db_roundup_get(r->round - 1, game))) 
		r->roundcount = prev->roundcount;
	r->roundcount += r->skip ? 0 : 1;

	/*
	 * Ok, now we want to make our adjustments for history.
	 * First, we see if we should NOT skip the last round.
	 * If we should NOT skip it, then divide it by half.
	 */
	if (NULL != prev && 0 == prev->skip) {
		mpq_set_ui(tmp, 1, 2);
		mpq_canonicalize(tmp);
		for (i = 0; i < r->p1sz; i++) {
			mpq_set(sum, prev->avgp1[i]);
			mpq_mul(prev->avgp1[i], tmp, sum);
		}
		for (i = 0; i < r->p2sz; i++) {
			mpq_set(sum, prev->avgp2[i]);
			mpq_mul(prev->avgp2[i], tmp, sum);
		}
	}

	/*
	 * Next, accumulate the last round into our own.
	 * If we're marked to skip it, we still incorporate it into our
	 * own.
	 */
	if (NULL != prev) {
		for (i = 0; i < r->p1sz; i++) {
			mpq_set(sum, r->avgp1[i]);
			mpq_add(r->avgp1[i], 
				prev->avgp1[i], sum);
		}
		for (i = 0; i < r->p2sz; i++) {
			mpq_set(sum, r->avgp2[i]);
			mpq_add(r->avgp2[i], 
				prev->avgp2[i], sum);
		}
	}

	avgsp1 = db_mpq2str(r->avgp1, r->p1sz);
	avgsp2 = db_mpq2str(r->avgp2, r->p2sz);

	fprintf(stderr, "Player 1 sums for round %" 
		PRId64 ": %s\n", r->round, avgsp1);
	fprintf(stderr, "Player 2 sums for round %" 
		PRId64 ": %s\n", r->round, avgsp2);
	fprintf(stderr, "Round-count for round %" 
		PRId64 ": %zu (skip=%d)\n", r->round, 
		r->roundcount, r->skip);

	stmt = db_stmt("INSERT INTO past (round,averagesp1,"
		"averagesp2,gameid,skip,roundcount) "
		"VALUES (?,?,?,?,?,?)");

	db_bind_int(stmt, 1, r->round);
	db_bind_text(stmt, 2, avgsp1);
	db_bind_text(stmt, 3, avgsp2);
	db_bind_int(stmt, 4, game->id);
	db_bind_int(stmt, 5, r->skip);
	db_bind_int(stmt, 6, r->roundcount);
	rc = db_step(stmt, DB_STEP_CONSTRAINT);
	db_finalise(stmt);

	if (SQLITE_CONSTRAINT == rc)
		fprintf(stderr, "Roundup slipped in during build\n");
	else
		db_roundup_players(round, r, game);

	free(avgsp1);
	free(avgsp2);
	mpq_clear(sum);
	mpq_clear(swap);
	mpq_clear(tmp);
	db_roundup_free(prev);
	db_roundup_free(r);
}

void
db_roundup(int64_t round)
{

	db_game_load_all(db_roundup_game, &round);
}

void
db_roundup_free(struct roundup *p)
{
	size_t		i;

	if (NULL == p)
		return;
	
	if (NULL != p->avgp1)
		for (i = 0; i < p->p1sz; i++)
			mpq_clear(p->avgp1[i]);
	if (NULL != p->avgp2)
		for (i = 0; i < p->p2sz; i++)
			mpq_clear(p->avgp2[i]);
	if (NULL != p->avg)
		for (i = 0; i < p->p1sz * p->p2sz; i++)
			mpq_clear(p->avg[i]);

	free(p->avg);
	free(p->avgp1);
	free(p->avgp2);
	free(p);
}

struct expr *
db_expr_get(void)
{
	sqlite3_stmt	*stmt;
	struct expr	*expr;
	int		 rc;

	stmt = db_stmt("SELECT start,rounds,"
		"minutes,loginuri,state FROM experiment");
	rc = db_step(stmt, 0);
	assert(SQLITE_ROW == rc);
	if (ESTATE_STARTED != sqlite3_column_int(stmt, 4)) {
		db_finalise(stmt);
		return(NULL);
	}

	expr = kcalloc(1, sizeof(struct expr));
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

void
db_expr_wipe(void)
{

	fprintf(stderr, "Database being wiped!\n");
	db_exec("DELETE FROM gameplay");
	db_exec("DELETE FROM sess WHERE playerid IS NOT NULL");
	db_exec("DELETE FROM payoff");
	db_exec("DELETE FROM choice");
	db_exec("DELETE FROM past");
	db_exec("DELETE FROM tickets");
	db_exec("UPDATE player SET state=0,enabled=1");
	db_exec("UPDATE experiment SET state=0,rounds=0,minutes=0,loginuri=\'\'");
}
