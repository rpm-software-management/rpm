%module db_java

%include "various.i"
%include "typemaps.i"

%include "java_util.i"
%include "java_except.i"
%include "java_typemaps.i"
%include "java_stat.i"
%include "java_callbacks.i"

/*
 * No finalize methods in general - most classes have "destructor" methods
 * that applications must call explicitly.
 */
%typemap(javafinalize) SWIGTYPE ""

/*
 * These are the exceptions - when there is no "close" method, we need to free
 * the native part at finalization time.  These are exactly the cases where C
 * applications manage the memory for the handles.
 */
%typemap(javafinalize) DbLsn, DbLock %{
  protected void finalize() {
    try {
      delete();
    } catch(Exception e) {
      System.err.println("Exception during finalization: " + e);
      e.printStackTrace(System.err);
    }
  }
%}

// Destructors
%rename(open0) open;
%rename(close0) close;
%rename(remove0) remove;
%rename(rename0) rename;
%rename(verify0) verify;
%rename(abort0) abort;
%rename(commit0) commit;
%rename(discard0) discard;

// Special case methods
%rename(set_tx_timestamp0) set_tx_timestamp;
%rename(setFeedbackHandler) set_feedback;
%rename(setErrorHandler) set_errcall;
%rename(setPanicHandler) set_paniccall;
%rename(get) pget;

// Extra code in the Java classes
%typemap(javacode) DbEnv %{
	// Internally, the JNI layer creates a global reference to each DbEnv,
	// which can potentially be different to this.  We keep a copy here so
	// we can clean up after destructors.
	private Object dbenv_ref;
	private DbAppDispatch app_dispatch_handler;
	private DbEnvFeedbackHandler env_feedback_handler;
	private DbErrorHandler error_handler;
	private DbPanicHandler panic_handler;
	private DbRepTransport rep_transport_handler;
	private String errpfx;

	public static class RepProcessMessage {
		public int envid;
	}

	// Called by the public DbEnv constructor and for private environments
	// by the Db constructor.
	void initialize() {
		dbenv_ref = db_java.initDbEnvRef0(this, this);
		// Start with System.err as the default error stream.
		set_error_stream(System.err);
	}

	void cleanup() {
		swigCPtr = 0;
		db_java.deleteRef0(dbenv_ref);
		dbenv_ref = null;
	}

	public synchronized void close(int flags) throws DbException {
		try {
			close0(flags);
		} finally {
			cleanup();
		}
	}

	private final int handle_app_dispatch(Dbt dbt, DbLsn lsn, int recops) {
		return app_dispatch_handler.appDispatch(this, dbt, lsn, recops);
	}

	private final void handle_env_feedback(int opcode, int percent) {
		env_feedback_handler.feedback(this, opcode, percent);
	}

	private final void handle_error(String msg) {
		error_handler.error(this.errpfx, msg);
	}

	private final void handle_panic(DbException e) {
		panic_handler.panic(this, e);
	}

	private final int handle_rep_transport(Dbt control, Dbt rec,
	    DbLsn lsn, int flags, int envid)
	    throws DbException {
		return rep_transport_handler.send(this, control, rec, lsn,
		    flags, envid);
	}
	
	public void lock_vec(/*u_int32_t*/ int locker, int flags,
	    DbLockRequest[] list, int offset, int count) throws DbException {
		db_javaJNI.DbEnv_lock_vec(swigCPtr, locker, flags, list,
		    offset, count);
	}

	public void open(String db_home, int flags, int mode)
	    throws DbException, java.io.FileNotFoundException {
		/* Java is always threaded */
		flags |= Db.DB_THREAD;
		open0(db_home, flags, mode);
	}

	public synchronized void remove(String db_home, int flags)
	    throws DbException, java.io.FileNotFoundException {
		try {
			remove0(db_home, flags);
		} finally {
			cleanup();
		}
	}

	/**
	 * @deprecated Replaced in Berkeley DB 4.2 by {@link #set_cachesize(long,int)}
	 */
	public void set_cachesize(int gbytes, int bytes, int ncache)
	    throws DbException {
		set_cachesize((long)gbytes * Db.GIGABYTE + bytes, ncache);
	}

	public String get_errpfx() {
		return this.errpfx;
	}

	/**
	 * @deprecated Replaced in Berkeley DB 4.2 by {@link #setErrorHandler(DbErrorHandler)}
	 */
	public void set_errcall(DbErrcall db_errcall_fcn) throws DbException {
		final DbErrcall ferrcall = db_errcall_fcn;
		try {
			setErrorHandler(new DbErrorHandler() {
				public void error(String prefix, String buffer) {
					ferrcall.errcall(prefix, buffer);
				}
			});
		}
		catch (DbException dbe) {
			// setErrorHandler throws an exception,
			// but set_error_stream does not.
			// If it does happen, report it.
			System.err.println("Exception during DbEnv.setErrorHandler: " + dbe);
			dbe.printStackTrace(System.err);
		}
	}

	public void set_error_stream(java.io.OutputStream stream) {
		final java.io.PrintWriter pw = new java.io.PrintWriter(stream);
		try {
			setErrorHandler(new DbErrorHandler() {
				public void error(String prefix, String buf) {
					if (prefix != null)
						pw.print(prefix + ": ");
					pw.println(buf);
					pw.flush();
				}
			});
		}
		catch (DbException dbe) {
			// setErrorHandler throws an exception,
			// but set_error_stream does not.
			// If it does happen, report it.
			System.err.println("Exception during DbEnv.setErrorHandler: " + dbe);
			dbe.printStackTrace(System.err);
		}
	}

	public void set_errpfx(String errpfx) {
		this.errpfx = errpfx;
	}

	/**
	 * @deprecated Replaced in Berkeley DB 4.2 by {@link #setFeedbackHandler(DbEnvFeedbackHandler)}
	 */
	public void set_feedback(DbEnvFeedback feedback) throws DbException {
		final DbEnvFeedback ffeedback = feedback;
		setFeedbackHandler(new DbEnvFeedbackHandler() {
			public void feedback(DbEnv env, int opcode, int percent) {
				ffeedback.feedback(env, opcode, percent);
			}
		});
	}

	/**
	 * @deprecated Replaced in Berkeley DB 4.2 by {@link #set_rep_limit(long)}
	 */
	public void set_rep_limit(int gbytes, int bytes)
	    throws DbException {
		set_rep_limit((long)gbytes * Db.GIGABYTE + bytes);
	}
	
	public void set_tx_timestamp(java.util.Date timestamp) {
		set_tx_timestamp0(timestamp.getTime()/1000);
	}
%}

