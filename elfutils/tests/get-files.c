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
      Dwarf_Unsigned cuhl;
      Dwarf_Half v;
      Dwarf_Unsigned o;
      Dwarf_Half sz;
      Dwarf_Unsigned ncu;
      char **files;
      Dwarf_Signed nfiles;
      int res;

      if (dwarf_init (fd, DW_DLC_READ, NULL, NULL, &dbg, NULL) != DW_DLV_OK)
	{
	  printf ("%s not usable\n", argv[cnt]);
	  result = 1;
	  close (fd);
	  continue;
	}

      while (dwarf_next_cu_header (dbg, &cuhl, &v, &o, &sz, &ncu, NULL)
	     == DW_DLV_OK)
	{
	  Dwarf_Die die;

	  printf ("cuhl = %llu, v = %hu, o = %llu, sz = %hu, ncu = %llu\n",
		  (unsigned long long int) cuhl, v, (unsigned long long int) o,
		  sz, (unsigned long long int) ncu);

	  if (dwarf_siblingof (dbg, NULL, &die, NULL) != DW_DLV_OK)
	    {
	      printf ("%s: cannot get CU die\n", argv[cnt]);
	      result = 1;
	      close (fd);
	      continue;
	    }

	  res = dwarf_srcfiles (die, &files, &nfiles, NULL);
	  if (res == DW_DLV_ERROR)
	    {
	      printf ("%s: cannot get files\n", argv[cnt]);
	      result = 1;
	      close (fd);
	      continue;
	    }

	  if (res == DW_DLV_OK)
	    {
	      int i;

	      for (i = 0; i < nfiles; ++i)
		{
		  printf (" file[%d] = \"%s\"\n", i, files[i]);
		  dwarf_dealloc (dbg, files[i], DW_DLA_STRING);
		}

	      dwarf_dealloc (dbg, files, DW_DLA_LIST);
	    }
	}

      dwarf_finish (dbg, NULL);
      close (fd);
    }

  return result;
}
