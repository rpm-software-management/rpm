/* Typemaps */
%define JAVA_TYPEMAP(_ctype, _jtype, _jnitype)
%typemap(jstype) _ctype #_jtype
%typemap(jtype) _ctype #_jtype
%typemap(jni) _ctype #_jnitype
%typemap(out) _ctype %{ $result = (_jnitype)$1; %}
%typemap(javain) _ctype "$javainput"
%typemap(javaout) _ctype { return $jnicall; }
%enddef

JAVA_TYPEMAP(int32_t, int, jint)
JAVA_TYPEMAP(u_int32_t, int, jint)
JAVA_TYPEMAP(u_int32_t pagesize, long, jlong)
JAVA_TYPEMAP(long, long, jlong)
JAVA_TYPEMAP(db_seq_t, long, jlong)
JAVA_TYPEMAP(db_timeout_t, long, jlong)
JAVA_TYPEMAP(size_t, long, jlong)
JAVA_TYPEMAP(db_ret_t, void, void)
%typemap(javaout) db_ret_t { $jnicall; }
%typemap(out) db_ret_t ""

JAVA_TYPEMAP(int_bool, boolean, jboolean)
%typemap(in) int_bool %{ $1 = ($input == JNI_TRUE); %}
%typemap(out) int_bool %{ $result = ($1) ? JNI_TRUE : JNI_FALSE; %}

/* Dbt handling */
JAVA_TYPEMAP(DBT *, com.sleepycat.db.DatabaseEntry, jobject)

