/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */
^L

/* Define to the name of the distribution.  */
#undef PACKAGE

/* Define to the version of the distribution.  */
#undef VERSION

/* Define to 1 if ANSI function prototypes are usable.  */
#undef PROTOTYPES

/* Define to 1 if NLS is requested.  */
#undef ENABLE_NLS

/* Define as 1 if you have catgets and don't want to use GNU gettext.  */
#undef HAVE_CATGETS

/* Define as 1 if you have gettext and don't want to use GNU gettext.  */
#undef HAVE_GETTEXT

/* Define to 1 if you have the stpcpy function.  */
#undef HAVE_STPCPY

/* Define if your locale.h file contains LC_MESSAGES.  */
#undef HAVE_LC_MESSAGES

/* Define as 1 if you have mntctl() (only aix?) */
#undef HAVE_MNTCTL

/* Define as 1 if <netdb.h> defines h_errno */
#undef HAVE_HERRNO

/* Define as 1 if <sys/stat.h> defines S_ISLNK */
#undef HAVE_S_ISLNK

/* Define as 1 if <sys/stat.h> defines S_IFSOCK */
#undef HAVE_S_IFSOCK

/* Define as 1 if <sys/stat.h> defines S_ISSOCK */
#undef HAVE_S_ISSOCK

/* Define as 1 if we need timezone */
#undef NEED_TIMEZONE

/* Define as 1 if we need myrealloc */
#undef NEED_MYREALLOC

/* Define as one if we need to include <strings.h> (along with <string.h>) */
#undef NEED_STRINGS_H

/* Define as 1 if you have getmntinfo_r() (only osf?) */
#undef HAVE_GETMNTINFO_R

/* Define as 1 if you have "struct mnttab" (only sco?) */
#undef HAVE_STRUCT_MNTTAB

/* Define as 1 if you have lchown() */
#undef HAVE_LCHOWN

/* Define as 1 if chown() follows symlinks and you don't have lchown() */
#undef CHOWN_FOLLOWS_SYMLINK

/* Define if the patch call you'll be using is 2.1 or older */
#undef HAVE_OLDPATCH_21

/* Define as 1 if your zlib has gzseek() */
#undef HAVE_GZSEEK

/* Define to the full path name of the bzip2 library (libbz2.a) */
#undef BZIP2LIB

/* Define as 1 if you bzip2 1.0 */
#undef HAVE_BZ2_1_0

/* A full path to a program, possibly with arguments, that will create a
   directory and all necessary parent directories, ala `mkdir -p'        */
#undef MKDIR_P

/* Full path to rpm locale directory (usually /usr/share/locale) */
#undef LOCALEDIR

/* Full path to rpm global configuration directory (usually /usr/lib/rpm) */
#undef RPMCONFIGDIR

/* Full path to rpm system configuration directory (usually /etc/rpm) */
#undef SYSCONFIGDIR

/* Full path to find-provides script (usually /usr/lib/rpm/find-provides) */
#undef FINDPROVIDES

/* Full path to find-requires script (usually /usr/lib/rpm/find-requires) */
#undef FINDREQUIRES

/* Full path to rpmpopt configuration file (usually /usr/lib/rpm/rpmpopt) */
#undef LIBRPMALIAS_FILENAME

/* Full path to rpmrc configuration file (usually /usr/lib/rpm/rpmrc) */
#undef LIBRPMRC_FILENAME

/* Full path to macros configuration file (usually /usr/lib/rpm/macros) */
#undef MACROFILES

/* statfs in <sys/statvfs.h> (for solaris 2.6+ systems) */
#undef STATFS_IN_SYS_STATVFS

/* statfs in <sys/vfs.h> (for linux systems) */
#undef STATFS_IN_SYS_VFS

/* statfs in <sys/mount.h> (for Digital Unix 4.0D systems) */
#undef STATFS_IN_SYS_MOUNT

/* statfs in <sys/statfs.h> (for Irix 6.4 systems) */
#undef STATFS_IN_SYS_STATFS

/* define if struct statfs has the f_bavail member */
#undef STATFS_HAS_F_BAVAIL

/* define if the statfs() call takes 4 arguments */
#undef STAT_STATFS4

/* Absolute path to rpm top_sourcedir. */
#undef TOP_SOURCEDIR

/* define if support rpm-1.0 packages is desired */
#undef ENABLE_V1_PACKAGES

/* define if experimental support rpm-4.0 packages is desired */
#undef ENABLE_V5_PACKAGES

/* Use the included glob.c? */
#undef USE_GNU_GLOB

/* Use the Berkeley db3 API? */
#undef USE_DB3

/* Use the Berkeley db2 API? */
#undef USE_DB2

/* Use the Berkeley db1 retrofit to db2/db3 API? */
#undef USE_DB1

/* Use the Berkeley db1 API from glibc? */
#undef USE_DB0

^L
/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */

