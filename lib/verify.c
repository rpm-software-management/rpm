/** \ingroup rpmcli
 * \file lib/verify.c
 * Verify installed payload files from package metadata.
 */

#include "system.h"

#include <rpmcli.h>

#include "psm.h"
#include "rpmfi.h"
#include "rpmts.h"

#include "legacy.h"	/* XXX domd5(), uidToUname(), gnameToGid */
#include "ugid.h"
#include "misc.h"	/* XXX for uidToUname() and gnameToGid() */
#include "debug.h"

/*@access rpmps @*/
/*@access rpmProblem @*/
/*@access PSM_t @*/	/* XXX for %verifyscript through psmStage() */
/*@access FD_t @*/	/* XXX compared with NULL */

#define S_ISDEV(m) (S_ISBLK((m)) || S_ISCHR((m)))

int rpmVerifyFile(const rpmts ts, const rpmfi fi,
		rpmVerifyAttrs * res, rpmVerifyAttrs omitMask)
{
    unsigned short fmode = rpmfiFMode(fi);
    rpmfileAttrs fileAttrs = rpmfiFFlags(fi);
    rpmVerifyAttrs flags = rpmfiVFlags(fi);
    const char * fn = rpmfiFN(fi);
    const char * rootDir = rpmtsRootDir(ts);
    struct stat sb;
    int rc;

    /* Prepend the path to root (if specified). */
/*@-bounds@*/
    if (rootDir && *rootDir != '\0'
     && !(rootDir[0] == '/' && rootDir[1] == '\0'))
    {
	int nb = strlen(fn) + strlen(rootDir) + 1;
	char * tb = alloca(nb);
	char * t;

	t = tb;
	*t = '\0';
	t = stpcpy(t, rootDir);
	while (t > tb && t[-1] == '/') {
	    --t;
	    *t = '\0';
	}
	t = stpcpy(t, fn);
	fn = t;
    }
/*@=bounds@*/

    *res = RPMVERIFY_NONE;

    /*
     * Check to see if the file was installed - if not pretend all is OK.
     */
    switch (rpmfiFState(fi)) {
    case RPMFILE_STATE_NETSHARED:
    case RPMFILE_STATE_REPLACED:
    case RPMFILE_STATE_NOTINSTALLED:
	return 0;
	/*@notreached@*/ break;
    case RPMFILE_STATE_NORMAL:
	break;
    }

    if (fn == NULL || Lstat(fn, &sb) != 0) {
	*res |= RPMVERIFY_LSTATFAIL;
	return 1;
    }

    /*
     * Not all attributes of non-regular files can be verified.
     */
    if (S_ISDIR(sb.st_mode))
	flags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME | 
			RPMVERIFY_LINKTO);
    else if (S_ISLNK(sb.st_mode)) {
	flags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
		RPMVERIFY_MODE);
#if CHOWN_FOLLOWS_SYMLINK
	    flags &= ~(RPMVERIFY_USER | RPMVERIFY_GROUP);
