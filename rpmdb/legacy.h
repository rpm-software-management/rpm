#ifndef H_LEGACY
#define H_LEGACY

/**
 * \file rpmdb/legacy.h
 *
 */

/**
 */
/*@-redecl@*/
/*@unchecked@*/
extern int _noDirTokens;
/*@=redecl@*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculate MD5 sum for file.
 * @todo Eliminate, use beecrypt instead.
 * @param fn		file name
 * @retval digest	address of md5sum
 * @param asAscii	return md5sum as ascii string?
 * @return		0 on success, 1 on error
 */
/*@-exportlocal@*/
int domd5(const char * fn, /*@out@*/ unsigned char * digest, int asAscii)
	/*@globals fileSystem@*/
	/*@modifies digest, fileSystem @*/;
/*@=exportlocal@*/

/**
 * Return MD5 sum of file as ASCII string.
 * @todo Eliminate, use beecrypt instead.
 * @param fn		file name
 * @retval digest	MD5 digest
 * @return		0 on success, 1 on error
 */
/*@unused@*/ static inline
int mdfile(const char * fn, /*@out@*/ unsigned char * digest)
	/*@globals fileSystem@*/
	/*@modifies digest, fileSystem @*/
{
    return domd5(fn, digest, 1);
}

/**
 * Return MD5 sum of file as binary data.
 * @todo Eliminate, use beecrypt instead.
 * @param fn		file name
 * @retval bindigest	MD5 digest
 * @return		0 on success, 1 on error
 */
/*@unused@*/ static inline
int mdbinfile(const char * fn, /*@out@*/ unsigned char * bindigest)
	/*@globals fileSystem@*/
	/*@modifies bindigest, fileSystem @*/
{
    return domd5(fn, bindigest, 0);
}

/**
 * Convert absolute path tag to (dirname,basename,dirindex) tags.
 * @param h		header
 */
void compressFilelist(Header h)
	/*@modifies h @*/;

/**
 * Convert (dirname,basename,dirindex) tags to absolute path tag.
 * @param h		header
 */
void expandFilelist(Header h)
	/*@modifies h @*/;

/**
 * @param h		header
 * @retval fileListPtr	list of files
 * @retval fileCountPtr	number of files
 */
void buildOrigFileList(Header h, /*@out@*/ const char *** fileListPtr, 
			/*@out@*/ int * fileCountPtr)
	/*@modifies *fileListPtr, *fileCountPtr @*/;

/**
 * Retrofit a Provides: name = version-release dependency into legacy
 * packages.
 * @param h		header
 */
void providePackageNVR(Header h)
	/*@modifies h @*/;

/**
 * Do all necessary retorfits for a package header.
 * @param h		header
 * @param lead
 */
void legacyRetrofit(Header h, const struct rpmlead * lead)
	/*@modifies h@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_LEGACY */
