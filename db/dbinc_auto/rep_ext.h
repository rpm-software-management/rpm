/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_rep_ext_h_
#define	_rep_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __rep_dbenv_create __P((DB_ENV *));
int __rep_open __P((DB_ENV *));
void __rep_elect_master __P((DB_ENV *, REP *, int *));
int __rep_process_message __P((DB_ENV *, DBT *, DBT *, int *, DB_LSN *));
int __rep_process_txn __P((DB_ENV *, DBT *));
int __rep_tally __P((DB_ENV *, REP *, int, int *, u_int32_t, u_int32_t));
void __rep_cmp_vote __P((DB_ENV *, REP *, int *, DB_LSN *, int, int, int));
int __rep_cmp_vote2 __P((DB_ENV *, REP *, int, u_int32_t));
int __rep_region_init __P((DB_ENV *));
int __rep_region_destroy __P((DB_ENV *));
void __rep_dbenv_refresh __P((DB_ENV *));
int __rep_dbenv_close __P((DB_ENV *));
int __rep_preclose __P((DB_ENV *, int));
int __rep_check_alloc __P((DB_ENV *, TXN_RECS *, int));
int __rep_send_message __P((DB_ENV *, int, u_int32_t, DB_LSN *, const DBT *, u_int32_t));
int __rep_new_master __P((DB_ENV *, REP_CONTROL *, int));
int __rep_is_client __P((DB_ENV *));
int __rep_noarchive __P((DB_ENV *));
void __rep_send_vote __P((DB_ENV *, DB_LSN *, int, int, int, u_int32_t, int, u_int32_t));
void __rep_elect_done __P((DB_ENV *, REP *));
int __rep_grow_sites __P((DB_ENV *dbenv, int nsites));
void __env_rep_enter __P((DB_ENV *));
void __env_rep_exit __P((DB_ENV *));
int __db_rep_enter __P((DB *, int, int));
void __db_rep_exit __P((DB_ENV *));
void __op_rep_enter __P((DB_ENV *));
void __op_rep_exit __P((DB_ENV *));
void __rep_get_gen __P((DB_ENV *, u_int32_t *));
void __rep_print_message __P((DB_ENV *, int, REP_CONTROL *, char *));

#if defined(__cplusplus)
}
#endif
#endif /* !_rep_ext_h_ */
