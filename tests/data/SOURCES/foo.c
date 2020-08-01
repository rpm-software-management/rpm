#include "foobar.h"

struct debug_types_section1 var_foo1;

/* Use different DW_AT_stmt_list in .debug_types having it outside foobar.h.
   Also use two types in .debug_types for multiple COMDAT sections in object
   files.  */
struct debug_types_section_foo
{
  int stringp_foo, stf;
};
struct debug_types_section_foo var_foo;

int
foo ()
{
  return number;
}
