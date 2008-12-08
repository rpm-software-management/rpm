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
 * Initialize a lead structure
 * @return		Pointer to empty lead structure
 */
rpmlead rpmLeadNew(void);

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
 * @retval lead		package lead
 * @return		RPMRC_OK on success, RPMRC_FAIL/RPMRC_NOTFOUND on error
 */
rpmRC rpmLeadRead(FD_t fd, rpmlead lead);

/** \ingroup lead
 * Check lead for compatibility.
 * @param lead		Pointer to lead handle
 * @retval fn		Pointer to error message, NULL on success
 * @return		RPMRC_OK on success, 
 * 			RPMRC_NOTFOUND if not an rpm,
 * 			RPMRC_FAIL on invalid/incompatible rpm
 */
rpmRC rpmLeadCheck(rpmlead lead, const char **msg);

/** \ingroup lead
 * Returen type (source vs binary) of lead
 * @param lead		Pointer to lead handle
 * @return		RPMLEAD_BINARY or RPMLEAD_SOURCE, -1 on invalid
 */
int rpmLeadType(rpmlead lead);

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMLEAD */
