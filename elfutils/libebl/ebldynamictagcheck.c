/* Check dynamic tag.
   Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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

#include <inttypes.h>
#include <libeblP.h>


bool
ebl_dynamic_tag_check (ebl, tag)
     Ebl *ebl;
     int64_t tag;
{
  bool res = ebl != NULL ? ebl->dynamic_tag_check (tag) : false;

  if (!res
      && ((tag >= 0 && tag < DT_NUM)
	  || (tag >= DT_GNU_PRELINKED && tag <= DT_SYMINENT)
	  || (tag >= DT_GNU_CONFLICT && tag <= DT_SYMINFO)
	  || tag == DT_VERSYM
	  || (tag >= DT_RELACOUNT && tag <= DT_VERNEEDNUM)
	  || tag == DT_AUXILIARY
	  || tag == DT_FILTER))
    res = true;

  return res;
}