%{
typedef struct __dbt_locked {
	DBT dbt;
	jbyteArray jarr;
	jbyte *orig_data;
	jint offset;
	int reuse;
	u_int32_t orig_size;
	jsize array_len;
} DBT_LOCKED;

static int __dbj_dbt_copyin(
    JNIEnv *jenv, DBT_LOCKED *ldbt, DBT **dbtp, jobject jdbt, int allow_null)
{
	DBT *dbt;

	memset(ldbt, 0, sizeof (*ldbt));

	if (jdbt == NULL) {
		if (allow_null) {
			*dbtp = NULL;
			return (0);
		} else {
			return (__dbj_throw(jenv, EINVAL,
			    "DatabaseEntry must not be null", NULL, NULL));
		}
	}
	
	dbt = &ldbt->dbt;
	if (dbtp != NULL)
		*dbtp = dbt;
	
	ldbt->jarr = (jbyteArray)(*jenv)->GetObjectField(jenv,
	    jdbt, dbt_data_fid);
	ldbt->offset = (*jenv)->GetIntField(jenv, jdbt, dbt_offset_fid);

	dbt->size = (*jenv)->GetIntField(jenv, jdbt, dbt_size_fid);
	ldbt->orig_size = dbt->size;
	dbt->ulen = (*jenv)->GetIntField(jenv, jdbt, dbt_ulen_fid);
	dbt->dlen = (*jenv)->GetIntField(jenv, jdbt, dbt_dlen_fid);
	dbt->doff = (*jenv)->GetIntField(jenv, jdbt, dbt_doff_fid);
	dbt->flags = (*jenv)->GetIntField(jenv, jdbt, dbt_flags_fid);

	/*
	 * We don't support DB_DBT_REALLOC - map anything that's not USERMEM to
	 * MALLOC.
	 */
	if (!F_ISSET(dbt, DB_DBT_USERMEM)) {
		ldbt->reuse = !F_ISSET(dbt, DB_DBT_MALLOC);
		F_CLR(dbt, DB_DBT_REALLOC);
		F_SET(dbt, DB_DBT_MALLOC);
	}

	/* Verify parameters before allocating or locking data. */
	if ((jint)dbt->doff < 0)
		return (__dbj_throw(jenv, EINVAL, "DatabaseEntry doff illegal",
		    NULL, NULL));
	
	if (ldbt->jarr == NULL) {
		/*
		 * Some code makes the assumption that if a DBT's size or ulen
		 * is non-zero, there is data to copy from dbt->data.  
		 *
		 * Clean up the dbt fields so we don't run into trouble.
		 * (Note that doff, dlen, and flags all may contain
		 * meaningful values.)
		 */
		ldbt->orig_data = dbt->data = NULL;
		ldbt->array_len = ldbt->offset = dbt->size = dbt->ulen = 0;
		return (0);
	} else
		ldbt->array_len = (*jenv)->GetArrayLength(jenv, ldbt->jarr);
	
	if (F_ISSET(dbt, DB_DBT_USERMEM)) {
		if (ldbt->offset < 0)
			return (__dbj_throw(jenv, EINVAL,
			    "offset cannot be negative",
			    NULL, NULL));
		if (dbt->size > dbt->ulen)
			return (__dbj_throw(jenv, EINVAL,
			    "size must be less than or equal to ulen",
			    NULL, NULL));
		if ((jsize)(ldbt->offset + dbt->ulen) > ldbt->array_len)
			return (__dbj_throw(jenv, EINVAL,
			    "offset + ulen greater than array length",
			    NULL, NULL));
		if ((ldbt->orig_data = (*jenv)->GetByteArrayElements(jenv,
		    ldbt->jarr, NULL)) == NULL)
			return (EINVAL); /* an exception will be pending */
		dbt->data = ldbt->orig_data + ldbt->offset;
	} else {
		if (__os_umalloc(NULL, dbt->size, &dbt->data) != 0)
			return (ENOMEM);
		ldbt->orig_data = dbt->data;
		(*jenv)->GetByteArrayRegion(jenv,
		    ldbt->jarr, ldbt->offset, dbt->size, dbt->data);
		if ((*jenv)->ExceptionOccurred(jenv)) {
			(void)__os_ufree(NULL, dbt->data);
			return (EINVAL);
		}
	}

	return (0);
}

static void __dbj_dbt_copyout(
    JNIEnv *jenv, const DBT *dbt, jbyteArray *jarr, jobject jdbt)
{
	jbyteArray newarr = (*jenv)->NewByteArray(jenv, (jsize)dbt->size);
	if (newarr == NULL)
		return; /* An exception is pending */
	(*jenv)->SetByteArrayRegion(jenv, newarr, 0, (jsize)dbt->size,
	    (jbyte *)dbt->data);
	(*jenv)->SetObjectField(jenv, jdbt, dbt_data_fid, newarr);
	(*jenv)->SetIntField(jenv, jdbt, dbt_offset_fid, 0);
	(*jenv)->SetIntField(jenv, jdbt, dbt_size_fid, (jint)dbt->size);
	if (jarr != NULL)
		*jarr = newarr;
	else
		(*jenv)->DeleteLocalRef(jenv, newarr);
}

static void __dbj_dbt_release(
    JNIEnv *jenv, jobject jdbt, DBT *dbt, DBT_LOCKED *ldbt) {
	jthrowable t;

	if (dbt == NULL)
		return;
	
	if (dbt->size != ldbt->orig_size)
		(*jenv)->SetIntField(jenv, jdbt, dbt_size_fid, (jint)dbt->size);
	    
	if (F_ISSET(dbt, DB_DBT_USERMEM)) {
                if (ldbt->jarr != NULL)
		    (*jenv)->ReleaseByteArrayElements(jenv,
		        ldbt->jarr, ldbt->orig_data, 0);

		if (dbt->size > dbt->ulen &&
		    (t = (*jenv)->ExceptionOccurred(jenv)) != NULL &&
		    (*jenv)->IsInstanceOf(jenv, t, memex_class)) {
			(*jenv)->CallNonvirtualVoidMethod(jenv, t, memex_class,
			    memex_update_method, jdbt);
			/*
			 * We have to rethrow the exception because calling
			 * into Java clears it.
			 */
			(*jenv)->Throw(jenv, t);
		}
	} else {
		if (dbt->size > 0 && dbt->data != ldbt->orig_data) {
			if (ldbt->reuse &&
			    (jsize)(ldbt->offset + dbt->size) <= ldbt->array_len)
				(*jenv)->SetByteArrayRegion(jenv,
				    ldbt->jarr, ldbt->offset, (jsize)dbt->size,
				    (jbyte *)dbt->data);
			else
				__dbj_dbt_copyout(jenv, dbt, NULL, jdbt);
			(void)__os_ufree(NULL, dbt->data);
		}

		if (ldbt->orig_data != NULL)
			(void)__os_ufree(NULL, ldbt->orig_data);
	}
}
%}

