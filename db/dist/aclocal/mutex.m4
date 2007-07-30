# $Id: mutex.m4,v 12.26 2007/05/18 15:08:50 bostic Exp $

# POSIX pthreads tests: inter-process safe and intra-process only.
AC_DEFUN(AM_PTHREADS_SHARED, [
AC_TRY_RUN([
#include <pthread.h>
main() {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_condattr_t condattr;
	pthread_mutexattr_t mutexattr;
	exit (
	pthread_condattr_init(&condattr) ||
	pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED) ||
	pthread_mutexattr_init(&mutexattr) ||
	pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED) ||
	pthread_cond_init(&cond, &condattr) ||
	pthread_mutex_init(&mutex, &mutexattr) ||
	pthread_mutex_lock(&mutex) ||
	pthread_mutex_unlock(&mutex) ||
	pthread_mutex_destroy(&mutex) ||
	pthread_cond_destroy(&cond) ||
	pthread_condattr_destroy(&condattr) ||
	pthread_mutexattr_destroy(&mutexattr));
}], [db_cv_mutex="$1"],,
AC_TRY_LINK([
#include <pthread.h>],[
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_condattr_t condattr;
	pthread_mutexattr_t mutexattr;
	exit (
	pthread_condattr_init(&condattr) ||
	pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED) ||
	pthread_mutexattr_init(&mutexattr) ||
	pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED) ||
	pthread_cond_init(&cond, &condattr) ||
	pthread_mutex_init(&mutex, &mutexattr) ||
	pthread_mutex_lock(&mutex) ||
	pthread_mutex_unlock(&mutex) ||
	pthread_mutex_destroy(&mutex) ||
	pthread_cond_destroy(&cond) ||
	pthread_condattr_destroy(&condattr) ||
	pthread_mutexattr_destroy(&mutexattr));
], [db_cv_mutex="$1"]))])
AC_DEFUN(AM_PTHREADS_PRIVATE, [
AC_TRY_RUN([
#include <pthread.h>
main() {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_condattr_t condattr;
	pthread_mutexattr_t mutexattr;
	exit (
	pthread_condattr_init(&condattr) ||
	pthread_mutexattr_init(&mutexattr) ||
	pthread_cond_init(&cond, &condattr) ||
	pthread_mutex_init(&mutex, &mutexattr) ||
	pthread_mutex_lock(&mutex) ||
	pthread_mutex_unlock(&mutex) ||
	pthread_mutex_destroy(&mutex) ||
	pthread_cond_destroy(&cond) ||
	pthread_condattr_destroy(&condattr) ||
	pthread_mutexattr_destroy(&mutexattr));
}], [db_cv_mutex="$1"],,
AC_TRY_LINK([
#include <pthread.h>],[
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_condattr_t condattr;
	pthread_mutexattr_t mutexattr;
	exit (
	pthread_condattr_init(&condattr) ||
	pthread_mutexattr_init(&mutexattr) ||
	pthread_cond_init(&cond, &condattr) ||
	pthread_mutex_init(&mutex, &mutexattr) ||
	pthread_mutex_lock(&mutex) ||
	pthread_mutex_unlock(&mutex) ||
	pthread_mutex_destroy(&mutex) ||
	pthread_cond_destroy(&cond) ||
	pthread_condattr_destroy(&condattr) ||
	pthread_mutexattr_destroy(&mutexattr));
], [db_cv_mutex="$1"]))])

