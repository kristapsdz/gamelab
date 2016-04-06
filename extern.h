//*	$Id$ */
/*
 * Copyright (c) 2014--2015 Kristaps Dzonsons <kristaps@kcons.eu>
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
#ifndef EXTERN_H
#define EXTERN_H

/*
 * The state of an experiment.
 */
enum	estate {
	ESTATE_NEW = 0, /* experiment being configured */
	ESTATE_STARTED = 1, /* players playing */
	ESTATE_PREWIN = 2, /* experiment over, no lottery tally */
	ESTATE_POSTWIN = 3 /* experiment over, lottery tallied */
};

/*
 * An experiment encompassing more than one games.
 * There is always a table row corresponding to the experiment, though
 * at the outset, all of these fields except "state" (which will be
 * ESTATE_NEW) are undefined or NULL.
 */
struct	expr {
	enum estate	 state; /* state of play */
	time_t		 start; /* game-play begins */
	int64_t		 rounds; /* total experiment rounds */
	int64_t		 roundpid; /* round-watcher daemon (or 0) */
	int64_t		 playermax; /* max simultaneous players */
	int64_t		 prounds; /* per-player rounds */
	time_t		 roundbegan; /* time that round began */
	double		 roundpct; /* percent-based round advance */
	int64_t		 roundmin; /* if percent-based, min minutes */
	int64_t		 minutes; /* minutes per game play */
	double		 conversion; /* points -> currency */
	char		*loginuri; /* player login (email click) */
	char		*instr; /* instruction markup */
	char		*history; /* "fake" JSON history */
	int64_t		 total; /* total winnings (>ESTATE_STARTED) */
	int64_t		 autoadd; /* auto-adding players */
	char		*hitid; /* mechanical turk id (or '') */
	char		*awsaccesskey; /* AWS access key */
	char		*awssecretkey; /* AWS secret key */
	char		*awserror; /* if AWS contact had errors */
	int64_t		 autoaddpreserve; /* keep autoadd on start */
	int64_t		 round; /* round (<0 initial, then >=0) */
	char		*lottery; /* lottery amount (empty is none) */
	int64_t		 questionnaire; /* require questions */
#define	EXPR_NOHISTORY 	 0x01 /* don't show history */
#define	EXPR_NOSHUFFLE	 0x02 /* don't shuffle games/rows */
#define	EXPR_RELATIVE	 0x04 /* show relative rounds */
#define	EXPR_SANDBOX	 0x08 /* Mechanical Turk sandbox */
	int64_t		 flags;
};

/*
 * A user session.
 * The usual web stuff.
 */
struct	sess {
	int64_t	 	 cookie; /* randomly-generated cookie */
	int64_t	 	 id; /* unique identifier */
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
	int64_t		 id; /* unique identifier */
};

/*
 * A roundup consists of the average plays of a game for a particular
 * round.
 * The roundup happens after the round ends: all plays are tallied to
 * create the object.
 */
struct	roundup {
	mpq_t		*curp1; /* current avg. row player */
	mpq_t		*curp2; /* current avg. column player */
	double		*navgp1; /* last round avg. row player */
	double		*navgp2; /* last round avg. colun player */
	double		*navg;  /* last round avg. matrix */
	int		 skip; /* insufficient plays? */
	size_t		 roundcount; /* for denominator */
	size_t		 p1sz; /* row player strategy size */
	size_t		 p2sz; /* column player strategy size */
	int64_t		 plays; /* plays in this round */
	int64_t		 gameid; /* game identifier */
	int64_t		 round; /* round identifier */
};

/*
 * A period consists of all roundup objects from now (at some round > 0)
 * back to the first round (index zero) for a particular game.
 * In essence, this is the entire history of play.
 */
struct	period {
	struct roundup	**roundups; /* roundups per round */
	size_t		  roundupsz; /* number of rounds */
	int64_t		  gameid; /* which game */
};

/*
 * This is the entire play history for all games ("periodsz") over all
 * rounds (roundupsz in the period object).
 */
