/* 
   Date manipulation routines
   Copyright (C) 1999-2000, Joe Orton <joe@orton.demon.co.uk>

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

#ifndef DATES_H
#define DATES_H

#include "config.h"

#include <sys/types.h>

/* Date manipulation routines as per RFC1123 and RFC1036 */

/* Return current date/time in RFC1123 format */
char *rfc1123_date( time_t anytime );

/* Returns time from date/time in RFC1123 format */
time_t rfc1123_parse( const char *date );

time_t rfc1036_parse( const char *date );

/* Parses asctime date string */
time_t asctime_parse( const char *date );

#endif /* DATES_H */
