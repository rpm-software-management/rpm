#include <fcntl.h>
#include <libdwarf.h>
#include <libelf.h>
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
      int nr = 0;

      if (dwarf_init (fd, DW_DLC_READ, NULL, NULL, &dbg, NULL) != DW_DLV_OK)
	{
	  printf ("%s not usable\n", argv[cnt]);
	  result = 1;
	  close (fd);
	  continue;
	}

      while (dwarf_next_cu_header (dbg, &cuhl, &v, &o, &sz, &ncu, NULL) == DW_DLV_OK)
	{
	  printf ("cuhl = %llu, v = %hu, o = %llu, sz = %hu, ncu = %llu\n",
		  (unsigned long long int) cuhl, v, (unsigned long long int) o,
		  sz, (unsigned long long int) ncu);
	  ++nr;
	}

      if (nr == 0)
	{
	  printf ("%s: no CUs found\n", argv[cnt]);
	  result = 1;
	}

      dwarf_finish (dbg, NULL);
      close (fd);
    }

  return result;
}
