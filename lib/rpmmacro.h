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
#define	RMIL_MACROFILES	-9
#define	RMIL_RPMRC	-7
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

#define COMPRESSED_NOT   0
#define COMPRESSED_OTHER 1
#define COMPRESSED_BZIP2 2

int isCompressed(char *file, int *compressed);

void	dumpMacroTable	__P((MacroContext *mc));

void	initMacros	__P((MacroContext *mc, const char *macrofile));
void	freeMacros	__P((MacroContext *mc));

void	addMacro	__P((MacroContext *mc, const char *n, const char *o, const char *b, int depth));
void	delMacro	__P((MacroContext *mc, const char *n));
int	expandMacros	__P((void *spec, MacroContext *mc, char *sbuf, size_t sbuflen));

const char *getMacroBody __P((MacroContext *mc, const char *name));

#ifdef __cplusplus
}
#endif

#endif	/* _H_ MACRO_ */
