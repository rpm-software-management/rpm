#include <libasm.h>
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include <system.h>


static const char fname[] = "asm-tst6-out.o";


int
main (void)
{
  AsmCtx_t *ctx;
  int result = 0;
  size_t cnt;

  elf_version (EV_CURRENT);

  ctx = asm_begin (fname, false, EM_386, ELFCLASS32, ELFDATA2LSB);
  if (ctx == NULL)
    {
      printf ("cannot create assembler context: %s\n", asm_errmsg (-1));
      return 1;
    }

  for (cnt = 0; cnt < 22000; ++cnt)
    {
      char buf[512];
      AsmScnGrp_t *grp;
      AsmScn_t *scn;
      AsmSym_t *sym;

      snprintf (buf, sizeof (buf), ".grp%Zu", cnt);
      grp = asm_newscngrp (ctx, buf, NULL, 0);
      if (grp == NULL)
	{
	  printf ("cannot section group %Zu: %s\n", cnt, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      scn = asm_newscn_ingrp (ctx, ".data", SHT_PROGBITS,
			      SHF_ALLOC | SHF_WRITE, grp);
      if (scn == NULL)
	{
	  printf ("cannot data section for group %Zu: %s\n",
		  cnt, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      /* Add a name.  */
      snprintf (buf, sizeof (buf), "%Zu", cnt);
      sym = asm_newsym (scn, xstrdup (buf), sizeof (uint32_t), STT_OBJECT,
			STB_GLOBAL);
      if (sym == NULL)
	{
	  printf ("cannot create symbol \"%s\": %s\n", buf, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      /* Add some content.  */
      if (asm_adduint32 (scn, cnt) != 0)
	{
	  printf ("cannot create content of section \"%s\": %s\n",
		  buf, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      /* Now we have a symbol, use it as the signature.  */
      if (asm_scngrp_newsignature (grp, sym) != 0)
	{
	  printf ("cannot set signature for section group %Zu: %s\n",
		  cnt, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      /* Create a phony debug info section.  */
      scn = asm_newscn_ingrp (ctx, ".stab", SHT_PROGBITS, 0, grp);
      if (scn == NULL)
	{
	  printf ("cannot stab section for group %Zu: %s\n",
		  cnt, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      /* Add some content.  */
      if (asm_adduint32 (scn, cnt) != 0)
	{
	  printf ("cannot create content of section \"%s\": %s\n",
		  buf, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}
    }

  /* Create the output file.  */
  if (asm_end (ctx) != 0)
    {
      printf ("cannot create output file: %s\n", asm_errmsg (-1));
      asm_abort (ctx);
      return 1;
    }

  if (result == 0)
    result = WEXITSTATUS (system ("../src/elflint -q asm-tst6-out.o"));

  /* We don't need the file anymore.  */
  unlink (fname);

  return result;
}
