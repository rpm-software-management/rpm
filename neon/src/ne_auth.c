/* 
   HTTP Authentication routines
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


/* HTTP Authentication, as per RFC2617.
 * 
 * TODO:
 *  - Improve cnonce? Make it really random using /dev/random or whatever?
 *  - Test auth-int support
 */

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <time.h>

#ifdef HAVE_SNPRINTF_H
#include "snprintf.h"
#endif

#include "base64.h"

#include "ne_md5.h"
#include "ne_dates.h"
#include "ne_request.h"
#include "ne_auth.h"
#include "ne_string.h"
#include "ne_utils.h"
#include "ne_alloc.h"
#include "ne_uri.h"
#include "ne_i18n.h"

/* TODO: should remove this eventually. Need it for
 * ne_pull_request_body. */
#include "ne_private.h"

/* The MD5 digest of a zero-length entity-body */
#define DIGEST_MD5_EMPTY "d41d8cd98f00b204e9800998ecf8427e"

#define HOOK_SERVER_ID "http://webdav.org/neon/hooks/server-auth"
#define HOOK_PROXY_ID "http://webdav.org/neon/hooks/proxy-auth"

/* The authentication scheme we are using */
typedef enum {
    auth_scheme_basic,
    auth_scheme_digest
} auth_scheme;

typedef enum { 
    auth_alg_md5,
    auth_alg_md5_sess,
    auth_alg_unknown
} auth_algorithm;

/* Selected method of qop which the client is using */
typedef enum {
    auth_qop_none,
    auth_qop_auth,
    auth_qop_auth_int
} auth_qop;

/* A challenge */
struct auth_challenge {
    auth_scheme scheme;
    char *realm;
    char *domain;
    char *nonce;
    char *opaque;
    unsigned int stale:1; /* if stale=true */
    unsigned int got_qop:1; /* we were given a qop directive */
    unsigned int qop_auth:1; /* "auth" token in qop attrib */
    unsigned int qop_auth_int:1; /* "auth-int" token in qop attrib */
    auth_algorithm alg;
    struct auth_challenge *next;
};

static const char *qop_values[] = {
    NULL,
    "auth",
    "auth-int"
};
static const char *algorithm_names[] = {
    "MD5",
    "MD5-sess",
    NULL
};

/* The callback used to request the username and password in the given
 * realm. The username and password must be placed in malloc()-allocate
 * memory.
 * Must return:
 *   0 on success, 
 *  -1 to cancel.
 */

/* Authentication session state. */
typedef struct {
    ne_session *sess;

    /* for making this proxy/server generic. */
    const char *req_hdr, *resp_hdr, *resp_info_hdr, *fail_msg;
    int status_code;
    int fail_code;

    /* The scheme used for this authentication session */
    auth_scheme scheme;
    /* The callback used to request new username+password */
    ne_request_auth reqcreds;
    void *reqcreds_udata;

    /*** Session details ***/

    /* The username and password we are using to authenticate with */
    char *username;
    /* Whether we CAN supply authentication at the moment */
    unsigned int can_handle:1;
    /* This used for Basic auth */
    char *basic; 
    /* These all used for Digest auth */
    char *unq_realm;
    char *unq_nonce;
    char *unq_cnonce;
    char *opaque;
    /* A list of domain strings */
    unsigned int domain_count;
    char **domain;
    auth_qop qop;
    auth_algorithm alg;
    int nonce_count;
    /* The ASCII representation of the session's H(A1) value */
    char h_a1[33];

    /* Temporary store for half of the Request-Digest
     * (an optimisation - used in the response-digest calculation) */
    struct ne_md5_ctx stored_rdig;

    /* Details of server... needed to reconstruct absoluteURI's when
     * necessary */
    const char *host;
    const char *uri_scheme;
    unsigned int port;

} auth_session;

struct auth_request {
    auth_session *session;

    /*** Per-request details. ***/
    ne_request *request; /* the request object. */

    /* The method and URI we are using for the current request */
    const char *uri;
    const char *method;
    /* Whether we WILL supply authentication for this request or not */
    unsigned int will_handle:1;

    /* Used for calculation of H(entity-body) of the response */
    struct ne_md5_ctx response_body;

    int tries;
    /* Results of response-header callbacks */
    char *auth_hdr, *auth_info_hdr;
};

/* Private prototypes which used to be public. */

static auth_session *auth_create(ne_session *sess,
				 ne_request_auth callback, 
				 void *userdata);

/* Pass this the value of the "(Proxy,WWW)-Authenticate: " header field.
 * Returns:
 *   0 if we can now authenticate ourselves with the server.
 *   non-zero if we can't
 */
static int auth_challenge(auth_session *sess, const char *value);

