#include "config.h"

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "build.h"
#include "intl.h"
#include "lib/rpmlib.h"
#include "build/build.h"
#include "build/parse.h"
#include "build/spec.h"

int build(char *arg, int buildAmount, char *passPhrase,
	         char *buildRoot, int fromTarball, int test, char *cookie) {
    FILE *f;
    char * specfile;
    int res = 0;
    struct stat statbuf;
    char * specDir;
    char * tmpSpecFile;
    char * cmd;
    char * s;
    int count, fd;
    char buf[BUFSIZ];
    Spec spec = NULL;

    if (fromTarball) {
	specDir = rpmGetVar(RPMVAR_SPECDIR);
	tmpSpecFile = alloca(BUFSIZ);
	sprintf(tmpSpecFile, "%s/rpm-spec-file-%d", specDir, (int) getpid());

	cmd = alloca(strlen(arg) + 50 + strlen(tmpSpecFile));
	sprintf(cmd, "gunzip < %s | tar xOvf - Specfile 2>&1 > %s", arg,
			tmpSpecFile);
	if (!(f = popen(cmd, "r"))) {
	    fprintf(stderr, _("Failed to open tar pipe: %s\n"), 
			strerror(errno));
	    return 1;
	}
	if ((!fgets(buf, sizeof(buf) - 1, f)) || !strchr(buf, '/')) {
	    /* Try again */
	    pclose(f);

	    sprintf(cmd, "gunzip < %s | tar xOvf - \\*.spec 2>&1 > %s", arg,
		    tmpSpecFile);
	    if (!(f = popen(cmd, "r"))) {
		fprintf(stderr, _("Failed to open tar pipe: %s\n"), 
			strerror(errno));
		return 1;
	    }
	    if (!fgets(buf, sizeof(buf) - 1, f)) {
		/* Give up */
		fprintf(stderr, _("Failed to read spec file from %s\n"), arg);
		unlink(tmpSpecFile);
		return 1;
	    }
	}
	pclose(f);

	cmd = specfile = buf;
	while (*cmd) {
	    if (*cmd == '/') specfile = cmd + 1;
	    cmd++;
	}

	cmd = specfile;

	/* remove trailing \n */
	specfile = cmd + strlen(cmd) - 1;
	*specfile = '\0';

	specfile = alloca(strlen(specDir) + strlen(cmd) + 5);
	sprintf(specfile, "%s/%s", specDir, cmd);
	
	if (rename(tmpSpecFile, specfile)) {
	    fprintf(stderr, _("Failed to rename %s to %s: %s\n"),
		    tmpSpecFile, specfile, strerror(errno));
	    unlink(tmpSpecFile);
	    return 1;
	}

	/* Make the directory which contains the tarball the source 
	   directory for this run */

	if (*arg != '/') {
	    getcwd(buf, BUFSIZ);
	    strcat(buf, "/");
	    strcat(buf, arg);
	} else 
	    strcpy(buf, arg);

	cmd = buf + strlen(buf) - 1;
	while (*cmd != '/') cmd--;
	*cmd = '\0';

	rpmSetVar(RPMVAR_SOURCEDIR, buf);
    } else if (arg[0] == '/') {
	specfile = arg;
    } else {
	specfile = alloca(BUFSIZ);
	getcwd(specfile, BUFSIZ);
	strcat(specfile, "/");
	strcat(specfile, arg);
    }

    stat(specfile, &statbuf);
    if (! S_ISREG(statbuf.st_mode)) {
	fprintf(stderr, _("File is not a regular file: %s\n"), specfile);
	return 1;
    }
    
    if ((fd = open(specfile, O_RDONLY)) < 0) {
	fprintf(stderr, _("Unable to open spec file: %s\n"), specfile);
	return 1;
    }
    count = read(fd, buf, sizeof(buf) < 128 ? sizeof(buf) : 128);
    close(fd);
    s = buf;
    while(count--) {
	if (! (isprint(*s) || isspace(*s))) {
	    fprintf(stderr, _("File contains non-printable characters(%c): %s\n"), *s,
		    specfile);
	    return 1;
	}
	s++;
    }

    if (parseSpec(&spec, specfile, buildRoot, 0, passPhrase, cookie)) {
	return 1;
    }

    if (buildSpec(spec, buildAmount, test)) {
	freeSpec(spec);
	return 1;
    }
    
    if (fromTarball) unlink(specfile);

    freeSpec(spec);
    
    return res;
}
