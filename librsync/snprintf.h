/* Id: snprintf.h,v 1.2 2000/08/08 09:20:34 mbp Exp  */

#ifndef HAVE_SNPRINTF
int snprintf (char *str, size_t count, const char *fmt, ...);
#endif
#ifndef HAVE_VSPRINTF
int vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#endif
