#include "system.h"
#include "build/rpmbuild.h"
#include "build.h"
#include "intl.h"

int buildplatform(char *arg, int buildAmount, char *passPhrase,
	         char *buildRoot, int fromTarball, int test, char *cookie);

int buildplatform(char *arg, int buildAmount, char *passPhrase,
	         char *buildRoot, int fromTarball, int test, char *cookie)
{

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

    rpmSetTables(RPM_MACHTABLE_BUILDARCH, RPM_MACHTABLE_BUILDOS);

    if (fromTarball) {
	specDir = alloca(BUFSIZ);
	strcpy(specDir, "%{_specdir}");
	/* XXX can't use spec->macros yet */
	expandMacros(NULL, &globalMacroContext, specDir, BUFSIZ);

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

	addMacro(&globalMacroContext, "_sourcedir", NULL, buf, RMIL_TARBALL);
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

#define	_anyarch(_f)	\
(((_f)&(RPMBUILD_PACKAGESOURCE|RPMBUILD_PACKAGEBINARY)) == RPMBUILD_PACKAGESOURCE)
    if (parseSpec(&spec, specfile, buildRoot, 0, passPhrase, cookie,
	_anyarch(buildAmount))) {
	    return 1;
    }
#undef	_anyarch

    if (buildSpec(spec, buildAmount, test)) {
	freeSpec(spec);
	return 1;
    }
    
    if (fromTarball) unlink(specfile);

    freeSpec(spec);
    
    return res;
}

int build(char *arg, int buildAmount, char *passPhrase,
	         char *buildRoot, int fromTarball, int test, char *cookie,
                 char * rcfile, char * arch, char * os, 
                 char *buildplatforms)
{
    char *platform, *t;
    int rc;

    if (buildplatforms == NULL) {
	rc =  buildplatform(arg, buildAmount, passPhrase, buildRoot,
		fromTarball, test, cookie);
	return rc;
    }

    /* parse up the build operators */

    printf("building these platforms: %s\n", buildplatforms);

    t = buildplatforms;
    while((platform = strtok(t, ",")) != NULL) {
	t = NULL;
	printf("building %s\n", platform);

	rpmSetVar(RPMVAR_BUILDPLATFORM,platform);
	rpmReadConfigFiles(rcfile, arch, os, 1, platform);
	rc = buildplatform(arg, buildAmount, passPhrase, buildRoot,
	    fromTarball, test, cookie);
	if (rc)
	    return rc;
    }

    return 0;
}
