/* Print contents of object file note.
   Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <libeblP.h>


void
ebl_object_note (ebl, name, type, descsz, desc)
     Ebl *ebl;
     const char *name;
     uint32_t type;
     uint32_t descsz;
     const char *desc;
{
  if (! ebl->object_note (name, type, descsz, desc))
    /* The machine specific function did not know this type.  */
    switch (type)
      {
      case NT_VERSION:
	if (strcmp (name, "GNU") == 0 && descsz >= 8)
	  {
	    struct
	    {
	      uint32_t os;
	      uint32_t version[descsz / 4 - 1];
	    } *tag = (__typeof (tag)) desc;
	    size_t cnt;

	    const char *os;
	    switch (tag->os)
	      {
	      case ELF_NOTE_OS_LINUX:
		os = "Linux";
		break;

	      case ELF_NOTE_OS_GNU:
		os = "GNU";
		break;

	      case ELF_NOTE_OS_SOLARIS2:
		os = "Solaris";
		break;

	      case ELF_NOTE_OS_FREEBSD:
		os = "FreeBSD";
		break;

	      default:
		os = "???";
		break;
	      }

	    printf (gettext ("    OS: %s, ABI: "), os);
	    for (cnt = 0; cnt < descsz / 4 - 1; ++cnt)
	      {
		if (cnt != 0)
		  putchar_unlocked ('.');
		printf ("%" PRIu32, tag->version[cnt]);
	      }
	    putchar_unlocked ('\n');
	    break;
	  }
	/* FALLTHROUGH */

      default:
	/* Unknown type.  */
	break;
      }
}