#endif
    }
    else if (S_ISFIFO(sb.st_mode))
	flags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME | 
			RPMVERIFY_LINKTO);
    else if (S_ISCHR(sb.st_mode))
	flags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME | 
			RPMVERIFY_LINKTO);
    else if (S_ISBLK(sb.st_mode))
	flags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME | 
			RPMVERIFY_LINKTO);
    else 
	flags &= ~(RPMVERIFY_LINKTO);

    /*
     * Content checks of %ghost files are meaningless.
     */
    if (fileAttrs & RPMFILE_GHOST)
	flags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME | 
			RPMVERIFY_LINKTO);

    /*
     * Don't verify any features in omitMask.
     */
    flags &= ~(omitMask | RPMVERIFY_LSTATFAIL|RPMVERIFY_READFAIL|RPMVERIFY_READLINKFAIL);

    if (flags & RPMVERIFY_MD5) {
	unsigned char md5sum[16];
	size_t fsize;

	/* XXX If --nomd5, then prelinked library sizes are not corrected. */
	rc = domd5(fn, md5sum, 0, &fsize);
	sb.st_size = fsize;
	if (rc)
	    *res |= (RPMVERIFY_READFAIL|RPMVERIFY_MD5);
	else {
	    const unsigned char * md5 = rpmfiMD5(fi);
	    if (md5 == NULL || memcmp(md5sum, md5, sizeof(md5sum)))
		*res |= RPMVERIFY_MD5;
	}
    } 

    if (flags & RPMVERIFY_LINKTO) {
	char linkto[1024+1];
	int size = 0;

	if ((size = Readlink(fn, linkto, sizeof(linkto)-1)) == -1)
	    *res |= (RPMVERIFY_READLINKFAIL|RPMVERIFY_LINKTO);
	else {
	    const char * flink = rpmfiFLink(fi);
	    linkto[size] = '\0';
	    if (flink == NULL || strcmp(linkto, flink))
		*res |= RPMVERIFY_LINKTO;
	}
    } 

    if (flags & RPMVERIFY_FILESIZE) {
	if (sb.st_size != rpmfiFSize(fi))
	    *res |= RPMVERIFY_FILESIZE;
    } 

    if (flags & RPMVERIFY_MODE) {
	unsigned short metamode = fmode;
	unsigned short filemode;

	/*
	 * Platforms (like AIX) where sizeof(unsigned short) != sizeof(mode_t)
	 * need the (unsigned short) cast here. 
	 */
	filemode = (unsigned short)sb.st_mode;

	/*
	 * Comparing the type of %ghost files is meaningless, but perms are OK.
	 */
	if (fileAttrs & RPMFILE_GHOST) {
	    metamode &= ~0xf000;
	    filemode &= ~0xf000;
	}

	if (metamode != filemode)
	    *res |= RPMVERIFY_MODE;
    }

    if (flags & RPMVERIFY_RDEV) {
	if (S_ISCHR(fmode) != S_ISCHR(sb.st_mode)
	 || S_ISBLK(fmode) != S_ISBLK(sb.st_mode))
	{
	    *res |= RPMVERIFY_RDEV;
	} else if (S_ISDEV(fmode) && S_ISDEV(sb.st_mode)) {
	    uint_16 st_rdev = (sb.st_rdev & 0xffff);
	    uint_16 frdev = (rpmfiFRdev(fi) & 0xffff);
	    if (st_rdev != frdev)
		*res |= RPMVERIFY_RDEV;
	} 
    }

    if (flags & RPMVERIFY_MTIME) {
	if (sb.st_mtime != rpmfiFMtime(fi))
	    *res |= RPMVERIFY_MTIME;
    }

    if (flags & RPMVERIFY_USER) {
	const char * name = uidToUname(sb.st_uid);
	const char * fuser = rpmfiFUser(fi);
	if (name == NULL || fuser == NULL || strcmp(name, fuser))
	    *res |= RPMVERIFY_USER;
    }

    if (flags & RPMVERIFY_GROUP) {
	const char * name = gidToGname(sb.st_gid);
	const char * fgroup = rpmfiFGroup(fi);
	if (name == NULL || fgroup == NULL || strcmp(name, fgroup))
	    *res |= RPMVERIFY_GROUP;
    }

    return 0;
}

/**
 * Return exit code from running verify script from header.
 * @todo malloc/free/refcount handling is fishy here.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param fi		file info set
 * @param scriptFd      file handle to use for stderr (or NULL)
 * @return              0 on success
 */
static int rpmVerifyScript(/*@unused@*/ QVA_t qva, rpmts ts,
		rpmfi fi, /*@null@*/ FD_t scriptFd)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, fi, scriptFd, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    int rc = 0;
    PSM_t psm;

    psm = memset(alloca(sizeof(*psm)), 0, sizeof(*psm));
    psm->ts = rpmtsLink(ts, "rpmVerifyScript");

    if (scriptFd != NULL)
	rpmtsSetScriptFd(ts, scriptFd);

    psm->fi = rpmfiLink(fi, "rpmVerifyScript");
    psm->stepName = "verify";
    psm->scriptTag = RPMTAG_VERIFYSCRIPT;
    psm->progTag = RPMTAG_VERIFYSCRIPTPROG;
    rc = psmStage(psm, PSM_SCRIPT);
    psm->fi = rpmfiUnlink(fi, "rpmVerifyScript");

    if (scriptFd != NULL)
	rpmtsSetScriptFd(ts, NULL);

    psm->ts = rpmtsUnlink(ts, "rpmVerifyScript");

    return rc;
}

