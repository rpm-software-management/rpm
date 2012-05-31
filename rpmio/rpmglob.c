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

/* AIX requires this to be the first thing in the file.  */
#if defined _AIX && !defined __GNUC__
#pragma alloca
#endif

#include "system.h"

#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <assert.h>
#include <sys/stat.h>		/* S_ISDIR */

/* Bits set in the FLAGS argument to `glob'.  */
#define	GLOB_ERR	(1 << 0)	/* Return on read errors.  */
#define	GLOB_MARK	(1 << 1)	/* Append a slash to each name.  */
#define	GLOB_NOSORT	(1 << 2)	/* Don't sort the names.  */
#define	GLOB_DOOFFS	(1 << 3)	/* Insert PGLOB->gl_offs NULLs.  */
#define	GLOB_NOCHECK	(1 << 4)	/* If nothing matches, return the pattern.  */
#define	GLOB_APPEND	(1 << 5)	/* Append to results of a previous call.  */
#define	GLOB_NOESCAPE	(1 << 6)	/* Backslashes don't quote metacharacters.  */
#define	GLOB_PERIOD	(1 << 7)	/* Leading `.' can be matched by metachars.  */

#define GLOB_MAGCHAR	 (1 << 8)	/* Set in gl_flags if any metachars seen.  */
#define GLOB_ALTDIRFUNC (1 << 9)	/* Use gl_opendir et al functions.  */
#define GLOB_BRACE	 (1 << 10)	/* Expand "{a,b}" to "a" "b".  */
#define GLOB_NOMAGIC	 (1 << 11)	/* If no magic chars, return the pattern.  */
#define GLOB_TILDE	 (1 << 12)	/* Expand ~user and ~ to home directories. */
#define GLOB_ONLYDIR	 (1 << 13)	/* Match only directories.  */
#define GLOB_TILDE_CHECK (1 << 14)	/* Like GLOB_TILDE but return an error
					   if the user name is not available.  */
#define __GLOB_FLAGS	(GLOB_ERR|GLOB_MARK|GLOB_NOSORT|GLOB_DOOFFS| \
			 GLOB_NOESCAPE|GLOB_NOCHECK|GLOB_APPEND|     \
			 GLOB_PERIOD|GLOB_ALTDIRFUNC|GLOB_BRACE|     \
			 GLOB_NOMAGIC|GLOB_TILDE|GLOB_ONLYDIR|GLOB_TILDE_CHECK)

/* Error returns from `glob'.  */
#define	GLOB_NOSPACE	1	/* Ran out of memory.  */
#define	GLOB_ABORTED	2	/* Read error.  */
#define	GLOB_NOMATCH	3	/* No matches found.  */
#define GLOB_NOSYS	4	/* Not implemented.  */

/* Structure describing a globbing run.  */
typedef struct {
    size_t gl_pathc;		/* Count of paths matched by the pattern.  */
    char **gl_pathv;		/* List of matched pathnames.  */
    size_t gl_offs;		/* Slots to reserve in `gl_pathv'.  */
    int gl_flags;		/* Set to FLAGS, maybe | GLOB_MAGCHAR.  */

    /* If the GLOB_ALTDIRFUNC flag is set, the following functions
       are used instead of the normal file access functions.  */
    void (*gl_closedir)(void *);
    struct dirent *(*gl_readdir)(void *);
    void *(*gl_opendir)(const char *);
    int (*gl_lstat)(const char *, struct stat *);
    int (*gl_stat)(const char *, struct stat *);
} glob_t;

#define	NAMLEN(_d)	NLENGTH(_d)

#if (defined POSIX || defined WINDOWS32) && !defined __GNU_LIBRARY__
/* Posix does not require that the d_ino field be present, and some
   systems do not provide it. */
#define REAL_DIR_ENTRY(dp) 1
#else
#define REAL_DIR_ENTRY(dp) (dp->d_ino != 0)
#endif				/* POSIX */

