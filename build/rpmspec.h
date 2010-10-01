#ifndef _H_SPEC_
#define _H_SPEC_

/** \ingroup rpmbuild
 * \file build/rpmspec.h
 *  The rpmSpec and Package data structures used during build.
 */

#include <rpm/rpmstring.h>	/* StringBuf */
#include <rpm/rpmcli.h>	/* for QVA_t */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmbuild
 */
typedef struct Package_s * rpmSpecPkg;
typedef struct Source * rpmSpecSrc;

enum rpmSourceFlags_e {
    RPMBUILD_ISSOURCE	= (1 << 0),
    RPMBUILD_ISPATCH	= (1 << 1),
    RPMBUILD_ISICON	= (1 << 2),
    RPMBUILD_ISNO	= (1 << 3),
};

typedef rpmFlags rpmSourceFlags;

#define RPMBUILD_DEFAULT_LANG "C"

enum rpmSpecFlags_e {
    RPMSPEC_NONE	= 0,
    RPMSPEC_ANYARCH	= (1 << 0),
    RPMSPEC_FORCE	= (1 << 1),
    RPMSPEC_NOLANG	= (1 << 2),
};

typedef rpmFlags rpmSpecFlags;

/** \ingroup rpmbuild
 * Destroy Spec structure.
 * @param spec		spec file control structure
 * @return		NULL always
 */
rpmSpec rpmSpecFree(rpmSpec spec);

/** \ingroup rpmbuild
 * Function to query spec file(s).
 * @param ts		transaction set
 * @param qva		parsed query/verify options
 * @param arg		query argument
 * @return		0 on success, else no. of failures
 */
int rpmspecQuery(rpmts ts, QVA_t qva, const char * arg);

#ifdef __cplusplus
}
#endif

#endif /* _H_SPEC_ */