/* If you receive a "(Proxy-)Authentication-Info:" header, pass its value to
 * this function. Returns zero if this successfully authenticates
 * the response as coming from the server, and false if it hasn't. */
static int verify_response(struct auth_request *req, 
			   auth_session *sess, const char *value);

/* Private prototypes */
static char *get_cnonce(void);
static void clean_session(auth_session *sess);
static int digest_challenge(auth_session *, struct auth_challenge *);
static int basic_challenge(auth_session *, struct auth_challenge *);
static char *request_digest(auth_session *sess, struct auth_request *req);
static char *request_basic(auth_session *);

/* Domain handling */
static int is_in_domain(auth_session *sess, const char *uri);
static int parse_domain(auth_session *sess, const char *domain);

/* Get the credentials, passing a temporary store for the password value */
static int get_credentials(auth_session *sess, char **password);

auth_session *auth_create(ne_session *sess,
			  ne_request_auth callback, 
			  void *userdata) 
{
    auth_session *ret = ne_calloc(sizeof(auth_session));

    ret->reqcreds = callback;
    ret->reqcreds_udata = userdata;
    ret->sess = sess;
   
    return ret;
}

#if 0
void ne_auth_set_server(auth_session *sess, 
			   const char *host, unsigned int port, const char *scheme)
{
    sess->host = host;
    sess->port = port;
    sess->req_scheme = scheme;
}
#endif

static void clean_session(auth_session *sess) 
{
    sess->can_handle = 0;
    NE_FREE(sess->basic);
    NE_FREE(sess->unq_realm);
    NE_FREE(sess->unq_nonce);
    NE_FREE(sess->unq_cnonce);
    NE_FREE(sess->opaque);
    NE_FREE(sess->username);
    if (sess->domain_count > 0) {
	split_string_free(sess->domain);
	sess->domain_count = 0;
    }
}

/* Returns cnonce-value. We just use base64(time).
 * TODO: Could improve this? */
static char *get_cnonce(void) 
{
    char *ret, *tmp;
    tmp = ne_rfc1123_date(time(NULL));
    ret = ne_base64(tmp, strlen(tmp));
    free(tmp);
    return ret;
}

static int 
get_credentials(auth_session *sess, char **password) 
{
    return (*sess->reqcreds)(sess->reqcreds_udata, sess->unq_realm,
			     &sess->username, password);
}

static int parse_domain(auth_session *sess, const char *domain) {
    char *unq, **doms;
    int count, ret;

    unq = shave_string(domain, '"');
    doms = split_string_c(unq, ' ', NULL, " \r\n\t", &count);
    if (doms != NULL) {
	if (count > 0) {
	    sess->domain = doms;
	    sess->domain_count = count;
	    ret = 0;
	} else {
	    free(doms);
	    ret = -1;
	}
    } else {
	ret = -1;
    }
    free(unq);
    return ret;
}

/* Returns:
 *    0: if uri is in NOT in domain of session
 * else: uri IS in domain of session (or no domain known)
 */
static int is_in_domain(auth_session *sess, const char *uri)
{
#if 1
    return 1;
#else
    /* TODO: Need proper URI comparison for this to work. */
    int ret, dom;
    const char *abs_path;
    if (sess->domain_count == 0) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "No domain, presuming okay.\n");
	return 1;
    }
    ret = 0;
    abs_path = uri_abspath(uri);
    for (dom = 0; dom < sess->domain_count; dom++) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "Checking domain: %s\n", sess->domain[dom]);
	if (uri_childof(sess->domain[dom], abs_path)) {
	    ret = 1;
	    break;
	}
    }
    return ret;
#endif
}

/* Examine a Basic auth challenge.
 * Returns 0 if an valid challenge, else non-zero. */
static int 
basic_challenge(auth_session *sess, struct auth_challenge *parms) 
{
    char *tmp, *password;

    /* Verify challenge... must have a realm */
    if (parms->realm == NULL) {
	return -1;
    }

    NE_DEBUG(NE_DBG_HTTPAUTH, "Got Basic challenge with realm [%s]\n", 
	   parms->realm);

    clean_session(sess);

    sess->unq_realm = shave_string(parms->realm, '"');

    if (get_credentials(sess, &password)) {
	/* Failed to get credentials */
	NE_FREE(sess->unq_realm);
	return -1;
    }

    sess->scheme = auth_scheme_basic;

    CONCAT3(tmp, sess->username, ":", password?password:"");
    sess->basic = ne_base64(tmp, strlen(tmp));
    free(tmp);

    NE_FREE(password);

    return 0;
}

/* Add Basic authentication credentials to a request */
static char *request_basic(auth_session *sess) 
{
    char *buf;
    CONCAT3(buf, "Basic ", sess->basic, "\r\n");
    return buf;
}

/* Examine a digest challenge: return 0 if it is a valid Digest challenge,
 * else non-zero. */
