/**
 * \file lib/stubs.c
 */

/* XXX Portable shared libraries require rpmlib to contain these functions. */

#include "system.h"

#if !defined(HAVE_BASENAME)
#include "misc/basename.c"
#endif

#if !defined(HAVE_GETCWD)
#include "misc/getcwd.c"
#endif

#if !defined(HAVE_GETWD)
#include "misc/getwd.c"
#endif

#if !defined(HAVE_PUTENV)
#include "misc/putenv.c"
#endif

#if !defined(HAVE_REALPATH)
#include "misc/realpath.c"
#endif

#if !defined(HAVE_SETENV)
#include "misc/setenv.c"
#endif

#if !defined(HAVE_STPCPY)
#include "misc/stpcpy.c"
#endif

#if !defined(HAVE_STPNCPY)
#include "misc/stpncpy.c"
#endif

#if !defined(HAVE_STRCSPN)
#include "misc/strcspn.c"
#endif

#if !defined(HAVE_STRSPN)
#include "misc/strdup.c"
#endif

#if !defined(HAVE_STRERROR)
#include "misc/error.c"
#endif

#if !defined(HAVE_STRTOL)
#include "misc/strtol.c"
#endif

#if !defined(HAVE_STRTOUL)
#include "misc/strtoul.c"
#endif

#if !defined(HAVE_STRSPN)
#include "misc/strspn.c"
#endif

#if !defined(HAVE_STRSTR)
#include "misc/strstr.c"
#endif

#if defined(USE_GNU_GLOB)
#include "misc/fnmatch.c"
#include "misc/glob.c"
#endif
