/* 
   Text->Base64 convertion, from RFC1521
   Copyright (C) 1998-2000 Joe Orton <joe@orton.demon.co.uk>

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

#ifndef BASE64_H
#define BASE64_H

/* Plain->Base64 converter - will malloc the buffer to the correct size,
 * and fill it with the Base64 conversion of the parameter.
 * Base64 specification from RFC 1521. You must free() it. */

char *base64( const char *text );

#endif /* BASE64_H */

