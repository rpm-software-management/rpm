#ifndef H_CHECKSIG
#define H_CHECKSIG

int doCheckSig(int pgp, char **argv);

int doReSign(char *passPhrase, char **argv);

#endif
