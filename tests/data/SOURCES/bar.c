#include "foobar.h"

struct debug_types_section1 var_bar1;

/* Use different DW_AT_stmt_list in .debug_types having it outside foobar.h.
   Also use two types in .debug_types for multiple COMDAT sections in object
   files.  */
struct debug_types_section_bar
{
  int stringp_bar, stb;
};
struct debug_types_section_bar var_bar;

int number = NUMBER;

int
bar (int n)
{
  return n - NUMBER;
}
