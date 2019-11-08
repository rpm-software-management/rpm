#ifndef _H_RPMLEAD
#define _H_RPMLEAD

/** \ingroup lead
 * \file rpmlead.h
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

/** \ingroup lead
 * The lead data structure.
 * The lead needs to be 8 byte aligned.
 * @deprecated The lead (except for signature_type) is legacy.
 * @todo Don't use any information from lead.
 */
struct rpmlead_s {
    unsigned char magic[4];
    unsigned char major;
    unsigned char minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type;       /*!< Signature header type (RPMSIG_HEADERSIG) */
    char reserved[16];      /*!< Pad to 96 bytes -- 8 byte aligned! */
};

/** \ingroup lead
 * Write lead to file handle.
 * @param fd		file handle
 * @param h		package header
 * @return		RPMRC_OK on success, RPMRC_FAIL on error
 */
rpmRC rpmLeadWriteFromHeader(FD_t fd, Header h);

/** \ingroup lead
 * Write lead to file handle.
 * @param fd		file handle
 * @param l		lead
 * @return		RPMRC_OK on success, RPMRC_FAIL on error
 */
rpmRC rpmLeadWrite(FD_t fd, struct rpmlead_s l);

/** \ingroup lead
 * Read lead from file handle.
 * @param fd		file handle
 * @param[out] emsg		failure message on error (malloced)
 * @return		RPMRC_OK on success, RPMRC_FAIL/RPMRC_NOTFOUND on error
 */
rpmRC rpmLeadRead(FD_t fd, char **emsg);

/** \ingroup lead
 * Read lead from file handle and return it.
 * @param fd		file handle
 * @param[out] emsg		failure message on error (malloced)
 * @param[out] ret		address of lead
 * @return		RPMRC_OK on success, RPMRC_FAIL/RPMRC_NOTFOUND on error
 */
rpmRC rpmLeadReadAndReturn(FD_t fd, char **emsg, struct rpmlead_s * ret);

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMLEAD */
