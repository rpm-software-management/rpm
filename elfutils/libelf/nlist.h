/* Interface for nlist.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.

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

#ifndef _NLIST_H
#define _NLIST_H 1


/* Symbol list type.  */
struct nlist
{
  char *n_name;			/* Symbol name.  */
  long int n_value;		/* Value of symbol.  */
  short int n_scnum;		/* Section number found in.  */
  unsigned short int n_type;	/* Type of symbol.  */
  char n_sclass;		/* Storage class.  */
  char n_numaux;		/* Number of auxiliary entries.  */
};


#ifdef __cplusplus
extern "C" {
#endif

/* Get specified entries from file.  */
extern int nlist (__const char *__filename, struct nlist *__nl);

#ifdef __cplusplus
}
#endif

#endif  /* nlist.h */
