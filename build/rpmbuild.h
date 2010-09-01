#ifndef	_H_RPMBUILD_
#define	_H_RPMBUILD_

/** \ingroup rpmbuild
 * \file build/rpmbuild.h
 *  This is the *only* module users of librpmbuild should need to include.
 */

#include <rpm/rpmcli.h>
#include <rpm/rpmds.h>
#include <rpm/rpmspec.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmbuild
 * Bit(s) to control buildSpec() operation.
 */
typedef enum rpmBuildFlags_e {
    RPMBUILD_NONE	= 0,
    RPMBUILD_PREP	= (1 <<  0),	/*!< Execute %%prep. */
    RPMBUILD_BUILD	= (1 <<  1),	/*!< Execute %%build. */
    RPMBUILD_INSTALL	= (1 <<  2),	/*!< Execute %%install. */
    RPMBUILD_CHECK	= (1 <<  3),	/*!< Execute %%check. */
    RPMBUILD_CLEAN	= (1 <<  4),	/*!< Execute %%clean. */
    RPMBUILD_FILECHECK	= (1 <<  5),	/*!< Check %%files manifest. */
    RPMBUILD_PACKAGESOURCE = (1 <<  6),	/*!< Create source package. */
    RPMBUILD_PACKAGEBINARY = (1 <<  7),	/*!< Create binary package(s). */
    RPMBUILD_RMSOURCE	= (1 <<  8),	/*!< Remove source(s) and patch(s). */
    RPMBUILD_RMBUILD	= (1 <<  9),	/*!< Remove build sub-tree. */
    RPMBUILD_STRINGBUF	= (1 << 10),	/*!< only for doScript() */
    RPMBUILD_RMSPEC	= (1 << 11)	/*!< Remove spec file. */
} rpmBuildFlags;

/** \ingroup rpmbuild
 * Describe build command line request.
 */
struct rpmBuildArguments_s {
    rpmQueryFlags qva_flags;	/*!< Bit(s) to control verification. */
    int buildAmount;		/*!< Bit(s) to control operation. */
    char * buildRootOverride; 	/*!< from --buildroot */
    char * targets;		/*!< Target platform(s), comma separated. */
    char * cookie;		/*!< NULL for binary, ??? for source, rpm's */
    int force;			/*!< from --force */
    int noBuild;		/*!< from --nobuild */
    int noDeps;			/*!< from --nodeps */
    int noLang;			/*!< from --nolang */
    int shortCircuit;		/*!< from --short-circuit */
    char buildMode;		/*!< Build mode (one of "btBC") */
    char buildChar;		/*!< Build stage (one of "abcilps ") */
    const char * rootdir;
};

/** \ingroup rpmbuild
 */
typedef	struct rpmBuildArguments_s *	BTA_t;

/** \ingroup rpmbuild
 */
extern struct rpmBuildArguments_s	rpmBTArgs;

/** \ingroup rpmbuild
 */
extern struct poptOption		rpmBuildPoptTable[];

/** \ingroup rpmbuild
 * Should version 3 packages be produced?
 */
extern int _noDirTokens;

/** \ingroup rpmbuild
 * Parse spec file into spec control structure.
 * @param ts		transaction set (spec file control in ts->spec)
 * @param specFile
 * @param rootDir
 * @param buildRoot
 * @param recursing	parse is recursive?
 * @param passPhrase	(unused)
 * @param cookie
 * @param anyarch
 * @param force
 * @return
 */
int parseSpec(rpmts ts, const char * specFile,
		const char * rootDir,
		const char * buildRoot,
		int recursing,
		const char * passPhrase,
		const char * cookie,
		int anyarch, int force);

/** \ingroup rpmbuild
 * Build stages state machine driver.
 * @param buildArgs	build arguments
 * @param spec		spec file control structure
 * @return		RPMRC_OK on success
 */
rpmRC rpmSpecBuild(BTA_t buildArgs, rpmSpec spec);

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMBUILD_ */
