#ifndef H_RPMSIGNFILES
#define H_RPMSIGNFILES

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sign file digests in header and store the signatures in header
 * @param h		package header
 * @param key		signing key
 * @return		RPMRC_OK on success
 */
rpmRC rpmSignFiles(Header h, const char *key);

#ifdef _cplusplus
}
#endif

#endif /* H_RPMSIGNFILES */
