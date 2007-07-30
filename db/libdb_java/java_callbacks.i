/* Callbacks */
%define JAVA_CALLBACK(_sig, _jclass, _name)
JAVA_TYPEMAP(_sig, _jclass, jboolean)
%typemap(jtype) _sig "boolean"
%typemap(javain) _sig %{ (_name##_handler = $javainput) != null %}

/*
 * The Java object is stored in the Db or DbEnv class.
 * Here we only care whether it is non-NULL.
 */
%typemap(in) _sig %{
	$1 = ($input == JNI_TRUE) ? __dbj_##_name : NULL;
%}
%enddef

%{
static void __dbj_error(const DB_ENV *dbenv,
    const char *prefix, const char *msg)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);

	COMPQUIET(prefix, NULL);

	if (jdbenv != NULL)
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv, dbenv_class,
		    errcall_method, (*jenv)->NewStringUTF(jenv, msg));
}

static void __dbj_env_feedback(DB_ENV *dbenv, int opcode, int percent)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);

	if (jdbenv != NULL)
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv, dbenv_class,
		    env_feedback_method, opcode, percent);
}

static void __dbj_message(const DB_ENV *dbenv, const char *msg)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);

	if (jdbenv != NULL)
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv, dbenv_class,
		    msgcall_method, (*jenv)->NewStringUTF(jenv, msg));
}

static void __dbj_panic(DB_ENV *dbenv, int err)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);

	if (jdbenv != NULL)
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv, dbenv_class,
		    paniccall_method,
		    __dbj_get_except(jenv, err, NULL, NULL, jdbenv));
}

static int __dbj_app_dispatch(DB_ENV *dbenv,
    DBT *dbt, DB_LSN *lsn, db_recops recops)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);
	jobject jdbt, jlsn;
	jbyteArray jdbtarr;
	int ret;

	if (jdbenv == NULL)
		return (EINVAL);

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

static void __dbj_event_notify(DB_ENV *dbenv, u_int32_t event_id, void * info)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);

	if (jdbenv == NULL)
		return ;

	switch (event_id) {
	case DB_EVENT_PANIC:
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv,
		    dbenv_class, panic_event_notify_method);
		break;
	case DB_EVENT_REP_CLIENT:
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv,
		    dbenv_class, rep_client_event_notify_method);
		break;
	case DB_EVENT_REP_ELECTED:
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv,
		    dbenv_class, rep_elected_event_notify_method);
		break;
	case DB_EVENT_REP_MASTER:
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv,
		    dbenv_class, rep_master_event_notify_method);
		break;
	case DB_EVENT_REP_NEWMASTER:
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv,
		    dbenv_class, rep_new_master_event_notify_method, 
		    *(int*)info);
		break;
	case DB_EVENT_REP_PERM_FAILED:
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv,
		    dbenv_class, rep_perm_failed_event_notify_method);
		break;
	case DB_EVENT_REP_STARTUPDONE:
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv,
		    dbenv_class, rep_startup_done_event_notify_method);
		break;
	case DB_EVENT_WRITE_FAILED:
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdbenv,
		    dbenv_class, write_failed_event_notify_method, 
		    *(int*)info);
		break;
	default:
                dbenv->errx(dbenv, "Unhandled event callback in the Java API");
                DB_ASSERT(dbenv, 0);
	}
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

	if (jdbenv == NULL)
		return (EINVAL);

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
	jobjectArray jskeys;
	jsize i, num_skeys;
	jbyteArray jkeyarr, jdataarr;
	DBT_LOCKED lresult;
	DBT *tresult;
	int ret;

	if (jdb == NULL)
		return (EINVAL);

	jkey = (key->app_data != NULL) ?
	    ((DBT_LOCKED *)key->app_data)->jdbt :
	    (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	jdata = (data->app_data != NULL) ?
	    ((DBT_LOCKED *)data->app_data)->jdbt :
	    (*jenv)->NewObject(jenv, dbt_class, dbt_construct);
	if (jkey == NULL || jdata == NULL)
		return (ENOMEM); /* An exception is pending */

	if (key->app_data == NULL) {
		__dbj_dbt_copyout(jenv, key, &jkeyarr, jkey);
		if (jkeyarr == NULL)
			return (ENOMEM); /* An exception is pending */
	}
	if (data->app_data == NULL) {
		__dbj_dbt_copyout(jenv, data, &jdataarr, jdata);
		if (jdataarr == NULL)
			return (ENOMEM); /* An exception is pending */
	}

	jskeys = (jobjectArray)(*jenv)->CallNonvirtualObjectMethod(jenv,
	    jdb, db_class, seckey_create_method, jkey, jdata);

	if (jskeys == NULL ||
	    (num_skeys = (*jenv)->GetArrayLength(jenv, jskeys)) == 0) {
		ret = DB_DONOTINDEX;
		goto err;
	} else if (num_skeys == 1) {
		memset(result, 0, sizeof (DBT));
		tresult = result;
	} else {
		if ((ret = __os_umalloc(db->dbenv,
		    num_skeys * sizeof (DBT), &result->data)) != 0)
			goto err;
		memset(result->data, 0, num_skeys * sizeof (DBT));
		result->size = num_skeys;
		F_SET(result, DB_DBT_APPMALLOC | DB_DBT_MULTIPLE);
		tresult = (DBT *)result->data;
	}

	if ((*jenv)->ExceptionOccurred(jenv)) {
		/* The exception will be thrown, so this could be any error. */
		ret = EINVAL;
		goto err;
	}

	for (i = 0; i < num_skeys; i++, tresult++) {
		jresult = (*jenv)->GetObjectArrayElement(jenv, jskeys, i);

		if ((ret =
		    __dbj_dbt_copyin(jenv, &lresult, NULL, jresult, 0)) != 0)
			goto err;

		if (lresult.dbt.size != 0) {
			/* If there's data, we need to take a copy of it.  */
			tresult->size = lresult.dbt.size;
			if ((ret = __os_umalloc(NULL,
			    tresult->size, &tresult->data)) != 0)
				goto err;
			if ((ret = __dbj_dbt_memcopy(&lresult.dbt, 0,
			    tresult->data, tresult->size,
			    DB_USERCOPY_GETDATA)) != 0)
				goto err;
			__dbj_dbt_release(jenv,
			    jresult, &lresult.dbt, &lresult);
			(*jenv)->DeleteLocalRef(jenv, lresult.jarr);
			F_SET(tresult, DB_DBT_APPMALLOC);
		}

		(*jenv)->DeleteLocalRef(jenv, jresult);
	}

err:	if (key->app_data == NULL) {
		(*jenv)->DeleteLocalRef(jenv, jkeyarr);
		(*jenv)->DeleteLocalRef(jenv, jkey);
	}
	if (data->app_data == NULL) {
		(*jenv)->DeleteLocalRef(jenv, jdataarr);
		(*jenv)->DeleteLocalRef(jenv, jdata);
	}

	return (ret);
}

