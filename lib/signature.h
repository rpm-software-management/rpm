/* signature.h - generate and verify signatures */

#include "header.h"

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
/* Signature Tags                                 */
/*                                                */
/* These go in the sig Header to specify          */
/* individual signature types.                    */
/*                                                */
/**************************************************/

#define SIGTAG_SIZE         1000
#define SIGTAG_MD5          1001
#define SIGTAG_PGP          1002

/**************************************************/
/*                                                */
/* verifySignature() results                      */
/*                                                */
/**************************************************/

/* verifySignature() results */
#define RPMSIG_OK        0
#define RPMSIG_UNKNOWN   1
#define RPMSIG_BAD       2
#define RPMSIG_NOKEY     3  /* Do not have the key to check this signature */

/**************************************************/
/*                                                */
/* Prototypes                                     */
/*                                                */
/**************************************************/

Header newSignature(void);

/* If an old-style signature is found, we emulate a new style one */
int readSignature(int fd, Header *header, short sig_type);
int writeSignature(int fd, Header header);

/* Generate a signature of data in file, insert in header */
int addSignature(Header header, char *file, int_32 sigTag, char *passPhrase);

void freeSignature(Header h);

/******************************************************************/

int verifySignature(char *file, int_32 sigTag, void *sig, int count,
		    char *result);

/* Return type of signature in effect for building */
int sigLookupType(void);

/* Utility to read a pass phrase from the user */
char *getPassPhrase(char *prompt);
