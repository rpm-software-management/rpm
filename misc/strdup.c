/*
 * Author: Tim Mooney <mooney@plains.nodak.edu>
 * Copyright: This file is in the public domain.
 *
 * a replacement for strdup() for those platforms that don't have it,
 * like Ultrix.
 *
 * Requires: malloc(), strlen(), strcpy().
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if defined(HAVE_STDLIB_H) || defined(STDC_HEADERS)
# include <stdlib.h>
#else
extern void *malloc(size_t);
#endif

#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
# include <string.h>
#else
extern size_t strlen(const char *);
extern char *strcpy(char *, const char *);
#endif

char * strdup(const char *s) {
	void *p = NULL;

	p = malloc(strlen(s)+1);
	if (!p) {
		return NULL;
	}

	strcpy( (char *) p, s);
	return (char *) p;
}

