#
# Copyright (c) 2004
#	Sleepycat Software.  All rights reserved.
#
# http://www.apache.org/licenses/LICENSE-2.0.txt
#

dnl $Id: config.m4,v 1.1 2004/10/05 14:45:58 bostic Exp $
dnl config.m4 for extension db4

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

PHP_ARG_WITH(db4, whether to enable db4 support,
[  --enable-db4           Enable db4 support])

PHP_ARG_WITH(mod_db4, whether to link against mod_db4 or db4,
[  --with-mod_db4         Enable mod_db4 support])

if test "$PHP_DB4" != "no"; then
  if test "$PHP_DB4" != "no"; then
    for i in $withval /usr/local/BerkeleyDB.4.2 /usr/local/BerkeleyDB.4.1 /usr/local/BerkeleyDB.4.0 /usr/local /usr; do
      if test -f "$i/db4/db.h"; then
        THIS_PREFIX=$i
        THIS_INCLUDE=$i/db4/db.h
        break
      elif test -f "$i/include/db4/db.h"; then
        THIS_PREFIX=$i
        THIS_INCLUDE=$i/include/db4/db.h
        break
      elif test -f "$i/include/db/db4.h"; then
        THIS_PREFIX=$i
        THIS_INCLUDE=$i/include/db/db4.h
        break
      elif test -f "$i/include/db4.h"; then
        THIS_PREFIX=$i
        THIS_INCLUDE=$i/include/db4.h
        break
      elif test -f "$i/include/db.h"; then
        THIS_PREFIX=$i
        THIS_INCLUDE=$i/include/db.h
        break
      fi
    done
    PHP_ADD_INCLUDE(THIS_INCLUDE)
    PHP_ADD_LIBRARY_WITH_PATH(db-4.2, THIS_PREFIX, DB4_SHARED_LIBADD)
  fi 
  if test "$PHP_MOD_DB4" != "no" && test "$PHP_MOD_DB4" != "yes"; then
    PHP_ADD_INCLUDE("$PHP_MOD_DB4")
    AC_DEFINE(HAVE_MOD_DB4, 1, [Whether you have mod_db4])
  elif test "$PHP_MOD_DB4" = "no"; then
    PHP_ADD_LIBRARY(db-4.2,, DB4_SHARED_LIBADD)
  else 
    AC_MSG_RESULT([no])
  fi
  PHP_NEW_EXTENSION(db4, db4.c, $ext_shared)
  PHP_SUBST(DB4_SHARED_LIBADD)
fi
