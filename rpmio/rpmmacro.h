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

extern MacroContext globalMacroContext;

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

#ifndef	__P
#ifdef __STDC__
#define	__P(protos)	protos
#else
#define	__P(protos)	()
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

void	dumpMacroTable	__P((MacroContext *mc, FILE *f));

/* XXX this is used only in build/expression.c and will go away. */
const char *getMacroBody __P((MacroContext *mc, const char *name));

int	expandMacros	__P((void *spec, MacroContext *mc, char *sbuf, size_t sbuflen));
void	addMacro	__P((MacroContext *mc, const char *n, const char *o, const char *b, int depth));
void	delMacro	__P((MacroContext *mc, const char *n));

int	rpmDefineMacro	__P((MacroContext *mc, const char *macro, int level));
void	initMacros	__P((MacroContext *mc, const char *macrofile));
void	freeMacros	__P((MacroContext *mc));

#define COMPRESSED_NOT   0
#define COMPRESSED_OTHER 1
#define COMPRESSED_BZIP2 2
int	isCompressed	__P((const char *file, int *compressed));

char *	rpmExpand	__P((const char *arg, ...));
const char *rpmGetPath	__P((const char *path, ...));
int	rpmExpandNumeric __P((const char *arg));

#ifdef __cplusplus
}
#endif

#endif	/* _H_ MACRO_ */
