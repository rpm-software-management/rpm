/* Copyright (C) 1991-1993, 1996-1999, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

# include "system.h"

/* Find the first occurrence of C in S or the final NUL byte.  */
static inline char *
__strchrnul (s, c)
     const char *s;
     int c;
{
  const unsigned char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, magic_bits, charmask;

  c = (unsigned char) c;

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a longword boundary.  */
  for (char_ptr = s; ((unsigned long int) char_ptr
		      & (sizeof (longword) - 1)) != 0;
       ++char_ptr)
    if (*char_ptr == c || *char_ptr == '\0')
      return (void *) char_ptr;

  /* All these elucidatory comments refer to 4-byte longwords,
     but the theory applies equally well to 8-byte longwords.  */

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
     the "holes."  Note that there is a hole just to the left of
     each byte, with an extra at the end:

     bits:  01111110 11111110 11111110 11111111
     bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

     The 1-bits make sure that carries propagate to the next 0-bit.
     The 0-bits provide holes for carries to fall into.  */
  switch (sizeof (longword))
    {
    case 4: magic_bits = 0x7efefeffL; break;
    case 8: magic_bits = ((0x7efefefeL << 16) << 16) | 0xfefefeffL; break;
    default:
      abort ();
    }

  /* Set up a longword, each of whose bytes is C.  */
  charmask = c | (c << 8);
  charmask |= charmask << 16;
  if (sizeof (longword) > 4)
    /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
    charmask |= (charmask << 16) << 16;
  if (sizeof (longword) > 8)
    abort ();

  /* Instead of the traditional loop which tests each character,
     we will test a longword at a time.  The tricky part is testing
     if *any of the four* bytes in the longword in question are zero.  */
  for (;;)
    {
      /* We tentatively exit the loop if adding MAGIC_BITS to
	 LONGWORD fails to change any of the hole bits of LONGWORD.

	 1) Is this safe?  Will it catch all the zero bytes?
	 Suppose there is a byte with all zeros.  Any carry bits
	 propagating from its left will fall into the hole at its
	 least significant bit and stop.  Since there will be no
	 carry from its most significant bit, the LSB of the
	 byte to the left will be unchanged, and the zero will be
	 detected.

	 2) Is this worthwhile?  Will it ignore everything except
	 zero bytes?  Suppose every byte of LONGWORD has a bit set
	 somewhere.  There will be a carry into bit 8.  If bit 8
	 is set, this will carry into bit 16.  If bit 8 is clear,
	 one of bits 9-15 must be set, so there will be a carry
	 into bit 16.  Similarly, there will be a carry into bit
	 24.  If one of bits 24-30 is set, there will be a carry
	 into bit 31, so all of the hole bits will be changed.

	 The one misfire occurs when bits 24-30 are clear and bit
	 31 is set; in this case, the hole at bit 31 is not
	 changed.  If we had access to the processor carry flag,
	 we could close this loophole by putting the fourth hole
	 at bit 32!

	 So it ignores everything except 128's, when they're aligned
	 properly.

	 3) But wait!  Aren't we looking for C as well as zero?
	 Good point.  So what we do is XOR LONGWORD with a longword,
	 each of whose bytes is C.  This turns each byte that is C
	 into a zero.  */

      longword = *longword_ptr++;

      /* Add MAGIC_BITS to LONGWORD.  */
      if ((((longword + magic_bits)

	    /* Set those bits that were unchanged by the addition.  */
	    ^ ~longword)

	   /* Look at only the hole bits.  If any of the hole bits
	      are unchanged, most likely one of the bytes was a
	      zero.  */
	   & ~magic_bits) != 0 ||

	  /* That caught zeroes.  Now test for C.  */
	  ((((longword ^ charmask) + magic_bits) ^ ~(longword ^ charmask))
	   & ~magic_bits) != 0)
	{
	  /* Which of the bytes was C or zero?
	     If none of them were, it was a misfire; continue the search.  */

	  const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

	  if (*cp == c || *cp == '\0')
	    return (char *) cp;
	  if (*++cp == c || *cp == '\0')
	    return (char *) cp;
	  if (*++cp == c || *cp == '\0')
	    return (char *) cp;
	  if (*++cp == c || *cp == '\0')
	    return (char *) cp;
	  if (sizeof (longword) > 4)
	    {
	      if (*++cp == c || *cp == '\0')
		return (char *) cp;
	      if (*++cp == c || *cp == '\0')
		return (char *) cp;
	      if (*++cp == c || *cp == '\0')
		return (char *) cp;
	      if (*++cp == c || *cp == '\0')
		return (char *) cp;
	    }
	}
    }

  /* This should never happen.  */
  return NULL;
}

/* For platform which support the ISO C amendement 1 functionality we
   support user defined character classes.  */
