#ifndef _H_RPMLEAD
#define _H_RPMLEAD

/** \ingroup lead
 * \file lib/rpmlead.h
 * Routines to read and write an rpm lead structure for a a package.
 */
#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup lead
 * Write lead to file handle.
 * @param fd		file handle
 * @param lead		package lead
 * @return		RPMRC_OK on success, RPMRC_FAIL on error
 */
rpmRC writeLead(FD_t fd, const struct rpmlead *lead)
	/*@globals fileSystem @*/
	/*@modifies fd, fileSystem @*/;

/** \ingroup lead
 * Read lead from file handle.
 * @param fd		file handle
 * @retval lead		package lead
 * @return		RPMRC_OK on success, RPMRC_FAIL/RPMRC_NOTFOUND on error
 */
rpmRC readLead(FD_t fd, /*@out@*/ struct rpmlead *lead)
	/*@modifies fd, *lead @*/;

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMLEAD */
