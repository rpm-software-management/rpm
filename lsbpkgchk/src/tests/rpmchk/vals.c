#include "rpmchk.h"
/*
 * This file contains some values which must match, and some places to
 * stick things which are discovered in one place, but used in another.
 */
char *architecture =
#if defined(__i386__)
	"i486";
#elif defined(__ia64__)
	"ia64";
#elif defined(__powerpc__)
	"powerpc";
#else
	"unknown architecture";
#endif

char *validos = "linux";
char *validdepver = "1.2";

char *pkgname;
int  lsbdepidx=-1;

/* Stuff that we read in one part, but need to use for validation in
 * another part. 
 */
unsigned char *sigdata;
uint32_t  sigsize;
uint32_t  archivesize;
uint32_t  rpmtagsize;
uint32_t *filesizes;
uint16_t *filemodes;
uint32_t *filedevs;
uint16_t *filerdevs;
uint32_t *fileinodes;
uint32_t *filetimes;
char	*filemd5s;
char	*filelinktos;
char	*fileusernames;
char	*filegroupnames;
char	*filelangs;
uint32_t	*dirindicies;
char	**basenames;
char	**dirnames;
int	numdirnames;

int	hasPayloadFilesHavePrefix;
int	rpmchkdebug;
