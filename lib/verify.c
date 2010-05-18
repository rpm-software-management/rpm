/** \ingroup rpmcli
 * \file lib/verify.c
 * Verify installed payload files from package metadata.
 */

#include "system.h"

#include <rpm/rpmcli.h>
#include <rpm/header.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmfileutil.h>

#include "lib/psm.h"
#include "lib/misc.h" 	/* uidToUname(), gnameToGid */
#include "lib/rpmte_internal.h"	/* rpmteOpen(), rpmteClose() */

#include "debug.h"

#define S_ISDEV(m) (S_ISBLK((m)) || S_ISCHR((m)))

extern int _rpmds_unspecified_epoch_noise;
extern int _cacheDependsRC;

/* If cap_compare() (Linux extension) not available, do it the hard way */
#if WITH_CAP && !defined(HAVE_CAP_COMPARE)
static int cap_compare(cap_t acap, cap_t bcap)
{
    int rc = 0;
    size_t asize = cap_size(acap);
    size_t bsize = cap_size(bcap);

    if (asize != bsize) {
	rc = 1;
    } else {
	char *abuf = xcalloc(asize, sizeof(*abuf));
	char *bbuf = xcalloc(bsize, sizeof(*bbuf));
	cap_copy_ext(abuf, acap, asize);
	cap_copy_ext(bbuf, bcap, bsize);
	rc = memcmp(abuf, bbuf, asize);
	free(abuf);
	free(bbuf);
    }
    return rc;
}
#endif
	
int rpmVerifyFile(const rpmts ts, const rpmfi fi,
		rpmVerifyAttrs * res, rpmVerifyAttrs omitMask)
{
    rpm_mode_t fmode = rpmfiFMode(fi);
    rpmfileAttrs fileAttrs = rpmfiFFlags(fi);
    rpmVerifyAttrs flags = rpmfiVFlags(fi);
    const char * fn = rpmfiFN(fi);
    struct stat sb;
    int rc;

    *res = RPMVERIFY_NONE;

    /*
     * Check to see if the file was installed - if not pretend all is OK.
     */
    switch (rpmfiFState(fi)) {
    case RPMFILE_STATE_NETSHARED:
    case RPMFILE_STATE_REPLACED:
    case RPMFILE_STATE_NOTINSTALLED:
    case RPMFILE_STATE_WRONGCOLOR:
	return 0;
	break;
    case RPMFILE_STATE_NORMAL:
	break;
    }

    if (fn == NULL || lstat(fn, &sb) != 0) {
	*res |= RPMVERIFY_LSTATFAIL;
	return 1;
    }

    /*
     * Not all attributes of non-regular files can be verified.
     */
    if (S_ISDIR(sb.st_mode))
	flags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO | RPMVERIFY_CAPS);
    else if (S_ISLNK(sb.st_mode)) {
	flags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_MODE | RPMVERIFY_CAPS);
#if CHOWN_FOLLOWS_SYMLINK
	    flags &= ~(RPMVERIFY_USER | RPMVERIFY_GROUP);
