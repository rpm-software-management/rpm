/* Interface for nlist.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

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
extern int nlist (__const char *__filename, struct nlist *nl)
	/*@modifies *nl @*/;

#ifdef __cplusplus
}
#endif

#endif  /* nlist.h */