# Figure out mutexes for this compiler/architecture.
#
# There are 3 mutex groups in BDB: pthreads-style, test-and-set, or a hybrid
# combination of the two.   We first test for the pthreads-style mutex, and
# then for a test-and-set mutex.
AC_DEFUN(AM_DEFINE_MUTEXES, [

# Mutexes we don't test for, but want the #defines to exist for other ports.
AH_TEMPLATE(HAVE_MUTEX_VMS, [Define to 1 to use VMS mutexes.])
AH_TEMPLATE(HAVE_MUTEX_VXWORKS, [Define to 1 to use VxWorks mutexes.])

AC_CACHE_CHECK([for mutexes], db_cv_mutex, [

orig_libs=$LIBS

db_cv_mutex=no

# Mutexes can be disabled.
if test "$db_cv_build_mutexsupport" = no; then
	db_cv_mutex=disabled;
fi

# User-specified Win32 mutexes (MinGW build)
if test "$db_cv_mingw" = yes; then
	db_cv_mutex=win32/gcc
fi

if test "$db_cv_mutex" = no; then
	# User-specified POSIX or UI mutexes.
	#
	# There are two different reasons to specify mutexes: First, the
	# application is already using one type of mutex and doesn't want
	# to mix-and-match (for example, on Solaris, which has POSIX, UI
	# and LWP mutexes).  Second, the application's POSIX pthreads
	# mutexes don't support inter-process locking, but the application
	# wants to use them anyway (for example, some Linux and *BSD systems).
	#
	# Test for LWP threads before testing for UI/POSIX threads, we prefer
	# them on Solaris, for two reasons: a bug in SunOS 5.7 causes
	# applications to get pwrite, not pwrite64, if they load the C library
	# before the appropriate threads library, e.g., tclsh using dlopen to
	# load the DB library.  Second, LWP mutexes are faster than POSIX
	# pthread mutexes by some amount.
	#
	# Otherwise, test for POSIX threads before UI threads.  There are Linux
	# systems that support a UI compatibility mode, and applications are
	# more likely to be written for POSIX threads than UI threads.

	if test "$db_cv_posixmutexes" = yes; then
		db_cv_mutex=posix_only;
	fi
	if test "$db_cv_uimutexes" = yes; then
		db_cv_mutex=ui_only;
	fi

	# LWP threads: _lwp_XXX
	if test "$db_cv_mutex" = no; then
	AC_TRY_LINK([
	#include <synch.h>],[
		static lwp_mutex_t mi = SHAREDMUTEX;
		static lwp_cond_t ci = SHAREDCV;
		lwp_mutex_t mutex = mi;
		lwp_cond_t cond = ci;
		exit (
		_lwp_mutex_lock(&mutex) ||
		_lwp_mutex_unlock(&mutex));
	], [db_cv_mutex=Solaris/lwp])
	fi

	# POSIX.1 pthreads: pthread_XXX
	#
	# If the user specified we use POSIX pthreads mutexes, and we fail to
	# find the full interface, try and configure for just intra-process
	# support.
	if test "$db_cv_mutex" = no -o "$db_cv_mutex" = posix_only; then
		LIBS="$LIBS -lpthread"
		AM_PTHREADS_SHARED(POSIX/pthreads/library)
		LIBS="$orig_libs"
	fi
	if test "$db_cv_mutex" = no -o "$db_cv_mutex" = posix_only; then
		AM_PTHREADS_SHARED(POSIX/pthreads)
	fi
	if test "$db_cv_mutex" = posix_only; then
		AM_PTHREADS_PRIVATE(POSIX/pthreads/private)
	fi
	if test "$db_cv_mutex" = posix_only; then
		LIBS="$LIBS -lpthread"
		AM_PTHREADS_PRIVATE(POSIX/pthreads/library/private)
		LIBS="$orig_libs"
	fi
	if test "$db_cv_mutex" = posix_only; then
		AC_MSG_ERROR([unable to find POSIX 1003.1 mutex interfaces])
	fi

	# UI threads: thr_XXX
	if test "$db_cv_mutex" = no -o "$db_cv_mutex" = ui_only; then
	LIBS="$LIBS -lthread"
	AC_TRY_LINK([
	#include <thread.h>
	#include <synch.h>],[
		mutex_t mutex;
		cond_t cond;
		int type = USYNC_PROCESS;
		exit (
		mutex_init(&mutex, type, NULL) ||
		cond_init(&cond, type, NULL) ||
		mutex_lock(&mutex) ||
		mutex_unlock(&mutex));
	], [db_cv_mutex=UI/threads/library])
	LIBS="$orig_libs"
	fi
	if test "$db_cv_mutex" = no -o "$db_cv_mutex" = ui_only; then
	AC_TRY_LINK([
	#include <thread.h>
	#include <synch.h>],[
		mutex_t mutex;
		cond_t cond;
		int type = USYNC_PROCESS;
		exit (
		mutex_init(&mutex, type, NULL) ||
		cond_init(&cond, type, NULL) ||
		mutex_lock(&mutex) ||
		mutex_unlock(&mutex));
	], [db_cv_mutex=UI/threads])
	fi
	if test "$db_cv_mutex" = ui_only; then
		AC_MSG_ERROR([unable to find UI mutex interfaces])
	fi

	# We're done testing for pthreads-style mutexes.  Next, check for
	# test-and-set mutexes.  Check first for hybrid implementations,
	# because we check for them even if we've already found a
	# pthreads-style mutex and they're the most common architectures
	# anyway.
	#
	# x86/gcc: FreeBSD, NetBSD, BSD/OS, Linux
	AC_TRY_COMPILE(,[
	#if (defined(i386) || defined(__i386__)) && defined(__GNUC__)
		exit(0);
	#else
		FAIL TO COMPILE/LINK
	#endif
	], [db_cv_mutex="$db_cv_mutex/x86/gcc-assembly"])

	# x86_64/gcc: FreeBSD, NetBSD, BSD/OS, Linux
	AC_TRY_COMPILE(,[
	#if (defined(x86_64) || defined(__x86_64__)) && defined(__GNUC__)
		exit(0);
	#else
		FAIL TO COMPILE/LINK
	#endif
	], [db_cv_mutex="$db_cv_mutex/x86_64/gcc-assembly"])

	# Solaris is one of the systems where we can configure hybrid mutexes.
	# However, we require the membar_enter function for that, and only newer
	# Solaris releases have it.  Check to see if we can configure hybrids.
	AC_TRY_LINK([
	#include <sys/atomic.h>
	#include <sys/machlock.h>],[
		typedef lock_t tsl_t;
		lock_t x;
		_lock_try(&x);
		_lock_clear(&x);
		membar_enter();
	], [db_cv_mutex="$db_cv_mutex/Solaris/_lock_try/membar"])

	# Sparc/gcc: SunOS, Solaris
	AC_TRY_COMPILE(,[
	#if defined(__sparc__) && defined(__GNUC__)
		exit(0);
	#else
		FAIL TO COMPILE/LINK
	#endif
	], [db_cv_mutex="$db_cv_mutex/Sparc/gcc-assembly"])

	# We're done testing for any hybrid mutex implementations.  If we did
	# not find a pthreads-style mutex, but did find a test-and-set mutex,
	# we set db_cv_mutex to "no/XXX" -- clean that up.
	db_cv_mutex=`echo $db_cv_mutex | sed 's/^no\///'`
fi

# If we still don't have a mutex implementation yet, continue testing for a
# test-and-set mutex implementation.

# _lock_try/_lock_clear: Solaris
# On Solaris systems without other mutex interfaces, DB uses the undocumented
# _lock_try _lock_clear function calls instead of either the sema_trywait(3T)
# or sema_wait(3T) function calls.  This is because of problems in those
# interfaces in some releases of the Solaris C library.
if test "$db_cv_mutex" = no; then
AC_TRY_LINK([
#include <sys/atomic.h>
#include <sys/machlock.h>],[
	typedef lock_t tsl_t;
	lock_t x;
	_lock_try(&x);
	_lock_clear(&x);
], [db_cv_mutex=Solaris/_lock_try])
fi

# msemaphore: HPPA only
# Try HPPA before general msem test, it needs special alignment.
if test "$db_cv_mutex" = no; then
AC_TRY_LINK([
#include <sys/mman.h>],[
#if defined(__hppa)
	typedef msemaphore tsl_t;
	msemaphore x;
	msem_init(&x, 0);
	msem_lock(&x, 0);
	msem_unlock(&x, 0);
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=HP/msem_init])
fi

# msemaphore: AIX, OSF/1
if test "$db_cv_mutex" = no; then
AC_TRY_LINK([
#include <sys/types.h>
#include <sys/mman.h>],[
	typedef msemaphore tsl_t;
	msemaphore x;
	msem_init(&x, 0);
	msem_lock(&x, 0);
	msem_unlock(&x, 0);
	exit(0);
], [db_cv_mutex=UNIX/msem_init])
fi

# ReliantUNIX
if test "$db_cv_mutex" = no; then
LIBS="$LIBS -lmproc"
AC_TRY_LINK([
#include <ulocks.h>],[
	typedef spinlock_t tsl_t;
	spinlock_t x;
	initspin(&x, 1);
	cspinlock(&x);
	spinunlock(&x);
], [db_cv_mutex=ReliantUNIX/initspin])
LIBS="$orig_libs"
fi

# SCO: UnixWare has threads in libthread, but OpenServer doesn't.
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if defined(__USLC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=SCO/x86/cc-assembly])
fi

# abilock_t: SGI
if test "$db_cv_mutex" = no; then
AC_TRY_LINK([
#include <abi_mutex.h>],[
	typedef abilock_t tsl_t;
	abilock_t x;
	init_lock(&x);
	acquire_lock(&x);
	release_lock(&x);
], [db_cv_mutex=SGI/init_lock])
fi

# sema_t: Solaris
# The sema_XXX calls do not work on Solaris 5.5.  I see no reason to ever
# turn this test on, unless we find some other platform that uses the old
# POSIX.1 interfaces.
if test "$db_cv_mutex" = DOESNT_WORK; then
AC_TRY_LINK([
#include <synch.h>],[
	typedef sema_t tsl_t;
	sema_t x;
	sema_init(&x, 1, USYNC_PROCESS, NULL);
	sema_wait(&x);
	sema_post(&x);
], [db_cv_mutex=UNIX/sema_init])
fi

# _check_lock/_clear_lock: AIX
if test "$db_cv_mutex" = no; then
AC_TRY_LINK([
#include <sys/atomic_op.h>],[
	int x;
	_check_lock(&x,0,1);
	_clear_lock(&x,0);
], [db_cv_mutex=AIX/_check_lock])
fi

# _spin_lock_try/_spin_unlock: Apple/Darwin
if test "$db_cv_mutex" = no; then
AC_TRY_LINK(,[
	int x;
	_spin_lock_try(&x);
	_spin_unlock(&x);
], [db_cv_mutex=Darwin/_spin_lock_try])
fi

# Tru64/cc
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if defined(__alpha) && defined(__DECC)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=Tru64/cc-assembly])
fi

# Alpha/gcc
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if defined(__alpha) && defined(__GNUC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=ALPHA/gcc-assembly])
fi

