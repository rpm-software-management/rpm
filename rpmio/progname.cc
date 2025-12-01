#include "system.h"

#include <rpm/rpmutil.h>

#if defined(HAVE_SETPROGNAME) /* BSD'ish systems */
#include <stdlib.h>
#elif defined(HAVE___PROGNAME) /* glibc and others */
extern const char *__progname;
#else /* generic implementation */
#include <string.h> /* strrchr */
static const char *rprogname = NULL;
#endif

const char *rgetprogname(void)
{
#if defined(HAVE_SETPROGNAME) /* BSD'ish systems */
    return getprogname();
#elif defined(HAVE___PROGNAME) /* glibc and others */
    return __progname;
#else /* generic implementation */
    return (rprogname != NULL) ? rprogname : "";
#endif
}

void rsetprogname(const char *pn)
{
#if defined(HAVE_SETPROGNAME) /* BSD'ish systems */
    setprogname(pn);
#elif defined(HAVE___PROGNAME) /* glibc and others */
#else /* generic implementation */
    if (pn != NULL && rprogname == NULL /* set the value only once */) {
	const char *p = strrchr(pn, '/'); /* locate the last occurrence of '/' */
	if (p != NULL)
	    rprogname = p + 1 /* strip beginning '/' */;
	else
	    rprogname = pn;
    }
#endif
}
