#ifndef H_LEGACY
#define H_LEGACY

/**
 * \file lib/legacy.h
 *
 */

/**
 */
extern int _noDirTokens;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert absolute path tag to (dirname,basename,dirindex) tags.
 * @param h		header
 */
void compressFilelist(Header h);

/**
 * Convert (dirname,basename,dirindex) tags to absolute path tag.
 * @param h		header
 */
void expandFilelist(Header h);

/**
 * Do all necessary retrofits for a package header.
 * @param h		header
 */
void legacyRetrofit(Header h);

#ifdef __cplusplus
}
#endif

#endif	/* H_LEGACY */
