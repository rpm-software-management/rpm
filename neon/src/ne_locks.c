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

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <ctype.h> /* for isdigit() */

#include "ne_alloc.h"

#include "ne_request.h"
#include "ne_xml.h"
#include "ne_locks.h"
#include "ne_uri.h"
#include "ne_basic.h"
#include "ne_props.h"
#include "ne_207.h"
#include "ne_i18n.h"

#define HOOK_ID "http://webdav.org/neon/hooks/webdav-locking"

/* The list of locks to submit in an If header 
 * for the current requests */
struct submit_locks {
    const struct ne_lock *lock;
    const char *uri;
    struct submit_locks *next;
};

struct ne_lock_session_s {
    struct ne_lock *locks;
};

/* Per-request lock structure */
struct request_locks {
    struct submit_locks *locks; /* for the If header */
    ne_lock_session *session;
};

/* Context for PROPFIND/lockdiscovery callbacks */
struct discover_ctx {
    ne_lock_result results;
    void *userdata;
};

/* Hook callbacks */
static void *create(void *session, ne_request *req, 
		    const char *method, const char *uri);
static void pre_send(void *private, ne_buffer *req);
static void destroy(void *private);

/* The hooks are needed for construction of the If header */
static ne_request_hooks lock_hooks = {
    HOOK_ID, /* unique id for the locking hooks */
    create,
    pre_send,
    NULL, /* post_send not needed */
    destroy
};

/* Element ID's start at HIP_ELM_UNUSED and work upwards */

#define NE_ELM_LOCK_FIRST (NE_ELM_207_UNUSED)

#define NE_ELM_lockdiscovery (NE_ELM_LOCK_FIRST)
#define NE_ELM_activelock (NE_ELM_LOCK_FIRST + 1)
#define NE_ELM_lockscope (NE_ELM_LOCK_FIRST + 2)
#define NE_ELM_locktype (NE_ELM_LOCK_FIRST + 3)
#define NE_ELM_depth (NE_ELM_LOCK_FIRST + 4)
#define NE_ELM_owner (NE_ELM_LOCK_FIRST + 5)
#define NE_ELM_timeout (NE_ELM_LOCK_FIRST + 6)
#define NE_ELM_locktoken (NE_ELM_LOCK_FIRST + 7)
#define NE_ELM_lockinfo (NE_ELM_LOCK_FIRST + 8)
#define NE_ELM_write (NE_ELM_LOCK_FIRST + 9)
#define NE_ELM_exclusive (NE_ELM_LOCK_FIRST + 10)
#define NE_ELM_shared (NE_ELM_LOCK_FIRST + 11)

static const struct ne_xml_elm lock_elms[] = {
#define A(x) { "DAV:", #x, NE_ELM_ ## x, NE_XML_COLLECT } /* ANY */
#define D(x) { "DAV:", #x, NE_ELM_ ## x, 0 }               /* normal */
#define C(x) { "DAV:", #x, NE_ELM_ ## x, NE_XML_CDATA }   /* (#PCDATA) */
#define E(x) { "DAV:", #x, NE_ELM_ ## x, 0 /* LEAF */ }    /* EMPTY */
    D(lockdiscovery), D(activelock),
    D(prop),
    D(lockscope), D(locktype), C(depth), A(owner), C(timeout), D(locktoken),
    /* no lockentry */
    D(lockinfo), D(lockscope), D(locktype),
    E(write), E(exclusive), E(shared),
    C(href),
#undef A
#undef D
#undef C
#undef E
    { NULL, 0, 0 }
};

static const ne_propname lock_props[] = {
    { "DAV:", "lockdiscovery" },
    { NULL }
};

static void *create(void *session, ne_request *req, 
		    const char *method, const char *uri)
{
    struct request_locks *rl = ne_calloc(sizeof *rl);
    rl->session = session;
    return rl;
}

static void pre_send(void *private, ne_buffer *req)
{
    struct request_locks *rl = private;
    
    if (rl->locks != NULL) {
	struct submit_locks *item;

	/* Add in the If header */
	ne_buffer_zappend(req, "If:");
	for (item = rl->locks; item != NULL; item = item->next) {
	    ne_buffer_concat(req, " <", item->lock->uri, "> (<",
			   item->lock->token, ">)", NULL);
	}
	ne_buffer_zappend(req, EOL);
    }
}

static void destroy(void *priv)
{
    struct request_locks *rl = priv;
    struct submit_locks *lock, *next;
    
    for (lock = rl->locks; lock != NULL; lock = next) {
	next = lock->next;
	free(lock);
    }
    
    free(rl);
}

