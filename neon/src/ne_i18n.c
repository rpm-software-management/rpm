/* 
   Internationalization of neon
   Copyright (C) 1999-2003, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

void neon_i18n_init(void)
{
#if defined(ENABLE_NLS) && defined(NEON_IS_LIBRARY)
    /* if neon is build bundled in (i.e., not as a standalone
     * library), then there is probably no point in this, since the
     * messages won't be pointing in the right direction.
     * there's not really any point in doing this if neon is
     * a library since the messages aren't i18n'ized, but... */
    bindtextdomain("neon", LOCALEDIR);
#endif
}