#include <errno.h>
#ifndef __set_errno
#define __set_errno(val) errno = (val)
#endif

#include <popt.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmurl.h>

#include "debug.h"

/* Outcomment the following line for production quality code.  */
/* #define NDEBUG 1 */

#define GLOB_INTERFACE_VERSION 1

static void globfree(glob_t * pglob);
static inline const char *next_brace_sub(const char *begin);
static int glob_in_dir(const char *pattern, const char *directory,
			    int flags,
			    int (*errfunc) (const char *, int),
			    glob_t * pglob);
static int prefix_array(const char *prefix, char **array, size_t n);
static int collated_compare(const void *, const void *);

#ifndef HAVE_MEMPCPY
static void * mempcpy(void *dest, const void *src, size_t n)
{
    return (char *) memcpy(dest, src, n) + n;
}
#endif

/* Find the end of the sub-pattern in a brace expression.  We define
   this as an inline function if the compiler permits.  */
static inline const char *next_brace_sub(const char *begin)
{
    unsigned int depth = 0;
    const char *cp = begin;

    while (1) {
	if (depth == 0) {
	    if (*cp != ',' && *cp != '}' && *cp != '\0') {
		if (*cp == '{')
		    ++depth;
		++cp;
		continue;
	    }
	} else {
	    while (*cp != '\0' && (*cp != '}' || depth > 0)) {
		if (*cp == '}')
		    --depth;
		++cp;
	    }
	    if (*cp == '\0')
		/* An incorrectly terminated brace expression.  */
		return NULL;

	    continue;
	}
	break;
    }

    return cp;
}

static int __glob_pattern_p(const char *pattern, int quote);

/* Do glob searching for PATTERN, placing results in PGLOB.
   The bits defined above may be set in FLAGS.
   If a directory cannot be opened or read and ERRFUNC is not nil,
   it is called with the pathname that caused the error, and the
   `errno' value from the failing call; if it returns non-zero
   `glob' returns GLOB_ABORTED; if it returns zero, the error is ignored.
   If memory cannot be allocated for PGLOB, GLOB_NOSPACE is returned.
   Otherwise, `glob' returns zero.  */
