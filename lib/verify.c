/** \ingroup rpmcli
 * \file lib/verify.c
 * Verify installed payload files from package metadata.
 */

#include "system.h"

#include "psm.h"
#include <rpmcli.h>

#include "misc.h"	/* XXX for uidToUname() and gnameToGid() */
#include "debug.h"

/*@access TFI_t*/
/*@access PSM_t*/
/*@access FD_t*/	/* XXX compared with NULL */
/*@access rpmdb*/	/* XXX compared with NULL */

#define S_ISDEV(m) (S_ISBLK((m)) || S_ISCHR((m)))

int rpmVerifyFile(const char * root, Header h, int filenum,
		rpmVerifyAttrs * result, rpmVerifyAttrs omitMask)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    int_32 * fileFlags;
    rpmfileAttrs fileAttrs = RPMFILE_NONE;
    int_32 * verifyFlags;
    rpmVerifyAttrs flags = RPMVERIFY_ALL;
    unsigned short * modeList;
    const char * fileStatesList;
    const char * filespec = NULL;
    int count;
    int rc;
    struct stat sb;

    rc = hge(h, RPMTAG_FILEMODES, NULL, (void **) &modeList, &count);
    if (hge(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlags, NULL))
	fileAttrs = fileFlags[filenum];

    if (hge(h, RPMTAG_FILEVERIFYFLAGS, NULL, (void **) &verifyFlags, NULL))
	flags = verifyFlags[filenum];

    {
	const char ** baseNames;
	const char ** dirNames;
	int_32 * dirIndexes;
	rpmTagType bnt, dnt;

	if (hge(h, RPMTAG_BASENAMES, &bnt, (void **) &baseNames, NULL)
	&&  hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, NULL)
	&&  hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL))
	{
	    int nb = (strlen(dirNames[dirIndexes[filenum]]) + 
		      strlen(baseNames[filenum]) + strlen(root) + 5);
	    char * t = alloca(nb);
	    filespec = t;
	    *t = '\0';
	    if (root && !(root[0] == '/' && root[1] == '\0')) {
		t = stpcpy(t, root);
		while (t > filespec && t[-1] == '/') {
		    --t;
		    *t = '\0';
		}
	    }
	    t = stpcpy(t, dirNames[dirIndexes[filenum]]);
	    t = stpcpy(t, baseNames[filenum]);
	}
	baseNames = hfd(baseNames, bnt);
	dirNames = hfd(dirNames, dnt);
    }

    *result = RPMVERIFY_NONE;

    /*
     * Check to see if the file was installed - if not pretend all is OK.
     */
    if (hge(h, RPMTAG_FILESTATES, NULL, (void **) &fileStatesList, NULL) &&
	fileStatesList != NULL)
    {
	rpmfileState fstate = fileStatesList[filenum];
	switch (fstate) {
	case RPMFILE_STATE_NETSHARED:
	case RPMFILE_STATE_REPLACED:
	case RPMFILE_STATE_NOTINSTALLED:
	    return 0;
	    /*@notreached@*/ break;
	case RPMFILE_STATE_NORMAL:
	    break;
	}
    }

    if (filespec == NULL) {
	*result |= RPMVERIFY_LSTATFAIL;
	return 1;
    }

    if (Lstat(filespec, &sb) != 0) {
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
	unsigned char md5sum[40];
	const char ** md5List;
	rpmTagType mdt;

	if (!hge(h, RPMTAG_FILEMD5S, &mdt, (void **) &md5List, NULL))
	    *result |= RPMVERIFY_MD5;
	else {
	    rc = mdfile(filespec, md5sum);
	    if (rc)
		*result |= (RPMVERIFY_READFAIL|RPMVERIFY_MD5);
	    else if (strcmp(md5sum, md5List[filenum]))
		*result |= RPMVERIFY_MD5;
	}
	md5List = hfd(md5List, mdt);
    } 

    if (flags & RPMVERIFY_LINKTO) {
	char linkto[1024];
	int size = 0;
	const char ** linktoList;
	rpmTagType ltt;

	if (!hge(h, RPMTAG_FILELINKTOS, &ltt, (void **) &linktoList, NULL)
	|| (size = Readlink(filespec, linkto, sizeof(linkto)-1)) == -1)
	    *result |= (RPMVERIFY_READLINKFAIL|RPMVERIFY_LINKTO);
	else {
	    linkto[size] = '\0';
	    if (strcmp(linkto, linktoList[filenum]))
		*result |= RPMVERIFY_LINKTO;
	}
	linktoList = hfd(linktoList, ltt);
    } 

    if (flags & RPMVERIFY_FILESIZE) {
	int_32 * sizeList;

	if (!hge(h, RPMTAG_FILESIZES, NULL, (void **) &sizeList, NULL)
	|| sizeList[filenum] != sb.st_size)
	    *result |= RPMVERIFY_FILESIZE;
    } 

    if (flags & RPMVERIFY_MODE) {
	unsigned short metamode = modeList[filenum];
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
	if (S_ISCHR(modeList[filenum]) != S_ISCHR(sb.st_mode) ||
	    S_ISBLK(modeList[filenum]) != S_ISBLK(sb.st_mode))
	{
	    *result |= RPMVERIFY_RDEV;
	} else if (S_ISDEV(modeList[filenum]) && S_ISDEV(sb.st_mode)) {
	    unsigned short * rdevList;
	    if (!hge(h, RPMTAG_FILERDEVS, NULL, (void **) &rdevList, NULL)
	    || rdevList[filenum] != sb.st_rdev)
		*result |= RPMVERIFY_RDEV;
	} 
    }

    if (flags & RPMVERIFY_MTIME) {
	int_32 * mtimeList;

	if (!hge(h, RPMTAG_FILEMTIMES, NULL, (void **) &mtimeList, NULL)
	||  mtimeList[filenum] != sb.st_mtime)
	    *result |= RPMVERIFY_MTIME;
    }

    if (flags & RPMVERIFY_USER) {
	const char * name;
	const char ** unameList;
	int_32 * uidList;
	rpmTagType unt;

	if (hge(h, RPMTAG_FILEUSERNAME, &unt, (void **) &unameList, NULL)) {
	    name = uidToUname(sb.st_uid);
	    if (!name || strcmp(unameList[filenum], name))
		*result |= RPMVERIFY_USER;
	    unameList = hfd(unameList, unt);
	} else if (hge(h, RPMTAG_FILEUIDS, NULL, (void **) &uidList, NULL)) {
	    if (uidList[filenum] != sb.st_uid)
		*result |= RPMVERIFY_GROUP;
	} else {
	    rpmError(RPMERR_INTERNAL, _("package lacks both user name and id "
		  "lists (this should never happen)\n"));
	    *result |= RPMVERIFY_GROUP;
	}
    }

    if (flags & RPMVERIFY_GROUP) {
	const char ** gnameList;
	int_32 * gidList;
	rpmTagType gnt;
	gid_t gid;

	if (hge(h, RPMTAG_FILEGROUPNAME, &gnt, (void **) &gnameList, NULL)) {
	    rc = gnameToGid(gnameList[filenum], &gid);
	    if (rc || (gid != sb.st_gid))
		*result |= RPMVERIFY_GROUP;
	    gnameList = hfd(gnameList, gnt);
	} else if (hge(h, RPMTAG_FILEGIDS, NULL, (void **) &gidList, NULL)) {
	    if (gidList[filenum] != sb.st_gid)
		*result |= RPMVERIFY_GROUP;
	} else {
	    rpmError(RPMERR_INTERNAL, _("package lacks both group name and id "
		     "lists (this should never happen)\n"));
	    *result |= RPMVERIFY_GROUP;
	}
    }

    return 0;
}

