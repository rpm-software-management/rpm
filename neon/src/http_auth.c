/* 
   HTTP Authentication routines
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

   Id: http_auth.c,v 1.8 2000/05/09 18:33:37 joe Exp 
*/


/* HTTP Authentication, as per RFC2617.
 * 
 * TODO:
 *  - Improve cnonce? Make it really random using /dev/random or whatever?
 *  - Test auth-int support
 */

#include <config.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <time.h>

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "base64.h"
#include "md5.h"
#include "dates.h"

#include "http_auth.h"
#include "string_utils.h"
#include "uri.h"
#include "http_utils.h"

/* The MD5 digest of a zero-length entity-body */
#define DIGEST_MD5_EMPTY "d41d8cd98f00b204e9800998ecf8427e"

/* A challenge */
struct http_auth_chall {
    http_auth_scheme scheme;
    char *realm;
    char *domain;
    char *nonce;
    char *opaque;
    unsigned int stale:1; /* if stale=true */
    unsigned int got_qop:1; /* we were given a qop directive */
    unsigned int qop_auth:1; /* "auth" token in qop attrib */
    unsigned int qop_auth_int:1; /* "auth-int" token in qop attrib */
    http_auth_algorithm alg;
    struct http_auth_chall *next;
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

/* Private prototypes */
static char *get_cnonce(void);
static void clean_session( http_auth_session *sess );
static int digest_challenge( http_auth_session *, struct http_auth_chall * );
static int basic_challenge( http_auth_session *, struct http_auth_chall * );
static char *request_digest( http_auth_session * );
static char *request_basic( http_auth_session * );

/* Domain handling */
static int is_in_domain( http_auth_session *sess, const char *uri );
static int parse_domain( http_auth_session *sess, const char *domain );

/* Get the credentials, passing a temporary store for the password value */
static int get_credentials( http_auth_session *sess, char **password );

/* Initialize an auth session */
void http_auth_init( http_auth_session *sess ) 
{
    memset( sess, 0, sizeof( http_auth_session ) );
}

http_auth_session *http_auth_create( void ) 
{
    http_auth_session *sess = xmalloc(sizeof(http_auth_session));
    http_auth_init( sess );
    return sess;
}

void http_auth_destroy( http_auth_session *sess ) 
{
    http_auth_finish( sess );
    free( sess );
}

void http_auth_set_creds_cb( http_auth_session *sess,
			     http_auth_request_creds callback, void *userdata )
{
    sess->reqcreds = callback;
    sess->reqcreds_udata = userdata;
}

#if 0
void http_auth_set_server( http_auth_session *sess, 
			   const char *host, unsigned int port, const char *scheme )
{
    sess->host = host;
    sess->port = port;
    sess->req_scheme = scheme;
}
#endif

/* Start a new request. */
void http_auth_new_request( http_auth_session *sess,
			    const char *method, const char *uri, 
			    const char *body_buffer, FILE *body_stream ) 
{
    if( !sess->can_handle ) {
	DEBUG( DEBUG_HTTPAUTH, "Not handling session.\n" );
    } else if( !is_in_domain( sess, uri ) ) {
	    /* We have moved out of the authentication domain */
	    DEBUG( DEBUG_HTTPAUTH, "URI %s outside session domain, not handling.\n", uri );
	    sess->will_handle = 0;
	} else 

    {
	DEBUG( DEBUG_HTTPAUTH, "URI %s inside session domain, will handle.\n", uri );

	sess->will_handle = 1;
	sess->uri = uri;
	sess->method = method;
	sess->got_body = (body_buffer!=NULL) || (body_stream!=NULL);
	sess->body_buffer = body_buffer;
	sess->body_stream = body_stream;
	md5_init_ctx( &sess->response_body );
    }
}

static void clean_session( http_auth_session *sess ) 
{
    sess->can_handle = 0;
    HTTP_FREE( sess->basic );
    HTTP_FREE( sess->unq_realm );
    HTTP_FREE( sess->unq_nonce );
    HTTP_FREE( sess->unq_cnonce );
    HTTP_FREE( sess->opaque );
    HTTP_FREE( sess->username );
    if( sess->domain_count > 0 ) {
	split_string_free( sess->domain );
	sess->domain_count = 0;
    }
}

void http_auth_finish( http_auth_session *sess ) {
    clean_session( sess );
}

/* Returns cnonce-value. We just use base64( time ).
 * TODO: Could improve this? */
static char *get_cnonce(void) 
{
    char *ret, *tmp;
    tmp = rfc1123_date( time(NULL) );
    ret = base64( tmp );
    free( tmp );
    return ret;
}

static int 
get_credentials( http_auth_session *sess, char **password) 
{
    return (*sess->reqcreds)( sess->reqcreds_udata, sess->unq_realm,
			      &sess->username, password );
}

static int parse_domain( http_auth_session *sess, const char *domain ) {
    char *unq, **doms;
    int count, ret;

    unq = shave_string(domain, '"' );
    doms = split_string_c( unq, ' ', NULL, HTTP_WHITESPACE, &count );
    if( doms != NULL ) {
	if( count > 0 ) {
	    sess->domain = doms;
	    sess->domain_count = count;
	    ret = 0;
	} else {
	    free( doms );
	    ret = -1;
	}
    } else {
	ret = -1;
    }
    free( unq );
    return ret;
}

/* Returns:
 *    0: if uri is in NOT in domain of session
 * else: uri IS in domain of session (or no domain known)
 */
static int is_in_domain( http_auth_session *sess, const char *uri )
{
#if 1
    return 1;
#else
    /* TODO: Need proper URI comparison for this to work. */
    int ret, dom;
    const char *abs_path;
    if( sess->domain_count == 0 ) {
	DEBUG( DEBUG_HTTPAUTH, "No domain, presuming okay.\n" );
	return 1;
    }
    ret = 0;
    abs_path = uri_abspath( uri );
    for( dom = 0; dom < sess->domain_count; dom++ ) {
	DEBUG( DEBUG_HTTPAUTH, "Checking domain: %s\n", sess->domain[dom] );
	if( uri_childof( sess->domain[dom], abs_path ) ) {
	    ret = 1;
	    break;
	}
    }
    return ret;
#endif
}

/* Add authentication creditials to a request */
char *http_auth_request_header( http_auth_session *sess ) 
{
    if( sess->will_handle ) {
	switch( sess->scheme ) {
	case http_auth_scheme_basic:
	    return request_basic( sess );
	    break;
	case http_auth_scheme_digest:
	    return request_digest( sess );
	    break;
	default:
	    break;
	}
    }
    return NULL;
}

/* Examine a Basic auth challenge.
 * Returns 0 if an valid challenge, else non-zero. */
static int 
basic_challenge( http_auth_session *sess, struct http_auth_chall *parms ) 
{
    char *tmp, *password;

    /* Verify challenge... must have a realm */
    if( parms->realm == NULL ) {
	return -1;
    }

    DEBUG( DEBUG_HTTPAUTH, "Got Basic challenge with realm [%s]\n", 
	   parms->realm );

    clean_session( sess );

    sess->unq_realm = shave_string(parms->realm, '"' );

    if( get_credentials( sess, &password ) ) {
	/* Failed to get credentials */
	HTTP_FREE( sess->unq_realm );
	return -1;
    }

    sess->scheme = http_auth_scheme_basic;

    CONCAT3( tmp, sess->username, ":", password?password:"" );
    sess->basic = base64( tmp );
    free( tmp );

    HTTP_FREE( password );

    return 0;
}

/* Add Basic authentication credentials to a request */
static char *request_basic( http_auth_session *sess ) 
{
    char *buf;
    CONCAT3( buf, "Basic ", sess->basic, "\r\n" );
    return buf;
}

/* Examine a digest challenge: return 0 if it is a valid Digest challenge,
 * else non-zero. */
static int digest_challenge( http_auth_session *sess,
			     struct http_auth_chall *parms ) 
{
    struct md5_ctx tmp;
    unsigned char tmp_md5[16];
    char *password;

