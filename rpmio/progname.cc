#include "system.h"

#include <rpm/rpmutil.h>

#if defined(HAVE_SETPROGNAME) /* BSD'ish systems */
#include <stdlib.h>
#elif defined(HAVE___PROGNAME) /* glibc and others */
extern const char *__progname;
#else
# error "Did not find any sutable implementation of getprogname/setprogname"
#endif

const char *rgetprogname(void)
{
#if defined(HAVE_SETPROGNAME) /* BSD'ish systems */
    return getprogname();
#elif defined(HAVE___PROGNAME) /* glibc and others */
    return __progname;
#else
# error "Did not find any sutable implementation of getprogname"
#endif
}

void rsetprogname(const char *pn)
{
#if defined(HAVE_SETPROGNAME) /* BSD'ish systems */
    setprogname(pn);
#elif defined(HAVE___PROGNAME) /* glibc and others */
#else
# error "Did not find any sutable implementation of setprogname"
#endif
}