/**
 * Return exit code from running verify script in header.
 * @todo gnorpm/kpackage prevents static, should be using VERIFY_SCRIPT flag.
 * @param rootDir       path to top of install tree
 * @param h             header
 * @param scriptFd      file handle to use for stderr (or NULL)
 * @return              0 on success
 */
int rpmVerifyScript(const char * rootDir, Header h, /*@null@*/ FD_t scriptFd)
{
    rpmdb db = NULL;
    rpmTransactionSet ts = rpmtransCreateSet(db, rootDir);
    TFI_t fi = xcalloc(1, sizeof(*fi));
    struct psm_s psmbuf;
    PSM_t psm = &psmbuf;
    int rc;

    /*@-type@*/ /* FIX: cast? */
    if (scriptFd != NULL)
	ts->scriptFd = fdLink(scriptFd, "rpmVerifyScript");
    /*@=type@*/
    fi->magic = TFIMAGIC;
    loadFi(ts, fi, h, 1);
    memset(psm, 0, sizeof(*psm));
    psm->ts = ts;
    psm->fi = fi;
    psm->stepName = "verify";
    psm->scriptTag = RPMTAG_VERIFYSCRIPT;
    psm->progTag = RPMTAG_VERIFYSCRIPTPROG;
    rc = psmStage(psm, PSM_SCRIPT);
    freeFi(fi);
    fi = _free(fi);
    ts = rpmtransFree(ts);
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
    &&	!hge(h, RPMTAG_SHA1RHN, &hdt, (void **) &hdigest, NULL))
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
 * @param h		header
 * @return		0 no problems, 1 problems found
 */
