#ifndef H_MANIFEST
#define H_MANIFEST

/**
 * \file lib/manifest.h
 * Routines to expand a manifest containing glob expressions into an argv list.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return ls(1)-like formatted mode string.
 * @param mode		file mode
 * @return		(malloc'd) formatted mode string
 */
char * rpmPermsString(int mode)	
;

/**
 * Read manifest, glob items, and append to existing args.
 * @param fd		manifest file handle
 * @param[out] argcPtr	no. of args
 * @param[out] argvPtr	args themselves
 * @return		RPMRC_OK on success
 */
rpmRC rpmReadPackageManifest(FD_t fd, int * argcPtr, char *** argvPtr);

#ifdef __cplusplus
}
#endif

#endif	/* H_MANIFEST */
