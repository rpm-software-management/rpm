// Callbacks
%define JAVA_CALLBACK(_sig, _jclass, _name)
JAVA_TYPEMAP(_sig, _jclass, jobject)
%typemap(javain) _sig %{ (_name##_handler = $javainput) %}

/*
 * The Java object is stored in the Db or DbEnv class.
 * Here we only care whether it is non-NULL.
 */
%typemap(in) _sig %{
	$1 = ($input == NULL) ? NULL : __dbj_##_name;
%}
%enddef

%{
/*
 * We do a dance so that the prefix in the C API points to the DB_ENV.
 * The real prefix is stored as a Java string in the DbEnv object.
 */
static void __dbj_error(const char *prefix, char *msg)
{
	DB_ENV *dbenv = (DB_ENV *)prefix;
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);

	if (jdbenv != NULL)
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv, dbenv_class,
		    errcall_method, (*jenv)->NewStringUTF(jenv, msg));
}

static void __dbj_env_feedback(DB_ENV *dbenv, int opcode, int percent)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);

	(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv, dbenv_class,
	    env_feedback_method, opcode, percent);
}

static void __dbj_panic(DB_ENV *dbenv, int err)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);

	(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv, dbenv_class,
	    paniccall_method, __dbj_get_except(jenv, err, NULL, NULL, jdbenv));
}

static int __dbj_app_dispatch(DB_ENV *dbenv,
    DBT *dbt, DB_LSN *lsn, db_recops recops)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);
	jobject jdbt, jlsn;
	jbyteArray jdbtarr;
	int ret;

	jdbt = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	__dbj_dbt_copyout(jenv, dbt, &jdbtarr, jdbt);
	if (jdbt == NULL)
		return (ENOMEM); /* An exception is pending */

	jlsn = (lsn == NULL) ? NULL : __dbj_wrap_DB_LSN(jenv, lsn);

	ret = (*jenv)->CallNonvirtualIntMethod(jenv, jdbenv, dbenv_class,
	    app_dispatch_method, jdbt, jlsn, recops);

	if ((*jenv)->ExceptionOccurred(jenv)) {
		/* The exception will be thrown, so this could be any error. */
		ret = EINVAL;
	}
	
	(*jenv)->DeleteLocalRef(jenv, jdbtarr);
	(*jenv)->DeleteLocalRef(jenv, jdbt);
	if (jlsn != NULL)
		(*jenv)->DeleteLocalRef(jenv, jlsn);

	return (ret);
}

static int __dbj_rep_transport(DB_ENV *dbenv,
    const DBT *control, const DBT *rec, const DB_LSN *lsn, int envid,
    u_int32_t flags)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);
	jobject jcontrol, jrec, jlsn;
	jbyteArray jcontrolarr, jrecarr;
	int ret;

	jcontrol = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	jrec = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	if (jcontrol == NULL || jrec == NULL)
		return (ENOMEM); /* An exception is pending */

	__dbj_dbt_copyout(jenv, control, &jcontrolarr, jcontrol);
	__dbj_dbt_copyout(jenv, rec, &jrecarr, jrec);
	jlsn = (lsn == NULL) ? NULL : __dbj_wrap_DB_LSN(jenv, (DB_LSN *)lsn);

	if (jcontrolarr == NULL || jrecarr == NULL)
		return (ENOMEM); /* An exception is pending */

	ret = (*jenv)->CallNonvirtualIntMethod(jenv, jdbenv, dbenv_class,
	    rep_transport_method, jcontrol, jrec, jlsn, envid, flags);

	if ((*jenv)->ExceptionOccurred(jenv)) {
		/* The exception will be thrown, so this could be any error. */
		ret = EINVAL;
	}
	
	(*jenv)->DeleteLocalRef(jenv, jrecarr);
	(*jenv)->DeleteLocalRef(jenv, jcontrolarr);
	(*jenv)->DeleteLocalRef(jenv, jrec);
	(*jenv)->DeleteLocalRef(jenv, jcontrol);
	if (jlsn != NULL)
		(*jenv)->DeleteLocalRef(jenv, jlsn);

	return (ret);
}