static int __dbj_append_recno(DB *db, DBT *dbt, db_recno_t recno)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jobject jdbt;
	DBT_LOCKED lresult;
	jbyteArray jdbtarr;
	int ret;

	if (jdb == NULL)
		return (EINVAL);

	/*
	 * The dbt we're passed will be from the application, but we can't
	 * just reuse it, since we will have already taken a copy of the data.
	 * Make a new DatabaseEntry object here for the callback.
	 */
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
		return (EINVAL);
	}

	ret = __dbj_dbt_copyin(jenv, &lresult, NULL, jdbt, 0);
	memset(dbt, 0, sizeof (DBT));

	if (ret == 0 && lresult.dbt.size != 0) {
		/* If there's data, we need to take a copy of it.  */
		dbt->size = lresult.dbt.size;
		if ((ret =
		    __os_umalloc(NULL, dbt->size, &dbt->data)) != 0)
			goto err;
		if ((ret = __dbj_dbt_memcopy(&lresult.dbt, 0,
		    dbt->data, dbt->size,
		    DB_USERCOPY_GETDATA)) != 0)
			goto err;
		__dbj_dbt_release(jenv, jdbt, &lresult.dbt, &lresult);
		(*jenv)->DeleteLocalRef(jenv, lresult.jarr);
		F_SET(dbt, DB_DBT_APPMALLOC);
	}

err:	(*jenv)->DeleteLocalRef(jenv, jdbtarr);
	(*jenv)->DeleteLocalRef(jenv, jdbt);

	return (ret);
}

/*
 * Shared by __dbj_bt_compare and __dbj_h_compare
 */
