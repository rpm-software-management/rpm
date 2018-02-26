#ifndef _H_INTERP_
#define _H_INTERP_

#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int _rpminterp_debug;

/** \ingroup rpminterp
 * Initialization flags for init().
 */
enum rpminterpFlag_e {
    RPMINTERP_DEFAULT 		= 0,
    RPMINTERP_NO_INIT    	= (1 << 0), /*!< skip interpreter init code */
    RPMINTERP_NO_IO_REDIR	= (1 << 1), /*!< skip I/O redirection code */
};

typedef rpmFlags rpminterpFlag;

struct rpminterp_s {
	const char	*name;
	rpmRC 		(*init) (ARGV_t *av, rpminterpFlag flags);
	void 		(*free) (void);
	rpmRC		(*run) (const char * str, char ** resultp);
};

typedef const struct rpminterp_s * rpminterp;

#define rpminterpInit(name, init, free, run) \
	const struct rpminterp_s rpminterp_ ## name = { #name, init, free, run}

/** \ingroup rpminterp
 * Load embedded language interpreter.
 * @param name		interpreter name (%{name:...})
 * @param modpath	path (supports glob) to library implementing rpminterp
 * @return		NULL on failure
 */
rpminterp rpminterpLoad(const char* name, const char *modpath);

#ifdef __cplusplus
}
#endif

#endif /* _H_INTERP_ */
