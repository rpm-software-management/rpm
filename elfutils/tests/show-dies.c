#include <fcntl.h>
#include <libelf.h>
#include <libdwarf.h>
#include <stdio.h>
#include <unistd.h>

void
handle (Dwarf_Debug dbg, Dwarf_Die die, int n)
{
  Dwarf_Die child;
  printf ("%*sdie\n", n * 5, "");
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
