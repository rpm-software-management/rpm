/* Dummy header for libintl.h */

#include "config.h"

#if HAVE_LIBINTL_H
#include <libintl.h>
#define _(String) gettext((String))
#else
#define bindtextdomain(foo, bar)
#define textdomain(foo)
#define _(String) (String)
#endif

