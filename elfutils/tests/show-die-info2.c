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


const struct
{
  int code;
  const char *name;
} attrs[] =
{
  { DW_AT_sibling, "sibling" },
  { DW_AT_location, "location" },
  { DW_AT_name, "name" },
  { DW_AT_ordering, "ordering" },
  { DW_AT_subscr_data, "subscr_data" },
  { DW_AT_byte_size, "byte_size" },
  { DW_AT_bit_offset, "bit_offset" },
  { DW_AT_bit_size, "bit_size" },
  { DW_AT_element_list, "element_list" },
  { DW_AT_stmt_list, "stmt_list" },
  { DW_AT_low_pc, "low_pc" },
  { DW_AT_high_pc, "high_pc" },
  { DW_AT_language, "language" },
  { DW_AT_member, "member" },
  { DW_AT_discr, "discr" },
  { DW_AT_discr_value, "discr_value" },
  { DW_AT_visibility, "visibility" },
  { DW_AT_import, "import" },
  { DW_AT_string_length, "string_length" },
  { DW_AT_common_reference, "common_reference" },
  { DW_AT_comp_dir, "comp_dir" },
  { DW_AT_const_value, "const_value" },
  { DW_AT_containing_type, "containing_type" },
  { DW_AT_default_value, "default_value" },
  { DW_AT_inline, "inline" },
  { DW_AT_is_optional, "is_optional" },
  { DW_AT_lower_bound, "lower_bound" },
  { DW_AT_producer, "producer" },
  { DW_AT_prototyped, "prototyped" },
  { DW_AT_return_addr, "return_addr" },
  { DW_AT_start_scope, "start_scope" },
  { DW_AT_stride_size, "stride_size" },
  { DW_AT_upper_bound, "upper_bound" },
  { DW_AT_abstract_origin, "abstract_origin" },
  { DW_AT_accessibility, "accessibility" },
  { DW_AT_address_class, "address_class" },
  { DW_AT_artificial, "artificial" },
  { DW_AT_base_types, "base_types" },
  { DW_AT_calling_convention, "calling_convention" },
  { DW_AT_count, "count" },
  { DW_AT_data_member_location, "data_member_location" },
  { DW_AT_decl_column, "decl_column" },
  { DW_AT_decl_file, "decl_file" },
  { DW_AT_decl_line, "decl_line" },
  { DW_AT_declaration, "declaration" },
  { DW_AT_discr_list, "discr_list" },
  { DW_AT_encoding, "encoding" },
  { DW_AT_external, "external" },
  { DW_AT_frame_base, "frame_base" },
  { DW_AT_friend, "friend" },
  { DW_AT_identifier_case, "identifier_case" },
  { DW_AT_macro_info, "macro_info" },
  { DW_AT_namelist_items, "namelist_items" },
  { DW_AT_priority, "priority" },
  { DW_AT_segment, "segment" },
  { DW_AT_specification, "specification" },
  { DW_AT_static_link, "static_link" },
  { DW_AT_type, "type" },
  { DW_AT_use_location, "use_location" },
  { DW_AT_variable_parameter, "variable_parameter" },
  { DW_AT_virtuality, "virtuality" },
  { DW_AT_vtable_elem_location, "vtable_elem_location" },
  { DW_AT_MIPS_fde, "MIPS_fde" },
  { DW_AT_MIPS_loop_begin, "MIPS_loop_begin" },
  { DW_AT_MIPS_tail_loop_begin, "MIPS_tail_loop_begin" },
  { DW_AT_MIPS_epilog_begin, "MIPS_epilog_begin" },
  { DW_AT_MIPS_loop_unroll_factor, "MIPS_loop_unroll_factor" },
  { DW_AT_MIPS_software_pipeline_depth, "MIPS_software_pipeline_depth" },
  { DW_AT_MIPS_linkage_name, "MIPS_linkage_name" },
  { DW_AT_MIPS_stride, "MIPS_stride" },
  { DW_AT_MIPS_abstract_name, "MIPS_abstract_name" },
  { DW_AT_MIPS_clone_origin, "MIPS_clone_origin" },
  { DW_AT_MIPS_has_inlines, "MIPS_has_inlines" },
  { DW_AT_MIPS_stride_byte, "MIPS_stride_byte" },
  { DW_AT_MIPS_stride_elem, "MIPS_stride_elem" },
  { DW_AT_MIPS_ptr_dopetype, "MIPS_ptr_dopetype" },
  { DW_AT_MIPS_allocatable_dopetype, "MIPS_allocatable_dopetype" },
  { DW_AT_MIPS_assumed_shape_dopetype, "MIPS_assumed_shape_dopetype" },
  { DW_AT_MIPS_assumed_size, "MIPS_assumed_size" },
  { DW_AT_sf_names, "sf_names" },
  { DW_AT_src_info, "src_info" },
  { DW_AT_mac_info, "mac_info" },
  { DW_AT_src_coords, "src_coords" },
  { DW_AT_body_begin, "body_begin" },
  { DW_AT_body_end, "body_end" }
};
#define nattrs (sizeof (attrs) / sizeof (attrs[0]))


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
  int cnt;
  Dwarf_Bool b;
  Dwarf_Addr addr;
  Dwarf_Unsigned u;
  Dwarf_Half h;

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
  printf ("%*s Name      : %s\n", n * 5, "", name);
  printf ("%*s Offset    : %lld\n", n * 5, "", (long long int) off);
  printf ("%*s CU offset : %lld\n", n * 5, "", (long long int) cuoff);

  printf ("%*s Attrs     :", n * 5, "");
  for (cnt = 0; cnt < nattrs; ++cnt)
    if (dwarf_hasattr (die, attrs[cnt].code, &b, NULL) == DW_DLV_OK && b)
      printf (" %s", attrs[cnt].name);
  puts ("");

  if (dwarf_hasattr (die, DW_AT_low_pc, &b, NULL) == DW_DLV_OK && b
      && dwarf_lowpc (die, &addr, NULL) == DW_DLV_OK)
    {
      Dwarf_Attribute attr;
      Dwarf_Addr addr2;
      printf ("%*s low PC    : %#llx\n",
	      n * 5, "", (unsigned long long int) addr);

      if (dwarf_attr (die, DW_AT_low_pc, &attr, NULL) != DW_DLV_OK
	  || dwarf_formaddr (attr, &addr2, NULL) != DW_DLV_OK
	  || addr != addr2)
	puts ("************* DW_AT_low_pc verify failed ************");
      else if (dwarf_hasform (attr, DW_FORM_addr, &b, NULL) != DW_DLV_OK || !b)
	puts ("************* DW_AT_low_pc form failed ************");
      else if (dwarf_whatform (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_FORM_addr)
	puts ("************* DW_AT_low_pc form (2) failed ************");
      else if (dwarf_whatattr (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_AT_low_pc)
	puts ("************* DW_AT_low_pc attr failed ************");
    }
  if (dwarf_hasattr (die, DW_AT_high_pc, &b, NULL) == DW_DLV_OK && b
      && dwarf_highpc (die, &addr, NULL) == DW_DLV_OK)
    {
      Dwarf_Attribute attr;
      Dwarf_Addr addr2;
      printf ("%*s high PC   : %#llx\n",
	      n * 5, "", (unsigned long long int) addr);
      if (dwarf_attr (die, DW_AT_high_pc, &attr, NULL) != DW_DLV_OK
	  || dwarf_formaddr (attr, &addr2, NULL) != DW_DLV_OK
	  || addr != addr2)
	puts ("************* DW_AT_high_pc verify failed ************");
      else if (dwarf_hasform (attr, DW_FORM_addr, &b, NULL) != DW_DLV_OK || !b)
	puts ("************* DW_AT_high_pc form failed ************");
      else if (dwarf_whatform (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_FORM_addr)
	puts ("************* DW_AT_high_pc form (2) failed ************");
      else if (dwarf_whatattr (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_AT_high_pc)
	puts ("************* DW_AT_high_pc attr failed ************");
    }

  if (dwarf_hasattr (die, DW_AT_byte_size, &b, NULL) == DW_DLV_OK && b
      && dwarf_bytesize (die, &u, NULL) == DW_DLV_OK)
    {
      Dwarf_Attribute attr;
      Dwarf_Unsigned u2;
      printf ("%*s byte size : %llu\n", n * 5, "", (unsigned long long int) u);
      if (dwarf_attr (die, DW_AT_byte_size, &attr, NULL) != DW_DLV_OK
	  || dwarf_formudata (attr, &u2, NULL) != DW_DLV_OK
	  || u != u2)
	puts ("************* DW_AT_byte_size verify failed ************");
      else if (dwarf_hasform (attr, DW_FORM_data1, &b, NULL) != DW_DLV_OK
	       || (!b
		   && (dwarf_hasform (attr, DW_FORM_data2, &b, NULL) != DW_DLV_OK
		       || (!b
			   && (dwarf_hasform (attr, DW_FORM_data4, &b, NULL) != DW_DLV_OK
			       || (!b
				   && (dwarf_hasform (attr, DW_FORM_data8, &b, NULL) != DW_DLV_OK
				       || (!b
					   && (dwarf_hasform (attr, DW_FORM_sdata, &b, NULL) != DW_DLV_OK
					       || (!b
						   && (dwarf_hasform (attr, DW_FORM_udata, &b, NULL) != DW_DLV_OK
						       || !b)))))))))))
	puts ("************* DW_AT_byte_size form failed ************");
      else if (dwarf_whatform (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_FORM_data1
	       && h != DW_FORM_data2
	       && h != DW_FORM_data4
	       && h != DW_FORM_data8
	       && h != DW_FORM_sdata
	       && h != DW_FORM_udata)
	puts ("************* DW_AT_byte_size form (2) failed ************");
      else if (dwarf_whatattr (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_AT_byte_size)
	puts ("************* DW_AT_byte_size attr failed ************");
    }
  if (dwarf_hasattr (die, DW_AT_bit_size, &b, NULL) == DW_DLV_OK && b
      && dwarf_bitsize (die, &u, NULL) == DW_DLV_OK)
    {
      Dwarf_Attribute attr;
      Dwarf_Unsigned u2;
      printf ("%*s bit size  : %llu\n", n * 5, "", (unsigned long long int) u);
      if (dwarf_attr (die, DW_AT_bit_size, &attr, NULL) != DW_DLV_OK
	  || dwarf_formudata (attr, &u2, NULL) != DW_DLV_OK
	  || u != u2)
	puts ("************* DW_AT_bit_size test failed ************");
      else if (dwarf_hasform (attr, DW_FORM_data1, &b, NULL) != DW_DLV_OK
	       || (!b
		   && (dwarf_hasform (attr, DW_FORM_data2, &b, NULL) != DW_DLV_OK
		       || (!b
			   && (dwarf_hasform (attr, DW_FORM_data4, &b, NULL) != DW_DLV_OK
			       || (!b
				   && (dwarf_hasform (attr, DW_FORM_data8, &b, NULL) != DW_DLV_OK
				       || (!b
					   && (dwarf_hasform (attr, DW_FORM_sdata, &b, NULL) != DW_DLV_OK
					       || (!b
						   && (dwarf_hasform (attr, DW_FORM_udata, &b, NULL) != DW_DLV_OK
						       || !b)))))))))))
	puts ("************* DW_AT_bit_size form failed ************");
      else if (dwarf_whatform (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_FORM_data1
	       && h != DW_FORM_data2
	       && h != DW_FORM_data4
	       && h != DW_FORM_data8
	       && h != DW_FORM_sdata
	       && h != DW_FORM_udata)
	puts ("************* DW_AT_bit_size form (2) failed ************");
      else if (dwarf_whatattr (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_AT_bit_size)
	puts ("************* DW_AT_bit_size attr failed ************");
    }
  if (dwarf_hasattr (die, DW_AT_bit_offset, &b, NULL) == DW_DLV_OK && b
      && dwarf_bitoffset (die, &u, NULL) == DW_DLV_OK)
    {
      Dwarf_Attribute attr;
      Dwarf_Unsigned u2;
      printf ("%*s bit offset: %llu\n", n * 5, "", (unsigned long long int) u);
      if (dwarf_attr (die, DW_AT_bit_offset, &attr, NULL) != DW_DLV_OK
	  || dwarf_formudata (attr, &u2, NULL) != DW_DLV_OK
	  || u != u2)
	puts ("************* DW_AT_bit_offset test failed ************");
      else if (dwarf_hasform (attr, DW_FORM_data1, &b, NULL) != DW_DLV_OK
	       || (!b
		   && (dwarf_hasform (attr, DW_FORM_data2, &b, NULL) != DW_DLV_OK
		       || (!b
			   && (dwarf_hasform (attr, DW_FORM_data4, &b, NULL) != DW_DLV_OK
			       || (!b
				   && (dwarf_hasform (attr, DW_FORM_data8, &b, NULL) != DW_DLV_OK
				       || (!b
					   && (dwarf_hasform (attr, DW_FORM_sdata, &b, NULL) != DW_DLV_OK
					       || (!b
						   && (dwarf_hasform (attr, DW_FORM_udata, &b, NULL) != DW_DLV_OK
						       || !b)))))))))))
	puts ("************* DW_AT_bit_offset form failed ************");
      else if (dwarf_whatform (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_FORM_data1
	       && h != DW_FORM_data2
	       && h != DW_FORM_data4
	       && h != DW_FORM_data8
	       && h != DW_FORM_sdata
	       && h != DW_FORM_udata)
	puts ("************* DW_AT_bit_offset form (2) failed ************");
      else if (dwarf_whatattr (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_AT_bit_offset)
	puts ("************* DW_AT_bit_offset attr failed ************");
    }

  if (dwarf_hasattr (die, DW_AT_language, &b, NULL) == DW_DLV_OK && b
      && dwarf_srclang (die, &u, NULL) == DW_DLV_OK)
    {
      Dwarf_Attribute attr;
      Dwarf_Unsigned u2;
      printf ("%*s language  : %llu\n", n * 5, "", (unsigned long long int) u);
      if (dwarf_attr (die, DW_AT_language, &attr, NULL) != DW_DLV_OK
	  || dwarf_formudata (attr, &u2, NULL) != DW_DLV_OK
	  || u != u2)
	puts ("************* DW_AT_language test failed ************");
      else if (dwarf_hasform (attr, DW_FORM_data1, &b, NULL) != DW_DLV_OK
	       || (!b
		   && (dwarf_hasform (attr, DW_FORM_data2, &b, NULL) != DW_DLV_OK
		       || (!b
			   && (dwarf_hasform (attr, DW_FORM_data4, &b, NULL) != DW_DLV_OK
			       || (!b
				   && (dwarf_hasform (attr, DW_FORM_data8, &b, NULL) != DW_DLV_OK
				       || (!b
					   && (dwarf_hasform (attr, DW_FORM_sdata, &b, NULL) != DW_DLV_OK
					       || (!b
						   && (dwarf_hasform (attr, DW_FORM_udata, &b, NULL) != DW_DLV_OK
						       || !b)))))))))))
	puts ("************* DW_AT_language form failed ************");
      else if (dwarf_whatform (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_FORM_data1
	       && h != DW_FORM_data2
	       && h != DW_FORM_data4
	       && h != DW_FORM_data8
	       && h != DW_FORM_sdata
	       && h != DW_FORM_udata)
	puts ("************* DW_AT_language form (2) failed ************");
      else if (dwarf_whatattr (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_AT_language)
	puts ("************* DW_AT_language attr failed ************");
    }

  if (dwarf_hasattr (die, DW_AT_ordering, &b, NULL) == DW_DLV_OK && b
      && dwarf_arrayorder (die, &u, NULL) == DW_DLV_OK)
    {
      Dwarf_Attribute attr;
      Dwarf_Unsigned u2;
      printf ("%*s ordering  : %llu\n", n * 5, "", (unsigned long long int) u);
      if (dwarf_attr (die, DW_AT_ordering, &attr, NULL) != DW_DLV_OK
	  || dwarf_formudata (attr, &u2, NULL) != DW_DLV_OK
	  || u != u2)
	puts ("************* DW_AT_ordering test failed ************");
      else if (dwarf_hasform (attr, DW_FORM_data1, &b, NULL) != DW_DLV_OK
	       || (!b
		   && (dwarf_hasform (attr, DW_FORM_data2, &b, NULL) != DW_DLV_OK
		       || (!b
			   && (dwarf_hasform (attr, DW_FORM_data4, &b, NULL) != DW_DLV_OK
			       || (!b
				   && (dwarf_hasform (attr, DW_FORM_data8, &b, NULL) != DW_DLV_OK
				       || (!b
					   && (dwarf_hasform (attr, DW_FORM_sdata, &b, NULL) != DW_DLV_OK
					       || (!b
						   && (dwarf_hasform (attr, DW_FORM_udata, &b, NULL) != DW_DLV_OK
						       || !b)))))))))))
	puts ("************* DW_AT_ordering failed ************");
      else if (dwarf_whatform (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_FORM_data1
	       && h != DW_FORM_data2
	       && h != DW_FORM_data4
	       && h != DW_FORM_data8
	       && h != DW_FORM_sdata
	       && h != DW_FORM_udata)
	puts ("************* DW_AT_ordering form (2) failed ************");
      else if (dwarf_whatattr (attr, &h, NULL) != DW_DLV_OK
	       && h != DW_AT_ordering)
	puts ("************* DW_AT_ordering attr failed ************");
    }

  if (dwarf_hasattr (die, DW_AT_comp_dir, &b, NULL) == DW_DLV_OK && b)
    {
      Dwarf_Attribute attr;
      if (dwarf_attr (die, DW_AT_comp_dir, &attr, NULL) != DW_DLV_OK
	  || dwarf_formstring (attr, &name, NULL) != DW_DLV_OK)
	puts ("************* DW_AT_comp_dir attr failed ************");
      else
	printf ("%*s directory : %s\n", n * 5, "", name);
    }

  if (dwarf_hasattr (die, DW_AT_producer, &b, NULL) == DW_DLV_OK && b)
    {
      Dwarf_Attribute attr;
      if (dwarf_attr (die, DW_AT_producer, &attr, NULL) != DW_DLV_OK
	  || dwarf_formstring (attr, &name, NULL) != DW_DLV_OK)
	puts ("************* DW_AT_comp_dir attr failed ************");
      else
	printf ("%*s producer  : %s\n", n * 5, "", name);
    }

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
