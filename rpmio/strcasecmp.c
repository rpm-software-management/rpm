/** \ingroup rpmio
 * \file rpmio/strcasecmp.c
 */

#include "system.h"
#include "rpmio.h"
#include "debug.h"

int xstrcasecmp(const char * s1, const char * s2)
{
  const char * p1 = s1;
  const char * p2 = s2;
  char c1, c2;

  if (p1 == p2)
    return 0;

  do
    {
      c1 = xtolower (*p1++);
      c2 = xtolower (*p2++);
      if (c1 == '\0')
        break;
    }
  while (c1 == c2);

  return (int)(c1 - c2);
}

int xstrncasecmp(const char *s1, const char *s2, size_t n)
{
  const char * p1 = s1;
  const char * p2 = s2;
  char c1, c2;

  if (p1 == p2 || n == 0)
    return 0;

  do
    {
      c1 = xtolower (*p1++);
      c2 = xtolower (*p2++);
      if (c1 == '\0' || c1 != c2)
	break;
    } while (--n > 0);

  return (int)(c1 - c2);
}