static int digest_challenge(auth_session *sess,
			    struct auth_challenge *parms) 
{
    struct ne_md5_ctx tmp;
    unsigned char tmp_md5[16];
    char *password;

    /* Verify they've given us the right bits. */
    if (parms->alg == auth_alg_unknown ||
	((parms->alg == auth_alg_md5_sess) &&
	 !(parms->qop_auth || parms->qop_auth_int)) ||
	parms->realm==NULL || parms->nonce==NULL) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "Invalid challenge.");
	return -1;
    }

    if (parms->stale) {
	/* Just a stale response, don't need to get a new username/password */
	NE_DEBUG(NE_DBG_HTTPAUTH, "Stale digest challenge.\n");
    } else {
	/* Forget the old session details */
	NE_DEBUG(NE_DBG_HTTPAUTH, "In digest challenge.\n");

	clean_session(sess);
	sess->unq_realm = shave_string(parms->realm, '"');

	/* Not a stale response: really need user authentication */
	if (get_credentials(sess, &password)) {
	    /* Failed to get credentials */
	    NE_FREE(sess->unq_realm);
	    return -1;
	}
    }
    sess->alg = parms->alg;
    sess->scheme = auth_scheme_digest;
    sess->unq_nonce = shave_string(parms->nonce, '"');
    sess->unq_cnonce = get_cnonce();
    if (parms->domain) {
	if (parse_domain(sess, parms->domain)) {
	    /* TODO: Handle the error? */
	}
    } else {
	sess->domain = NULL;
	sess->domain_count = 0;
    }
    if (parms->opaque != NULL) {
	sess->opaque = ne_strdup(parms->opaque); /* don't strip the quotes */
    }
    
    if (parms->got_qop) {
	/* What type of qop are we to apply to the message? */
	NE_DEBUG(NE_DBG_HTTPAUTH, "Got qop directive.\n");
	sess->nonce_count = 0;
	if (parms->qop_auth_int) {
	    sess->qop = auth_qop_auth_int;
	} else {
	    sess->qop = auth_qop_auth;
	}
    } else {
	/* No qop at all/ */
	sess->qop = auth_qop_none;
    }
    
    if (!parms->stale) {
	/* Calculate H(A1).
	 * tmp = H(unq(username-value) ":" unq(realm-value) ":" passwd)
	 */
	NE_DEBUG(NE_DBG_HTTPAUTH, "Calculating H(A1).\n");
	ne_md5_init_ctx(&tmp);
	ne_md5_process_bytes(sess->username, strlen(sess->username), &tmp);
	ne_md5_process_bytes(":", 1, &tmp);
	ne_md5_process_bytes(sess->unq_realm, strlen(sess->unq_realm), &tmp);
	ne_md5_process_bytes(":", 1, &tmp);
	if (password != NULL)
	    ne_md5_process_bytes(password, strlen(password), &tmp);
	ne_md5_finish_ctx(&tmp, tmp_md5);
	if (sess->alg == auth_alg_md5_sess) {
	    unsigned char a1_md5[16];
	    struct ne_md5_ctx a1;
	    char tmp_md5_ascii[33];
	    /* Now we calculate the SESSION H(A1)
	     *    A1 = H(...above...) ":" unq(nonce-value) ":" unq(cnonce-value) 
	     */
	    ne_md5_to_ascii(tmp_md5, tmp_md5_ascii);
	    ne_md5_init_ctx(&a1);
	    ne_md5_process_bytes(tmp_md5_ascii, 32, &a1);
	    ne_md5_process_bytes(":", 1, &a1);
	    ne_md5_process_bytes(sess->unq_nonce, strlen(sess->unq_nonce), &a1);
	    ne_md5_process_bytes(":", 1, &a1);
	    ne_md5_process_bytes(sess->unq_cnonce, strlen(sess->unq_cnonce), &a1);
	    ne_md5_finish_ctx(&a1, a1_md5);
	    ne_md5_to_ascii(a1_md5, sess->h_a1);
	    NE_DEBUG(NE_DBG_HTTPAUTH, "Session H(A1) is [%s]\n", sess->h_a1);
	} else {
	    ne_md5_to_ascii(tmp_md5, sess->h_a1);
	    NE_DEBUG(NE_DBG_HTTPAUTH, "H(A1) is [%s]\n", sess->h_a1);
	}
	
	NE_FREE(password);
    }
    
    NE_DEBUG(NE_DBG_HTTPAUTH, "I like this Digest challenge.\n");

    return 0;
}

/* callback for ne_pull_request_body. */
static int digest_body(void *userdata, const char *buf, size_t count)
{
    struct ne_md5_ctx *ctx = userdata;
    ne_md5_process_bytes(buf, count, ctx);
    return 0;
}

