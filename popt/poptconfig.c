/** \ingroup popt
 * \file popt/poptconfig.c
 */

/* (C) 1998-2000 Red Hat, Inc. -- Licensing details are in the COPYING
   file accompanying popt source distributions, available from 
   ftp://ftp.rpm.org/pub/rpm/dist. */

#include "system.h"
#include "poptint.h"

static void configLine(poptContext con, char * line)
	/*@modifies *line,
		con->execs, con->numExecs @*/
{
    int nameLength = strlen(con->appName);
    const char * opt;
    struct poptAlias alias;
    const char * entryType;
    const char * longName = NULL;
    char shortName = '\0';
    
    if (strncmp(line, con->appName, nameLength)) return;
    line += nameLength;
    if (*line == '\0' || !isspace(*line)) return;
    while (*line != '\0' && isspace(*line)) line++;
    entryType = line;

    while (*line == '\0' || !isspace(*line)) line++;
    *line++ = '\0';
    while (*line != '\0' && isspace(*line)) line++;
    if (*line == '\0') return;
    opt = line;

    while (*line == '\0' || !isspace(*line)) line++;
    *line++ = '\0';
    while (*line != '\0' && isspace(*line)) line++;
    if (*line == '\0') return;

    if (opt[0] == '-' && opt[1] == '-')
	longName = opt + 2;
    else if (opt[0] == '-' && !opt[2])
	shortName = opt[1];

    if (!strcmp(entryType, "alias")) {
	if (poptParseArgvString(line, &alias.argc, &alias.argv)) return;
	alias.longName = longName, alias.shortName = shortName;
	(void) poptAddAlias(con, alias, 0);
    } else if (!strcmp(entryType, "exec")) {
	con->execs = realloc(con->execs,
				sizeof(*con->execs) * (con->numExecs + 1));
	if (con->execs == NULL) return;	/* XXX can't happen */
	if (longName)
	    con->execs[con->numExecs].longName = xstrdup(longName);
	else
	    con->execs[con->numExecs].longName = NULL;

	con->execs[con->numExecs].shortName = shortName;
	con->execs[con->numExecs].script = xstrdup(line);
	
	/*@-noeffect@*/		/* LCL: broken? */
	con->numExecs++;
	/*@=noeffect@*/
    }
}

int poptReadConfigFile(poptContext con, const char * fn)
{
    const char * file, * chptr, * end;
    char * buf;
/*@dependent@*/ char * dst;
    int fd, rc;
    off_t fileLength;

    fd = open(fn, O_RDONLY);
    if (fd < 0)
	return (errno == ENOENT ? 0 : POPT_ERROR_ERRNO);

    fileLength = lseek(fd, 0, SEEK_END);
    if (fileLength == -1 || lseek(fd, 0, 0) == -1) {
	rc = errno;
	(void) close(fd);
	errno = rc;
	return POPT_ERROR_ERRNO;
    }

    file = alloca(fileLength + 1);
    if (read(fd, (char *)file, fileLength) != fileLength) {
	rc = errno;
	(void) close(fd);
	errno = rc;
	return POPT_ERROR_ERRNO;
    }
    if (close(fd) == -1)
	return POPT_ERROR_ERRNO;

    dst = buf = alloca(fileLength + 1);

    chptr = file;
    end = (file + fileLength);
    /*@-infloops@*/	/* LCL: can't detect chptr++ */
    while (chptr < end) {
	switch (*chptr) {
	  case '\n':
	    *dst = '\0';
	    dst = buf;
	    while (*dst && isspace(*dst)) dst++;
	    if (*dst && *dst != '#')
		configLine(con, dst);
	    chptr++;
	    break;
	  case '\\':
	    *dst++ = *chptr++;
	    if (chptr < end) {
		if (*chptr == '\n') 
		    dst--, chptr++;	
		    /* \ at the end of a line does not insert a \n */
		else
		    *dst++ = *chptr++;
	    }
	    break;
	  default:
	    *dst++ = *chptr++;
	    break;
	}
    }
    /*@=infloops@*/

    return 0;
}

int poptReadDefaultConfig(poptContext con, /*@unused@*/ int useEnv) {
    char * fn, * home;
    int rc;

    if (!con->appName) return 0;

    rc = poptReadConfigFile(con, "/etc/popt");
    if (rc) return rc;
    if (getuid() != geteuid()) return 0;

    if ((home = getenv("HOME"))) {
	fn = alloca(strlen(home) + 20);
	strcpy(fn, home);
	strcat(fn, "/.popt");
	rc = poptReadConfigFile(con, fn);
	if (rc) return rc;
    }

    return 0;
}
