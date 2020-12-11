/** \ingroup rpmbuild
 * \file build/build.c
 *  Top-level build dispatcher.
 */

#include "system.h"

#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include "build/rpmbuild_internal.h"
#include "build/rpmbuild_misc.h"
#include "lib/rpmug.h"

#include "debug.h"

static rpm_time_t getBuildTime(void)
{
    rpm_time_t buildTime = 0;
    char *srcdate;
    time_t epoch;
    char *endptr;

    srcdate = getenv("SOURCE_DATE_EPOCH");
    if (srcdate && rpmExpandNumeric("%{?use_source_date_epoch_as_buildtime}")) {
        errno = 0;
        epoch = strtol(srcdate, &endptr, 10);
        if (srcdate == endptr || *endptr || errno != 0)
            rpmlog(RPMLOG_ERR, _("unable to parse SOURCE_DATE_EPOCH\n"));
        else
            buildTime = (int32_t) epoch;
    } else
        buildTime = (int32_t) time(NULL);

    return buildTime;
}

static char * buildHost(void)
{
    char* hostname;
    char *bhMacro;

    bhMacro = rpmExpand("%{?_buildhost}", NULL);
    if (strcmp(bhMacro, "") != 0) {
        rasprintf(&hostname, "%s", bhMacro);
    } else {
	hostname = rcalloc(NI_MAXHOST + 1, sizeof(*hostname));
	if (gethostname(hostname, NI_MAXHOST) == 0) {
	    struct addrinfo *ai, hints;
	    memset(&hints, 0, sizeof(hints));
	    hints.ai_flags = AI_CANONNAME;

	    if (getaddrinfo(hostname, NULL, &hints, &ai) == 0) {
		strcpy(hostname, ai->ai_canonname);
		freeaddrinfo(ai);
	    } else {
		rpmlog(RPMLOG_WARNING,
                    _("Could not canonicalize hostname: %s\n"), hostname);
	    }
	}
    }
    free(bhMacro);
    return(hostname);
}

/**
 */
static rpmRC doRmSource(rpmSpec spec)
{
    struct Source *p;
    Package pkg;
    int rc = 0;
    
    for (p = spec->sources; p != NULL; p = p->next) {
	if (! (p->flags & RPMBUILD_ISNO)) {
	    rc = unlink(p->path);
	    if (rc) goto exit;
	}
    }

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	for (p = pkg->icon; p != NULL; p = p->next) {
	    if (! (p->flags & RPMBUILD_ISNO)) {
		rc = unlink(p->path);
	        if (rc) goto exit;
	    }
	}
    }
exit:
    return !rc ? 0 : 1;
}

/*
 * @todo Single use by %%doc in files.c prevents static.
 */
