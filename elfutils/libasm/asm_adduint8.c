/* Add unsigned integer to a section.
   Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libasmP.h>

#ifndef SIZE
# define SIZE 8
#endif

#define UFCT(size) _UFCT(size)
#define _UFCT(size) asm_adduint##size
#define FCT(size) _FCT(size)
#define _FCT(size) asm_addint##size
#define UTYPE(size) _UTYPE(size)
#define _UTYPE(size) uint##size##_t
#define TYPE(size) _TYPE(size)
#define _TYPE(size) int##size##_t


int
UFCT(SIZE) (asmscn, num)
     AsmScn_t *asmscn;
     UTYPE(SIZE) num;
{
  return INTUSE(FCT(SIZE)) (asmscn, (TYPE(SIZE)) num);
}
