/** \ingroup rpmcli
 * \file lib/verify.c
 * Verify installed payload files from package metadata.
 */

#include "system.h"

#include <errno.h>
#if WITH_CAP
#include <sys/capability.h>
#endif
#if WITH_ACL
#include <acl/libacl.h>
#endif

#include <rpm/rpmcli.h>
#include <rpm/header.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmfileutil.h>

#include "lib/misc.h"
#include "lib/rpmchroot.h"
#include "lib/rpmte_internal.h"	/* rpmteProcess() */
#include "lib/rpmug.h"

#include "debug.h"

#define S_ISDEV(m) (S_ISBLK((m)) || S_ISCHR((m)))

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
	
rpmVerifyAttrs rpmfilesVerify(rpmfiles fi, int ix, rpmVerifyAttrs omitMask)
{
    rpm_mode_t fmode = rpmfilesFMode(fi, ix);
    rpmfileAttrs fileAttrs = rpmfilesFFlags(fi, ix);
    rpmVerifyAttrs flags = rpmfilesVFlags(fi, ix);
    const char * fn = rpmfilesFN(fi, ix);
    struct stat sb;
    rpmVerifyAttrs vfy = RPMVERIFY_NONE;

    /*
     * Check to see if the file was installed - if not pretend all is OK.
     */
    switch (rpmfilesFState(fi, ix)) {
    case RPMFILE_STATE_NETSHARED:
    case RPMFILE_STATE_NOTINSTALLED:
	goto exit;
	break;
    case RPMFILE_STATE_REPLACED:
	/* For replaced files we can only verify if it exists at all */
	flags = RPMVERIFY_LSTATFAIL;
	break;
    case RPMFILE_STATE_WRONGCOLOR:
	/*
	 * Files with wrong color are supposed to share some attributes
	 * with the actually installed file - verify what we can.
	 */
	flags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE |
		   RPMVERIFY_MTIME | RPMVERIFY_RDEV);
	break;
    case RPMFILE_STATE_NORMAL:
    /* File from a non-installed package, try to verify nevertheless */
    case RPMFILE_STATE_MISSING:
	break;
    }

    if (fn == NULL || lstat(fn, &sb) != 0) {
	vfy |= RPMVERIFY_LSTATFAIL;
	goto exit;
    }

    /* If we expected a directory but got a symlink to one, follow the link */
    if (S_ISDIR(fmode) && S_ISLNK(sb.st_mode)) {
	struct stat dsb;
	/* ...if it actually points to a directory  */
	if (stat(fn, &dsb) == 0 && S_ISDIR(dsb.st_mode)) {
	    uid_t fuid;
	    /* ...and is by a legit user, to match fsmVerify() behavior */
	    if (sb.st_uid == 0 ||
			(rpmugUid(rpmfilesFUser(fi, ix), &fuid) == 0 &&
			 sb.st_uid == fuid)) {
		sb = dsb; /* struct assignment */
	    }
	}
    }

    /* Links have no mode, other types have no linkto */
    if (S_ISLNK(sb.st_mode))
	flags &= ~(RPMVERIFY_MODE);
    else
	flags &= ~(RPMVERIFY_LINKTO);

    /* Not all attributes of non-regular files can be verified */
    if (!S_ISREG(sb.st_mode))
	flags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE |
		   RPMVERIFY_MTIME | RPMVERIFY_CAPS);

    /* Content checks of %ghost files are meaningless. */
    if (fileAttrs & RPMFILE_GHOST)
	flags &= ~(RPMVERIFY_FILEDIGEST | RPMVERIFY_FILESIZE |
		   RPMVERIFY_MTIME | RPMVERIFY_LINKTO);

    /* Don't verify any features in omitMask. */
    flags &= ~(omitMask | RPMVERIFY_FAILURES);


    if (flags & RPMVERIFY_FILEDIGEST) {
	const unsigned char *digest; 
	int algo;
	size_t diglen;

	/* XXX If --nomd5, then prelinked library sizes are not corrected. */
	if ((digest = rpmfilesFDigest(fi, ix, &algo, &diglen))) {
	    unsigned char fdigest[diglen];
	    rpm_loff_t fsize;

	    if (rpmDoDigest(algo, fn, 0, fdigest, &fsize)) {
		vfy |= (RPMVERIFY_READFAIL|RPMVERIFY_FILEDIGEST);
	    } else {
		sb.st_size = fsize;
		if (memcmp(fdigest, digest, diglen))
		    vfy |= RPMVERIFY_FILEDIGEST;
	    }
	} else {
	    vfy |= RPMVERIFY_FILEDIGEST;
	} 
    } 

    if (flags & RPMVERIFY_LINKTO) {
	char linkto[1024+1];
	int size = 0;

	if ((size = readlink(fn, linkto, sizeof(linkto)-1)) == -1)
	    vfy |= (RPMVERIFY_READLINKFAIL|RPMVERIFY_LINKTO);
	else {
	    const char * flink = rpmfilesFLink(fi, ix);
	    linkto[size] = '\0';
	    if (flink == NULL || !rstreq(linkto, flink))
		vfy |= RPMVERIFY_LINKTO;
	}
    } 

    if (flags & RPMVERIFY_FILESIZE) {
	if (sb.st_size != rpmfilesFSize(fi, ix))
	    vfy |= RPMVERIFY_FILESIZE;
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
	    vfy |= RPMVERIFY_MODE;

#if WITH_ACL
	/*
	 * For now, any non-default acl's on a file is a difference as rpm
	 * cannot have set them.
	 */
	acl_t facl = acl_get_file(fn, ACL_TYPE_ACCESS);
	if (facl) {
	    if (acl_equiv_mode(facl, NULL) == 1) {
		vfy |= RPMVERIFY_MODE;
	    }
	    acl_free(facl);
	}
#endif 
    }

    if (flags & RPMVERIFY_RDEV) {
	if (S_ISCHR(fmode) != S_ISCHR(sb.st_mode)
	 || S_ISBLK(fmode) != S_ISBLK(sb.st_mode))
	{
	    vfy |= RPMVERIFY_RDEV;
	} else if (S_ISDEV(fmode) && S_ISDEV(sb.st_mode)) {
	    rpm_rdev_t st_rdev = (sb.st_rdev & 0xffff);
	    rpm_rdev_t frdev = (rpmfilesFRdev(fi, ix) & 0xffff);
	    if (st_rdev != frdev)
		vfy |= RPMVERIFY_RDEV;
	} 
    }