static int
glob(const char *pattern, int flags,
     int (*errfunc)(const char *, int), glob_t * pglob)
{
    const char *filename;
    const char *dirname;
    size_t dirlen;
    int status;
    int oldcount;

    if (pattern == NULL || pglob == NULL || (flags & ~__GLOB_FLAGS) != 0) {
	__set_errno(EINVAL);
	return -1;
    }

    if (flags & GLOB_BRACE) {
	const char *begin = strchr(pattern, '{');
	if (begin != NULL) {
	    /* Allocate working buffer large enough for our work.  Note that
	       we have at least an opening and closing brace.  */
	    int firstc;
	    char *alt_start;
	    const char *p;
	    const char *next;
	    const char *rest;
	    size_t rest_len;
	    char onealt[strlen(pattern) - 1];

	    /* We know the prefix for all sub-patterns.  */
	    alt_start = mempcpy(onealt, pattern, begin - pattern);

	    /* Find the first sub-pattern and at the same time find the
	       rest after the closing brace.  */
	    next = next_brace_sub(begin + 1);
	    if (next == NULL) {
		/* It is an illegal expression.  */
		return glob(pattern, flags & ~GLOB_BRACE, errfunc, pglob);
	    }

	    /* Now find the end of the whole brace expression.  */
	    rest = next;
	    while (*rest != '}') {
		rest = next_brace_sub(rest + 1);
		if (rest == NULL) {
		    /* It is an illegal expression.  */
		    return glob(pattern, flags & ~GLOB_BRACE, errfunc,
				pglob);
		}
	    }
	    /* Please note that we now can be sure the brace expression
	       is well-formed.  */
	    rest_len = strlen(++rest) + 1;

	    /* We have a brace expression.  BEGIN points to the opening {,
	       NEXT points past the terminator of the first element, and END
	       points past the final }.  We will accumulate result names from
	       recursive runs for each brace alternative in the buffer using
	       GLOB_APPEND.  */

	    if (!(flags & GLOB_APPEND)) {
		/* This call is to set a new vector, so clear out the
		   vector so we can append to it.  */
		pglob->gl_pathc = 0;
		pglob->gl_pathv = NULL;
	    }
	    firstc = pglob->gl_pathc;

	    p = begin + 1;
	    while (1) {
		int result;

		/* Construct the new glob expression.  */
		mempcpy(mempcpy(alt_start, p, next - p), rest, rest_len);

		result = glob(onealt,
			      ((flags & ~(GLOB_NOCHECK | GLOB_NOMAGIC))
			       | GLOB_APPEND), errfunc, pglob);

		/* If we got an error, return it.  */
		if (result && result != GLOB_NOMATCH) {
		    if (!(flags & GLOB_APPEND))
			globfree(pglob);
		    return result;
		}

		if (*next == '}')
		    /* We saw the last entry.  */
		    break;

		p = next + 1;
		next = next_brace_sub(p);
		assert(next != NULL);
	    }

	    if (pglob->gl_pathc != firstc)
		/* We found some entries.  */
		return 0;
	    else if (!(flags & (GLOB_NOCHECK | GLOB_NOMAGIC)))
		return GLOB_NOMATCH;
	}
    }

    /* Find the filename.  */
    filename = strrchr(pattern, '/');
    if (filename == NULL) {
	/* This can mean two things: a simple name or "~name".  The latter
	   case is nothing but a notation for a directory.  */
	if ((flags & (GLOB_TILDE | GLOB_TILDE_CHECK)) && pattern[0] == '~') {
	    dirname = pattern;
	    dirlen = strlen(pattern);

	    /* Set FILENAME to NULL as a special flag.  This is ugly but
	       other solutions would require much more code.  We test for
	       this special case below.  */
	    filename = NULL;
	} else {
	    filename = pattern;
	    dirname = ".";
	    dirlen = 0;
	}
    } else if (filename == pattern) {
	/* "/pattern".  */
	dirname = "/";
	dirlen = 1;
	++filename;
    } else {
	char *newp;
	dirlen = filename - pattern;
	newp = (char *) alloca(dirlen + 1);
	*((char *) mempcpy(newp, pattern, dirlen)) = '\0';
	dirname = newp;
	++filename;

	if (filename[0] == '\0' && dirlen > 1) {
	    /* "pattern/".  Expand "pattern", appending slashes.  */
	    int val = glob(dirname, flags | GLOB_MARK, errfunc, pglob);
	    if (val == 0)
		pglob->gl_flags = ((pglob->gl_flags & ~GLOB_MARK)
				   | (flags & GLOB_MARK));
	    return val;
	}
    }

    if (!(flags & GLOB_APPEND)) {
	pglob->gl_pathc = 0;
	pglob->gl_pathv = NULL;
    }

    oldcount = pglob->gl_pathc;

    if ((flags & (GLOB_TILDE | GLOB_TILDE_CHECK)) && dirname[0] == '~') {
	if (dirname[1] == '\0' || dirname[1] == '/') {
	    /* Look up home directory.  */
	    const char *home_dir = getenv("HOME");
	    if (home_dir == NULL || home_dir[0] == '\0') {
		int success;
		char *name;
		success = (name = getlogin()) != NULL;
		if (success) {
		    struct passwd *p;
		    p = getpwnam(name);
		    if (p != NULL)
			home_dir = p->pw_dir;
		}
	    }
	    if (home_dir == NULL || home_dir[0] == '\0') {
		if (flags & GLOB_TILDE_CHECK)
		    return GLOB_NOMATCH;
		else
		    home_dir = "~";	/* No luck.  */
	    }
	    /* Now construct the full directory.  */
	    if (dirname[1] == '\0')
		dirname = home_dir;
	    else {
		char *newp;
		size_t home_len = strlen(home_dir);
		newp = (char *) alloca(home_len + dirlen);
		mempcpy(mempcpy(newp, home_dir, home_len),
			&dirname[1], dirlen);
		dirname = newp;
	    }
	}
	else {
	    char *end_name = strchr(dirname, '/');
	    const char *user_name;
	    const char *home_dir;

	    if (end_name == NULL)
		user_name = dirname + 1;
	    else {
		char *newp;
		newp = (char *) alloca(end_name - dirname);
		*((char *) mempcpy(newp, dirname + 1, end_name - dirname))
		    = '\0';
		user_name = newp;
	    }

	    /* Look up specific user's home directory.  */
	    {
		struct passwd *p;
		p = getpwnam(user_name);
		if (p != NULL)
		    home_dir = p->pw_dir;
		else
		    home_dir = NULL;
	    }
	    /* If we found a home directory use this.  */
	    if (home_dir != NULL) {
		char *newp;
		size_t home_len = strlen(home_dir);
		size_t rest_len = end_name == NULL ? 0 : strlen(end_name);
		newp = (char *) alloca(home_len + rest_len + 1);
		*((char *) mempcpy(mempcpy(newp, home_dir, home_len),
				   end_name, rest_len)) = '\0';
		dirname = newp;
	    } else if (flags & GLOB_TILDE_CHECK)
		/* We have to regard it as an error if we cannot find the
		   home directory.  */
		return GLOB_NOMATCH;
	}
    }

    /* Now test whether we looked for "~" or "~NAME".  In this case we
       can give the answer now.  */
    if (filename == NULL) {
	struct stat st;

	/* Return the directory if we don't check for error or if it exists.  */
	if ((flags & GLOB_NOCHECK)
	    || (((flags & GLOB_ALTDIRFUNC)
		 ? (*pglob->gl_stat) (dirname, &st)
		 : stat(dirname, &st)) == 0 && S_ISDIR(st.st_mode))) {
	    pglob->gl_pathv
		= (char **) xrealloc(pglob->gl_pathv,
				     (pglob->gl_pathc +
				      ((flags & GLOB_DOOFFS) ?
				       pglob->gl_offs : 0) +
				      1 + 1) * sizeof(char *));

	    if (flags & GLOB_DOOFFS)
		while (pglob->gl_pathc < pglob->gl_offs)
		    pglob->gl_pathv[pglob->gl_pathc++] = NULL;

	    pglob->gl_pathv[pglob->gl_pathc] = xstrdup(dirname);
	    if (pglob->gl_pathv[pglob->gl_pathc] == NULL) {
		free(pglob->gl_pathv);
		return GLOB_NOSPACE;
	    }
	    pglob->gl_pathv[++pglob->gl_pathc] = NULL;
	    pglob->gl_flags = flags;

	    return 0;
	}

	/* Not found.  */
	return GLOB_NOMATCH;
    }

    if (__glob_pattern_p(dirname, !(flags & GLOB_NOESCAPE))) {
	/* The directory name contains metacharacters, so we
	   have to glob for the directory, and then glob for
	   the pattern in each directory found.  */
	glob_t dirs;
	register int i;

	if ((flags & GLOB_ALTDIRFUNC) != 0) {
	    /* Use the alternative access functions also in the recursive
	       call.  */
	    dirs.gl_opendir = pglob->gl_opendir;
	    dirs.gl_readdir = pglob->gl_readdir;
	    dirs.gl_closedir = pglob->gl_closedir;
	    dirs.gl_stat = pglob->gl_stat;
	    dirs.gl_lstat = pglob->gl_lstat;
	}

	status = glob(dirname,
		      ((flags & (GLOB_ERR | GLOB_NOCHECK | GLOB_NOESCAPE
				 | GLOB_ALTDIRFUNC))
		       | GLOB_NOSORT | GLOB_ONLYDIR), errfunc, &dirs);
	if (status != 0)
	    return status;

	/* We have successfully globbed the preceding directory name.
	   For each name we found, call glob_in_dir on it and FILENAME,
	   appending the results to PGLOB.  */
	for (i = 0; i < dirs.gl_pathc; ++i) {
	    int old_pathc = pglob->gl_pathc;
	    status = glob_in_dir(filename, dirs.gl_pathv[i],
				 ((flags | GLOB_APPEND)
				  & ~(GLOB_NOCHECK | GLOB_ERR)),
				 errfunc, pglob);
	    if (status == GLOB_NOMATCH)
		/* No matches in this directory.  Try the next.  */
		continue;

	    if (status != 0) {
		globfree(&dirs);
		globfree(pglob);
		return status;
	    }

	    /* Stick the directory on the front of each name.  */
	    if (prefix_array(dirs.gl_pathv[i],
			     &pglob->gl_pathv[old_pathc],
			     pglob->gl_pathc - old_pathc)) {
		globfree(&dirs);
		globfree(pglob);
		return GLOB_NOSPACE;
	    }
	}

	flags |= GLOB_MAGCHAR;

	/* We have ignored the GLOB_NOCHECK flag in the `glob_in_dir' calls.
	   But if we have not found any matching entry and thie GLOB_NOCHECK
	   flag was set we must return the list consisting of the disrectory
	   names followed by the filename.  */
	if (pglob->gl_pathc == oldcount) {
	    /* No matches.  */
	    if (flags & GLOB_NOCHECK) {
		size_t filename_len = strlen(filename) + 1;
		char **new_pathv;
		struct stat st;

		/* This is an pessimistic guess about the size.  */
		pglob->gl_pathv
		    = (char **) xrealloc(pglob->gl_pathv,
					 (pglob->gl_pathc +
					  ((flags & GLOB_DOOFFS) ?
					   pglob->gl_offs : 0) +
					  dirs.gl_pathc + 1) *
					 sizeof(char *));

		if (flags & GLOB_DOOFFS)
		    while (pglob->gl_pathc < pglob->gl_offs)
			pglob->gl_pathv[pglob->gl_pathc++] = NULL;

		for (i = 0; i < dirs.gl_pathc; ++i) {
		    const char *dir = dirs.gl_pathv[i];
		    size_t dir_len = strlen(dir);

		    /* First check whether this really is a directory.  */
		    if (((flags & GLOB_ALTDIRFUNC)
			 ? (*pglob->gl_stat) (dir, &st) : stat(dir,
								 &st)) != 0
			|| !S_ISDIR(st.st_mode))
			/* No directory, ignore this entry.  */
			continue;

		    pglob->gl_pathv[pglob->gl_pathc] = xmalloc(dir_len + 1
							       +
							       filename_len);
		    mempcpy(mempcpy
			    (mempcpy
			     (pglob->gl_pathv[pglob->gl_pathc], dir,
			      dir_len), "/", 1), filename, filename_len);
		    ++pglob->gl_pathc;
		}

		pglob->gl_pathv[pglob->gl_pathc] = NULL;
		pglob->gl_flags = flags;

		/* Now we know how large the gl_pathv vector must be.  */
		new_pathv = (char **) xrealloc(pglob->gl_pathv,
					       ((pglob->gl_pathc + 1)
						* sizeof(char *)));
		pglob->gl_pathv = new_pathv;
	    } else
		return GLOB_NOMATCH;
	}

	globfree(&dirs);
    } else {
	status = glob_in_dir(filename, dirname, flags, errfunc, pglob);
	if (status != 0)
	    return status;

	if (dirlen > 0) {
	    /* Stick the directory on the front of each name.  */
	    int ignore = oldcount;

	    if ((flags & GLOB_DOOFFS) && ignore < pglob->gl_offs)
		ignore = pglob->gl_offs;

	    if (prefix_array(dirname,
			     &pglob->gl_pathv[ignore],
			     pglob->gl_pathc - ignore)) {
		globfree(pglob);
		return GLOB_NOSPACE;
	    }
	}
    }

    if (flags & GLOB_MARK) {
	/* Append slashes to directory names.  */
	int i;
	struct stat st;
	for (i = oldcount; i < pglob->gl_pathc; ++i)
	    if (((flags & GLOB_ALTDIRFUNC)
		 ? (*pglob->gl_stat) (pglob->gl_pathv[i], &st)
		 : stat(pglob->gl_pathv[i], &st)) == 0
		&& S_ISDIR(st.st_mode)) {
		size_t len = strlen(pglob->gl_pathv[i]) + 2;
		char *new = xrealloc(pglob->gl_pathv[i], len);
		strcpy(&new[len - 2], "/");
		pglob->gl_pathv[i] = new;
	    }
    }

    if (!(flags & GLOB_NOSORT)) {
	/* Sort the vector.  */
	int non_sort = oldcount;

	if ((flags & GLOB_DOOFFS) && pglob->gl_offs > oldcount)
	    non_sort = pglob->gl_offs;

	qsort(& pglob->gl_pathv[non_sort],
	      pglob->gl_pathc - non_sort,
	      sizeof(char *), collated_compare);
    }

    return 0;
}


