#ifndef H_CHECKSIG
#define H_CHECKSIG

#define CHECKSIG_PGP (1 << 0)
#define CHECKSIG_MD5 (1 << 1)
#define CHECKSIG_GPG (1 << 2)

int doCheckSig(int flags, const char **argv);
int doReSign(int add, char *passPhrase, const char **argv);

#define ADD_SIGNATURE 1
#define NEW_SIGNATURE 0

#endif
