/* Coordinate ELF library and application versions.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libelfP.h>


/* Is the version initialized?  */
int __libelf_version_initialized;

/* Currently selected version.  */
unsigned int __libelf_version = EV_CURRENT;


/*@-mods@*/
unsigned int
elf_version (unsigned int version)
{
  if (version == EV_NONE)
    return __libelf_version;

  if (likely (version < EV_NUM))
    {
      /* Phew, we know this version.  */
      unsigned int last_version = __libelf_version;

      /* Store the new version.  */
      __libelf_version = version;

      /* Signal that the version is now initialized.  */
      __libelf_version_initialized = 1;

      /* And return the last version.  */
      return last_version;
    }

  /* We cannot handle this version.  */
  __libelf_seterrno (ELF_E_UNKNOWN_VERSION);
  return EV_NONE;
}
/*@=mods@*/
INTDEF(elf_version)
