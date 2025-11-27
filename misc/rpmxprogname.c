/* Original author:  Kamil Rytarowski
   File:             rpmxprogname.c
   Date of creation: 2013-08-10
   License:          the same as RPM itself */

#include "rpmxprogname.h"

#include <string.h> /* strrchr */

const char *_rpmxprogname = NULL;

const char *_rpmxgetprogname(void)
{
  const char *empty = "";

  if (_rpmxprogname != NULL) /* never return NULL string */
    return _rpmxprogname;
  else
    return empty;
}

void _rpmxsetprogname(const char *pn)
{
  if (pn != NULL && _rpmxprogname == NULL /* set the value only once */) {
    char *p = strrchr(pn, '/'); /* locate the last occurrence of '/' */
    if (p != NULL)
      _rpmxprogname = p + 1 /* strip beginning '/' */;
    else
      _rpmxprogname = pn;
  }
}
