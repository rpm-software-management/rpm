#include <fcntl.h>
#include <libelf.h>
#include <libdwarf.h>
#include <stdio.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);
      Dwarf_Debug dbg;
      Dwarf_Global *globals;
      Dwarf_Signed nglobals;
      int res;

      if (dwarf_init (fd, DW_DLC_READ, NULL, NULL, &dbg, NULL) != DW_DLV_OK)
	{
	  printf ("%s not usable\n", argv[cnt]);
	  result = 1;
	  close (fd);
	  continue;
	}

      res = dwarf_get_globals (dbg, &globals, &nglobals, NULL);
      if (res == DW_DLV_NO_ENTRY)
	printf ("%s: no pubnames\n", argv[cnt]);
      else if (res == DW_DLV_ERROR)
	{
	  printf ("%s: cannot get pubnames\n", argv[cnt]);
	  result = 1;
	}
      else
	{
	  int i;

	  for (i = 0; i < nglobals; ++i)
	    {
	      char *name;
	      Dwarf_Off die_offset;
	      Dwarf_Off cu_offset;

	      if (dwarf_global_name_offsets (globals[i], &name, &die_offset,
					     &cu_offset, NULL) != DW_DLV_OK)
		{
		  printf ("cannot get info from globals[%d]\n", i);
		  result = 1;
		}
	      else
		{
		  char *cuname;
		  char *diename;
		  Dwarf_Die cu_die;
		  Dwarf_Die die;

		  printf (" [%2d] \"%s\", die: %llu, cu: %llu\n",
			  i, name, (unsigned long long int) die_offset,
			  (unsigned long long int) cu_offset);

		  if (dwarf_offdie (dbg, cu_offset, &cu_die, NULL)
		      != DW_DLV_OK
		      || dwarf_diename (cu_die, &cuname, NULL) != DW_DLV_OK)
		    {
		      puts ("failed to get CU die");
		      result = 1;
		    }
		  else
		    {
		      printf ("CU name: \"%s\"\n", cuname);
		      dwarf_dealloc (dbg, cuname, DW_DLA_STRING);
		    }

		  if (dwarf_offdie (dbg, die_offset, &die, NULL) != DW_DLV_OK
		      || dwarf_diename (die, &diename, NULL) != DW_DLV_OK)
		    {
		      puts ("failed to get object die");
		      result = 1;
		    }
		  else
		    {
		      printf ("object name: \"%s\"\n", diename);
		      dwarf_dealloc (dbg, diename, DW_DLA_STRING);
		    }
		}

	      dwarf_dealloc (dbg, globals[i], DW_DLA_GLOBAL);
	    }

	  dwarf_dealloc (dbg, globals, DW_DLA_LIST);
	}

      dwarf_finish (dbg, NULL);
      close (fd);
    }

  return result;
}