# ARM/gcc: Linux
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if defined(__arm__) && defined(__GNUC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=ARM/gcc-assembly])
fi

# MIPS/gcc: Linux
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if (defined(__mips) || defined(__mips__)) && defined(__GNUC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=MIPS/gcc-assembly])
fi

# PaRisc/gcc: HP/UX
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if (defined(__hppa) || defined(__hppa__)) && defined(__GNUC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=HPPA/gcc-assembly])
fi

# PPC/gcc:
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if (defined(__powerpc__) || defined(__ppc__)) && defined(__GNUC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=PPC/gcc-assembly])
fi

# 68K/gcc: SunOS
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if (defined(mc68020) || defined(sun3)) && defined(__GNUC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=68K/gcc-assembly])
fi

# S390/cc: IBM OS/390 Unix
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if defined(__MVS__) && defined(__IBMC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=S390/cc-assembly])
fi

# S390/gcc: Linux
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if defined(__s390__) && defined(__GNUC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=S390/gcc-assembly])
fi

# ia64/gcc: Linux
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if defined(__ia64) && defined(__GNUC__)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=ia64/gcc-assembly])
fi

# uts/cc: UTS
if test "$db_cv_mutex" = no; then
AC_TRY_COMPILE(,[
#if defined(_UTS)
	exit(0);
#else
	FAIL TO COMPILE/LINK
#endif
], [db_cv_mutex=UTS/cc-assembly])
fi

