#include "system.h"

#include <popt.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmurl.h>

#include "debug.h"

int rpmGlob(const char * patterns, int * argcPtr, ARGV_t * argvPtr)
{
    int ac = 0;
    const char ** av = NULL;
    int argc = 0;
    ARGV_t argv = NULL;
    char * globRoot = NULL;
    const char *home = getenv("HOME");
    int gflags = 0;
#ifdef ENABLE_NLS
    char * old_collate = NULL;
    char * old_ctype = NULL;
    const char * t;
#endif
    size_t maxb, nb;
    int i, j;
    int rc;

    if (home != NULL && strlen(home) > 0) 
	gflags |= GLOB_TILDE;

    /* Can't use argvSplit() here, it doesn't handle whitespace etc escapes */
    rc = poptParseArgvString(patterns, &ac, &av);
    if (rc)
	return rc;

#ifdef ENABLE_NLS
    t = setlocale(LC_COLLATE, NULL);
    if (t)
    	old_collate = xstrdup(t);
    t = setlocale(LC_CTYPE, NULL);
    if (t)
    	old_ctype = xstrdup(t);
    (void) setlocale(LC_COLLATE, "C");
    (void) setlocale(LC_CTYPE, "C");
#endif
	
    if (av != NULL)
    for (j = 0; j < ac; j++) {
	char * globURL;
	const char * path;
	int ut = urlPath(av[j], &path);
	int local = (ut == URL_IS_PATH) || (ut == URL_IS_UNKNOWN);
	size_t plen = strlen(path);
	int flags = gflags;
	int dir_only = (plen > 0 && path[plen-1] == '/');
	glob_t gl;

	if (!local || (!glob_pattern_p(av[j], 0) && strchr(path, '~') == NULL)) {
	    argvAdd(&argv, av[j]);
	    continue;
	}

#ifdef GLOB_ONLYDIR
	if (dir_only)
	    flags |= GLOB_ONLYDIR;
#endif
	
	gl.gl_pathc = 0;
	gl.gl_pathv = NULL;
	
	rc = glob(av[j], flags, NULL, &gl);
	if (rc)
	    goto exit;

	/* XXX Prepend the URL leader for globs that have stripped it off */
	maxb = 0;
	for (i = 0; i < gl.gl_pathc; i++) {
	    if ((nb = strlen(&(gl.gl_pathv[i][0]))) > maxb)
		maxb = nb;
	}
	
	nb = ((ut == URL_IS_PATH) ? (path - av[j]) : 0);
	maxb += nb;
	maxb += 1;
	globURL = globRoot = xmalloc(maxb);

	switch (ut) {
	case URL_IS_PATH:
	case URL_IS_DASH:
	    strncpy(globRoot, av[j], nb);
	    break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	case URL_IS_HKP:
	case URL_IS_UNKNOWN:
	default:
	    break;
	}
	globRoot += nb;
	*globRoot = '\0';

	for (i = 0; i < gl.gl_pathc; i++) {
	    const char * globFile = &(gl.gl_pathv[i][0]);

	    if (dir_only) {
		struct stat sb;
		if (lstat(gl.gl_pathv[i], &sb) || !S_ISDIR(sb.st_mode))
		    continue;
	    }
		
	    if (globRoot > globURL && globRoot[-1] == '/')
		while (*globFile == '/') globFile++;
	    strcpy(globRoot, globFile);
	    argvAdd(&argv, globURL);
	}
	globfree(&gl);
	free(globURL);
    }

    argc = argvCount(argv);
    if (argc > 0) {
	if (argvPtr)
	    *argvPtr = argv;
	if (argcPtr)
	    *argcPtr = argc;
	rc = 0;
    } else
	rc = 1;


exit:
#ifdef ENABLE_NLS	
    if (old_collate) {
	(void) setlocale(LC_COLLATE, old_collate);
	free(old_collate);
    }
    if (old_ctype) {
	(void) setlocale(LC_CTYPE, old_ctype);
	free(old_ctype);
    }
#endif
    av = _free(av);
    if (rc || argvPtr == NULL) {
	argvFree(argv);
    }
    return rc;
}