%typemap(in) DBT * (DBT_LOCKED ldbt) %{
	if (__dbj_dbt_copyin(jenv, &ldbt, &$1, $input, 0) != 0) {
		return $null;
	}%}

/* Special case for the cdata param in DbEnv.rep_start - it may be null */
%typemap(in) DBT *cdata (DBT_LOCKED ldbt) %{
	if (__dbj_dbt_copyin(jenv, &ldbt, &$1, $input, 1) != 0) {
		return $null;
	}%}

%typemap(freearg) DBT * %{ __dbj_dbt_release(jenv, $input, $1, &ldbt$argnum); %}


/* DbLsn handling */
JAVA_TYPEMAP(DB_LSN *, com.sleepycat.db.LogSequenceNumber, jobject)

%typemap(check) DB_LSN * %{
	if ($input == NULL) {
		__dbj_throw(jenv, EINVAL, "LogSequenceNumber must not be null", NULL, NULL);
		return $null;
	}
%}

%typemap(in) DB_LSN * (DB_LSN lsn) %{
	/* XXX: TODO */
	$1 = &lsn;
%}

%typemap(freearg) DB_LSN * %{
	/* XXX: TODO */
	/* -- __dbj_dbt_release(jenv, $input, $1, &lsn$argnum); */
%}


/* Various typemaps */
JAVA_TYPEMAP(time_t, long, jlong)
JAVA_TYPEMAP(time_t *, long, jlong)
%typemap(in) time_t * (time_t time) %{
	time = (time_t)$input;
	$1 = &time;
%}

JAVA_TYPEMAP(void *client, DbClient, jobject)
%typemap(check) void *client %{
	if ($1 != NULL) {
		__dbj_throw(jenv, EINVAL, "DbEnv.set_rpc_server client arg "
		    "must be null; reserved for future use", NULL, JDBENV);
		return $null;
	}
%}

JAVA_TYPEMAP(DB_KEY_RANGE *, com.sleepycat.db.KeyRange, jobject)
%typemap(in) DB_KEY_RANGE * (DB_KEY_RANGE range) {
	$1 = &range;
}
%typemap(argout) DB_KEY_RANGE * {
	(*jenv)->SetDoubleField(jenv, $input, kr_less_fid, $1->less);
	(*jenv)->SetDoubleField(jenv, $input, kr_equal_fid, $1->equal);
	(*jenv)->SetDoubleField(jenv, $input, kr_greater_fid, $1->greater);
}

JAVA_TYPEMAP(DBC **, Dbc[], jobjectArray)
%typemap(in) DBC ** {
	int i, count, err;
	
	count = (*jenv)->GetArrayLength(jenv, $input);
	if ((err = __os_malloc(NULL, (count + 1) * sizeof(DBC *), &$1)) != 0) {
		__dbj_throw(jenv, err, NULL, NULL, DB2JDBENV);
		return $null;
	}
	for (i = 0; i < count; i++) {
		jobject jobj = (*jenv)->GetObjectArrayElement(jenv, $input, i);
		/*
		 * A null in the array is treated as an endpoint.
		 */
		if (jobj == NULL) {
			$1[i] = NULL;
			break;
		} else {
			jlong jptr = (*jenv)->GetLongField(jenv, jobj,
			    dbc_cptr_fid);
			$1[i] = *(DBC **)&jptr;
		}
	}
	$1[count] = NULL;
}

