/* Helper file for type conversion function generation.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */


/* Simple types.  */
/*@-mods@*/
FUNDAMENTAL (ADDR, Addr, LIBELFBITS);
FUNDAMENTAL (OFF, Off, LIBELFBITS);
FUNDAMENTAL (HALF, Half, LIBELFBITS);
FUNDAMENTAL (WORD, Word, LIBELFBITS);
FUNDAMENTAL (SWORD, Sword, LIBELFBITS);
FUNDAMENTAL (XWORD, Xword, LIBELFBITS);
FUNDAMENTAL (SXWORD, Sxword, LIBELFBITS);
/*@=mods@*/

/* The strctured types.  */
/*@-modunconnomods -noeffectuncon@*/
TYPE (Ehdr, LIBELFBITS)
TYPE (Phdr, LIBELFBITS)
TYPE (Shdr, LIBELFBITS)
TYPE (Sym, LIBELFBITS)
TYPE (Rel, LIBELFBITS)
TYPE (Rela, LIBELFBITS)
TYPE (Note, LIBELFBITS)
TYPE (Dyn, LIBELFBITS)
TYPE (Syminfo, LIBELFBITS)
TYPE (Move, LIBELFBITS)
/*@=modunconnomods =noeffectuncon@*/


/* Prepare for the next round.  */
#undef LIBELFBITS