#if WITH_CAP
    if (flags & RPMVERIFY_CAPS) {
	/*
 	 * Empty capability set ("=") is not exactly the same as no
 	 * capabilities at all but suffices for now... 
 	 */
	cap_t cap, fcap;
	cap = cap_from_text(rpmfilesFCaps(fi, ix));
	if (!cap) {
	    cap = cap_from_text("=");
	}
	fcap = cap_get_file(fn);
	if (!fcap) {
	    fcap = cap_from_text("=");
	}
	
	if (cap_compare(cap, fcap) != 0)
	    vfy |= RPMVERIFY_CAPS;

	cap_free(fcap);
	cap_free(cap);
    }
#endif

    if ((flags & RPMVERIFY_MTIME) && (sb.st_mtime != rpmfilesFMtime(fi, ix))) {
	vfy |= RPMVERIFY_MTIME;
    }

    if (flags & RPMVERIFY_USER) {
	const char * name = rpmugUname(sb.st_uid);
	const char * fuser = rpmfilesFUser(fi, ix);
	uid_t uid;
	int namematch = 0;
	int idmatch = 0;

	if (name && fuser)
	   namematch =  rstreq(name, fuser);
	if (fuser && rpmugUid(fuser, &uid) == 0)
	    idmatch = (uid == sb.st_uid);

	if (namematch != idmatch) {
	    rpmlog(RPMLOG_WARNING,
		    _("Duplicate username or UID for user %s\n"), fuser);
	}

	if (!(namematch || idmatch))
	    vfy |= RPMVERIFY_USER;
    }

    if (flags & RPMVERIFY_GROUP) {
	const char * name = rpmugGname(sb.st_gid);
	const char * fgroup = rpmfilesFGroup(fi, ix);
	gid_t gid;
	int namematch = 0;
	int idmatch = 0;

	if (name && fgroup)
	    namematch = rstreq(name, fgroup);
	if (fgroup && rpmugGid(fgroup, &gid) == 0)
	    idmatch = (gid == sb.st_gid);

	if (namematch != idmatch) {
	    rpmlog(RPMLOG_WARNING,
		    _("Duplicate groupname or GID for group %s\n"), fgroup);
	}

	if (!(namematch || idmatch))
	    vfy |= RPMVERIFY_GROUP;
    }