# UNIX fcntl system call mutexes.
if test "$db_cv_mutex" = no; then
	db_cv_mutex=UNIX/fcntl
AC_TRY_LINK([
#include <fcntl.h>],[
	struct flock l;
	l.l_whence = SEEK_SET;
	l.l_start = 10;
	l.l_len = 1;
	l.l_type = F_WRLCK;
	fcntl(0, F_SETLK, &l);
], [db_cv_mutex=UNIX/fcntl])
fi
])

# Mutexes may not have been found, or may have been disabled.
case "$db_cv_mutex" in
disabled)		;;
no)
			AC_MSG_ERROR([Unable to find a mutex implementation]);;
*)			AC_DEFINE(HAVE_MUTEX_SUPPORT)
			AH_TEMPLATE(HAVE_MUTEX_SUPPORT,
			    [Define to 1 if the Berkeley DB library should support mutexes.])
esac

# Configure a pthreads-style mutex implementation.
hybrid=pthread
case "$db_cv_mutex" in
POSIX/pthreads*)	ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_PTHREADS)
			AH_TEMPLATE(HAVE_MUTEX_PTHREADS,
			    [Define to 1 to use POSIX 1003.1 pthread_XXX mutexes.]);;
POSIX/pthreads/private*)ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_PTHREADS)
			AC_DEFINE(HAVE_MUTEX_THREAD_ONLY)
			AH_TEMPLATE(HAVE_MUTEX_THREAD_ONLY,
			    [Define to 1 to configure mutexes intra-process only.]);;
