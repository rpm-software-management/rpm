#ifndef _H_RPMFC_
#define _H_RPMFC_

/** \ingroup rpmfc rpmbuild
 * \file build/rpmfc.h
 * Structures and methods for build-time file classification.
 */

#include <magic.h>

#include <rpm/rpmtypes.h>
#include <rpm/argv.h>	/* for ARGV_t */
#include <rpm/rpmstring.h>	/* for StringBuf */
#include <rpm/rpmspec.h>	/* for Package */

#ifdef __cplusplus
extern "C" {
#endif

extern int _rpmfc_debug;

/** \ingroup rpmfc
 */
typedef struct rpmfc_s * rpmfc;

/** \ingroup rpmfc
 */
enum FCOLOR_e {
    RPMFC_BLACK			= 0,
    RPMFC_ELF32			= (1 <<  0),
    RPMFC_ELF64			= (1 <<  1),
#define	RPMFC_ELF	(RPMFC_ELF32|RPMFC_ELF64)

    RPMFC_PKGCONFIG		= (1 <<  5),
    RPMFC_LIBTOOL		= (1 <<  6),
    RPMFC_MODULE		= (1 <<  7),
    RPMFC_EXECUTABLE		= (1 <<  8),
    RPMFC_SCRIPT		= (1 <<  9),
    RPMFC_TEXT			= (1 << 10),
    RPMFC_DATA			= (1 << 11),	/* XXX unused */
    RPMFC_DOCUMENT		= (1 << 12),
    RPMFC_STATIC		= (1 << 13),
    RPMFC_NOTSTRIPPED		= (1 << 14),
    RPMFC_COMPRESSED		= (1 << 15),

    RPMFC_DIRECTORY		= (1 << 16),
    RPMFC_SYMLINK		= (1 << 17),
    RPMFC_DEVICE		= (1 << 18),
    RPMFC_LIBRARY		= (1 << 19),
    RPMFC_ARCHIVE		= (1 << 20),
    RPMFC_FONT			= (1 << 21),
    RPMFC_IMAGE			= (1 << 22),
    RPMFC_MANPAGE		= (1 << 23),

    RPMFC_PERL			= (1 << 24),
    RPMFC_JAVA			= (1 << 25),
    RPMFC_PYTHON		= (1 << 26),
    RPMFC_PHP			= (1 << 27),
    RPMFC_TCL			= (1 << 28),
    RPMFC_MONO                  = (1 << 6),

    RPMFC_WHITE			= (1 << 29),
    RPMFC_INCLUDE		= (1 << 30),
    RPMFC_ERROR			= (1 << 31)
};
/** \ingroup rpmfc
 */
typedef	enum FCOLOR_e FCOLOR_t;

/** \ingroup rpmfc
 */
typedef struct rpmfcTokens_s * rpmfcToken;

/** \ingroup rpmfc
 * Return helper output.
 * @param av		helper argv (with possible macros)
 * @param sb_stdin	helper input
 * @retval *sb_stdoutp	helper output
 * @param failnonzero	IS non-zero helper exit status a failure?
 */
int rpmfcExec(ARGV_t av, StringBuf sb_stdin, StringBuf * sb_stdoutp,
		int failnonzero);

/** \ingroup rpmfc
 * Return file color given file(1) string.
 * @param fmstr		file(1) string
 * @return		file color
 */
int rpmfcColoring(const char * fmstr);

/** \ingroup rpmfc
 * Print results of file classification.
 * @todo Remove debugging routine.
 * @param msg		message prefix (NULL for none)
 * @param fc		file classifier
 * @param fp		output file handle (NULL for stderr)
 */
void rpmfcPrint(const char * msg, rpmfc fc, FILE * fp);

/** \ingroup rpmfc
 * Destroy a file classifier.
 * @param fc		file classifier
 * @return		NULL always
 */
rpmfc rpmfcFree(rpmfc fc);

/** \ingroup rpmfc
 * Create a file classifier.
 * @return		new file classifier
 */
rpmfc rpmfcNew(void);

/** \ingroup rpmfc
 * Build file class dictionary and mappings.
 * @param fc		file classifier
 * @param argv		files to classify
 * @param fmode		files mode_t array (or NULL)
 * @return		RPMRC_OK on success
 */
rpmRC rpmfcClassify(rpmfc fc, ARGV_t argv, rpm_mode_t * fmode);

/** \ingroup rpmfc
 * Build file/package dependency dictionary and mappings.
 * @param fc		file classifier
 * @return		RPMRC_OK on success
 */
rpmRC rpmfcApply(rpmfc fc);

/** \ingroup rpmfc
 * Generate package dependencies.
 * @param spec		spec file control
 * @param pkg		package control
 * @return		RPMRC_OK on success
 */
rpmRC rpmfcGenerateDepends(const rpmSpec spec, Package pkg);

/** \ingroup rpmfc
 * Retrieve file classification provides
 * @param fc		file classifier
 * @return		rpmds dependency set of fc provides
 */
rpmds rpmfcProvides(rpmfc fc);

/** \ingroup rpmfc
 * Retrieve file classification requires
 * @param fc		file classifier
 * @return		rpmds dependency set of fc requires
 */
rpmds rpmfcRequires(rpmfc fc);

#ifdef __cplusplus
}
#endif

#endif /* _H_RPMFC_ */
