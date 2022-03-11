#ifndef _H_MACRO_INTERNAL
#define	_H_MACRO_INTERNAL

/** \ingroup rpmio
 * \file rpmmacro_internal.h
 *
 * Internal Macro API
 */

#include <rpm/rpmmacro.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MacroBuf_s *MacroBuf;
typedef void (*macroFunc)(MacroBuf mb, rpmMacroEntry me, ARGV_t argv, size_t *parsed);

/*! The structure used to store a macro. */
struct rpmMacroEntry_s {
    struct rpmMacroEntry_s *prev;/*!< Macro entry stack. */
    const char *name;  	/*!< Macro name. */
    const char *opts;  	/*!< Macro parameters (a la getopt) */
    const char *body;	/*!< Macro body. */
    macroFunc func;	/*!< Macro function (builtin macros) */
    void *priv;		/*!< App private (aux macros) */
    int nargs;		/*!< Number of required args */
    int flags;		/*!< Macro state bits. */
    int level;          /*!< Scoping level. */
    char arena[];   	/*!< String arena. */
};

void mbAppend(MacroBuf mb, char c);
void mbAppendStr(MacroBuf mb, const char *str);
void mbErr(MacroBuf mb, int error, const char *fmt, ...);

/** \ingroup rpmmacro
 * Find the end of a macro call
 * @param str           pointer to the character after the initial '%'
 * @return              pointer to the next character after the macro
 */
RPM_GNUC_INTERNAL
const char *findMacroEnd(const char *str);

typedef int (*rgetoptcb)(int c, const char *oarg, int oint, void *data);

RPM_GNUC_INTERNAL
int rgetopt(int argc, char * const argv[], const char *opts,
		rgetoptcb callback, void *data);

RPM_GNUC_INTERNAL
void splitQuoted(ARGV_t *av, const char * str, const char * seps);

RPM_GNUC_INTERNAL
char *unsplitQuoted(ARGV_const_t av, const char *sep);

/* RPM_GNUC_INTERNAL omitted to allow use from librpmbuild */
int rpmPushMacroAux(rpmMacroContext mc,
		    const char * n, const char * o,
		    macroFunc f, void *priv, int nargs,
		    int level, rpmMacroFlags flags);

#ifdef __cplusplus
}
#endif

#endif	/* _H_ MACRO_INTERNAL */
