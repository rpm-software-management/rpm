/*
   neon SSL/TLS support using GNU TLS
   Copyright (C) 2002-2004, Joe Orton <joe@manyfish.co.uk>
   Copyright (C) 2004, Aleix Conchillo Flaque <aleix@member.fsf.org>

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

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <gnutls/gnutls.h>
#include <gnutls/pkcs12.h>

#include "ne_ssl.h"
#include "ne_string.h"
#include "ne_session.h"
#include "ne_i18n.h"

#include "ne_private.h"
#include "ne_privssl.h"

struct ne_ssl_dname_s {
    int subject; /* non-zero if this is the subject DN object */
    gnutls_x509_crt cert;
};

struct ne_ssl_certificate_s {
    ne_ssl_dname subj_dn, issuer_dn;
    gnutls_x509_crt subject;
    ne_ssl_certificate *issuer;
    char *identity;
};

struct ne_ssl_client_cert_s {
    gnutls_pkcs12 p12;
    int decrypted; /* non-zero if successfully decrypted. */
    ne_ssl_certificate cert;
    gnutls_x509_privkey pkey;
    char *friendly_name;
};

/* Returns the highest used index in subject (or issuer) DN of
 * certificate CERT for OID, or -1 if no RDNs are present in the DN
 * using that OID. */
static int oid_find_highest_index(gnutls_x509_crt cert, int subject, const char *oid)
{
    int ret, idx = -1;

    do {
        size_t len = 0;

        if (subject)
            ret = gnutls_x509_crt_get_dn_by_oid(cert, oid, ++idx, 0, 
                                                NULL, &len);
        else
            ret = gnutls_x509_crt_get_issuer_dn_by_oid(cert, oid, ++idx, 0, 
                                                       NULL, &len);
    } while (ret == GNUTLS_E_SHORT_MEMORY_BUFFER);
    
    return idx - 1;
}

/* Appends the value of RDN with given oid from certitifcate x5
 * subject (if subject is non-zero), or issuer DN to buffer 'buf': */
static void append_rdn(ne_buffer *buf, gnutls_x509_crt x5, int subject, const char *oid)
{
    int idx, top, ret;
    char rdn[50];

    top = oid_find_highest_index(x5, subject, oid);
    
    for (idx = top; idx >= 0; idx--) {
        size_t rdnlen = sizeof rdn;

        if (subject)
            ret = gnutls_x509_crt_get_dn_by_oid(x5, oid, idx, 0, rdn, &rdnlen);
        else
            ret = gnutls_x509_crt_get_issuer_dn_by_oid(x5, oid, idx, 0, rdn, &rdnlen);
        
        if (ret < 0)
            return;

        if (buf->used > 1) {
            ne_buffer_append(buf, ", ", 2);
        }
        
        ne_buffer_append(buf, rdn, rdnlen);
    }
}


char *ne_ssl_readable_dname(const ne_ssl_dname *name)
{
    ne_buffer *buf = ne_buffer_create();
#if 0
    /* this code can be used once there is a released version of GnuTLS
     * with fixed _get_dn_oid functions */
    int ret, idx = 0;

    do {
        char oid[32] = {0};
        size_t oidlen = sizeof oid;
        
        ret = name->subject 
            ? gnutls_x509_crt_get_dn_oid(name->cert, idx, oid, &oidlen)
            : gnutls_x509_crt_get_issuer_dn_oid(name->cert, idx, oid, &oidlen);
        
        if (ret == 0) {
            append_rdn(buf, name->cert, name->subject, oid);
            idx++;
        }
    } while (ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);
#else

#define APPEND_RDN(x) append_rdn(buf, name->cert, name->subject, GNUTLS_OID_##x)

    APPEND_RDN(X520_ORGANIZATIONAL_UNIT_NAME);
    APPEND_RDN(X520_ORGANIZATION_NAME);
    APPEND_RDN(X520_LOCALITY_NAME);
    APPEND_RDN(X520_STATE_OR_PROVINCE_NAME);
    APPEND_RDN(X520_COUNTRY_NAME);

    if (buf->used == 1) APPEND_RDN(X520_COMMON_NAME);
    if (buf->used == 1) APPEND_RDN(PKCS9_EMAIL);

#undef APPEND_RDN
#endif

    return ne_buffer_finish(buf);
}

int ne_ssl_dname_cmp(const ne_ssl_dname *dn1, const ne_ssl_dname *dn2)
{
#warning incomplete
    return 1;
}

