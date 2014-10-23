#include <sys/param.h>

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <kcgi.h>
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

	snprintf(dbpath, sizeof(dbpath), "%s/lab.db",
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
	} else if (SQLITE_DONE == rc)
		return(stmt);

	fprintf(stderr, "sqlite3_stmt: %s\n", sqlite3_errmsg(db));
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
	exit(EXIT_FAILURE);
}

static void
db_bind_int(sqlite3_stmt *stmt, size_t pos, int64_t val)
{

	assert(pos > 0);
	if (SQLITE_OK == sqlite3_bind_int64(stmt, pos, val))
		return;
	fprintf(stderr, "sqlite3_bind_int64: %s\n", sqlite3_errmsg(db));
	exit(EXIT_FAILURE);
}

int
db_sess_valid(int64_t id, int64_t cookie)
{
	int	 	 rc;
	sqlite3_stmt	*stmt;

	stmt = db_stmt("SELECT 1 FROM sess WHERE id=? AND cookie=?");
	db_bind_int(stmt, 1, id);
	db_bind_int(stmt, 2, cookie);
	rc = db_step(stmt, 0);
	db_finalise(stmt);
	return(SQLITE_ROW == rc);
}

int
db_admin_valid(const char *email, const char *pass)
{
	sqlite3_stmt	*stmt;
	const char	*hash;

	stmt = db_stmt("SELECT hash FROM admin WHERE email=?");
	db_bind_text(stmt, 1, email);

	if (SQLITE_ROW != db_step(stmt, 0)) {
		fprintf(stderr, "%s: not admin\n", email);
		db_finalise(stmt);
		return(0);
	} 

	hash = crypt(pass, (char *)sqlite3_column_text(stmt, 0));
	if (strcmp(hash, (char *)sqlite3_column_text(stmt, 0))) {
		fprintf(stderr, "%s: not password\n", pass);
		db_finalise(stmt);
		return(0);
	}

	db_finalise(stmt);
	return(1);
}