    /* Do we understand this challenge? */
    if( parms->alg == http_auth_alg_unknown ) {
	DEBUG( DEBUG_HTTPAUTH, "Unknown algorithm.\n" );
	return -1;
    }
    if( (parms->alg == http_auth_alg_md5_sess) &&
	!( parms->qop_auth || parms->qop_auth_int ) ) {
	DEBUG( DEBUG_HTTPAUTH, "Server did not give qop with MD5-session alg.\n" );
	return -1;
    }
    if( (parms->realm==NULL) || (parms->nonce==NULL) ) {
	DEBUG( DEBUG_HTTPAUTH, "Challenge missing nonce or realm.\n" );
	return -1;
    }

    if( parms->stale ) {
	/* Just a stale response, don't need to get a new username/password */
	DEBUG( DEBUG_HTTPAUTH, "Stale digest challenge.\n" );
    } else {
	/* Forget the old session details */
	DEBUG( DEBUG_HTTPAUTH, "In digest challenge.\n" );

	clean_session( sess );
	sess->unq_realm = shave_string(parms->realm, '"' );

	/* Not a stale response: really need user authentication */
	if( get_credentials( sess, &password ) ) {
	    /* Failed to get credentials */
	    HTTP_FREE( sess->unq_realm );
	    return -1;
	}
    }
    sess->alg = parms->alg;
    sess->scheme = http_auth_scheme_digest;
    sess->unq_nonce = shave_string(parms->nonce, '"' );
    sess->unq_cnonce = get_cnonce();
    if( parms->domain ) {
	if( parse_domain( sess, parms->domain ) ) {
	    /* TODO: Handle the error? */
	}
    } else {
	sess->domain = NULL;
	sess->domain_count = 0;
    }
    if( parms->opaque != NULL ) {
	sess->opaque = xstrdup( parms->opaque ); /* don't strip the quotes */
    }
    
