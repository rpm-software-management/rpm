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

/* librpmio exported interfaces */

int rpmGlob(const char * pattern, int * argcPtr, ARGV_t * argvPtr)
{
    int argc = 0;
    ARGV_t argv = NULL;
    char * globRoot = NULL;
    char * globURL;
    const char *home = getenv("HOME");
    const char *path;
    int ut = urlPath(pattern, &path);
    int local = (ut == URL_IS_PATH) || (ut == URL_IS_UNKNOWN);
    size_t plen = strlen(path);
    int dir_only = (plen > 0 && path[plen-1] == '/');
    glob_t gl;
    int flags = GLOB_NOMAGIC;
#ifdef ENABLE_NLS
    char * old_collate = NULL;
    char * old_ctype = NULL;
    const char * t;
#endif
    size_t maxb, nb;
    int i;
    int rc = 0;

    flags |= GLOB_BRACE;

    if (home != NULL && strlen(home) > 0) 
	flags |= GLOB_TILDE;

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

    if (!local) {
	argvAdd(&argv, pattern);
	goto exit;
    }

    if (dir_only)
	flags |= GLOB_ONLYDIR;
    
    gl.gl_pathc = 0;
    gl.gl_pathv = NULL;
    
    rc = glob(pattern, flags, NULL, &gl);
    if (rc)
	goto exit;

    /* XXX Prepend the URL leader for globs that have stripped it off */
    maxb = 0;
    for (i = 0; i < gl.gl_pathc; i++) {
	if ((nb = strlen(&(gl.gl_pathv[i][0]))) > maxb)
	    maxb = nb;
    }
    
    nb = ((ut == URL_IS_PATH) ? (path - pattern) : 0);
    maxb += nb;
    maxb += 1;
    globURL = globRoot = xmalloc(maxb);

    switch (ut) {
    case URL_IS_PATH:
    case URL_IS_DASH:
	strncpy(globRoot, pattern, nb);
	break;
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    case URL_IS_HKP:
    case URL_IS_UNKNOWN:
    default:
	break;
    }
    globRoot += nb;
    *globRoot = '\0';

    for (i = 0; i < gl.gl_pathc; i++) {
	const char * globFile = &(gl.gl_pathv[i][0]);

	if (dir_only) {
	    struct stat sb;
	    if (lstat(gl.gl_pathv[i], &sb) || !S_ISDIR(sb.st_mode))
		continue;
	}
	    
	if (globRoot > globURL && globRoot[-1] == '/')
	    while (*globFile == '/') globFile++;
	strcpy(globRoot, globFile);
	argvAdd(&argv, globURL);
    }
    globfree(&gl);
    free(globURL);

exit:
    argc = argvCount(argv);
    if (argc > 0) {
	if (argvPtr)
	    *argvPtr = argv;
	if (argcPtr)
	    *argcPtr = argc;
	rc = 0;
    } else if (rc == 0)
	rc = 1;


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
    if (rc || argvPtr == NULL) {
	argvFree(argv);
    }
    return rc;
}
