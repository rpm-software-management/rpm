#include <dwarf.h>
#include <libelf.h>
#include <libdwarf.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>


static const char *tagnames[] =
{
  [DW_TAG_array_type] = "DW_TAG_array_type",
  [DW_TAG_class_type] = "DW_TAG_class_type",
  [DW_TAG_entry_point] = "DW_TAG_entry_point",
  [DW_TAG_enumeration_type] = "DW_TAG_enumeration_type",
  [DW_TAG_formal_parameter] = "DW_TAG_formal_parameter",
  [DW_TAG_imported_declaration] = "DW_TAG_imported_declaration",
  [DW_TAG_label] = "DW_TAG_label",
  [DW_TAG_lexical_block] = "DW_TAG_lexical_block",
  [DW_TAG_member] = "DW_TAG_member",
  [DW_TAG_pointer_type] = "DW_TAG_pointer_type",
  [DW_TAG_reference_type] = "DW_TAG_reference_type",
  [DW_TAG_compile_unit] = "DW_TAG_compile_unit",
  [DW_TAG_string_type] = "DW_TAG_string_type",
  [DW_TAG_structure_type] = "DW_TAG_structure_type",
  [DW_TAG_subroutine_type] = "DW_TAG_subroutine_type",
  [DW_TAG_typedef] = "DW_TAG_typedef",
  [DW_TAG_union_type] = "DW_TAG_union_type",
  [DW_TAG_unspecified_parameters] = "DW_TAG_unspecified_parameters",
  [DW_TAG_variant] = "DW_TAG_variant",
  [DW_TAG_common_block] = "DW_TAG_common_block",
  [DW_TAG_common_inclusion] = "DW_TAG_common_inclusion",
  [DW_TAG_inheritance] = "DW_TAG_inheritance",
  [DW_TAG_inlined_subroutine] = "DW_TAG_inlined_subroutine",
  [DW_TAG_module] = "DW_TAG_module",
  [DW_TAG_ptr_to_member_type] = "DW_TAG_ptr_to_member_type",
  [DW_TAG_set_type] = "DW_TAG_set_type",
  [DW_TAG_subrange_type] = "DW_TAG_subrange_type",
  [DW_TAG_with_stmt] = "DW_TAG_with_stmt",
  [DW_TAG_access_declaration] = "DW_TAG_access_declaration",
  [DW_TAG_base_type] = "DW_TAG_base_type",
  [DW_TAG_catch_block] = "DW_TAG_catch_block",
  [DW_TAG_const_type] = "DW_TAG_const_type",
  [DW_TAG_constant] = "DW_TAG_constant",
  [DW_TAG_enumerator] = "DW_TAG_enumerator",
  [DW_TAG_file_type] = "DW_TAG_file_type",
  [DW_TAG_friend] = "DW_TAG_friend",
  [DW_TAG_namelist] = "DW_TAG_namelist",
  [DW_TAG_namelist_item] = "DW_TAG_namelist_item",
  [DW_TAG_packed_type] = "DW_TAG_packed_type",
  [DW_TAG_subprogram] = "DW_TAG_subprogram",
  [DW_TAG_template_type_param] = "DW_TAG_template_type_param",
  [DW_TAG_template_value_param] = "DW_TAG_template_value_param",
  [DW_TAG_thrown_type] = "DW_TAG_thrown_type",
  [DW_TAG_try_block] = "DW_TAG_try_block",
  [DW_TAG_variant_part] = "DW_TAG_variant_part",
  [DW_TAG_variable] = "DW_TAG_variable",
  [DW_TAG_volatile_type] = "DW_TAG_volatile_type"
};
#define ntagnames (sizeof (tagnames) / sizeof (tagnames[0]))


void
handle (Dwarf_Debug dbg, Dwarf_Die die, int n)
{
  Dwarf_Die child;
  Dwarf_Half tag;
  const char *str;
  char buf[30];
  char *name;
  int res;
  Dwarf_Off off;
  Dwarf_Off cuoff;

  if (dwarf_tag (die, &tag, NULL) == DW_DLV_OK)
    {
      if (tag < ntagnames)
	str = tagnames[tag];
      else
	{
	  snprintf (buf, sizeof buf, "%#x", tag);
	  str = buf;
	}
    }
  else
    str = "* NO TAG *";

  res = dwarf_diename (die, &name, NULL);
  if (res == DW_DLV_ERROR)
    name = strdup ("???");
  else if (res == DW_DLV_NO_ENTRY)
    name = strdup ("* NO NAME *");

  if (dwarf_dieoffset (die, &off, NULL) != DW_DLV_OK)
    off = -1l;
  if (dwarf_die_CU_offset (die, &cuoff, NULL) != DW_DLV_OK)
    cuoff = -1l;

  printf ("%*s%s\n", n * 5, "", str);
  printf ("%*s Name     : %s\n", n * 5, "", name);
  printf ("%*s Offset   : %lld\n", n * 5, "", (long long int) off);
  printf ("%*s CU offset: %lld\n", n * 5, "", (long long int) cuoff);

  if (dwarf_child (die, &child, NULL) == DW_DLV_OK)
    handle (dbg, child, n + 1);
  if (dwarf_siblingof (dbg, die, &die, NULL) == DW_DLV_OK)
    handle (dbg, die, n);
}


int
main (int argc, char *argv[])
{
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);
      Dwarf_Debug dbg;
      Dwarf_Unsigned cuhl;
      Dwarf_Half v;
      Dwarf_Unsigned o;
      Dwarf_Half sz;
      Dwarf_Unsigned ncu;
      Dwarf_Die die;

      printf ("file: %s\n", argv[cnt]);

      if (dwarf_init (fd, DW_DLC_READ, NULL, NULL, &dbg, NULL) != DW_DLV_OK)
	{
	  printf ("%s not usable\n", argv[cnt]);
	  close (fd);
	  continue;
	}

      while (dwarf_next_cu_header (dbg, &cuhl, &v, &o, &sz, &ncu, NULL)
	     == DW_DLV_OK)
	{
	  printf ("New CU: cuhl = %llu, v = %hu, o = %llu, sz = %hu, ncu = %llu\n",
		  (unsigned long long int) cuhl, v, (unsigned long long int) o,
		  sz, (unsigned long long int) ncu);

	  if (dwarf_siblingof (dbg, NULL, &die, NULL) == DW_DLV_OK)
	    handle (dbg, die, 1);
	}

      dwarf_finish (dbg, NULL);
      close (fd);
    }

  return 0;
}