static void free_locks(void *session)
{
    ne_lock_session *sess = session;
    struct ne_lock *lk = sess->locks;

    while (lk) {
	struct ne_lock *nextlk = lk->next;
	ne_lock_free(lk);
	lk = nextlk;
    }	

    free(sess);
}

ne_lock_session *ne_lock_register(ne_session *sess)
{
    ne_lock_session *locksess = ne_calloc(sizeof *locksess);

    /* Register the hooks */
    ne_add_hooks(sess, &lock_hooks, locksess, free_locks);
    
    return locksess;
}

/* Submit the given lock for the given URI */
static void submit_lock(struct request_locks *rl, struct ne_lock *lock, 
			const char *uri)
{
    struct submit_locks *slock;

    /* Check for dups */
    for (slock = rl->locks; slock != NULL; slock = slock->next) {
	if (strcasecmp(slock->lock->token, lock->token) == 0)
	    return;
    }

    slock = ne_calloc(sizeof *slock);
    slock->lock = lock;
    slock->uri = uri;
    slock->next = rl->locks;
    rl->locks = slock;
}

struct ne_lock *ne_lock_find(ne_lock_session *sess, const char *uri)
{
    struct ne_lock *cur;
    for (cur = sess->locks; cur != NULL; cur = cur->next) {
	if (uri_compare(uri, cur->uri) == 0) 
	    return cur;
    }
    return NULL;
}

void ne_lock_using_parent(ne_request *req, const char *uri)
{
    struct request_locks *rl = ne_request_hook_private(req, HOOK_ID);
    char *parent;

    if (rl == NULL)
	return;	

    parent = uri_parent(uri);

    if (parent != NULL) {
	struct ne_lock *lock;
	/* Find any locks on the parent resource.
	 * FIXME: should check for depth-infinity locks too. */
	lock = ne_lock_find(rl->session, parent);
	if (lock) {
	    NE_DEBUG(NE_DBG_LOCKS, "Locked parent, %s on %s\n", lock->token, 
		  lock->uri);
	    submit_lock(rl, lock, uri);
	}
	free(parent);
    }
}

int ne_lock_iterate(ne_lock_session *sess, 
		     ne_lock_walkfunc func, void *userdata)
{
    struct ne_lock *lock;
    int count = 0;

    for (lock = sess->locks; lock != NULL; lock = lock->next) {
	if (func != NULL) {
	    (*func)(lock, userdata);
	}
	count++;
    }
    
    return count;
}

void ne_lock_using_resource(ne_request *req, const char *uri, int depth)
{
    /* Grab the private cookie for this request */
    struct request_locks *rl = ne_request_hook_private(req, HOOK_ID);
    struct ne_lock *lock; /* all the known locks */
    int match;

    if (rl == NULL)
	return;	

    /* Iterate over the list of session locks to see if any of
     * them apply to this resource */
    for (lock = rl->session->locks; lock != NULL; lock = lock->next) {
	
	match = 0;
	
	if (depth == NE_DEPTH_INFINITE && uri_childof(uri, lock->uri)) {
	    /* Case 1: this is a depth-infinity request which will 
	     * modify a lock somewhere inside the collection. */
	    NE_DEBUG(NE_DBG_LOCKS, "Has child: %s\n", lock->token);
	    match = 1;
	} 
	else if (uri_compare(uri, lock->uri) == 0) {
	    /* Case 2: this request is directly on a locked resource */
	    NE_DEBUG(NE_DBG_LOCKS, "Has direct lock: %s\n", lock->token);
	    match = 1;
	}
	else if (lock->depth == NE_DEPTH_INFINITE && 
		 uri_childof(lock->uri, uri)) {
	    /* Case 3: there is a higher-up infinite-depth lock which
	     * covers the resource that this request will modify. */
	    NE_DEBUG(NE_DBG_LOCKS, "Is child of: %s\n", lock->token);
	    match = 1;
	}
	
	if (match) {
	    submit_lock(rl, lock, uri);
	}
    }

}

void ne_lock_add(ne_lock_session *sess, struct ne_lock *lock)
{
    if (sess->locks != NULL) {
	sess->locks->prev = lock;
    }
    lock->prev = NULL;
    lock->next = sess->locks;
    sess->locks = lock;
}

void ne_lock_remove(ne_lock_session *sess, struct ne_lock *lock)
{
    if (lock->prev != NULL) {
	lock->prev->next = lock->next;
    } else {
	sess->locks = lock->next;
    }
    if (lock->next != NULL) {
	lock->next->prev = lock->prev;
    }
}