rpmRC doScript(rpmSpec spec, rpmBuildFlags what, const char *name,
	       const char *sb, int test, StringBuf * sb_stdoutp)
{
    char *scriptName = NULL;
    char * buildDir = rpmGenPath(spec->rootDir, "%{_builddir}", "");
    char * buildCmd = NULL;
    char * buildTemplate = NULL;
    char * buildPost = NULL;
    const char * mTemplate = NULL;
    const char * mCmd = NULL;
    const char * mPost = NULL;
    int argc = 0;
    const char **argv = NULL;
    FILE * fp = NULL;

    FD_t fd = NULL;
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    
    switch (what) {
    case RPMBUILD_PREP:
	mTemplate = "%{__spec_prep_template}";
	mPost = "%{__spec_prep_post}";
	mCmd = "%{__spec_prep_cmd}";
	break;
    case RPMBUILD_BUILDREQUIRES:
	mTemplate = "%{__spec_buildrequires_template}";
	mPost = "%{__spec_buildrequires_post}";
	mCmd = "%{__spec_buildrequires_cmd}";
	break;
    case RPMBUILD_BUILD:
	mTemplate = "%{__spec_build_template}";
	mPost = "%{__spec_build_post}";
	mCmd = "%{__spec_build_cmd}";
	break;
    case RPMBUILD_INSTALL:
	mTemplate = "%{__spec_install_template}";
	mPost = "%{__spec_install_post}";
	mCmd = "%{__spec_install_cmd}";
	break;
    case RPMBUILD_CHECK:
	mTemplate = "%{__spec_check_template}";
	mPost = "%{__spec_check_post}";
	mCmd = "%{__spec_check_cmd}";
	break;
    case RPMBUILD_CLEAN:
	mTemplate = "%{__spec_clean_template}";
	mPost = "%{__spec_clean_post}";
	mCmd = "%{__spec_clean_cmd}";
	break;
    case RPMBUILD_RMBUILD:
	mTemplate = "%{__spec_clean_template}";
	mPost = "%{__spec_clean_post}";
	mCmd = "%{__spec_clean_cmd}";
	break;
    case RPMBUILD_STRINGBUF:
    default:
	mTemplate = "%{___build_template}";
	mPost = "%{___build_post}";
	mCmd = "%{___build_cmd}";
	break;
    }

    if ((what != RPMBUILD_RMBUILD) && sb == NULL) {
	rc = RPMRC_OK;
	goto exit;
    }
    
    fd = rpmMkTempFile(spec->rootDir, &scriptName);
    if (Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("Unable to open temp file: %s\n"), Fstrerror(fd));
	goto exit;
    }

    if ((fp = fdopen(Fileno(fd), "w")) == NULL) {
	rpmlog(RPMLOG_ERR, _("Unable to open stream: %s\n"), strerror(errno));
	goto exit;
    }
    
    buildTemplate = rpmExpand(mTemplate, NULL);
    buildPost = rpmExpand(mPost, NULL);

    (void) fputs(buildTemplate, fp);

    if (what != RPMBUILD_PREP && what != RPMBUILD_RMBUILD && spec->buildSubdir)
	fprintf(fp, "cd '%s'\n", spec->buildSubdir);

    if (what == RPMBUILD_RMBUILD) {
	if (spec->buildSubdir)
	    fprintf(fp, "rm -rf '%s'\n", spec->buildSubdir);
    } else if (sb != NULL)
	fprintf(fp, "%s", sb);

    (void) fputs(buildPost, fp);
    (void) fclose(fp);

    if (test) {
	rc = RPMRC_OK;
	goto exit;
    }
    
    if (buildDir && buildDir[0] != '/') {
	goto exit;
    }

    buildCmd = rpmExpand(mCmd, " ", scriptName, NULL);
    (void) poptParseArgvString(buildCmd, &argc, &argv);

    rpmlog(RPMLOG_NOTICE, _("Executing(%s): %s\n"), name, buildCmd);
    if (rpmfcExec((ARGV_const_t)argv, NULL, sb_stdoutp, 1,
		  spec->buildSubdir)) {
	rpmlog(RPMLOG_ERR, _("Bad exit status from %s (%s)\n"),
		scriptName, name);
	goto exit;
    }
    rc = RPMRC_OK;
    
exit:
    Fclose(fd);
    if (scriptName) {
	if (rc == RPMRC_OK && !rpmIsDebug())
	    (void) unlink(scriptName);
	free(scriptName);
    }
    free(argv);
    free(buildCmd);
    free(buildTemplate);
    free(buildPost);
    free(buildDir);

    return rc;
}

static int doBuildRequires(rpmSpec spec, int test)
{
    StringBuf sb_stdout = NULL;
    int outc;
    ARGV_t output = NULL;

    int rc = 1; /* assume failure */

    if (!spec->buildrequires) {
	rc = RPMRC_OK;
	goto exit;
    }

    if ((rc = doScript(spec, RPMBUILD_BUILDREQUIRES, "%generate_buildrequires",
		       getStringBuf(spec->buildrequires), test, &sb_stdout)))
	goto exit;

    /* add results to requires of the srpm */
    argvSplit(&output, getStringBuf(sb_stdout), "\n\r");
    outc = argvCount(output);

    for (int i = 0; i < outc; i++) {
	if (parseRCPOT(spec, spec->sourcePackage, output[i], RPMTAG_REQUIRENAME,
		       0, RPMSENSE_FIND_REQUIRES, addReqProvPkg, NULL))
	    goto exit;
    }

    rpmdsPutToHeader(
	*packageDependencies(spec->sourcePackage, RPMTAG_REQUIRENAME),
	spec->sourcePackage->header);

    rc = RPMRC_MISSINGBUILDREQUIRES;

 exit:
    freeStringBuf(sb_stdout);
    free(output);
    return rc;
}

