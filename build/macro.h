#ifndef _MACRO_H
#define	_MACRO_H

typedef struct MacroEntry {
	struct MacroEntry *prev;
	const char *name;	/* Macro name */
	const char *opts;	/* Macro parameters (ala getopt) */
	const char *body;	/* Macro body */
	int	used;		/* No. of expansions */
	int	level;
} MacroEntry;

typedef struct MacroContext {
	MacroEntry **	macroTable;
	int		macrosAllocated;
	int		firstFree;
} MacroContext;

#ifndef	__P
#ifdef __STDC__
#define	__P(protos)	protos
#else
#define	__P(protos)	()
#endif
#endif

void	initMacros	__P((MacroContext *mc, const char *macrofile));
void	freeMacros	__P((MacroContext *mc));

void	addMacro	__P((MacroContext *mc, const char *n, const char *o, const char *b, int depth));
void	delMacro	__P((MacroContext *mc, const char *n));
int	expandMacros	__P((Spec spec, MacroContext *mc, char *sbuf, size_t sbuflen));

const char *getMacroBody __P((MacroContext *mc, const char *name));

#endif	/* _MACRO_H */