struct ne_lock *ne_lock_copy(const struct ne_lock *lock)
{
    struct ne_lock *ret = ne_calloc(sizeof *ret);

    /* TODO: check whether uri or token are NULL? Or, maybe, might as
     * well crash now if they are, since something else will later
     * on. */
    ret->uri = ne_strdup(lock->uri);
    ret->depth = lock->depth;
    ret->type = lock->type;
    ret->scope = lock->scope;
    ret->token = ne_strdup(lock->token);
    if (lock->owner) ret->owner = ne_strdup(lock->owner);
    ret->timeout = lock->timeout;

    return ret;
}

void ne_lock_free(struct ne_lock *lock)
{
    NE_FREE(lock->uri);
    NE_FREE(lock->owner);
    NE_FREE(lock->token);
    free(lock);
}

int ne_unlock(ne_session *sess, struct ne_lock *lock)
{
    ne_request *req = ne_request_create(sess, "UNLOCK", lock->uri);
    int ret;
    
    ne_print_request_header(req, "Lock-Token", "<%s>", lock->token);
    
    /* TODO: need this or not?
     * it definitely goes away if lock-null resources go away */
    ne_lock_using_parent(req, lock->uri);

    ret = ne_request_dispatch(req);
    
    if (ret == NE_OK && ne_get_status(req)->klass == 2) {
	ret = NE_OK;    
    } else {
	ret = NE_ERROR;
    }

    ne_request_destroy(req);
    
    return ret;
}

static int check_context(ne_xml_elmid parent, ne_xml_elmid child) {
    NE_DEBUG(NE_DBG_XML, "ne_locks: check_context %d in %d\n", child, parent);
    switch (parent) {
    case NE_ELM_root:
	/* TODO: for LOCK requests only...
	 * shouldn't allow this for PROPFIND really */
	if (child == NE_ELM_prop)
	    return NE_XML_VALID;
	break;	    
    case NE_ELM_prop:
	if (child == NE_ELM_lockdiscovery)
	    return NE_XML_VALID;
	break;
    case NE_ELM_lockdiscovery:
	if (child == NE_ELM_activelock)
	    return NE_XML_VALID;
	break;
    case NE_ELM_activelock:
	switch (child) {
	case NE_ELM_lockscope:
	case NE_ELM_locktype:
	case NE_ELM_depth:
	case NE_ELM_owner:
	case NE_ELM_timeout:
	case NE_ELM_locktoken:
	    return NE_XML_VALID;
	default:
	    break;
	}
	break;
    case NE_ELM_lockscope:
	switch (child) {
	case NE_ELM_exclusive:
	case NE_ELM_shared:
	    return NE_XML_VALID;
	default:
	    break;
	}
    case NE_ELM_locktype:
	if (child == NE_ELM_write)
	    return NE_XML_VALID;
	break;
	/* ... depth is PCDATA, owner is COLLECT, timeout is PCDATA */
    case NE_ELM_locktoken:
	if (child == NE_ELM_href)
	    return NE_XML_VALID;
	break;
    }
    return NE_XML_DECLINE;
}

static int parse_depth(const char *depth) {
    if (strcasecmp(depth, "infinity") == 0) {
	return NE_DEPTH_INFINITE;
    } else if (isdigit(depth[0])) {
	return atoi(depth);
    } else {
	return -1;
    }
}

static long parse_timeout(const char *timeout) {
    if (strcasecmp(timeout, "infinite") == 0) {
	return NE_TIMEOUT_INFINITE;
    } else if (strncasecmp(timeout, "Second-", 7) == 0) {
	long to = strtol(timeout, NULL, 10);
	if (to == LONG_MIN || to == LONG_MAX)
	    return NE_TIMEOUT_INVALID;
	return to;
    } else {
	return NE_TIMEOUT_INVALID;
    }
}

static void discover_results(void *userdata, const char *href,
			     const ne_prop_result_set *set)
{
    struct discover_ctx *ctx = userdata;
    struct ne_lock *lock = ne_propset_private(set);

    if (lock == NULL)
	return;

    lock->uri = ne_strdup(href);

    ctx->results(ctx->userdata, lock, 
		 href, ne_propset_status(set, &lock_props[0]));

    
    ne_lock_free(lock);

    NE_DEBUG(NE_DBG_LOCKS, "End of response for %s\n", href);
}

