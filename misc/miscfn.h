#ifndef H_MISCFN
#define H_MISCFN

#include "misc-config.h"

#if HAVE_FNMATCH_H
#include <fnmatch.h>
#else
#include <misc-fnmatch.h>
#endif

#if HAVE_GLOB_H
#include <glob.h>
#else
#include <misc-glob.h>
#endif

#endif
