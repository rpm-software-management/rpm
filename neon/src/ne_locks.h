/* 
   WebDAV Class 2 locking operations
   Copyright (C) 1999-2001, Joe Orton <joe@light.plus.com>

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

#ifndef NE_LOCKS_H
#define NE_LOCKS_H

#include "ne_request.h" /* for ne_session + http_req */

BEGIN_NEON_DECLS

/* The scope of a lock */
enum ne_lock_scope {
    ne_lockscope_exclusive,
    ne_lockscope_shared
};

/* Lock type. Only write locks are defined in RFC2518. */
enum ne_lock_type {
    ne_locktype_write
};

/* A lock object. Lock objects are kept on doubly-linked lists. */
struct ne_lock {
    char *uri; /* the URI which this lock covers. */
    int depth; /* the depth of the lock (NE_DEPTH_*). */
    enum ne_lock_type type;
    enum ne_lock_scope scope;
    char *token; /* the lock token: uniquely identifies this lock. */
    char *owner; /* string describing the owner of the lock. */
    long timeout; /* timeout in seconds. (or NE_TIMEOUT_*) */
    struct ne_lock *next, *prev;
};
/* NB: struct ne_lock Would be typedef'ed to ne_lock except lock is
 * a verb and a noun, so we already have ne_lock the function. Damn
 * the English language. */

#define NE_TIMEOUT_INFINITE -1
#define NE_TIMEOUT_INVALID -2

typedef struct ne_lock_session_s ne_lock_session;

/* TODO: 
 * "session" is a bad word, and it's already used for ne_session,
 * maybe think up a better name. lock_store is quite good.
 */

/* Register the locking hooks with an ne_session.  Owned locks
 * persist for the duration of this session. The lock session lasts
 * exactly as long as the corresponding ne_session. Once you call
 * ne_session_destroy(sess), any use of the lock session has
 * undefined results.  */
ne_lock_session *ne_lock_register(ne_session *sess);

/* Add a lock to the given session. The lock will subsequently be
 * submitted as required in an If: header with any requests created
 * using the ne_session which the lock session is tied to.  Requests
 * indicate to the locking layer which locks they might need using
 * ne_lock_using_*, as described below. */
void ne_lock_add(ne_lock_session *sess, struct ne_lock *lock);

/* Remove lock, which must have been previously added to the
 * session using 'ne_lock_add' above. */
void ne_lock_remove(ne_lock_session *sess, struct ne_lock *lock);

typedef void (*ne_lock_walkfunc)(struct ne_lock *lock, void *userdata);

/* For each lock added to the session, call func, passing the lock
 * and the given userdata. Returns the number of locks. func may be
 * pass as NULL, in which case, can be used to simply return number
 * of locks in the session. */
int ne_lock_iterate(ne_lock_session *sess, 
		    ne_lock_walkfunc func, void *userdata);

/* Issue a LOCK request for the given lock. */
int ne_lock(ne_session *sess, struct ne_lock *lock);
/* Issue an UNLOCK request for the given lock */
int ne_unlock(ne_session *sess, struct ne_lock *lock);

/* Find a lock in the session with given URI */
struct ne_lock *ne_lock_find(ne_lock_session *sess, const char *uri);

/* Deep-copy a lock structure. */
struct ne_lock *ne_lock_copy(const struct ne_lock *lock);

/* Free a lock structure */
void ne_lock_free(struct ne_lock *lock);

/* Callback for lock discovery.  If 'lock' is NULL, 
 * something went wrong retrieving lockdiscover for the resource,
 * look at 'status' for the details. */
typedef void (*ne_lock_result)(void *userdata, const struct ne_lock *lock, 
			       const char *uri, const ne_status *status);

/* Perform lock discovery on the given URI.  'result' is called
 * with the results (possibly >1 times).  */
int ne_lock_discover(ne_session *sess, const char *uri, 
		     ne_lock_result result, void *userdata);

/*** For use by method functions */

/* Indicate that this request is of depth n on given uri */
void ne_lock_using_resource(ne_request *req, const char *uri, int depth);
/* Indicate that this request will modify parent collection of given URI */
void ne_lock_using_parent(ne_request *req, const char *uri);

END_NEON_DECLS

#endif /* NE_LOCKS_H */
