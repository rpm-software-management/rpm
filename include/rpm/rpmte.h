#ifndef H_RPMTE
#define H_RPMTE

/** \ingroup rpmts rpmte
 * \file rpmte.h
 * Structures used for an "rpmte" transaction element.
 */

#include <rpm/rpmtypes.h>
#include <rpm/argv.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmte
 * Transaction element type.
 */
typedef enum rpmElementType_e {
    TR_ADDED		= (1 << 0),	/*!< Package will be installed. */
    TR_REMOVED		= (1 << 1),	/*!< Package will be removed. */
    TR_RPMDB		= (1 << 2),	/*!< Package from the rpmdb. */
    TR_RESTORED		= (1 << 3),	/*!< Package will be restored. */
} rpmElementType;

typedef rpmFlags rpmElementTypes;

/** \ingroup rpmte
 * Retrieve header from transaction element.
 * @param te		transaction element
 * @return		header (new reference)
 */
RPM_PUBLIC_API
Header rpmteHeader(rpmte te);

/** \ingroup rpmte
 * Save header into transaction element.
 * @param te		transaction element
 * @param h		header
 * @return		NULL always
 */
RPM_PUBLIC_API
Header rpmteSetHeader(rpmte te, Header h);

/** \ingroup rpmte
 * Retrieve type of transaction element.
 * @param te		transaction element
 * @return		type
 */
RPM_PUBLIC_API
rpmElementType rpmteType(rpmte te);

/** \ingroup rpmte
 * Retrieve name string of transaction element.
 * @param te		transaction element
 * @return		name string
 */
RPM_PUBLIC_API
const char * rpmteN(rpmte te);

/** \ingroup rpmte
 * Retrieve epoch string of transaction element.
 * @param te		transaction element
 * @return		epoch string
 */
RPM_PUBLIC_API
const char * rpmteE(rpmte te);

/** \ingroup rpmte
 * Retrieve version string of transaction element.
 * @param te		transaction element
 * @return		version string
 */
RPM_PUBLIC_API
const char * rpmteV(rpmte te);

/** \ingroup rpmte
 * Retrieve release string of transaction element.
 * @param te		transaction element
 * @return		release string
 */
RPM_PUBLIC_API
const char * rpmteR(rpmte te);

/** \ingroup rpmte
 * Retrieve arch string of transaction element.
 * @param te		transaction element
 * @return		arch string
 */
RPM_PUBLIC_API
const char * rpmteA(rpmte te);

/** \ingroup rpmte
 * Retrieve os string of transaction element.
 * @param te		transaction element
 * @return		os string
 */
RPM_PUBLIC_API
const char * rpmteO(rpmte te);

/** \ingroup rpmte
 * Retrieve isSource attribute of transaction element.
 * @param te		transaction element
 * @return		isSource attribute
 */
RPM_PUBLIC_API
int rpmteIsSource(rpmte te);

/** \ingroup rpmte
 * Retrieve color bits of transaction element.
 * @param te		transaction element
 * @return		color bits
 */
RPM_PUBLIC_API
rpm_color_t rpmteColor(rpmte te);

/** \ingroup rpmte
 * Set color bits of transaction element.
 * @param te		transaction element
 * @param color		new color bits
 * @return		previous color bits
 */
RPM_PUBLIC_API
rpm_color_t rpmteSetColor(rpmte te, rpm_color_t color);

/** \ingroup rpmte
 * Retrieve last instance installed to the database.
 * @param te		transaction element
 * @return		last install instance.
 */
RPM_PUBLIC_API
unsigned int rpmteDBInstance(rpmte te);

/** \ingroup rpmte
 * Set last instance installed to the database.
 * @param te		transaction element
 * @param instance	Database instance of last install element.
 */
RPM_PUBLIC_API
void rpmteSetDBInstance(rpmte te, unsigned int instance);

/** \ingroup rpmte
 * Retrieve size in bytes of package file.
 * @todo Signature header is estimated at 256b.
 * @param te		transaction element
 * @return		size in bytes of package file.
 */
RPM_PUBLIC_API
rpm_loff_t rpmtePkgFileSize(rpmte te);

/** \ingroup rpmte
 * Retrieve parent transaction element.
 * @param te		transaction element
 * @return		parent transaction element
 */
RPM_PUBLIC_API
rpmte rpmteParent(rpmte te);

/** \ingroup rpmte
 * Set parent transaction element.
 * @param te		transaction element
 * @param pte		new parent transaction element
 * @return		previous parent transaction element
 */
RPM_PUBLIC_API
rpmte rpmteSetParent(rpmte te, rpmte pte);