/* Return Digest authentication credentials header value for the given
 * session. */
static char *request_digest(auth_session *sess, struct auth_request *req) 
{
    struct ne_md5_ctx a2, rdig;
    unsigned char a2_md5[16], rdig_md5[16];
    char a2_md5_ascii[33], rdig_md5_ascii[33];
    char nc_value[9] = {0};
    const char *qop_value; /* qop-value */
    ne_buffer *ret;

    /* Increase the nonce-count */
    if (sess->qop != auth_qop_none) {
	sess->nonce_count++;
	snprintf(nc_value, 9, "%08x", sess->nonce_count);
	NE_DEBUG(NE_DBG_HTTPAUTH, "Nonce count is %d, nc is [%s]\n", 
	       sess->nonce_count, nc_value);
    }
    qop_value = qop_values[sess->qop];

    /* Calculate H(A2). */
    ne_md5_init_ctx(&a2);
    ne_md5_process_bytes(req->method, strlen(req->method), &a2);
    ne_md5_process_bytes(":", 1, &a2);
    ne_md5_process_bytes(req->uri, strlen(req->uri), &a2);
    
    if (sess->qop == auth_qop_auth_int) {
	struct ne_md5_ctx body;
	char tmp_md5_ascii[33];
	unsigned char tmp_md5[16];
	
	ne_md5_init_ctx(&body);

	/* Calculate H(entity-body): pull in the request body from
	 * where-ever it is coming from, and calculate the digest. */
	
	NE_DEBUG(NE_DBG_HTTPAUTH, "Digesting request body...\n");
	ne_pull_request_body(req->request, digest_body, &body);
	NE_DEBUG(NE_DBG_HTTPAUTH, "Digesting request body done.\n");
		
	ne_md5_finish_ctx(&body, tmp_md5);
	ne_md5_to_ascii(tmp_md5, tmp_md5_ascii);

	NE_DEBUG(NE_DBG_HTTPAUTH, "H(entity-body) is [%s]\n", tmp_md5_ascii);
	
	/* Append to A2 */
	ne_md5_process_bytes(":", 1, &a2);
	ne_md5_process_bytes(tmp_md5_ascii, 32, &a2);
    }
    ne_md5_finish_ctx(&a2, a2_md5);
    ne_md5_to_ascii(a2_md5, a2_md5_ascii);
    NE_DEBUG(NE_DBG_HTTPAUTH, "H(A2): %s\n", a2_md5_ascii);

    NE_DEBUG(NE_DBG_HTTPAUTH, "Calculating Request-Digest.\n");
    /* Now, calculation of the Request-Digest.
     * The first section is the regardless of qop value
     *     H(A1) ":" unq(nonce-value) ":" */
    ne_md5_init_ctx(&rdig);

    /* Use the calculated H(A1) */
    ne_md5_process_bytes(sess->h_a1, 32, &rdig);

    ne_md5_process_bytes(":", 1, &rdig);
    ne_md5_process_bytes(sess->unq_nonce, strlen(sess->unq_nonce), &rdig);
    ne_md5_process_bytes(":", 1, &rdig);
    if (sess->qop != auth_qop_none) {
	/* Add on:
	 *    nc-value ":" unq(cnonce-value) ":" unq(qop-value) ":"
	 */
	NE_DEBUG(NE_DBG_HTTPAUTH, "Have qop directive, digesting: [%s:%s:%s]\n",
	       nc_value, sess->unq_cnonce, qop_value);
	ne_md5_process_bytes(nc_value, 8, &rdig);
	ne_md5_process_bytes(":", 1, &rdig);
	ne_md5_process_bytes(sess->unq_cnonce, strlen(sess->unq_cnonce), &rdig);
	ne_md5_process_bytes(":", 1, &rdig);
	/* Store a copy of this structure (see note below) */
	sess->stored_rdig = rdig;
	ne_md5_process_bytes(qop_value, strlen(qop_value), &rdig);
	ne_md5_process_bytes(":", 1, &rdig);
    } else {
	/* Store a copy of this structure... we do this because the
	 * calculation of the rspauth= field in the Auth-Info header 
	 * is the same as this digest, up to this point. */
	sess->stored_rdig = rdig;
    }
    /* And finally, H(A2) */
    ne_md5_process_bytes(a2_md5_ascii, 32, &rdig);
    ne_md5_finish_ctx(&rdig, rdig_md5);
    ne_md5_to_ascii(rdig_md5, rdig_md5_ascii);
    
    ret = ne_buffer_create();

    ne_buffer_concat(ret, 
		   "Digest username=\"", sess->username, "\", "
		   "realm=\"", sess->unq_realm, "\", "
		   "nonce=\"", sess->unq_nonce, "\", "
		   "uri=\"", req->uri, "\", "
		   "response=\"", rdig_md5_ascii, "\", "
		   "algorithm=\"", algorithm_names[sess->alg], "\"", NULL);
    
    if (sess->opaque != NULL) {
	/* We never unquote it, so it's still quoted here */
	ne_buffer_concat(ret, ", opaque=", sess->opaque, NULL);
    }

    if (sess->qop != auth_qop_none) {
	/* Add in cnonce and nc-value fields */
	ne_buffer_concat(ret, 
		       ", cnonce=\"", sess->unq_cnonce, "\", "
		       "nc=", nc_value, ", "
		       "qop=\"", qop_values[sess->qop], "\"", NULL);
    }

    ne_buffer_zappend(ret, "\r\n");
    
    NE_DEBUG(NE_DBG_HTTPAUTH, "Digest request header is %s\n", ret->data);

    return ne_buffer_finish(ret);
}

