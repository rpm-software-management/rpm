/* Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#include <assert.h>
#include <stdlib.h>
#include <system.h>

/* Before including this file the following macros must be defined:

   NAME      name of the hash table structure.
   TYPE      data type of the hash table entries
   COMPARE   comparison function taking two pointers to TYPE objects

   The following macros if present select features:

   ITERATE   iterating over the table entries is possible
   REVERSE   iterate in reverse order of insert
 */


static size_t
lookup (NAME *htab, unsigned long int hval, /*@null@*/ TYPE val)
	/*@*/
{
  /* First hash function: simply take the modul but prevent zero.  */
  size_t idx = 1 + hval % htab->size;

  if (htab->table[idx].hashval != 0)
    {
      unsigned long int hash;

      if (htab->table[idx].hashval == hval
	  && COMPARE (htab->table[idx].data, val) == 0)
	return idx;

      /* Second hash function as suggested in [Knuth].  */
      hash = 1 + hval % (htab->size - 2);

      do
	{
	  if (idx <= hash)
	    idx = htab->size + idx - hash;
	  else
	    idx -= hash;

	  /* If entry is found use it.  */
	  if (htab->table[idx].hashval == hval
	      && COMPARE (htab->table[idx].data, val) == 0)
	    return idx;
	}
      while (htab->table[idx].hashval);
    }
  return idx;
}


static void
insert_entry_2 (NAME *htab, unsigned long int hval, size_t idx, TYPE data)
	/*@modifies htab @*/
{
#ifdef ITERATE
  if (htab->table[idx].hashval == 0)
    {
# ifdef REVERSE
      htab->table[idx].next = htab->first;
      htab->first = &htab->table[idx];
# else
      /* Add the new value to the list.  */
      if (htab->first == NULL)
	htab->first = htab->table[idx].next = &htab->table[idx];
      else
	{
	  htab->table[idx].next = htab->first->next;
	  htab->first = htab->first->next = &htab->table[idx];
	}
# endif
    }
#endif

  htab->table[idx].hashval = hval;
  htab->table[idx].data = data;

  ++htab->filled;
  if (100 * htab->filled > 90 * htab->size)
    {
      /* Table is filled more than 90%.  Resize the table.  */
#ifdef ITERATE
      __typeof__ (htab->first) first;
# ifndef REVERSE
      __typeof__ (htab->first) runp;
# endif
#else
      unsigned long int old_size = htab->size;
#endif
#define _TABLE(name) \
      name##_ent *table = htab->table
#define TABLE(name) _TABLE (name)
      TABLE(NAME);

      htab->size = next_prime (htab->size * 2);
      htab->filled = 0;
#ifdef ITERATE
      first = htab->first;
      htab->first = NULL;
#endif
      htab->table = calloc ((1 + htab->size), sizeof (htab->table[0]));
      if (htab->table == NULL)
	{
	  /* We cannot enlarge the table.  Live with what we got.  This
	     might lead to an infinite loop at some point, though.  */
	  htab->table = table;
	  return;
	}

      /* Add the old entries to the new table.  When iteration is
	 supported we maintain the order.  */
#ifdef ITERATE
# ifdef REVERSE
      while (first != NULL)
	{
	  insert_entry_2 (htab, first->hashval,
			  lookup (htab, first->hashval, first->data),
			  first->data);

	  first = first->next;
	}
# else
      assert (first != NULL);
      runp = first = first->next;
      do
	insert_entry_2 (htab, runp->hashval,
			lookup (htab, runp->hashval, runp->data), runp->data);
      while ((runp = runp->next) != first);
# endif
#else
      for (idx = 1; idx <= old_size; ++idx)
	if (table[idx].hashval != 0)
	  insert_entry_2 (htab, table[idx].hashval,
			  lookup (htab, table[idx].hashval, table[idx].data),
			  table[idx].data);
#endif

      free (table);
    }
}


int
#define INIT(name) _INIT (name)
#define _INIT(name) \
  name##_init
INIT(NAME) (NAME *htab, unsigned long int init_size)
{
  /* We need the size to be a prime.  */
  init_size = next_prime (init_size);

  /* Initialize the data structure.  */
  htab->size = init_size;
  htab->filled = 0;
#ifdef ITERATE
  htab->first = NULL;
#endif
  htab->table = (void *) calloc ((init_size + 1), sizeof (htab->table[0]));
  if (htab->table == NULL)
    return -1;

  return 0;
}


int
#define FREE(name) _FREE (name)
#define _FREE(name) \
  name##_free
FREE(NAME) (NAME *htab)
{
  free (htab->table);
  return 0;
}


int
#define INSERT(name) _INSERT (name)
#define _INSERT(name) \
  name##_insert
INSERT(NAME) (NAME *htab, unsigned long int hval, TYPE data)
{
  size_t idx;

  /* Make the hash value nonzero.  */
  hval = hval ? hval : 1;

  idx = lookup (htab, hval, data);

  if (htab->table[idx].hashval != 0)
    /* We don't want to overwrite the old value.  */
    return -1;

  /* An empty bucket has been found.  */
  insert_entry_2 (htab, hval, idx, data);
  return 0;
}


#ifdef OVERWRITE
int
#define INSERT(name) _INSERT (name)
#define _INSERT(name) \
  name##_overwrite
INSERT(NAME) (NAME *htab, unsigned long int hval, TYPE data)
{
  size_t idx;

  /* Make the hash value nonzero.  */
  hval = hval ? hval : 1;

  idx = lookup (htab, hval, data);

  /* The correct bucket has been found.  */
  insert_entry_2 (htab, hval, idx, data);
  return 0;
}
#endif


TYPE
#define FIND(name) _FIND (name)
#define _FIND(name) \
  name##_find
FIND(NAME) (NAME *htab, unsigned long int hval, TYPE val)
{
  size_t idx;

  /* Make the hash value nonzero.  */
  hval = hval ? hval : 1;

  idx = lookup (htab, hval, val);

  if (htab->table[idx].hashval == 0)
    return NULL;

  return htab->table[idx].data;
}


#ifdef ITERATE
# define ITERATEFCT(name) _ITERATEFCT (name)
# define _ITERATEFCT(name) \
  name##_iterate
TYPE
ITERATEFCT(NAME) (NAME *htab, void **ptr)
{
# define TYPENAME(name) _TYPENAME (name)
# define _TYPENAME(name) \
  name##_ent
# ifdef REVERSE
  if (*ptr == NULL)
    *ptr = htab->first;
  else
    *ptr = ((TYPENAME(NAME) *) *ptr)->next;

  if (*ptr == NULL)
    return NULL;
# else
  if (*ptr == NULL)
    {
      if (htab->first == NULL)
	return NULL;
      *ptr = htab->first->next;
    }
  else
    {
      if (*ptr == htab->first)
	return NULL;
      *ptr = ((TYPENAME(NAME) *) *ptr)->next;
    }
# endif

  return ((TYPENAME(NAME) *) *ptr)->data;
}
#endif
