/* Dummy header for libintl.h */

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#define _(String) gettext((String))
#else
#define _(String) (String)
#endif

