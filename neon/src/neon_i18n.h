
/* Include this to use I18N inside neon */

#undef _
#ifdef ENABLE_NLS
#include <libintl.h>
#define _(str) gettext(str)
#else
#define _(str) (str)
#endif /* ENABLE_NLS */
#define N_(str) (str)