/* Pass this the value of the 'Authentication-Info:' header field, if
 * one is received.
 * Returns:
 *    0 if it gives a valid authentication for the server 
 *    non-zero otherwise (don't believe the response in this case!).
 */
int verify_response(struct auth_request *req, auth_session *sess, const char *value) 
{
    char **pairs;
    auth_qop qop = auth_qop_none;
    char *nextnonce = NULL, /* for the nextnonce= value */
	*rspauth = NULL, /* for the rspauth= value */
	*cnonce = NULL, /* for the cnonce= value */
	*nc = NULL, /* for the nc= value */
	*unquoted, *qop_value = NULL;
    int n, nonce_count, okay;
    
    if (!req->will_handle) {
	/* Ignore it */
	return 0;
    }
    
    if (sess->scheme != auth_scheme_digest) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "Found Auth-Info header not in response to Digest credentials - dodgy.\n");
	return -1;
    }
    
    NE_DEBUG(NE_DBG_HTTPAUTH, "Auth-Info header: %s\n", value);

    pairs = pair_string(value, ',', '=', "\"'", " \r\n\t");
    
    for (n = 0; pairs[n]!=NULL; n+=2) {
	unquoted = shave_string(pairs[n+1], '"');
	if (strcasecmp(pairs[n], "qop") == 0) {
	    qop_value = ne_strdup(pairs[n+1]);
	    if (strcasecmp(pairs[n+1], "auth-int") == 0) {
		qop = auth_qop_auth_int;
	    } else if (strcasecmp(pairs[n+1], "auth") == 0) {
		qop = auth_qop_auth;
	    } else {
		qop = auth_qop_none;
	    }
	} else if (strcasecmp(pairs[n], "nextnonce") == 0) {
	    nextnonce = ne_strdup(unquoted);
	} else if (strcasecmp(pairs[n], "rspauth") == 0) {
	    rspauth = ne_strdup(unquoted);
	} else if (strcasecmp(pairs[n], "cnonce") == 0) {
	    cnonce = ne_strdup(unquoted);
	} else if (strcasecmp(pairs[n], "nc") == 0) { 
	    nc = ne_strdup(pairs[n]);
	    if (sscanf(pairs[n+1], "%x", &nonce_count) != 1) {
		NE_DEBUG(NE_DBG_HTTPAUTH, "Couldn't scan [%s] for nonce count.\n",
		       pairs[n+1]);
	    } else {
		NE_DEBUG(NE_DBG_HTTPAUTH, "Got nonce_count: %d\n", nonce_count);
	    }
	}
	free(unquoted);
    }
    pair_string_free(pairs);

    /* Presume the worst */
    okay = -1;

    if ((qop != auth_qop_none) && (qop_value != NULL)) {
	if ((rspauth == NULL) || (cnonce == NULL) || (nc == NULL)) {
	    NE_DEBUG(NE_DBG_HTTPAUTH, "Missing rspauth, cnonce or nc with qop.\n");
	} else { /* Have got rspauth, cnonce and nc */
	    if (strcmp(cnonce, sess->unq_cnonce) != 0) {
		NE_DEBUG(NE_DBG_HTTPAUTH, "Response cnonce doesn't match.\n");
	    } else if (nonce_count != sess->nonce_count) { 
		NE_DEBUG(NE_DBG_HTTPAUTH, "Response nonce count doesn't match.\n");
	    } else {
		/* Calculate and check the response-digest value.
		 * joe: IMO the spec is slightly ambiguous as to whether
		 * we use the qop which WE sent, or the qop which THEY
		 * sent...  */
		struct ne_md5_ctx a2;
		unsigned char a2_md5[16], rdig_md5[16];
		char a2_md5_ascii[33], rdig_md5_ascii[33];

		NE_DEBUG(NE_DBG_HTTPAUTH, "Calculating response-digest.\n");

		/* First off, H(A2) again. */
		ne_md5_init_ctx(&a2);
		ne_md5_process_bytes(":", 1, &a2);
		ne_md5_process_bytes(req->uri, strlen(req->uri), &a2);
		if (qop == auth_qop_auth_int) {
		    unsigned char heb_md5[16];
		    char heb_md5_ascii[33];
		    /* Add on ":" H(entity-body) */
		    ne_md5_finish_ctx(&req->response_body, heb_md5);
		    ne_md5_to_ascii(heb_md5, heb_md5_ascii);
		    ne_md5_process_bytes(":", 1, &a2);
		    ne_md5_process_bytes(heb_md5_ascii, 32, &a2);
		    NE_DEBUG(NE_DBG_HTTPAUTH, "Digested [:%s]\n", heb_md5_ascii);
		}
		ne_md5_finish_ctx(&a2, a2_md5);
		ne_md5_to_ascii(a2_md5, a2_md5_ascii);
		
		/* We have the stored digest-so-far of 
		 *   H(A1) ":" unq(nonce-value) 
		 *        [ ":" nc-value ":" unq(cnonce-value) ] for qop
		 * in sess->stored_rdig, to save digesting them again.
		 *
		 */
		if (qop != auth_qop_none) {
		    /* Add in qop-value */
		    NE_DEBUG(NE_DBG_HTTPAUTH, "Digesting qop-value [%s:].\n", 
			   qop_value);
		    ne_md5_process_bytes(qop_value, strlen(qop_value), 
				       &sess->stored_rdig);
		    ne_md5_process_bytes(":", 1, &sess->stored_rdig);
		}
		/* Digest ":" H(A2) */
		ne_md5_process_bytes(a2_md5_ascii, 32, &sess->stored_rdig);
		/* All done */
		ne_md5_finish_ctx(&sess->stored_rdig, rdig_md5);
		ne_md5_to_ascii(rdig_md5, rdig_md5_ascii);

		NE_DEBUG(NE_DBG_HTTPAUTH, "Calculated response-digest of: [%s]\n",
		       rdig_md5_ascii);
		NE_DEBUG(NE_DBG_HTTPAUTH, "Given response-digest of:      [%s]\n",
		       rspauth);

		/* And... do they match? */
		okay = (strcasecmp(rdig_md5_ascii, rspauth) == 0)?0:-1;
		NE_DEBUG(NE_DBG_HTTPAUTH, "Matched: %s\n", okay?"nope":"YES!");
	    }
	    free(rspauth);
	    free(cnonce);
	    free(nc);
	}
	free(qop_value);
    } else {
	NE_DEBUG(NE_DBG_HTTPAUTH, "No qop directive, auth okay.\n");
	okay = 0;
    }

    /* Check for a nextnonce */
    if (nextnonce != NULL) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "Found nextnonce of [%s].\n", nextnonce);
	if (sess->unq_nonce != NULL)
	    free(sess->unq_nonce);
	sess->unq_nonce = nextnonce;
    }

    return okay;
}