struct	interval {
	struct period	 *periods; /* per-game roundup history */
	size_t		  periodsz; /* number of games */
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
	char		*hitid; /* mturk id (or empty) */
	char		*assignmentid; /* assignment id (or empty) */
	char		*hash; /* password token */
	int		 mturkdone; /* entire mturk process done */
	enum pstate	 state; /* state of participant */
	int		 enabled; /* whether can login */
	int		 autoadd; /* whether auto-added */
	int64_t		 role; /* row/column player role */
	int64_t		 rseed; /* random seed */
	int64_t		 instr; /* show instructions? */
	int64_t		 finalrank; /* ticket offset in all awards */
	int64_t		 finalscore; /* number of tickets */
	int64_t		 version; /* increment at update */
	int64_t		 joined; /* round when joined experiment */
	int64_t		 answer; /* # answered questions */
	int64_t		 id; /* unique identifier */
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

/*
 * At the end of the game, we create a winning object for each player
 * that records, well, whether they've won or not.
 * If they've won, it keeps track of the random sequence so that it can
 * be reconstructed later.
 * This object is filled in for winners only.
 */
struct	winner {
	int64_t		 rnum;  /* the winning lottery ticket */
	int64_t		 rank; /* which dice-throw this was */
};

#define SHA1_BLOCK_LENGTH               64
#define SHA1_DIGEST_LENGTH              20

/*
 * Used for constructing the HMAC-SHA1 for Mechanical Turk enquiries.
 */
typedef struct {
        u_int32_t       state[5];
        u_int64_t       count;
        unsigned char   buffer[SHA1_BLOCK_LENGTH];
} SHA1_CTX;
  
__BEGIN_DECLS

void 		  SHA1Init(SHA1_CTX *);
void 		  SHA1Transform(u_int32_t[5], 
			const unsigned char[SHA1_BLOCK_LENGTH]);
void 		  SHA1Update(SHA1_CTX *context, 
			const unsigned char *, unsigned int);
void 		  SHA1Final(unsigned char[SHA1_DIGEST_LENGTH], 
			SHA1_CTX *);

void		  hmac_sha1(const unsigned char *, int,
			const unsigned char *, int, unsigned char *);

void		  mturk_bonus(const struct expr *, 
			const struct player *, int64_t);
void		  mturk_create(const char *, const char *, const char *,
			const char *, int64_t, int64_t, int64_t, int, 
			double, const char *, const char *, const char *,
			int64_t, int64_t);

void		  base64file(FILE *, size_t, 
			int (*)(const char *, size_t, void *), 
			void *);
size_t		  base64len(size_t);
size_t		  base64buf(char *, const char *, size_t);

int		  doublefork(struct kreq *);

typedef void	(*gamef)(const struct game *, void *);
typedef void	(*gameroundf)(const struct game *, int64_t, void *);
typedef void	(*winnerf)(const struct player *, const struct winner *, void *);
typedef void	(*playerf)(const struct player *, void *);
typedef void	(*playerscorefp)(const struct player *, double, int64_t, void *);

char		*db_admin_get_mail(void);
int64_t		 db_admin_get_flags(void);
void		 db_admin_set_pass(const char *);
void		 db_admin_set_mail(const char *);
int		 db_admin_valid(const char *, const char *);
int		 db_admin_valid_email(const char *);
int		 db_admin_valid_pass(const char *);
struct sess	*db_admin_sess_alloc(const char *);
int		 db_admin_sess_valid(int64_t, int64_t);

int		 db_backup(const char *);

mpq_t		*db_choices_get(int64_t, int64_t, int64_t, size_t *);

void		 db_close(void);

int		 db_expr_advance(void);
void		 db_expr_advanceend(void);
void		 db_expr_advancenext(void);
int		 db_expr_checkstate(enum estate);
void		 db_expr_finish(struct expr **, size_t);
void		 db_expr_free(struct expr *);
struct expr	*db_expr_get(int);
size_t		 db_expr_lobbysize(void);
void		 db_expr_mturk(const char *, const char *);
size_t		 db_expr_round_count(const struct expr *, 
			int64_t, int64_t);
void		 db_expr_setautoadd(int64_t, int64_t);
void		 db_expr_setinstr(const char *);
void		 db_expr_setmailer(int64_t, int64_t);
int		 db_expr_start(int64_t, int64_t, int64_t, int64_t, 
			int64_t, int64_t, int64_t, const char *, 
			const char *, const char *, const char *, 
			int64_t, double, int64_t,
			const char *, const char *);
void		 db_expr_wipe(void);

struct game	*db_game_alloc(const char *,
			const char *, int64_t, int64_t);
int		 db_game_check_player(int64_t, int64_t, int64_t);
size_t		 db_game_count_all(void);
int		 db_game_delete(int64_t);
void		 db_game_free(struct game *);
struct game	*db_game_load(int64_t);
struct game	*db_game_load_array(size_t *);
size_t		 db_game_round_count_done(int64_t, int64_t, size_t);
void		 db_game_load_all(gamef fp, void *);
void		 db_game_load_player(int64_t, 
			int64_t, gameroundf, void *);

struct interval	*db_interval_get(int64_t);
void		 db_interval_free(struct interval *);

int		 db_payoff_get(int64_t, int64_t, int64_t, mpq_t);

size_t		 db_player_count_all(void);
size_t		 db_player_count_plays(int64_t, int64_t);
int		 db_player_create(const char *, char **, 
			const char *, const char *);
int		 db_player_delete(int64_t);
void		 db_player_disable(int64_t);
void		 db_player_enable(int64_t);
void		 db_player_free(struct player *);
struct player	*db_player_load(int64_t);
void		 db_player_load_all(playerf, void *);
struct player	**db_player_load_bonuses(int64_t **, size_t *);
void		 db_player_load_playing(const struct expr *, playerf, void *);
void		 db_player_load_highest(playerscorefp, void *, size_t);
int		 db_player_lottery(int64_t, int64_t, 
		 	mpq_t, mpq_t, int64_t *, size_t);
int		 db_player_mturkvrfy(const char *, char **, 
			const char *, const char *);
void		 db_player_mturkdone(int64_t);
int		 db_player_join(const struct player *, int64_t);
char		*db_player_next_new(int64_t *, char **);
void		 db_player_questionnaire(int64_t, int64_t);
struct sess	*db_player_sess_alloc(int64_t, const char *);
int		 db_player_sess_valid(int64_t *, int64_t, int64_t);
#if 0
int64_t		 db_player_set_bonus(int64_t);
#endif
int		 db_player_play(const struct player *, int64_t, 
			int64_t, int64_t, mpq_t *, size_t);
void		 db_player_reset_all(void);
void		 db_player_reset_error(void);
void		 db_player_set_answered(int64_t, int64_t);
void		 db_player_set_instr(int64_t, int64_t);
void		 db_player_set_mailed(int64_t, const char *);
void		 db_player_set_state(int64_t, enum pstate);
struct player	*db_player_valid(const char *, const char *);

void		 db_sess_delete(int64_t);
void		 db_sess_free(struct sess *);

struct smtp	*db_smtp_get(void);
void		 db_smtp_free(struct smtp *);
void		 db_smtp_set(const char *, const char *, 
			const char *, const char *);

void		 db_winners_load_all(void *, winnerf);
int		 db_winners(struct expr **, size_t, int64_t, size_t);
struct winner	*db_winners_get(int64_t);
void		 db_winners_free(struct winner *);

void		 mail_players(const char *, const char *);
void		 mail_roundadvance(const char *, int64_t, int64_t);
void		 mail_backup(void);
void		 mail_wipe(int);
void		 mail_test(void);

void		 json_puthistory(struct kjsonreq *, int,
			const struct expr *, struct interval *);
void		 json_putplayer(struct kjsonreq *, const struct player *);
void		 json_putmpqp(struct kjsonreq *, const char *, const mpq_t);
void		 json_putmpq(struct kjsonreq *, mpq_t);
void		 json_putmpqs(struct kjsonreq *, 
			const char *, mpq_t *, int64_t, int64_t);
void		 json_putroundup(struct kjsonreq *, 
			const char *, const struct roundup *, int);
void		 json_putexpr(struct kjsonreq *, const struct expr *, int);

mpq_t		*mpq_str2mpqsinit(const unsigned char *, size_t);
void		 mpq_str2mpqinit(const unsigned char *, mpq_t);
int		 mpq_str2mpq(const char *, mpq_t);
int		 mpq_str2mpqu(const char *, mpq_t);
void		 mpq_summation(mpq_t, const mpq_t);
void		 mpq_summation_str(mpq_t, const unsigned char *);
void		 mpq_summation_strvec(mpq_t *, const unsigned char *, size_t);
char		*mpq_mpq2str(mpq_t *, size_t);

#define		 INFO(_fmt, ...) \
		 dbg_info(__FILE__, __LINE__, _fmt, ##__VA_ARGS__)
#define		 WARNX(_fmt, ...) \
		 dbg_warnx(__FILE__, __LINE__, _fmt, ##__VA_ARGS__)
#define		 WARN(_fmt, ...) \
		 dbg_warn(__FILE__, __LINE__, _fmt, ##__VA_ARGS__)
void		 dbg_info(const char *, size_t, const char *, ...)
			__attribute__((format(printf, 3, 4)));
void		 dbg_warn(const char *, size_t, const char *, ...)
			__attribute__((format(printf, 3, 4)));
void		 dbg_warnx(const char *, size_t, const char *, ...)
			__attribute__((format(printf, 3, 4)));

__END_DECLS

#endif
