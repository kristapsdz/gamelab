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
	char		*instructions; /* instruction markup */
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

struct	roundup {
	mpq_t		*curp1; /* current avg. row player */
	mpq_t		*curp2; /* current avg. column player */
	mpq_t		*aggrp1; /* aggr. row player */
	mpq_t		*aggrp2; /* aggr. col player */
	double		*avg; /* weighted aggr. avg. matrix */
	double		*avgp1; /* weighted aggr. avg. row player */
	double		*avgp2; /* weighted aggr. avg. column player */
	int		 skip; /* insufficient plays? */
	size_t		 roundcount; /* for denominator */
	size_t		 p1sz; /* row player strategy size */
	size_t		 p2sz; /* column player strategy size */
	int64_t		 gameid; /* game identifier */
	int64_t		 round; /* round identifier */
};

struct	period {
	struct roundup	**roundups;
	size_t		  roundupsz;
	int64_t		  gameid;
};

struct	interval {
	struct period	 *periods;
	size_t		  periodsz;
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
	int64_t		 rseed; /* random seed */
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
typedef void	(*gameroundf)(const struct game *, int64_t, void *);
typedef void	(*playerf)(const struct player *, void *);

char		*db_admin_get_mail(void);
void		 db_admin_set_pass(const char *);
void		 db_admin_set_mail(const char *);
int		 db_admin_valid(const char *, const char *);
int		 db_admin_valid_email(const char *);
int		 db_admin_valid_pass(const char *);
struct sess	*db_admin_sess_alloc(void);
int		 db_admin_sess_valid(int64_t, int64_t);

void		 db_close(void);

int		 db_expr_checkstate(enum estate);
int		 db_expr_start(int64_t, int64_t, int64_t, 
			const char *, const char *);
void		 db_expr_free(struct expr *);
struct expr	*db_expr_get(void);
void		 db_expr_wipe(void);

struct interval	*db_interval_get(int64_t);
void		 db_interval_free(struct interval *);

struct game	*db_game_alloc(const char *,
			const char *, int64_t, int64_t);
int		 db_game_check_player(int64_t, int64_t, int64_t);
size_t		 db_game_count_all(void);
int		 db_game_delete(int64_t);
void		 db_game_free(struct game *);
struct game	*db_game_load(int64_t);
struct game	*db_game_load_array(size_t *);
size_t		 db_game_round_count(int64_t, int64_t, int64_t);
size_t		 db_game_round_count_done(int64_t, int64_t, size_t);
void		 db_game_load_all(gamef fp, void *);
void		 db_game_load_player(int64_t, 
			int64_t, gameroundf, void *);

void		 db_player_reset_error(void);
void		 db_player_set_mailed(int64_t, const char *);
void		 db_player_set_state(int64_t, enum pstate);
size_t		 db_player_count_all(void);
int		 db_player_create(const char *);
void		 db_player_enable(int64_t);
int		 db_player_delete(int64_t);
void		 db_player_disable(int64_t);
struct player	*db_player_load(int64_t);
void		 db_player_load_all(playerf, void *);
char		*db_player_next_new(int64_t *, char **);
struct sess	*db_player_sess_alloc(int64_t);
int		 db_player_valid(int64_t *, 
			const char *, const char *);
int		 db_player_sess_valid(int64_t *, int64_t, int64_t);
void		 db_player_free(struct player *);
int		 db_player_play(int64_t, int64_t, 
			int64_t, mpq_t *, size_t);
int		 db_player_lottery(int64_t, int64_t, mpq_t, mpq_t, size_t);

void		 db_sess_delete(int64_t);
void		 db_sess_free(struct sess *);

struct smtp	*db_smtp_get(void);
void		 db_smtp_free(struct smtp *);
void		 db_smtp_set(const char *, const char *, 
			const char *, const char *);

void		 mail_players(const char *);
void		 mail_test(void);

void		 json_putmpqp(struct kjsonreq *, const char *, mpq_t);
void		 json_putmpqs(struct kjsonreq *, 
			const char *, mpq_t *, int64_t, int64_t);
void		 json_putroundup(struct kjsonreq *, 
			const char *, const struct roundup *);
void		 json_putexpr(struct kjsonreq *, const struct expr *);

int		 mpq_str2mpq(const char *, mpq_t);
int		 mpq_str2mpqu(const char *, mpq_t);
void		 mpq_summation(mpq_t, const mpq_t);
void		 mpq_summation_str(mpq_t, const unsigned char *);
void		 mpq_summation_strvec(mpq_t *, const unsigned char *, size_t);
char		*mpq_mpq2str(mpq_t *, size_t);

__END_DECLS

#endif
