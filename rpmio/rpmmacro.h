#ifndef _H_MACRO_
#define	_H_MACRO_

/** \ingroup rpmio
 * \file rpmio/rpmmacro.h
 */

/*! The structure used to store a macro. */
typedef /*@abstract@*/ struct MacroEntry_s {
    struct MacroEntry_s *prev;/*!< Macro entry stack. */
    const char *name;	/*!< Macro name. */
    const char *opts;	/*!< Macro parameters (a la getopt) */
    const char *body;	/*!< Macro body. */
    int	used;		/*!< No. of expansions. */
    int	level;		/*!< Scoping level. */
} * MacroEntry;

/*! The structure used to store the set of macros in a context. */
typedef /*@abstract@*/ struct MacroContext_s {
/*@owned@*//*@null@*/ MacroEntry *macroTable;	/*!< Macro entry table for context. */
    int		macrosAllocated;/*!< No. of allocated macros. */
    int		firstFree;	/*!< No. of macros. */
} * MacroContext;

/**
 * Markers for sources of macros added throughout rpm.
 */
#define	RMIL_DEFAULT	-15
#define	RMIL_MACROFILES	-13
#define	RMIL_RPMRC	-11

#define	RMIL_CMDLINE	-7
#define	RMIL_TARBALL	-5
#define	RMIL_SPEC	-3
#define	RMIL_OLDSPEC	-1
#define	RMIL_GLOBAL	0

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print macros to file stream.
 * @param mc		macro context (NULL uses global context).
 * @param fp		file stream (NULL uses stderr).
 */
void	rpmDumpMacroTable	(/*@null@*/ MacroContext mc,
					/*@null@*/ FILE * fp)
	/*@modifies *fp, fileSystem @*/;

/**
 * Expand macro into buffer.
 * @deprecated Use rpmExpand().
 * @todo Eliminate from API.
 * @param spec		cookie (unused)
 * @param mc		macro context (NULL uses global context).
 * @retval sbuf		input macro to expand, output expansion
 * @param sbuflen	size of buffer
 * @return		0 on success
 */
int	expandMacros	(/*@null@*/ void * spec, /*@null@*/ MacroContext mc,
				/*@in@*/ /*@out@*/ char * sbuf,
				size_t sbuflen)
	/*@modifies *sbuf, internalState @*/;

/**
 * Add macro to context.
 * @deprecated Use rpmDefineMacro().
 * @param mc		macro context (NULL uses global context).
 * @param n		macro name
 * @param o		macro paramaters
 * @param b		macro body
 * @param level		macro recursion level (0 is entry API)
 */
void	addMacro	(/*@null@*/ MacroContext mc, const char * n,
				/*@null@*/ const char * o,
				/*@null@*/ const char * b, int level)
	/*@modifies mc, internalState @*/;

/**
 * Delete macro from context.
 * @param mc		macro context (NULL uses global context).
 * @param n		macro name
 */
void	delMacro	(/*@null@*/ MacroContext mc, const char * n)
	/*@modifies mc, internalState @*/;

/**
 * Define macro in context.
 * @param mc		macro context (NULL uses global context).
 * @param n		macro name, options, body
 * @param level		macro recursion level (0 is entry API)
 * @return		@todo Document.
 */
int	rpmDefineMacro	(/*@null@*/ MacroContext mc, const char * macro,
				int level)
	/*@modifies mc, internalState @*/;

/**
 * Load macros from context into global context.
 * @param mc		macro context (NULL does nothing).
 * @param level		macro recursion level (0 is entry API)
 */
void	rpmLoadMacros	(/*@null@*/ MacroContext mc, int level)
	/*@modifies mc, internalState @*/;

/**
 * Initialize macro context from set of macrofile(s).
 * @param mc		macro context (NULL uses global context).
 * @param macrofiles	colon separated list of macro files (NULL does nothing)
 */
void	rpmInitMacros	(/*@null@*/ MacroContext mc, const char * macrofiles)
	/*@modifies mc, internalState, fileSystem @*/;

/**
 * Destroy macro context.
 * @param mc		macro context (NULL uses global context).
 */
void	rpmFreeMacros	(/*@null@*/ MacroContext mc)
	/*@modifies mc, internalState @*/;

typedef enum rpmCompressedMagic_e {
    COMPRESSED_NOT		= 0,	/*!< not compressed */
    COMPRESSED_OTHER		= 1,	/*!< gzip can handle */
    COMPRESSED_BZIP2		= 2,	/*!< bzip2 can handle */
    COMPRESSED_ZIP		= 3	/*!< unzip can handle */
} rpmCompressedMagic;

/**
 * Return type of compression used in file.
 * @param file		name of file
 * @retval compressed	address of compression type
 * @return		0 on success, 1 on I/O error
 */
int	isCompressed	(const char * file,
				/*@out@*/ rpmCompressedMagic * compressed)
	/*@modifies *compressed, fileSystem @*/;

/**
 * Return (malloc'ed) concatenated macro expansion(s).
 * @param arg		macro(s) to expand (NULL terminates list)
 * @return		macro expansion (malloc'ed)
 */
char *	rpmExpand	(/*@null@*/ const char * arg, ...)
	/*@*/;

/**
 * Canonicalize file path.
 * @param path		path to canonicalize (in-place)
 * @return		canonicalized path (malloc'ed)
 */
/*@null@*/ char * rpmCleanPath	(/*@null@*/ char * path)
	/*@modifies *path @*/;

/**
 * Return (malloc'ed) expanded, canonicalized, file path.
 * @param path		macro(s) to expand (NULL terminates list)
 * @return		canonicalized path (malloc'ed)
 */
/*@-redecl@*/
const char * rpmGetPath	(/*@null@*/ const char * path, ...)
	/*@*/;
/*@=redecl@*/

/**
 * Merge 3 args into path, any or all of which may be a url.
 * The leading part of the first URL encountered is used
 * for the result, other URL's are discarded, permitting
 * a primitive form of inheiritance.
 * @param root		root URL (often path to chroot, or NULL)
 * @param mdir		directory URL (often a directory, or NULL)
 * @param file		file URL (often a file, or NULL)
 * @return		expanded, merged, canonicalized path (malloc'ed)
 */
/*@-redecl@*/
const char * rpmGenPath	(/*@null@*/ const char * root,
			/*@null@*/ const char * mdir,
			/*@null@*/ const char * file)
	/*@*/;
/*@=redecl@*/

/**
 * Return macro expansion as a numeric value.
 * Boolean values ('Y' or 'y' returns 1, 'N' or 'n' returns 0)
 * are permitted as well. An undefined macro returns 0.
 * @param arg		macro to expand
 * @return		numeric value
 */
int	rpmExpandNumeric (const char * arg)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* _H_ MACRO_ */
