/** \ingroup rpmcli
 * \file lib/verify.c
 */

#include "system.h"

#include <rpmlib.h>

#include "md5.h"
#include "misc.h"
#include "install.h"

#include "build/rpmbuild.h"
#include <rpmurl.h>

static int _ie = 0x44332211;
static union _vendian { int i; char b[4]; } *_endian = (union _vendian *)&_ie;
#define	IS_BIG_ENDIAN()		(_endian->b[0] == '\x44')
#define	IS_LITTLE_ENDIAN()	(_endian->b[0] == '\x11')

#define S_ISDEV(m) (S_ISBLK((m)) || S_ISCHR((m)))

#define	POPT_NOFILES	1000

/* ========== Verify specific popt args */
static void verifyArgCallback(/*@unused@*/poptContext con,
	/*@unused@*/enum poptCallbackReason reason,
	const struct poptOption * opt, /*@unused@*/const char * arg,
	const void * data)
{
    QVA_t *qva = (QVA_t *) data;
    switch (opt->val) {
      case POPT_NOFILES: qva->qva_flags |= VERIFY_FILES; break;
    }
}

static int noFiles = 0;
/** */
struct poptOption rpmVerifyPoptTable[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
		verifyArgCallback, 0, NULL, NULL },
 	{ "nofiles", '\0', 0, &noFiles, POPT_NOFILES,
		N_("don't verify files in package"), NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL }
};

/* ======================================================================== */
/* XXX static */
/** */
int rpmVerifyFile(const char * prefix, Header h, int filenum, int * result, 
		  int omitMask)
{
    char ** baseNames, ** md5List, ** linktoList, ** dirNames;
    int_32 * verifyFlags, flags;
    int_32 * sizeList, * mtimeList, * dirIndexes;
    unsigned short * modeList, * rdevList;
    char * fileStatesList;
    char * filespec;
    char * name;
    gid_t gid;
    int type, count, rc;
    struct stat sb;
    unsigned char md5sum[40];
    int_32 * uidList, * gidList;
    char linkto[1024];
    int size;
    char ** unameList, ** gnameList;
    int_32 useBrokenMd5;

  if (IS_BIG_ENDIAN()) {	/* XXX was ifdef WORDS_BIGENDIAN */
    int_32 * brokenPtr;
    if (!headerGetEntry(h, RPMTAG_BROKENMD5, NULL, (void **) &brokenPtr, NULL)) {
	char * rpmVersion;

	if (headerGetEntry(h, RPMTAG_RPMVERSION, NULL, (void **) &rpmVersion, 
				NULL)) {
	    useBrokenMd5 = ((rpmvercmp(rpmVersion, "2.3.3") >= 0) &&
			    (rpmvercmp(rpmVersion, "2.3.8") <= 0));
	} else {
	    useBrokenMd5 = 1;
	}
	headerAddEntry(h, RPMTAG_BROKENMD5, RPM_INT32_TYPE, &useBrokenMd5, 1);
    } else {
	useBrokenMd5 = *brokenPtr;
    }
  } else {
    useBrokenMd5 = 0;
  }

    headerGetEntry(h, RPMTAG_FILEMODES, &type, (void **) &modeList, &count);

    if (headerGetEntry(h, RPMTAG_FILEVERIFYFLAGS, &type, (void **) &verifyFlags, 
		 &count)) {
	flags = verifyFlags[filenum];
    } else {
	flags = RPMVERIFY_ALL;
    }

    headerGetEntry(h, RPMTAG_BASENAMES, &type, (void **) &baseNames, 
		   &count);
    headerGetEntry(h, RPMTAG_DIRINDEXES, &type, (void **) &dirIndexes, 
		   NULL);
    headerGetEntry(h, RPMTAG_DIRNAMES, &type, (void **) &dirNames, NULL);

    filespec = alloca(strlen(dirNames[dirIndexes[filenum]]) + 
		      strlen(baseNames[filenum]) + strlen(prefix) + 5);
    sprintf(filespec, "%s/%s%s", prefix, dirNames[dirIndexes[filenum]],
		baseNames[filenum]);
    free(baseNames);
    free(dirNames);
    
    *result = 0;

    /* Check to see if the file was installed - if not pretend all is OK */
    if (headerGetEntry(h, RPMTAG_FILESTATES, &type, 
		 (void **) &fileStatesList, &count) && fileStatesList) {
	if (fileStatesList[filenum] == RPMFILE_STATE_NOTINSTALLED)
	    return 0;
    }

    if (lstat(filespec, &sb)) {
	*result |= RPMVERIFY_LSTATFAIL;
	return 1;
    }

    if (S_ISDIR(sb.st_mode))
	flags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME | 
			RPMVERIFY_LINKTO);
    else if (S_ISLNK(sb.st_mode)) {
	flags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
		RPMVERIFY_MODE);
