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
      Dwarf_Line *lb;
      Dwarf_Signed nlb;
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

	  res = dwarf_srclines (die, &lb, &nlb, NULL);
	  if (res == DW_DLV_ERROR)
	    {
	      printf ("%s: cannot get lines\n", argv[cnt]);
	      result = 1;
	      close (fd);
	      continue;
	    }

	  if (res == DW_DLV_OK)
	    {
	      int i;

	      printf (" %lld lines\n", (long long int) nlb);

	      for (i = 0; i < nlb; ++i)
		{
		  Dwarf_Unsigned addr;
		  Dwarf_Unsigned line;
		  char *file;
		  Dwarf_Signed column;
		  Dwarf_Bool is_stmt;
		  Dwarf_Bool end_sequence;
		  Dwarf_Bool basic_block;

		  if (dwarf_lineaddr (lb[i], &addr, NULL) != DW_DLV_OK)
		    addr = 0;
		  if (dwarf_linesrc (lb[i], &file, NULL) != DW_DLV_OK)
		    file = NULL;
		  if (dwarf_lineno (lb[i], &line, NULL) != DW_DLV_OK)
		    line = 0;

		  printf ("%llx: %s:%llu:", (unsigned long long int) addr,
			  file ?: "???", (unsigned long long int) line);

		  if (dwarf_lineoff (lb[i], &column, NULL) != DW_DLV_OK)
		    line = 0;
		  if (column >= 0)
		    printf ("%lld:", (long long int) column);

		  if (dwarf_linebeginstatement (lb[i], &is_stmt, NULL)
		      != DW_DLV_OK)
		    is_stmt = 2;
		  if (dwarf_lineendsequence (lb[i], &end_sequence, NULL)
		      != DW_DLV_OK)
		    end_sequence = 2;
		  if (dwarf_lineblock (lb[i], &basic_block, NULL) != DW_DLV_OK)
		    basic_block = 2;

		  printf (" is_stmt:%d, end_seq:%d, bb: %d",
			  (int) is_stmt, (int) end_sequence,
			  (int) basic_block);

		  puts ("");

		  if (file != NULL)
		    dwarf_dealloc (dbg, file, DW_DLA_STRING);

		  dwarf_dealloc (dbg, lb[i], DW_DLA_LINE);
		}
	      dwarf_dealloc (dbg, lb, DW_DLA_LIST);
	    }
	}

      dwarf_finish (dbg, NULL);
      close (fd);
    }

  return result;
}