/* Free storage allocated in PGLOB by a previous `glob' call.  */
static void globfree(glob_t * pglob)
{
    if (pglob->gl_pathv != NULL) {
	register int i;
	for (i = 0; i < pglob->gl_pathc; ++i)
	    if (pglob->gl_pathv[i] != NULL)
		free(pglob->gl_pathv[i]);
	free(pglob->gl_pathv);
    }
}


/* Do a collated comparison of A and B.  */
static int collated_compare(const void * a, const void * b)
{
    const char *const s1 = *(const char *const *const) a;
    const char *const s2 = *(const char *const *const) b;

    if (s1 == s2)
	return 0;
    if (s1 == NULL)
	return 1;
    if (s2 == NULL)
	return -1;
    return strcoll(s1, s2);
}


/* Prepend DIRNAME to each of N members of ARRAY, replacing ARRAY's
   elements in place.  Return nonzero if out of memory, zero if successful.
   A slash is inserted between DIRNAME and each elt of ARRAY,
   unless DIRNAME is just "/".  Each old element of ARRAY is freed.  */
static int prefix_array(const char *dirname, char **array, size_t n)
{
    register size_t i;
    size_t dirlen = strlen(dirname);

    if (dirlen == 1 && dirname[0] == '/')
	/* DIRNAME is just "/", so normal prepending would get us "//foo".
	   We want "/foo" instead, so don't prepend any chars from DIRNAME.  */
	dirlen = 0;

    for (i = 0; i < n; ++i) {
	size_t eltlen = strlen(array[i]) + 1;
	char *new = (char *) xmalloc(dirlen + 1 + eltlen);
	{
	    char *endp = (char *) mempcpy(new, dirname, dirlen);
	    *endp++ = '/';
	    mempcpy(endp, array[i], eltlen);
	}
	free(array[i]);
	array[i] = new;
    }

    return 0;
}