void ne_ssl_clicert_free(ne_ssl_client_cert *cc)
{
    if (cc->p12)
        gnutls_pkcs12_deinit(cc->p12);
    if (cc->decrypted) {
        if (cc->cert.identity) ne_free(cc->cert.identity);
        if (cc->pkey) gnutls_x509_privkey_deinit(cc->pkey);
        if (cc->cert.subject) gnutls_x509_crt_deinit(cc->cert.subject);
    }
    if (cc->friendly_name) ne_free(cc->friendly_name);
    ne_free(cc);
}

void ne_ssl_cert_validity(const ne_ssl_certificate *cert,
                          char *from, char *until)
{
#warning FIXME strftime not portable
    if (from) {
        time_t t = gnutls_x509_crt_get_activation_time(cert->subject);
        strftime(from, NE_SSL_VDATELEN, "%b %d %H:%M:%S %Y %Z", localtime(&t));
    }
    if (until) {
        time_t t = gnutls_x509_crt_get_expiration_time(cert->subject);
        strftime(until, NE_SSL_VDATELEN, "%b %d %H:%M:%S %Y %Z", localtime(&t));
    }
}

/* Return non-zero if hostname from certificate (cn) matches hostname
 * used for session (hostname).  (Wildcard matching is no longer
 * mandated by RFC3280, but certs are deployed which use wildcards) */
static int match_hostname(char *cn, const char *hostname)
{
    const char *dot;
    NE_DEBUG(NE_DBG_SSL, "Match %s on %s...\n", cn, hostname);
    dot = strchr(hostname, '.');
    if (dot == NULL) {
	char *pnt = strchr(cn, '.');
	/* hostname is not fully-qualified; unqualify the cn. */
	if (pnt != NULL) {
	    *pnt = '\0';
	}
    }
    else if (strncmp(cn, "*.", 2) == 0) {
	hostname = dot + 1;
	cn += 2;
    }
    return !strcasecmp(cn, hostname);
}

/* Check certificate identity.  Returns zero if identity matches; 1 if
 * identity does not match, or <0 if the certificate had no identity.
 * If 'identity' is non-NULL, store the malloc-allocated identity in
 * *identity.  If 'server' is non-NULL, it must be the network address
 * of the server in use, and identity must be NULL. */
static int check_identity(const char *hostname, gnutls_x509_crt cert,
                          char **identity)
{
    char name[255];
    unsigned int critical;
    int ret, seq = 0;
    int match = 0, found = 0;
    size_t len;

    do {
        len = sizeof name;
        ret = gnutls_x509_crt_get_subject_alt_name(cert, seq, name, &len,
                                                   &critical);
        switch (ret) {
        case GNUTLS_SAN_DNSNAME:
            if (identity && !found) *identity = ne_strdup(name);
            match = match_hostname(name, hostname);
            found = 1;
            break;
        default:
            break;
        }
        seq++;
    } while (!match && ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);

    /* Check against the commonName if no DNS alt. names were found,
     * as per RFC3280. */
    if (!found) {
        seq = oid_find_highest_index(cert, 1, GNUTLS_OID_X520_COMMON_NAME);

        if (seq >= 0) {
            len = sizeof name;
            name[0] = '\0';
            ret = gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME,
                                                seq, 0, name, &len);
            if (ret == 0) {
                if (identity) *identity = ne_strdup(name);
                match = match_hostname(name, hostname);
            }
        } else {
            return -1;
        }
    }

    NE_DEBUG(NE_DBG_SSL, "Identity match: %s\n", match ? "good" : "bad");
    return match ? 0 : 1;
}

/* Populate an ne_ssl_certificate structure from an X509 object. */
static ne_ssl_certificate *populate_cert(ne_ssl_certificate *cert,
                                         gnutls_x509_crt x5)
{
    cert->subj_dn.cert = x5;
    cert->subj_dn.subject = 1;
    cert->issuer_dn.cert = x5;
    cert->issuer_dn.subject = 0;
    cert->issuer = NULL;
    cert->subject = x5;
    cert->identity = NULL;
    check_identity("", x5, &cert->identity);
    return cert;
}