static int 
end_element_common(struct ne_lock *l, const struct ne_xml_elm *elm,
		   const char *cdata)
{
    switch (elm->id){ 
    case NE_ELM_write:
	l->type = ne_locktype_write;
	break;
    case NE_ELM_exclusive:
	l->scope = ne_lockscope_exclusive;
	break;
    case NE_ELM_shared:
	l->scope = ne_lockscope_shared;
	break;
    case NE_ELM_depth:
	NE_DEBUG(NE_DBG_LOCKS, "Got depth: %s\n", cdata);
	l->depth = parse_depth(cdata);
	if (l->depth == -1) {
	    return -1;
	}
	break;
    case NE_ELM_timeout:
	NE_DEBUG(NE_DBG_LOCKS, "Got timeout: %s\n", cdata);
	l->timeout = parse_timeout(cdata);
	if (l->timeout == NE_TIMEOUT_INVALID) {
	    return -1;
	}
	break;
    case NE_ELM_owner:
	l->owner = strdup(cdata);
	break;
    case NE_ELM_href:
	l->token = strdup(cdata);
	break;
    }
    return 0;
}

/* End-element handler for lock discovery PROPFIND response */
static int 
end_element_ldisc(void *userdata, const struct ne_xml_elm *elm, 
		  const char *cdata) 
{
    struct ne_lock *lock = ne_propfind_current_private(userdata);

    return end_element_common(lock, elm, cdata);
}

/* End-element handler for LOCK response */
static int
end_element_lock(void *userdata, const struct ne_xml_elm *elm, 
		 const char *cdata)
{
    struct ne_lock *lock = userdata;
    return end_element_common(lock, elm, cdata);
}

static void *create_private(void *userdata, const char *uri)
{
    return ne_calloc(sizeof(struct ne_lock));
}

/* Discover all locks on URI */
int ne_lock_discover(ne_session *sess, const char *uri, 
		      ne_lock_result callback, void *userdata)
{
    ne_propfind_handler *handler;
    struct discover_ctx ctx = {0};
    int ret;
    
    ctx.results = callback;
    ctx.userdata = userdata;

    handler = ne_propfind_create(sess, uri, NE_DEPTH_ZERO);

    ne_propfind_set_private(handler, create_private, NULL);
    
    ne_xml_push_handler(ne_propfind_get_parser(handler), lock_elms, 
			 check_context, NULL, end_element_ldisc, handler);
    
    ret = ne_propfind_named(handler, lock_props, discover_results, &ctx);
    
    ne_propfind_destroy(handler);

    return ret;
}

int ne_lock(ne_session *sess, struct ne_lock *lock) 
{
    ne_request *req = ne_request_create(sess, "LOCK", lock->uri);
    ne_buffer *body = ne_buffer_create();
    ne_xml_parser *parser = ne_xml_create();
    int ret, parse_failed;

    ne_xml_push_handler(parser, lock_elms, check_context, 
			 NULL, end_element_lock, lock);
    
    /* Create the body */
    ne_buffer_concat(body, "<?xml version=\"1.0\" encoding=\"utf-8\"?>" EOL
		    "<lockinfo xmlns='DAV:'>" EOL " <lockscope>",
		    lock->scope==ne_lockscope_exclusive?
		    "<exclusive/>":"<shared/>",
		    "</lockscope>" EOL
		    "<locktype><write/></locktype>", NULL);

    if (lock->owner) {
	ne_buffer_concat(body, "<owner>", lock->owner, "</owner>" EOL, NULL);
    }
    ne_buffer_zappend(body, "</lockinfo>" EOL);

    ne_set_request_body_buffer(req, body->data, ne_buffer_size(body));
    ne_add_response_body_reader(req, ne_accept_2xx, 
				  ne_xml_parse_v, parser);
    ne_add_request_header(req, "Content-Type", "text/xml");
    ne_add_depth_header(req, lock->depth);

    /* TODO: 
     * By 2518, we need this only if we are creating a lock-null resource.
     * Since we don't KNOW whether the lock we're given is a lock-null
     * or not, we cover our bases.
     */
    ne_lock_using_parent(req, lock->uri);
    /* This one is clearer from 2518 sec 8.10.4. */
    ne_lock_using_resource(req, lock->uri, lock->depth);

    ret = ne_request_dispatch(req);

    ne_buffer_destroy(body);
    parse_failed = !ne_xml_valid(parser);
    
    if (ret == NE_OK && ne_get_status(req)->klass == 2) {
	if (parse_failed) {
	    ret = NE_ERROR;
	    ne_set_error(sess, ne_xml_get_error(parser));
	}
	else if (ne_get_status(req)->code == 207) {
	    ret = NE_ERROR;
	    /* TODO: set the error string appropriately */
	}
    } else {
	ret = NE_ERROR;
    }

    ne_request_destroy(req);
    ne_xml_destroy(parser);

    /* TODO: free the list */
    return ret;
}