/* Return nonzero if PATTERN contains any metacharacters.
   Metacharacters can be quoted with backslashes if QUOTE is nonzero.  */
static int __glob_pattern_p(const char *pattern, int quote)
{
    register const char *p;
    int open = 0;

    for (p = pattern; *p != '\0'; ++p)
	switch (*p) {
	case '?':
	case '*':
	    return 1;

	case '\\':
	    if (quote && p[1] != '\0')
		++p;
	    break;

	case '[':
	    open = 1;
	    break;

	case ']':
	    if (open)
		return 1;
	    break;
	}

    return 0;
}

/* Like `glob', but PATTERN is a final pathname component,
   and matches are searched for in DIRECTORY.
   The GLOB_NOSORT bit in FLAGS is ignored.  No sorting is ever done.
   The GLOB_APPEND flag is assumed to be set (always appends).  */
static int
glob_in_dir(const char *pattern, const char *directory, int flags,
	    int (*errfunc)(const char *, int), glob_t * pglob)
{
    void * stream = NULL;

    struct globlink {
	struct globlink *next;
	char *name;
    };
    struct globlink *names = NULL;
    size_t nfound;
    int meta;
    int save;

    meta = __glob_pattern_p(pattern, !(flags & GLOB_NOESCAPE));
    if (meta == 0) {
	if (flags & (GLOB_NOCHECK | GLOB_NOMAGIC))
	    /* We need not do any tests.  The PATTERN contains no meta
	       characters and we must not return an error therefore the
	       result will always contain exactly one name.  */
	    flags |= GLOB_NOCHECK;
	else {
	    /* Since we use the normal file functions we can also use stat()
	       to verify the file is there.  */
	    struct stat st;
	    size_t patlen = strlen(pattern);
	    size_t dirlen = strlen(directory);
	    char *fullname = (char *) alloca(dirlen + 1 + patlen + 1);

	    mempcpy(mempcpy(mempcpy(fullname, directory, dirlen),
			    "/", 1), pattern, patlen + 1);
	    if (((flags & GLOB_ALTDIRFUNC)
		 ? (*pglob->gl_stat) (fullname, &st)
		 : stat(fullname, &st)) == 0)
		/* We found this file to be existing.  Now tell the rest
		   of the function to copy this name into the result.  */
		flags |= GLOB_NOCHECK;
	}

	nfound = 0;
    } else {
	if (pattern[0] == '\0') {
	    /* This is a special case for matching directories like in
	       "*a/".  */
	    names = (struct globlink *) alloca(sizeof(struct globlink));
	    names->name = (char *) xmalloc(1);
	    names->name[0] = '\0';
	    names->next = NULL;
	    nfound = 1;
	    meta = 0;
	} else {
	    stream = ((flags & GLOB_ALTDIRFUNC)
		      ? (*pglob->gl_opendir) (directory)
		      : opendir(directory));
	    if (stream == NULL) {
		if (errno != ENOTDIR
		    && ((errfunc != NULL && (*errfunc) (directory, errno))
			|| (flags & GLOB_ERR)))
		    return GLOB_ABORTED;
		nfound = 0;
		meta = 0;
	    } else {
		int fnm_flags = ((!(flags & GLOB_PERIOD) ? FNM_PERIOD : 0) |
				 ((flags & GLOB_NOESCAPE) ? FNM_NOESCAPE : 0));
		nfound = 0;
		flags |= GLOB_MAGCHAR;

		while (1) {
		    const char *name;
		    size_t len;
		    struct dirent *d = ((flags & GLOB_ALTDIRFUNC)
					? (*pglob->gl_readdir) (stream)
					: readdir((DIR *) stream));
		    if (d == NULL)
			break;
		    if (!REAL_DIR_ENTRY(d))
			continue;

#ifdef HAVE_STRUCT_DIRENT_D_TYPE
		    /* If we shall match only directories use the information
		       provided by the dirent call if possible.  */
		    if ((flags & GLOB_ONLYDIR)
			&& d->d_type != DT_UNKNOWN && d->d_type != DT_DIR)
			continue;
#endif

		    name = d->d_name;

		    if (fnmatch(pattern, name, fnm_flags) == 0) {
			struct globlink *new = (struct globlink *)
			    alloca(sizeof(struct globlink));
			len = NAMLEN(d);
			new->name = (char *) xmalloc(len + 1);
			*((char *) mempcpy(new->name, name, len))
			    = '\0';
			new->next = names;
			names = new;
			++nfound;
		    }
		}
	    }
	}
    }

    if (nfound == 0 && (flags & GLOB_NOCHECK)) {
	size_t len = strlen(pattern);
	nfound = 1;
	names = (struct globlink *) alloca(sizeof(struct globlink));
	names->next = NULL;
	names->name = (char *) xmalloc(len + 1);
	*((char *) mempcpy(names->name, pattern, len)) = '\0';
    }

    if (nfound != 0) {
	pglob->gl_pathv
	    = (char **) xrealloc(pglob->gl_pathv,
				 (pglob->gl_pathc +
				  ((flags & GLOB_DOOFFS) ? pglob->
				   gl_offs : 0) + nfound +
				  1) * sizeof(char *));

	if (flags & GLOB_DOOFFS)
	    while (pglob->gl_pathc < pglob->gl_offs)
		pglob->gl_pathv[pglob->gl_pathc++] = NULL;

	for (; names != NULL; names = names->next)
	    pglob->gl_pathv[pglob->gl_pathc++] = names->name;
	pglob->gl_pathv[pglob->gl_pathc] = NULL;

	pglob->gl_flags = flags;
    }

    save = errno;
    if (stream != NULL) {
	if (flags & GLOB_ALTDIRFUNC)
	    (*pglob->gl_closedir) (stream);
	else
	    closedir((DIR *) stream);
    }
    __set_errno(save);

    return nfound == 0 ? GLOB_NOMATCH : 0;
}

