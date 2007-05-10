#ifndef _H_RPMFC_
#define _H_RPMFC_

#undef	FILE_RCSID
#include "magic.h"

/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmfc_debug;
/*@=exportlocal@*/

/**
 */
typedef /*@abstract@*/ struct rpmfc_s * rpmfc;

/**
 */
struct rpmfc_s {
    int nfiles;		/*!< no. of files */
    int fknown;		/*!< no. of classified files */
    int fwhite;		/*!< no. of "white" files */
    int ix;		/*!< current file index */
    int skipProv;	/*!< Don't auto-generate Provides:? */
    int skipReq;	/*!< Don't auto-generate Requires:? */
    int tracked;	/*!< Versioned Provides: tracking dependency added? */
    size_t brlen;	/*!< strlen(spec->buildRoot) */

    ARGV_t fn;		/*!< (#files) file names */
    ARGI_t fcolor;	/*!< (#files) file colors */
    ARGI_t fcdictx;	/*!< (#files) file class dictionary indices */
    ARGI_t fddictx;	/*!< (#files) file depends dictionary start */
    ARGI_t fddictn;	/*!< (#files) file depends dictionary no. entries */
    ARGV_t cdict;	/*!< (#classes) file class dictionary */
    ARGV_t ddict;	/*!< (#dependencies) file depends dictionary */
    ARGI_t ddictx;	/*!< (#dependencies) file->dependency mapping */

/*@relnull@*/
    rpmds provides;	/*!< (#provides) package provides */
/*@relnull@*/
    rpmds requires;	/*!< (#requires) package requires */

    StringBuf sb_java;	/*!< concatenated list of java colored files. */
    StringBuf sb_perl;	/*!< concatenated list of perl colored files. */
    StringBuf sb_python;/*!< concatenated list of python colored files. */

};

/**
 */
enum FCOLOR_e {
    RPMFC_BLACK			= 0,
    RPMFC_ELF32			= (1 <<  0),
    RPMFC_ELF64			= (1 <<  1),
#define	RPMFC_ELF	(RPMFC_ELF32|RPMFC_ELF64)

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
typedef	enum FCOLOR_e FCOLOR_t;

/**
 */
struct rpmfcTokens_s {
/*@observer@*/
    const char * token;
    int colors;
};

/**
 */
typedef struct rpmfcTokens_s * rpmfcToken;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return helper output.
 * @param av		helper argv (with possible macros)
 * @param sb_stdin	helper input
 * @retval *sb_stdoutp	helper output
 * @param failnonzero	IS non-zero helper exit status a failure?
 */
int rpmfcExec(ARGV_t av, StringBuf sb_stdin, /*@out@*/ StringBuf * sb_stdoutp,
		int failnonzero)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *sb_stdoutp, rpmGlobalMacroContext,
		fileSystem, internalState @*/
        /*@requires maxSet(sb_stdoutp) >= 0 @*/;

/**
 * Return file color given file(1) string.
 * @param fmstr		file(1) string
 * @return		file color
 */
/*@-exportlocal@*/
int rpmfcColoring(const char * fmstr)
	/*@*/;
/*@=exportlocal@*/

/**
 * Print results of file classification.
 * @todo Remove debugging routine.
 * @param msg		message prefix (NULL for none)
 * @param fc		file classifier
 * @param fp		output file handle (NULL for stderr)
 */
/*@-exportlocal@*/
void rpmfcPrint(/*@null@*/ const char * msg, rpmfc fc, /*@null@*/ FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies *fp, fc, fileSystem @*/;
/*@=exportlocal@*/

/**
 * Destroy a file classifier.
 * @param fc		file classifier
 * @return		NULL always
 */
/*@-exportlocal@*/
/*@null@*/
rpmfc rpmfcFree(/*@only@*/ /*@null@*/ rpmfc fc)
	/*@modifies fc @*/;
/*@=exportlocal@*/

/**
 * Create a file classifier.
 * @return		new file classifier
 */
/*@-exportlocal@*/
rpmfc rpmfcNew(void)
	/*@*/;
/*@=exportlocal@*/

/**
 * Build file class dictionary and mappings.
 * @param fc		file classifier
 * @param argv		files to classify
 * @param fmode		files mode_t array (or NULL)
 * @return		0 on success
 */
/*@-exportlocal@*/
int rpmfcClassify(rpmfc fc, ARGV_t argv, /*@null@*/ int16_t * fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fc, fileSystem, internalState @*/;
/*@=exportlocal@*/

/**
 * Build file/package dependency dictionary and mappings.
 * @param fc		file classifier
 * @return		0 on success
 */
/*@-exportlocal@*/
int rpmfcApply(rpmfc fc)
	/*@modifies fc @*/;
/*@=exportlocal@*/

/**
 * Generate package dependencies.
 * @param spec		spec file control
 * @param pkg		package control
 * @return		0 on success
 */
int rpmfcGenerateDepends(const Spec spec, Package pkg)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies pkg->cpioList, pkg->header,
		rpmGlobalMacroContext, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_RPMFC_ */
