#include <sys/param.h>

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <kcgi.h>
#include <gmp.h>
#include <sqlite3.h>

#include "extern.h"

/*
 * The database, its location, and its statement (if any).
 * This is opened on-demand (see db_tryopen()) and closed automatically
 * on system exit (see db_atexit()).
 */
static sqlite3		*db;

static void
db_finalise(sqlite3_stmt *stmt)
{

	sqlite3_finalize(stmt);
}

/*
 * When the system exits (i.e., exit(3) is called), make sure that the
 * database has been properly closed out.
 */
static void
db_atexit(void)
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
	if (-1 == atexit(db_atexit)) {
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

int
db_sess_valid(int64_t id, int64_t cookie)
{
	int	 	 rc;
	sqlite3_stmt	*stmt;

	stmt = db_stmt("SELECT * FROM sess WHERE id=? AND cookie=?");
	db_bind_int(stmt, 1, id);
	db_bind_int(stmt, 2, cookie);
	rc = db_step(stmt, 0);
	db_finalise(stmt);
	return(SQLITE_ROW == rc);
}

struct sess *
db_sess_alloc(void)
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
	fprintf(stderr, "%" PRId64 ": new session cookie %" 
		PRId64 "\n", sess->id, sess->cookie);
	return(sess);
}

void
db_sess_free(struct sess *sess)
{

	free(sess);
}

int
db_admin_valid(const char *email, const char *pass)
{
	sqlite3_stmt	*stmt;
#if 0
	const char	*hash;
#endif

	stmt = db_stmt("SELECT hash FROM admin WHERE email=?");
	db_bind_text(stmt, 1, email);

	if (SQLITE_ROW != db_step(stmt, 0)) {
		fprintf(stderr, "%s: not admin\n", email);
		db_finalise(stmt);
		return(0);
	} 
#if 0
	hash = crypt(pass, (char *)sqlite3_column_text(stmt, 0));
	if (strcmp(hash, (char *)sqlite3_column_text(stmt, 0))) {
		fprintf(stderr, "hash = %s", hash);
		fprintf(stderr, "%s: not password\n", pass);
		db_finalise(stmt);
		return(0);
	}
#endif
	db_finalise(stmt);
	return(1);
}

void
db_game_free(struct game *game)
{

	if (NULL == game)
		return;

	free(game->payoffs);
	free(game->name);
	free(game);
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

	sv = buf = strdup(poffs);
	assert(NULL != buf);
	fprintf(stderr, "parsing: %s\n", sv);

	/* FIXME: integer overflow. */
	rops = calloc(p1 * p2, sizeof(mpq_t));
	fprintf(stderr, "strategies: %zu\n", (size_t)(p1 * p2));
	assert(NULL != rops);

	for (i = 0; i < (size_t)(p1 * p2); i++)
		mpq_init(rops[i]);

	/* Verify that these are real numbers. */
	count = 0;
	while (NULL != (tok = strsep(&buf, " \t\n\r"))) {
		fprintf(stderr, "processing: %s\n", tok);
		if (count > (size_t)(p1 * p2)) {
			fprintf(stderr, "%s: matrix too big", name);
			free(sv);
			free(rops);
			return(NULL);
		} else if (-1 != mpq_set_str(rops[count++], tok, 10))
			continue;
		fprintf(stderr, "%s: bad payoff: %s\n", name, tok);
		free(sv);
		free(rops);
		return(NULL);
	}
	free(sv);

	if (count != (size_t)(p1 * p2)) {
		fprintf(stderr, "%s: matrix size: %zu != %" 
			PRId64 "\n", name, count, p1 * p2);
		free(rops);
		return(NULL);
	}

	game = calloc(1, sizeof(struct game));
	assert(NULL != game);
	game->payoffs = rops;
	game->p1 = p1;
	game->p2 = p2;
	game->name = strdup(name);
	assert(NULL != game->name);

	stmt = db_stmt("INSERT INTO game "
		"(payoffs, p1, p2, name) VALUES(?,?,?,?)");
	db_bind_text(stmt, 1, poffs);
	db_bind_int(stmt, 2, game->p1);
	db_bind_int(stmt, 3, game->p2);
	db_bind_text(stmt, 4, game->name);
	db_step(stmt, 0);
	db_finalise(stmt);
	game->id = sqlite3_last_insert_rowid(db);
	fprintf(stderr, "%" PRId64 ": new game: %s", 
		game->id, game->name);
	return(game);
}
