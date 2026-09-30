#ifndef H_RPMALIGN
#define H_RPMALIGN

#include <sys/types.h>		/* off_t */
#include <stdint.h>
#include <limits>

/** \ingroup payload
 * \file Shared helpers for block-aligned payloads.
 *
 * A content alignment is stored in RPMTAG_PAYLOADALIGNMENT and must be a
 * positive power of two. These helpers centralize validity and round-up
 * arithmetic for the physical raw representation.
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

/** Round @a pos up to the next multiple of @a align, or -1 on overflow. */
static inline off_t rpmAlignUp(off_t pos, uint64_t align)
{
    const uint64_t maxoff = (uint64_t)std::numeric_limits<off_t>::max();

    if (pos < 0 || !rpmAlignIsValid(align) ||
	(uint64_t)pos > maxoff - (align - 1))
	return -1;
    return (off_t)(((uint64_t)pos + align - 1) & ~(align - 1));
}

#endif	/* H_RPMALIGN */
