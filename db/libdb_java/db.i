%{
#include "db_config.h"
#include "db_int.h"
#include "dbinc/txn.h"
%}

#if defined(SWIGJAVA)
%include "db_java.i"
#elif defined(SWIGCSHARP)
%include "db_csharp.i"
#endif

typedef	unsigned char u_int8_t;
typedef	long int32_t;
typedef	long long db_seq_t;
typedef	unsigned long u_int32_t;
typedef	u_int32_t	db_recno_t;	/* Record number type. */
typedef	u_int32_t	db_timeout_t;	/* Type of a timeout. */

typedef	int db_recops;
typedef	int db_lockmode_t;
typedef	int DBTYPE;
typedef	int DB_CACHE_PRIORITY;

/* Fake typedefs for SWIG */
typedef	int db_ret_t;    /* An int that is mapped to a void */
typedef	int int_bool;    /* An int that is mapped to a boolean */

%{
typedef int db_ret_t;
typedef int int_bool;

struct __db_lk_conflicts {
	u_int8_t *lk_conflicts;
	int lk_modes;
};

struct __db_out_stream {
	void *handle;
	int (*callback) __P((void *, const void *));
};

#define Db __db
#define Dbc __dbc
#define Dbt __db_dbt
#define DbEnv __db_env
#define DbLock __db_lock_u
#define DbLogc __db_log_cursor
#define DbLsn __db_lsn
#define DbMpoolFile __db_mpoolfile
#define DbSequence __db_sequence
#define DbTxn __db_txn

/* Suppress a compilation warning for an unused symbol */
void *unused = SWIG_JavaThrowException;
%}

struct Db;		typedef struct Db DB;
struct Dbc;		typedef struct Dbc DBC;
struct Dbt;	typedef struct Dbt DBT;
struct DbEnv;	typedef struct DbEnv DB_ENV;
struct DbLock;	typedef struct DbLock DB_LOCK;
struct DbLogc;	typedef struct DbLogc DB_LOGC;
struct DbLsn;	typedef struct DbLsn DB_LSN;
struct DbMpoolFile;	typedef struct DbMpoolFile DB_MPOOLFILE;
struct DbSequence;		typedef struct Db DB_SEQUENCE;
struct DbTxn;	typedef struct DbTxn DB_TXN;

/* Methods that allocate new objects */
%newobject Db::join(DBC **curslist, u_int32_t flags);
%newobject Db::dup(u_int32_t flags);
%newobject DbEnv::lock_get(u_int32_t locker,
	u_int32_t flags, const DBT *object, db_lockmode_t lock_mode);
%newobject DbEnv::log_cursor(u_int32_t flags);