    if( parms->got_qop ) {
	/* What type of qop are we to apply to the message? */
	DEBUG( DEBUG_HTTPAUTH, "Got qop directive.\n" );
	sess->nonce_count = 0;
	if( parms->qop_auth_int ) {
	    sess->qop = http_auth_qop_auth_int;
	} else {
	    sess->qop = http_auth_qop_auth;
	}
    } else {
	/* No qop at all/ */
	sess->qop = http_auth_qop_none;
    }
    
    if( !parms->stale ) {
	/* Calculate H(A1).
	 * tmp = H( unq(username-value) ":" unq(realm-value) ":" passwd
	 */
	DEBUG( DEBUG_HTTPAUTH, "Calculating H(A1).\n" );
	md5_init_ctx( &tmp );
	md5_process_bytes( sess->username, strlen(sess->username), &tmp);
	md5_process_bytes( ":", 1, &tmp );
	md5_process_bytes( sess->unq_realm, strlen(sess->unq_realm), &tmp );
	md5_process_bytes( ":", 1, &tmp );
	if( password != NULL )
	    md5_process_bytes( password, strlen(password), &tmp);
	md5_finish_ctx( &tmp, tmp_md5 );
	if( sess->alg == http_auth_alg_md5_sess ) {
	    unsigned char a1_md5[16];
	    struct md5_ctx a1;
	    char tmp_md5_ascii[33];
	    /* Now we calculate the SESSION H(A1)
	     *    A1 = H( ...above...) ":" unq(nonce-value) ":" unq(cnonce-value) 
	     */
	    md5_to_ascii( tmp_md5, tmp_md5_ascii );
	    md5_init_ctx( &a1 );
	    md5_process_bytes( tmp_md5_ascii, 32, &a1 );
	    md5_process_bytes( ":", 1, &a1 );
	    md5_process_bytes( sess->unq_nonce, strlen(sess->unq_nonce), &a1 );
	    md5_process_bytes( ":", 1, &a1 );
	    md5_process_bytes( sess->unq_cnonce, strlen(sess->unq_cnonce), &a1 );
	    md5_finish_ctx( &a1, a1_md5 );
	    md5_to_ascii( a1_md5, sess->h_a1 );
	    DEBUG( DEBUG_HTTPAUTH, "Session H(A1) is [%s]\n", sess->h_a1 );
	} else {
	    md5_to_ascii( tmp_md5, sess->h_a1 );
	    DEBUG( DEBUG_HTTPAUTH, "H(A1) is [%s]\n", sess->h_a1 );
	}
	
	HTTP_FREE( password );

    }
    
