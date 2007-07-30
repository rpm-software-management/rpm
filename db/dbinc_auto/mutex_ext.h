/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_mutex_ext_h_
#define	_mutex_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __mutex_alloc __P((DB_ENV *, int, u_int32_t, db_mutex_t *));
int __mutex_alloc_int __P((DB_ENV *, int, int, u_int32_t, db_mutex_t *));
int __mutex_free __P((DB_ENV *, db_mutex_t *));
int __mutex_free_int __P((DB_ENV *, int, db_mutex_t *));
int __mut_failchk __P((DB_ENV *));
int __db_fcntl_mutex_init __P((DB_ENV *, db_mutex_t, u_int32_t));
int __db_fcntl_mutex_lock __P((DB_ENV *, db_mutex_t));
int __db_fcntl_mutex_unlock __P((DB_ENV *, db_mutex_t));
int __db_fcntl_mutex_destroy __P((DB_ENV *, db_mutex_t));
int __mutex_alloc_pp __P((DB_ENV *, u_int32_t, db_mutex_t *));
int __mutex_free_pp __P((DB_ENV *, db_mutex_t));
int __mutex_lock_pp __P((DB_ENV *, db_mutex_t));
int __mutex_unlock_pp __P((DB_ENV *, db_mutex_t));
int __mutex_get_align __P((DB_ENV *, u_int32_t *));
int __mutex_set_align __P((DB_ENV *, u_int32_t));
int __mutex_get_increment __P((DB_ENV *, u_int32_t *));
int __mutex_set_increment __P((DB_ENV *, u_int32_t));
int __mutex_get_max __P((DB_ENV *, u_int32_t *));
int __mutex_set_max __P((DB_ENV *, u_int32_t));
int __mutex_get_tas_spins __P((DB_ENV *, u_int32_t *));
int __mutex_set_tas_spins __P((DB_ENV *, u_int32_t));
int __db_pthread_mutex_init __P((DB_ENV *, db_mutex_t, u_int32_t));
int __db_pthread_mutex_lock __P((DB_ENV *, db_mutex_t));
int __db_pthread_mutex_unlock __P((DB_ENV *, db_mutex_t));
int __db_pthread_mutex_destroy __P((DB_ENV *, db_mutex_t));
int __mutex_open __P((DB_ENV *, int));
int __mutex_env_refresh __P((DB_ENV *));
void __mutex_resource_return __P((DB_ENV *, REGINFO *));
int __mutex_stat_pp __P((DB_ENV *, DB_MUTEX_STAT **, u_int32_t));
int __mutex_stat_print_pp __P((DB_ENV *, u_int32_t));
int __mutex_stat_print __P((DB_ENV *, u_int32_t));
void __mutex_print_debug_single __P((DB_ENV *, const char *, db_mutex_t, u_int32_t));
void __mutex_print_debug_stats __P((DB_ENV *, DB_MSGBUF *, db_mutex_t, u_int32_t));
void __mutex_set_wait_info __P((DB_ENV *, db_mutex_t, u_int32_t *, u_int32_t *));
void __mutex_clear __P((DB_ENV *, db_mutex_t));
int __db_tas_mutex_init __P((DB_ENV *, db_mutex_t, u_int32_t));
int __db_tas_mutex_lock __P((DB_ENV *, db_mutex_t));
int __db_tas_mutex_unlock __P((DB_ENV *, db_mutex_t));
int __db_tas_mutex_destroy __P((DB_ENV *, db_mutex_t));
int __db_win32_mutex_init __P((DB_ENV *, db_mutex_t, u_int32_t));
int __db_win32_mutex_lock __P((DB_ENV *, db_mutex_t));
int __db_win32_mutex_unlock __P((DB_ENV *, db_mutex_t));
int __db_win32_mutex_destroy __P((DB_ENV *, db_mutex_t));

#if defined(__cplusplus)
}
#endif
#endif /* !_mutex_ext_h_ */
