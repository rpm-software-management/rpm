#include <fcntl.h>
#include <libelf.h>
#include <libdwarf.h>
#include <stdio.h>
#include <unistd.h>


static const Dwarf_Addr testaddr[] =
{
  0x804842b, 0x804842c, 0x804843c, 0x8048459, 0x804845a,
  0x804845b, 0x804845c, 0x8048460, 0x8048465, 0x8048466,
  0x8048467, 0x8048468, 0x8048470, 0x8048471, 0x8048472
};
#define ntestaddr (sizeof (testaddr) / sizeof (testaddr[0]))


int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);
      Dwarf_Debug dbg;
      Dwarf_Arange *aranges;
      Dwarf_Signed naranges;
      int res;

      if (dwarf_init (fd, DW_DLC_READ, NULL, NULL, &dbg, NULL) != DW_DLV_OK)
	{
	  printf ("%s not usable\n", argv[cnt]);
	  result = 1;
	  close (fd);
	  continue;
	}

      res = dwarf_get_aranges (dbg, &aranges, &naranges, NULL);
      if (res == DW_DLV_NO_ENTRY)
	printf ("%s: no aranges\n", argv[cnt]);
      else if (res == DW_DLV_ERROR)
	{
	  printf ("%s: cannot get aranges\n", argv[cnt]);
	  result = 1;
	}
      else
	{
	  int i;

	  for (i = 0; i < ntestaddr; ++i)
	    {
	      Dwarf_Arange found;
	      int res;

	      res = dwarf_get_arange (aranges, naranges, testaddr[i],
				      &found, NULL);
	      if (res == DW_DLV_OK)
		{
		  Dwarf_Off cu_offset;

		  if (dwarf_get_cu_die_offset (found, &cu_offset, NULL)
		      != DW_DLV_OK)
		    {
		      puts ("failed to get CU die offset");
		      result = 1;
		    }
		  else
		    {
		      char *cuname;
		      Dwarf_Die cu_die;

		      if (dwarf_offdie (dbg, cu_offset, &cu_die, NULL)
			  != DW_DLV_OK
			  || (dwarf_diename (cu_die, &cuname, NULL)
			      != DW_DLV_OK))
			{
			  puts ("failed to get CU die");
			  result = 1;
			}
		      else
			{
			  printf ("CU name: \"%s\"\n", cuname);
			  dwarf_dealloc (dbg, cuname, DW_DLA_STRING);
			}
		    }
		}
	      else if (res == DW_DLV_NO_ENTRY)
		printf ("%#llx: not in range\n",
			(unsigned long long int) testaddr[i]);
	      else
		{
		  printf ("%#llx: failed\n",
			  (unsigned long long int) testaddr[i]);
		  result = 1;
		}
	    }

	  for (i = 0; i < naranges; ++i)
	    {
	      Dwarf_Addr start;
	      Dwarf_Unsigned length;
	      Dwarf_Off cu_offset;

	      if (dwarf_get_arange_info (aranges[i], &start, &length,
					 &cu_offset, NULL) != DW_DLV_OK)
		{
		  printf ("cannot get info from aranges[%d]\n", i);
		  result = 1;
		}
	      else
		{
		  char *cuname;
		  Dwarf_Die cu_die;

		  printf (" [%2d] start: %#llx, length: %llu, cu: %llu\n",
			  i, (unsigned long long int) start,
			  (unsigned long long int) length,
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
		}

	      dwarf_dealloc (dbg, aranges[i], DW_DLA_ARANGE);
	    }

	  dwarf_dealloc (dbg, aranges, DW_DLA_LIST);
	}

      dwarf_finish (dbg, NULL);
      close (fd);
    }

  return result;
}