/* librpmio exported interfaces */

int rpmGlob(const char * patterns, int * argcPtr, ARGV_t * argvPtr)
{
    int ac = 0;
    const char ** av = NULL;
    int argc = 0;
    ARGV_t argv = NULL;
    char * globRoot = NULL;
    const char *home = getenv("HOME");
    int gflags = 0;
#ifdef ENABLE_NLS
    char * old_collate = NULL;
    char * old_ctype = NULL;
    const char * t;
#endif
    size_t maxb, nb;
    int i, j;
    int rc;

    if (home != NULL && strlen(home) > 0) 
	gflags |= GLOB_TILDE;

    /* Can't use argvSplit() here, it doesn't handle whitespace etc escapes */
    rc = poptParseArgvString(patterns, &ac, &av);
    if (rc)
	return rc;

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
	
    if (av != NULL)
    for (j = 0; j < ac; j++) {
	char * globURL;
	const char * path;
	int ut = urlPath(av[j], &path);
	int local = (ut == URL_IS_PATH) || (ut == URL_IS_UNKNOWN);
	size_t plen = strlen(path);
	int flags = gflags;
	int dir_only = (plen > 0 && path[plen-1] == '/');
	glob_t gl;

	if (!local || (!rpmIsGlob(av[j], 0) && strchr(path, '~') == NULL)) {
	    argvAdd(&argv, av[j]);
	    continue;
	}

	if (dir_only)
	    flags |= GLOB_ONLYDIR;
	
	gl.gl_pathc = 0;
	gl.gl_pathv = NULL;
	
	rc = glob(av[j], flags, NULL, &gl);
	if (rc)
	    goto exit;

	/* XXX Prepend the URL leader for globs that have stripped it off */
	maxb = 0;
	for (i = 0; i < gl.gl_pathc; i++) {
	    if ((nb = strlen(&(gl.gl_pathv[i][0]))) > maxb)
		maxb = nb;
	}
	
	nb = ((ut == URL_IS_PATH) ? (path - av[j]) : 0);
	maxb += nb;
	maxb += 1;
	globURL = globRoot = xmalloc(maxb);

	switch (ut) {
	case URL_IS_PATH:
	case URL_IS_DASH:
	    strncpy(globRoot, av[j], nb);
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
    }

    argc = argvCount(argv);
    if (argc > 0) {
	if (argvPtr)
	    *argvPtr = argv;
	if (argcPtr)
	    *argcPtr = argc;
	rc = 0;
    } else
	rc = 1;


exit:
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
    av = _free(av);
    if (rc || argvPtr == NULL) {
	argvFree(argv);
    }
    return rc;
}

int rpmIsGlob(const char * pattern, int quote)
{
    return __glob_pattern_p(pattern, quote);
}