POSIX/pthreads/library*)ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_PTHREADS);;
POSIX/pthreads/library/private*)
			ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_PTHREADS)
			AC_DEFINE(HAVE_MUTEX_THREAD_ONLY);;
Solaris/lwp*)		ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SOLARIS_LWP)
			AH_TEMPLATE(HAVE_MUTEX_SOLARIS_LWP,
			    [Define to 1 to use the Solaris lwp threads mutexes.]);;
UI/threads*)		ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_UI_THREADS)
			AH_TEMPLATE(HAVE_MUTEX_UI_THREADS,
			    [Define to 1 to use the UNIX International mutexes.]);;
UI/threads/library*)	ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_UI_THREADS);;
*)			hybrid=no;;
esac

# Configure a test-and-set mutex implementation.
case "$db_cv_mutex" in
68K/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_68K_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_68K_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and 68K assembly language mutexes.]);;
AIX/_check_lock)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_AIX_CHECK_LOCK)
			AH_TEMPLATE(HAVE_MUTEX_AIX_CHECK_LOCK,
			    [Define to 1 to use the AIX _check_lock mutexes.]);;
Darwin/_spin_lock_try)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_DARWIN_SPIN_LOCK_TRY)
			AH_TEMPLATE(HAVE_MUTEX_DARWIN_SPIN_LOCK_TRY,
			    [Define to 1 to use the Apple/Darwin _spin_lock_try mutexes.]);;
ALPHA/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_ALPHA_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_ALPHA_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and Alpha assembly language mutexes.]);;
ARM/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_ARM_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_ARM_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and ARM assembly language mutexes.]);;
HP/msem_init)		ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_HPPA_MSEM_INIT)
			AH_TEMPLATE(HAVE_MUTEX_HPPA_MSEM_INIT,
			    [Define to 1 to use the msem_XXX mutexes on HP-UX.]);;
HPPA/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_HPPA_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_HPPA_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and PaRisc assembly language mutexes.]);;
ia64/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_IA64_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_IA64_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and IA64 assembly language mutexes.]);;
MIPS/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_MIPS_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_MIPS_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and MIPS assembly language mutexes.]);;
PPC/gcc-assembly)
			ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_PPC_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_PPC_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and PowerPC assembly language mutexes.]);;
ReliantUNIX/initspin)	LIBSO_LIBS="$LIBSO_LIBS -lmproc"
			ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_RELIANTUNIX_INITSPIN)
			AH_TEMPLATE(HAVE_MUTEX_RELIANTUNIX_INITSPIN,
			    [Define to 1 to use Reliant UNIX initspin mutexes.]);;
S390/cc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_S390_CC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_S390_CC_ASSEMBLY,
			    [Define to 1 to use the IBM C compiler and S/390 assembly language mutexes.]);;
S390/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_S390_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_S390_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and S/390 assembly language mutexes.]);;
SCO/x86/cc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SCO_X86_CC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_SCO_X86_CC_ASSEMBLY,
			    [Define to 1 to use the SCO compiler and x86 assembly language mutexes.]);;
SGI/init_lock)		ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SGI_INIT_LOCK)
			AH_TEMPLATE(HAVE_MUTEX_SGI_INIT_LOCK,
			    [Define to 1 to use the SGI XXX_lock mutexes.]);;
Solaris/_lock_try)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SOLARIS_LOCK_TRY)
			AH_TEMPLATE(HAVE_MUTEX_SOLARIS_LOCK_TRY,
			    [Define to 1 to use the Solaris _lock_XXX mutexes.]);;
