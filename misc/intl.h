/* Dummy header for libintl.h */

#include "misc-config.h"

#if HAVE_LIBINTL_H
#include <libintl.h>
#define _(String) gettext((String))
#else
#define _(String) (String)
#endif