%typemap(javacode) Db %{
	/* package */ static final int GIGABYTE = 1 << 30;
	// Internally, the JNI layer creates a global reference to each Db,
	// which can potentially be different to this.  We keep a copy here so
	// we can clean up after destructors.
	private Object db_ref;
	private DbEnv dbenv;
	private boolean private_dbenv;
	private DbAppendRecno append_recno_handler;
	private DbBtreeCompare bt_compare_handler;
	private DbBtreePrefix bt_prefix_handler;
	private DbDupCompare dup_compare_handler;
	private DbFeedbackHandler db_feedback_handler;
	private DbHash h_hash_handler;
	private DbSecondaryKeyCreate seckey_create_handler;

	// Called by the Db constructor
	private void initialize(DbEnv dbenv) {
		if (dbenv == null) {
			private_dbenv = true;
			dbenv = db_java.getDbEnv0(this);
			dbenv.initialize();
		}
		this.dbenv = dbenv;
		db_ref = db_java.initDbRef0(this, this);
	}

	private void cleanup() {
		swigCPtr = 0;
		db_java.deleteRef0(db_ref);
		db_ref = null;
		if (private_dbenv)
			dbenv.cleanup();
		dbenv = null;
	}

	public synchronized void close(int flags) throws DbException {
		try {
			close0(flags);
		} finally {
			cleanup();
		}
	}
	
	public DbEnv get_env() throws DbException {
		return dbenv;
	}

	private final void handle_append_recno(Dbt data, int recno)
	    throws DbException {
		append_recno_handler.dbAppendRecno(this, data, recno);
	}

	private final int handle_bt_compare(Dbt dbt1, Dbt dbt2) {
		return bt_compare_handler.compare(this, dbt1, dbt2);
	}

	private final int handle_bt_prefix(Dbt dbt1, Dbt dbt2) {
		return bt_prefix_handler.prefix(this, dbt1, dbt2);
	}

	private final void handle_db_feedback(int opcode, int percent) {
		db_feedback_handler.feedback(this, opcode, percent);
	}

	private final int handle_dup_compare(Dbt dbt1, Dbt dbt2) {
		return dup_compare_handler.compareDuplicates(this, dbt1, dbt2);
	}

	private final int handle_h_hash(byte[] data, int len) {
		return h_hash_handler.hash(this, data, len);
	}

	private final int handle_seckey_create(Dbt key, Dbt data, Dbt result)
	    throws DbException {
		return seckey_create_handler.secondaryKeyCreate(
		    this, key, data, result);
	}

	/**
	 * Determine if a database was configured to store data.
	 * The only algorithm currently available is AES.
	 *
	 * @see #set_encrypt
	 * @return true if the database contents are encrypted.
	 */
	public boolean isEncrypted() {
		return (get_encrypt_flags() != 0);
	}

	public void open(DbTxn txnid, String file, String database,
	    int type, int flags, int mode)
	    throws DbException, java.io.FileNotFoundException,
	      DbDeadlockException, DbLockNotGrantedException {
		/* Java is always threaded */
		flags |= Db.DB_THREAD;
		open0(txnid, file, database, type, flags, mode);
	}

	/**
	 * @deprecated Replaced in Berkeley DB 4.2 by {@link #get(DbTxn,Dbt,Dbt,Dbt,int)}
	 */
	public int pget(DbTxn txnid, Dbt key, Dbt pkey, Dbt data, int flags)
	    throws DbException {
		return get(txnid, key, pkey, data, flags);
	}

	public synchronized void remove(String file, String database, int flags)
	    throws DbException, java.io.FileNotFoundException {
		try {
			remove0(file, database, flags);
		} finally {
			cleanup();
		}
	}

	public synchronized void rename(String file, String database,
	    String newname, int flags)
	    throws DbException, java.io.FileNotFoundException {
		try {
			rename0(file, database, newname, flags);
		} finally {
			cleanup();
		}
	}

	public synchronized void verify(String file, String database,
	    java.io.OutputStream outfile, int flags)
	    throws DbException, java.io.FileNotFoundException {
		try {
			verify0(file, database, outfile, flags);
		} finally {
			cleanup();
		}
	}

	/**
	 * @deprecated Replaced in Berkeley DB 4.2 by {@link #set_cachesize(long,int)}
	 */
	public void set_cachesize(int gbytes, int bytes, int ncache)
	    throws DbException {
		set_cachesize((long)gbytes * Db.GIGABYTE + bytes, ncache);
	}

	/**
	 * @deprecated Replaced in Berkeley DB 4.2 by {@link #setErrorHandler(DbErrorHandler)}
	 */
	public void set_errcall(DbErrcall db_errcall_fcn) {
		final DbErrcall ferrcall = db_errcall_fcn;
		try {
			dbenv.setErrorHandler(new DbErrorHandler() {
				public void error(String prefix, String str) {
					ferrcall.errcall(prefix, str);
				}
			});
		}
		catch (DbException dbe) {
			// setErrorHandler throws an exception,
			// but set_errcall does not.
			// If it does happen, report it.
			System.err.println("Exception during DbEnv.setErrorHandler: " + dbe);
			dbe.printStackTrace(System.err);
		}

	}

	public void setErrorHandler(DbErrorHandler db_errcall_fcn) {
		dbenv.setErrorHandler(db_errcall_fcn);
	}

	public String get_errpfx() {
		return dbenv.get_errpfx();
	}

	public void set_errpfx(String errpfx) {
		dbenv.set_errpfx(errpfx);
	}

	public void set_error_stream(java.io.OutputStream stream) {
		dbenv.set_error_stream(stream);
	}

	/**
	 * @deprecated Replaced in Berkeley DB 4.2 by {@link #setFeedbackHandler(DbFeedbackHandler)}
	 */
	public void set_feedback(DbFeedback feedback) throws DbException {
		final DbFeedback ffeedback = feedback;
		setFeedbackHandler(new DbFeedbackHandler() {
			public void feedback(Db db, int opcode, int percent) {
				ffeedback.feedback(db, opcode, percent);
			}
		});
	}

	public void setPanicHandler(DbPanicHandler db_panic_fcn) throws DbException {
		dbenv.setPanicHandler(db_panic_fcn);
	}

	// Don't remove these - special comments used by s_java to add constants
	// BEGIN-JAVA-SPECIAL-CONSTANTS
	// END-JAVA-SPECIAL-CONSTANTS

	static {
	// BEGIN-JAVA-CONSTANT-INITIALIZATION
	// END-JAVA-CONSTANT-INITIALIZATION
	}
%}

