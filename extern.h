#ifndef EXTERN_H
#define EXTERN_H

__BEGIN_DECLS

int	db_sess_valid(int64_t id, int64_t cookie);
int	db_admin_valid(const char *email, const char *pass);

__END_DECLS

#endif
