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
 * Return MD5 sum and size of a file.
 * @param fn		file name
 * @retval digest	address of md5sum
 * @param asAscii	return md5sum as ascii string?
 * @retval *fsizep	file size pointer (or NULL)
 * @return		0 on success, 1 on error
 */
int domd5(const char * fn, /*@out@*/ unsigned char * digest, int asAscii,
		/*@null@*/ /*@out@*/ size_t *fsizep)
	/*@globals fileSystem, internalState @*/
	/*@modifies digest, *fsizep, fileSystem, internalState @*/;

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