#	if CHOWN_FOLLOWS_SYMLINK
	    flags &= ~(RPMVERIFY_USER | RPMVERIFY_GROUP);
#	endif
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

    /* Don't verify any features in omitMask */
    flags &= ~(omitMask | RPMVERIFY_LSTATFAIL|RPMVERIFY_READFAIL|RPMVERIFY_READLINKFAIL);

    if (flags & RPMVERIFY_MD5) {
	headerGetEntry(h, RPMTAG_FILEMD5S, &type, (void **) &md5List, &count);
	if (useBrokenMd5) {
	    rc = mdfileBroken(filespec, md5sum);
	} else {
	    rc = mdfile(filespec, md5sum);
	}

	if (rc)
	    *result |= (RPMVERIFY_READFAIL|RPMVERIFY_MD5);
	else if (strcmp(md5sum, md5List[filenum]))
	    *result |= RPMVERIFY_MD5;
	free(md5List);
    } 
    if (flags & RPMVERIFY_LINKTO) {
	headerGetEntry(h, RPMTAG_FILELINKTOS, &type, (void **) &linktoList, &count);
	size = readlink(filespec, linkto, sizeof(linkto)-1);
	if (size == -1)
	    *result |= (RPMVERIFY_READLINKFAIL|RPMVERIFY_LINKTO);
	else  {
	    linkto[size] = '\0';
	    if (strcmp(linkto, linktoList[filenum]))
		*result |= RPMVERIFY_LINKTO;
	}
	free(linktoList);
    } 

    if (flags & RPMVERIFY_FILESIZE) {
	headerGetEntry(h, RPMTAG_FILESIZES, &type, (void **) &sizeList, &count);
	if (sizeList[filenum] != sb.st_size)
	    *result |= RPMVERIFY_FILESIZE;
    } 

    if (flags & RPMVERIFY_MODE) {
	if (modeList[filenum] != sb.st_mode)
	    *result |= RPMVERIFY_MODE;
    }

    if (flags & RPMVERIFY_RDEV) {
	if (S_ISCHR(modeList[filenum]) != S_ISCHR(sb.st_mode) ||
	    S_ISBLK(modeList[filenum]) != S_ISBLK(sb.st_mode)) {
	    *result |= RPMVERIFY_RDEV;
	} else if (S_ISDEV(modeList[filenum]) && S_ISDEV(sb.st_mode)) {
	    headerGetEntry(h, RPMTAG_FILERDEVS, NULL, (void **) &rdevList, 
			   NULL);
	    if (rdevList[filenum] != sb.st_rdev)
		*result |= RPMVERIFY_RDEV;
	} 
    }

    if (flags & RPMVERIFY_MTIME) {
	headerGetEntry(h, RPMTAG_FILEMTIMES, NULL, (void **) &mtimeList, NULL);
	if (mtimeList[filenum] != sb.st_mtime)
	    *result |= RPMVERIFY_MTIME;
    }

    if (flags & RPMVERIFY_USER) {
	if (headerGetEntry(h, RPMTAG_FILEUSERNAME, NULL, (void **) &unameList, 
			   NULL)) {
	    name = uidToUname(sb.st_uid);
	    if (!name || strcmp(unameList[filenum], name))
		*result |= RPMVERIFY_USER;
	    free(unameList);
	} else if (headerGetEntry(h, RPMTAG_FILEUIDS, NULL, (void **) &uidList, 
				  &count)) {
	    if (uidList[filenum] != sb.st_uid)
		*result |= RPMVERIFY_GROUP;
	} else {
	    rpmError(RPMERR_INTERNAL, _("package lacks both user name and id "
		  "lists (this should never happen)"));
	    *result |= RPMVERIFY_GROUP;
	}
    }

    if (flags & RPMVERIFY_GROUP) {
	if (headerGetEntry(h, RPMTAG_FILEGROUPNAME, NULL, (void **) &gnameList, 
			NULL)) {
	    rc =  gnameToGid(gnameList[filenum],&gid);
	    if (rc || (gid != sb.st_gid))
		*result |= RPMVERIFY_GROUP;
	    free(gnameList);
	} else if (headerGetEntry(h, RPMTAG_FILEGIDS, NULL, (void **) &gidList, 
				  &count)) {
	    if (gidList[filenum] != sb.st_gid)
		*result |= RPMVERIFY_GROUP;
	} else {
	    rpmError(RPMERR_INTERNAL, _("package lacks both group name and id "
		     "lists (this should never happen)"));
	    *result |= RPMVERIFY_GROUP;
	}
    }

    return 0;
}

