#ifndef _H_MACRO_
#define	_H_MACRO_

typedef /*@abstract@*/ struct MacroEntry {
	struct MacroEntry *prev;
	const char *name;	/* Macro name */
	const char *opts;	/* Macro parameters (ala getopt) */
	const char *body;	/* Macro body */
	int	used;		/* No. of expansions */
	int	level;
} MacroEntry;

typedef /*@abstract@*/ struct MacroContext {
	MacroEntry **	macroTable;
	int		macrosAllocated;
	int		firstFree;
} MacroContext;

/*
 * Markers for types of macros added throughout rpm.
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

void	rpmDumpMacroTable	(MacroContext * mc, FILE * fp);

/* XXX this is used only in build/expression.c and will go away. */
const char *getMacroBody (MacroContext *mc, const char *name);

int	expandMacros	(void * spec, MacroContext * mc, char * sbuf,
				size_t sbuflen);
void	addMacro	(MacroContext * mc, const char * n, const char * o,
				const char * b, int depth);
void	delMacro	(MacroContext * mc, const char * n);

int	rpmDefineMacro	(MacroContext * mc, const char * macro, int level);
void	rpmLoadMacros	(MacroContext *mc, int level);
void	rpmInitMacros	(MacroContext * mc, const char * macrofiles);
void	rpmFreeMacros	(MacroContext * mc);

#define COMPRESSED_NOT   0
#define COMPRESSED_OTHER 1
#define COMPRESSED_BZIP2 2
int	isCompressed	(const char * file, int * compressed);

char *	rpmExpand	(const char * arg, ...);
char *	rpmCleanPath	(char * path);
const char *rpmGetPath	(const char * path, ...);
const char *rpmGenPath	(const char * root, const char * mdir,
				const char * file);
int	rpmExpandNumeric (const char * arg);

#ifdef __cplusplus
}
#endif

#endif	/* _H_ MACRO_ */