exit:
    return vfy;
}

int rpmVerifyFile(const rpmts ts, const rpmfi fi,
		rpmVerifyAttrs * res, rpmVerifyAttrs omitMask)
{
    rpmVerifyAttrs vfy = rpmfiVerify(fi, omitMask);
    if (res)
	*res = vfy;

    return (vfy & RPMVERIFY_LSTATFAIL) ? 1 : 0;
}

/**
 * Return exit code from running verify script from header.
 * @param ts		transaction set
 * @param h		header
 * @return              0 on success
 */
static int rpmVerifyScript(rpmts ts, Header h)
{
    int rc = 0;

    if (headerIsEntry(h, RPMTAG_VERIFYSCRIPT)) {
	/* fake up a erasure transaction element */
	rpmte p = rpmteNew(ts, h, TR_REMOVED, NULL, NULL);

	if (p != NULL) {
	    rpmteSetHeader(p, h);

	    rc = (rpmpsmRun(ts, p, PKG_VERIFY) != RPMRC_OK);

	    /* clean up our fake transaction bits */
	    rpmteFree(p);
	} else {
	    rc = RPMRC_FAIL;
	}
    }

    return rc;
}

#define unknown "?"
#define	_verify(_RPMVERIFY_F, _C, _pad)	\
	((verifyResult & _RPMVERIFY_F) ? _C : _pad)