/* A new challenge presented by the server */
int auth_challenge(auth_session *sess, const char *value) 
{
    char **pairs, *pnt, *unquoted, *key;
    struct auth_challenge *chall = NULL, *challenges = NULL;
    int n, success;

    NE_DEBUG(NE_DBG_HTTPAUTH, "Got new auth challenge: %s\n", value);

    /* The header value may be made up of one or more challenges.
     * We split it down into attribute-value pairs, then search for
     * schemes in the pair keys.
     */
    pairs = pair_string(value, ',', '=', "\"'", " \r\n\t");

    for (n = 0; pairs[n]!=NULL; n+=2) {
	/* Look for an auth-scheme in the key */
	pnt = strchr(pairs[n], ' ');
	if (pnt != NULL) {
	    /* We have a new challenge */
	    NE_DEBUG(NE_DBG_HTTPAUTH, "New challenge.\n");
	    chall = ne_calloc(sizeof *chall);

	    chall->next = challenges;
	    challenges = chall;
	    /* Initialize the challenge parameters */
	    /* Which auth-scheme is it (case-insensitive matching) */
	    if (strncasecmp(pairs[n], "basic ", 6) == 0) {
		NE_DEBUG(NE_DBG_HTTPAUTH, "Basic scheme.\n");
		chall->scheme = auth_scheme_basic;
	    } else if (strncasecmp(pairs[n], "digest ", 7) == 0) {
		NE_DEBUG(NE_DBG_HTTPAUTH, "Digest scheme.\n");
		chall->scheme = auth_scheme_digest;
	    } else {
		NE_DEBUG(NE_DBG_HTTPAUTH, "Unknown scheme.\n");
		free(chall);
		challenges = NULL;
		break;
	    }
	    /* Now, the real key for this pair starts after the 
	     * auth-scheme... skipping whitespace */
	    while (strchr(" \r\n\t", *(++pnt)) != NULL)
		/* nullop */;
	    key = pnt;
	} else if (chall == NULL) {
	    /* If we haven't got an auth-scheme, and we're
	     * haven't yet found a challenge, skip this pair.
	     */
	    continue;
	} else {
	    key = pairs[n];
	}
	NE_DEBUG(NE_DBG_HTTPAUTH, "Got pair: [%s] = [%s]\n", key, pairs[n+1]);
	/* Most values are quoted, so unquote them here */
	unquoted = shave_string(pairs[n+1], '"');
	/* Now parse the attribute */
	NE_DEBUG(NE_DBG_HTTPAUTH, "Unquoted pair is: [%s]\n", unquoted);
	if (strcasecmp(key, "realm") == 0) {
	    chall->realm = pairs[n+1];
	} else if (strcasecmp(key, "nonce") == 0) {
	    chall->nonce = pairs[n+1];
	} else if (strcasecmp(key, "opaque") == 0) {
	    chall->opaque = pairs[n+1];
	} else if (strcasecmp(key, "domain") == 0) {
	    chall->domain = pairs[n+1];
	} else if (strcasecmp(key, "stale") == 0) {
	    /* Truth value */
	    chall->stale = 
		(strcasecmp(unquoted, "true") == 0);
	} else if (strcasecmp(key, "algorithm") == 0) {
	    if (strcasecmp(unquoted, "md5") == 0) {
		chall->alg = auth_alg_md5;
	    } else if (strcasecmp(unquoted, "md5-sess") == 0) {
		chall->alg = auth_alg_md5_sess;
	    } else {
		chall->alg = auth_alg_unknown;
	    }
	} else if (strcasecmp(key, "qop") == 0) {
	    char **qops;
	    int qop;
	    qops = split_string(unquoted, ',', NULL, " \r\n\t");
	    chall->got_qop = 1;
	    for (qop = 0; qops[qop] != NULL; qop++) {
		if (strcasecmp(qops[qop], "auth") == 0) {
		    chall->qop_auth = 1;
		} else if (strcasecmp(qops[qop], "auth-int") == 0) {
		    chall->qop_auth_int = 1;
		}
	    }
	    split_string_free(qops);
	}
	free(unquoted);
    }

    NE_DEBUG(NE_DBG_HTTPAUTH, "Finished parsing parameters.\n");

    /* Did we find any challenges */
    if (challenges == NULL) {
	pair_string_free(pairs);
	return -1;
    }
    
    success = 0;

    NE_DEBUG(NE_DBG_HTTPAUTH, "Looking for Digest challenges.\n");

    /* Try a digest challenge */
    for (chall = challenges; chall != NULL; chall = chall->next) {
	if (chall->scheme == auth_scheme_digest) {
	    if (!digest_challenge(sess, chall)) {
		success = 1;
		break;
	    }
	}
    }

    if (!success) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "No good Digest challenges, looking for Basic.\n");
	for (chall = challenges; chall != NULL; chall = chall->next) {
	    if (chall->scheme == auth_scheme_basic) {
		if (!basic_challenge(sess, chall)) {
		    success = 1;
		    break;
		}
	    }
	}

	if (!success) {
	    /* No good challenges - record this in the session state */
	    NE_DEBUG(NE_DBG_HTTPAUTH, "Did not understand any challenges.\n");
	}

    }
    
    /* Remember whether we can now supply the auth details */
    sess->can_handle = success;

    while (challenges != NULL) {
	chall = challenges->next;
	free(challenges);
	challenges = chall;
    }

    /* Free up the parsed header values */
    pair_string_free(pairs);

    return !success;
}