static rpmRC doCheckBuildRequires(rpmts ts, rpmSpec spec, int test)
{
    rpmRC rc = RPMRC_OK;
    rpmps ps = rpmSpecCheckDeps(ts, spec);

    if (ps) {
	rpmlog(RPMLOG_ERR, _("Failed build dependencies:\n"));
	rpmpsPrint(NULL, ps);
	rc = RPMRC_MISSINGBUILDREQUIRES;
    }

    rpmpsFree(ps);
    return rc;
}

static rpmRC buildSpec(rpmts ts, BTA_t buildArgs, rpmSpec spec, int what)
{
    rpmRC rc = RPMRC_OK;
    int missing_buildreqs = 0;
    int test = (what & RPMBUILD_NOBUILD);
    char *cookie = buildArgs->cookie ? xstrdup(buildArgs->cookie) : NULL;
    /* handle quiet mode by capturing the output into a sink buffer */
    StringBuf sink = NULL;
    StringBuf *sbp = rpmIsVerbose() ? NULL : &sink;

    if (rpmExpandNumeric("%{?source_date_epoch_from_changelog}") &&
	getenv("SOURCE_DATE_EPOCH") == NULL) {
	/* Use date of first (== latest) changelog entry */
	Header h = spec->packages->header;
	struct rpmtd_s td;
	if (headerGet(h, RPMTAG_CHANGELOGTIME, &td, (HEADERGET_MINMEM|HEADERGET_RAW))) {
	    char sdestr[22];
	    long long sdeint = rpmtdGetNumber(&td);
	    if (sdeint % 86400 == 43200) /* date was rounded to 12:00 */
		/* make sure it is in the past, so that clamping times works */
		sdeint -= 43200;
	    snprintf(sdestr, sizeof(sdestr), "%lli", sdeint);
	    rpmlog(RPMLOG_NOTICE, _("setting %s=%s\n"), "SOURCE_DATE_EPOCH", sdestr);
	    setenv("SOURCE_DATE_EPOCH", sdestr, 0);
	    rpmtdFreeData(&td);
	}
    }

    spec->buildTime = getBuildTime();
    spec->buildHost = buildHost();

    /* XXX TODO: rootDir is only relevant during build, eliminate from spec */
    spec->rootDir = buildArgs->rootdir;
    if (!spec->recursing && spec->BACount) {
	int x;
	/* When iterating over BANames, do the source    */
	/* packaging on the first run, and skip RMSOURCE altogether */
	if (spec->BASpecs != NULL)
	for (x = 0; x < spec->BACount; x++) {
	    if ((rc = buildSpec(ts, buildArgs, spec->BASpecs[x],
				(what & ~RPMBUILD_RMSOURCE) |
				(x ? 0 : (what & RPMBUILD_PACKAGESOURCE))))) {
		goto exit;
	    }
	}
    } else {
	int didBuild = (what & (RPMBUILD_PREP|RPMBUILD_BUILD|RPMBUILD_INSTALL));
	int sourceOnly = ((what & RPMBUILD_PACKAGESOURCE) &&
	   !(what & (RPMBUILD_BUILD|RPMBUILD_INSTALL|RPMBUILD_PACKAGEBINARY)));

	if (!spec->buildrequires && sourceOnly) {
		/* don't run prep if not needed for source build */
		/* with(out) dynamic build requires*/
	    what &= ~(RPMBUILD_PREP);
	}

	if ((what & RPMBUILD_CHECKBUILDREQUIRES) &&
	    (rc = doCheckBuildRequires(ts, spec, test)))
		goto exit;

	if ((what & RPMBUILD_PREP) &&
	    (rc = doScript(spec, RPMBUILD_PREP, "%prep",
			   getStringBuf(spec->prep), test, sbp)))
		goto exit;

	if (what & RPMBUILD_BUILDREQUIRES)
            rc = doBuildRequires(spec, test);
	if ((what & RPMBUILD_CHECKBUILDREQUIRES) &&
	        (rc == RPMRC_MISSINGBUILDREQUIRES))
	    rc = doCheckBuildRequires(ts, spec, test);
	if (rc == RPMRC_MISSINGBUILDREQUIRES) {
	    if ((what & RPMBUILD_DUMPBUILDREQUIRES) &&
		!(spec->flags & RPMSPEC_FORCE)) {
		/* Create buildreqs package */
		char *nvr = headerGetAsString(spec->packages->header, RPMTAG_NVR);
		rasprintf(&spec->sourceRpmName, "%s.buildreqs.nosrc.rpm", nvr);
		free(nvr);
		/* free sources to not include them in the buildreqs package */
		spec->sources = freeSources(spec->sources);
		spec->numSources = 0;
		missing_buildreqs = 1;
		what = RPMBUILD_PACKAGESOURCE;
	    } else {
		rc = RPMRC_OK;
	    }
	} else if (rc) {
	    goto exit;
	}

	if ((what & RPMBUILD_BUILD) &&
	    (rc = doScript(spec, RPMBUILD_BUILD, "%build",
			   getStringBuf(spec->build), test, sbp)))
		goto exit;

	if ((what & RPMBUILD_INSTALL) &&
	    (rc = doScript(spec, RPMBUILD_INSTALL, "%install",
			   getStringBuf(spec->install), test, sbp)))
		goto exit;

	if ((what & RPMBUILD_CHECK) &&
	    (rc = doScript(spec, RPMBUILD_CHECK, "%check",
			   getStringBuf(spec->check), test, sbp)))
		goto exit;

	if ((what & RPMBUILD_PACKAGESOURCE) &&
	    (rc = processSourceFiles(spec, buildArgs->pkgFlags)))
		goto exit;

	if (((what & RPMBUILD_INSTALL) || (what & RPMBUILD_PACKAGEBINARY) ||
	    (what & RPMBUILD_FILECHECK)) &&
	    (rc = processBinaryFiles(spec, buildArgs->pkgFlags,
				     what & RPMBUILD_INSTALL, test)))
		goto exit;

	if (((what & RPMBUILD_INSTALL) || (what & RPMBUILD_PACKAGEBINARY)) &&
	    (rc = processBinaryPolicies(spec, test)))
		goto exit;

	if (((what & RPMBUILD_PACKAGESOURCE) && !test) &&
	    (rc = packageSources(spec, &cookie)))
		goto exit;

	if (((what & RPMBUILD_PACKAGEBINARY) && !test) &&
	    (rc = packageBinaries(spec, cookie, (didBuild == 0))))
		goto exit;
	
	if ((what & RPMBUILD_CLEAN) &&
	    (rc = doScript(spec, RPMBUILD_CLEAN, "%clean",
			   getStringBuf(spec->clean), test, sbp)))
		goto exit;

	if ((what & RPMBUILD_RMBUILD) &&
	    (rc = doScript(spec, RPMBUILD_RMBUILD, "--clean", NULL, test, sbp)))
		goto exit;
    }

    if (what & RPMBUILD_RMSOURCE)
	doRmSource(spec);

    if (what & RPMBUILD_RMSPEC)
	(void) unlink(spec->specFile);

exit:
    freeStringBuf(sink);
    free(cookie);
    spec->rootDir = NULL;
    if (rc != RPMRC_OK && rc != RPMRC_MISSINGBUILDREQUIRES &&
	    rpmlogGetNrecs() > 0) {
	rpmlog(RPMLOG_NOTICE, _("\n\nRPM build errors:\n"));
	rpmlogPrint(NULL);
    }
    rpmugFree();
    if (missing_buildreqs && !rc) {
	rc = RPMRC_MISSINGBUILDREQUIRES;
    }
    if (rc == RPMRC_FAIL) {
	rc = 1;
    }
    return rc;
}

int rpmSpecBuild(rpmts ts, rpmSpec spec, BTA_t buildArgs)
{
    /* buildSpec() can recurse with different buildAmount, pass it separately */
    return buildSpec(ts, buildArgs, spec, buildArgs->buildAmount);
}