static int __dbj_am_compare(DB *db, const DBT *dbt1, const DBT *dbt2,
    jmethodID compare_method)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jbyteArray jdbtarr1, jdbtarr2;
	int ret;

	if (jdb == NULL)
		return (EINVAL);

	if (dbt1->app_data != NULL)
		jdbtarr1 = ((DBT_LOCKED *)dbt1->app_data)->jarr;
	else {
		jdbtarr1 = (*jenv)->NewByteArray(jenv, (jsize)dbt1->size);
		if (jdbtarr1 == NULL)
			return (ENOMEM);
		(*jenv)->SetByteArrayRegion(jenv, jdbtarr1, 0,
		    (jsize)dbt1->size, (jbyte *)dbt1->data);
	}

	if (dbt2->app_data != NULL)
		jdbtarr2 = ((DBT_LOCKED *)dbt2->app_data)->jarr;
	else {
		jdbtarr2 = (*jenv)->NewByteArray(jenv, (jsize)dbt2->size);
		if (jdbtarr2 == NULL)
			return (ENOMEM);
		(*jenv)->SetByteArrayRegion(jenv, jdbtarr2, 0,
		    (jsize)dbt2->size, (jbyte *)dbt2->data);
	}

	ret = (int)(*jenv)->CallNonvirtualIntMethod(jenv, jdb, db_class,
	    compare_method, jdbtarr1, jdbtarr2);

	if ((*jenv)->ExceptionOccurred(jenv)) {
		/* The exception will be thrown, so this could be any error. */
		ret = EINVAL;
	}

	if (dbt1->app_data == NULL)
		(*jenv)->DeleteLocalRef(jenv, jdbtarr1);
	if (dbt2->app_data == NULL)
		(*jenv)->DeleteLocalRef(jenv, jdbtarr2);

	return (ret);
}

static int __dbj_bt_compare(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return __dbj_am_compare(db, dbt1, dbt2, bt_compare_method);
}

static size_t __dbj_bt_prefix(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jobject jdbt1, jdbt2;
	jbyteArray jdbtarr1, jdbtarr2;
	int ret;

	if (jdb == NULL)
		return (EINVAL);

	if (dbt1->app_data != NULL)
		jdbt1 = ((DBT_LOCKED *)dbt1->app_data)->jdbt;
	else {
		if ((jdbt1 =
		    (*jenv)->NewObject(jenv, dbt_class, dbt_construct)) == NULL)
			return (ENOMEM); /* An exception is pending */
		__dbj_dbt_copyout(jenv, dbt1, &jdbtarr1, jdbt1);
		if (jdbtarr1 == NULL)
			return (ENOMEM); /* An exception is pending */
	}

	if (dbt2->app_data != NULL)
		jdbt2 = ((DBT_LOCKED *)dbt2->app_data)->jdbt;
	else {
		if ((jdbt2 =
		    (*jenv)->NewObject(jenv, dbt_class, dbt_construct)) == NULL)
			return (ENOMEM); /* An exception is pending */
		__dbj_dbt_copyout(jenv, dbt2, &jdbtarr2, jdbt2);
		if (jdbtarr2 == NULL)
			return (ENOMEM); /* An exception is pending */
	}

	ret = (int)(*jenv)->CallNonvirtualIntMethod(jenv, jdb, db_class,
	    bt_prefix_method, jdbt1, jdbt2);

	if (dbt1->app_data == NULL) {
		(*jenv)->DeleteLocalRef(jenv, jdbtarr1);
		(*jenv)->DeleteLocalRef(jenv, jdbt1);
	}
	if (dbt2->app_data == NULL) {
		(*jenv)->DeleteLocalRef(jenv, jdbtarr2);
		(*jenv)->DeleteLocalRef(jenv, jdbt2);
	}

	return (ret);
}

static int __dbj_dup_compare(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jbyteArray jdbtarr1, jdbtarr2;
	int ret;

	if (jdb == NULL)
		return (EINVAL);

	jdbtarr1 = (*jenv)->NewByteArray(jenv, (jsize)dbt1->size);
	if (jdbtarr1 == NULL)
		return (ENOMEM);
	(*jenv)->SetByteArrayRegion(jenv, jdbtarr1, 0, (jsize)dbt1->size,
	    (jbyte *)dbt1->data);

	jdbtarr2 = (*jenv)->NewByteArray(jenv, (jsize)dbt2->size);
	if (jdbtarr2 == NULL)
		return (ENOMEM);
	(*jenv)->SetByteArrayRegion(jenv, jdbtarr2, 0, (jsize)dbt2->size,
	    (jbyte *)dbt2->data);

	ret = (int)(*jenv)->CallNonvirtualIntMethod(jenv, jdb, db_class,
	    dup_compare_method, jdbtarr1, jdbtarr2);

	if ((*jenv)->ExceptionOccurred(jenv)) {
		/* The exception will be thrown, so this could be any error. */
		ret = EINVAL;
	}

	(*jenv)->DeleteLocalRef(jenv, jdbtarr2);
	(*jenv)->DeleteLocalRef(jenv, jdbtarr1);

	return (ret);
}

