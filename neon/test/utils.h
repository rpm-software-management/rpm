/* 
   neon-specific test utils
   Copyright (C) 2001, Joe Orton <joe@manyfish.co.uk>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef UTILS_H
#define UTILS_H 1

#include "ne_request.h"

#define ONREQ(x) do { int _ret = (x); if (_ret) { snprintf(on_err_buf, 500, "%s line %d: HTTP error:\n%s", __FUNCTION__, __LINE__, ne_get_error(sess)); i_am(on_err_buf); return FAIL; } } while (0);


#endif /* UTILS_H */
