/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_xa_ext_h_
#define	_xa_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __db_xa_open __P((char *, int, long));
int __db_xa_close __P((char *, int, long));
int __db_xa_start __P((XID *, int, long));
int __db_xa_end __P((XID *, int, long));
int __db_xa_prepare __P((XID *, int, long));
int __db_xa_commit __P((XID *, int, long));
int __db_xa_recover __P((XID *, long, int, long));
int __db_xa_rollback __P((XID *, int, long));
int __db_xa_forget __P((XID *, int, long));
int __db_xa_complete __P((int *, int *, int, long));
int __db_xa_create __P((DB *));
int __db_rmid_to_env __P((int rmid, DB_ENV **envp));
int __db_xid_to_txn __P((DB_ENV *, XID *, size_t *));
int __db_map_rmid __P((int, DB_ENV *));
int __db_unmap_rmid __P((int));
int __db_map_xid __P((DB_ENV *, XID *, size_t));
void __db_unmap_xid __P((DB_ENV *, XID *, size_t));
int __db_get_xa_txn __P((DB_ENV *env, DB_TXN **txnp));
int __db_get_current_thread __P((void **));

#if defined(__cplusplus)
}
#endif
#endif /* !_xa_ext_h_ */
