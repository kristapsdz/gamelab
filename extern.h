#ifndef EXTERN_H
#define EXTERN_H

/*
 * The state of an experiment.
 * These can be either ESTATE_NEW, where the configuration is being laid
 * out, or ESTATE_STARTED, where the configuration has been committed.
 * At ESTATE_STARTED, players are initially mailed regarding
 * participation.
 */
enum	estate {
	ESTATE_NEW = 0,
	ESTATE_STARTED = 1,
};

/*
 * An experiment encompassing more than one games.
 * There is always a table row corresponding to the experiment, though
 * at the outside, all of these fields except "state" (which will be
 * ESTATE_NEW) are undefined or NULL.
 */
struct	expr {
	time_t		 start; /* game-play begins */
	time_t		 end; /* game-play ends (computed) */
	int64_t		 rounds; /* number of game plays */
	int64_t		 minutes; /* minutes per game play */
	char		*loginuri; /* player login (email click) */
};

/*
 * A user session.
 * The usual web stuff.
 */
struct	sess {
	int64_t	 	 cookie;
	int64_t	 	 id;
};

/*
 * There are more than one two-player games per experiment.
 * Each game is simply a matrix with payoffs (linearly laid out from
 * top-left to bottom-right, row-player first) and a name.
 */
struct	game {
	mpq_t		*payoffs; /* top-left to bottom-right */
	int64_t		 p1; /* rows */
	int64_t		 p2; /* columns */
	char		*name; /* name (never shown to players) */
	int64_t		 id;
};

/*
 * The state of a user's e-mail and password.
 */
enum	pstate {
	PSTATE_NEW = 0, /* new user, no pass */
	PSTATE_MAILED = 1, /* mailed password */
	PSTATE_LOGGEDIN = 2, /* logged in w/password */
	PSTATE_ERROR = 3 /* error in mailing */
};

/*
 * A participant in the game.
 * When the game switches from ESTATE_NEW to ESTATE_STARTED, players are
 * e-mailed their passwords.
 * However this can happen again (with the passwords being reset) if
 * initial mailing fails (state is PSTATE_ERROR).
 */
struct	player {
	char		*mail; /* e-mail address */
	enum pstate	 state; /* state of participant */
	int		 enabled; /* whether can login */
	int64_t		 role; /* row/column player role */
	int64_t		 id;
};

/*
 * SMTP configuration.
 * (This must be an SSL/TLS-enabled server.)
 * This is required to send out any e-mail information.
 */
struct	smtp {
	char		*user; /* server username */
	char		*pass; /* server password */
	char		*server; /* smtp://server:587 */
	char		*from; /* "from" address on mails */
};

__BEGIN_DECLS

typedef void	(*gamef)(const struct game *, void *);
typedef void	(*playerf)(const struct player *, void *);

char		*db_admin_get_mail(void);
void		 db_admin_set_pass(const char *pass);
void		 db_admin_set_mail(const char *email);
int		 db_admin_valid(const char *email, const char *pass);
int		 db_admin_valid_email(const char *email);
int		 db_admin_valid_pass(const char *pass);
struct sess	*db_admin_sess_alloc(void);
int		 db_admin_sess_valid(int64_t id, int64_t cookie);

void		 db_close(void);

int		 db_expr_checkstate(enum estate state);
int		 db_expr_start(int64_t date, int64_t rounds, 
			int64_t minutes, const char *uri);
void		 db_expr_free(struct expr *expr);
struct expr	*db_expr_get(void);

struct game	*db_game_alloc(const char *payoffs,
			const char *name, 
			int64_t p1, int64_t p2);
int		 db_game_check_player(int64_t playerid, 
			int64_t round, int64_t gameid);
size_t		 db_game_count_all(void);
int		 db_game_delete(int64_t id);
void		 db_game_free(struct game *game);
struct game	*db_game_load(int64_t gameid);
void		 db_game_load_all(gamef fp, void *arg);
void		 db_game_load_player(int64_t playerid, 
			int64_t round, gamef f, void *arg);

void		 db_player_reset_error(void);
void		 db_player_set_mailed(int64_t id, const char *pass);
void		 db_player_set_state(int64_t id, enum pstate state);
size_t		 db_player_count_all(void);
int		 db_player_create(const char *email);
void		 db_player_enable(int64_t id);
int		 db_player_delete(int64_t id);
void		 db_player_disable(int64_t id);
struct player	*db_player_load(int64_t id);
void		 db_player_load_all(playerf fp, void *arg);
char		*db_player_next_new(int64_t *id, char **pass);
struct sess	*db_player_sess_alloc(int64_t playerid);
int		 db_player_valid(int64_t *id, 
			const char *mail, const char *pass);
int		 db_player_sess_valid(int64_t *playerid, 
			int64_t id, int64_t cookie);
void		 db_player_free(struct player *player);
int		 db_player_play(int64_t playerid, int64_t round, 
			int64_t gameid, const char *const *plays, 
			size_t sz, size_t choice, double rr);

void		 db_sess_delete(int64_t id);
void		 db_sess_free(struct sess *sess);

struct smtp	*db_smtp_get(void);
void		 db_smtp_free(struct smtp *);
void		 db_smtp_set(const char *user, const char *server, 
			const char *from, const char *pass);

void		 mail_players(const char *uri);
void		 mail_test(void);

void		 json_putmpqs(struct kjsonreq *r, 
			mpq_t *vals, int64_t p1, int64_t p2);
void		 json_putexpr(struct kjsonreq *r, 
			const struct expr *expr);

__END_DECLS

#endif
