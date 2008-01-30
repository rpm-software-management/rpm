#ifndef _RPMTYPES_H
#define _RPMTYPES_H

/** \ingroup rpmtypes
 * \file lib/rpmtypes.h
 */

#include <rpm/rpmints.h>

typedef const char *    errmsg_t;

typedef int32_t		rpm_tag_t;
typedef uint32_t	rpm_tagtype_t;
typedef uint32_t	rpm_count_t;

typedef void *		rpm_data_t;
typedef const void *	rpm_constdata_t;

#endif /* _RPMTYPES_H */