static void __dbj_db_feedback(DB *db, int opcode, int percent)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);

	if (jdb != NULL)
		(*jenv)->CallNonvirtualVoidMethod(jenv, jdb, db_class,
		    db_feedback_method, opcode, percent);
}

static int __dbj_h_compare(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return __dbj_am_compare(db, dbt1, dbt2, h_compare_method);
}

static u_int32_t __dbj_h_hash(DB *db, const void *data, u_int32_t len)
{
	JNIEnv *jenv = __dbj_get_jnienv();
	jobject jdb = (jobject)DB_INTERNAL(db);
	jbyteArray jarr = (*jenv)->NewByteArray(jenv, (jsize)len);
	int ret;

	if (jdb == NULL)
		return (EINVAL);

	if ((jarr = (*jenv)->NewByteArray(jenv, (jsize)len)) == NULL)
		return (ENOMEM); /* An exception is pending */

	(*jenv)->SetByteArrayRegion(jenv, jarr, 0, (jsize)len, (jbyte *)data);

	ret = (int)(*jenv)->CallNonvirtualIntMethod(jenv, jdb, db_class,
	    h_hash_method, jarr, len);

	(*jenv)->DeleteLocalRef(jenv, jarr);

	return (ret);
}
%}

JAVA_CALLBACK(void (*db_errcall_fcn)(const DB_ENV *,
    const char *, const char *), com.sleepycat.db.ErrorHandler, error)
JAVA_CALLBACK(void (*env_feedback_fcn)(DB_ENV *, int, int),
    com.sleepycat.db.FeedbackHandler, env_feedback)
JAVA_CALLBACK(void (*db_msgcall_fcn)(const DB_ENV *, const char *),
    com.sleepycat.db.MessageHandler, message)
JAVA_CALLBACK(void (*db_panic_fcn)(DB_ENV *, int),
    com.sleepycat.db.PanicHandler, panic)
JAVA_CALLBACK(void (*event_notify)(DB_ENV *, u_int32_t, void *),
    com.sleepycat.db.EventHandler, event_notify)
JAVA_CALLBACK(int (*tx_recover)(DB_ENV *, DBT *, DB_LSN *, db_recops),
    com.sleepycat.db.LogRecordHandler, app_dispatch)
JAVA_CALLBACK(int (*send)(DB_ENV *, const DBT *, const DBT *,
			       const DB_LSN *, int, u_int32_t),
    com.sleepycat.db.ReplicationTransport, rep_transport)

/*
 * Db.associate is a special case, because the handler must be set in the
 * secondary DB - that's what we have in the callback.  In addition, there
 * are two flavors of callback (single key and multi-key), so we need to
 * check for both types when working out whether the C callback should
 * be NULL.  Note that this implies that the multi-key callback will be set
 * on the secondary database *before* associate is called.
 */
JAVA_CALLBACK(int (*callback)(DB *, const DBT *, const DBT *, DBT *),
    com.sleepycat.db.SecondaryKeyCreator, seckey_create)
%typemap(javain) int (*callback)(DB *, const DBT *, const DBT *, DBT *)
    %{ (secondary.seckey_create_handler = $javainput) != null ||
	(secondary.secmultikey_create_handler != null) %}

JAVA_CALLBACK(int (*db_append_recno_fcn)(DB *, DBT *, db_recno_t),
    com.sleepycat.db.RecordNumberAppender, append_recno)
JAVA_CALLBACK(int (*bt_compare_fcn)(DB *, const DBT *, const DBT *),
    java.util.Comparator, bt_compare)
JAVA_CALLBACK(size_t (*bt_prefix_fcn)(DB *, const DBT *, const DBT *),
    com.sleepycat.db.BtreePrefixCalculator, bt_prefix)
JAVA_CALLBACK(int (*dup_compare_fcn)(DB *, const DBT *, const DBT *),
    java.util.Comparator, dup_compare)
JAVA_CALLBACK(void (*db_feedback_fcn)(DB *, int, int),
    com.sleepycat.db.FeedbackHandler, db_feedback)
JAVA_CALLBACK(int (*h_compare_fcn)(DB *, const DBT *, const DBT *),
    java.util.Comparator, h_compare)
JAVA_CALLBACK(u_int32_t (*h_hash_fcn)(DB *, const void *, u_int32_t),
    com.sleepycat.db.Hasher, h_hash)
