#include <fcntl.h>
#include <libdwarf.h>
#include <libelf.h>
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
      Dwarf_Unsigned cuhl;
      Dwarf_Half v;
      Dwarf_Unsigned o;
      Dwarf_Half sz;
      Dwarf_Unsigned ncu;

      if (dwarf_init (fd, DW_DLC_READ, NULL, NULL, &dbg, NULL) != DW_DLV_OK)
	{
	  printf ("%s not usable\n", argv[cnt]);
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
	      puts ("no CU die found");
	      continue;
	    }
	}

      dwarf_finish (dbg, NULL);
      close (fd);
    }

  return 0;
}
