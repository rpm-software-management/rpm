#include "system.h"

#include "rpmlib.h"

#include "md5.h"
#include "misc.h"
#include "install.h"

static int _ie = 0x44332211;
static union _endian { int i; char b[4]; } *_endian = (union _endian *)&_ie;
#define	IS_BIG_ENDIAN()		(_endian->b[0] == '\x44')
#define	IS_LITTLE_ENDIAN()	(_endian->b[0] == '\x11')

#define S_ISDEV(m) (S_ISBLK((m)) || S_ISCHR((m)))

int rpmVerifyFile(const char * prefix, Header h, int filenum, int * result, 
		  int omitMask) {
    char ** fileList, ** md5List, ** linktoList;
    int_32 * verifyFlags, flags;
    int_32 * sizeList, * mtimeList;
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

    headerGetEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, &count);
    filespec = alloca(strlen(fileList[filenum]) + strlen(prefix) + 5);
    strcpy(filespec, prefix);
    strcat(filespec, "/");
    strcat(filespec, fileList[filenum]);

    free(fileList);
    
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
	size = readlink(filespec, linkto, sizeof(linkto));
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

int rpmVerifyScript(const char * root, Header h, FD_t err) {
    return runInstScript(root, h, RPMTAG_VERIFYSCRIPT, RPMTAG_VERIFYSCRIPTPROG,
		     0, 0, err);
}