%typemap(freearg) DBC ** %{
	__os_free(NULL, $1);
%}

JAVA_TYPEMAP(u_int8_t *gid, byte[], jbyteArray)
%typemap(check) u_int8_t *gid %{
	if ((*jenv)->GetArrayLength(jenv, $input) < DB_XIDDATASIZE) {
		__dbj_throw(jenv, EINVAL,
		    "DbTxn.prepare gid array must be >= 128 bytes", NULL, TXN2JDBENV);
		return $null;
	}
%}

%typemap(in) u_int8_t *gid %{
	$1 = (u_int8_t *)(*jenv)->GetByteArrayElements(jenv, $input, NULL);
%}

%typemap(freearg) u_int8_t *gid %{
	(*jenv)->ReleaseByteArrayElements(jenv, $input, (jbyte *)$1, 0);
%}

%define STRING_ARRAY_OUT
	int i, len;
	
	len = 0;
	while ($1[len] != NULL)
		len++;
	if (($result = (*jenv)->NewObjectArray(jenv, (jsize)len, string_class,
	    NULL)) == NULL)
		return $null; /* an exception is pending */
	for (i = 0; i < len; i++) {
		jstring str = (*jenv)->NewStringUTF(jenv, $1[i]);
		(*jenv)->SetObjectArrayElement(jenv, $result, (jsize)i, str);
	}
%enddef

JAVA_TYPEMAP(char **, String[], jobjectArray)
%typemap(out) const char ** {
	if($1 != NULL) {
		STRING_ARRAY_OUT
	}
}
%typemap(out) char ** {
	if($1 != NULL) {
		STRING_ARRAY_OUT
		__os_ufree(NULL, $1);
	}
}

JAVA_TYPEMAP(struct __db_lk_conflicts, byte[][], jobjectArray)
%typemap(in) struct __db_lk_conflicts {
	int i, len, err;
	size_t bytesize;

	len = $1.lk_modes = (*jenv)->GetArrayLength(jenv, $input);
	bytesize = sizeof(u_char) * len * len;

	if ((err = __os_malloc(NULL, bytesize, &$1.lk_conflicts)) != 0) {
		__dbj_throw(jenv, err, NULL, NULL, JDBENV);
		return $null;
	}

	for (i = 0; i < len; i++) {
		jobject sub_array = (*jenv)->GetObjectArrayElement(jenv,
		    $input, i);
		(*jenv)->GetByteArrayRegion(jenv,(jbyteArray)sub_array, 0, len,
		    (jbyte *)&$1.lk_conflicts[i * len]);
	}
}

%typemap(freearg) struct __db_lk_conflicts %{
	__os_free(NULL, $1.lk_conflicts);
%}

%typemap(out) struct __db_lk_conflicts {
	int i;
	jbyteArray bytes;
	
	$result = (*jenv)->NewObjectArray(jenv,
	    (jsize)$1.lk_modes, bytearray_class, NULL);
	if ($result == NULL)
		return $null; /* an exception is pending */
	for (i = 0; i < $1.lk_modes; i++) {
		bytes = (*jenv)->NewByteArray(jenv, (jsize)$1.lk_modes);
		if (bytes == NULL)
			return $null; /* an exception is pending */
		(*jenv)->SetByteArrayRegion(jenv, bytes, 0, (jsize)$1.lk_modes,
		    (jbyte *)($1.lk_conflicts + i * $1.lk_modes));
		(*jenv)->SetObjectArrayElement(jenv, $result, (jsize)i, bytes);
	}
}