#if defined _LIBC || (defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H)
/* Solaris 2.5 has a bug: <wchar.h> must be included before <wctype.h>.  */
# include <wchar.h>
# include <wctype.h>
#endif

/* Comment out all this code if we are using the GNU C Library, and are not
   actually compiling the library itself.  This code is part of the GNU C
   Library, but also included in many other GNU distributions.  Compiling
   and linking in this code is a waste when using the GNU C library
   (especially if it is a shared library).  Rather than having every GNU
   program understand `configure --with-gnu-libc' and omit the object files,
   it is simpler to just do this in the source for each such file.  */

#if defined _LIBC || !defined __GNU_LIBRARY__


# if defined STDC_HEADERS || !defined isascii
#  define ISASCII(c) 1
# else
#  define ISASCII(c) isascii(c)
# endif

#ifdef isblank
# define ISBLANK(c) (ISASCII (c) && isblank (c))
#else
# define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#endif
#ifdef isgraph
# define ISGRAPH(c) (ISASCII (c) && isgraph (c))
#else
# define ISGRAPH(c) (ISASCII (c) && isprint (c) && !isspace (c))
#endif

#define ISPRINT(c) (ISASCII (c) && isprint (c))
#define ISDIGIT(c) (ISASCII (c) && isdigit (c))
#define ISALNUM(c) (ISASCII (c) && isalnum (c))
#define ISALPHA(c) (ISASCII (c) && isalpha (c))
#define ISCNTRL(c) (ISASCII (c) && iscntrl (c))
#define ISLOWER(c) (ISASCII (c) && islower (c))
#define ISPUNCT(c) (ISASCII (c) && ispunct (c))
#define ISSPACE(c) (ISASCII (c) && isspace (c))
#define ISUPPER(c) (ISASCII (c) && isupper (c))
#define ISXDIGIT(c) (ISASCII (c) && isxdigit (c))

# define STREQ(s1, s2) ((strcmp (s1, s2) == 0))

# if defined _LIBC || (defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H)
/* The GNU C library provides support for user-defined character classes
   and the functions from ISO C amendement 1.  */
#  ifdef CHARCLASS_NAME_MAX
#   define CHAR_CLASS_MAX_LENGTH CHARCLASS_NAME_MAX
#  else
/* This shouldn't happen but some implementation might still have this
   problem.  Use a reasonable default value.  */
#   define CHAR_CLASS_MAX_LENGTH 256
#  endif

#  ifdef _LIBC
#   define IS_CHAR_CLASS(string) __wctype (string)
#  else
#   define IS_CHAR_CLASS(string) wctype (string)
#  endif
# else
#  define CHAR_CLASS_MAX_LENGTH  6 /* Namely, `xdigit'.  */

#  define IS_CHAR_CLASS(string)						      \
   (STREQ (string, "alpha") || STREQ (string, "upper")			      \
    || STREQ (string, "lower") || STREQ (string, "digit")		      \
    || STREQ (string, "alnum") || STREQ (string, "xdigit")		      \
    || STREQ (string, "space") || STREQ (string, "print")		      \
    || STREQ (string, "punct") || STREQ (string, "graph")		      \
    || STREQ (string, "cntrl") || STREQ (string, "blank"))
# endif

/* Avoid depending on library functions or files
   whose names are inconsistent.  */

# if !defined _LIBC && !defined getenv
extern char *getenv ();
# endif

# ifndef errno
extern int errno;
# endif

/* Match STRING against the filename pattern PATTERN, returning zero if
   it matches, nonzero if not.  */
