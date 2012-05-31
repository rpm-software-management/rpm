/**
 * \file rpmio/stubs.c
 */

/* XXX Portable shared libraries require rpmlib to contain these functions. */

#include "system.h"

#if !defined(HAVE_STPCPY)
#include "misc/stpcpy.c"
#endif

#if !defined(HAVE_STPNCPY)
#include "misc/stpncpy.c"
#endif

#include "misc/fnmatch.h"
#include "misc/fnmatch.c"
