/* 
   WebDAV Class 2 locking operations
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

#ifndef DAV_LOCKS_H
#define DAV_LOCKS_H

enum dav_lock_scope {
    dav_lockscope_exclusive,
    dav_lockscope_shared
};

enum dav_lock_type {
    dav_locktype_write
};

/* Would be typedef'ed to dav_lock except lock is a verb and a noun.
 * Damn the English language. */
struct dav_lock {
    char *uri;
    int depth;
    enum dav_lock_type type;
    enum dav_lock_scope scope;
    char *token;
    char *owner;
    long timeout;
    struct dav_lock *next;
    struct dav_lock *prev;
};

struct dav_lock_result {
    struct dav_lock *lock;
    char *href;
    http_status status;
    char *status_line;
    struct dav_lock_result *next;
};

#define DAV_TIMEOUT_INFINITE -1
#define DAV_TIMEOUT_INVALID -2

typedef struct dav_lock_session_s dav_lock_session;

dav_lock_session *dav_lock_register(http_session *sess);
void dav_lock_unregister(dav_lock_session *sess);

void dav_lock_add(dav_lock_session *sess, struct dav_lock *lock);
void dav_lock_remove(dav_lock_session *sess, struct dav_lock *lock);

typedef void (*dav_lock_walkfunc)(struct dav_lock *lock, void *userdata);

int dav_lock_iterate(dav_lock_session *sess, 
		     dav_lock_walkfunc func, void *userdata);

/* Indicate that this request is of depth n on given uri */
void dav_lock_using_resource(http_req *req, const char *uri, int depth);
/* Indicate that this request will modify parent collection of given URI */
void dav_lock_using_parent(http_req *req, const char *uri);

int dav_lock(http_session *sess, struct dav_lock *lock);
int dav_lock_discover(http_session *sess,
		      const char *uri, struct dav_lock_result **locks);

struct dav_lock *dav_lock_find(dav_lock_session *sess, const char *uri);
int dav_unlock(http_session *sess, struct dav_lock *lock);

void dav_lock_free(struct dav_lock *lock);

#endif /* DAV_LOCKS_H */