*Solaris/_lock_try/membar)
			hybrid="$hybrid/tas"
			ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SOLARIS_LOCK_TRY)
			AH_TEMPLATE(HAVE_MUTEX_SOLARIS_LOCK_TRY,
			    [Define to 1 to use the Solaris _lock_XXX mutexes.]);;
*Sparc/gcc-assembly)	hybrid="$hybrid/tas"
			ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SPARC_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_SPARC_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and Sparc assembly language mutexes.]);;
Tru64/cc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_TRU64_CC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_TRU64_CC_ASSEMBLY,
			    [Define to 1 to use the CC compiler and Tru64 assembly language mutexes.]);;
UNIX/msem_init)		ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_MSEM_INIT)
			AH_TEMPLATE(HAVE_MUTEX_MSEM_INIT,
			    [Define to 1 to use the msem_XXX mutexes on systems other than HP-UX.]);;
UNIX/sema_init)		ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SEMA_INIT)
			AH_TEMPLATE(HAVE_MUTEX_SEMA_INIT,
			    [Define to 1 to use the obsolete POSIX 1003.1 sema_XXX mutexes.]);;
UTS/cc-assembly)	ADDITIONAL_OBJS="uts4.cc${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_UTS_CC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_UTS_CC_ASSEMBLY,
			    [Define to 1 to use the UTS compiler and assembly language mutexes.]);;
*x86/gcc-assembly)	hybrid="$hybrid/tas"
			ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_X86_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_X86_GCC_ASSEMBLY,
			    [Define to 1 to use the GCC compiler and 32-bit x86 assembly language mutexes.]);;
*x86_64/gcc-assembly)	hybrid="$hybrid/tas"
			ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_X86_64_GCC_ASSEMBLY)
			AH_TEMPLATE(HAVE_MUTEX_X86_64_GCC_ASSEMBLY,
			[Define to 1 to use the GCC compiler and 64-bit x86 assembly language mutexes.]);;
esac

# Configure the remaining special cases.
case "$db_cv_mutex" in
UNIX/fcntl)		AC_MSG_WARN(
			    [NO FAST MUTEXES FOUND FOR THIS COMPILER/ARCHITECTURE.])
			ADDITIONAL_OBJS="mut_fcntl${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_FCNTL)
			AH_TEMPLATE(HAVE_MUTEX_FCNTL,
			    [Define to 1 to use the UNIX fcntl system call mutexes.]);;
win32)			ADDITIONAL_OBJS="mut_win32${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_WIN32)
			AH_TEMPLATE(HAVE_MUTEX_WIN32, [Define to 1 to use the MSVC compiler and Windows mutexes.]);;
win32/gcc)		ADDITIONAL_OBJS="mut_win32${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_WIN32_GCC)
			AH_TEMPLATE(HAVE_MUTEX_WIN32_GCC, [Define to 1 to use the GCC compiler and Windows mutexes.]);;
esac

# We may have found both a pthreads-style mutex implementation as well as a
# test-and-set, in which case configure for the hybrid.
if test "$hybrid" = pthread/tas; then
	AC_DEFINE(HAVE_MUTEX_HYBRID)
	AH_TEMPLATE(HAVE_MUTEX_HYBRID,
	    [Define to 1 to use test-and-set mutexes with blocking mutexes.])
fi

# The mutex selection may require specific declarations -- we fill in most of
# them above, but here are the common ones.
#
# The mutex selection may tell us what kind of thread package we're using,
# which we use to figure out the thread type.
#
# If we're configured for the POSIX pthread API, then force the thread ID type
# and include function, regardless of the mutex selection.  Ditto for the
# (default) Solaris lwp mutexes, because they don't have a way to return the
# thread ID.
#
# Try and link with a threads library if possible.  The problem is the Solaris
# C library has UI/POSIX interface stubs, but they're broken, configuring them
# for inter-process mutexes doesn't return an error, but it doesn't work either.
# For that reason always add -lpthread if we're using pthread calls or mutexes
# and there's a pthread library.
#
# We can't depend on any specific call existing (pthread_create, for example),
# as it may be #defined in an include file -- OSF/1 (Tru64) has this problem.

AC_SUBST(thread_h_decl)
AC_SUBST(db_threadid_t_decl)
db_threadid_t_decl=notset

