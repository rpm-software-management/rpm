#ifndef H_RPMALIGN
#define H_RPMALIGN

#include <stdint.h>

/** \ingroup payload
 * \file Shared helpers for block-aligned payloads.
 *
 * A content alignment is stored in RPMTAG_PAYLOADALIGNMENT and must be a
 * positive power of two. This helper centralizes the validity check.
 */

/* Bound the worst-case padding overhead from an accidentally excessive
 * alignment while leaving ample room for practical filesystem block sizes. */
#define RPMALIGN_MAX (UINT64_C(1) << 20)

/** True when @a align is a usable content alignment (a bounded power of two). */
static inline int rpmAlignIsValid(uint64_t align)
{
    return align > 0 && align <= RPMALIGN_MAX &&
	   (align & (align - 1)) == 0;
}

#endif	/* H_RPMALIGN */
