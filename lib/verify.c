/** \ingroup rpmcli
 * \file lib/verify.c
 * Verify installed payload files from package metadata.
 */

#include "system.h"

#include "psm.h"
#include <rpmcli.h>

#include "legacy.h"	/* XXX domd5(), uidToUname(), gnameToGid */
#include "ugid.h"
#include "misc.h"	/* XXX for uidToUname() and gnameToGid() */
#include "debug.h"

/*@access rpmProblem @*/
/*@access rpmTransactionSet @*/
/*@access PSM_t @*/	/* XXX for %verifyscript through psmStage() */
/*@access FD_t @*/	/* XXX compared with NULL */

#define S_ISDEV(m) (S_ISBLK((m)) || S_ISCHR((m)))

int rpmVerifyFile(const rpmTransactionSet ts, const TFI_t fi,
		rpmVerifyAttrs * result, rpmVerifyAttrs omitMask)
{
    unsigned short fmode = tfiGetFMode(fi);
    rpmfileAttrs fileAttrs = tfiGetFFlags(fi);
    rpmVerifyAttrs flags = tfiGetVFlags(fi);
    const char * filespec = tfiGetFN(fi);
    int rc;
    struct stat sb;

    /* Prepend the path to root (if specified). */
    if (ts->rootDir && *ts->rootDir != '\0'
     && !(ts->rootDir[0] == '/' && ts->rootDir[1] == '\0'))
    {
	int nb = strlen(filespec) + strlen(ts->rootDir) + 1;
	char * tb = alloca(nb);
	char * t;

	t = tb;
	*t = '\0';
	t = stpcpy(t, ts->rootDir);
	while (t > tb && t[-1] == '/') {
	    --t;
	    *t = '\0';
	}
	t = stpcpy(t, filespec);
	filespec = t;
    }

    *result = RPMVERIFY_NONE;

    /*
     * Check to see if the file was installed - if not pretend all is OK.
     */
    switch (tfiGetFState(fi)) {
    case RPMFILE_STATE_NETSHARED:
    case RPMFILE_STATE_REPLACED:
    case RPMFILE_STATE_NOTINSTALLED:
	return 0;
	/*@notreached@*/ break;
    case RPMFILE_STATE_NORMAL:
	break;
    }

    if (filespec == NULL || Lstat(filespec, &sb) != 0) {
	*result |= RPMVERIFY_LSTATFAIL;
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
	byte md5sum[16];

	rc = domd5(filespec, md5sum, 0);
	if (rc)
	    *result |= (RPMVERIFY_READFAIL|RPMVERIFY_MD5);
	else {
	    const byte * md5 = tfiGetMD5(fi);
	    if (md5 == NULL || memcmp(md5sum, md5, sizeof(md5sum)))
		*result |= RPMVERIFY_MD5;
	}
    } 

    if (flags & RPMVERIFY_LINKTO) {
	char linkto[1024];
	int size = 0;

	if ((size = Readlink(filespec, linkto, sizeof(linkto)-1)) == -1)
	    *result |= (RPMVERIFY_READLINKFAIL|RPMVERIFY_LINKTO);
	else {
	    const char * flink = tfiGetFLink(fi);
	    linkto[size] = '\0';
	    if (flink == NULL || strcmp(linkto, flink))
		*result |= RPMVERIFY_LINKTO;
	}
    } 

    if (flags & RPMVERIFY_FILESIZE) {
	if (sb.st_size != tfiGetFSize(fi))
	    *result |= RPMVERIFY_FILESIZE;
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
	    *result |= RPMVERIFY_MODE;
    }

    if (flags & RPMVERIFY_RDEV) {
	if (S_ISCHR(fmode) != S_ISCHR(sb.st_mode)
	 || S_ISBLK(fmode) != S_ISBLK(sb.st_mode))
	{
	    *result |= RPMVERIFY_RDEV;
	} else if (S_ISDEV(fmode) && S_ISDEV(sb.st_mode)) {
	    if (sb.st_rdev != tfiGetFRdev(fi))
		*result |= RPMVERIFY_RDEV;
	} 
    }

    if (flags & RPMVERIFY_MTIME) {
	if (sb.st_mtime != tfiGetFMtime(fi))
	    *result |= RPMVERIFY_MTIME;
    }

    if (flags & RPMVERIFY_USER) {
	const char * name = uidToUname(sb.st_uid);
	const char * fuser = tfiGetFUser(fi);
	if (name == NULL || fuser == NULL || strcmp(name, fuser))
	    *result |= RPMVERIFY_USER;
    }

    if (flags & RPMVERIFY_GROUP) {
	const char * name = uidToUname(sb.st_gid);
	const char * fgroup = tfiGetFGroup(fi);
	if (name == NULL || fgroup == NULL || strcmp(name, fgroup))
	    *result |= RPMVERIFY_GROUP;
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
static int rpmVerifyScript(/*@unused@*/ QVA_t qva, rpmTransactionSet ts,
		TFI_t fi, /*@null@*/ FD_t scriptFd)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, fi, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    int rc = 0;
    FD_t savefd = NULL;
    PSM_t psm;

    psm = memset(alloca(sizeof(*psm)), 0, sizeof(*psm));
    psm->ts = rpmtsLink(ts, "rpmVerifyScript");

    if (scriptFd != NULL) {
	savefd = ts->scriptFd;
	ts->scriptFd = fdLink(scriptFd, "rpmVerifyScript");
    }

    psm->fi = rpmfiLink(fi, "rpmVerifyScript");
    psm->stepName = "verify";
    psm->scriptTag = RPMTAG_VERIFYSCRIPT;
    psm->progTag = RPMTAG_VERIFYSCRIPTPROG;
    rc = psmStage(psm, PSM_SCRIPT);
    psm->fi = rpmfiUnlink(fi, "rpmVerifyScript");

    if (scriptFd != NULL) {
	ts->scriptFd = fdFree(ts->scriptFd, "rpmVerifyScript");
	ts->scriptFd = savefd;
    }

    psm->ts = rpmtsUnlink(ts, "rpmVerifyScript");

    return rc;
}

int rpmVerifyDigest(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntry;	/* XXX headerGetEntryMinMemory? */
    HFD_t hfd = headerFreeData;
    void * uh = NULL;
    rpmTagType uht;
    int_32 uhc;
    const char * hdigest;
    rpmTagType hdt;
    int ec = 0;		/* assume no problems */

    /* Retrieve header digest. */
    if (!hge(h, RPMTAG_SHA1HEADER, &hdt, (void **) &hdigest, NULL)
     &&!hge(h, RPMTAG_SHA1RHN, &hdt, (void **) &hdigest, NULL))
    {
	    return 0;
    }
    /* Regenerate original header. */
    if (!hge(h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc))
	return 0;

    if (hdigest == NULL || uh == NULL)
	return 0;

    /* Compute header digest. */
    {	DIGEST_CTX ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	const char * digest;
	size_t digestlen;

	(void) rpmDigestUpdate(ctx, uh, uhc);
	(void) rpmDigestFinal(ctx, (void **)&digest, &digestlen, 1);

	/* XXX can't happen: report NULL malloc return as a digest failure. */
	ec = (digest == NULL || strcmp(hdigest, digest)) ? 1 : 0;
	digest = _free(digest);
    }

    uh = hfd(uh, uht);
    hdigest = hfd(hdigest, hdt);

    return ec;
}

/**
 * Check file info from header against what's actually installed.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param fi		file info set
 * @return		0 no problems, 1 problems found
 */
static int verifyHeader(QVA_t qva, const rpmTransactionSet ts, TFI_t fi)
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
    fi = tfiInit(fi, 0);
    if (fi != NULL)	/* XXX lclint */
    while ((i = tfiNext(fi)) >= 0) {
	rpmfileAttrs fileAttrs;
	int rc;

	fileAttrs = tfiGetFFlags(fi);

	/* If not verifying %ghost, skip ghost files. */
	if (!(qva->qva_fflags & RPMFILE_GHOST)
	&& (fileAttrs & RPMFILE_GHOST))
	    continue;

	rc = rpmVerifyFile(ts, fi, &verifyResult, omitMask);
	if (rc) {
	    if (!(fileAttrs & RPMFILE_MISSINGOK) || rpmIsVerbose()) {
		sprintf(te, _("missing    %s"), tfiGetFN(fi));
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
			tfiGetFN(fi));
	    te += strlen(te);
	}

	if (te > t) {
	    *te++ = '\n';
	    *te = '\0';
	    rpmMessage(RPMMESS_NORMAL, "%s", t);
	    te = t = buf;
	    *t = '\0';
	}
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
static int verifyDependencies(/*@unused@*/ QVA_t qva, rpmTransactionSet ts,
		Header h)
	/*@globals fileSystem, internalState @*/
	/*@modifies ts, h, fileSystem, internalState @*/
{
    rpmProblem conflicts;
    int numConflicts;
    int rc = 0;		/* assume no problems */
    int i;

    rpmtransClean(ts);
    (void) rpmtransAddPackage(ts, h, NULL, 0, NULL);

    (void) rpmdepCheck(ts, &conflicts, &numConflicts);

    /*@-branchstate@*/
    if (numConflicts) {
	const char * pkgNEVR, * altNEVR;
	rpmProblem c;
	char * t, * te;
	int nb = 512;

	for (i = 0; i < numConflicts; i++) {
	    c = conflicts + i;
	    altNEVR = (c->altNEVR ? c->altNEVR : "? ?altNEVR?");
	    nb += strlen(altNEVR+2) + sizeof(", ") - 1;
	}
	te = t = alloca(nb);
	*te = '\0';
	pkgNEVR = (conflicts->pkgNEVR ? conflicts->pkgNEVR : "?pkgNEVR?");
	sprintf(te, _("Unsatisifed dependencies for %s: "), pkgNEVR);
	te += strlen(te);
	for (i = 0; i < numConflicts; i++) {
	    c = conflicts + i;
	    altNEVR = (c->altNEVR ? c->altNEVR : "? ?altNEVR?");
	    if (i) te = stpcpy(te, ", ");
	    /* XXX FIXME: should probably supply the "[R|C] " type prefix */
	    te = stpcpy(te, altNEVR+2);
	}
	conflicts = rpmdepFreeConflicts(conflicts, numConflicts);

	if (te > t) {
	    *te++ = '\n';
	    *te = '\0';
	    rpmMessage(RPMMESS_NORMAL, "%s", t);
	    te = t;
	    *t = '\0';
	}
	rc = 1;
    }
    /*@=branchstate@*/

    rpmtransClean(ts);

    return rc;
}

int showVerifyPackage(QVA_t qva, rpmTransactionSet ts, Header h)
{
    int scareMem = 1;	/* XXX only rpmVerifyScript needs now */
    TFI_t fi;
    int ec = 0;
    int rc;

#ifdef	DYING
    if (qva->qva_flags & VERIFY_DIGEST) {
	if ((rc = rpmVerifyDigest(h)) != 0) {
	    const char *n, *v, *r;
	    (void) headerNVR(h, &n, &v, &r);
	    rpmMessage(RPMMESS_NORMAL,
		   _("%s-%s-%s: immutable header region digest check failed\n"),
			n, v, r);
	    ec = rc;
	}
    }
#endif

    fi = fiNew(ts, NULL, h, RPMTAG_BASENAMES, scareMem);
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

	fi = fiFree(fi, 1);
    }

    return ec;
}

int rpmcliVerify(rpmTransactionSet ts, QVA_t qva, const char ** argv)
{
    const char * arg;
    int ec = 0;

    if (qva->qva_showPackage == NULL)
        qva->qva_showPackage = showVerifyPackage;

    switch (qva->qva_source) {
#ifdef	DYING
    case RPMQV_RPM:
	if (!(qva->qva_flags & VERIFY_DEPS))
	    break;
	/*@fallthrough@*/
#endif
    default:
	if (rpmtsOpenDB(ts, O_RDONLY))
	    return 1;	/* XXX W2DO? */
	break;
    }

    /* XXX verifyFlags are inverted */
    ts->nodigests = (qva->qva_flags & VERIFY_DIGEST);
    ts->nosignatures = (qva->qva_flags & VERIFY_SIGNATURE);

    if (qva->qva_source == RPMQV_ALL) {
	/*@-nullpass@*/ /* FIX: argv can be NULL, cast to pass argv array */
	ec = rpmQueryVerify(qva, ts, (const char *) argv);
	/*@=nullpass@*/
    } else {
	if (argv != NULL)
	while ((arg = *argv++) != NULL) {
	    ec += rpmQueryVerify(qva, ts, arg);
	    rpmtransClean(ts);
	}
    }

    if (qva->qva_showPackage == showVerifyPackage)
        qva->qva_showPackage = NULL;

    return ec;
}
