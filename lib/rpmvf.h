#ifndef _RPMVF_H
#define _RPMVF_H

/** \ingroup rpmvf
 * \file lib/rpmvf.h
 *
 * \brief Verify a package. The constants that enable/disable some sanity checks (mainly used at post (un)install)
 */
#include <rpm/rpmtypes.h>
#include <rpm/rpmutil.h>
#include <rpm/rpmfiles.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmvf
 * Bit(s) to control rpmVerify() operation
 */
enum rpmVerifyFlags_e {
    VERIFY_DEFAULT	= 0,		/*!< */
	/* bits 1-14 unused */
    VERIFY_CONTEXTS	= (1 << 15),	/*!< verify: from --nocontexts */
    VERIFY_FILES	= (1 << 16),	/*!< verify: from --nofiles */
    VERIFY_DEPS		= (1 << 17),	/*!< verify: from --nodeps */
    VERIFY_SCRIPT	= (1 << 18),	/*!< verify: from --noscripts */
    VERIFY_DIGEST	= (1 << 19),	/*!< verify: from --nodigest */
    VERIFY_SIGNATURE	= (1 << 20),	/*!< verify: from --nosignature */
    VERIFY_PATCHES	= (1 << 21),	/*!< verify: from --nopatches */
    VERIFY_HDRCHK	= (1 << 22),	/*!< verify: from --nohdrchk */
	/* bits 28-31 used in rpmVerifyAttrs */
};

typedef rpmFlags rpmVerifyFlags;

#define	VERIFY_ALL	\
  ( VERIFY_FILES | VERIFY_DEPS | VERIFY_SCRIPT | VERIFY_DIGEST |\
    VERIFY_SIGNATURE | VERIFY_HDRCHK )

#ifdef __cplusplus
}
#endif

#endif /* _RPMTYPES_H */