static int
#ifdef _LIBC
internal_function
#endif
internal_fnmatch (const char *pattern, const char *string,
		  int no_leading_period, int flags)
{
  register const char *p = pattern, *n = string;
  register unsigned char c;

/* Note that this evaluates C many times.  */
# ifdef _LIBC
#  define FOLD(c) ((flags & FNM_CASEFOLD) ? tolower (c) : (c))
# else
#  define FOLD(c) ((flags & FNM_CASEFOLD) && ISUPPER (c) ? tolower (c) : (c))
# endif

  while ((c = *p++) != '\0')
    {
      c = FOLD (c);

      switch (c)
	{
	case '?':
	  if (*n == '\0')
	    return FNM_NOMATCH;
	  else if (*n == '/' && (flags & FNM_FILE_NAME))
	    return FNM_NOMATCH;
	  else if (*n == '.' && no_leading_period
		   && (n == string
		       || (n[-1] == '/' && (flags & FNM_FILE_NAME))))
	    return FNM_NOMATCH;
	  break;

	case '\\':
	  if (!(flags & FNM_NOESCAPE))
	    {
	      c = *p++;
	      if (c == '\0')
		/* Trailing \ loses.  */
		return FNM_NOMATCH;
	      c = FOLD (c);
	    }
	  if (FOLD ((unsigned char) *n) != c)
	    return FNM_NOMATCH;
	  break;

	case '*':
	  if (*n == '.' && no_leading_period
	      && (n == string
		  || (n[-1] == '/' && (flags & FNM_FILE_NAME))))
	    return FNM_NOMATCH;

	  for (c = *p++; c == '?' || c == '*'; c = *p++)
	    {
	      if (*n == '/' && (flags & FNM_FILE_NAME))
		/* A slash does not match a wildcard under FNM_FILE_NAME.  */
		return FNM_NOMATCH;
	      else if (c == '?')
		{
		  /* A ? needs to match one character.  */
		  if (*n == '\0')
		    /* There isn't another character; no match.  */
		    return FNM_NOMATCH;
		  else
		    /* One character of the string is consumed in matching
		       this ? wildcard, so *??? won't match if there are
		       less than three characters.  */
		    ++n;
		}
	    }

	  if (c == '\0')
	    /* The wildcard(s) is/are the last element of the pattern.
	       If the name is a file name and contains another slash
	       this does mean it cannot match.  If the FNM_LEADING_DIR
	       flag is set and exactly one slash is following, we have
	       a match.  */
	    {
	      int result = (flags & FNM_FILE_NAME) == 0 ? 0 : FNM_NOMATCH;

	      if (flags & FNM_FILE_NAME)
		{
		  const char *slashp = strchr (n, '/');

		  if (flags & FNM_LEADING_DIR)
		    {
		      if (slashp != NULL
			  && strchr (slashp + 1, '/') == NULL)
			result = 0;
		    }
		  else
		    {
		      if (slashp == NULL)
			result = 0;
		    }
		}

	      return result;
	    }
	  else
	    {
	      const char *endp;

	      endp = __strchrnul (n, (flags & FNM_FILE_NAME) ? '/' : '\0');

	      if (c == '[')
		{
		  int flags2 = ((flags & FNM_FILE_NAME)
				? flags : (flags & ~FNM_PERIOD));

		  for (--p; n < endp; ++n)
		    if (internal_fnmatch (p, n,
					  (no_leading_period
					   && (n == string
					       || (n[-1] == '/'
						   && (flags
						       & FNM_FILE_NAME)))),
					  flags2)
			== 0)
		      return 0;
		}
	      else if (c == '/' && (flags & FNM_FILE_NAME))
		{
		  while (*n != '\0' && *n != '/')
		    ++n;
		  if (*n == '/'
		      && (internal_fnmatch (p, n + 1, flags & FNM_PERIOD,
					    flags) == 0))
		    return 0;
		}
	      else
		{
		  int flags2 = ((flags & FNM_FILE_NAME)
				? flags : (flags & ~FNM_PERIOD));

		  if (c == '\\' && !(flags & FNM_NOESCAPE))
		    c = *p;
		  c = FOLD (c);
		  for (--p; n < endp; ++n)
		    if (FOLD ((unsigned char) *n) == c
			&& (internal_fnmatch (p, n,
					      (no_leading_period
					       && (n == string
						   || (n[-1] == '/'
						       && (flags
							   & FNM_FILE_NAME)))),
					      flags2) == 0))
		      return 0;
		}
	    }

	  /* If we come here no match is possible with the wildcard.  */
	  return FNM_NOMATCH;

	case '[':
	  {
	    /* Nonzero if the sense of the character class is inverted.  */
	    static int posixly_correct;
	    register int not;
	    char cold;

	    if (posixly_correct == 0)
	      posixly_correct = getenv ("POSIXLY_CORRECT") != NULL ? 1 : -1;

	    if (*n == '\0')
	      return FNM_NOMATCH;

	    if (*n == '.' && no_leading_period && (n == string
						   || (n[-1] == '/'
						       && (flags
							   & FNM_FILE_NAME))))
	      return FNM_NOMATCH;

	    if (*n == '/' && (flags & FNM_FILE_NAME))
	      /* `/' cannot be matched.  */
	      return FNM_NOMATCH;

	    not = (*p == '!' || (posixly_correct < 0 && *p == '^'));
	    if (not)
	      ++p;

	    c = *p++;
	    for (;;)
	      {
		unsigned char fn = FOLD ((unsigned char) *n);

		if (!(flags & FNM_NOESCAPE) && c == '\\')
		  {
		    if (*p == '\0')
		      return FNM_NOMATCH;
		    c = FOLD ((unsigned char) *p);
		    ++p;

		    if (c == fn)
		      goto matched;
		  }
		else if (c == '[' && *p == ':')
		  {
		    /* Leave room for the null.  */
		    char str[CHAR_CLASS_MAX_LENGTH + 1];
		    size_t c1 = 0;
# if defined _LIBC || (defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H)
		    wctype_t wt;
# endif
		    const char *startp = p;

		    for (;;)
		      {
			if (c1 == CHAR_CLASS_MAX_LENGTH)
			  /* The name is too long and therefore the pattern
			     is ill-formed.  */
			  return FNM_NOMATCH;

			c = *++p;
			if (c == ':' && p[1] == ']')
			  {
			    p += 2;
			    break;
			  }
			if (c < 'a' || c >= 'z')
			  {
			    /* This cannot possibly be a character class name.
			       Match it as a normal range.  */
			    p = startp;
			    c = '[';
			    goto normal_bracket;
			  }
			str[c1++] = c;
		      }
		    str[c1] = '\0';

# if defined _LIBC || (defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H)
		    wt = IS_CHAR_CLASS (str);
		    if (wt == 0)
		      /* Invalid character class name.  */
		      return FNM_NOMATCH;

		    if (__iswctype (__btowc ((unsigned char) *n), wt))
		      goto matched;
# else
		    if ((STREQ (str, "alnum") && ISALNUM ((unsigned char) *n))
			|| (STREQ (str, "alpha") && ISALPHA ((unsigned char) *n))
			|| (STREQ (str, "blank") && ISBLANK ((unsigned char) *n))
			|| (STREQ (str, "cntrl") && ISCNTRL ((unsigned char) *n))
			|| (STREQ (str, "digit") && ISDIGIT ((unsigned char) *n))
			|| (STREQ (str, "graph") && ISGRAPH ((unsigned char) *n))
			|| (STREQ (str, "lower") && ISLOWER ((unsigned char) *n))
			|| (STREQ (str, "print") && ISPRINT ((unsigned char) *n))
			|| (STREQ (str, "punct") && ISPUNCT ((unsigned char) *n))
			|| (STREQ (str, "space") && ISSPACE ((unsigned char) *n))
			|| (STREQ (str, "upper") && ISUPPER ((unsigned char) *n))
			|| (STREQ (str, "xdigit") && ISXDIGIT ((unsigned char) *n)))
		      goto matched;
# endif
		  }
		else if (c == '\0')
		  /* [ (unterminated) loses.  */
		  return FNM_NOMATCH;
		else
		  {
		    c = FOLD (c);
		  normal_bracket:
		    if (c == fn)
		      goto matched;

		    cold = c;
		    c = *p++;

		    if (c == '-' && *p != ']')
		      {
			/* It is a range.  */
			char lo[2];
			char fc[2];
			unsigned char cend = *p++;
			if (!(flags & FNM_NOESCAPE) && cend == '\\')
			  cend = *p++;
			if (cend == '\0')
			  return FNM_NOMATCH;

			lo[0] = cold;
			lo[1] = '\0';
			fc[0] = fn;
			fc[1] = '\0';
			if (strcoll (lo, fc) <= 0)
			  {
			    char hi[2];
			    hi[0] = FOLD (cend);
			    hi[1] = '\0';
			    if (strcoll (fc, hi) <= 0)
			      goto matched;
			  }

			c = *p++;
		      }
		  }

		if (c == ']')
		  break;
	      }

	    if (!not)
	      return FNM_NOMATCH;
	    break;

	  matched:
	    /* Skip the rest of the [...] that already matched.  */
	    while (c != ']')
	      {
		if (c == '\0')
		  /* [... (unterminated) loses.  */
		  return FNM_NOMATCH;

		c = *p++;
		if (!(flags & FNM_NOESCAPE) && c == '\\')
		  {
		    if (*p == '\0')
		      return FNM_NOMATCH;
		    /* XXX 1003.2d11 is unclear if this is right.  */
		    ++p;
		  }
		else if (c == '[' && *p == ':')
		  {
		    do
		      if (*++p == '\0')
			return FNM_NOMATCH;
		    while (*p != ':' || p[1] == ']');
		    p += 2;
		    c = *p;
		  }
	      }
	    if (not)
	      return FNM_NOMATCH;
	  }
	  break;

	default:
	  if (c != FOLD ((unsigned char) *n))
	    return FNM_NOMATCH;
	}

      ++n;
    }

  if (*n == '\0')
    return 0;

  if ((flags & FNM_LEADING_DIR) && *n == '/')
    /* The FNM_LEADING_DIR flag says that "foo*" matches "foobar/frobozz".  */
    return 0;

  return FNM_NOMATCH;

# undef FOLD
}


int
fnmatch (pattern, string, flags)
     const char *pattern;
     const char *string;
     int flags;
{
  return internal_fnmatch (pattern, string, flags & FNM_PERIOD, flags);
}

#endif	/* _LIBC or not __GNU_LIBRARY__.  */
