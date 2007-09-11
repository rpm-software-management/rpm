#ifndef H_LEGACY
#define H_LEGACY

/**
 * \file rpmdb/legacy.h
 *
 */

/**
 */
extern int _noDirTokens;

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
int domd5(const char * fn, unsigned char * digest, int asAscii,
		size_t *fsizep);

/**
 * Convert absolute path tag to (dirname,basename,dirindex) tags.
 * @param h		header
 */
void compressFilelist(Header h);

/**
 * Retrieve file names from header.
 *
 * The representation of file names in package headers changed in rpm-4.0.
 * Originally, file names were stored as an array of absolute paths.
 * In rpm-4.0, file names are stored as separate arrays of dirname's and
 * basename's, * with a dirname index to associate the correct dirname
 * with each basname.
 *
 * This function is used to retrieve file names independent of how the
 * file names are represented in the package header.
 * 
 * @param h		header
 * @param tagN		RPMTAG_BASENAMES | PMTAG_ORIGBASENAMES
 * @retval *fnp		array of file names
 * @retval *fcp		number of files
 */
void rpmfiBuildFNames(Header h, rpmTag tagN,
		const char *** fnp, int * fcp);

/**
 * Convert (dirname,basename,dirindex) tags to absolute path tag.
 * @param h		header
 */
void expandFilelist(Header h);

/**
 * Retrofit a Provides: name = version-release dependency into legacy
 * package headers.
 * @param h		header
 */
void providePackageNVR(Header h);

/**
 * Do all necessary retorfits for a package header.
 * @param h		header
 * @param lead
 */
void legacyRetrofit(Header h, const struct rpmlead * lead);

#ifdef __cplusplus
}
#endif

#endif	/* H_LEGACY */
