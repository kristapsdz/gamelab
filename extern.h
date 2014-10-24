#ifndef EXTERN_H
#define EXTERN_H

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

__BEGIN_DECLS

int		 db_admin_valid(const char *email, const char *pass);

struct game	*db_game_alloc(const char *payoffs,
			const char *name, 
			int64_t p1, int64_t p2);
void		 db_game_free(struct game *game);

struct sess	*db_sess_alloc(void);
int		 db_sess_valid(int64_t id, int64_t cookie);
void		 db_sess_free(struct sess *sess);

__END_DECLS

#endif
