dnl Process this file with autoconf to produce a configure script. -*-m4-*-
dnl Configure input file for elfutils.
dnl
dnl Copyright (C) 1996-2001, 2002 Red Hat, Inc.
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License version 2 as
dnl published by the Free Software Foundation.
dnl
dnl his program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software Foundation,
dnl Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
dnl
AC_INIT([elfutils],[0.52],[drepper@redhat.com],[elfutils])
AM_INIT_AUTOMAKE([gnits 1.6.3 dist-bzip2])
AC_COPYRIGHT([Copyright (C) 1996-2001, 2002 Red Hat, Inc.])
AC_CONFIG_SRCDIR([libelf/elf_begin.c])
AM_CONFIG_HEADER([config.h])
AC_PREREQ(2.54)			dnl Minimum Autoconf version required.
AC_CANONICAL_HOST

ALL_LINGUAS=

AC_PROG_CC
AC_PROG_CPP
AC_PROG_GCC_TRADITIONAL
AM_GNU_GETTEXT([external])
AC_LIBLTDL_CONVENIENCE
AC_SUBST(INCLTDL)
AC_SUBST(LIBLTDL)
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL
AC_CONFIG_SUBDIRS(libltdl)
AC_PROG_YACC
AM_PROG_LEX

LOCALEDIR=$datadir
AC_SUBST(LOCALEDIR)
AC_DEFINE_UNQUOTED(LOCALEDIR, "$LOCALEDIR")
AH_TEMPLATE([LOCALEDIR], [Directory to place translation files in.])

AH_BOTTOM([
/* Eventually we will allow multi-threaded applications to use the
   libraries.  Therefore we will add the necessary locking although
   the macros used expand to nothing for now.  */
#define lock_lock(lock)
#define rwlock_define(class,name) class int name
#define rwlock_init(lock)
#define rwlock_fini(lock)
#define rwlock_rdlock(lock)
#define rwlock_wrlock(lock)
#define rwlock_unlock(lock)
#define tls_key_t void *
#define key_create(keyp, freefct) ((void) (keyp), (void) (freefct), 1)
#define getspecific(key) key
#define setspecific(key,val) key = val
#define once_define(class,name) class int name
#define once_execute(name,fct) \
  do {									      \
    if (name == 0)							      \
      fct ();								      \
    name = 1;								      \
  } while (0)

/* gettext helper macro.  */
#define N_(Str) Str

/* Compiler-specific definitions.  */
#define strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));

#ifdef __i386__
# define internal_function \
  __attribute__ ((regparm (3), stdcall, visibility ("internal")))
# define internal_strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name), stdcall, visibility ("internal")));
#else
# define internal_function \
  __attribute__ ((visibility ("internal")))
# define internal_strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name), visibility ("hiden")));
#endif

#define attribute_hidden \
  __attribute__ ((visibility ("hidden")))

/* Define ALLOW_UNALIGNED if the architecture allows operations on
   unaligned memory locations.  */
#if defined __i386__ || defined __alpha__ || defined __x86_64__ || defined __ia64__
# define ALLOW_UNALIGNED	1
#else
# define ALLOW_UNALIGNED	0
#endif

#ifdef __GNUC__
# define unlikely(expr) __builtin_expect (expr, 0)
# define likely(expr) __builtin_expect (expr, 1)
#else
# define unlikely(expr) (expr)
# define likely(expr) (expr)
#endif

#if __STDC_VERSION__ >= 199901L
# define flexarr_size /* empty */
#else
# define flexarr_size 0
#endif

/* Calling conventions.  */
#ifdef __i386__
# define CALLING_CONVENTION regparm (3), stdcall
# define AND_CALLING_CONVENTION , regparm (3), stdcall
#else
# define CALLING_CONVENTION
# define AND_CALLING_CONVENTION
#endif

/* Avoid PLT entries.  */
#if defined PIC && defined __GNUC__
# define INTUSE(name) _INTUSE(name)
# define _INTUSE(name) __##name##_internal
# define INTDEF(name) _INTDEF(name)
# define _INTDEF(name) \
  extern __typeof__ (name) __##name##_internal __attribute__ ((alias (#name)));
#else
# define INTUSE(name) name
# define INTDEF(name) /* empty */
#endif
])

dnl This test must come as early as possible after the compiler configuration
dnl tests, because the choice of the file model can (in principle) affect
dnl whether functions and headers are available, whether they work, etc.
AC_SYS_LARGEFILE

dnl AC_CONFIG_FILES([Makefile
dnl 	   elfutils.spec
dnl 	   m4/Makefile
dnl 	   doc/Makefile
dnl 	   lib/Makefile
dnl 	   libelf/Makefile
dnl 	   libebl/Makefile
dnl 	   libdwarf/Makefile
dnl 	   libcpu/Makefile
dnl 	   libasm/Makefile
dnl 	   src/Makefile
dnl 	   libelf-po/Makefile.in
dnl 	   libebl-po/Makefile.in
dnl 	   libdwarf-po/Makefile.in
dnl 	   libasm-po/Makefile.in
dnl 	   po/Makefile.in
dnl 	   tests/Makefile])

AC_CONFIG_FILES([Makefile
	   elfutils.spec
	   m4/Makefile
	   doc/Makefile
	   lib/Makefile
	   libelf/Makefile
	   libebl/Makefile
	   libdwarf/Makefile
	   libelf-po/Makefile.in
	   libebl-po/Makefile.in
	   libdwarf-po/Makefile.in
	   po/Makefile.in])
AC_OUTPUT
