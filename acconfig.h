/* Define as 1 if you have inet_aton() */
#undef HAVE_INET_ATON

/* Define as 1 if you have mntctl() (only aix?) */
#undef HAVE_MNTCTL

/* Define as 1 if your system provides realpath() */
#undef HAVE_REALPATH

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

/* A full path to a program, possibly with arguments, that will create a
   directory and all necessary parent directories, ala `mkdir -p'        */
#undef MKDIR_P

/* define this to be whatever root's primary group is, in double quotes */
#undef ROOT_GROUP