/**
 * Check file info from header against what's actually installed.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param fi		file info set
 * @return		0 no problems, 1 problems found
 */
static int verifyHeader(QVA_t qva, const rpmts ts, rpmfi fi)
	/*@globals fileSystem, internalState @*/
	/*@modifies fi, fileSystem, internalState  @*/
{
    char buf[BUFSIZ];
    char * t, * te;
    rpmVerifyAttrs verifyResult = 0;
    /*@-type@*/ /* FIX: union? */
    rpmVerifyAttrs omitMask = ((qva->qva_flags & VERIFY_ATTRS) ^ VERIFY_ATTRS);
    /*@=type@*/
    int ec = 0;		/* assume no problems */
    int i;

    te = t = buf;
    *te = '\0';

    fi = rpmfiLink(fi, "verifyHeader");
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)	/* XXX lclint */
    while ((i = rpmfiNext(fi)) >= 0) {
	rpmfileAttrs fileAttrs;
	int rc;

	fileAttrs = rpmfiFFlags(fi);

	/* If not verifying %ghost, skip ghost files. */
	if (!(qva->qva_fflags & RPMFILE_GHOST)
	&& (fileAttrs & RPMFILE_GHOST))
	    continue;

/*@-boundswrite@*/
	rc = rpmVerifyFile(ts, fi, &verifyResult, omitMask);
/*@=boundswrite@*/
	if (rc) {
	    if (!(fileAttrs & RPMFILE_MISSINGOK) || rpmIsVerbose()) {
		sprintf(te, _("missing    %s"), rpmfiFN(fi));
		te += strlen(te);
		ec = rc;
	    }
	} else if (verifyResult) {
	    const char * size, * md5, * link, * mtime, * mode;
	    const char * group, * user, * rdev;
	    /*@observer@*/ static const char *const aok = ".";
	    /*@observer@*/ static const char *const unknown = "?";

	    ec = 1;

#define	_verify(_RPMVERIFY_F, _C)	\
	((verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifylink(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READLINKFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifyfile(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
	
	    md5 = _verifyfile(RPMVERIFY_MD5, "5");
	    size = _verify(RPMVERIFY_FILESIZE, "S");
	    link = _verifylink(RPMVERIFY_LINKTO, "L");
	    mtime = _verify(RPMVERIFY_MTIME, "T");
	    rdev = _verify(RPMVERIFY_RDEV, "D");
	    user = _verify(RPMVERIFY_USER, "U");
	    group = _verify(RPMVERIFY_GROUP, "G");
	    mode = _verify(RPMVERIFY_MODE, "M");

#undef _verify
#undef _verifylink
#undef _verifyfile

	    sprintf(te, "%s%s%s%s%s%s%s%s %c %s",
			size, mode, md5, rdev, link, user, group, mtime, 
			((fileAttrs & RPMFILE_CONFIG)	? 'c' :
			 (fileAttrs & RPMFILE_DOC)	? 'd' :
			 (fileAttrs & RPMFILE_GHOST)	? 'g' :
			 (fileAttrs & RPMFILE_LICENSE)	? 'l' :
			 (fileAttrs & RPMFILE_README)	? 'r' : ' '), 
			rpmfiFN(fi));
	    te += strlen(te);
	}

/*@-boundswrite@*/
	if (te > t) {
	    *te++ = '\n';
	    *te = '\0';
	    rpmMessage(RPMMESS_NORMAL, "%s", t);
	    te = t = buf;
	    *t = '\0';
	}
/*@=boundswrite@*/
    }
    fi = rpmfiUnlink(fi, "verifyHeader");
	
    return ec;
}

/**
 * Check installed package dependencies for problems.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param h		header
 * @return		0 no problems, 1 problems found
 */
static int verifyDependencies(/*@unused@*/ QVA_t qva, rpmts ts,
		Header h)
	/*@globals fileSystem, internalState @*/
	/*@modifies ts, h, fileSystem, internalState @*/
{
    rpmps ps;
    int numProblems;
    int rc = 0;		/* assume no problems */
    int xx;
    int i;

    rpmtsClean(ts);
    (void) rpmtsAddInstallElement(ts, h, NULL, 0, NULL);

    xx = rpmtsCheck(ts);
    ps = rpmtsProblems(ts);

    numProblems = rpmpsNumProblems(ps);
    /*@-branchstate@*/
    if (ps != NULL && numProblems > 0) {
	const char * pkgNEVR, * altNEVR;
	rpmProblem p;
	char * t, * te;
	int nb = 512;

	for (i = 0; i < numProblems; i++) {
	    p = ps->probs + i;
	    altNEVR = (p->altNEVR ? p->altNEVR : "? ?altNEVR?");
	    nb += strlen(altNEVR+2) + sizeof(", ") - 1;
	}
	te = t = alloca(nb);
/*@-boundswrite@*/
	*te = '\0';
	pkgNEVR = (ps->probs->pkgNEVR ? ps->probs->pkgNEVR : "?pkgNEVR?");
	sprintf(te, _("Unsatisifed dependencies for %s: "), pkgNEVR);
	te += strlen(te);
	for (i = 0; i < numProblems; i++) {
	    p = ps->probs + i;
	    altNEVR = (p->altNEVR ? p->altNEVR : "? ?altNEVR?");
	    if (i) te = stpcpy(te, ", ");
	    /* XXX FIXME: should probably supply the "[R|C] " type prefix */
	    te = stpcpy(te, altNEVR+2);
	}

	if (te > t) {
	    *te++ = '\n';
	    *te = '\0';
	    rpmMessage(RPMMESS_NORMAL, "%s", t);
	    te = t;
	    *t = '\0';
	}
/*@=boundswrite@*/
	rc = 1;
    }
    /*@=branchstate@*/

    ps = rpmpsFree(ps);

    rpmtsClean(ts);

    return rc;
}

int showVerifyPackage(QVA_t qva, rpmts ts, Header h)
{
    int scareMem = 1;	/* XXX only rpmVerifyScript needs now */
    rpmfi fi;
    int ec = 0;
    int rc;

    fi = rpmfiNew(ts, NULL, h, RPMTAG_BASENAMES, scareMem);
    if (fi != NULL) {

	if (qva->qva_flags & VERIFY_DEPS) {
	    if ((rc = verifyDependencies(qva, ts, h)) != 0)
		ec = rc;
	}
	if (qva->qva_flags & VERIFY_FILES) {
	    if ((rc = verifyHeader(qva, ts, fi)) != 0)
		ec = rc;
	}
	if ((qva->qva_flags & VERIFY_SCRIPT)
	 && headerIsEntry(h, RPMTAG_VERIFYSCRIPT))
	{
	    FD_t fdo = fdDup(STDOUT_FILENO);
	    if ((rc = rpmVerifyScript(qva, ts, fi, fdo)) != 0)
		ec = rc;
	    if (fdo)
		rc = Fclose(fdo);
	}

	fi = rpmfiFree(fi, 1);
    }

    return ec;
}

int rpmcliVerify(rpmts ts, QVA_t qva, const char ** argv)
{
    const char * arg;
    int vsflags;
    int ec = 0;

    if (qva->qva_showPackage == NULL)
        qva->qva_showPackage = showVerifyPackage;

    /* XXX verify flags are inverted from query. */
    vsflags = 0;
    if (!(qva->qva_flags & VERIFY_DIGEST))
	vsflags |= _RPMTS_VSF_NODIGESTS;
    if (!(qva->qva_flags & VERIFY_SIGNATURE))
	vsflags |= _RPMTS_VSF_NOSIGNATURES;
    vsflags |= _RPMTS_VSF_VERIFY_LEGACY;

    (void) rpmtsSetVerifySigFlags(ts, vsflags);
    if (qva->qva_source == RPMQV_ALL) {
	/*@-nullpass@*/ /* FIX: argv can be NULL, cast to pass argv array */
	ec = rpmQueryVerify(qva, ts, (const char *) argv);
	/*@=nullpass@*/
    } else {
/*@-boundsread@*/
	if (argv != NULL)
	while ((arg = *argv++) != NULL) {
	    ec += rpmQueryVerify(qva, ts, arg);
	    rpmtsClean(ts);
	}
/*@=boundsread@*/
    }
    (void) rpmtsSetVerifySigFlags(ts, 0);

    if (qva->qva_showPackage == showVerifyPackage)
        qva->qva_showPackage = NULL;

    return ec;
}