/* The body reader callback. */
static void auth_body_reader(void *cookie, const char *block, size_t length)
{
    struct ne_md5_ctx *ctx = cookie;
    NE_DEBUG(NE_DBG_HTTPAUTH, "Digesting %d bytes of response body.\n", length);
    ne_md5_process_bytes(block, length, ctx);
}

static void *ah_create(void *session, ne_request *request, const char *method,
		       const char *uri)
{
    auth_session *sess = session;
    struct auth_request *areq = ne_calloc(sizeof *areq);

    NE_DEBUG(NE_DBG_HTTPAUTH, "ah_create, for %s\n", sess->resp_hdr);

    areq->session = sess;
    areq->method = method;
    areq->uri = uri;
    areq->request = request;

    ne_add_response_header_handler(request, sess->resp_hdr,
				   ne_duplicate_header, &areq->auth_hdr);

	
    ne_add_response_header_handler(request, sess->resp_info_hdr,
				   ne_duplicate_header, 
				   &areq->auth_info_hdr);
    
    return areq;
}


static void ah_pre_send(void *cookie, ne_buffer *request)
{
    struct auth_request *req = cookie;
    auth_session *sess = req->session;


    if (!sess->can_handle) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "Not handling session.\n");
    } else if (!is_in_domain(sess, req->uri)) {
	/* We have moved out of the authentication domain */
	NE_DEBUG(NE_DBG_HTTPAUTH, 
	      "URI %s outside session domain, not handling.\n", req->uri);
	req->will_handle = 0;
    } else {
	char *value;

	NE_DEBUG(NE_DBG_HTTPAUTH, "Handling.");
	req->will_handle = 1;

	if (sess->qop == auth_qop_auth_int) {
	    /* Digest mode / qop=auth-int: take an MD5 digest of the
	     * response body. */
	    ne_md5_init_ctx(&req->response_body);
	    ne_add_response_body_reader(req->request, ne_accept_always, 
					auth_body_reader, &req->response_body);
	}
	
	switch(sess->scheme) {
	case auth_scheme_basic:
	    value = request_basic(sess);
	    break;
	case auth_scheme_digest:
	    value = request_digest(sess, req);
	    break;
	default:
	    value = NULL;
	    break;
	}

	if (value != NULL) {
	    ne_buffer_concat(request, sess->req_hdr, ": ", value, NULL);
	    free(value);
	}

	/* increase counter so we don't retry this >1. */
	req->tries++;

    }

}