static int __dbj_seckey_create(DB *db,
    const DBT *key, const DBT *data, DBT *result)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jobject jkey, jdata, jresult;
	jbyteArray jkeyarr, jdataarr;
	DBT_LOCKED lresult;
	void *data_copy;
	int ret;

	jkey = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	jdata = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	jresult = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	if (jkey == NULL || jdata == NULL || jresult == NULL)
		return (ENOMEM); /* An exception is pending */
	
	__dbj_dbt_copyout(jenv, key, &jkeyarr, jkey);
	__dbj_dbt_copyout(jenv, data, &jdataarr, jdata);

	if (jkeyarr == NULL || jdataarr == NULL)
		return (ENOMEM); /* An exception is pending */
	
	ret = (int)(*jenv)->CallNonvirtualIntMethod(jenv, jdb, db_class,
	    seckey_create_method, jkey, jdata, jresult);

	if ((*jenv)->ExceptionOccurred(jenv)) {
		/* The exception will be thrown, so this could be any error. */
		ret = EINVAL;
		goto err;
	}
	
	if ((ret = __dbj_dbt_copyin(jenv, &lresult, jresult)) != 0)
		goto err;

	if (lresult.jarr != NULL) {
		/*
		 * If there's data, we need to make a copy because we can't
		 * keep the Java array pinned.
		 */
		memset(result, 0, sizeof (DBT));
		*result = lresult.dbt;
		if ((ret = __os_umalloc(NULL, result->size, &data_copy)) == 0)
			memcpy(data_copy, result->data, result->size);
		(*jenv)->ReleaseByteArrayElements(jenv, lresult.jarr,
		    lresult.orig_data, 0);
		(*jenv)->DeleteLocalRef(jenv, lresult.jarr);
		result->data = data_copy;
		result->flags |= DB_DBT_APPMALLOC;
	}
	
err:	(*jenv)->DeleteLocalRef(jenv, jkeyarr);
	(*jenv)->DeleteLocalRef(jenv, jkey);
	(*jenv)->DeleteLocalRef(jenv, jdataarr);
	(*jenv)->DeleteLocalRef(jenv, jdata);
	(*jenv)->DeleteLocalRef(jenv, jresult);

	return (ret);
}

static int __dbj_append_recno(DB *db, DBT *dbt, db_recno_t recno)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jobject jdbt;
	DBT_LOCKED lresult;
	void *data_copy;
	jbyteArray jdbtarr;
	int ret;

	jdbt = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	if (jdbt == NULL)
		return (ENOMEM); /* An exception is pending */

	__dbj_dbt_copyout(jenv, dbt, &jdbtarr, jdbt);
	if (jdbtarr == NULL)
		return (ENOMEM); /* An exception is pending */

	ret = 0;
	(*jenv)->CallNonvirtualVoidMethod(jenv, jdb, db_class,
 	    append_recno_method, jdbt, recno);

	if ((*jenv)->ExceptionOccurred(jenv)) {
		/* The exception will be thrown, so this could be any error. */
		ret = EINVAL;
		goto err;
	}

	if ((ret = __dbj_dbt_copyin(jenv, &lresult, jdbt)) != 0)
		goto err;

	if (lresult.jarr != NULL) {
		/*
		 * If there's data, we need to make a copy because we can't
		 * keep the Java array pinned.
		 */
		*dbt = lresult.dbt;
		if ((ret = __os_umalloc(db->dbenv, dbt->size, &data_copy)) == 0)
			memcpy(data_copy, dbt->data, dbt->size);
		(*jenv)->ReleaseByteArrayElements(jenv, lresult.jarr,
		    lresult.orig_data, 0);
		(*jenv)->DeleteLocalRef(jenv, lresult.jarr);
		dbt->data = data_copy;
		dbt->flags |= DB_DBT_APPMALLOC;
	}

err:	(*jenv)->DeleteLocalRef(jenv, jdbtarr);
	(*jenv)->DeleteLocalRef(jenv, jdbt);

	return (ret);
}

static int __dbj_bt_compare(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jobject jdbt1, jdbt2;
	jbyteArray jdbtarr1, jdbtarr2;
	int ret;

	jdbt1 = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	jdbt2 = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	if (jdbt1 == NULL || jdbt2 == NULL)
		return ENOMEM; /* An exception is pending */

	__dbj_dbt_copyout(jenv, dbt1, &jdbtarr1, jdbt1);
	__dbj_dbt_copyout(jenv, dbt2, &jdbtarr2, jdbt2);
	if (jdbtarr1 == NULL || jdbtarr2 == NULL)
		return ENOMEM; /* An exception is pending */
	
	ret = (int)(*jenv)->CallNonvirtualIntMethod(jenv, jdb, db_class,
	    bt_compare_method, jdbt1, jdbt2);

	if ((*jenv)->ExceptionOccurred(jenv)) {
		/* The exception will be thrown, so this could be any error. */
		ret = EINVAL;
	}
	
	(*jenv)->DeleteLocalRef(jenv, jdbtarr2);
	(*jenv)->DeleteLocalRef(jenv, jdbtarr1);
	(*jenv)->DeleteLocalRef(jenv, jdbt2);
	(*jenv)->DeleteLocalRef(jenv, jdbt1);

	return (ret);
}

