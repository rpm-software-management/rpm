/* Line length calculation routine.
   Taken from fileutils-4.0i/src/ls.c
   Copyright (C) 85, 88, 90, 91, 1995-1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 

   Id: linelen.c,v 1.1 1999/06/25 19:26:46 joe Exp  
 */

#if HAVE_TERMIOS_H
# include <termios.h>
#endif

#ifdef GWINSZ_IN_SYS_IOCTL
# include <sys/ioctl.h>
#endif

#ifdef WINSIZE_IN_PTEM
# include <sys/stream.h>
# include <sys/ptem.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h> /* For STDOUT_FILENO */
#endif

#define DEFAULT_LINE_LENGTH 80

int get_line_length( void ) {
    int ret = DEFAULT_LINE_LENGTH;

/* joe: Some systems do this with a 'struct ttysize'?
 * There is code in tin to do it, but tin has a dodgy license.
 * DIY, if you have such a system.
 */

#ifdef TIOCGWINSZ
    struct winsize ws;
    
    if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0)
	ret = ws.ws_col;
    
#endif
    return ret;
}