case "$db_cv_mutex" in
POSIX/pthread*|Solaris/lwp*)
	thread_h_decl="#include <pthread.h>"
	db_threadid_t_decl="typedef pthread_t db_threadid_t;"
	AC_HAVE_LIBRARY(pthread, LIBSO_LIBS="$LIBSO_LIBS -lpthread");;
UI/threads*)
	thread_h_decl="#include <thread.h>"
	db_threadid_t_decl="typedef thread_t db_threadid_t;"
	AC_HAVE_LIBRARY(thread, LIBSO_LIBS="$LIBSO_LIBS -lthread");;
*)
	if test "$db_cv_pthread_api" = yes; then
		thread_h_decl="#include <pthread.h>"
		db_threadid_t_decl="typedef pthread_t db_threadid_t;"
		AC_HAVE_LIBRARY(pthread, LIBSO_LIBS="$LIBSO_LIBS -lpthread")
	fi;;
esac

# We need to know if the thread ID type will fit into an integral type and we
# can compare it for equality and generally treat it like an int, or if it's a
# non-integral type and we have to treat it like a structure or other untyped
# block of bytes.  For example, MVS typedef's pthread_t to a structure.
AH_TEMPLATE(HAVE_SIMPLE_THREAD_TYPE,
    [Define to 1 if thread identifier type db_threadid_t is integral.])
if test "$db_threadid_t_decl" = notset; then
	db_threadid_t_decl="typedef uintmax_t db_threadid_t;"
	AC_DEFINE(HAVE_SIMPLE_THREAD_TYPE)
else
	AC_TRY_COMPILE(
	#include <sys/types.h>
	$thread_h_decl, [
	$db_threadid_t_decl
	db_threadid_t a;
	a = 0;
	], AC_DEFINE(HAVE_SIMPLE_THREAD_TYPE))
fi

# If we're building replication and configured with POSIX pthreads or were
# told to build using the POSIX API, build the replication manager framework.
db_cv_build_replication_mgr=no
if test "$db_cv_build_replication" = yes; then
	AH_TEMPLATE(HAVE_REPLICATION_THREADS,
	    [Define to 1 if building the Berkeley DB replication framework.])
	case "$db_cv_mutex" in
	POSIX/pthread*|Solaris/lwp)
		db_cv_build_replication_mgr=yes;;
	esac
	if test "$db_cv_pthread_api" = yes; then
		db_cv_build_replication_mgr=yes
	fi

	if test "$db_cv_build_replication_mgr" = yes; then
		AC_DEFINE(HAVE_REPLICATION_THREADS)

		# Solaris requires the socket and nsl libraries to build the
		# replication manager.  Don't add nsl regardless of the OS,
		# it causes RPC to fail on AIX 4.3.3.
		case "$host_os" in
		solaris*)
			AC_HAVE_LIBRARY(nsl, LIBSO_LIBS="$LIBSO_LIBS -lnsl")
			AC_HAVE_LIBRARY(socket,
			    LIBSO_LIBS="$LIBSO_LIBS -lsocket");;
		esac
	fi
fi

# There are 3 classes of mutexes:
#
# 1: Mutexes requiring no cleanup, for example, test-and-set mutexes.
# 2: Mutexes that must be destroyed, but which don't hold permanent system
#    resources, for example, pthread mutexes on MVS aka OS/390 aka z/OS.
# 3: Mutexes that must be destroyed, even after the process is gone, for
#    example, pthread mutexes on QNX and binary semaphores on VxWorks.
#
# DB cannot currently distinguish between #2 and #3 because DB does not know
# if the application is running environment recovery as part of startup and
# does not need to do cleanup, or if the environment is being removed and/or
# recovered in a loop in the application, and so does need to clean up.  If
# we get it wrong, we're going to call the mutex destroy routine on a random
# piece of memory, which usually works, but just might drop core.  For now,
# we group #2 and #3 into the HAVE_MUTEX_SYSTEM_RESOURCES define, until we
# have a better solution or reason to solve this in a general way -- so far,
# the places we've needed to handle this are few.
AH_TEMPLATE(HAVE_MUTEX_SYSTEM_RESOURCES,
    [Define to 1 if mutexes hold system resources.])

case "$host_os$db_cv_mutex" in
*qnx*POSIX/pthread*|openedition*POSIX/pthread*)
	AC_DEFINE(HAVE_MUTEX_SYSTEM_RESOURCES);;
esac])
