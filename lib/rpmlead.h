#ifndef _H_RPMLEAD
#define _H_RPMLEAD

/** \ingroup lead
 * \file lib/rpmlead.h
 * Routines to read and write an rpm lead structure for a a package.
 */

#include <rpmlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup lead
 * Write lead to file handle.
 * @param fd		file handle
 * @param lead		data address
 * @return		0 on success, 1 on error
 */
int writeLead(FD_t fd, struct rpmlead *lead);

/** \ingroup lead
 * Read lead from file handle.
 * @param fd		file handle
 * @retval lead		data address
 * @return		0 on success, 1 on error
 */
int readLead(FD_t fd, /*@out@*/ struct rpmlead *lead)
	/*@modifies fd, *lead @*/;

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMLEAD */
