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
    case RPMFILE_STATE_NOTINSTALLED:
	return 0;
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
	*res |= RPMVERIFY_LSTATFAIL;
	return 1;
    }

    /* If we expected a directory but got a symlink to one, follow the link */
    if (S_ISDIR(fmode) && S_ISLNK(sb.st_mode) && stat(fn, &sb) != 0) {
	*res |= RPMVERIFY_LSTATFAIL;
	return 1;
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
	*res |= RPMVERIFY_MTIME;
    }

    if (flags & RPMVERIFY_USER) {
	const char * name = rpmugUname(sb.st_uid);
	const char * fuser = rpmfiFUser(fi);
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
	    *res |= RPMVERIFY_USER;
    }

    if (flags & RPMVERIFY_GROUP) {
	const char * name = rpmugGname(sb.st_gid);
	const char * fgroup = rpmfiFGroup(fi);
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
	    *res |= RPMVERIFY_GROUP;
    }

    return 0;
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
 * @param ghosts	should ghosts be verified?
 * @return		0 no problems, 1 problems found
 */
static int verifyHeader(rpmts ts, Header h, rpmVerifyAttrs omitMask, int ghosts)
{
    rpmVerifyAttrs verifyResult = 0;
    int ec = 0;		/* assume no problems */
    rpmfi fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, RPMFI_FLAGS_VERIFY);

    if (fi == NULL)
	return 1;

    rpmfiInit(fi, 0);
    while (rpmfiNext(fi) >= 0) {
	rpmfileAttrs fileAttrs = rpmfiFFlags(fi);
	char *buf = NULL, *attrFormat;
	const char *fstate = NULL;
	char ac;
	int rc;

	/* If not verifying %ghost, skip ghost files. */
	if ((fileAttrs & RPMFILE_GHOST) && !ghosts)
	    continue;

	rc = rpmVerifyFile(ts, fi, &verifyResult, omitMask);

	/* Filter out timestamp differences of shared files */
	if (rc == 0 && (verifyResult & RPMVERIFY_MTIME)) {
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
	if (rc) {
	    if (!(fileAttrs & (RPMFILE_MISSINGOK|RPMFILE_GHOST)) || rpmIsVerbose()) {
		rasprintf(&buf, _("missing   %c %s"), ac, rpmfiFN(fi));
		if ((verifyResult & RPMVERIFY_LSTATFAIL) != 0 &&
		    errno != ENOENT) {
		    char *app;
		    rasprintf(&app, " (%s)", strerror(errno));
		    rstrcat(&buf, app);
		    free(app);
		}
		ec = rc;
	    }
	} else if (verifyResult || fstate || rpmIsVerbose()) {
	    char *verifyFormat = rpmVerifyString(verifyResult, ".");
	    rasprintf(&buf, "%s  %c %s", verifyFormat, ac, rpmfiFN(fi));
	    free(verifyFormat);

	    if (verifyResult) ec = 1;
	}
	free(attrFormat);

	if (buf) {
	    if (fstate)
		buf = rstrscat(&buf, " (", fstate, ")", NULL);
	    rpmlog(RPMLOG_NOTICE, "%s\n", buf);
	    buf = _free(buf);
	}
    }
    rpmfiFree(fi);
	
    return ec;
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
    int ghosts = (qva->qva_fflags & RPMFILE_GHOST);
    int ec = 0;
    int rc;

    if (qva->qva_flags & VERIFY_DEPS) {
	if ((rc = verifyDependencies(ts, h)) != 0)
	    ec = rc;
    }
    if (qva->qva_flags & VERIFY_FILES) {
	if ((rc = verifyHeader(ts, h, omitMask, ghosts)) != 0)
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
