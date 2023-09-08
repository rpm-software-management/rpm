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
 * Transaction element file install function type.
 *
 * This function is called to install the files of transaction element
 *
 */
typedef int (*rmpteFileInstallFunction)(rpmte te, rpmfi fi, filedata fp, rpmfiles files, rpmpsm psm, int nodigest, filedata *firstlink, int *firstlinkfile, diriter di, int *fdp, int rc, rpmPlugin plugin);

/** \ingroup rpmte
 * Transaction element archive reader function type.
 *
 * This function is called to get an archive reader iterator
 *
 */
typedef rpmfi (*rpmteArchiveReaderFunction)(FD_t fd, rpmfiles files, int itype, rpmPlugin plugin);

/** \ingroup rpmte
 * Transaction element verification function type.
 *
 * This function is called to verify a package
 *
 */
typedef int (*rpmteVerifyFunction)(FD_t fd, rpmts ts, rpmte p, rpmvs vs, vfydata pvd, int *pverified);

/** \ingroup rpmte
 * Retrieve header from transaction element.
 * @param te		transaction element
 * @return		header (new reference)
 */
Header rpmteHeader(rpmte te);

/** \ingroup rpmte
 * Save header into transaction element.
 * @param te		transaction element
 * @param h		header
 * @return		NULL always
 */
Header rpmteSetHeader(rpmte te, Header h);

/** \ingroup rpmte
 * Retrieve type of transaction element.
 * @param te		transaction element
 * @return		type
 */
rpmElementType rpmteType(rpmte te);

/** \ingroup rpmte
 * Retrieve name string of transaction element.
 * @param te		transaction element
 * @return		name string
 */
const char * rpmteN(rpmte te);

/** \ingroup rpmte
 * Retrieve epoch string of transaction element.
 * @param te		transaction element
 * @return		epoch string
 */
const char * rpmteE(rpmte te);

/** \ingroup rpmte
 * Retrieve version string of transaction element.
 * @param te		transaction element
 * @return		version string
 */
const char * rpmteV(rpmte te);

/** \ingroup rpmte
 * Retrieve release string of transaction element.
 * @param te		transaction element
 * @return		release string
 */
const char * rpmteR(rpmte te);

/** \ingroup rpmte
 * Retrieve arch string of transaction element.
 * @param te		transaction element
 * @return		arch string
 */
const char * rpmteA(rpmte te);

/** \ingroup rpmte
 * Retrieve os string of transaction element.
 * @param te		transaction element
 * @return		os string
 */
const char * rpmteO(rpmte te);

/** \ingroup rpmte
 * Retrieve isSource attribute of transaction element.
 * @param te		transaction element
 * @return		isSource attribute
 */
int rpmteIsSource(rpmte te);

/** \ingroup rpmte
 * Retrieve color bits of transaction element.
 * @param te		transaction element
 * @return		color bits
 */
rpm_color_t rpmteColor(rpmte te);

/** \ingroup rpmte
 * Set color bits of transaction element.
 * @param te		transaction element
 * @param color		new color bits
 * @return		previous color bits
 */
rpm_color_t rpmteSetColor(rpmte te, rpm_color_t color);

/** \ingroup rpmte
 * Retrieve last instance installed to the database.
 * @param te		transaction element
 * @return		last install instance.
 */
unsigned int rpmteDBInstance(rpmte te);

/** \ingroup rpmte
 * Set last instance installed to the database.
 * @param te		transaction element
 * @param instance	Database instance of last install element.
 */
void rpmteSetDBInstance(rpmte te, unsigned int instance);

/** \ingroup rpmte
 * Retrieve size in bytes of package file.
 * @todo Signature header is estimated at 256b.
 * @param te		transaction element
 * @return		size in bytes of package file.
 */
rpm_loff_t rpmtePkgFileSize(rpmte te);

/** \ingroup rpmte
 * Retrieve parent transaction element.
 * @param te		transaction element
 * @return		parent transaction element
 */
rpmte rpmteParent(rpmte te);

/** \ingroup rpmte
 * Set parent transaction element.
 * @param te		transaction element
 * @param pte		new parent transaction element
 * @return		previous parent transaction element
 */
rpmte rpmteSetParent(rpmte te, rpmte pte);

/** \ingroup rpmte
 * Return problem set info of transaction element.
 * @param te		transaction element
 * @return		problem set (or NULL if none)
 */
rpmps rpmteProblems(rpmte te);

/** \ingroup rpmte
 * Destroy problem set info of transaction element.
 * @param te		transaction element
 */
void rpmteCleanProblems(rpmte te);

/** \ingroup rpmte
 * Destroy dependency set info of transaction element.
 * @param te		transaction element
 */
void rpmteCleanDS(rpmte te);

/** \ingroup rpmte
 * Set dependent element of transaction element.
 * @param te		transaction element
 * @param depends       dependent transaction element
 */
void rpmteSetDependsOn(rpmte te, rpmte depends);

/** \ingroup rpmte
 * Retrieve dependent element of transaction element.
 * @param te		transaction element
 * @return		dependent transaction element
 */
rpmte rpmteDependsOn(rpmte te);

/** \ingroup rpmte
 * Retrieve rpmdb instance of transaction element.
 * @param te		transaction element
 * @return		rpmdb instance (0 if not installed))
 */
int rpmteDBOffset(rpmte te);

/** \ingroup rpmte
 * Retrieve [epoch:]version-release string from transaction element.
 * @param te		transaction element
 * @return		[epoch:]version-release string
 */
const char * rpmteEVR(rpmte te);

/** \ingroup rpmte
 * Retrieve name-[epoch:]version-release string from transaction element.
 * @param te		transaction element
 * @return		name-[epoch:]version-release string
 */
const char * rpmteNEVR(rpmte te);

/** \ingroup rpmte
 * Retrieve name-[epoch:]version-release.arch string from transaction element.
 * @param te		transaction element
 * @return		name-[epoch:]version-release.arch string
 */
const char * rpmteNEVRA(rpmte te);

FD_t rpmteFd(rpmte te);

/** \ingroup rpmte
 * Retrieve key from transaction element.
 * @param te		transaction element
 * @return		key
 */
fnpyKey rpmteKey(rpmte te);

/** \ingroup rpmte
 * Set private user data of transaction element.
 * @param te		transaction element
 * @param data		pointer to private user data
 */
void rpmteSetUserdata(rpmte te, void *data);

/** \ingroup rpmte
 * Retrieve private user data of transaction element.
 * @param te		transaction element
 * @return		pointer to private user data
 */
void *rpmteUserdata(rpmte te);

/** \ingroup rpmte
 * Return failure status of transaction element.
 * If the element itself failed, this is 1, larger count means one of
 * it's parents failed.
 * @param te		transaction element
 * @return		number of failures for this transaction element
 */
int rpmteFailed(rpmte te);

/** \ingroup rpmte
 * Retrieve dependency tag set from transaction element.
 * @param te		transaction element
 * @param tag		dependency tag
 * @return		dependency tag set
 */
rpmds rpmteDS(rpmte te, rpmTagVal tag);

/** \ingroup rpmte
 * Retrieve file info set from transaction element.
 * @param te		transaction element
 * @return		file info set (refcounted)
 */
rpmfiles rpmteFiles(rpmte te);

/** \ingroup rpmte
 * Retrieve verification status from transaction element.
 * Returns RPMSIG_UNVERIFIED_TYPE if no verify has been attempted,
 * otherwise RPMSIG_SIGNATURE_TYPE and RPMSIG_DIGEST_TYPE bits will
 * be set if that type of verification was successfully performed.
 * @param te		transaction element
 * @return 		verification status
 */
int rpmteVerified(rpmte te);

/** \ingroup rpmte
 * Get file install function
 * @param   te		transaction element
 * @return		pointer to install function
 */
rmpteFileInstallFunction rpmteFileInstall(rpmte te);

/** \ingroup rpmte
 * Get archive reader function
 * @param   te		transaction element
 * @return		pointer to archive reader function
 */
rpmteArchiveReaderFunction rpmteArchiveReader(rpmte te);

/** \ingroup rpmte
 * Get verify package function
 * @param   te		transaction element
 * @return		pointer to verify function
 */
rpmteVerifyFunction rpmteVerify(rpmte te);

/** \ingroup rpmte
 * Get content handler plugin
 * @param   te		transaction element
 * @return		content handler plugin
 */
rpmPlugin rpmteContentHandlerPlugin(rpmte te);

/** \ingroup rpmte
 * Set content handler functions
 * @param   te		transaction element
 * @param   handler	structure containing handling functions
 * @param   plugin	plugin providing content handling functions
 */
void rpmteSetContentHandler(rpmte te, rpmPluginContentHandler handler, rpmPlugin plugin);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMTE */
