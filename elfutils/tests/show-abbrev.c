#include <fcntl.h>
#include <libdwarf.h>
#include <stdio.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);
      Dwarf_Debug dbg;
      Dwarf_Error err;

       if (dwarf_init (fd, DW_DLC_READ, NULL, NULL, &dbg, &err) != DW_DLV_OK)
	 {
	   continue;
	   printf ("%s not usable: %s\n", argv[cnt], dwarf_errmsg (err));
	 }
      else
	{
	  Dwarf_Unsigned offset = 0;

	  while (1)
	    {
	      Dwarf_Abbrev abbrev;
	      Dwarf_Unsigned length;
	      Dwarf_Unsigned attrcnt;
	      Dwarf_Half tag;
	      Dwarf_Unsigned code;
	      Dwarf_Signed children;
	      Dwarf_Half attr_num;
	      Dwarf_Signed attr_form;
	      Dwarf_Off attr_offset;
	      int j;

	      if (dwarf_get_abbrev (dbg, offset, &abbrev, &length, &attrcnt,
				    &err) != DW_DLV_OK)
		{
		  printf ("dwarf_get_abbrev for offset %llu returned error: %s\n",
			  (unsigned long long int) offset,
			  dwarf_errmsg (err));
		  break;
		}
	      else if (length == 1)
		/* End of the list.  */
		break;

	      if (dwarf_get_abbrev_tag (abbrev, &tag, &err) != DW_DLV_OK)
		{
		  printf ("dwarf_get_abbrev_tag at offset %llu returned error: %s\n",
			  (unsigned long long int) offset,
			  dwarf_errmsg (err));
		  break;
		}

	      if (dwarf_get_abbrev_code (abbrev, &code, &err) != DW_DLV_OK)
		{
		  printf ("dwarf_get_abbrev_code at offset %llu returned error: %s\n",
			  (unsigned long long int) offset,
			  dwarf_errmsg (err));
		  break;
		}

	      if (dwarf_get_abbrev_children_flag (abbrev, &children, &err)
		  != DW_DLV_OK)
		{
		  printf ("dwarf_get_abbrev_children_flag at offset %llu returned error: %s\n",
			  (unsigned long long int) offset,
			  dwarf_errmsg (err));
		  break;
		}

	      printf ("abbrev[%llu]: code = %llu, tag = %hu, children = %lld\n",
		      (unsigned long long int) offset,
		      (unsigned long long int) code, tag,
		      (long long int) children);

	      if (dwarf_get_abbrev_entry (abbrev, -1, &attr_num, &attr_form,
					  &attr_offset, &err)
		  != DW_DLV_NO_ENTRY)
		printf ("dwarf_get_abbrev_entry for abbrev[%llu] and index %d does not return DW_DLV_NO_ENTRY\n",
			(unsigned long long int) offset, -1);

	      for (j = 0; j < attrcnt; ++j)
		if (dwarf_get_abbrev_entry (abbrev, j, &attr_num, &attr_form,
					    &attr_offset, &err) != DW_DLV_OK)
		  printf ("dwarf_get_abbrev_entry for abbrev[%llu] and index %d does not return DW_DLV_OK\n",
			  (unsigned long long int) offset, j);
		else
		  printf ("abbrev[%llu]: attr[%d]: code = %hu, form = %lld, offset = %lld\n",
			  (unsigned long long int) offset, j, attr_num,
			  (long long int) attr_form,
			  (long long int) attr_offset);

	      if (dwarf_get_abbrev_entry (abbrev, j, &attr_num, &attr_form,
					  &attr_offset, &err)
		  != DW_DLV_NO_ENTRY)
		printf ("dwarf_get_abbrev_entry for abbrev[%llu] and index %d does not return DW_DLV_NO_ENTRY\n",
			(unsigned long long int) offset, j);

	      offset += length;
	    }

	  if (dwarf_finish (dbg, &err) != DW_DLV_OK)
	    printf ("dwarf_finish failed for %s: %s\n", argv[cnt],
		    dwarf_errmsg (err));

	  close (fd);
	}
    }

  return 0;
}
