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

   Id: dates.c,v 1.5 2000/05/09 18:25:37 joe Exp 
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>

#include <time.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>

#include "xalloc.h"
#include "dates.h"

/* Generic date manipulation routines. */

/* RFC1123: Sun, 06 Nov 1994 08:49:37 GMT */
#define RFC1123_FORMAT "%3s, %02d %3s %4d %02d:%02d:%02d GMT"
/* RFC850:  Sunday, 06-Nov-94 08:49:37 GMT */
#define RFC1036_FORMAT "%s, %2d-%3s-%2d %2d:%2d:%2d GMT"
/* asctime: Wed Jun 30 21:49:08 1993 */
#define ASCTIME_FORMAT "%3s %3s %2d %2d:%2d:%2d %4d"

static const char *rfc1123_weekdays[7] = { 
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" 
};
static const char *short_months[12] = { 
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Returns the time/date GMT, in RFC1123-type format: eg
 *  Sun, 06 Nov 1994 08:49:37 GMT. */
char *rfc1123_date( time_t anytime ) {
    struct tm *gmt;
    char *ret;
    gmt = gmtime( &anytime );
    ret = xmalloc( 29 + 1 ); /* dates are 29 chars long */
/*  it goes: Sun, 06 Nov 1994 08:49:37 GMT */
    snprintf( ret, 30, RFC1123_FORMAT,
	      rfc1123_weekdays[gmt->tm_wday], gmt->tm_mday, 
	      short_months[gmt->tm_mon], 1900 + gmt->tm_year, 
	      gmt->tm_hour, gmt->tm_min, gmt->tm_sec );
    
    return ret;
}

/* Takes an RFC1123-formatted date string and returns the time_t.
 * Returns (time_t)-1 if the parse fails. */
time_t rfc1123_parse( const char *date ) {
    struct tm gmt = {0};
    static char wkday[4], mon[4];
    int n;
/*  it goes: Sun, 06 Nov 1994 08:49:37 GMT */
    n = sscanf( date, RFC1123_FORMAT,
	    wkday, &gmt.tm_mday, mon, &gmt.tm_year, &gmt.tm_hour,
	    &gmt.tm_min, &gmt.tm_sec );
    /* Is it portable to check n==7 here? */
    gmt.tm_year -= 1900;
    for( n=0; n<12; n++ )
	if( strcmp( mon, short_months[n] ) == 0 )
	    break;
    /* tm_mon comes out as 12 if the month is corrupt, which is desired,
     * since the mktime will then fail */
    gmt.tm_mon = n;
    return mktime( &gmt );
}

/* Takes a string containing a RFC1036-style date and returns the time_t */
time_t rfc1036_parse( const char *date ) {
    struct tm gmt = {0};
    int n;
    static char wkday[10], mon[4];
    /* RFC850/1036 style dates: Sunday, 06-Nov-94 08:49:37 GMT */
    n = sscanf( RFC1036_FORMAT,
		wkday, &gmt.tm_mday, mon, &gmt.tm_year,
		&gmt.tm_hour, &gmt.tm_min, &gmt.tm_sec );
    /* portable to check n here? */
    for( n=0; n<12; n++ )
	if( strcmp( mon, short_months[n] ) == 0 )
	    break;
    /* tm_mon comes out as 12 if the month is corrupt, which is desired,
     * since the mktime will then fail */
    gmt.tm_mon = n;
    return mktime( &gmt );
}


/* (as)ctime dates are like:
 *    Wed Jun 30 21:49:08 1993
 */
time_t asctime_parse( const char *date ) {
    struct tm gmt = {0};
    int n;
    static char wkday[4], mon[4];
    n = sscanf( ASCTIME_FORMAT,
		wkday, mon, &gmt.tm_mday, 
		&gmt.tm_hour, &gmt.tm_min, &gmt.tm_sec,
		&gmt.tm_year );
    /* portable to check n here? */
    for( n=0; n<12; n++ )
	if( strcmp( mon, short_months[n] ) == 0 )
	    break;
    /* tm_mon comes out as 12 if the month is corrupt, which is desired,
     * since the mktime will then fail */
    gmt.tm_mon = n;
    return mktime( &gmt );
}

#undef RFC1036_FORMAT
#undef ASCTIME_FORMAT
#undef RFC1123_FORMAT

#ifdef RFC1123_TEST

int main( int argc, char **argv ) {
    time_t now, in;
    char *out;
    if( argc > 1 ) {
	printf( "Got: %s\n", argv[1] );
	in = rfc1123_parse( argv[1] );
	printf( "Parsed: %d\n", in );
	out = rfc1123_date( in );
	printf( "Back again: %s\n", out );
    } else {
	now = time(NULL);
	out = rfc1123_date(now);
	in = rfc1123_parse(out);
	printf( "time(NULL) = %d\n", now );
	printf( "RFC1123 Time: [%s]\n", out );
	printf( "Parsed = %d\n", in );
	out = rfc1123_date( in );
	printf( "Back again: [%s]\n", out );
    }
    return 0;
}

#endif


