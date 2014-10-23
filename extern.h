#ifndef EXTERN_H
#define EXTERN_H

struct	sess {
	int64_t	 cookie;
	int64_t	 id;
};

__BEGIN_DECLS

int		 db_admin_valid(const char *email, const char *pass);

struct sess	*db_sess_alloc(void);
int		 db_sess_valid(int64_t id, int64_t cookie);
void		 db_sess_free(struct sess *sess);

__END_DECLS

#endif
