%pragma(java) jniclasscode=%{
	static {
		// An alternate library name can be specified via a property.
		String libname;

		if ((libname = System.getProperty("sleepycat.db.libfile"))
		    != null)
			System.load(libname);
		else if ((libname = System.getProperty("sleepycat.db.libname"))
		    != null)
			System.loadLibrary(libname);
		else {
			String os = System.getProperty("os.name");
			if (os != null && os.startsWith("Windows")) {
				// library name is e.g., "libdb_java30.dll"
				// on Windows
				System.loadLibrary("libdb_java" +
				    DbConstants.DB_VERSION_MAJOR +
				    DbConstants.DB_VERSION_MINOR +
				    (DbConstants.DB_DEBUG ? "d" : ""));
			} else {
				// library name is e.g. "libdb_java-3.0.so"
				// on UNIX
				System.loadLibrary("db_java-" +
				    DbConstants.DB_VERSION_MAJOR + "." +
				    DbConstants.DB_VERSION_MINOR);
			}
		}

		initialize();
	}

	static native final void initialize();
%}

%{
/* don't use SWIG's array handling - save code space */
#define	SWIG_NOINCLUDE 1

#define	DB_ENV_INTERNAL(dbenv) ((dbenv)->api2_internal)
#define	DB_INTERNAL(db) ((db)->api_internal)

#define	DB_PKG "com/sleepycat/db/"

/* Forward declarations */
static int __dbj_throw(JNIEnv *jenv, int err, const char *msg, jobject obj, jobject jdbenv);

/* Global data - JVM handle, classes, fields and methods */
static JavaVM *javavm;

static jclass db_class, dbc_class, dbenv_class, dbt_class, dblsn_class;
static jclass dbpreplist_class, dbtxn_class;
static jclass keyrange_class;
static jclass btree_stat_class, hash_stat_class, lock_stat_class;
static jclass log_stat_class, mpool_stat_class, mpool_fstat_class;
static jclass queue_stat_class, rep_stat_class, txn_stat_class;
static jclass txn_active_class;
static jclass lock_class, lockreq_class, rep_processmsg_class;
static jclass dbex_class, deadex_class, lockex_class, memex_class;
static jclass runrecex_class;
static jclass filenotfoundex_class, illegalargex_class;
static jclass bytearray_class, string_class, outputstream_class;

static jfieldID dbc_cptr_fid;
static jfieldID dbt_data_fid, dbt_size_fid, dbt_ulen_fid, dbt_dlen_fid;
static jfieldID dbt_doff_fid, dbt_flags_fid, dbt_offset_fid;
static jfieldID kr_less_fid, kr_equal_fid, kr_greater_fid;
static jfieldID lock_cptr_fid;
static jfieldID lockreq_op_fid, lockreq_mode_fid, lockreq_timeout_fid;
static jfieldID lockreq_obj_fid, lockreq_lock_fid;
static jfieldID rep_processmsg_envid;
static jfieldID txn_stat_active_fid;

static jmethodID dbenv_construct, dbt_construct, dblsn_construct;
static jmethodID dbpreplist_construct, dbtxn_construct;
static jmethodID btree_stat_construct, hash_stat_construct;
static jmethodID lock_stat_construct, log_stat_construct, mpool_stat_construct;
static jmethodID mpool_fstat_construct, queue_stat_construct;
static jmethodID rep_stat_construct, txn_stat_construct, txn_active_construct;
static jmethodID dbex_construct, deadex_construct, lockex_construct;
static jmethodID memex_construct, memex_update_method, runrecex_construct;
static jmethodID filenotfoundex_construct, illegalargex_construct;
static jmethodID lock_construct;

static jmethodID app_dispatch_method, errcall_method, env_feedback_method;
static jmethodID paniccall_method, rep_transport_method;
static jmethodID append_recno_method, bt_compare_method, bt_prefix_method;
static jmethodID db_feedback_method, dup_compare_method, h_hash_method;
static jmethodID seckey_create_method;

static jmethodID outputstream_write_method;

const struct {
	jclass *cl;
	const char *name;
} all_classes[] = {
	{ &dbenv_class, DB_PKG "DbEnv" },
	{ &db_class, DB_PKG "Db" },
	{ &dbc_class, DB_PKG "Dbc" },
	{ &dbt_class, DB_PKG "Dbt" },
	{ &dblsn_class, DB_PKG "DbLsn" },
	{ &dbpreplist_class, DB_PKG "DbPreplist" },
	{ &dbtxn_class, DB_PKG "DbTxn" },

	{ &btree_stat_class, DB_PKG "DbBtreeStat" },
	{ &hash_stat_class, DB_PKG "DbHashStat" },
	{ &lock_stat_class, DB_PKG "DbLockStat" },
	{ &log_stat_class, DB_PKG "DbLogStat" },
	{ &mpool_fstat_class, DB_PKG "DbMpoolFStat" },
	{ &mpool_stat_class, DB_PKG "DbMpoolStat" },
	{ &queue_stat_class, DB_PKG "DbQueueStat" },
	{ &rep_stat_class, DB_PKG "DbRepStat" },
	{ &txn_stat_class, DB_PKG "DbTxnStat" },
	{ &txn_active_class, DB_PKG "DbTxnStat$Active" },

	{ &keyrange_class, DB_PKG "DbKeyRange" },
	{ &lock_class, DB_PKG "DbLock" },
	{ &lockreq_class, DB_PKG "DbLockRequest" },
	{ &rep_processmsg_class, DB_PKG "DbEnv$RepProcessMessage" },

	{ &dbex_class, DB_PKG "DbException" },
	{ &deadex_class, DB_PKG "DbDeadlockException" },
	{ &lockex_class, DB_PKG "DbLockNotGrantedException" },
	{ &memex_class, DB_PKG "DbMemoryException" },
	{ &runrecex_class, DB_PKG "DbRunRecoveryException" },
	{ &filenotfoundex_class, "java/io/FileNotFoundException" },
	{ &illegalargex_class, "java/lang/IllegalArgumentException" },

	{ &bytearray_class, "[B" },
	{ &string_class, "java/lang/String" },
	{ &outputstream_class, "java/io/OutputStream" }
};

const struct {
	jfieldID *fid;
	jclass *cl;
	const char *name;
	const char *sig;
} all_fields[] = {
	{ &dbc_cptr_fid, &dbc_class, "swigCPtr", "J" },
	
	{ &dbt_data_fid, &dbt_class, "data", "[B" },
	{ &dbt_size_fid, &dbt_class, "size", "I" },
	{ &dbt_ulen_fid, &dbt_class, "ulen", "I" },
	{ &dbt_dlen_fid, &dbt_class, "dlen", "I" },
	{ &dbt_doff_fid, &dbt_class, "doff", "I" },
	{ &dbt_flags_fid, &dbt_class, "flags", "I" },
	{ &dbt_offset_fid, &dbt_class, "offset", "I" },

	{ &kr_less_fid, &keyrange_class, "less", "D" },
	{ &kr_equal_fid, &keyrange_class, "equal", "D" },
	{ &kr_greater_fid, &keyrange_class, "greater", "D" },

	{ &lock_cptr_fid, &lock_class, "swigCPtr", "J" },

	{ &lockreq_op_fid, &lockreq_class, "op", "I" },
	{ &lockreq_mode_fid, &lockreq_class, "mode", "I" },
	{ &lockreq_timeout_fid, &lockreq_class, "timeout", "I" },
	{ &lockreq_obj_fid, &lockreq_class, "obj", "L" DB_PKG "Dbt;" },
	{ &lockreq_lock_fid, &lockreq_class, "lock", "L" DB_PKG "DbLock;" },

	{ &rep_processmsg_envid, &rep_processmsg_class, "envid", "I" },
	{ &txn_stat_active_fid, &txn_stat_class, "st_txnarray",
	    "[L" DB_PKG "DbTxnStat$Active;" }
};

const struct {
	jmethodID *mid;
	jclass *cl;
	const char *name;
	const char *sig;
} all_methods[] = {
	{ &dbenv_construct, &dbenv_class, "<init>", "(JZ)V" },
	{ &dbt_construct, &dbt_class, "<init>", "()V" },
	{ &dblsn_construct, &dblsn_class, "<init>", "(JZ)V" },
	{ &dbpreplist_construct, &dbpreplist_class, "<init>",
	    "(L" DB_PKG "DbTxn;[B)V" },
	{ &dbtxn_construct, &dbtxn_class, "<init>", "(JZ)V" },

	{ &btree_stat_construct, &btree_stat_class, "<init>", "()V" },
	{ &hash_stat_construct, &hash_stat_class, "<init>", "()V" },
	{ &lock_stat_construct, &lock_stat_class, "<init>", "()V" },
	{ &log_stat_construct, &log_stat_class, "<init>", "()V" },
	{ &mpool_stat_construct, &mpool_stat_class, "<init>", "()V" },
	{ &mpool_fstat_construct, &mpool_fstat_class, "<init>", "()V" },
	{ &queue_stat_construct, &queue_stat_class, "<init>", "()V" },
	{ &rep_stat_construct, &rep_stat_class, "<init>", "()V" },
	{ &txn_stat_construct, &txn_stat_class, "<init>", "()V" },
	{ &txn_active_construct, &txn_active_class, "<init>", "()V" },

	{ &dbex_construct, &dbex_class, "<init>", "(Ljava/lang/String;IL" DB_PKG "DbEnv;)V" },
	{ &deadex_construct, &deadex_class, "<init>",
	    "(Ljava/lang/String;IL" DB_PKG "DbEnv;)V" },
	{ &lockex_construct, &lockex_class, "<init>",
	    "(Ljava/lang/String;IIL" DB_PKG "Dbt;L" DB_PKG "DbLock;IL" DB_PKG "DbEnv;)V" },
	{ &memex_construct, &memex_class, "<init>",
	    "(Ljava/lang/String;L" DB_PKG "Dbt;IL" DB_PKG "DbEnv;)V" },
	{ &memex_update_method, &memex_class, "update_dbt",
	    "(L" DB_PKG "Dbt;)V" },
	{ &runrecex_construct, &runrecex_class, "<init>",
	    "(Ljava/lang/String;IL" DB_PKG "DbEnv;)V" },
	{ &filenotfoundex_construct, &filenotfoundex_class, "<init>",
	    "(Ljava/lang/String;)V" },
	{ &illegalargex_construct, &illegalargex_class, "<init>",
	    "(Ljava/lang/String;)V" },

	{ &lock_construct, &lock_class, "<init>", "(JZ)V" },

	{ &app_dispatch_method, &dbenv_class, "handle_app_dispatch",
	    "(L" DB_PKG "Dbt;L" DB_PKG "DbLsn;I)I" },
	{ &env_feedback_method, &dbenv_class, "handle_env_feedback", "(II)V" },
	{ &errcall_method, &dbenv_class, "handle_error",
	    "(Ljava/lang/String;)V" },
	{ &paniccall_method, &dbenv_class, "handle_panic",
	    "(L" DB_PKG "DbException;)V" },
	{ &rep_transport_method, &dbenv_class, "handle_rep_transport",
	    "(L" DB_PKG "Dbt;L" DB_PKG "Dbt;L" DB_PKG "DbLsn;II)I" },

	{ &append_recno_method, &db_class, "handle_append_recno",
	    "(L" DB_PKG "Dbt;I)V" },
	{ &bt_compare_method, &db_class, "handle_bt_compare",
	    "(L" DB_PKG "Dbt;L" DB_PKG "Dbt;)I" },
	{ &bt_prefix_method, &db_class, "handle_bt_prefix",
	    "(L" DB_PKG "Dbt;L" DB_PKG "Dbt;)I" },
	{ &db_feedback_method, &db_class, "handle_db_feedback", "(II)V" },
	{ &dup_compare_method, &db_class, "handle_dup_compare",
	    "(L" DB_PKG "Dbt;L" DB_PKG "Dbt;)I" },
	{ &h_hash_method, &db_class, "handle_h_hash", "([BI)I" },
	{ &seckey_create_method, &db_class, "handle_seckey_create",
	    "(L" DB_PKG "Dbt;L" DB_PKG "Dbt;L" DB_PKG "Dbt;)I" },

	{ &outputstream_write_method, &outputstream_class, "write", "([BII)V" }
};

#define NELEM(x) (sizeof (x) / sizeof (x[0]))

JNIEXPORT void JNICALL
Java_com_sleepycat_db_db_1javaJNI_initialize(JNIEnv *jenv, jclass clazz)
{
	jclass cl;
	unsigned int i;
	
	COMPQUIET(clazz, NULL);
	
	if ((*jenv)->GetJavaVM(jenv, &javavm) != 0) {
		__db_err(NULL, "Cannot get Java VM");
		return;
	}
	
	for (i = 0; i < NELEM(all_classes); i++) {
		cl = (*jenv)->FindClass(jenv, all_classes[i].name);
		if (cl == NULL) {
			__db_err(NULL,
			    "Failed to load class %s - check CLASSPATH",
			    all_classes[i].name);
			return;
		}

		/*
		 * Wrap classes in GlobalRefs so we keep the reference between
		 * calls.
		 */
		*all_classes[i].cl = (jclass)(*jenv)->NewGlobalRef(jenv, cl);

		if (*all_classes[i].cl == NULL) {
			__db_err(NULL,
			    "Failed to create a global reference for class %s",
			    all_classes[i].name);
			return;
		}
	}


	/* Get field IDs */
	for (i = 0; i < NELEM(all_fields); i++) {
		*all_fields[i].fid = (*jenv)->GetFieldID(jenv,
		    *all_fields[i].cl, all_fields[i].name, all_fields[i].sig);
		
		if (*all_fields[i].fid == NULL) {
			__db_err(NULL, "Failed to look up field %s",
			    all_fields[i].name);
			return;
		}
	}
	
	/* Get method IDs */
	for (i = 0; i < NELEM(all_methods); i++) {
		*all_methods[i].mid = (*jenv)->GetMethodID(jenv,
		    *all_methods[i].cl, all_methods[i].name,
		    all_methods[i].sig);
		
		if (*all_methods[i].mid == NULL) {
			__db_err(NULL, "Failed to look up method %s",
			    all_methods[i].name);
			return;
		}
	}
}

static JNIEnv *__dbj_get_jnienv(void)
{
	/*
	 * Note: Different versions of the JNI disagree on the signature for
	 * AttachCurrentThread.  The most recent documentation seems to say
	 * that (JNIEnv **) is correct, but newer JNIs seem to use (void **),
	 * oddly enough.
	 */
#ifdef JNI_VERSION_1_2
	void *jenv = 0;
#else
	JNIEnv *jenv = 0;
#endif

	/*
	 * This should always succeed, as we are called via some Java activity.
	 * I think therefore I am (a thread).
	 */
	if ((*javavm)->AttachCurrentThread(javavm, &jenv, 0) != 0)
		return (0);

	return ((JNIEnv *)jenv);
}

static jobject __dbj_wrap_DB_LSN(JNIEnv *jenv, DB_LSN *lsn)
{
	jlong jptr;
	DB_LSN *lsn_copy;
	int err;

	if ((err = __os_malloc(NULL, sizeof(DB_LSN), &lsn_copy)) != 0) {
		__dbj_throw(jenv, err, NULL, NULL, NULL);
		return NULL;
	}
	memset(lsn_copy, 0, sizeof(DB_LSN));
	*lsn_copy = *lsn;
	/* Magic to convert a pointer to a long - must match SWIG */
	*(DB_LSN **)&jptr = lsn_copy;
	return (*jenv)->NewObject(jenv, dblsn_class, dblsn_construct,
	    jptr, JNI_TRUE);
}
%}
