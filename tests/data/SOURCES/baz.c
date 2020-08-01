#include "foobar.h"

struct debug_types_section1 var_baz1;

/* Use different DW_AT_stmt_list in .debug_types having it outside foobar.h.
   Also use two types in .debug_types for multiple COMDAT sections in object
   files.  */
struct debug_types_section_baz
{
  int stringp_baz, stz;
};
struct debug_types_section_baz var_baz;

int
main ()
{
  int res;
  res = foo ();
  res = bar (res);
  return res + number - NUMBER;
}
