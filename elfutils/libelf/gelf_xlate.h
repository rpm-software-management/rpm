/* Helper file for type conversion function generation.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* Simple types.  */
FUNDAMENTAL (ADDR, Addr, LIBELFBITS);
FUNDAMENTAL (OFF, Off, LIBELFBITS);
FUNDAMENTAL (HALF, Half, LIBELFBITS);
FUNDAMENTAL (WORD, Word, LIBELFBITS);
FUNDAMENTAL (SWORD, Sword, LIBELFBITS);
FUNDAMENTAL (XWORD, Xword, LIBELFBITS);
FUNDAMENTAL (SXWORD, Sxword, LIBELFBITS);

/* The strctured types.  */
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


/* Prepare for the next round.  */
#undef LIBELFBITS
