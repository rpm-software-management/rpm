/** \ingroup rpmio
 * \file rpmio/strcasecmp.c
 */

#include "system.h"
#include "rpmio.h"
#include "debug.h"

static inline unsigned char xtolower(unsigned char c)
{
    return ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c);
}

int xstrcasecmp(const char *s1, const char *s2)
{
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;

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

  return c1 - c2;
}

int xstrncasecmp(const char *s1, const char *s2, size_t n)
{
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;

  if (p1 == p2 || n == 0)
    return 0;

  do
    {
      c1 = xtolower (*p1++);
      c2 = xtolower (*p2++);
      if (c1 == '\0' || c1 != c2)
        return c1 - c2;
    } while (--n > 0);

  return c1 - c2;
}
