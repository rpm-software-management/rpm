/* signature.h - generate and verify signatures */

/* Signature types */
#define RPMSIG_NONE         0  /* Do not change! */
#define RPMSIG_PGP262_1024  1
#define RPMSIG_BAD          2  /* Returned for unknown types */

/* Read a sig_type signature from fd, alloc and return sig. */
int readSignature(int fd, short sig_type, void **sig);

/* Generate a signature of data in file, write it to ofd */
int makeSignature(char *file, short sig_type, int ofd);

/* Verify data on fd with sig.                      */
/* If result is not NULL, fill it with status info. */
int verifySignature(int fd, short sig_type, void *sig, char *result);

/* Return type of signature in effect for building */
unsigned short sigLookupType(void);
