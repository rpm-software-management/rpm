#ifndef H_SIGNATURE
#define	H_SIGNATURE

/** \file lib/signature.h
 * Generate and verify signatures.
 */

#include <header.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************/
/*                                                */
/* Signature types                                */
/*                                                */
/* These are what goes in the Lead                */
/*                                                */
/**************************************************/

#define RPMSIG_NONE         0  /* Do not change! */
#define RPMSIG_BAD          2  /* Returned for unknown types */
/* The following types are no longer generated */
#define RPMSIG_PGP262_1024  1  /* No longer generated */
#define RPMSIG_MD5          3
#define RPMSIG_MD5_PGP      4

/* These are the new-style signatures.  They are Header structures.    */
/* Inside them we can put any number of any type of signature we like. */

#define RPMSIG_HEADERSIG    5  /* New Header style signature */

/**************************************************/
/*                                                */
/* Prototypes                                     */
/*                                                */
/**************************************************/

Header rpmNewSignature(void);

/* If an old-style signature is found, we emulate a new style one */
int rpmReadSignature(FD_t fd, /*@out@*/ Header *header, short sig_type);
int rpmWriteSignature(FD_t fd, Header header);

/* Generate a signature of data in file, insert in header */
int rpmAddSignature(Header header, const char *file,
		    int_32 sigTag, const char *passPhrase);

/******************************************************************/

/* Possible actions for rpmLookupSignatureType() */
#define RPMLOOKUPSIG_QUERY	0	/* Lookup type in effect          */
#define RPMLOOKUPSIG_DISABLE	1	/* Disable (--sign was not given) */
#define RPMLOOKUPSIG_ENABLE	2	/* Re-enable %_signature          */

/* Return type of signature in effect for building */
int rpmLookupSignatureType(int action);

/* Utility to read a pass phrase from the user */
char *rpmGetPassPhrase(const char *prompt, const int sigTag);

/* >0 is a valid PGP version */
typedef enum pgpVersion_e {
	PGP_NOTDETECTED = -1, PGP_UNKNOWN = 0, PGP_2 = 2, PGP_5 = 5
} pgpVersion;

/* Return path to pgp executable of given type, or NULL when not found */
const char *rpmDetectPGPVersion( /*@out@*/ pgpVersion *pgpVersion);

#ifdef __cplusplus
}
#endif

#endif	/* H_SIGNATURE */
