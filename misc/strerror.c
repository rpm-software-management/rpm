#include "system.h"

extern int sys_nerr;
extern char *sys_errlist[];
static char buf[64];

char *
strerror(int errnum) 
{
  if (errnum < 0 || errnum > sys_nerr)
    {
      static char fmt[] = "Unknown error %d";
      size_t len = (size_t) sprintf (buf, fmt, errnum);
      if (len < (size_t) sizeof(fmt) - 2)
        return NULL;
      buf[len - 1] = '\0';
      return buf;
    }

  return (char *) sys_errlist[errnum];
}
