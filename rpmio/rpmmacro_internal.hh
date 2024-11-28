#ifndef _H_MACRO_INTERNAL
#define	_H_MACRO_INTERNAL

#include <mutex>
#include <string>
#include <utility>
#include <initializer_list>

#include <rpm/rpmmacro.h>
#include <rpm/rpmutil.h>
#include <rpm/argv.h>

/** \ingroup rpmio
 * \file rpmmacro_internal.h
 *
 * Internal Macro API
 */

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

namespace rpm {

/*
 * This is basically a RAII proxy C++ native macro interface, prefer it
 * over the public C API for all internal needs.
 * The constructor grabs a lock on the underlying macro context and
 * automatically unlocks when the handle goes out of scope. This allows
 * multiple macro operations on a single lock/unlock cycle, while also
 * making sure locking and unlocking are not forgotten. Use as local
 * variable only and in the smallest possible scope to get the job
 * done, mind what other code gets called while holding the handle.
 *
 * Generally the method names and arguments map to the C API in obvious ways,
 * exceptions noted below.
 */
class macros {
public:
    /* Clear all macro definitions in this context, like rpmFreeMacros() */
    void clear();
    /* Copy all macros from this context to another one */
    void copy(rpm::macros & dest, int level);
    int define(const char *macro, int level);
    void dump(FILE *fp = stderr);
    /* Expand macros to a C++ string, with a return code (rc, string) */
    std::pair<int,std::string> expand(const std::string & src, int flags = 0);
    std::pair<int,std::string> expand(const std::initializer_list<std::string> src,
					int flags = 0);
    std::pair<int,std::string> expand_this(const std::string & n, ARGV_const_t args,
					int flags = 0);
    /* Expand macros to numeric value, with a return code (rc, number) */
    std::pair<int,int64_t> expand_numeric(const std::string & src, int flags = 0);
    std::pair<int,int64_t> expand_numeric(const std::initializer_list<std::string> & src,
					int flags = 0);
    void init(const char *macrofiles);
    bool is_defined(const char *n);
    bool is_parametric(const char *n);
    int load(const char *fn);
    int pop(const char *n);
    int push(const char *n, const char *o, const char *b,
		int level, int flags = RPMMACRO_DEFAULT);
    int push_aux(const char *n, const char *o,
		macroFunc f, void *priv, int nargs,
		int level, int flags = RPMMACRO_DEFAULT);

    macros(rpmMacroContext mctx = rpmGlobalMacroContext);
    ~macros() = default;
private:
    rpmMacroContext mc;
    std::lock_guard<std::recursive_mutex> lock;
};

/* Join args into a / separated normalized path. Optionally expand args first */
std::string join_path(const std::initializer_list<std::string> & args,
			bool expand = true);

/* Same as expand() but return as normalized path */
std::string expand_path(const std::initializer_list<std::string> & args);

/* Normalize a path. */
std::string normalize_path(const std::string & args);

}; /* namespace rpm */

#endif	/* _H_ MACRO_INTERNAL */
