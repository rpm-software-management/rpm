/**
 * To be included after all other includes.
 */
#ifndef	H_DEBUG
#define	H_DEBUG

#include <assert.h>

#ifdef  __LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
#endif

#ifdef	DMALLOC
#include <dmalloc.h>
#endif

#endif	/* H_DEBUG */