%{
struct __dbj_verify_data {
	JNIEnv *jenv;
	jobject streamobj;
	jbyteArray bytes;
	int nbytes;
};

static int __dbj_verify_callback(void *handle, const void *str_arg) {
	char *str;
	struct __dbj_verify_data *vd;
	int len;
	JNIEnv *jenv;

	str = (char *)str_arg;
	vd = (struct __dbj_verify_data *)handle;
	jenv = vd->jenv;
	len = strlen(str) + 1;
	if (len > vd->nbytes) {
		vd->nbytes = len;
		if (vd->bytes != NULL)
			(*jenv)->DeleteLocalRef(jenv, vd->bytes);
		if ((vd->bytes = (*jenv)->NewByteArray(jenv, (jsize)len))
		    == NULL)
			return (ENOMEM);
	}

	if (vd->bytes != NULL) {
		(*jenv)->SetByteArrayRegion(jenv, vd->bytes, 0, (jsize)len,
		    (jbyte*)str);
		(*jenv)->CallVoidMethod(jenv, vd->streamobj,
		    outputstream_write_method, vd->bytes, 0, len - 1);
	}

	if ((*jenv)->ExceptionOccurred(jenv) != NULL)
		return (EIO);

	return (0);
}
%}

JAVA_TYPEMAP(struct __db_out_stream, java.io.OutputStream, jobject)
%typemap(in) struct __db_out_stream (struct __dbj_verify_data data) {
	data.jenv = jenv;
	data.streamobj = $input;
	data.bytes = NULL;
	data.nbytes = 0;
	$1.handle = &data;
	$1.callback = __dbj_verify_callback;
}

JAVA_TYPEMAP(DB_PREPLIST *, com.sleepycat.db.PreparedTransaction[], jobjectArray)
%typemap(out) DB_PREPLIST * {
	int i, len;
	
	len = 0;
	while ($1[len].txn != NULL)
		len++;
	$result = (*jenv)->NewObjectArray(jenv, (jsize)len, dbpreplist_class,
	    NULL);
	if ($result == NULL)
		return $null; /* an exception is pending */
	for (i = 0; i < len; i++) {
		jobject jtxn = (*jenv)->NewObject(jenv, dbtxn_class,
		    dbtxn_construct, $1[i].txn, JNI_FALSE);
		jobject bytearr = (*jenv)->NewByteArray(jenv,
		    (jsize)sizeof($1[i].gid));
		jobject obj = (*jenv)->NewObject(jenv, dbpreplist_class,
		    dbpreplist_construct, jtxn, bytearr);

		if (jtxn == NULL || bytearr == NULL || obj == NULL)
			return $null; /* An exception is pending */

		(*jenv)->SetByteArrayRegion(jenv, bytearr, 0,
		    (jsize)sizeof($1[i].gid), (jbyte *)$1[i].gid);
		(*jenv)->SetObjectArrayElement(jenv, $result, i, obj);
	}
	__os_ufree(NULL, $1);
}

JAVA_TYPEMAP(DB_LOCKREQ *, com.sleepycat.db.LockRequest[], jobjectArray)

%native(DbEnv_lock_vec) void DbEnv_lock_vec(DB_ENV *dbenv, u_int32_t locker,
    u_int32_t flags, DB_LOCKREQ *list, int offset, int nlist);
