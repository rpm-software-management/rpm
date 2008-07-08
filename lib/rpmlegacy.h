#ifndef _RPMLEGACY_H
#define _RPMLEGACY_H

#include <rpm/rpmtypes.h>

/* ==================================================================== */
/*         LEGACY INTERFACES AND TYPES, DO NOT USE IN NEW CODE!         */
/* ==================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _RPM_4_4_COMPAT

/* mappings for legacy types */
typedef int32_t		int_32;
typedef int16_t		int_16;
typedef int8_t		int_8;
typedef uint32_t	uint_32;
typedef uint16_t	uint_16;
typedef uint8_t		uint_8;
#endif /* _RPM_4_4_COMPAT */

#ifdef __cplusplus
}
#endif

#endif /* _RPMLEGACY_H */
