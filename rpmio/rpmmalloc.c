/** \ingroup rpmio
 * \file rpmio/rpmmalloc.c
 */

#include "system.h"
#include "debug.h"

#if !defined(EXIT_FAILURE)
#define	EXIT_FAILURE	1
#endif

/*@only@*/ void *vmefail(size_t size)
{
    fprintf(stderr, _("memory alloc (%u bytes) returned NULL.\n"), (unsigned)size);
    exit(EXIT_FAILURE);
    /*@notreached@*/
    return NULL;
}

#if !(HAVE_MCHECK_H && defined(__GNUC__))

/*@only@*/ void * xmalloc (size_t size)
{
    register void *value;
    if (size == 0) size++;
    value = malloc (size);
    if (value == 0)
	value = vmefail(size);
    /*@-compdef@*/
    return value;
    /*@=compdef@*/
}

/*@only@*/ void * xcalloc (size_t nmemb, size_t size)
{
    register void *value;
    if (size == 0) size++;
    if (nmemb == 0) nmemb++;
    value = calloc (nmemb, size);
    if (value == 0)
	value = vmefail(size);
    return value;
}

/*@only@*/ void * xrealloc (/*@only@*/ void *ptr, size_t size)
{
    register void *value;
    if (size == 0) size++;
    value = realloc (ptr, size);
    if (value == 0)
	value = vmefail(size);
    return value;
}

/*@only@*/ char * xstrdup (const char *str)
{
    size_t size = strlen(str) + 1;
    char *newstr = (char *) malloc (size);
    if (newstr == 0)
	newstr = (char *) vmefail(size);
    strcpy (newstr, str);
    return newstr;
}

#endif	/* !(HAVE_MCHECK_H && defined(__GNUC__)) */
