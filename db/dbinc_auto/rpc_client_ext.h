/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_rpc_client_ext_h_
#define	_rpc_client_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __dbcl_env_set_rpc_server __P((DB_ENV *, void *, const char *, long, long, u_int32_t));
int __dbcl_env_close_wrap __P((DB_ENV *, u_int32_t));
int __dbcl_env_open_wrap __P((DB_ENV *, const char *, u_int32_t, int));
int __dbcl_db_open_wrap __P((DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int));
int __dbcl_refresh __P((DB_ENV *));
int __dbcl_retcopy __P((DB_ENV *, DBT *, void *, u_int32_t, void **, u_int32_t *));
void __dbcl_txn_end __P((DB_TXN *));
void __dbcl_txn_setup __P((DB_ENV *, DB_TXN *, DB_TXN *, u_int32_t));
void __dbcl_c_refresh __P((DBC *));
int __dbcl_c_setup __P((u_int, DB *, DBC **));
int __dbcl_dbclose_common __P((DB *));
int __dbcl_dbenv_illegal __P((DB_ENV *));
int __dbcl_env_create __P((DB_ENV *, long));
int __dbcl_env_cdsgroup_begin __P((DB_ENV *, DB_TXN **));
int __dbcl_env_close __P((DB_ENV *, u_int32_t));
int __dbcl_env_dbremove __P((DB_ENV *, DB_TXN *, const char *, const char *, u_int32_t));
int __dbcl_env_dbrename __P((DB_ENV *, DB_TXN *, const char *, const char *, const char *, u_int32_t));
int __dbcl_env_get_cachesize __P((DB_ENV *, u_int32_t *, u_int32_t *, int *));
int __dbcl_env_get_encrypt_flags __P((DB_ENV *, u_int32_t *));
int __dbcl_env_get_flags __P((DB_ENV *, u_int32_t *));
int __dbcl_env_get_home __P((DB_ENV *, const char * *));
int __dbcl_env_get_open_flags __P((DB_ENV *, u_int32_t *));
int __dbcl_env_open __P((DB_ENV *, const char *, u_int32_t, int));
int __dbcl_env_remove __P((DB_ENV *, const char *, u_int32_t));
int __dbcl_env_set_cachesize __P((DB_ENV *, u_int32_t, u_int32_t, int));
int __dbcl_env_set_encrypt __P((DB_ENV *, const char *, u_int32_t));
int __dbcl_env_set_flags __P((DB_ENV *, u_int32_t, int));
int __dbcl_env_txn_begin __P((DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t));
int __dbcl_env_txn_recover __P((DB_ENV *, DB_PREPLIST *, long, long *, u_int32_t));
int __dbcl_db_create __P((DB *, DB_ENV *, u_int32_t));
int __dbcl_db_associate __P((DB *, DB_TXN *, DB *, int (*)(DB *, const DBT *, const DBT *, DBT *), u_int32_t));
int __dbcl_db_close __P((DB *, u_int32_t));
int __dbcl_db_cursor __P((DB *, DB_TXN *, DBC **, u_int32_t));
int __dbcl_db_del __P((DB *, DB_TXN *, DBT *, u_int32_t));
int __dbcl_db_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
int __dbcl_db_get_bt_minkey __P((DB *, u_int32_t *));
int __dbcl_db_get_dbname __P((DB *, const char * *, const char * *));
int __dbcl_db_get_encrypt_flags __P((DB *, u_int32_t *));
int __dbcl_db_get_flags __P((DB *, u_int32_t *));
int __dbcl_db_get_h_ffactor __P((DB *, u_int32_t *));
int __dbcl_db_get_h_nelem __P((DB *, u_int32_t *));
int __dbcl_db_get_lorder __P((DB *, int *));
int __dbcl_db_get_open_flags __P((DB *, u_int32_t *));
int __dbcl_db_get_pagesize __P((DB *, u_int32_t *));
int __dbcl_db_get_priority __P((DB *, DB_CACHE_PRIORITY *));
int __dbcl_db_get_q_extentsize __P((DB *, u_int32_t *));
int __dbcl_db_get_re_delim __P((DB *, int *));
int __dbcl_db_get_re_len __P((DB *, u_int32_t *));
int __dbcl_db_get_re_pad __P((DB *, int *));
int __dbcl_db_join __P((DB *, DBC **, DBC **, u_int32_t));
int __dbcl_db_key_range __P((DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t));
int __dbcl_db_open __P((DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int));
int __dbcl_db_pget __P((DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t));
int __dbcl_db_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
int __dbcl_db_remove __P((DB *, const char *, const char *, u_int32_t));
int __dbcl_db_rename __P((DB *, const char *, const char *, const char *, u_int32_t));
int __dbcl_db_set_bt_minkey __P((DB *, u_int32_t));
int __dbcl_db_set_encrypt __P((DB *, const char *, u_int32_t));
int __dbcl_db_set_flags __P((DB *, u_int32_t));
int __dbcl_db_set_h_ffactor __P((DB *, u_int32_t));
int __dbcl_db_set_h_nelem __P((DB *, u_int32_t));
int __dbcl_db_set_lorder __P((DB *, int));
int __dbcl_db_set_pagesize __P((DB *, u_int32_t));
int __dbcl_db_set_priority __P((DB *, DB_CACHE_PRIORITY));
int __dbcl_db_set_q_extentsize __P((DB *, u_int32_t));
int __dbcl_db_set_re_delim __P((DB *, int));
int __dbcl_db_set_re_len __P((DB *, u_int32_t));
int __dbcl_db_set_re_pad __P((DB *, int));
int __dbcl_db_stat __P((DB *, DB_TXN *, void *, u_int32_t));
int __dbcl_db_sync __P((DB *, u_int32_t));
int __dbcl_db_truncate __P((DB *, DB_TXN *, u_int32_t  *, u_int32_t));
int __dbcl_dbc_close __P((DBC *));
int __dbcl_dbc_count __P((DBC *, db_recno_t *, u_int32_t));
int __dbcl_dbc_del __P((DBC *, u_int32_t));
int __dbcl_dbc_dup __P((DBC *, DBC **, u_int32_t));
int __dbcl_dbc_get __P((DBC *, DBT *, DBT *, u_int32_t));
int __dbcl_dbc_get_priority __P((DBC *, DB_CACHE_PRIORITY *));
int __dbcl_dbc_pget __P((DBC *, DBT *, DBT *, DBT *, u_int32_t));
int __dbcl_dbc_put __P((DBC *, DBT *, DBT *, u_int32_t));
int __dbcl_dbc_set_priority __P((DBC *, DB_CACHE_PRIORITY));
int __dbcl_txn_abort __P((DB_TXN *));
int __dbcl_txn_commit __P((DB_TXN *, u_int32_t));
int __dbcl_txn_discard __P((DB_TXN *, u_int32_t));
int __dbcl_txn_prepare __P((DB_TXN *, u_int8_t *));
void __dbcl_dbp_init __P((DB *));
void __dbcl_dbc_init __P((DBC *));
void __dbcl_dbenv_init __P((DB_ENV *));
void __dbcl_txn_init __P((DB_TXN *));
int __dbcl_env_create_ret __P((DB_ENV *, long, __env_create_reply *));
int __dbcl_env_open_ret __P((DB_ENV *, const char *, u_int32_t, int, __env_open_reply *));
int __dbcl_env_remove_ret __P((DB_ENV *, const char *, u_int32_t, __env_remove_reply *));
int __dbcl_txn_abort_ret __P((DB_TXN *, __txn_abort_reply *));
int __dbcl_env_txn_begin_ret __P((DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t, __env_txn_begin_reply *));
int __dbcl_env_cdsgroup_begin_ret __P((DB_ENV *, DB_TXN **, __env_cdsgroup_begin_reply *));
int __dbcl_txn_commit_ret __P((DB_TXN *, u_int32_t, __txn_commit_reply *));
int __dbcl_txn_discard_ret __P((DB_TXN *, u_int32_t, __txn_discard_reply *));
int __dbcl_env_txn_recover_ret __P((DB_ENV *, DB_PREPLIST *, long, long *, u_int32_t, __env_txn_recover_reply *));
int __dbcl_db_close_ret __P((DB *, u_int32_t, __db_close_reply *));
int __dbcl_db_create_ret __P((DB *, DB_ENV *, u_int32_t, __db_create_reply *));
int __dbcl_db_get_ret __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t, __db_get_reply *));
int __dbcl_db_key_range_ret __P((DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t, __db_key_range_reply *));
int __dbcl_db_open_ret __P((DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int, __db_open_reply *));
int __dbcl_db_pget_ret __P((DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t, __db_pget_reply *));
int __dbcl_db_put_ret __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t, __db_put_reply *));
int __dbcl_db_remove_ret __P((DB *, const char *, const char *, u_int32_t, __db_remove_reply *));
int __dbcl_db_rename_ret __P((DB *, const char *, const char *, const char *, u_int32_t, __db_rename_reply *));
int __dbcl_db_stat_ret __P((DB *, DB_TXN *, void *, u_int32_t, __db_stat_reply *));
int __dbcl_db_truncate_ret __P((DB *, DB_TXN *, u_int32_t  *, u_int32_t, __db_truncate_reply *));
int __dbcl_db_cursor_ret __P((DB *, DB_TXN *, DBC **, u_int32_t, __db_cursor_reply *));
int __dbcl_db_join_ret __P((DB *, DBC **, DBC **, u_int32_t, __db_join_reply *));
int __dbcl_dbc_close_ret __P((DBC *, __dbc_close_reply *));
int __dbcl_dbc_count_ret __P((DBC *, db_recno_t *, u_int32_t, __dbc_count_reply *));
int __dbcl_dbc_dup_ret __P((DBC *, DBC **, u_int32_t, __dbc_dup_reply *));
int __dbcl_dbc_get_ret __P((DBC *, DBT *, DBT *, u_int32_t, __dbc_get_reply *));
int __dbcl_dbc_pget_ret __P((DBC *, DBT *, DBT *, DBT *, u_int32_t, __dbc_pget_reply *));
int __dbcl_dbc_put_ret __P((DBC *, DBT *, DBT *, u_int32_t, __dbc_put_reply *));

#if defined(__cplusplus)
}
#endif
#endif /* !_rpc_client_ext_h_ */
