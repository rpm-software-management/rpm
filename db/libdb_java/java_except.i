%{
/*
 * Macros to find the Java DbEnv object for methods in various classes.
 * Note that "arg1" is from the code SWIG generates for the "this"/"self".
 */
#define	JDBENV (arg1 ? (jobject)DB_ENV_INTERNAL(arg1) : NULL)
#define	DB2JDBENV ((jobject)DB_ENV_INTERNAL(arg1->dbenv))
#define	DBC2JDBENV ((jobject)DB_ENV_INTERNAL(arg1->dbp->dbenv))
#define	TXN2JDBENV ((jobject)DB_ENV_INTERNAL(arg1->mgrp->dbenv))
%}

/* Common case exception handling */
%typemap(check) SWIGTYPE *self %{
	if ($input == 0) {
		__dbj_throw(jenv, EINVAL, "call on closed handle", NULL, NULL);
		return $null;
	}
%}

%define JAVA_EXCEPT_NONE
%exception %{ $action %}
%enddef

/* Most methods return an error code. */
%define JAVA_EXCEPT(_retcheck, _jdbenv)
%exception %{
	$action
	if (!_retcheck(result))
		__dbj_throw(jenv, result, NULL, NULL, _jdbenv);
%}
%enddef

/* For everything else, errno is set in db.i. */
%define JAVA_EXCEPT_ERRNO(_retcheck, _jdbenv)
%exception %{
	errno = 0;
	$action
	if (!_retcheck(errno))
		__dbj_throw(jenv, errno, NULL, NULL, _jdbenv);
%}
%enddef

/* And then there are these (extra) special cases. */
%exception __db_env::lock_get %{
	errno = 0;
	$action
	if (errno == DB_LOCK_NOTGRANTED) {
                (*jenv)->Throw(jenv,
                    (*jenv)->NewObject(jenv, lockex_class, lockex_construct,
                    (*jenv)->NewStringUTF(jenv, "DbEnv.lock_get not granted"),
                        DB_LOCK_GET, arg5, jarg4, NULL, -1, JDBENV));
	} else if (!DB_RETOK_STD(errno))
		__dbj_throw(jenv, errno, NULL, NULL, JDBENV);
%}

%{

static jthrowable __dbj_get_except(JNIEnv *jenv,
    int err, const char *msg, jobject obj, jobject jdbenv) {
	jobject jmsg;

	if (msg == NULL)
		msg = db_strerror(err);
	
	jmsg = (*jenv)->NewStringUTF(jenv, msg);

	switch (err) {
	case EINVAL:
		return (jthrowable)(*jenv)->NewObject(jenv,
		    illegalargex_class, illegalargex_construct, jmsg);

	case ENOENT:
		return (jthrowable)(*jenv)->NewObject(jenv,
		    filenotfoundex_class, filenotfoundex_construct, jmsg);

	case ENOMEM:
		return (jthrowable)(*jenv)->NewObject(jenv, memex_class,
		    memex_construct, jmsg, obj, err, jdbenv);

	case DB_RUNRECOVERY:
		return (jthrowable)(*jenv)->NewObject(jenv, runrecex_class,
		    runrecex_construct, jmsg, err, jdbenv);

	case DB_LOCK_DEADLOCK:
		return (jthrowable)(*jenv)->NewObject(jenv, deadex_class,
		    deadex_construct, jmsg, err, jdbenv);
	
	case DB_LOCK_NOTGRANTED:
		return (jthrowable)(*jenv)->NewObject(jenv, lockex_class,
		    lockex_construct, jmsg, 0, 0, NULL, NULL, 0, jdbenv);
	
	default:
		return (jthrowable)(*jenv)->NewObject(jenv, dbex_class,
		    dbex_construct, jmsg, err, jdbenv);
	}
}

static int __dbj_throw(JNIEnv *jenv, int err, const char *msg, jobject obj, jobject jdbenv)
{
	jthrowable t;

	/* If an exception is pending, ignore requests to throw a new one. */
	if ((*jenv)->ExceptionOccurred(jenv) == NULL) {
		if ((t = __dbj_get_except(jenv, err, msg, obj, jdbenv)) == NULL) {
			/*
			 * This is a problem - something went wrong creating an
			 * exception.  We have to assume there is an exception
			 * created by the JVM that is pending as a result
			 * (e.g., OutOfMemoryError), but we don't want to lose
			 * this error, so we just call __db_err here.
			 */
			if (msg == NULL)
				msg = db_strerror(err);
	
			 __db_err(NULL, "Couldn't create exception for: '%s'",
			     msg);
		} else
			(*jenv)->Throw(jenv, t);
	}
	
	return (err);
}
%}
