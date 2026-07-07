#ifndef H_RPMALIGN
#define H_RPMALIGN

#include <sys/types.h>		/* off_t */
#include <stdint.h>

/** \ingroup payload
 * \file Shared helpers for block-aligned payloads.
 *
 * A content alignment is stored in RPMTAG_PAYLOADALIGNMENT and must be a
 * positive power of two. These helpers centralize the validity check and the
 * round-up arithmetic so the masking is defined in exactly one place.
 */

/** True when @a align is a usable content alignment (a positive power of two). */
static inline int rpmAlignIsValid(uint64_t align)
{
    return align > 0 && (align & (align - 1)) == 0;
}

/** Round @a pos up to the next multiple of @a align (a power of two). */
static inline off_t rpmAlignUp(off_t pos, uint64_t align)
{
    return (off_t)((pos + (off_t)align - 1) & ~((off_t)align - 1));
}

#endif	/* H_RPMALIGN */
