#include "system.h"

#if !defined(EXIT_FAILURE)
#define	EXIT_FAILURE	1
#endif

void *vmefail(void)
{
    fprintf(stderr, _("memory alloc returned NULL.\n"));
    exit(EXIT_FAILURE);
    /*@notreached@*/
    return NULL;
}

#if !(HAVE_MCHECK_H && defined(__GNUC__))

void * xmalloc (size_t size)
{
    register void *value;
    if (size == 0) size++;
    value = malloc (size);
    if (value == 0)
	value = vmefail();
    return value;
}

void * xcalloc (size_t nmemb, size_t size)
{
    register void *value;
    if (size == 0) size++;
    if (nmemb == 0) nmemb++;
    value = calloc (nmemb, size);
    if (value == 0)
	value = vmefail();
    return value;
}

void * xrealloc (void *ptr, size_t size)
{
    register void *value;
    if (size == 0) size++;
    value = realloc (ptr, size);
    if (value == 0)
	value = vmefail();
    return value;
}

char * xstrdup (const char *str)
{
    char *newstr = (char *) malloc (strlen(str) + 1);
    if (newstr == 0)
	newstr = (char *) vmefail();
    strcpy (newstr, str);
    return newstr;
}

#endif	/* !(HAVE_MCHECK_H && defined(__GNUC__)) */