/* Returns a copy certificate of certificate SRC. */
static gnutls_x509_crt x509_crt_copy(gnutls_x509_crt src)
{
    int ret;
    size_t size;
    gnutls_datum tmp;
    gnutls_x509_crt dest;
    
    if (gnutls_x509_crt_init(&dest) == 0) {
        return NULL;
    }

    if (gnutls_x509_crt_export(src, GNUTLS_X509_FMT_DER, NULL, &size) 
        != GNUTLS_E_SHORT_MEMORY_BUFFER) {
        gnutls_x509_crt_deinit(dest);
        return NULL;
    }

    tmp.data = ne_malloc(size);
    ret = gnutls_x509_crt_export(src, GNUTLS_X509_FMT_DER, tmp.data, &size);
    if (ret == 0) {
        tmp.size = size;
        ret = gnutls_x509_crt_import(dest, &tmp, GNUTLS_X509_FMT_DER);
    }

    if (ret) {
        gnutls_x509_crt_deinit(dest);
        dest = NULL;
    }

    ne_free(tmp.data);
    return dest;
}

/* Duplicate a client certificate, which must be in the decrypted state. */
static ne_ssl_client_cert *dup_client_cert(const ne_ssl_client_cert *cc)
{
    int ret;
    ne_ssl_client_cert *newcc = ne_calloc(sizeof *newcc);

    newcc->decrypted = 1;

    ret = gnutls_x509_privkey_init(&newcc->pkey);
    if (ret != 0) goto dup_error;

    ret = gnutls_x509_privkey_cpy(newcc->pkey, cc->pkey);
    if (ret != 0) goto dup_error;
    
    newcc->cert.subject = x509_crt_copy(cc->cert.subject);
    if (!newcc->cert.subject) goto dup_error;

    if (cc->friendly_name) newcc->friendly_name = ne_strdup(cc->friendly_name);

    populate_cert(&newcc->cert, newcc->cert.subject);
    return newcc;

dup_error:
    if (newcc->pkey) gnutls_x509_privkey_deinit(newcc->pkey);
    if (newcc->cert.subject) gnutls_x509_crt_deinit(newcc->cert.subject);
    ne_free(newcc);
    return NULL;
}    

void ne_ssl_set_clicert(ne_session *sess, const ne_ssl_client_cert *cc)
{
    sess->client_cert = dup_client_cert(cc);
}

ne_ssl_context *ne_ssl_context_create(int flags)
{
    ne_ssl_context *ctx = ne_malloc(sizeof *ctx);
    gnutls_certificate_allocate_credentials(&ctx->cred);
    return ctx;
}

int ne_ssl_context_keypair(ne_ssl_context *ctx, 
                           const char *cert, const char *key)
{
    gnutls_certificate_set_x509_key_file(ctx->cred, cert, key,
                                         GNUTLS_X509_FMT_PEM);
    return 0;
}

int ne_ssl_context_set_verify(ne_ssl_context *ctx, int required,
                              const char *ca_names, const char *verify_cas)
{
    if (verify_cas) {
        gnutls_certificate_set_x509_trust_file(ctx->cred, verify_cas,
                                               GNUTLS_X509_FMT_PEM);
    }
#warning argh
    return 0;
}


void ne_ssl_context_destroy(ne_ssl_context *ctx)
{
    gnutls_certificate_free_credentials(ctx->cred);
    ne_free(ctx);
}

/* Return the certificate chain sent by the peer, or NULL on error. */
static ne_ssl_certificate *make_peers_chain(gnutls_session sock)
{
    ne_ssl_certificate *current = NULL, *top = NULL;
    const gnutls_datum *certs;
    unsigned int n, count;

    certs = gnutls_certificate_get_peers(sock, &count);
    if (!certs) {
        return NULL;
    }
    
    for (n = 0; n < count; n++) {
        ne_ssl_certificate *cert = ne_malloc(sizeof *cert);
        gnutls_x509_crt x5;

        if (gnutls_x509_crt_init(&x5) ||
            gnutls_x509_crt_import(x5, &certs[n], GNUTLS_X509_FMT_DER)) {
            /* leak! */
            return NULL;
        }

        populate_cert(cert, x5);
        
        if (top == NULL) {
            current = top = cert;
        } else {
            current->issuer = cert;
            current = cert;
        }
    }
    
    return top;
}

