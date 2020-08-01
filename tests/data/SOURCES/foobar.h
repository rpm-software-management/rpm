#define NUMBER 42

extern int number;

extern int foo (void);
extern int bar (int);

struct debug_types_section1
{
  /* Test both DW_FORM_strp and DW_FORM_string.  */
  int stringp1, st1;
};
