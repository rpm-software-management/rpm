/**
 * \file lib/fileutil.c
 */

#include "system.h"
#include <rpmlib.h>
#include "misc.h"
#include "debug.h"

static struct {
    const char *token;
    int nargs;
} cmds[] = {
    { "mkdir",		1 },
#define	RPMSYSCALL_MKDIR	0
    { "rmdir",		1 },
#define	RPMSYSCALL_RMDIR	1
    { "mv",		-2 },
#define	RPMSYSCALL_MV		2
    { "symlink",	2 },
#define	RPMSYSCALL_SYMLINK	3
    { "S_ISLNK",	1 }
#define	RPMSYSCALL_ISLNK	4
};
int ncmds = sizeof(cmds)/sizeof(cmds[0]);

int rpmSyscall(const char * cmd, int noexec)
{
    const char ** argv = NULL;
    int argc;
    struct stat st;
    int i, j;
    int rc;

    rc = (noexec)
	? poptParseArgvString(cmd, &argc, &argv)
	: rpmGlob(cmd, &argc, &argv);

    if (rc)
	goto exit;

    /* Check: argv[0] must be known. */
    for (i = 0; i < ncmds; i++) {
	if (!strcmp(cmds[i].token, argv[0]))
	    break;
    }
    if (i >= ncmds) {
	rc = EPERM;
	goto exit;
    }

    /* Check: must have exactly (or, if negative, at least) nargs. */
    if (cmds[i].nargs >= 0 && argc != (cmds[i].nargs+1)) {
	rc = EPERM;
	goto exit;
    } else if (argc < (1-cmds[i].nargs)) {
	rc = EPERM;
	goto exit;
    }

    /* Check: all args (except for 1st arg to symlink) must start with '/'. */
    for (j = (i == RPMSYSCALL_SYMLINK ? 2 : 1); j < argc; j++) {
	if (argv[j][0] != '/') {
	    rc = EPERM;
	    goto exit;
	}
    }

    /* Check: parse only with 2 or more args, last arg cannot be glob. */
    if (noexec) {
	if (argc > 2 && myGlobPatternP(argv[argc-1])) {
	    rc = 1;
	    goto exit;
	}
	rc = 0;
	goto exit;
    }

    /* Execute only checks below */

    switch (i) {
    case RPMSYSCALL_MKDIR:	/* mkdir */
    {	mode_t mode = 0755;
	rc = mkdir(argv[1], mode);
	if (rc < 0) rc = errno;
	if (stat(argv[1], &st) < 0) {
	    if (rc == 0) rc = errno;
	    goto exit;
	}
	if (!S_ISDIR(st.st_mode)) {
	    if (rc == 0) rc = ENOTDIR;
	    goto exit;
	}
	rc = 0;
    }	break;
    case RPMSYSCALL_RMDIR:	/* rmdir */
	rc = rmdir(argv[1]);
	if (rc < 0) rc = errno;
	break;
    case RPMSYSCALL_MV:		/* mv */
    {	dev_t dev;
	const char * fn, * bn;
	char * t;
	size_t bnlen;

	/* Check: if more than 2 args ... */
	if (argc > 3) {
	    /* ... last arg must be existing directory ... */
	    if (stat(argv[argc-1], &st) < 0) {
		if (rc == 0) rc = errno;
		goto exit;
	    }
	    if (!S_ISDIR(st.st_mode)) {
		if (rc == 0) rc = ENOTDIR;
		goto exit;
	    }
	    /* ... and other args must all be on the same device. */
	    dev = st.st_dev;
	    bnlen = 0;
	    for (j = 1; j < (argc-1); j++) {
		if (stat(argv[j], &st) < 0) {
		    if (rc == 0) rc = errno;
		    goto exit;
		}
		if (dev != st.st_dev) {
		    if (rc == 0) rc = EXDEV;
		    goto exit;
		}
		bn = strrchr(argv[j], '/');
		if (bn) {
		    int k = strlen(bn);
		    if (k > bnlen) bnlen = k;
		}
	    }
	    /* Everything looks OK, so do the renames. */
	    fn = t = alloca(strlen(argv[argc-1])+2+bnlen);
	    t = stpcpy(t, argv[argc-1]);
	    if (t[-1] != '/') *t++ = '/';
	    for (j = 1; j < (argc-1); j++) {
		bn = strrchr(argv[j], '/');
		(void) stpcpy(t, bn+1);
		rc = rename(argv[j], fn);
		if (rc < 0) {
		    rc = errno;
		    goto exit;
		}
	    }
	} else { /* Otherwise exactly 2 args. */
	    rc = rename(argv[1], argv[2]);
	    if (rc < 0) rc = errno;
        }
    }	break;
    case RPMSYSCALL_SYMLINK:	/* symlink */
	rc = symlink(argv[1], argv[2]);
	if (rc < 0) rc = errno;
	break;
    case RPMSYSCALL_ISLNK:	/* stat(2) with S_ISLNK */
	if (stat(argv[1], &st) < 0) {
	    if (rc == 0) rc = errno;
	    goto exit;
	}
	if (S_ISLNK(st.st_mode)) {
	    if (rc == 0) rc = EPERM;
	    goto exit;
	}
	rc = 0;
	break;
    default:
	rc = EPERM;
	break;
    }

exit:
    if (argv)
	free((void *)argv);
    return rc;
}
