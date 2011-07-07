#ifndef _H_RPMLEAD
#define _H_RPMLEAD

/** \ingroup lead
 * \file lib/rpmlead.h
 * Routines to read and write an rpm lead structure for a a package.
 */
#ifdef __cplusplus
extern "C" {
#endif

#define RPMLEAD_BINARY 0
#define RPMLEAD_SOURCE 1

#define RPMLEAD_MAGIC0 0xed
#define RPMLEAD_MAGIC1 0xab
#define RPMLEAD_MAGIC2 0xee
#define RPMLEAD_MAGIC3 0xdb

#define RPMLEAD_SIZE 96         /*!< Don't rely on sizeof(struct) */

typedef struct rpmlead_s * rpmlead;

/** \ingroup lead
 * Initialize a lead structure from header
 * param h		Header
 * @return		Pointer to populated lead structure (malloced)
 */
rpmlead rpmLeadFromHeader(Header h);

/** \ingroup lead
 * Free a lead structure
 * @param lead		Pointer to lead structure
 * @return		NULL always
 */
rpmlead rpmLeadFree(rpmlead lead);

/** \ingroup lead
 * Write lead to file handle.
 * @param fd		file handle
 * @param lead		package lead
 * @return		RPMRC_OK on success, RPMRC_FAIL on error
 */
rpmRC rpmLeadWrite(FD_t fd, rpmlead lead);

/** \ingroup lead
 * Read lead from file handle.
 * @param fd		file handle
 * @retval lead		pointer to package lead (malloced)
 * @retval type		RPMLEAD_BINARY or RPMLEAD_SOURCE on success
 * @retval emsg		failure message on error (malloced)
 * @return		RPMRC_OK on success, RPMRC_FAIL/RPMRC_NOTFOUND on error
 */
rpmRC rpmLeadRead(FD_t fd, rpmlead *lead, int *type, char **emsg);

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMLEAD */
