#include <fcntl.h>
#include <libelf.h>
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
      Dwarf_Signed cie_cnt;
      Dwarf_Cie *cie_data;
      Dwarf_Signed fde_cnt;
      Dwarf_Fde *fde_data;
      Dwarf_Error err;

       if (dwarf_init (fd, DW_DLC_READ, NULL, NULL, &dbg, &err) != DW_DLV_OK)
	 {
	   printf ("%s not usable: %s\n", argv[cnt], dwarf_errmsg (err));
	   continue;
	 }
      else if (dwarf_get_fde_list_eh (dbg, &cie_data, &cie_cnt, &fde_data,
				      &fde_cnt, &err) != DW_DLV_OK)
	printf ("cannot get CIEs and FDEs from %s: %s\n", argv[cnt],
		dwarf_errmsg (err));
      else
	{
	  Dwarf_Addr low_pc;
	  Dwarf_Addr high_pc;
	  Dwarf_Unsigned func_length;
	  Dwarf_Ptr fde_bytes;
	  Dwarf_Unsigned fde_byte_length;
	  Dwarf_Off cie_offset;
	  Dwarf_Signed cie_index;
	  Dwarf_Off fde_offset;
	  Dwarf_Fde fde;
	  int i;

	  printf ("%s has %lld CIEs and %lld FDEs\n",
		  argv[cnt], (long long int) cie_cnt, (long long int) fde_cnt);

	  for (i = 0; i < cie_cnt; ++i)
	    {
	      Dwarf_Unsigned bytes_in_cie;
	      Dwarf_Small version;
	      char *augmenter;
	      Dwarf_Unsigned code_alignment_factor;
	      Dwarf_Signed data_alignment_factor;
	      Dwarf_Half return_address_register;
	      Dwarf_Ptr initial_instructions;
	      Dwarf_Unsigned initial_instructions_length;

	      if (dwarf_get_cie_info (cie_data[i], &bytes_in_cie, &version,
				      &augmenter, &code_alignment_factor,
				      &data_alignment_factor,
				      &return_address_register,
				      &initial_instructions,
				      &initial_instructions_length, &err)
		  != DW_DLV_OK)
		printf ("cannot get info for CIE %d: %s\n", i,
			dwarf_errmsg (err));
	      else
		{
		  int j;

		  printf ("CIE[%d]: bytes_in_cie = %llu, version = %hhd, augmenter = \"%s\"\n",
			  i, (unsigned long long int) bytes_in_cie, version,
			  augmenter);
		  printf ("CIE[%d]: code_alignment_factor = %llx\n"
			  "CIE[%d]: data_alignment_factor = %llx\n"
			  "CIE[%d]: return_address_register = %hu\n"
			  "CIE[%d]: bytes =",
			  i, (unsigned long long int) code_alignment_factor,
			  i, (unsigned long long int) data_alignment_factor,
			  i, return_address_register, i);

		  for (j = 0; j < initial_instructions_length; ++j)
		    printf (" %02hhx",
			    ((unsigned char *) initial_instructions)[j]);

		  putchar ('\n');
		}
	    }

	  for (i = 0; i < fde_cnt; ++i)
	    {
	      Dwarf_Cie cie;

	      if (dwarf_get_fde_range (fde_data[i], &low_pc, &func_length,
				       &fde_bytes, &fde_byte_length,
				       &cie_offset, &cie_index, &fde_offset,
				       &err) != DW_DLV_OK)
		printf ("cannot get range of FDE %d: %s\n", i,
			dwarf_errmsg (err));
	      else
		{
		  int j;
		  Dwarf_Ptr instrs;
		  Dwarf_Unsigned len;

		  printf ("FDE[%d]: low_pc = %#llx, length = %llu\n", i,
			  (unsigned long long int) low_pc,
			  (unsigned long long int) func_length);
		  printf ("FDE[%d]: bytes =", i);

		  for (j = 0; j < fde_byte_length; ++j)
		    printf (" %02hhx", ((unsigned char *) fde_bytes)[j]);

		  printf ("\nFDE[%d]: cie_offset = %lld, cie_index = %lld, fde_offset = %lld\n",
			  i, (long long int) cie_offset,
			  (long long int) cie_index,
			  (long long int) fde_offset);

		  if (dwarf_get_fde_instr_bytes (fde_data[i], &instrs, &len,
						 &err) != DW_DLV_OK)
		    printf ("cannot get instructions of FDE %d: %s\n", i,
			    dwarf_errmsg (err));
		  else
		    {
		      printf ("FDE[%d]: instructions =", i);

		      for (j = 0; j < len; ++j)
			printf (" %02hhx", ((unsigned char *) instrs)[j]);

		      putchar ('\n');
		    }

		  /* Consistency check.  */
		  if (dwarf_get_cie_of_fde (fde_data[i], &cie, &err)
		      != DW_DLV_OK)
		    printf ("cannot get CIE of FDE %d: %s\n", i,
			    dwarf_errmsg (err));
		  else if (cie_data[cie_index] != cie)
		    puts ("cie_index for FDE[%d] does not match dwarf_get_cie_of_fde result");
		}

	      if (dwarf_get_fde_n (fde_data, i, &fde, &err) != DW_DLV_OK)
		printf ("dwarf_get_fde_n for FDE[%d] failed\n", i);
	      else if (fde != fde_data[i])
		printf ("dwarf_get_fde_n for FDE[%d] didn't return the right value\n", i);
	    }

	  if (dwarf_get_fde_n (fde_data, fde_cnt, &fde, &err)
	      != DW_DLV_NO_ENTRY)
	    puts ("dwarf_get_fde_n for invalid index doesn't return DW_DLV_NO_ENTRY");

	  {
	    const unsigned int addrs[] =
	      {
		0x8048400, 0x804842c, 0x8048454, 0x8048455, 0x80493fc
	      };
	    const int naddrs = sizeof (addrs) / sizeof (addrs[0]);

	    for (i = 0; i < naddrs; ++i)
	      if (dwarf_get_fde_at_pc (fde_data, addrs[i], &fde, &low_pc,
				       &high_pc, &err) != DW_DLV_OK)
		printf ("no FDE at %x\n", addrs[i]);
	      else
		{
		  Dwarf_Addr other_low_pc;

		  if (dwarf_get_fde_range (fde, &other_low_pc, &func_length,
					   &fde_bytes, &fde_byte_length,
					   &cie_offset, &cie_index,
					   &fde_offset, &err) != DW_DLV_OK)
		    printf ("cannot get range of FDE returned by dwarf_get_fde_at_pc for %u: %s\n",
			    addrs[i], dwarf_errmsg (err));
		  else
		    {
		      printf ("FDE[@%x]: cie_offset = %lld, cie_index = %lld, fde_offset = %lld\n",
			      addrs[i],
			      (long long int) cie_offset,
			      (long long int) cie_index,
			      (long long int) fde_offset);

		      if (low_pc != other_low_pc)
			printf ("low_pc returned by dwarf_get_fde_at_pc for %x and dwarf_get_fde_range differs",
				addrs[i]);

		      if (high_pc != low_pc + func_length - 1)
			printf ("high_pc returned by dwarf_get_fde_at_pc for %x and dwarf_get_fde_range differs",
				addrs[i]);
		    }
		}
	  }
	}

      if (dwarf_finish (dbg, &err) != DW_DLV_OK)
	printf ("dwarf_finish failed for %s: %s\n", argv[cnt],
		dwarf_errmsg (err));

      close (fd);
    }

  return 0;
}
