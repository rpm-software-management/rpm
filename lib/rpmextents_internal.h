#ifndef _RPMEXTENTS_INTERNAL_H
#define _RPMEXTENTS_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** \ingroup rpmextents
 * RPM extents library
 */

/* magic value at end of file (64 bits) that indicates this is a transcoded
 * rpm.
 */
#define EXTENTS_MAGIC 3472329499408095051

typedef uint64_t extents_magic_t;

struct __attribute__ ((__packed__)) extents_footer_offsets_t {
    off64_t checksig_offset;
    off64_t table_offset;
    off64_t csum_offset;
};

struct __attribute__ ((__packed__)) extents_footer_t {
    struct extents_footer_offsets_t offsets;
    extents_magic_t magic;
};

/** \ingroup rpmextents
 * Checks the results of the signature verification ran during transcoding.
 * @param fd	The FD_t of the transcoded RPM
 * @param print_content Whether or not to print the result from rpmsig
 * @return	The number of checks that `rpmvsVerify` failed during transcoding.
 */
int extentsVerifySigs(FD_t fd, int print_content);

/** \ingroup rpmextents
 * Read the RPM Extents footer from a file descriptor.
 * @param fd		The FD_t of the transcoded RPM
 * @param[out] footer	A pointer to an allocated extents_footer_t with a copy of the footer.
 * @return		RPMRC_OK on success, RPMRC_NOTFOUND if not a transcoded file, RPMRC_FAIL on any failure.
 */
rpmRC extentsFooterFromFD(FD_t fd, struct extents_footer_t *footer);

/** \ingroup rpmextents
 * Check if a RPM is a transcoded RPM
 * @param fd	The FD_t of the transcoded RPM
 * return	RPMRC_OK on success, RPMRC_NOTFOUND if not a transcoded file, RPMRC_FAIL on any failure.
 */
rpmRC isTranscodedRpm(FD_t fd);

#ifdef __cplusplus
}
#endif
#endif /* _RPMEXTENTS_INTERNAL_H */