/* Verifies an SSL server certificate. */
static int check_certificate(ne_session *sess, gnutls_session sock,
                             ne_ssl_certificate *chain)
{
    time_t before, after, now = time(NULL);
    int ret, failures = 0;

    before = gnutls_x509_crt_get_activation_time(chain->subject);
    after = gnutls_x509_crt_get_expiration_time(chain->subject);

    if (now < before)
        failures |= NE_SSL_NOTYETVALID;
    else if (now > after)
        failures |= NE_SSL_EXPIRED;

    ret = check_identity(sess->server.hostname, chain->subject, NULL);
    if (ret < 0) {
        ne_set_error(sess, _("Server certificate was missing commonName "
                             "attribute in subject name"));
        return NE_ERROR;
    } else if (ret > 0) {
        failures |= NE_SSL_IDMISMATCH;
    }

    if (gnutls_certificate_verify_peers(sock)) {
        failures |= NE_SSL_UNTRUSTED;
    }

    NE_DEBUG(NE_DBG_SSL, "Failures = %d\n", failures);

    if (failures == 0) {
        ret = NE_OK;
    } else {
#warning TODO: set up error string
        ret = NE_ERROR;
        if (sess->ssl_verify_fn
            && sess->ssl_verify_fn(sess->ssl_verify_ud, failures, chain) == 0)
            ret = NE_OK;
    }

    return ret;
}

/* Negotiate an SSL connection. */
int ne__negotiate_ssl(ne_request *req)
{
    ne_session *const sess = ne_get_session(req);
    ne_ssl_context *const ctx = sess->ssl_context;
    ne_ssl_certificate *chain;
    gnutls_session sock;

    NE_DEBUG(NE_DBG_SSL, "Negotiating SSL connection.\n");

    if (ne_sock_connect_ssl(sess->socket, ctx)) {
	ne_set_error(sess, _("SSL negotiation failed: %s"),
		     ne_sock_error(sess->socket));
	return NE_ERROR;
    }

    sock = ne__sock_sslsock(sess->socket);

    chain = make_peers_chain(sock);
    if (chain == NULL) {
        ne_set_error(sess, _("Server did not send certificate chain"));
        return NE_ERROR;
    }

    if (check_certificate(sess, sock, chain)) {
        ne_ssl_cert_free(chain);
        return NE_ERROR;
    }

    return NE_OK;
}

const ne_ssl_dname *ne_ssl_cert_issuer(const ne_ssl_certificate *cert)
{
    return &cert->issuer_dn;
}

const ne_ssl_dname *ne_ssl_cert_subject(const ne_ssl_certificate *cert)
{
    return &cert->subj_dn;
}

const ne_ssl_certificate *ne_ssl_cert_signedby(const ne_ssl_certificate *cert)
{
    return cert->issuer;
}

const char *ne_ssl_cert_identity(const ne_ssl_certificate *cert)
{
    return cert->identity;
}

void ne_ssl_context_trustcert(ne_ssl_context *ctx, const ne_ssl_certificate *cert)
{
    gnutls_x509_crt certs = cert->subject;
    gnutls_certificate_set_x509_trust(ctx->cred, &certs, 1);
}

void ne_ssl_trust_default_ca(ne_session *sess)
{
#warning incomplete
}

/* Read the contents of file FILENAME into *DATUM. */
static int read_to_datum(const char *filename, gnutls_datum *datum)
{
    FILE *f = fopen(filename, "r");
    ne_buffer *buf;
    char tmp[4192];
    size_t len;

    if (!f) {
        return -1;
    }

    buf = ne_buffer_ncreate(8192);
    while ((len = fread(tmp, 1, sizeof tmp, f)) > 0) {
        ne_buffer_append(buf, tmp, len);
    }

    if (!feof(f)) {
        ne_buffer_destroy(buf);
        return -1;
    }
    
    datum->size = ne_buffer_size(buf);
    datum->data = ne_buffer_finish(buf);
    return 0;
}

/* Parses a PKCS#12 structure and loads the certificate, private key
 * and friendly name if possible.  Returns zero on success, non-zero
 * on error. */
static int pkcs12_parse(gnutls_pkcs12 p12, gnutls_x509_privkey *pkey,
                        gnutls_x509_crt *x5, char **friendly_name,
                        const char *password)
{
    gnutls_pkcs12_bag bag = NULL;
    int i, j, ret = 0;

    for (i = 0; ret == 0; ++i) {
        if (bag) gnutls_pkcs12_bag_deinit(bag);

        ret = gnutls_pkcs12_bag_init(&bag);
        if (ret < 0) continue;

        ret = gnutls_pkcs12_get_bag(p12, i, bag);
        if (ret < 0) continue;

        gnutls_pkcs12_bag_decrypt(bag, password == NULL ? "" : password);

        for (j = 0; ret == 0 && j < gnutls_pkcs12_bag_get_count(bag); ++j) {
            gnutls_pkcs12_bag_type type;
            gnutls_datum data;

            if (friendly_name && *friendly_name == NULL) {
                char *name;
                gnutls_pkcs12_bag_get_friendly_name(bag, j, &name);
                if (name) {
                    if (name[0] == '.') name++; /* weird GnuTLS bug? */
                    *friendly_name = ne_strdup(name);
                }
            }

            type = gnutls_pkcs12_bag_get_type(bag, j);
            switch (type) {
            case GNUTLS_BAG_PKCS8_KEY:
            case GNUTLS_BAG_PKCS8_ENCRYPTED_KEY:
                gnutls_x509_privkey_init(pkey);

                ret = gnutls_pkcs12_bag_get_data(bag, j, &data);
                if (ret < 0) continue;

                ret = gnutls_x509_privkey_import_pkcs8(*pkey, &data,
                                                       GNUTLS_X509_FMT_DER,
                                                       password,
                                                       0);
                if (ret < 0) continue;
                break;
            case GNUTLS_BAG_CERTIFICATE:
                gnutls_x509_crt_init(x5);

                ret = gnutls_pkcs12_bag_get_data(bag, j, &data);
                if (ret < 0) continue;

                ret = gnutls_x509_crt_import(*x5, &data, GNUTLS_X509_FMT_DER);
                if (ret < 0) continue;

                break;
            default:
                break;
            }
        }
    }

    /* Make sure last bag is freed */
    if (bag) gnutls_pkcs12_bag_deinit(bag);

    /* Free in case of error */
    if (ret < 0 && ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) {
        if (*x5) gnutls_x509_crt_deinit(*x5);
        if (*pkey) gnutls_x509_privkey_deinit(*pkey);
        if (friendly_name && *friendly_name) ne_free(*friendly_name);
    }

    if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) ret = 0;
    return ret;
}

ne_ssl_client_cert *ne_ssl_clicert_read(const char *filename)
{
    int ret;
    gnutls_datum data;
    gnutls_pkcs12 p12;
    ne_ssl_client_cert *cc;
    char *friendly_name = NULL;
    gnutls_x509_crt cert = NULL;
    gnutls_x509_privkey pkey = NULL;

    if (read_to_datum(filename, &data))
        return NULL;

    if (gnutls_pkcs12_init(&p12) != 0) {
        return NULL;
    }

    ret = gnutls_pkcs12_import(p12, &data, GNUTLS_X509_FMT_DER, 0);
    ne_free(data.data);
    if (ret < 0) {
        gnutls_pkcs12_deinit(p12);
        return NULL;
    }

    ret = pkcs12_parse(p12, &pkey, &cert, &friendly_name, NULL);

    if (gnutls_pkcs12_verify_mac(p12, "") == 0) {
        cc = ne_calloc(sizeof *cc);
        cc->pkey = pkey;
        cc->decrypted = 1;
        cc->friendly_name = friendly_name;
        populate_cert(&cc->cert, cert);
        gnutls_pkcs12_deinit(p12);
        cc->p12 = NULL;
        return cc;
    } else {
        if (ret == 0) {
            cc = ne_calloc(sizeof *cc);
            cc->friendly_name = friendly_name;
            cc->p12 = p12;
            return cc;
        } else {
            gnutls_pkcs12_deinit(p12);
            return NULL;
        }
    }
}

int ne_ssl_clicert_encrypted(const ne_ssl_client_cert *cc)
{
    return !cc->decrypted;
}

int ne_ssl_clicert_decrypt(ne_ssl_client_cert *cc, const char *password)
{
    int ret;
    gnutls_x509_crt cert = NULL;
    gnutls_x509_privkey pkey = NULL;

    if (gnutls_pkcs12_verify_mac(cc->p12, password) != 0) {
        return -1;
    }        

    ret = pkcs12_parse(cc->p12, &pkey, &cert, NULL, password);
    if (ret < 0)
        return ret;

    gnutls_pkcs12_deinit(cc->p12);
    populate_cert(&cc->cert, cert);
    cc->pkey = pkey;
    cc->decrypted = 1;
    cc->p12 = NULL;
    return 0;
}

