/* Determine prime number.
   Copyright (C) 2000, 2002 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <stddef.h>


/* Test whether CANDIDATE is a prime.  */
static int
is_prime (size_t candidate)
{
  /* No even number and none less than 10 will be passed here.  */
  size_t divn = 3;
  size_t sq = divn * divn;

  while (sq < candidate && candidate % divn != 0)
    {
      size_t old_sq = sq;
      ++divn;
      sq += 4 * divn;
      if (sq < old_sq)
	return 1;
      ++divn;
    }

  return candidate % divn != 0;
}


/* We need primes for the table size.  */
size_t
next_prime (size_t seed)
{
  /* Make it definitely odd.  */
  seed |= 1;

  while (!is_prime (seed))
    seed += 2;

  return seed;
}
