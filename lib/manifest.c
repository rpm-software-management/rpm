/** \ingroup rpmcli
 * \file lib/manifest.c
 */

#include "system.h"

#include <rpmio_internal.h>
#include <rpmlib.h>
#include <rpmmacro.h>

#include "stringbuf.h"
#include "manifest.h"
#include "misc.h"
#include "debug.h"

/*@access StringBuf @*/

/*@-boundswrite@*/
char * rpmPermsString(int mode)
{
    char *perms = xstrdup("----------");
   
    if (S_ISREG(mode)) 
	perms[0] = '-';
    else if (S_ISDIR(mode)) 
	perms[0] = 'd';
    else if (S_ISLNK(mode))
	perms[0] = 'l';
    else if (S_ISFIFO(mode)) 
	perms[0] = 'p';
    /*@-unrecog@*/
    else if (S_ISSOCK(mode)) 
	perms[0] = 's';
    /*@=unrecog@*/
    else if (S_ISCHR(mode))
	perms[0] = 'c';
    else if (S_ISBLK(mode))
	perms[0] = 'b';
    else
	perms[0] = '?';

    if (mode & S_IRUSR) perms[1] = 'r';
    if (mode & S_IWUSR) perms[2] = 'w';
    if (mode & S_IXUSR) perms[3] = 'x';
 
    if (mode & S_IRGRP) perms[4] = 'r';
    if (mode & S_IWGRP) perms[5] = 'w';
    if (mode & S_IXGRP) perms[6] = 'x';

    if (mode & S_IROTH) perms[7] = 'r';
    if (mode & S_IWOTH) perms[8] = 'w';
    if (mode & S_IXOTH) perms[9] = 'x';

    if (mode & S_ISUID)
	perms[3] = ((mode & S_IXUSR) ? 's' : 'S'); 

    if (mode & S_ISGID)
	perms[6] = ((mode & S_IXGRP) ? 's' : 'S'); 

    if (mode & S_ISVTX)
	perms[9] = ((mode & S_IXOTH) ? 't' : 'T');

    return perms;
}
/*@=boundswrite@*/

/**@todo Infinite loops through manifest files exist, operator error for now. */
/*@-boundsread@*/
rpmRC rpmReadPackageManifest(FD_t fd, int * argcPtr, const char *** argvPtr)
{
    StringBuf sb = newStringBuf();
    char * s = NULL;
    char * se;
    int ac = 0;
    const char ** av = NULL;
    int argc = (argcPtr ? *argcPtr : 0);
    const char ** argv = (argvPtr ? *argvPtr : NULL);
/*@+voidabstract@*/
    FILE * f = (FILE *) fdGetFp(fd);
/*@=voidabstract@*/
    rpmRC rpmrc = RPMRC_OK;
    int i, j, next, npre;

/*@-boundswrite@*/
    if (f != NULL)
    while (1) {
	char line[BUFSIZ];

	/* Read next line. */
	s = fgets(line, sizeof(line) - 1, f);
	if (s == NULL) {
	    /* XXX Ferror check needed */
	    break;
	}

	/* Skip comments. */
	if ((se = strchr(s, '#')) != NULL) *se = '\0';

	/* Trim white space. */
	se = s + strlen(s);
	while (se > s && (se[-1] == '\n' || se[-1] == '\r'))
	    *(--se) = '\0';
	while (*s && strchr(" \f\n\r\t\v", *s) != NULL)
	    s++;
	if (*s == '\0') continue;

	/* Insure that file contains only ASCII */
	if (*s < 32) {
	    rpmrc = RPMRC_NOTFOUND;
	    goto exit;
	}

	/* Concatenate next line in buffer. */
	*se++ = ' ';
	*se = '\0';
	appendStringBuf(sb, s);
    }

    /*@-branchstate@*/
    if (s == NULL)		/* XXX always true */
	s = getStringBuf(sb);
    /*@=branchstate@*/

    if (!(s && *s)) {
	rpmrc = RPMRC_NOTFOUND;
	goto exit;
    }

    /* Glob manifest items. */
    rpmrc = rpmGlob(s, &ac, &av);
    if (rpmrc != RPMRC_OK) goto exit;

    rpmMessage(RPMMESS_DEBUG, _("adding %d args from manifest.\n"), ac);

    /* Count non-NULL args, keeping track of 1st arg after last NULL. */
    npre = 0;
    next = 0;
    if (argv != NULL)
    for (i = 0; i < argc; i++) {
	if (argv[i] != NULL)
	    npre++;
	else if (i >= next)
	    next = i + 1;
    }

    /* Copy old arg list, inserting manifest before argv[next]. */
    if (argv != NULL) {
	int nac = npre + ac;
	const char ** nav = xcalloc((nac + 1), sizeof(*nav));

	for (i = 0, j = 0; i < next; i++) {
	    if (argv[i] != NULL)
		nav[j++] = argv[i];
	}

	if (ac)
	    memcpy(nav + j, av, ac * sizeof(*nav));
	if ((argc - next) > 0)
	    memcpy(nav + j + ac, argv + next, (argc - next) * sizeof(*nav));
	nav[nac] = NULL;

	if (argvPtr)
	    *argvPtr = argv = _free(argv);
	av = _free(av);
	av = nav;
	ac = nac;
    }

    /* Save new argc/argv list. */
    if (argvPtr) {
	*argvPtr = _free(*argvPtr);
	*argvPtr = av;
    }
    if (argcPtr)
	*argcPtr = ac;
/*@=boundswrite@*/

exit:
    /*@-branchstate@*/
    if (argvPtr == NULL || (rpmrc != RPMRC_OK && av)) {
	if (av)
/*@-boundswrite@*/
	for (i = 0; i < ac; i++)
	    /*@-unqualifiedtrans@*/av[i] = _free(av[i]); /*@=unqualifiedtrans@*/
/*@=boundswrite@*/
	/*@-dependenttrans@*/ av = _free(av); /*@=dependenttrans@*/
    }
    /*@=branchstate@*/
    sb = freeStringBuf(sb);
    /*@-nullstate@*/ /* FIX: *argvPtr may be NULL. */
    return rpmrc;
    /*@=nullstate@*/
}
/*@=boundsread@*/