#define SAFELY(x) ((x) != NULL?(x):"null")

static int ah_post_send(void *cookie, const ne_status *status)
{
    struct auth_request *areq = cookie;
    auth_session *sess = areq->session;
    int ret = NE_OK;

    NE_DEBUG(NE_DBG_HTTPAUTH, 
	  "ah_post_send (#%d), code is %d (want %d), %s is %s\n",
	  areq->tries, status->code, sess->status_code, 
	  SAFELY(sess->resp_hdr), SAFELY(areq->auth_hdr));
    if (areq->auth_info_hdr != NULL && 
	verify_response(areq, sess, areq->auth_info_hdr)) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "Response authentication invalid.\n");
	ne_set_error(sess->sess, sess->fail_msg);
	ret = NE_ERROR;
    } else if (status->code == sess->status_code && 
	       areq->auth_hdr != NULL && areq->tries == 0) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "Got challenge with code %d.\n", status->code);
	if (!auth_challenge(sess, areq->auth_hdr)) {
	    ret = NE_RETRY;
	}
    } else if (areq->tries > 0 && sess->status_code == status->code) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "Authentication failed - bad password?\n");
	clean_session(sess);
	ret = sess->fail_code;
    }

    NE_FREE(areq->auth_info_hdr);
    NE_FREE(areq->auth_hdr);
    
    return ret;
}

static void ah_destroy(void *cookie)
{
    free(cookie);
}

static const ne_request_hooks ah_server_hooks = {
    HOOK_SERVER_ID,
    ah_create,
    ah_pre_send,
    ah_post_send,
    ah_destroy
};

static const ne_request_hooks ah_proxy_hooks = {
    HOOK_PROXY_ID,
    ah_create,
    ah_pre_send,
    ah_post_send,
    ah_destroy
};

static void free_auth(void *cookie)
{
    auth_session *sess = cookie;

    clean_session(sess);
    free(sess);
}

void ne_set_server_auth(ne_session *sess, ne_request_auth callback, 
			  void *userdata)
{
    auth_session *auth_sess = auth_create(sess, callback, userdata);

    /* Server auth details */
    auth_sess->status_code = 401;
    auth_sess->fail_code = NE_AUTH;
    auth_sess->resp_hdr = "WWW-Authenticate";
    auth_sess->resp_info_hdr = "Authentication-Info";
    auth_sess->req_hdr = "Authorization";
    auth_sess->fail_msg = _("Server was not authenticated correctly.");

    ne_add_hooks(sess, &ah_server_hooks, auth_sess, free_auth);
}

void ne_set_proxy_auth(ne_session *sess, ne_request_auth callback, 
			 void *userdata)
{
    auth_session *auth_sess = auth_create(sess, callback, userdata);

    /* Proxy auth details */
    auth_sess->status_code = 407;
    auth_sess->fail_code = NE_PROXYAUTH;
    auth_sess->resp_hdr = "Proxy-Authenticate";
    auth_sess->resp_info_hdr = "Proxy-Authentication-Info";
    auth_sess->req_hdr = "Proxy-Authorization";
    auth_sess->fail_msg = _("Proxy server was not authenticated correctly.");

    ne_add_hooks(sess, &ah_proxy_hooks, auth_sess, free_auth);
}

void ne_forget_auth(ne_session *sess)
{
    clean_session(ne_session_hook_private(sess, HOOK_SERVER_ID));
    clean_session(ne_session_hook_private(sess, HOOK_PROXY_ID));
}

