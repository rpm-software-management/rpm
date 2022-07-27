#include "system.h"

/* Copyright (C) 1991,92,93,94,95,96,97,98,99 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "system.h"

#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pwd.h>
#include <assert.h>
#include <sys/stat.h>		/* S_ISDIR */
#include <fnmatch.h>
#include <glob.h>

#include <rpm/rpmfileutil.h>
#include <rpm/rpmurl.h>

#include "debug.h"

/* Find the end of the sub-pattern in a brace expression.  We define
   this as an inline function if the compiler permits.  */
static inline const char *next_brace_sub(const char *begin)
{
    unsigned int depth = 0;
    const char *cp = begin;

    while (*cp != '\0') {
	if ((*cp == '}' && depth-- == 0) || (*cp == ',' && depth == 0))
	    break;

	if (*cp++ == '{')
	    depth++;
    }

    return *cp != '\0' ? cp : NULL;
}

static int ismagic(const char *pattern)
{
    register const char *p, *q;
    int openBrackets = 0;

    for (p = pattern; *p != '\0'; p++)
	switch (*p) {
	case '?':
	case '*':
	case '\\':
	case '~':
	    return 1;
	case '[':
	    openBrackets = 1;
	    break;
	case ']':
	    if (openBrackets)
		return 1;
	    break;
	case '{':
	    q = p;
	    while (*q != '}')
		if ((q = next_brace_sub(q + 1)) == NULL)
		    return 0;
	    return 1;
	}

    return 0;
}

/* librpmio exported interfaces */

int rpmGlob(const char * pattern, int * argcPtr, ARGV_t * argvPtr)
{
    int argc = 0;
    ARGV_t argv = NULL;
    const char *home = getenv("HOME");
    const char *path;
    int ut = urlPath(pattern, &path);
    int local = (ut == URL_IS_PATH) || (ut == URL_IS_UNKNOWN);
    size_t plen = strlen(path);
    int dir_only = (plen > 0 && path[plen-1] == '/');
    glob_t gl;
    int flags = 0;
    int i;
    int rc = 0;

#ifdef ENABLE_NLS
    char * old_collate = NULL;
    char * old_ctype = NULL;
    const char * t;
#endif

    if (!local || !ismagic(pattern)) {
	argvAdd(argvPtr, pattern);
	goto exit;
    }

    flags |= GLOB_BRACE;
    if (home != NULL && strlen(home) > 0) 
	flags |= GLOB_TILDE;
    if (dir_only)
	flags |= GLOB_ONLYDIR;

    if (argvPtr == NULL)
	/* Use local arglist for counting purposes (argc) */
	argvPtr = &argv;

#ifdef ENABLE_NLS
    t = setlocale(LC_COLLATE, NULL);
    if (t)
    	old_collate = xstrdup(t);
    t = setlocale(LC_CTYPE, NULL);
    if (t)
    	old_ctype = xstrdup(t);
    (void) setlocale(LC_COLLATE, "C");
    (void) setlocale(LC_CTYPE, "C");
#endif
    
    gl.gl_pathc = 0;
    gl.gl_pathv = NULL;
    
    rc = glob(pattern, flags, NULL, &gl);
    if (rc)
	goto exit;

    for (i = 0; i < gl.gl_pathc; i++) {
	if (dir_only) {
	    struct stat sb;
	    if (lstat(gl.gl_pathv[i], &sb) || !S_ISDIR(sb.st_mode))
		continue;
	}
	argvAdd(argvPtr, gl.gl_pathv[i]);
    }
    globfree(&gl);

exit:
    argc = argvCount(*argvPtr);
    if (argcPtr)
	*argcPtr = argc;
    if (argvPtr == &argv)
	argvFree(argv);
    if (argc == 0 && rc == 0)
	rc = GLOB_NOMATCH;

#ifdef ENABLE_NLS	
    if (old_collate) {
	(void) setlocale(LC_COLLATE, old_collate);
	free(old_collate);
    }
    if (old_ctype) {
	(void) setlocale(LC_CTYPE, old_ctype);
	free(old_ctype);
    }
#endif

    return rc;
}