%{
JNIEXPORT void JNICALL Java_com_sleepycat_db_internal_db_1javaJNI_DbEnv_1lock_1vec
    (JNIEnv *jenv, jclass jcls, jlong jdbenvp, jint locker,
    jint flags, jobjectArray list, jint offset, jint count) {
	DB_ENV *dbenv;
	DB_LOCKREQ *lockreq;
	DB_LOCKREQ *prereq;	/* preprocessed requests */
	DB_LOCKREQ *failedreq;
	DB_LOCK *lockp;
	DBT_LOCKED *locked_dbts;
	int err, alloc_err, i;
	size_t bytesize, ldbtsize;
	jobject jlockreq;
	db_lockop_t op;
	jobject jobj, jlock, jdbenv;
	jlong jlockp;
	int completed;

	COMPQUIET(jcls, NULL);
	dbenv = *(DB_ENV **)&jdbenvp;
	jdbenv = (jobject)DB_ENV_INTERNAL(dbenv);

	if (dbenv == NULL) {
		__dbj_throw(jenv, EINVAL, "null object", NULL, jdbenv);
		return;
	}

	if ((*jenv)->GetArrayLength(jenv, list) < offset + count) {
		__dbj_throw(jenv, EINVAL,
		    "DbEnv.lock_vec array not large enough", NULL, jdbenv);
		goto out0;
	}

	bytesize = sizeof(DB_LOCKREQ) * count;
	if ((err = __os_malloc(dbenv, bytesize, &lockreq)) != 0) {
		__dbj_throw(jenv, err, NULL, NULL, jdbenv);
		goto out0;
	}
	memset(lockreq, 0, bytesize);

	ldbtsize = sizeof(DBT_LOCKED) * count;
	if ((err = __os_malloc(dbenv, ldbtsize, &locked_dbts)) != 0) {
		__dbj_throw(jenv, err, NULL, NULL, jdbenv);
		goto out1;
	}
	memset(locked_dbts, 0, ldbtsize);
	prereq = &lockreq[0];

	/* fill in the lockreq array */
	for (i = 0, prereq = &lockreq[0]; i < count; i++, prereq++) {
		jlockreq = (*jenv)->GetObjectArrayElement(jenv, list,
		    offset + i);
		if (jlockreq == NULL) {
			__dbj_throw(jenv, EINVAL,
			    "DbEnv.lock_vec list entry is null", NULL, jdbenv);
			goto out2;
		}
		op = (*jenv)->GetIntField(jenv, jlockreq, lockreq_op_fid);
		prereq->op = op;

		switch (op) {
		case DB_LOCK_GET_TIMEOUT:
			/* Needed: mode, timeout, obj.  Returned: lock. */
			prereq->op = (*jenv)->GetIntField(jenv, jlockreq,
			    lockreq_timeout_fid);
			/* FALLTHROUGH */
		case DB_LOCK_GET:
			/* Needed: mode, obj.  Returned: lock. */
			prereq->mode = (*jenv)->GetIntField(jenv, jlockreq,
			    lockreq_modeflag_fid);
			jobj = (*jenv)->GetObjectField(jenv, jlockreq,
			    lockreq_obj_fid);
			if ((err = __dbj_dbt_copyin(jenv,
			    &locked_dbts[i], &prereq->obj, jobj, 0)) != 0)
				goto out2;
			break;
		case DB_LOCK_PUT:
			/* Needed: lock.  Ignored: mode, obj. */
			jlock = (*jenv)->GetObjectField(jenv, jlockreq,
				lockreq_lock_fid);
			if (jlock == NULL ||
			    (jlockp = (*jenv)->GetLongField(jenv, jlock,
			    lock_cptr_fid)) == 0L) {
				__dbj_throw(jenv, EINVAL,
				    "LockRequest lock field is NULL", NULL, jdbenv);
				goto out2;
			}
			lockp = *(DB_LOCK **)&jlockp;
			prereq->lock = *lockp;
			break;
		case DB_LOCK_PUT_ALL:
		case DB_LOCK_TIMEOUT:
			/* Needed: (none).  Ignored: lock, mode, obj. */
			break;
		case DB_LOCK_PUT_OBJ:
			/* Needed: obj.  Ignored: lock, mode. */
			jobj = (*jenv)->GetObjectField(jenv, jlockreq,
			    lockreq_obj_fid);
			if ((err = __dbj_dbt_copyin(jenv,
			    &locked_dbts[i], &prereq->obj, jobj, 0)) != 0)
				goto out2;
			break;
		default:
			__dbj_throw(jenv, EINVAL,
			    "DbEnv.lock_vec bad op value", NULL, jdbenv);
			goto out2;
		}
	}

	err = dbenv->lock_vec(dbenv, (u_int32_t)locker, (u_int32_t)flags,
	    lockreq, count, &failedreq);
	if (err == 0)
		completed = count;
	else
		completed = failedreq - lockreq;

	/* do post processing for any and all requests that completed */
	for (i = 0; i < completed; i++) {
		op = lockreq[i].op;
		if (op == DB_LOCK_PUT) {
			/*
			 * After a successful put, the DbLock can no longer be
			 * used, so we release the storage related to it.
			 */
			jlockreq = (*jenv)->GetObjectArrayElement(jenv,
			    list, i + offset);
			jlock = (*jenv)->GetObjectField(jenv, jlockreq,
			    lockreq_lock_fid);
			jlockp = (*jenv)->GetLongField(jenv, jlock,
			    lock_cptr_fid);
			lockp = *(DB_LOCK **)&jlockp;
			__os_free(NULL, lockp);
			(*jenv)->SetLongField(jenv, jlock, lock_cptr_fid,
			    (jlong)0);
		}
		else if (op == DB_LOCK_GET) {
			/*
			 * Store the lock that was obtained.  We need to create
			 * storage for it since the lockreq array only exists
			 * during this method call.
			 */
			if ((alloc_err =
			    __os_malloc(dbenv, sizeof(DB_LOCK), &lockp)) != 0) {
				__dbj_throw(jenv, alloc_err, NULL, NULL, jdbenv);
				goto out2;
			}

			*lockp = lockreq[i].lock;
			*(DB_LOCK **)&jlockp = lockp;

			jlockreq = (*jenv)->GetObjectArrayElement(jenv,
			    list, i + offset);
			jlock = (*jenv)->NewObject(jenv, lock_class,
			    lock_construct, jlockp, JNI_TRUE);
			if (jlock == NULL)
				goto out2; /* An exception is pending */
			(*jenv)->SetLongField(jenv, jlock, lock_cptr_fid,
			    jlockp);
			(*jenv)->SetObjectField(jenv, jlockreq,
			    lockreq_lock_fid, jlock);
		}
	}

	/* If one of the locks was not granted, build the exception now. */
	if (err == DB_LOCK_NOTGRANTED && i < count) {
		jlockreq = (*jenv)->GetObjectArrayElement(jenv, list,
		    i + offset);
		jobj = (*jenv)->GetObjectField(jenv, jlockreq,
		    lockreq_obj_fid);
		jlock = (*jenv)->GetObjectField(jenv, jlockreq,
		    lockreq_lock_fid);
		(*jenv)->Throw(jenv,
		    (*jenv)->NewObject(jenv, lockex_class, lockex_construct,
		    (*jenv)->NewStringUTF(jenv, "DbEnv.lock_vec incomplete"),
		    lockreq[i].op, lockreq[i].mode, jobj, jlock, i, jdbenv));
	} else if (err != 0)
		__dbj_throw(jenv, err, NULL, NULL, jdbenv);

out2:	/* Free the dbts that we have locked */
	for (i = 0 ; i < (prereq - lockreq); i++) {
		if (((op = lockreq[i].op) == DB_LOCK_GET ||
		    op == DB_LOCK_PUT_OBJ) &&
		    locked_dbts[i].jarr != NULL)
			(*jenv)->ReleaseByteArrayElements(jenv,
			    locked_dbts[i].jarr, locked_dbts[i].orig_data, 0);
	}
	__os_free(dbenv, locked_dbts);
out1:	__os_free(dbenv, lockreq);
out0:	return;
}
%}

JAVA_TYPEMAP(int *envid, DbEnv.RepProcessMessage, jobject)
%typemap(in) int *envid (int id) %{
	id = (*jenv)->GetIntField(jenv, $input, rep_processmsg_envid);
	$1 = &id;
%}

%typemap(argout) int *envid %{
	(*jenv)->SetIntField(jenv, $input, rep_processmsg_envid, *$1);
%}

JAVA_TYPEMAP(void *, Object, jobject)
