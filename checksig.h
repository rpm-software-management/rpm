#ifndef H_CHECKSIG
#define H_CHECKSIG

int doCheckSig(int pgp, char **argv);

int doReSign(int add, char *passPhrase, char **argv);

#define ADD_SIGNATURE 1
#define NEW_SIGNATURE 0

#endif