const ne_ssl_certificate *ne_ssl_clicert_owner(const ne_ssl_client_cert *cc)
{
    return &cc->cert;
}

const char *ne_ssl_clicert_name(ne_ssl_client_cert *ccert)
{
    return ccert->friendly_name;
}

ne_ssl_certificate *ne_ssl_cert_read(const char *filename)
{
    int ret;
    gnutls_datum data;
    gnutls_x509_crt x5;

    if (read_to_datum(filename, &data))
        return NULL;

    if (gnutls_x509_crt_init(&x5) != 0)
        return NULL;

    ret = gnutls_x509_crt_import(x5, &data, GNUTLS_X509_FMT_PEM);
    ne_free(data.data);
    if (ret < 0) {
        gnutls_x509_crt_deinit(x5);
        return NULL;
    }
    
    return populate_cert(ne_calloc(sizeof(struct ne_ssl_certificate_s)), x5);
}

int ne_ssl_cert_write(const ne_ssl_certificate *cert, const char *filename)
{
    unsigned char buffer[10*1024];
    int len = sizeof buffer;

    FILE *fp = fopen(filename, "w");

    if (fp == NULL) return -1;

    if (gnutls_x509_crt_export(cert->subject, GNUTLS_X509_FMT_PEM, buffer,
                               &len) < 0) {
        fclose(fp);
        return -1;
    }

    if (fwrite(buffer, len, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0)
        return -1;

    return 0;
}

void ne_ssl_cert_free(ne_ssl_certificate *cert)
{
    gnutls_x509_crt_deinit(cert->subject);
    if (cert->identity) ne_free(cert->identity);
    if (cert->issuer) ne_ssl_cert_free(cert->issuer);
    ne_free(cert);
}

int ne_ssl_cert_cmp(const ne_ssl_certificate *c1, const ne_ssl_certificate *c2)
{
    char digest1[NE_SSL_DIGESTLEN], digest2[NE_SSL_DIGESTLEN];

    if (ne_ssl_cert_digest(c1, digest1) || ne_ssl_cert_digest(c2, digest2)) {
        return -1;
    }

    return strcmp(digest1, digest2);
}

/* The certificate import/export format is the base64 encoding of the
 * raw DER; PEM without the newlines and wrapping. */

ne_ssl_certificate *ne_ssl_cert_import(const char *data)
{
    int ret;
    size_t len;
    unsigned char *der;
    gnutls_datum buffer = { NULL, 0 };
    gnutls_x509_crt x5;

    if (gnutls_x509_crt_init(&x5) != 0)
        return NULL;

    /* decode the base64 to get the raw DER representation */
    len = ne_unbase64(data, &der);
    if (len == 0) return NULL;

    buffer.data = der;
    buffer.size = len;

    ret = gnutls_x509_crt_import(x5, &buffer, GNUTLS_X509_FMT_DER);
    ne_free(der);

    if (ret < 0) {
        gnutls_x509_crt_deinit(x5);
        return NULL;
    }

    return populate_cert(ne_calloc(sizeof(struct ne_ssl_certificate_s)), x5);
}

char *ne_ssl_cert_export(const ne_ssl_certificate *cert)
{
    unsigned char *der;
    size_t len = 0;
    char *ret;

    /* find the length of the DER encoding. */
    if (gnutls_x509_crt_export(cert->subject, GNUTLS_X509_FMT_DER, NULL, &len) != 
        GNUTLS_E_SHORT_MEMORY_BUFFER) {
        return NULL;
    }
    
    der = ne_malloc(len);
    if (gnutls_x509_crt_export(cert->subject, GNUTLS_X509_FMT_DER, der, &len)) {
        ne_free(der);
        return NULL;
    }
    
    ret = ne_base64(der, len);
    ne_free(der);
    return ret;
}

int ne_ssl_cert_digest(const ne_ssl_certificate *cert, char *digest)
{
    int j, len = 20;
    char sha1[20], *p;

    if (gnutls_x509_crt_get_fingerprint(cert->subject, GNUTLS_DIG_SHA,
                                        sha1, &len) < 0)
        return -1;

    for (j = 0, p = digest; j < 20; j++) {
        *p++ = NE_HEX2ASC((sha1[j] >> 4) & 0x0f);
        *p++ = NE_HEX2ASC(sha1[j] & 0x0f);
        *p++ = ':';
    }

    *--p = '\0';
    return 0;
}