/* XXX static */
/** */
int rpmVerifyScript(const char * root, Header h, FD_t err)
{
    return runInstScript(root, h, RPMTAG_VERIFYSCRIPT, RPMTAG_VERIFYSCRIPTPROG,
		     0, 0, err);
}

/* ======================================================================== */
static int verifyHeader(QVA_t *qva, Header h)
{
    const char ** fileNames;
    int count;
    int verifyResult;
    int i, ec, rc;
    int_32 * fileFlagsList;
    int omitMask = 0;

    ec = 0;
    if (!(qva->qva_flags & VERIFY_MD5)) omitMask = RPMVERIFY_MD5;

    if (headerGetEntry(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlagsList, NULL) &&
	headerIsEntry(h, RPMTAG_BASENAMES)) {
	rpmBuildFileList(h, &fileNames, &count);

	for (i = 0; i < count; i++) {
	    if ((rc = rpmVerifyFile(qva->qva_prefix, h, i, &verifyResult, omitMask)) != 0) {
		fprintf(stdout, _("missing    %s\n"), fileNames[i]);
	    } else {
		const char * size, * md5, * link, * mtime, * mode;
		const char * group, * user, * rdev;
		/*@observer@*/ static const char *const aok = ".";
		/*@observer@*/ static const char *const unknown = "?";

		if (!verifyResult) continue;
	    
		rc = 1;

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

		fprintf(stdout, "%s%s%s%s%s%s%s%s %c %s\n",
		       size, mode, md5, rdev, link, user, group, mtime, 
		       fileFlagsList[i] & RPMFILE_CONFIG ? 'c' : ' ', 
		       fileNames[i]);
	    }
	    if (rc)
		ec = rc;
	}
	
	free(fileNames);
    }
    return ec;
}

static int verifyDependencies(/*@only@*/ rpmdb rpmdb, Header h) {
    rpmTransactionSet rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    const char * name, * version, * release;
    int i;

    rpmdep = rpmtransCreateSet(rpmdb, NULL);
    rpmtransAddPackage(rpmdep, h, NULL, NULL, 0, NULL);

    rpmdepCheck(rpmdep, &conflicts, &numConflicts);
    rpmtransFree(rpmdep);

    if (numConflicts) {
	headerNVR(h, &name, &version, &release);
	fprintf(stdout, _("Unsatisfied dependencies for %s-%s-%s: "),
		name, version, release);
	for (i = 0; i < numConflicts; i++) {
	    if (i) fprintf(stdout, ", ");
	    fprintf(stdout, "%s", conflicts[i].needsName);
	    if (conflicts[i].needsFlags) {
		printDepFlags(stdout, conflicts[i].needsVersion, 
			      conflicts[i].needsFlags);
	    }
	}
	fprintf(stdout, "\n");
	rpmdepFreeConflicts(conflicts, numConflicts);
	return 1;
    }
    return 0;
}

/** */
int showVerifyPackage(QVA_t *qva, rpmdb rpmdb, Header h)
{
    int ec, rc;
    FD_t fdo;
    ec = 0;
    if ((qva->qva_flags & VERIFY_DEPS) &&
	(rc = verifyDependencies(rpmdb, h)) != 0)
	    ec = rc;
    if ((qva->qva_flags & VERIFY_FILES) &&
	(rc = verifyHeader(qva, h)) != 0)
	    ec = rc;;
    fdo = fdDup(STDOUT_FILENO);
    if ((qva->qva_flags & VERIFY_SCRIPT) &&
	(rc = rpmVerifyScript(qva->qva_prefix, h, fdo)) != 0)
	    ec = rc;
    Fclose(fdo);
    return ec;
}

/** */
int rpmVerify(QVA_t *qva, enum rpmQVSources source, const char *arg)
{
    rpmdb rpmdb = NULL;
    int rc;

    switch (source) {
    case RPMQV_RPM:
	if (!(qva->qva_flags & VERIFY_DEPS))
	    break;
	/*@fallthrough@*/
    default:
	if (rpmdbOpen(qva->qva_prefix, &rpmdb, O_RDONLY, 0644))
	    return 1;
	break;
    }

    rc = rpmQueryVerify(qva, source, arg, rpmdb, showVerifyPackage);

    if (rpmdb)
	rpmdbClose(rpmdb);

    return rc;
}