#endif
    }
    else if (S_ISFIFO(sb.st_mode))
	flags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO | RPMVERIFY_CAPS);
    else if (S_ISCHR(sb.st_mode))
	flags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO | RPMVERIFY_CAPS);
    else if (S_ISBLK(sb.st_mode))
	flags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO | RPMVERIFY_CAPS);
    else 
	flags &= ~(RPMVERIFY_LINKTO);

    /*
     * Content checks of %ghost files are meaningless.
     */
    if (fileAttrs & RPMFILE_GHOST)
	flags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO);

    /*
     * Don't verify any features in omitMask.
     */
    flags &= ~(omitMask | RPMVERIFY_FAILURES);


    if (flags & RPMVERIFY_FILEDIGEST) {
	const unsigned char *digest; 
	pgpHashAlgo algo;
	size_t diglen;

	/* XXX If --nomd5, then prelinked library sizes are not corrected. */
	if ((digest = rpmfiFDigest(fi, &algo, &diglen))) {
	    unsigned char fdigest[diglen];
	    rpm_loff_t fsize;

	    rc = rpmDoDigest(algo, fn, 0, fdigest, &fsize);
	    sb.st_size = fsize;
	    if (rc) {
		*res |= (RPMVERIFY_READFAIL|RPMVERIFY_FILEDIGEST);
	    } else if (memcmp(fdigest, digest, diglen)) {
		*res |= RPMVERIFY_FILEDIGEST;
	    }
	} else {
	    *res |= RPMVERIFY_FILEDIGEST;
	} 
    } 

    if (flags & RPMVERIFY_LINKTO) {
	char linkto[1024+1];
	int size = 0;

	if ((size = readlink(fn, linkto, sizeof(linkto)-1)) == -1)
	    *res |= (RPMVERIFY_READLINKFAIL|RPMVERIFY_LINKTO);
	else {
	    const char * flink = rpmfiFLink(fi);
	    linkto[size] = '\0';
	    if (flink == NULL || !rstreq(linkto, flink))
		*res |= RPMVERIFY_LINKTO;
	}
    } 

    if (flags & RPMVERIFY_FILESIZE) {
	if (sb.st_size != rpmfiFSize(fi))
	    *res |= RPMVERIFY_FILESIZE;
    } 

    if (flags & RPMVERIFY_MODE) {
	rpm_mode_t metamode = fmode;
	rpm_mode_t filemode;

	/*
	 * Platforms (like AIX) where sizeof(rpm_mode_t) != sizeof(mode_t)
	 * need the (rpm_mode_t) cast here. 
	 */
	filemode = (rpm_mode_t)sb.st_mode;

	/*
	 * Comparing the type of %ghost files is meaningless, but perms are OK.
	 */
	if (fileAttrs & RPMFILE_GHOST) {
	    metamode &= ~0xf000;
	    filemode &= ~0xf000;
	}

	if (metamode != filemode)
	    *res |= RPMVERIFY_MODE;

#if WITH_ACL
	/*
	 * For now, any non-default acl's on a file is a difference as rpm
	 * cannot have set them.
	 */
	acl_t facl = acl_get_file(fn, ACL_TYPE_ACCESS);
	if (facl) {
	    if (acl_equiv_mode(facl, NULL) == 1) {
		*res |= RPMVERIFY_MODE;
	    }
	    acl_free(facl);
	}
#endif 
    }

    if (flags & RPMVERIFY_RDEV) {
	if (S_ISCHR(fmode) != S_ISCHR(sb.st_mode)
	 || S_ISBLK(fmode) != S_ISBLK(sb.st_mode))
	{
	    *res |= RPMVERIFY_RDEV;
	} else if (S_ISDEV(fmode) && S_ISDEV(sb.st_mode)) {
	    rpm_rdev_t st_rdev = (sb.st_rdev & 0xffff);
	    rpm_rdev_t frdev = (rpmfiFRdev(fi) & 0xffff);
	    if (st_rdev != frdev)
		*res |= RPMVERIFY_RDEV;
	} 
    }

#if WITH_CAP
    if (flags & RPMVERIFY_CAPS) {
	/*
 	 * Empty capability set ("=") is not exactly the same as no
 	 * capabilities at all but suffices for now... 
 	 */
	cap_t cap, fcap;
	cap = cap_from_text(rpmfiFCaps(fi));
	if (!cap) {
	    cap = cap_from_text("=");
	}
	fcap = cap_get_file(fn);
	if (!fcap) {
	    fcap = cap_from_text("=");
	}
	
	if (cap_compare(cap, fcap) != 0)
	    *res |= RPMVERIFY_CAPS;

	cap_free(fcap);
	cap_free(cap);
    }
#endif

    if ((flags & RPMVERIFY_MTIME) && (sb.st_mtime != rpmfiFMtime(fi))) {
	/* Filter out timestamp differences of shared files */
	rpmdbMatchIterator mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, fn, 0);
	if (rpmdbGetIteratorCount(mi) < 2) 
	    *res |= RPMVERIFY_MTIME;
	rpmdbFreeIterator(mi);
    }

    if (flags & RPMVERIFY_USER) {
	const char * name = uidToUname(sb.st_uid);
	const char * fuser = rpmfiFUser(fi);
	if (name == NULL || fuser == NULL || !rstreq(name, fuser))
	    *res |= RPMVERIFY_USER;
    }

    if (flags & RPMVERIFY_GROUP) {
	const char * name = gidToGname(sb.st_gid);
	const char * fgroup = rpmfiFGroup(fi);
	if (name == NULL || fgroup == NULL || !rstreq(name, fgroup))
	    *res |= RPMVERIFY_GROUP;
    }

    return 0;
}

/**
 * Return exit code from running verify script from header.
 * @todo malloc/free/refcount handling is fishy here.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param h		header
 * @param scriptFd      file handle to use for stderr (or NULL)
 * @return              0 on success
 */
