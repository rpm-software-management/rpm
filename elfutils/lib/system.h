#ifndef BU_SYSTEM_H
#define BU_SYSTEM_H	1

#include <stddef.h>

extern void *xmalloc (size_t) __attribute__ ((__malloc__))
	/*@*/;
extern void *xcalloc (size_t, size_t) __attribute__ ((__malloc__))
	/*@*/;
extern void *xrealloc (void *, size_t) __attribute__ ((__malloc__))
	/*@*/;

extern char *xstrdup (const char *) __attribute__ ((__malloc__))
	/*@*/;
extern char *xstrndup (const char *, size_t) __attribute__ ((__malloc__))
	/*@*/;


/* A special gettext function we use if the strings are too short.  */
#define sgettext(Str) \
  ({ const char *__res = strrchr (gettext (Str), '|');			      \
     __res ? __res + 1 : Str; })

#define gettext_noop(Str) Str

#endif /* system.h */
