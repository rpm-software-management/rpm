/* Internal definitions for interface for libebl.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.

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

#ifndef _LIBEBLP_H
#define _LIBEBLP_H 1

#include <gelf.h>
#include <libebl.h>
#include <libintl.h>


/* Type of the initialization functions in the backend modules.  */
typedef int (*ebl_bhinit_t) (Elf *, GElf_Half, Ebl *, size_t);


/* gettext helper macros.  */
#define _(Str) dgettext ("libebl", Str)

#endif	/* libeblP.h */