static int rpmVerifyScript(QVA_t qva, rpmts ts, Header h, FD_t scriptFd)
{
    rpmpsm psm = NULL;
    rpmte te = NULL;
    int rc = 0;

    /* fake up a erasure transaction element */
    rc = rpmtsAddEraseElement(ts, h, -1);
    te = rpmtsElement(ts, 0);
    rpmteOpen(te, ts, 0);
    
    if (scriptFd != NULL)
	rpmtsSetScriptFd(ts, scriptFd);

    /* create psm to run the script */
    psm = rpmpsmNew(ts, te);
    rpmpsmScriptStage(psm, RPMTAG_VERIFYSCRIPT, RPMTAG_VERIFYSCRIPTPROG);
    rc = rpmpsmStage(psm, PSM_SCRIPT);
    psm = rpmpsmFree(psm);

    if (scriptFd != NULL)
	rpmtsSetScriptFd(ts, NULL);

    /* clean up our fake transaction bits */
    rpmteClose(te, ts, 0);
    rpmtsEmpty(ts);

    return rc;
}

/**
 * Check file info from header against what's actually installed.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param h		header to verify
 * @return		0 no problems, 1 problems found
 */
static int verifyHeader(QVA_t qva, const rpmts ts, Header h)
{
    rpmVerifyAttrs verifyResult = 0;
    /* FIX: union? */
    rpmVerifyAttrs omitMask = ((qva->qva_flags & VERIFY_ATTRS) ^ VERIFY_ATTRS);
    int ec = 0;		/* assume no problems */
    char *buf = NULL;

    rpmfi fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, RPMFI_FLAGS_VERIFY);
    rpmfiInit(fi, 0);
    while (rpmfiNext(fi) >= 0) {
	rpmfileAttrs fileAttrs;
	int rc;

	fileAttrs = rpmfiFFlags(fi);

	/* If not verifying %ghost, skip ghost files. */
	if (!(qva->qva_fflags & RPMFILE_GHOST)
	&& (fileAttrs & RPMFILE_GHOST))
	    continue;

	rc = rpmVerifyFile(ts, fi, &verifyResult, omitMask);
	if (rc) {
	    if (!(fileAttrs & (RPMFILE_MISSINGOK|RPMFILE_GHOST)) || rpmIsVerbose()) {
		rasprintf(&buf, _("missing   %c %s"),
			((fileAttrs & RPMFILE_CONFIG)	? 'c' :
			 (fileAttrs & RPMFILE_DOC)	? 'd' :
			 (fileAttrs & RPMFILE_GHOST)	? 'g' :
			 (fileAttrs & RPMFILE_LICENSE)	? 'l' :
			 (fileAttrs & RPMFILE_PUBKEY)	? 'P' :
			 (fileAttrs & RPMFILE_README)	? 'r' : ' '), 
			rpmfiFN(fi));
		if ((verifyResult & RPMVERIFY_LSTATFAIL) != 0 &&
		    errno != ENOENT) {
		    char *app;
		    rasprintf(&app, " (%s)", strerror(errno));
		    rstrcat(&buf, app);
		    free(app);
		}
		ec = rc;
	    }
	} else if (verifyResult || rpmIsVerbose()) {
	    const char * size, * filedigest, * link, * mtime, * mode;
	    const char * group, * user, * rdev, *caps;
	    static const char *const aok = ".";
	    static const char *const unknown = "?";

	    if (verifyResult) ec = 1;

#define	_verify(_RPMVERIFY_F, _C)	\
	((verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifylink(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READLINKFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifyfile(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
	
	    filedigest = _verifyfile(RPMVERIFY_FILEDIGEST, "5");
	    size = _verify(RPMVERIFY_FILESIZE, "S");
	    link = _verifylink(RPMVERIFY_LINKTO, "L");
	    mtime = _verify(RPMVERIFY_MTIME, "T");
	    rdev = _verify(RPMVERIFY_RDEV, "D");
	    user = _verify(RPMVERIFY_USER, "U");
	    group = _verify(RPMVERIFY_GROUP, "G");
	    mode = _verify(RPMVERIFY_MODE, "M");
	    caps = _verify(RPMVERIFY_CAPS, "P");

#undef _verifyfile
#undef _verifylink
#undef _verify

	    rasprintf(&buf, "%s%s%s%s%s%s%s%s%s  %c %s",
			size, mode, filedigest, rdev, link, user, group, mtime, caps,
			((fileAttrs & RPMFILE_CONFIG)	? 'c' :
			 (fileAttrs & RPMFILE_DOC)	? 'd' :
			 (fileAttrs & RPMFILE_GHOST)	? 'g' :
			 (fileAttrs & RPMFILE_LICENSE)	? 'l' :
			 (fileAttrs & RPMFILE_PUBKEY)	? 'P' :
			 (fileAttrs & RPMFILE_README)	? 'r' : ' '), 
			rpmfiFN(fi));
	}

	if (buf) {
	    rpmlog(RPMLOG_NOTICE, "%s\n", buf);
	    buf = _free(buf);
	}
    }
    rpmfiFree(fi);
	
    return ec;
}

/**
 * Check installed package dependencies for problems.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param h		header
 * @return		number of problems found (0 for no problems)
 */
static int verifyDependencies(QVA_t qva, rpmts ts,
		Header h)
{
    rpmps ps;
    rpmpsi psi;
    int rc = 0;		/* assume no problems */
    int xx;

    rpmtsEmpty(ts);
    (void) rpmtsAddInstallElement(ts, h, NULL, 0, NULL);

    xx = rpmtsCheck(ts);
    ps = rpmtsProblems(ts);

    psi = rpmpsInitIterator(ps);
    if (rpmpsNumProblems(ps) > 0) {
	char *nevra = headerGetAsString(h, RPMTAG_NEVRA);
	rpmlog(RPMLOG_NOTICE, _("Unsatisfied dependencies for %s:\n"), nevra);
	free(nevra);
	while (rpmpsNextIterator(psi) >= 0) {
	    rpmProblem p = rpmpsGetProblem(psi);
	    char * ps = rpmProblemString(p);
	    rpmlog(RPMLOG_NOTICE, "\t%s\n", ps);
	    free(ps);
	    rc++;	
	}
    }
    psi = rpmpsFreeIterator(psi);
    ps = rpmpsFree(ps);

    rpmtsEmpty(ts);

    return rc;
}

int showVerifyPackage(QVA_t qva, rpmts ts, Header h)
{
    int ec = 0;
    int rc;

    if (qva->qva_flags & VERIFY_DEPS) {
	if ((rc = verifyDependencies(qva, ts, h)) != 0)
	    ec = rc;
    }
    if (qva->qva_flags & VERIFY_FILES) {
	if ((rc = verifyHeader(qva, ts, h)) != 0)
	    ec = rc;
    }
    if ((qva->qva_flags & VERIFY_SCRIPT)
     && headerIsEntry(h, RPMTAG_VERIFYSCRIPT))
    {
	FD_t fdo = fdDup(STDOUT_FILENO);
	if ((rc = rpmVerifyScript(qva, ts, h, fdo)) != 0)
	    ec = rc;
	if (fdo != NULL)
	    rc = Fclose(fdo);
    }

    return ec;
}

int rpmcliVerify(rpmts ts, QVA_t qva, char * const * argv)
{
    rpmVSFlags vsflags, ovsflags;
    int ec = 0, xx;
    int dirfd = -1;
    const char * rootDir = rpmtsRootDir(ts);
    FD_t scriptFd = fdDup(STDOUT_FILENO);

    /* 
     * Open the DB + indices explicitly before possible chroot,
     * otherwises BDB is going to be unhappy...
     */
    rpmtsOpenDB(ts, O_RDONLY);
    rpmdbOpenAll(rpmtsGetRdb(ts));
    if (rootDir && !rstreq(rootDir, "/")) {
	dirfd = open(".", O_RDONLY);
	if (dirfd == -1 || chdir("/") == -1 || chroot(rootDir) == -1) {
	    rpmlog(RPMLOG_ERR, _("Unable to change root directory: %m\n"));
	    ec = 1;
	    goto exit;
	} else {
	    rpmtsSetChrootDone(ts, 1);
	}
    }

    if (qva->qva_showPackage == NULL)
        qva->qva_showPackage = showVerifyPackage;

    /* XXX verify flags are inverted from query. */
    vsflags = rpmExpandNumeric("%{?_vsflags_verify}");
    if (!(qva->qva_flags & VERIFY_DIGEST))
	vsflags |= _RPMVSF_NODIGESTS;
    if (!(qva->qva_flags & VERIFY_SIGNATURE))
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (!(qva->qva_flags & VERIFY_HDRCHK))
	vsflags |= RPMVSF_NOHDRCHK;
    vsflags &= ~RPMVSF_NEEDPAYLOAD;

    ovsflags = rpmtsSetVSFlags(ts, vsflags);
    ec = rpmcliArgIter(ts, qva, argv);
    vsflags = rpmtsSetVSFlags(ts, ovsflags);

    if (qva->qva_showPackage == showVerifyPackage)
        qva->qva_showPackage = NULL;

    rpmtsEmpty(ts);

    if (rpmtsChrootDone(ts)) {
	/* only done if previous chroot succeeded, assume success */
	xx = chroot(".");
	xx = fchdir(dirfd);
	rpmtsSetChrootDone(ts, 0);
    }

exit:
    Fclose(scriptFd);
    if (dirfd >= 0) close(dirfd);

    return ec;
}