    DEBUG( DEBUG_HTTPAUTH, "I like this Digest challenge.\n" );

    return 0;
}

/* Return Digest authentication credentials header value for the given
 * session. */
static char *request_digest( http_auth_session *sess ) 
{
    struct md5_ctx a2, rdig;
    unsigned char a2_md5[16], rdig_md5[16];
    char a2_md5_ascii[33], rdig_md5_ascii[33];
    char nc_value[9] = {0}, *ret;
    const char *qop_value; /* qop-value */
    size_t retlen;

    /* Increase the nonce-count */
    if( sess->qop != http_auth_qop_none ) {
	sess->nonce_count++;
	snprintf( nc_value, 9, "%08x", sess->nonce_count );
	DEBUG( DEBUG_HTTPAUTH, "Nonce count is %d, nc is [%s]\n", 
	       sess->nonce_count, nc_value );
    }
    qop_value = qop_values[sess->qop];

    /* Calculate H(A2). */
    md5_init_ctx( &a2 );
    md5_process_bytes( sess->method, strlen(sess->method), &a2 );
    md5_process_bytes( ":", 1, &a2 );
    md5_process_bytes( sess->uri, strlen(sess->uri), &a2 );
    if( sess->qop == http_auth_qop_auth_int ) {
	/* Calculate H(entity-body) */
	if( sess->got_body ) {
	    char tmp_md5_ascii[33], tmp_md5[16];
	    if( sess->body_stream != NULL ) {
		DEBUG( DEBUG_HTTPAUTH, "Digesting body stream.\n" );
		md5_stream( sess->body_stream, tmp_md5 );
		rewind( sess->body_stream ); /* leave it at the beginning */
	    } else if( sess->body_buffer ) {
		DEBUG( DEBUG_HTTPAUTH, "Digesting body buffer.\n" );
		md5_buffer( sess->body_buffer, strlen(sess->body_buffer), 
			    tmp_md5 );
	    }
	    md5_to_ascii( tmp_md5, tmp_md5_ascii );
	    DEBUG( DEBUG_HTTPAUTH, "H(entity-body) is [%s]\n", tmp_md5_ascii );
	    /* Append to A2 */
	    md5_process_bytes( ":", 1, &a2 );
	    md5_process_bytes( tmp_md5_ascii, 32, &a2 );
	} else {
	    /* No entity-body. */
	    DEBUG( DEBUG_HTTPAUTH, "Digesting empty entity-body.\n" );
	    md5_process_bytes( ":" DIGEST_MD5_EMPTY, 33, &a2 );
	}
    }
    md5_finish_ctx( &a2, a2_md5 );
    md5_to_ascii( a2_md5, a2_md5_ascii );
    DEBUG( DEBUG_HTTPAUTH, "H(A2): %s\n", a2_md5_ascii );

    DEBUG( DEBUG_HTTPAUTH, "Calculating Request-Digest.\n" );
    /* Now, calculation of the Request-Digest.
     * The first section is the regardless of qop value
     *     H(A1) ":" unq(nonce-value) ":" */
    md5_init_ctx( &rdig );

    /* Use the calculated H(A1) */
    md5_process_bytes( sess->h_a1, 32, &rdig );

    md5_process_bytes( ":", 1, &rdig );
    md5_process_bytes( sess->unq_nonce, strlen(sess->unq_nonce), &rdig );
    md5_process_bytes( ":", 1, &rdig );
    if( sess->qop != http_auth_qop_none ) {
	/* Add on:
	 *    nc-value ":" unq(cnonce-value) ":" unq(qop-value) ":"
	 */
	DEBUG( DEBUG_HTTPAUTH, "Have qop directive, digesting: [%s:%s:%s]\n",
	       nc_value, sess->unq_cnonce, qop_value );
	md5_process_bytes( nc_value, 8, &rdig );
	md5_process_bytes( ":", 1, &rdig );
	md5_process_bytes( sess->unq_cnonce, strlen(sess->unq_cnonce), &rdig );
	md5_process_bytes( ":", 1, &rdig );
	/* Store a copy of this structure (see note below) */
	sess->stored_rdig = rdig;
	md5_process_bytes( qop_value, strlen(qop_value), &rdig );
	md5_process_bytes( ":", 1, &rdig );
    } else {
	/* Store a copy of this structure... we do this because the
	 * calculation of the rspauth= field in the Auth-Info header 
	 * is the same as this digest, up to this point. */
	sess->stored_rdig = rdig;
    }
    /* And finally, H(A2) */
    md5_process_bytes( a2_md5_ascii, 32, &rdig );
    md5_finish_ctx( &rdig, rdig_md5 );
    md5_to_ascii( rdig_md5, rdig_md5_ascii );
    
    /* Buffer size calculation. */
    
    retlen = 
	6                                      /* Digest */
	+ 1 + 8 + 1 + 2 + strlen(sess->username)  /*  username="..." */
	+ 2 + 5 + 1 + 2 + strlen(sess->unq_realm) /* , realm="..." */
	+ 2 + 5 + 1 + 2 + strlen(sess->unq_nonce) /* , nonce="..." */
	+ 2 + 3 + 1 + 2 + strlen(sess->uri)       /* , uri="..." */
	+ 2 + 8 + 1 + 2 + 32                      /* , response="..." */
	+ 2 + 9 + 1 + strlen(algorithm_names[sess->alg]) /* , algorithm= */
	;

    if( sess->opaque != NULL )
	retlen += 2 + 6 + 1 + strlen(sess->opaque);   /* , opaque=... */

    if( sess->qop != http_auth_qop_none )
	retlen += 
	    2 + 6 + 2 + 1 + strlen(sess->unq_cnonce) +   /* , cnonce="..." */
	    2 + 2 + 1 + 8 +                       /* , nc=... */
	    2 + 3 + 1 + strlen(qop_values[sess->qop]) /* , qop=... */
	    ;

    retlen += 2;   /* \r\n */

    DEBUG( DEBUG_HTTPAUTH, "Calculated length of buffer: %d\n", retlen );

    ret = xmalloc( retlen + 1 );

    sprintf( ret,
	      "Digest username=\"%s\", realm=\"%s\""
	      ", nonce=\"%s\", uri=\"%s\", response=\"%s\""
	      ", algorithm=%s",
	      sess->username, sess->unq_realm, 
	      sess->unq_nonce, sess->uri, rdig_md5_ascii,
	      algorithm_names[sess->alg]);
    
    if( sess->opaque != NULL ) {
	/* We never unquote it, so it's still quoted here */
	strcat( ret, ", opaque=" );
	strcat( ret, sess->opaque );
    }

    if( sess->qop != http_auth_qop_none ) {
	/* Add in cnonce and nc-value fields */
	strcat( ret, ", cnonce=\"" );
	strcat( ret, sess->unq_cnonce );
	strcat( ret, "\", nc=" );
	strcat( ret, nc_value );
	strcat( ret, ", qop=" );
	strcat( ret, qop_values[sess->qop] );
    }

    DEBUG( DEBUG_HTTPAUTH, "Digest header field value:\n%s\n", ret );

    strcat( ret, "\r\n" );

    DEBUG( DEBUG_HTTPAUTH, "Calculated length: %d, actual length: %d\n", 
	   retlen, strlen( ret ) );
    
    return ret;
}

inline void http_auth_response_body( http_auth_session *sess, 
				     const char *buffer, size_t buffer_len ) 
{
    if( !sess->will_handle ||
	sess->scheme != http_auth_scheme_digest ) return;
    DEBUG( DEBUG_HTTPAUTH, "Digesting %d bytes of response body.\n",
	   buffer_len );
    md5_process_bytes( buffer, buffer_len, &sess->response_body );
}

/* Pass this the value of the 'Authentication-Info:' header field, if
 * one is received.
 * Returns:
 *    0 if it gives a valid authentication for the server 
 *    non-zero otherwise (don't believe the response in this case!).
 */
int http_auth_verify_response( http_auth_session *sess, const char *value ) 
{
    char **pairs;
    http_auth_qop qop = http_auth_qop_none;
    char *nextnonce = NULL, /* for the nextnonce= value */
	*rspauth = NULL, /* for the rspauth= value */
	*cnonce = NULL, /* for the cnonce= value */
	*nc = NULL, /* for the nc= value */
	*unquoted, *qop_value = NULL;
    int n, nonce_count, okay;
    
    if( !sess->will_handle ) {
	/* Ignore it */
	return 0;
    }
    
    if( sess->scheme != http_auth_scheme_digest ) {
	DEBUG( DEBUG_HTTPAUTH, "Found Auth-Info header not in response to Digest credentials - dodgy.\n" );
	return -1;
    }
    
    DEBUG (DEBUG_HTTPAUTH, "Auth-Info header: %s\n", value );

    pairs = pair_string( value, ',', '=', HTTP_QUOTES, HTTP_WHITESPACE );
    
    for( n = 0; pairs[n]!=NULL; n+=2 ) {
	unquoted = shave_string( pairs[n+1], '"' );
	if( strcasecmp( pairs[n], "qop" ) == 0 ) {
	    qop_value = xstrdup( pairs[n+1] );
	    if( strcasecmp( pairs[n+1], "auth-int" ) == 0 ) {
		qop = http_auth_qop_auth_int;
	    } else if( strcasecmp( pairs[n+1], "auth" ) == 0 ) {
		qop = http_auth_qop_auth;
	    } else {
		qop = http_auth_qop_none;
	    }
	} else if( strcasecmp( pairs[n], "nextnonce" ) == 0 ) {
	    nextnonce = xstrdup( unquoted );
	} else if( strcasecmp( pairs[n], "rspauth" ) == 0 ) {
	    rspauth = xstrdup( unquoted );
	} else if( strcasecmp( pairs[n], "cnonce" ) == 0 ) {
	    cnonce = xstrdup( unquoted );
	} else if( strcasecmp( pairs[n], "nc" ) == 0 ) { 
	    nc = xstrdup( pairs[n] );
	    if( sscanf( pairs[n+1], "%x", &nonce_count ) != 1 ) {
		DEBUG( DEBUG_HTTPAUTH, "Couldn't scan [%s] for nonce count.\n",
		       pairs[n+1] );
	    } else {
		DEBUG( DEBUG_HTTPAUTH, "Got nonce_count: %d\n", nonce_count );
	    }
	}
	free( unquoted );
    }
    pair_string_free( pairs );

    /* Presume the worst */
    okay = -1;

    if( (qop != http_auth_qop_none) && (qop_value != NULL) ) {
	if( (rspauth == NULL) || (cnonce == NULL) || (nc == NULL) ) {
	    DEBUG( DEBUG_HTTPAUTH, "Missing rspauth, cnonce or nc with qop.\n" );
	} else { /* Have got rspauth, cnonce and nc */
	    if( strcmp( cnonce, sess->unq_cnonce ) != 0 ) {
		DEBUG( DEBUG_HTTPAUTH, "Response cnonce doesn't match.\n" );
	    } else if( nonce_count != sess->nonce_count ) { 
		DEBUG( DEBUG_HTTPAUTH, "Response nonce count doesn't match.\n" );
	    } else {
		/* Calculate and check the response-digest value.
		 * joe: IMO the spec is slightly ambiguous as to whether
		 * we use the qop which WE sent, or the qop which THEY
		 * sent...  */
		struct md5_ctx a2;
		unsigned char a2_md5[16], rdig_md5[16];
		char a2_md5_ascii[33], rdig_md5_ascii[33];

		DEBUG( DEBUG_HTTPAUTH, "Calculating response-digest.\n" );

		/* First off, H(A2) again. */
		md5_init_ctx( &a2 );
		md5_process_bytes( ":", 1, &a2 );
		md5_process_bytes( sess->uri, strlen(sess->uri), &a2 );
		if( qop == http_auth_qop_auth_int ) {
		    unsigned char heb_md5[16];
		    char heb_md5_ascii[33];
		    /* Add on ":" H(entity-body) */
		    md5_finish_ctx( &sess->response_body, heb_md5 );
		    md5_to_ascii( heb_md5, heb_md5_ascii );
		    md5_process_bytes( ":", 1, &a2 );
		    md5_process_bytes( heb_md5_ascii, 32, &a2 );
		    DEBUG( DEBUG_HTTPAUTH, "Digested [:%s]\n", heb_md5_ascii );
		}
		md5_finish_ctx( &a2, a2_md5 );
		md5_to_ascii( a2_md5, a2_md5_ascii );
		
		/* We have the stored digest-so-far of 
		 *   H(A1) ":" unq(nonce-value) 
		 *        [ ":" nc-value ":" unq(cnonce-value) ] for qop
		 * in sess->stored_rdig, to save digesting them again.
		 *
		 */
		if( qop != http_auth_qop_none ) {
		    /* Add in qop-value */
		    DEBUG( DEBUG_HTTPAUTH, "Digesting qop-value [%s:].\n", 
			   qop_value );
		    md5_process_bytes( qop_value, strlen(qop_value), 
				       &sess->stored_rdig );
		    md5_process_bytes( ":", 1, &sess->stored_rdig );
		}
		/* Digest ":" H(A2) */
		md5_process_bytes( a2_md5_ascii, 32, &sess->stored_rdig );
		/* All done */
		md5_finish_ctx( &sess->stored_rdig, rdig_md5 );
		md5_to_ascii( rdig_md5, rdig_md5_ascii );

		DEBUG( DEBUG_HTTPAUTH, "Calculated response-digest of: [%s]\n",
		       rdig_md5_ascii );
		DEBUG( DEBUG_HTTPAUTH, "Given response-digest of:      [%s]\n",
		       rspauth );

		/* And... do they match? */
		okay = (strcasecmp( rdig_md5_ascii, rspauth ) == 0)?0:-1;
		DEBUG( DEBUG_HTTPAUTH, "Matched: %s\n", okay?"nope":"YES!" );
	    }
	    free( rspauth );
	    free( cnonce );
	    free( nc );
	}
	free( qop_value );
    } else {
	DEBUG( DEBUG_HTTPAUTH, "No qop directive, auth okay.\n" );
	okay = 0;
    }

    /* Check for a nextnonce */
    if( nextnonce != NULL ) {
	DEBUG( DEBUG_HTTPAUTH, "Found nextnonce of [%s].\n", nextnonce );
	if( sess->unq_nonce != NULL )
	    free( sess->unq_nonce );
	sess->unq_nonce = nextnonce;
    }

    return okay;
}

/* A new challenge presented by the server */
int http_auth_challenge( http_auth_session *sess, const char *value ) 
{
    char **pairs, *pnt, *unquoted, *key;
    struct http_auth_chall *chall = NULL, *challenges = NULL;
    int n, success;

    DEBUG( DEBUG_HTTPAUTH, "Got new auth challenge: %s\n", value );

    /* The header value may be made up of one or more challenges.
     * We split it down into attribute-value pairs, then search for
     * schemes in the pair keys.
     */
    pairs = pair_string( value, ',', '=', HTTP_QUOTES, HTTP_WHITESPACE );

    for( n = 0; pairs[n]!=NULL; n+=2 ) {
	/* Look for an auth-scheme in the key */
	pnt = strchr( pairs[n], ' ' );
	if( pnt != NULL ) {
	    /* We have a new challenge */
	    DEBUG( DEBUG_HTTPAUTH, "New challenge.\n" );
	    chall = xmalloc( sizeof(struct http_auth_chall) );
	    memset( chall, 0, sizeof(struct http_auth_chall) );
	    chall->next = challenges;
	    challenges = chall;
	    /* Initialize the challenge parameters */
	    /* Which auth-scheme is it (case-insensitive matching) */
	    if( strncasecmp( pairs[n], "basic ", 6 ) == 0 ) {
		DEBUG( DEBUG_HTTPAUTH, "Basic scheme.\n" );
		chall->scheme = http_auth_scheme_basic;
	    } else if( strncasecmp( pairs[n], "digest ", 7 ) == 0 ) {
		DEBUG( DEBUG_HTTPAUTH, "Digest scheme.\n" );
		chall->scheme = http_auth_scheme_digest;
	    } else {
		DEBUG( DEBUG_HTTPAUTH, "Unknown scheme.\n" );
		free( chall );
		challenges = NULL;
		break;
	    }
	    /* Now, the real key for this pair starts after the 
	     * auth-scheme... skipping whitespace */
	    while( strchr( HTTP_WHITESPACE, *(++pnt) ) != NULL )
		/* nullop */;
	    key = pnt;
	} else if( chall == NULL ) {
	    /* If we haven't got an auth-scheme, and we're
	     * haven't yet found a challenge, skip this pair.
	     */
	    continue;
	} else {
	    key = pairs[n];
	}
	DEBUG( DEBUG_HTTPAUTH, "Got pair: [%s] = [%s]\n", key, pairs[n+1] );
	/* Most values are quoted, so unquote them here */
	unquoted = shave_string( pairs[n+1], '"' );
	/* Now parse the attribute */
	DEBUG( DEBUG_HTTPAUTH, "Unquoted pair is: [%s]\n", unquoted );
	if( strcasecmp( key, "realm" ) == 0 ) {
	    chall->realm = pairs[n+1];
	} else if( strcasecmp( key, "nonce" ) == 0 ) {
	    chall->nonce = pairs[n+1];
	} else if( strcasecmp( key, "opaque" ) == 0 ) {
	    chall->opaque = pairs[n+1];
	} else if( strcasecmp( key, "domain" ) == 0 ) {
	    chall->domain = pairs[n+1];
	} else if( strcasecmp( key, "stale" ) == 0 ) {
	    /* Truth value */
	    chall->stale = 
		( strcasecmp( unquoted, "true" ) == 0 );
	} else if( strcasecmp( key, "algorithm" ) == 0 ) {
	    if( strcasecmp( unquoted, "md5" ) == 0 ) {
		chall->alg = http_auth_alg_md5;
	    } else if( strcasecmp( unquoted, "md5-sess" ) == 0 ) {
		chall->alg = http_auth_alg_md5_sess;
	    } else {
		chall->alg = http_auth_alg_unknown;
	    }
	} else if( strcasecmp( key, "qop" ) == 0 ) {
	    char **qops;
	    int qop;
	    qops = split_string( unquoted, ',', NULL, HTTP_WHITESPACE );
	    chall->got_qop = 1;
	    for( qop = 0; qops[qop] != NULL; qop++ ) {
		if( strcasecmp( qops[qop], "auth" ) == 0 ) {
		    chall->qop_auth = 1;
		} else if( strcasecmp( qops[qop], "auth-int" ) == 0 ) {
		    chall->qop_auth_int = 1;
		}
	    }
	    split_string_free( qops );
	}
	free( unquoted );
    }

    DEBUG( DEBUG_HTTPAUTH, "Finished parsing parameters.\n" );

    /* Did we find any challenges */
    if( challenges == NULL ) {
	pair_string_free( pairs );
	return -1;
    }
    
    success = 0;

    DEBUG( DEBUG_HTTPAUTH, "Looking for Digest challenges.\n" );

    /* Try a digest challenge */
    for( chall = challenges; chall != NULL; chall = chall->next ) {
	if( chall->scheme == http_auth_scheme_digest ) {
	    if( !digest_challenge( sess, chall ) ) {
		success = 1;
		break;
	    }
	}
    }

    if( !success ) {
	DEBUG( DEBUG_HTTPAUTH, "No good Digest challenges, looking for Basic.\n" );
	for( chall = challenges; chall != NULL; chall = chall->next ) {
	    if( chall->scheme == http_auth_scheme_basic ) {
		if( !basic_challenge( sess, chall ) ) {
		    success = 1;
		    break;
		}
	    }
	}

	if( !success ) {
	    /* No good challenges - record this in the session state */
	    DEBUG( DEBUG_HTTPAUTH, "Did not understand any challenges.\n" );
	}

    }
    
    /* Remember whether we can now supply the auth details */
    sess->can_handle = success;

    while( challenges != NULL ) {
	chall = challenges->next;
	free( challenges );
	challenges = chall;
    }

    /* Free up the parsed header values */
    pair_string_free( pairs );

    return !success;
}