struct Db
{
%extend {
	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	Db(DB_ENV *dbenv, u_int32_t flags) {
		DB *self;
		errno = db_create(&self, dbenv, flags);
		return (errno == 0) ? self : NULL;
	}
	
	JAVA_EXCEPT(DB_RETOK_STD, DB2JDBENV)
	db_ret_t associate(DB_TXN *txnid, DB *secondary,
	    int (*callback)(DB *, const DBT *, const DBT *, DBT *),
	    u_int32_t flags) {
		return self->associate(self, txnid, secondary, callback, flags);
	}

	/*
	 * Should probably be db_ret_t, but maintaining backwards compatibility
	 * for now.
	 */
	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	int close(u_int32_t flags) {
		errno = self->close(self, flags);
		return errno;
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, DB2JDBENV)
	DBC *cursor(DB_TXN *txnid, u_int32_t flags) {
		DBC *cursorp;
		errno = self->cursor(self, txnid, &cursorp, flags);
		return (errno == 0) ? cursorp : NULL;
	}

	JAVA_EXCEPT(DB_RETOK_DBDEL, DB2JDBENV)
	int del(DB_TXN *txnid, DBT *key, u_int32_t flags) {
		return self->del(self, txnid, key, flags);
	}

	JAVA_EXCEPT_NONE
	void err(int error, const char *message) {
		self->err(self, error, message);
	}

	void errx(const char *message) {
		self->errx(self, message);
	}
	
	int_bool get_transactional() {
		return self->get_transactional(self);
	}

#ifndef SWIGJAVA
	int fd() {
		int ret;
		errno = self->fd(self, &ret);
		return ret;
	}
#endif

	JAVA_EXCEPT(DB_RETOK_DBGET, DB2JDBENV)
	int get(DB_TXN *txnid, DBT *key, DBT *data, u_int32_t flags) {
		return self->get(self, txnid, key, data, flags);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, DB2JDBENV)
	int_bool get_byteswapped() {
		int ret;
		errno = self->get_byteswapped(self, &ret);
		return ret;
	}

	jlong get_cachesize() {
		u_int32_t gbytes, bytes;
		errno = self->get_cachesize(self, &gbytes, &bytes, NULL);
		return (jlong)gbytes * GIGABYTE + bytes;
	}

	u_int32_t get_cachesize_ncache() {
		int ret;
		errno = self->get_cachesize(self, NULL, NULL, &ret);
		return ret;
	}

	const char *get_filename() {
		const char *ret;
		errno = self->get_dbname(self, &ret, NULL);
		return ret;
	}

	const char *get_dbname() {
		const char *ret;
		errno = self->get_dbname(self, NULL, &ret);
		return ret;
	}

	u_int32_t get_encrypt_flags() {
		u_int32_t ret;
		errno = self->get_encrypt_flags(self, &ret);
		return ret;
	}

	/*
	 * This method is implemented in Java to avoid wrapping the object on
	 * every call.
	 */
#ifndef SWIGJAVA
	DB_ENV *get_env() {
		DB_ENV *env;
		errno = self->get_env(self, &env);
		return env;
	}
#endif

	const char *get_errpfx() {
		const char *ret;
		errno = 0;
		self->get_errpfx(self, &ret);
		return ret;
	}

	u_int32_t get_flags() {
		u_int32_t ret;
		errno = self->get_flags(self, &ret);
		return ret;
	}

	int get_lorder() {
		int ret;
		errno = self->get_lorder(self, &ret);
		return ret;
	}
	
	DB_MPOOLFILE *get_mpf() {
		errno = 0;
		return self->mpf;
	}

	u_int32_t get_open_flags() {
		u_int32_t ret;
		errno = self->get_open_flags(self, &ret);
		return ret;
	}

	u_int32_t get_pagesize() {
		u_int32_t ret;
		errno = self->get_pagesize(self, &ret);
		return ret;
	}

	u_int32_t get_bt_minkey() {
		u_int32_t ret;
		errno = self->get_bt_minkey(self, &ret);
		return ret;
	}

	u_int32_t get_h_ffactor() {
		u_int32_t ret;
		errno = self->get_h_ffactor(self, &ret);
		return ret;
	}

	u_int32_t get_h_nelem() {
		u_int32_t ret;
		errno = self->get_h_nelem(self, &ret);
		return ret;
	}

	int get_re_delim() {
		int ret;
		errno = self->get_re_delim(self, &ret);
		return ret;
	}

	u_int32_t get_re_len() {
		u_int32_t ret;
		errno = self->get_re_len(self, &ret);
		return ret;
	}

	int get_re_pad() {
		int ret;
		errno = self->get_re_pad(self, &ret);
		return ret;
	}

	const char *get_re_source() {
		const char *ret;
		errno = self->get_re_source(self, &ret);
		return ret;
	}

	u_int32_t get_q_extentsize() {
		u_int32_t ret;
		errno = self->get_q_extentsize(self, &ret);
		return ret;
	}

	DBTYPE get_type() {
		DBTYPE type;
		errno = self->get_type(self, &type);
		return type;
	}

	DBC *join(DBC **curslist, u_int32_t flags) {
		DBC *dbcp;
		errno = self->join(self, curslist, &dbcp, flags);
		return (errno == 0) ? dbcp : NULL;
	}

	JAVA_EXCEPT(DB_RETOK_STD, DB2JDBENV)
	db_ret_t key_range(DB_TXN *txnid, DBT *key, DB_KEY_RANGE *key_range,
	    u_int32_t flags) {
		return self->key_range(self, txnid, key, key_range, flags);
	}

	db_ret_t open(DB_TXN *txnid, const char *file, const char *database,
	    DBTYPE type, u_int32_t flags, int mode) {
		return self->open(self, txnid, file, database,
		    type, flags, mode);
	}

	JAVA_EXCEPT(DB_RETOK_DBGET, DB2JDBENV)
	int pget(DB_TXN *txnid, DBT *key, DBT *pkey, DBT *data,
	    u_int32_t flags) {
		return self->pget(self, txnid, key, pkey, data, flags);
	}

	JAVA_EXCEPT(DB_RETOK_DBPUT, DB2JDBENV)
	int put(DB_TXN *txnid, DBT *key, DBT *data, u_int32_t flags) {
		return self->put(self, txnid, key, data, flags);
	}

	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t remove(const char *file, const char *database,
	    u_int32_t flags) {
		return self->remove(self, file, database, flags);
	}

	db_ret_t rename(const char *file, const char *database,
	    const char *newname, u_int32_t flags) {
		return self->rename(self, file, database, newname, flags);
	}

	JAVA_EXCEPT(DB_RETOK_STD, DB2JDBENV)
	db_ret_t set_append_recno(
	    int (*db_append_recno_fcn)(DB *, DBT *, db_recno_t)) {
		return self->set_append_recno(self, db_append_recno_fcn);
	}

	db_ret_t set_bt_compare(
	    int (*bt_compare_fcn)(DB *, const DBT *, const DBT *)) {
		return self->set_bt_compare(self, bt_compare_fcn);
	}

	db_ret_t set_bt_maxkey(u_int32_t maxkey) {
		return self->set_bt_maxkey(self, maxkey);
	}

	db_ret_t set_bt_minkey(u_int32_t bt_minkey) {
		return self->set_bt_minkey(self, bt_minkey);
	}

	db_ret_t set_bt_prefix(
	    size_t (*bt_prefix_fcn)(DB *, const DBT *, const DBT *)) {
		return self->set_bt_prefix(self, bt_prefix_fcn);
	}

	db_ret_t set_cachesize(jlong bytes, int ncache) {
		return self->set_cachesize(self,
		    (u_int32_t)(bytes / GIGABYTE),
		    (u_int32_t)(bytes % GIGABYTE), ncache);
	}

	db_ret_t set_dup_compare(
	    int (*dup_compare_fcn)(DB *, const DBT *, const DBT *)) {
		return self->set_dup_compare(self, dup_compare_fcn);
	}

	db_ret_t set_encrypt(const char *passwd, u_int32_t flags) {
		return self->set_encrypt(self, passwd, flags);
	}

	JAVA_EXCEPT_NONE
#ifndef SWIGJAVA
	void set_errcall(void (*db_errcall_fcn)(const DB_ENV *, const char *, const char *)) {
		self->set_errcall(self, db_errcall_fcn);
	}
#endif /* SWIGJAVA */

	void set_errpfx(const char *errpfx) {
		self->set_errpfx(self, errpfx);
	}

	JAVA_EXCEPT(DB_RETOK_STD, DB2JDBENV)
	db_ret_t set_feedback(void (*db_feedback_fcn)(DB *, int, int)) {
		return self->set_feedback(self, db_feedback_fcn);
	}

	db_ret_t set_flags(u_int32_t flags) {
		return self->set_flags(self, flags);
	}

	db_ret_t set_h_ffactor(u_int32_t h_ffactor) {
		return self->set_h_ffactor(self, h_ffactor);
	}

	db_ret_t set_h_hash(
	    u_int32_t (*h_hash_fcn)(DB *, const void *, u_int32_t)) {
		return self->set_h_hash(self, h_hash_fcn);
	}

	db_ret_t set_h_nelem(u_int32_t h_nelem) {
		return self->set_h_nelem(self, h_nelem);
	}

	db_ret_t set_lorder(int lorder) {
		return self->set_lorder(self, lorder);
	}

#ifndef SWIGJAVA
	void set_msgcall(void (*db_msgcall_fcn)(const DB_ENV *, const char *)) {
		self->set_msgcall(self, db_msgcall_fcn);
	}
#endif /* SWIGJAVA */

	db_ret_t set_pagesize(u_int32_t pagesize) {
		return self->set_pagesize(self, pagesize);
	}

#ifndef SWIGJAVA
	db_ret_t set_paniccall(void (* db_panic_fcn)(DB_ENV *, int)) {
		return self->set_paniccall(self,  db_panic_fcn);
	}
#endif /* SWIGJAVA */

	db_ret_t set_re_delim(int re_delim) {
		return self->set_re_delim(self, re_delim);
	}

	db_ret_t set_re_len(u_int32_t re_len) {
		return self->set_re_len(self, re_len);
	}

	db_ret_t set_re_pad(int re_pad) {
		return self->set_re_pad(self, re_pad);
	}

	db_ret_t set_re_source(char *source) {
		return self->set_re_source(self, source);
	}

	db_ret_t set_q_extentsize(u_int32_t extentsize) {
		return self->set_q_extentsize(self, extentsize);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, DB2JDBENV)
	void *stat(DB_TXN *txnid, u_int32_t flags) {
		void *statp;
		errno = self->stat(self, txnid, &statp, flags);
		return (errno == 0) ? statp : NULL;
	}

	JAVA_EXCEPT(DB_RETOK_STD, DB2JDBENV)
	db_ret_t sync(u_int32_t flags) {
		return self->sync(self, flags);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, DB2JDBENV)
	int truncate(DB_TXN *txnid, u_int32_t flags) {
		u_int32_t count;
		errno = self->truncate(self, txnid, &count, flags);
		return count;
	}

	JAVA_EXCEPT(DB_RETOK_STD, DB2JDBENV)
	db_ret_t upgrade(const char *file, u_int32_t flags) {
		return self->upgrade(self, file, flags);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	int_bool verify(const char *file, const char *database,
	    struct __db_out_stream outfile, u_int32_t flags) {
		/*
		 * We can't easily #include "dbinc/db_ext.h" because of name
		 * clashes, so we declare this explicitly.
		 */
		extern int __db_verify_internal __P((DB *, const char *, const
		    char *, void *, int (*)(void *, const void *), u_int32_t));
		errno = __db_verify_internal(self, file, database,
		    outfile.handle, outfile.callback, flags);
		if (errno == DB_VERIFY_BAD) {
			errno = 0;
			return 0;
		} else
			return 1;
	}
}
};


struct Dbc
{
%extend {
	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t close() {
		return self->c_close(self);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, DBC2JDBENV)
	db_recno_t count(u_int32_t flags) {
		db_recno_t count;
		errno = self->c_count(self, &count, flags);
		return count;
	}

	JAVA_EXCEPT(DB_RETOK_DBCDEL, DBC2JDBENV)
	int del(u_int32_t flags) {
		return self->c_del(self, flags);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, DBC2JDBENV)
	DBC *dup(u_int32_t flags) {
		DBC *newcurs;
		errno = self->c_dup(self, &newcurs, flags);
		return (errno == 0) ? newcurs : NULL;
	}

	JAVA_EXCEPT(DB_RETOK_DBCGET, DBC2JDBENV)
	int get(DBT* key, DBT *data, u_int32_t flags) {
		return self->c_get(self, key, data, flags);
	}

	int pget(DBT* key, DBT* pkey, DBT *data, u_int32_t flags) {
		return self->c_pget(self, key, pkey, data, flags);
	}

	JAVA_EXCEPT(DB_RETOK_DBCPUT, DBC2JDBENV)
	int put(DBT* key, DBT *data, u_int32_t flags) {
		return self->c_put(self, key, data, flags);
	}
}
};


struct DbEnv
{
%extend {
	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	DbEnv(u_int32_t flags) {
		DB_ENV *self = NULL;
		errno = db_env_create(&self, flags);
		return (errno == 0) ? self : NULL;
	}

	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t close(u_int32_t flags) {
		return self->close(self, flags);
	}
	
	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t dbremove(DB_TXN *txnid, const char *file, const char *database,
	    u_int32_t flags) {
		return self->dbremove(self, txnid, file, database, flags);
	}

	db_ret_t dbrename(DB_TXN *txnid, const char *file, const char *database,
	    const char *newname, u_int32_t flags) {
		return self->dbrename(self,
		    txnid, file, database, newname, flags);
	}

	JAVA_EXCEPT_NONE
	void err(int error, const char *message) {
		self->err(self, error, message);
	}

	void errx(const char *message) {
		self->errx(self, message);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	const char **get_data_dirs() {
		const char **ret;
		errno = self->get_data_dirs(self, &ret);
		return ret;
	}
	
	u_int32_t get_encrypt_flags() {
		u_int32_t ret;
		errno = self->get_encrypt_flags(self, &ret);
		return ret;
	}

	const char *get_errpfx() {
		const char *ret;
		errno = 0;
		self->get_errpfx(self, &ret);
		return ret;
	}

	u_int32_t get_flags() {
		u_int32_t ret;
		errno = self->get_flags(self, &ret);
		return ret;
	}

	const char *get_home() {
		const char *ret;
		errno = self->get_home(self, &ret);
		return ret;
	}

	u_int32_t get_open_flags() {
		u_int32_t ret;
		errno = self->get_open_flags(self, &ret);
		return ret;
	}

	long get_shm_key() {
		long ret;
		errno = self->get_shm_key(self, &ret);
		return ret;
	}

	u_int32_t get_tas_spins() {
		u_int32_t ret;
		errno = self->get_tas_spins(self, &ret);
		return ret;
	}

	const char *get_tmp_dir() {
		const char *ret;
		errno = self->get_tmp_dir(self, &ret);
		return ret;
	}

	int_bool get_verbose(u_int32_t which) {
		int ret;
		errno = self->get_verbose(self, which, &ret);
		return ret;
	}

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t open(const char *db_home, u_int32_t flags, int mode) {
		return self->open(self, db_home, flags, mode);
	}
	
	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t remove(const char *db_home, u_int32_t flags) {
		return self->remove(self, db_home, flags);
	}
	
	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t set_cachesize(jlong bytes, int ncache) {
		return self->set_cachesize(self,
		    (u_int32_t)(bytes / GIGABYTE),
		    (u_int32_t)(bytes % GIGABYTE), ncache);
	}

	db_ret_t set_data_dir(const char *dir) {
		return self->set_data_dir(self, dir);
	}
	
	db_ret_t set_encrypt(const char *passwd, u_int32_t flags) {
		return self->set_encrypt(self, passwd, flags);
	}

	JAVA_EXCEPT_NONE
	void set_errcall(void (*db_errcall_fcn)(const DB_ENV *, const char *, const char *)) {
		self->set_errcall(self, db_errcall_fcn);
	}

	void set_errpfx(const char *errpfx) {
		self->set_errpfx(self, errpfx);
	}

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t set_flags(u_int32_t flags, int_bool onoff) {
		return self->set_flags(self, flags, onoff);
	}

	db_ret_t set_feedback(void (*env_feedback_fcn)(DB_ENV *, int, int)) {
		return self->set_feedback(self, env_feedback_fcn);
	}

	db_ret_t set_mp_mmapsize(size_t mp_mmapsize) {
		return self->set_mp_mmapsize(self, mp_mmapsize);
	}

	JAVA_EXCEPT_NONE
	void set_msgcall(void (*db_msgcall_fcn)(const DB_ENV *, const char *)) {
		self->set_msgcall(self, db_msgcall_fcn);
	}

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t set_paniccall(void (*db_panic_fcn)(DB_ENV *, int)) {
		return self->set_paniccall(self, db_panic_fcn);
	}

	db_ret_t set_rpc_server(void *client, char *host,
	    long cl_timeout, long sv_timeout, u_int32_t flags) {
		return self->set_rpc_server(self, client, host,
		    cl_timeout, sv_timeout, flags);
	}

	db_ret_t set_shm_key(long shm_key) {
		return self->set_shm_key(self, shm_key);
	}

	db_ret_t set_tas_spins(u_int32_t tas_spins) {
		return self->set_tas_spins(self, tas_spins);
	}

	db_ret_t set_timeout(db_timeout_t timeout, u_int32_t flags) {
		return self->set_timeout(self, timeout, flags);
	}

	db_ret_t set_tmp_dir(const char *dir) {
		return self->set_tmp_dir(self, dir);
	}

	db_ret_t set_tx_max(u_int32_t max) {
		return self->set_tx_max(self, max);
	}

	db_ret_t set_app_dispatch(
	    int (*tx_recover)(DB_ENV *, DBT *, DB_LSN *, db_recops)) {
		return self->set_app_dispatch(self, tx_recover);
	}

	db_ret_t set_tx_timestamp(time_t *timestamp) {
		return self->set_tx_timestamp(self, timestamp);
	}

	db_ret_t set_verbose(u_int32_t which, int_bool onoff) {
		return self->set_verbose(self, which, onoff);
	}

	/* Lock functions */
	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	struct __db_lk_conflicts get_lk_conflicts() {
		struct __db_lk_conflicts ret;
		errno = self->get_lk_conflicts(self,
		    (const u_int8_t **)&ret.lk_conflicts, &ret.lk_modes);
		return ret;
	}

	u_int32_t get_lk_detect() {
		u_int32_t ret;
		errno = self->get_lk_detect(self, &ret);
		return ret;
	}

	u_int32_t get_lk_max_locks() {
		u_int32_t ret;
		errno = self->get_lk_max_locks(self, &ret);
		return ret;
	}

	u_int32_t get_lk_max_lockers() {
		u_int32_t ret;
		errno = self->get_lk_max_lockers(self, &ret);
		return ret;
	}

	u_int32_t get_lk_max_objects() {
		u_int32_t ret;
		errno = self->get_lk_max_objects(self, &ret);
		return ret;
	}

	int lock_detect(u_int32_t flags, u_int32_t atype) {
		int aborted;
		errno = self->lock_detect(self, flags, atype, &aborted);
		return aborted;
	}

	DB_LOCK *lock_get(u_int32_t locker,
	    u_int32_t flags, const DBT *object, db_lockmode_t lock_mode) {
		DB_LOCK *lock = NULL;
		if ((errno = __os_malloc(self, sizeof (DB_LOCK), &lock)) == 0)
			errno = self->lock_get(self, locker, flags, object,
			    lock_mode, lock);
		return lock;
	}

	u_int32_t lock_id() {
		u_int32_t id;
		errno = self->lock_id(self, &id);
		return id;
	}

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t lock_id_free(u_int32_t id) {
		return self->lock_id_free(self, id);
	}

	db_ret_t lock_put(DB_LOCK *lock) {
		return self->lock_put(self, lock);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	DB_LOCK_STAT *lock_stat(u_int32_t flags) {
		DB_LOCK_STAT *statp;
		errno = self->lock_stat(self, &statp, flags);
		return (errno == 0) ? statp : NULL;
	}

#ifndef SWIGJAVA
	/* For Java, this is defined in native code */
	db_ret_t lock_vec(u_int32_t locker, u_int32_t flags, DB_LOCKREQ *list,
	    int offset, int nlist)
	{
		DB_LOCKREQ *elistp;
		return self->lock_vec(self, locker, flags, list + offset,
		    nlist, &elistp);
	}
#endif

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t set_lk_conflicts(struct __db_lk_conflicts conflicts) {
		return self->set_lk_conflicts(self,
		    conflicts.lk_conflicts, conflicts.lk_modes);
	}

	db_ret_t set_lk_detect(u_int32_t detect) {
		return self->set_lk_detect(self, detect);
	}

#ifndef SWIGJAVA
	db_ret_t set_lk_max(u_int32_t lk_max) {
		return self->set_lk_max(self, lk_max);
	}
#endif /* SWIGJAVA */

	db_ret_t set_lk_max_lockers(u_int32_t max) {
		return self->set_lk_max_lockers(self, max);
	}

	db_ret_t set_lk_max_locks(u_int32_t max) {
		return self->set_lk_max_locks(self, max);
	}

	db_ret_t set_lk_max_objects(u_int32_t max) {
		return self->set_lk_max_objects(self, max);
	}

	/* Log functions */
	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	u_int32_t get_lg_bsize() {
		u_int32_t ret;
		errno = self->get_lg_bsize(self, &ret);
		return ret;
	}

	const char *get_lg_dir() {
		const char *ret;
		errno = self->get_lg_dir(self, &ret);
		return ret;
	}

	u_int32_t get_lg_max() {
		u_int32_t ret;
		errno = self->get_lg_max(self, &ret);
		return ret;
	}

	u_int32_t get_lg_regionmax() {
		u_int32_t ret;
		errno = self->get_lg_regionmax(self, &ret);
		return ret;
	}

	char **log_archive(u_int32_t flags) {
		char **list = NULL;
		errno = self->log_archive(self, &list, flags);
		return (errno == 0) ? list : NULL;
	}

	JAVA_EXCEPT_NONE
	static int log_compare(const DB_LSN *lsn0, const DB_LSN *lsn1) {
		return log_compare(lsn0, lsn1);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	DB_LOGC *log_cursor(u_int32_t flags) {
		DB_LOGC *cursor;
		errno = self->log_cursor(self, &cursor, flags);
		return (errno == 0) ? cursor : NULL;
	}

	char *log_file(DB_LSN *lsn) {
		char namebuf[MAXPATHLEN];
		errno = self->log_file(self, lsn, namebuf, sizeof namebuf);
		return (errno == 0) ? strdup(namebuf) : NULL;
	}

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t log_flush(const DB_LSN *lsn) {
		return self->log_flush(self, lsn);
	}

	db_ret_t log_put(DB_LSN *lsn, const DBT *data, u_int32_t flags) {
		return self->log_put(self, lsn, data, flags);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	DB_LOG_STAT *log_stat(u_int32_t flags) {
		DB_LOG_STAT *sp;
		errno = self->log_stat(self, &sp, flags);
		return (errno == 0) ? sp : NULL;
	}

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t set_lg_bsize(u_int32_t lg_bsize) {
		return self->set_lg_bsize(self, lg_bsize);
	}

	db_ret_t set_lg_dir(const char *dir) {
		return self->set_lg_dir(self, dir);
	}

	db_ret_t set_lg_max(u_int32_t lg_max) {
		return self->set_lg_max(self, lg_max);
	}

	db_ret_t set_lg_regionmax(u_int32_t lg_regionmax) {
		return self->set_lg_regionmax(self, lg_regionmax);
	}

	/* Memory pool functions */
	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	jlong get_cachesize() {
		u_int32_t gbytes, bytes;
		errno = self->get_cachesize(self, &gbytes, &bytes, NULL);
		return (jlong)gbytes * GIGABYTE + bytes;
	}

	int get_cachesize_ncache() {
		int ret;
		errno = self->get_cachesize(self, NULL, NULL, &ret);
		return ret;
	}

	size_t get_mp_mmapsize() {
		size_t ret;
		errno = self->get_mp_mmapsize(self, &ret);
		return ret;
	}

	DB_MPOOL_STAT *memp_stat(u_int32_t flags) {
		DB_MPOOL_STAT *mp_stat;
		errno = self->memp_stat(self, &mp_stat, NULL, flags);
		return (errno == 0) ? mp_stat : NULL;
	}

	DB_MPOOL_FSTAT **memp_fstat(u_int32_t flags) {
		DB_MPOOL_FSTAT **mp_fstat;
		errno = self->memp_stat(self, NULL, &mp_fstat, flags);
		return (errno == 0) ? mp_fstat : NULL;
	}
	
	int memp_trickle(int percent) {
		int ret;
		errno = self->memp_trickle(self, percent, &ret);
		return ret;
	}

	/* Transaction functions */
	u_int32_t get_tx_max() {
		u_int32_t ret;
		errno = self->get_tx_max(self, &ret);
		return ret;
	}

	time_t get_tx_timestamp() {
		time_t ret;
		errno = self->get_tx_timestamp(self, &ret);
		return ret;
	}

	db_timeout_t get_timeout(u_int32_t flag) {
		db_timeout_t ret;
		errno = self->get_timeout(self, &ret, flag);
		return ret;
	}

	DB_TXN *txn_begin(DB_TXN *parent, u_int32_t flags) {
		DB_TXN *tid;
		errno = self->txn_begin(self, parent, &tid, flags);
		return (errno == 0) ? tid : NULL;
	}

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t txn_checkpoint(u_int32_t kbyte, u_int32_t min,
	    u_int32_t flags) {
		return self->txn_checkpoint(self, kbyte, min, flags);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	DB_PREPLIST *txn_recover(int count, u_int32_t flags) {
		DB_PREPLIST *preplist;
		long retcount;

		/* Add a NULL element to terminate the array. */
		if ((errno = __os_malloc(self,
		    (count + 1) * sizeof(DB_PREPLIST), &preplist)) != 0)
			return NULL;

		if ((errno = self->txn_recover(self, preplist, count,
		    &retcount, flags)) != 0) {
			__os_free(self, preplist);
			return NULL;
		}
		
		preplist[retcount].txn = NULL;
		return preplist;
	}

	DB_TXN_STAT *txn_stat(u_int32_t flags) {
		DB_TXN_STAT *statp;
		errno = self->txn_stat(self, &statp, flags);
		return (errno == 0) ? statp : NULL;
	}

	/* Replication functions */
	jlong get_rep_limit() {
		u_int32_t gbytes, bytes;
		errno = self->get_rep_limit(self, &gbytes, &bytes);
		return (jlong)gbytes * GIGABYTE + bytes;
	}

	int rep_elect(int nsites, int nvotes, int priority, u_int32_t timeout, u_int32_t flags) {
		int id;
		errno = self->rep_elect(self, nsites, nvotes, priority, timeout, &id, flags);
		return id;
	}

	JAVA_EXCEPT(DB_RETOK_REPPMSG, JDBENV)
	int rep_process_message(DBT *control, DBT *rec, int *envid, DB_LSN *ret_lsn) {
		return self->rep_process_message(self, control, rec, envid, ret_lsn);
	}

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t rep_start(DBT *cdata, u_int32_t flags) {
		return self->rep_start(self, cdata, flags);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, JDBENV)
	DB_REP_STAT *rep_stat(u_int32_t flags) {
		DB_REP_STAT *statp;
		errno = self->rep_stat(self, &statp, flags);
		return (errno == 0) ? statp : NULL;
	}

	JAVA_EXCEPT(DB_RETOK_STD, JDBENV)
	db_ret_t set_rep_limit(jlong bytes) {
		return self->set_rep_limit(self,
		    (u_int32_t)(bytes / GIGABYTE),
		    (u_int32_t)(bytes % GIGABYTE));
	}

	db_ret_t set_rep_transport(int envid,
	    int (*send)(DB_ENV *, const DBT *, const DBT *,
	    const DB_LSN *, int, u_int32_t)) {
		return self->set_rep_transport(self, envid, send);
	}

	/* Convert DB errors to strings */
	JAVA_EXCEPT_NONE
	static const char *strerror(int error) {
		return db_strerror(error);
	}
	
	/* Versioning information */
	static int get_version_major() {
		return DB_VERSION_MAJOR;
	}
	
	static int get_version_minor() {
		return DB_VERSION_MINOR;
	}
	
	static int get_version_patch() {
		return DB_VERSION_PATCH;
	}
	
	static const char *get_version_string() {
		return DB_VERSION_STRING;
	}
}
};


struct DbLock
{
%extend {
	JAVA_EXCEPT_NONE
	~DbLock() {
		__os_free(NULL, self);
	}
}
};


struct DbLogc
{
%extend {
	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t close(u_int32_t flags) {
		return self->close(self, flags);
	}

	JAVA_EXCEPT(DB_RETOK_LGGET, NULL)
	int get(DB_LSN *lsn, DBT *data, u_int32_t flags) {
		return self->get(self, lsn, data, flags);
	}
}
};


#ifndef SWIGJAVA
struct DbLsn
{
%extend {
	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	DbLsn(u_int32_t file, u_int32_t offset) {
		DB_LSN *self = NULL;
		errno = __os_malloc(NULL, sizeof (DB_LSN), &self);
		if (errno == 0) {
			self->file = file;
			self->offset = offset;
		}
		return self;
	}

	JAVA_EXCEPT_NONE
	~DbLsn() {
		__os_free(NULL, self);
	}

	u_int32_t get_file() {
		return self->file;
	}

	u_int32_t get_offset() {
		return self->offset;
	}
}
};
#endif


struct DbMpoolFile
{
%extend {
	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	DB_CACHE_PRIORITY get_priority() {
		DB_CACHE_PRIORITY ret;
		errno = self->get_priority(self, &ret);
		return ret;
	}
	
	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t set_priority(DB_CACHE_PRIORITY priority){
		return self->set_priority(self, priority);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	u_int32_t get_flags() {
		u_int32_t ret;
		errno = self->get_flags(self, &ret);
		return ret;
	}

	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t set_flags(u_int32_t flags, int_bool onoff) {
		return self->set_flags(self, flags, onoff);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	jlong get_maxsize() {
		u_int32_t gbytes, bytes;
		errno = self->get_maxsize(self, &gbytes, &bytes);
		return (jlong)gbytes * GIGABYTE + bytes;
	}
	
	/* New method - no backwards compatibility version */
	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t set_maxsize(jlong bytes) {
		return self->set_maxsize(self,
		    (u_int32_t)(bytes / GIGABYTE),
		    (u_int32_t)(bytes % GIGABYTE));
	}
}
};


struct DbSequence
{
%extend {
	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	DbSequence(DB *db, u_int32_t flags) {
		DB_SEQUENCE *self = NULL;
		errno = db_sequence_create(&self, db, flags);
		return self;
	}

	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t close(u_int32_t flags) {
		return self->close(self, flags);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	db_seq_t get(DB_TXN *txnid, int32_t delta, u_int32_t flags) {
		db_seq_t ret = 0;
		errno = self->get(self, txnid, delta, &ret, flags);
		return ret;
	}

	int32_t get_cachesize() {
		int32_t ret = 0;
		errno = self->get_cachesize(self, &ret);
		return ret;
	}

	DB *get_db() {
		DB *ret = NULL;
		errno = self->get_db(self, &ret);
		return ret;
	}

	u_int32_t get_flags() {
		u_int32_t ret = 0;
		errno = self->get_flags(self, &ret);
		return ret;
	}

	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t get_key(DBT *key) {
		return self->get_key(self, key);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	db_seq_t get_range_min() {
		db_seq_t ret = 0;
		errno = self->get_range(self, &ret, NULL);
		return ret;
	}

	db_seq_t get_range_max() {
		db_seq_t ret = 0;
		errno = self->get_range(self, NULL, &ret);
		return ret;
	}

	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t initial_value(db_seq_t val) {
		return self->initial_value(self, val);
	}

	db_ret_t open(DB_TXN *txnid, DBT *key, u_int32_t flags) {
		return self->open(self, txnid, key, flags);
	}

	db_ret_t remove(DB_TXN *txnid, u_int32_t flags) {
		return self->remove(self, txnid, flags);
	}

	db_ret_t set_cachesize(int32_t size) {
		return self->set_cachesize(self, size);
	}

	db_ret_t set_flags(u_int32_t flags) {
		return self->set_flags(self, flags);
	}

	db_ret_t set_range(db_seq_t min, db_seq_t max) {
		return self->set_range(self, min, max);
	}

	JAVA_EXCEPT_ERRNO(DB_RETOK_STD, NULL)
	DB_SEQUENCE_STAT *stat(u_int32_t flags) {
		DB_SEQUENCE_STAT *ret = NULL;
		errno = self->stat(self, &ret, flags);
		return ret;
	}
}
};


struct DbTxn
{
%extend {
	JAVA_EXCEPT(DB_RETOK_STD, NULL)
	db_ret_t abort() {
		return self->abort(self);
	}
	
	db_ret_t commit(u_int32_t flags) {
		return self->commit(self, flags);
	}

	db_ret_t discard(u_int32_t flags) {
		return self->discard(self, flags);
	}

	JAVA_EXCEPT_NONE
	u_int32_t id() {
		return self->id(self);
	}

	JAVA_EXCEPT(DB_RETOK_STD, TXN2JDBENV)
	db_ret_t prepare(u_int8_t *gid) {
		return self->prepare(self, gid);
	}

	db_ret_t set_timeout(db_timeout_t timeout, u_int32_t flags) {
		return self->set_timeout(self, timeout, flags);
	}
}
};


