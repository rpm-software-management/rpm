/* 
   Framework for testing with a server process
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

#ifndef CHILD_H
#define CHILD_H 1

/* Test which does DNS lookup on "localhost": this must be the first
 * named test. */
int lookup_localhost(void);

/* Callback for spawn_server. */
typedef int (*server_fn)(nsocket *sock, void *userdata);

/* Spawns server child process:
 * - forks child process.
 * - child process listens on localhost at given port.
 * - when you connect to it, 'fn' is run...
 * fn is passed the client/server socket as first argument,
 * and userdata as second.
 * - the socket is closed when 'fn' returns, so don't close in in 'fn'.
 */
int spawn_server(int port, server_fn fn, void *userdata);

/* Blocks until child process exits, and gives return code of 'fn'. */
int await_server(void);

/* Kills child process. */
int reap_server(void);

#endif /* CHILD_H */