static size_t __dbj_bt_prefix(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jobject jdbt1, jdbt2;
	jbyteArray jdbtarr1, jdbtarr2;
	int ret;

	jdbt1 = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	jdbt2 = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	if (jdbt1 == NULL || jdbt2 == NULL)
		return ENOMEM; /* An exception is pending */

	__dbj_dbt_copyout(jenv, dbt1, &jdbtarr1, jdbt1);
	__dbj_dbt_copyout(jenv, dbt2, &jdbtarr2, jdbt2);
	if (jdbtarr1 == NULL || jdbtarr2 == NULL)
		return ENOMEM; /* An exception is pending */
	
	ret = (int)(*jenv)->CallNonvirtualIntMethod(jenv, jdb, db_class,
	    bt_prefix_method, jdbt1, jdbt2);
	
	(*jenv)->DeleteLocalRef(jenv, jdbtarr2);
	(*jenv)->DeleteLocalRef(jenv, jdbtarr1);
	(*jenv)->DeleteLocalRef(jenv, jdbt2);
	(*jenv)->DeleteLocalRef(jenv, jdbt1);

	return (ret);
}

static int __dbj_dup_compare(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jobject jdbt1, jdbt2;
	jbyteArray jdbtarr1, jdbtarr2;
	int ret;

	jdbt1 = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	jdbt2 = (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	if (jdbt1 == NULL || jdbt2 == NULL)
		return ENOMEM; /* An exception is pending */

	__dbj_dbt_copyout(jenv, dbt1, &jdbtarr1, jdbt1);
	__dbj_dbt_copyout(jenv, dbt2, &jdbtarr2, jdbt2);
	if (jdbtarr1 == NULL || jdbtarr2 == NULL)
		return ENOMEM; /* An exception is pending */

	ret = (int)(*jenv)->CallNonvirtualIntMethod(jenv, jdb, db_class,
	    dup_compare_method, jdbt1, jdbt2);
	
	if ((*jenv)->ExceptionOccurred(jenv)) {
		/* The exception will be thrown, so this could be any error. */
		ret = EINVAL;
	}
	
	(*jenv)->DeleteLocalRef(jenv, jdbtarr2);
	(*jenv)->DeleteLocalRef(jenv, jdbtarr1);
	(*jenv)->DeleteLocalRef(jenv, jdbt2);
	(*jenv)->DeleteLocalRef(jenv, jdbt1);

	return (ret);
}

static void __dbj_db_feedback(DB *db, int opcode, int percent)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);

	(*jenv)->CallNonvirtualVoidMethod(jenv, jdb, db_class,
	    db_feedback_method, opcode, percent);
}

static u_int32_t __dbj_h_hash(DB *db, const void *data, u_int32_t len)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jbyteArray jarr = (*jenv)->NewByteArray(jenv, (jsize)len);
	int ret;

	if (jarr == NULL)
		return ENOMEM; /* An exception is pending */

	(*jenv)->SetByteArrayRegion(jenv, jarr, 0, (jsize)len, (jbyte *)data);

	ret = (int)(*jenv)->CallNonvirtualIntMethod(jenv, jdb, db_class,
	    h_hash_method, jarr, len);

	(*jenv)->DeleteLocalRef(jenv, jarr);

	return (ret);
}
%}

JAVA_CALLBACK(void (*db_errcall_fcn)(const char *, char *),
    DbErrorHandler, error)
JAVA_CALLBACK(void (*db_feedback_fcn)(DB_ENV *, int, int),
    DbEnvFeedbackHandler, env_feedback)
JAVA_CALLBACK(void (*db_panic_fcn)(DB_ENV *, int),
    DbPanicHandler, panic)
JAVA_CALLBACK(int (*tx_recover)(DB_ENV *, DBT *, DB_LSN *, db_recops),
    DbAppDispatch, app_dispatch)
JAVA_CALLBACK(int (*send)(DB_ENV *, const DBT *, const DBT *,
                               const DB_LSN *, int, u_int32_t),
    DbRepTransport, rep_transport)

/*
 * Db.associate is a special case, because the handler must be set in the
 * secondary DB - that's what we have in the callback.
 */
JAVA_CALLBACK(int (*callback)(DB *, const DBT *, const DBT *, DBT *),
    DbSecondaryKeyCreate, seckey_create)
%typemap(javain) int (*callback)(DB *, const DBT *, const DBT *, DBT *)
    %{ (secondary.seckey_create_handler = $javainput) %}

JAVA_CALLBACK(int (*db_append_recno_fcn)(DB *, DBT *, db_recno_t),
    DbAppendRecno, append_recno)
JAVA_CALLBACK(int (*bt_compare_fcn)(DB *, const DBT *, const DBT *),
    DbBtreeCompare, bt_compare)
JAVA_CALLBACK(size_t (*bt_prefix_fcn)(DB *, const DBT *, const DBT *),
    DbBtreePrefix, bt_prefix)
JAVA_CALLBACK(int (*dup_compare_fcn)(DB *, const DBT *, const DBT *),
    DbDupCompare, dup_compare)
JAVA_CALLBACK(void (*db_feedback_fcn)(DB *, int, int),
    DbFeedbackHandler, db_feedback)
JAVA_CALLBACK(u_int32_t (*h_hash_fcn)(DB *, const void *, u_int32_t),
    DbHash, h_hash)
