/*
errmsg.c - implementation of the elf_errmsg(3) function.
Copyright (C) 1995 Michael Riepe <riepe@ifwsn4.ifw.uni-hannover.de>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <private.h>

#if HAVE_NLS
# include <nl_types.h>
static nl_catd _libelf_cat = (nl_catd)0;
static const char domain[] = "libelf";
#endif

static const char *_messages[] = {
#define __err__(a,b)	b,
#include <errors.h>		/* include string tables from errors.h */
#undef __err__
};

const char*
elf_errmsg(int err) {
    if (err == 0) {
	err = _elf_errno;
	if (err == 0) {
	    return NULL;
	}
    }
    else if (err == -1) {
	err = _elf_errno;
    }

    if (err < 0 || err >= ERROR_NUM) {
	err = ERROR_UNKNOWN;
    }

#if HAVE_NLS
    if (_libelf_cat == (nl_catd)0) {
	_libelf_cat = catopen(domain, 0);
    }
    if (_libelf_cat != (nl_catd)-1) {
	return catgets(_libelf_cat, 1, err + 1, _messages[err]);
    }
#endif
    return _messages[err];
}