%typemap(javacode) Dbc %{
	public synchronized void close() throws DbException {
		try {
			close0();
		} finally {
			swigCPtr = 0;
		}
	}

	/**
	 * @deprecated Replaced in Berkeley DB 4.2 by {@link #get(Dbt,Dbt,Dbt,int)}
	 */
	public int pget(Dbt key, Dbt pkey, Dbt data, int flags)
	    throws DbException {
		return get(key, pkey, data, flags);
	}
%}

%typemap(javacode) DbLogc %{
	public synchronized void close(int flags) throws DbException {
		try {
			close0(flags);
		} finally {
			swigCPtr = 0;
		}
	}
%}

%typemap(javacode) DbTxn %{
	public void abort() throws DbException {
		try {
			abort0();
		} finally {
			swigCPtr = 0;
		}
	}

	public void commit(int flags) throws DbException {
		try {
			commit0(flags);
		} finally {
			swigCPtr = 0;
		}
	}

	public void discard(int flags) throws DbException {
		try {
			discard0(flags);
		} finally {
			swigCPtr = 0;
		}
	}

	// We override Object.equals because it is possible for
	// the Java API to create multiple DbTxns that reference
	// the same underlying object.  This can happen for example
	// during DbEnv.txn_recover().
	//
	public boolean equals(Object obj)
	{
		if (this == obj)
			return true;

		if (obj != null && (obj instanceof DbTxn)) {
			DbTxn that = (DbTxn)obj;
			return (this.swigCPtr == that.swigCPtr);
		}
		return false;
	}

	// We must override Object.hashCode whenever we override
	// Object.equals() to enforce the maxim that equal objects
	// have the same hashcode.
	//
	public int hashCode()
	{
		return ((int)swigCPtr ^ (int)(swigCPtr >> 32));
	}
%}