/** \ingroup rpmte
 * Return problem set info of transaction element.
 * @param te		transaction element
 * @return		problem set (or NULL if none)
 */
RPM_PUBLIC_API
rpmps rpmteProblems(rpmte te);

/** \ingroup rpmte
 * Destroy problem set info of transaction element.
 * @param te		transaction element
 */
RPM_PUBLIC_API
void rpmteCleanProblems(rpmte te);

/** \ingroup rpmte
 * Destroy dependency set info of transaction element.
 * @param te		transaction element
 */
RPM_PUBLIC_API
void rpmteCleanDS(rpmte te);

/** \ingroup rpmte
 * Set dependent element of transaction element.
 * @param te		transaction element
 * @param depends       dependent transaction element
 */
RPM_PUBLIC_API
void rpmteSetDependsOn(rpmte te, rpmte depends);

/** \ingroup rpmte
 * Retrieve dependent element of transaction element.
 * @param te		transaction element
 * @return		dependent transaction element
 */
RPM_PUBLIC_API
rpmte rpmteDependsOn(rpmte te);

/** \ingroup rpmte
 * Retrieve rpmdb instance of transaction element.
 * @param te		transaction element
 * @return		rpmdb instance (0 if not installed))
 */
RPM_PUBLIC_API
int rpmteDBOffset(rpmte te);

/** \ingroup rpmte
 * Retrieve [epoch:]version-release string from transaction element.
 * @param te		transaction element
 * @return		[epoch:]version-release string
 */
RPM_PUBLIC_API
const char * rpmteEVR(rpmte te);

/** \ingroup rpmte
 * Retrieve name-[epoch:]version-release string from transaction element.
 * @param te		transaction element
 * @return		name-[epoch:]version-release string
 */
RPM_PUBLIC_API
const char * rpmteNEVR(rpmte te);

/** \ingroup rpmte
 * Retrieve name-[epoch:]version-release.arch string from transaction element.
 * @param te		transaction element
 * @return		name-[epoch:]version-release.arch string
 */
RPM_PUBLIC_API
const char * rpmteNEVRA(rpmte te);

/** \ingroup rpmte
 * Retrieve key from transaction element.
 * @param te		transaction element
 * @return		key
 */
RPM_PUBLIC_API
fnpyKey rpmteKey(rpmte te);

/** \ingroup rpmte
 * Set private user data of transaction element.
 * @param te		transaction element
 * @param data		pointer to private user data
 */
RPM_PUBLIC_API
void rpmteSetUserdata(rpmte te, void *data);

/** \ingroup rpmte
 * Retrieve private user data of transaction element.
 * @param te		transaction element
 * @return		pointer to private user data
 */
RPM_PUBLIC_API
void *rpmteUserdata(rpmte te);

/** \ingroup rpmte
 * Return failure status of transaction element.
 * If the element itself failed, this is 1, larger count means one of
 * it's parents failed.
 * @param te		transaction element
 * @return		number of failures for this transaction element
 */
RPM_PUBLIC_API
int rpmteFailed(rpmte te);

/** \ingroup rpmte
 * Retrieve dependency tag set from transaction element.
 * @param te		transaction element
 * @param tag		dependency tag
 * @return		dependency tag set
 */
RPM_PUBLIC_API
rpmds rpmteDS(rpmte te, rpmTagVal tag);

/** \ingroup rpmte
 * Retrieve file info set from transaction element.
 * @param te		transaction element
 * @return		file info set (refcounted)
 */
RPM_PUBLIC_API
rpmfiles rpmteFiles(rpmte te);

/** \ingroup rpmte
 * Retrieve verification status from transaction element.
 * Returns RPMSIG_UNVERIFIED_TYPE if no verify has been attempted,
 * otherwise RPMSIG_SIGNATURE_TYPE and RPMSIG_DIGEST_TYPE bits will
 * be set if that type of verification was successfully performed.
 * @param te		transaction element
 * @return 		verification status
 */
RPM_PUBLIC_API
int rpmteVerified(rpmte te);

/** \ingroup rpmte
 * Get enforced per-package verify level. If per-package verify level
 * isn't set for this element, rpmtsVfyLevel() value is returned.
 * @param te            transaction element
 * @return              package verify level
 */
RPM_PUBLIC_API
int rpmteVfyLevel(rpmte te);

/** \ingroup rpmte
 * Set enforced per-package verify level.
 * vfylevel -1 means using transaction verify level.
 * @param te            transaction set
 * @param vfylevel      new per-package verify level
 * @return              old per-package verify level
 */
RPM_PUBLIC_API
int rpmteSetVfyLevel(rpmte te, int vfylevel);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMTE */
