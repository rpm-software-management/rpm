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
/*@-incondefs@*/
/*@only@*/
char * rpmPermsString(int mode)	
	/*@*/
	/*@ensures maxSet(result) == 10 /\ maxRead(result) == 10 @*/;
/*@=incondefs@*/

/**
 * Read manifest, glob items, and append to existing args.
 * @param fd		manifest file handle
 * @retval argcPtr	no. of args
 * @retval argvPtr	args themselves
 * @return		RPMRC_OK on success
 */
rpmRC rpmReadPackageManifest(FD_t fd, int * argcPtr, const char *** argvPtr)
	/*@globals fileSystem, internalState @*/
	/*@modifies fd, *argcPtr, *argvPtr, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_MANIFEST */
