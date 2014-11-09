#ifndef EXTERN_H
#define EXTERN_H

enum	estate {
	ESTATE_NEW = 0,
	ESTATE_STARTED = 1,
};

struct	expr {
	time_t		 start;
	int64_t		 days;
	char		*loginuri;
};

struct	sess {
	int64_t	 	 cookie;
	int64_t	 	 id;
};

struct	game {
	mpq_t		*payoffs;
	int64_t		 p1;
	int64_t		 p2;
	char		*name;
	int64_t		 id;
};

enum	pstate {
	PSTATE_NEW = 0,
	PSTATE_MAILED = 1,
	PSTATE_LOGGEDIN = 2
};

struct	player {
	char		*mail;
	enum pstate	 state;
	int		 enabled;
	int64_t		 role;
	int64_t		 id;
};

struct	smtp {
	char		*user;
	char		*pass;
	char		*server;
	char		*from;
};

__BEGIN_DECLS

void		 db_admin_set_pass(const char *pass);
void		 db_admin_set_mail(const char *email);
int		 db_admin_valid(const char *email, const char *pass);
int		 db_admin_valid_email(const char *email);
int		 db_admin_valid_pass(const char *pass);
struct sess	*db_admin_sess_alloc(void);
int		 db_admin_sess_valid(int64_t id, int64_t cookie);

void		 db_close(void);

int		 db_expr_checkstate(enum estate state);
int		 db_expr_start(int64_t date, int64_t days, const char *uri);
void		 db_expr_free(struct expr *expr);
struct expr	*db_expr_get(void);

struct game	*db_game_alloc(const char *payoffs,
			const char *name, 
			int64_t p1, int64_t p2);
size_t		 db_game_count_all(void);
void		 db_game_free(struct game *game);
size_t		 db_game_load_all(void (*fp)(const struct game *, size_t, void *), void *arg);
size_t		 db_game_load_player(int64_t playerid, int64_t round, 
			void (*fp)(const struct game *, size_t, void *), void *arg);
int		 db_game_check_player(int64_t playerid, int64_t round, int64_t gameid);

void		 db_player_set_mailed(int64_t id, const char *pass);
void		 db_player_set_loggedin(int64_t id);
size_t		 db_player_count_all(void);
int		 db_player_create(const char *email);
void		 db_player_enable(int64_t id);
int		 db_player_delete(int64_t id);
void		 db_player_disable(int64_t id);
struct player	*db_player_load(int64_t id);
size_t		 db_player_load_all(void (*fp)(const struct player *, size_t, void *), void *arg);
size_t		 db_player_load_player(int64_t playerid, void (*fp)(const struct player *, size_t, void *), void *arg);
char		*db_player_next_new(int64_t *id, char **pass);
struct sess	*db_player_sess_alloc(int64_t playerid);
int		 db_player_valid(int64_t *id, const char *mail, const char *pass);
int		 db_player_sess_valid(int64_t *playerid, int64_t id, int64_t cookie);
void		 db_player_free(struct player *player);

void		 db_sess_delete(int64_t id);
void		 db_sess_free(struct sess *sess);

struct smtp	*db_smtp_get(void);
void		 db_smtp_free(struct smtp *);
void		 db_smtp_set(const char *user, const char *server, 
			const char *from, const char *pass);

void		 mail_players(const char *uri);

void		 json_puts(struct kreq *r, const char *cp);
void		 json_putdouble(struct kreq *r, const char *key, double val);
void		 json_putint(struct kreq *r, const char *key, int64_t val);
void		 json_putstring(struct kreq *r, const char *key, const char *val);
void		 json_putmpqs(struct kreq *r, const char *key, 
			mpq_t *vals, int64_t p1, int64_t p2);

__END_DECLS

#endif
