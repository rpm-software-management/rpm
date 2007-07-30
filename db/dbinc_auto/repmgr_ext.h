/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_repmgr_ext_h_
#define	_repmgr_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __repmgr_init_election __P((DB_ENV *, int));
int __repmgr_become_master __P((DB_ENV *));
int __repmgr_start __P((DB_ENV *, int, u_int32_t));
int __repmgr_close __P((DB_ENV *));
int __repmgr_set_ack_policy __P((DB_ENV *, int));
int __repmgr_get_ack_policy __P((DB_ENV *, int *));
int __repmgr_env_create __P((DB_ENV *, DB_REP *));
void __repmgr_env_destroy __P((DB_ENV *, DB_REP *));
int __repmgr_stop_threads __P((DB_ENV *));
int __repmgr_set_local_site __P((DB_ENV *, const char *, u_int, u_int32_t));
int __repmgr_add_remote_site __P((DB_ENV *, const char *, u_int, int *, u_int32_t));
void *__repmgr_msg_thread __P((void *));
int __repmgr_handle_event __P((DB_ENV *, u_int32_t, void *));
void __repmgr_stash_generation __P((DB_ENV *));
int __repmgr_send __P((DB_ENV *, const DBT *, const DBT *, const DB_LSN *, int, u_int32_t));
int __repmgr_send_one __P((DB_ENV *, REPMGR_CONNECTION *, u_int, const DBT *, const DBT *));
int __repmgr_is_permanent __P((DB_ENV *, const DB_LSN *));
int __repmgr_bust_connection __P((DB_ENV *, REPMGR_CONNECTION *, int));
void __repmgr_cleanup_connection __P((DB_ENV *, REPMGR_CONNECTION *));
int __repmgr_find_site __P((DB_ENV *, const char *, u_int));
int __repmgr_pack_netaddr __P((DB_ENV *, const char *, u_int, ADDRINFO *, repmgr_netaddr_t *));
int __repmgr_getaddr __P((DB_ENV *, const char *, u_int, int, ADDRINFO **));
int __repmgr_add_site __P((DB_ENV *, const char *, u_int, REPMGR_SITE **));
int __repmgr_net_create __P((DB_ENV *, DB_REP *));
int __repmgr_listen __P((DB_ENV *));
int __repmgr_net_close __P((DB_ENV *));
void __repmgr_net_destroy __P((DB_ENV *, DB_REP *));
int __repmgr_thread_start __P((DB_ENV *, REPMGR_RUNNABLE *));
int __repmgr_thread_join __P((REPMGR_RUNNABLE *));
int __repmgr_set_nonblocking __P((socket_t));
int __repmgr_wake_waiting_senders __P((DB_ENV *));
int __repmgr_await_ack __P((DB_ENV *, const DB_LSN *));
void __repmgr_compute_wait_deadline __P((DB_ENV*, struct timespec *, db_timeout_t));
int __repmgr_init_sync __P((DB_ENV *, DB_REP *));
int __repmgr_close_sync __P((DB_ENV *));
int __repmgr_net_init __P((DB_ENV *, DB_REP *));
int __repmgr_lock_mutex __P((mgr_mutex_t *));
int __repmgr_unlock_mutex __P((mgr_mutex_t *));
int __repmgr_signal __P((cond_var_t *));
int __repmgr_wake_main_thread __P((DB_ENV*));
int __repmgr_writev __P((socket_t, db_iovec_t *, int, size_t *));
int __repmgr_readv __P((socket_t, db_iovec_t *, int, size_t *));
int __repmgr_select_loop __P((DB_ENV *));
int __repmgr_queue_create __P((DB_ENV *, DB_REP *));
void __repmgr_queue_destroy __P((DB_ENV *));
int __repmgr_queue_get __P((DB_ENV *, REPMGR_MESSAGE **));
int __repmgr_queue_put __P((DB_ENV *, REPMGR_MESSAGE *));
int __repmgr_queue_size __P((DB_ENV *));
void *__repmgr_select_thread __P((void *));
int __repmgr_accept __P((DB_ENV *));
int __repmgr_retry_connections __P((DB_ENV *));
int __repmgr_first_try_connections __P((DB_ENV *));
int __repmgr_connect_site __P((DB_ENV *, u_int eid));
int __repmgr_send_handshake __P((DB_ENV *, REPMGR_CONNECTION *));
int __repmgr_read_from_site __P((DB_ENV *, REPMGR_CONNECTION *));
int __repmgr_write_some __P((DB_ENV *, REPMGR_CONNECTION *));
int __repmgr_stat_pp __P((DB_ENV *, DB_REPMGR_STAT **, u_int32_t));
int __repmgr_stat_print_pp __P((DB_ENV *, u_int32_t));
int __repmgr_site_list __P((DB_ENV *, u_int *, DB_REPMGR_SITE **));
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_close __P((DB_ENV *));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_add_remote_site __P((DB_ENV *, const char *, u_int, int *, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_get_ack_policy __P((DB_ENV *, int *));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_set_ack_policy __P((DB_ENV *, int));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_set_local_site __P((DB_ENV *, const char *, u_int, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_site_list __P((DB_ENV *, u_int *, DB_REPMGR_SITE **));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_start __P((DB_ENV *, int, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_stat_pp __P((DB_ENV *, DB_REPMGR_STAT **, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_stat_print_pp __P((DB_ENV *, u_int32_t));
#endif
#ifndef HAVE_REPLICATION_THREADS
int __repmgr_handle_event __P((DB_ENV *, u_int32_t, void *));
#endif
int __repmgr_schedule_connection_attempt __P((DB_ENV *, u_int, int));
void __repmgr_reset_for_reading __P((REPMGR_CONNECTION *));
int __repmgr_new_connection __P((DB_ENV *, REPMGR_CONNECTION **, socket_t, u_int32_t));
int __repmgr_new_site __P((DB_ENV *, REPMGR_SITE**, const repmgr_netaddr_t *, int));
void __repmgr_cleanup_netaddr __P((DB_ENV *, repmgr_netaddr_t *));
void __repmgr_iovec_init __P((REPMGR_IOVECS *));
void __repmgr_add_buffer __P((REPMGR_IOVECS *, void *, size_t));
void __repmgr_add_dbt __P((REPMGR_IOVECS *, const DBT *));
int __repmgr_update_consumed __P((REPMGR_IOVECS *, size_t));
int __repmgr_prepare_my_addr __P((DB_ENV *, DBT *));
u_int __repmgr_get_nsites __P((DB_REP *));
void __repmgr_thread_failure __P((DB_ENV *, int));
char *__repmgr_format_eid_loc __P((DB_REP *, int, char *));
char *__repmgr_format_site_loc __P((REPMGR_SITE *, char *));
void __repmgr_timespec_diff_now __P((DB_ENV *, db_timespec *, db_timespec *));
int __repmgr_repstart __P((DB_ENV *, u_int32_t));
int __repmgr_wsa_init __P((DB_ENV *));

#if defined(__cplusplus)
}
#endif
#endif /* !_repmgr_ext_h_ */
