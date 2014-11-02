#ifndef EXTERN_H
#define EXTERN_H

enum	estate {
	ESTATE_NEW = 0,
	ESTATE_STARTED = 1,
};

struct	expr {
	enum estate	 state;
	char		*name;
	time_t		 start;
	time_t		 end;
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
	PSTATE_MAILED = 1
};

struct	player {
	char		*mail;
	enum pstate	 state;
	int64_t		 id;
};

__BEGIN_DECLS

void		 db_admin_set_pass(const char *pass);
void		 db_admin_set_mail(const char *email);
int		 db_admin_valid(const char *email, const char *pass);
int		 db_admin_valid_email(const char *email);
int		 db_admin_valid_pass(const char *pass);
struct sess	*db_admin_sess_alloc(void);
int		 db_admin_sess_valid(int64_t id, int64_t cookie);

int		 db_expr_checkstate(enum estate state);
void		 db_expr_start(int64_t date, int64_t days);

struct game	*db_game_alloc(const char *payoffs,
			const char *name, 
			int64_t p1, int64_t p2);
size_t		 db_game_count_all(void);
void		 db_game_free(struct game *game);
size_t		 db_game_load_all(void (*fp)(const struct game *, size_t, void *), void *arg);

size_t		 db_player_count_all(void);
void		 db_player_create(const char *email);
size_t		 db_player_load_all(void (*fp)(const struct player *, size_t, void *), void *arg);

void		 db_sess_delete(int64_t id);
void		 db_sess_free(struct sess *sess);

__END_DECLS

#endif