static int verifyHeader(QVA_t qva, Header h)
	/*@globals fileSystem@*/
	/*@modifies h, fileSystem @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    char buf[BUFSIZ];
    char * t, * te;
    const char * prefix = (qva->qva_prefix ? qva->qva_prefix : "");
    const char ** fileNames = NULL;
    int count;
    int_32 * fileFlags = NULL;
    rpmVerifyAttrs verifyResult = 0;
    /*@-type@*/ /* FIX: union? */
    rpmVerifyAttrs omitMask = ((qva->qva_flags & VERIFY_ATTRS) ^ VERIFY_ATTRS);
    /*@=type@*/
    int ec = 0;		/* assume no problems */
    int i;

    te = t = buf;
    *te = '\0';

    if (!hge(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlags, NULL))
	goto exit;

    if (!headerIsEntry(h, RPMTAG_BASENAMES))
	goto exit;

    rpmBuildFileList(h, &fileNames, &count);

    for (i = 0; i < count; i++) {
	rpmfileAttrs fileAttrs;
	int rc;

	fileAttrs = fileFlags[i];

	/* If not verifying %ghost, skip ghost files. */
	if (!(qva->qva_fflags & RPMFILE_GHOST)
	&& (fileAttrs & RPMFILE_GHOST))
	    continue;

	rc = rpmVerifyFile(prefix, h, i, &verifyResult, omitMask);
	if (rc) {
	    /*@-internalglobs@*/ /* FIX: shrug */
	    if (!(fileAttrs & RPMFILE_MISSINGOK) || rpmIsVerbose()) {
		sprintf(te, _("missing    %s"), fileNames[i]);
		te += strlen(te);
		ec = rc;
	    }
	    /*@=internalglobs@*/
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
			fileNames[i]);
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
	
exit:
    fileNames = _free(fileNames);
    return ec;
}

/**
 * Check installed package dependencies for problems.
 * @param db		rpm database
 * @param h		header
 * @return		0 no problems, 1 problems found
 */
static int verifyDependencies(rpmdb db, Header h)
	/*@globals fileSystem@*/
	/*@modifies h, fileSystem @*/
{
    rpmTransactionSet ts;
    rpmDependencyConflict conflicts;
    int numConflicts;
    int rc = 0;		/* assume no problems */
    int i;

    ts = rpmtransCreateSet(db, NULL);
    (void) rpmtransAddPackage(ts, h, NULL, NULL, 0, NULL);

    (void) rpmdepCheck(ts, &conflicts, &numConflicts);
    ts = rpmtransFree(ts);

    /*@-branchstate@*/
    if (numConflicts) {
	const char *n, *v, *r;
	char * t, * te;
	int nb = 512;
	(void) headerNVR(h, &n, &v, &r);

	/*@-type@*/ /* FIX: rpmDependencyConflict usage */
	for (i = 0; i < numConflicts; i++) {
	    nb += strlen(conflicts[i].needsName) + sizeof(", ") - 1;
	    if (conflicts[i].needsFlags)
		nb += strlen(conflicts[i].needsVersion) + 5;
	}
	te = t = alloca(nb);
	*te = '\0';
	sprintf(te, _("Unsatisfied dependencies for %s-%s-%s: "), n, v, r);
	te += strlen(te);
	for (i = 0; i < numConflicts; i++) {
	    if (i) te = stpcpy(te, ", ");
	    te = stpcpy(te, conflicts[i].needsName);
	    if (conflicts[i].needsFlags) {
		int flags = conflicts[i].needsFlags;
		*te++ = ' ';
		if (flags & RPMSENSE_LESS)	*te++ = '<';
		if (flags & RPMSENSE_GREATER)	*te++ = '>';
		if (flags & RPMSENSE_EQUAL)	*te++ = '=';
		*te++ = ' ';
		te = stpcpy(te, conflicts[i].needsVersion);
	    }
	}
	conflicts = rpmdepFreeConflicts(conflicts, numConflicts);
	/*@=type@*/

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
    return rc;
}

int showVerifyPackage(QVA_t qva, rpmdb db, Header h)
{
    const char * prefix = (qva->qva_prefix ? qva->qva_prefix : "");
    int ec = 0;
    int rc;

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
    if (qva->qva_flags & VERIFY_DEPS) {
	if ((rc = verifyDependencies(db, h)) != 0)
	    ec = rc;
    }
    if (qva->qva_flags & VERIFY_FILES) {
	if ((rc = verifyHeader(qva, h)) != 0)
	    ec = rc;
    }
    if (qva->qva_flags & VERIFY_SCRIPT) {
	FD_t fdo = fdDup(STDOUT_FILENO);
	if ((rc = rpmVerifyScript(prefix, h, fdo)) != 0)
	    ec = rc;
	if (fdo)
	    rc = Fclose(fdo);
    }
    return ec;
}

int rpmVerify(QVA_t qva, rpmQVSources source, const char * arg)
{
    rpmdb db = NULL;
    int rc;

    switch (source) {
    case RPMQV_RPM:
	if (!(qva->qva_flags & VERIFY_DEPS))
	    break;
	/*@fallthrough@*/
    default:
	if ((rc = rpmdbOpen(qva->qva_prefix, &db, O_RDONLY, 0644)) != 0)
	    return 1;
	break;
    }

    rc = rpmQueryVerify(qva, source, arg, db, showVerifyPackage);

    if (db != NULL)
	(void) rpmdbClose(db);

    return rc;
}