#define	_verifylink(_RPMVERIFY_F, _C, _pad)	\
	((verifyResult & RPMVERIFY_READLINKFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : _pad)
#define	_verifyfile(_RPMVERIFY_F, _C, _pad)	\
	((verifyResult & RPMVERIFY_READFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : _pad)
char * rpmVerifyString(uint32_t verifyResult, const char *pad)
{
    char *fmt = NULL;
    rasprintf(&fmt, "%s%s%s%s%s%s%s%s%s",
		_verify(RPMVERIFY_FILESIZE, "S", pad),
		_verify(RPMVERIFY_MODE, "M", pad),
		_verifyfile(RPMVERIFY_FILEDIGEST, "5", pad),
		_verify(RPMVERIFY_RDEV, "D", pad),
		_verifylink(RPMVERIFY_LINKTO, "L", pad),
		_verify(RPMVERIFY_USER, "U", pad),
		_verify(RPMVERIFY_GROUP, "G", pad),
		_verify(RPMVERIFY_MTIME, "T", pad),
		_verify(RPMVERIFY_CAPS, "P", pad));
		
    return fmt;
}
#undef _verifyfile
#undef _verifylink
#undef _verify
#undef aok
#undef unknown

char * rpmFFlagsString(uint32_t fflags, const char *pad)
{
    char *fmt = NULL;
    rasprintf(&fmt, "%s%s%s%s%s%s%s%s",
		(fflags & RPMFILE_DOC) ? "d" : pad,
		(fflags & RPMFILE_CONFIG) ? "c" : pad,
		(fflags & RPMFILE_SPECFILE) ? "s" : pad,
		(fflags & RPMFILE_MISSINGOK) ? "m" : pad,
		(fflags & RPMFILE_NOREPLACE) ? "n" : pad,
		(fflags & RPMFILE_GHOST) ? "g" : pad,
		(fflags & RPMFILE_LICENSE) ? "l" : pad,
		(fflags & RPMFILE_README) ? "r" : pad);
    return fmt;
}

static const char * stateStr(rpmfileState fstate)
{
    switch (fstate) {
    case RPMFILE_STATE_NORMAL:
	return NULL;
    case RPMFILE_STATE_NOTINSTALLED:
	return rpmIsVerbose() ? _("not installed") : NULL;
    case RPMFILE_STATE_NETSHARED:
	return rpmIsVerbose() ? _("net shared") : NULL;
    case RPMFILE_STATE_WRONGCOLOR:
	return rpmIsVerbose() ? _("wrong color") : NULL;
    case RPMFILE_STATE_REPLACED:
	return _("replaced");
    case RPMFILE_STATE_MISSING:
	return _("no state");
    }
    return _("unknown state");
}

/**
 * Check file info from header against what's actually installed.
 * @param ts		transaction set
 * @param h		header to verify
 * @param omitMask	bits to disable verify checks
 * @param skipAttr	skip files with these attrs (eg %ghost)
 * @return		0 no problems, 1 problems found
 */
static int verifyHeader(rpmts ts, Header h, rpmVerifyAttrs omitMask,
			rpmfileAttrs skipAttrs)
{
    rpmVerifyAttrs verifyResult = 0;
    rpmVerifyAttrs verifyAll = 0; /* assume no problems */
    rpmfi fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, RPMFI_FLAGS_VERIFY);

    if (fi == NULL)
	return 1;

    rpmfiInit(fi, 0);
    while (rpmfiNext(fi) >= 0) {
	rpmfileAttrs fileAttrs = rpmfiFFlags(fi);
	char *buf = NULL, *attrFormat;
	const char *fstate = NULL;
	char ac;

	/* Skip on attributes (eg from --noghost) */
	if (skipAttrs & fileAttrs)
	    continue;

	verifyResult = rpmfiVerify(fi, omitMask);

	/* Filter out timestamp differences of shared files */
	if (verifyResult & RPMVERIFY_MTIME) {
	    rpmdbMatchIterator mi;
	    mi = rpmtsInitIterator(ts, RPMDBI_BASENAMES, rpmfiFN(fi), 0);
	    if (rpmdbGetIteratorCount(mi) > 1) 
		verifyResult &= ~RPMVERIFY_MTIME;
	    rpmdbFreeIterator(mi);
	}

	/* State is only meaningful for installed packages */
	if (headerGetInstance(h))
	    fstate = stateStr(rpmfiFState(fi));

	attrFormat = rpmFFlagsString(fileAttrs, "");
	ac = rstreq(attrFormat, "") ? ' ' : attrFormat[0];
	if (verifyResult & RPMVERIFY_LSTATFAIL) {
	    if (!(fileAttrs & (RPMFILE_MISSINGOK|RPMFILE_GHOST)) || rpmIsVerbose()) {
		rasprintf(&buf, _("missing   %c %s"), ac, rpmfiFN(fi));
		if ((verifyResult & RPMVERIFY_LSTATFAIL) != 0 &&
		    errno != ENOENT) {
		    char *app;
		    rasprintf(&app, " (%s)", strerror(errno));
		    rstrcat(&buf, app);
		    free(app);
		}
	    }
	} else if (verifyResult || fstate || rpmIsVerbose()) {
	    char *verifyFormat = rpmVerifyString(verifyResult, ".");
	    rasprintf(&buf, "%s  %c %s", verifyFormat, ac, rpmfiFN(fi));
	    free(verifyFormat);
	}
	free(attrFormat);

	if (buf) {
	    if (fstate)
		buf = rstrscat(&buf, " (", fstate, ")", NULL);
	    rpmlog(RPMLOG_NOTICE, "%s\n", buf);
	    buf = _free(buf);
	}
	verifyAll |= verifyResult;
    }
    rpmfiFree(fi);
	
    return (verifyAll != 0) ? 1 : 0;
}

/**
 * Check installed package dependencies for problems.
 * @param ts		transaction set
 * @param h		header
 * @return		number of problems found (0 for no problems)
 */
static int verifyDependencies(rpmts ts, Header h)
{
    rpmps ps;
    rpmte te;
    int rc;

    rpmtsEmpty(ts);
    (void) rpmtsAddInstallElement(ts, h, NULL, 0, NULL);

    (void) rpmtsCheck(ts);
    te = rpmtsElement(ts, 0);
    ps = rpmteProblems(te);
    rc = rpmpsNumProblems(ps);

    if (rc > 0) {
	rpmlog(RPMLOG_NOTICE, _("Unsatisfied dependencies for %s:\n"),
	       rpmteNEVRA(te));
	rpmpsi psi = rpmpsInitIterator(ps);
	rpmProblem p;

	while ((p = rpmpsiNext(psi)) != NULL) {
	    char * ps = rpmProblemString(p);
	    rpmlog(RPMLOG_NOTICE, "\t%s\n", ps);
	    free(ps);
	}
	rpmpsFreeIterator(psi);
    }
    rpmpsFree(ps);
    rpmtsEmpty(ts);

    return rc;
}

int showVerifyPackage(QVA_t qva, rpmts ts, Header h)
{
    rpmVerifyAttrs omitMask = ((qva->qva_flags & VERIFY_ATTRS) ^ VERIFY_ATTRS);
    int ec = 0;
    int rc;

    if (qva->qva_flags & VERIFY_DEPS) {
	if ((rc = verifyDependencies(ts, h)) != 0)
	    ec = rc;
    }
    if (qva->qva_flags & VERIFY_FILES) {
	if ((rc = verifyHeader(ts, h, omitMask, qva->qva_fflags)) != 0)
	    ec = rc;
    }
    if (qva->qva_flags & VERIFY_SCRIPT) {
	if ((rc = rpmVerifyScript(ts, h)) != 0)
	    ec = rc;
    }

    return ec;
}

int rpmcliVerify(rpmts ts, QVA_t qva, char * const * argv)
{
    rpmVSFlags vsflags, ovsflags;
    int ec = 0;
    FD_t scriptFd = fdDup(STDOUT_FILENO);

    /* 
     * Open the DB + indices explicitly before possible chroot,
     * otherwises BDB is going to be unhappy...
     */
    rpmtsOpenDB(ts, O_RDONLY);
    rpmdbOpenAll(rpmtsGetRdb(ts));
    if (rpmChrootSet(rpmtsRootDir(ts)) || rpmChrootIn()) {
	ec = 1;
	goto exit;
    }

    if (qva->qva_showPackage == NULL)
        qva->qva_showPackage = showVerifyPackage;

    vsflags = rpmExpandNumeric("%{?_vsflags_verify}");
    if (rpmcliQueryFlags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (rpmcliQueryFlags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (rpmcliQueryFlags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    vsflags &= ~RPMVSF_NEEDPAYLOAD;

    rpmtsSetScriptFd(ts, scriptFd);
    ovsflags = rpmtsSetVSFlags(ts, vsflags);
    ec = rpmcliArgIter(ts, qva, argv);
    rpmtsSetVSFlags(ts, ovsflags);
    rpmtsSetScriptFd(ts, NULL);

    if (qva->qva_showPackage == showVerifyPackage)
        qva->qva_showPackage = NULL;

    rpmtsEmpty(ts);

    if (rpmChrootOut() || rpmChrootSet(NULL))
	ec = 1;

exit:
    Fclose(scriptFd);

    return ec;
}