%native(initDbEnvRef0) jobject initDbEnvRef0(DB_ENV *self, void *handle);
%native(initDbRef0) jobject initDbRef0(DB *self, void *handle);
%native(deleteRef0) void deleteRef0(jobject ref);
%native(getDbEnv0) DB_ENV *getDbEnv0(DB *self);

%{
JNIEXPORT jobject JNICALL Java_com_sleepycat_db_db_1javaJNI_initDbEnvRef0(
    JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg2) {
	DB_ENV *self = *(DB_ENV **)&jarg1;
	COMPQUIET(jcls, NULL);

	DB_ENV_INTERNAL(self) = (void *)(*jenv)->NewGlobalRef(jenv, jarg2);
	self->set_errpfx(self, (const char*)self);
	return (jobject)DB_ENV_INTERNAL(self);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_db_1javaJNI_initDbRef0(
    JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg2) {
	DB *self = *(DB **)&jarg1;
	COMPQUIET(jcls, NULL);

	DB_INTERNAL(self) = (void *)(*jenv)->NewGlobalRef(jenv, jarg2);
	return (jobject)DB_INTERNAL(self);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_db_1javaJNI_deleteRef0(
    JNIEnv *jenv, jclass jcls, jobject jref) {
	COMPQUIET(jcls, NULL);

	if (jref != NULL)
		(*jenv)->DeleteGlobalRef(jenv, jref);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_db_1javaJNI_getDbRef0(
    JNIEnv *jenv, jclass jcls, jlong jarg1) {
	DB *self = *(DB **)&jarg1;
	COMPQUIET(jcls, NULL);
	COMPQUIET(jenv, NULL);

	return (jobject)DB_INTERNAL(self);
}

JNIEXPORT jlong JNICALL Java_com_sleepycat_db_db_1javaJNI_getDbEnv0(
    JNIEnv *jenv, jclass jcls, jlong jarg1) {
	DB *self = *(DB **)&jarg1;
	jlong env_cptr;

	COMPQUIET(jenv, NULL);
	COMPQUIET(jcls, NULL);

	*(DB_ENV **)&env_cptr = self->dbenv;
	return env_cptr;
}

JNIEXPORT jboolean JNICALL
Java_com_sleepycat_db_DbUtil_is_1big_1endian(JNIEnv *jenv, jclass clazz)
{
	COMPQUIET(jenv, NULL);
	COMPQUIET(clazz, NULL);

	return (__db_isbigendian() ? JNI_TRUE : JNI_FALSE);
}
%}
