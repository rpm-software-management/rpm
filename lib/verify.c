#include <alloca.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "md5.h"
#include "rpmlib.h"

int rpmVerifyFile(char * prefix, Header h, int filenum, int * result) {
    char ** fileList, ** md5List, ** linktoList;
    int_32 * verifyFlags, flags;
    int_32 * sizeList, * mtimeList;
    unsigned short * modeList, * rdevList;
    char * filespec;
    int type, count, rc;
    struct stat sb;
    unsigned char md5sum[40];
    char linkto[1024];
    int size;
    int_32 * uidList, * gidList;
    int useBrokenMd5;

    if (getEntry(h, RPMTAG_RPMVERSION, NULL, NULL, NULL))
	useBrokenMd5 = 0;
    else
	useBrokenMd5 = 1;

    getEntry(h, RPMTAG_FILEMODES, &type, (void **) &modeList, &count);

    if (getEntry(h, RPMTAG_FILEVERIFYFLAGS, &type, (void **) &verifyFlags, 
		 &count)) {
	flags = verifyFlags[filenum];
    } else {
	flags = VERIFY_ALL;
    }

    getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, &count);
    filespec = alloca(strlen(fileList[filenum]) + strlen(prefix) + 5);
    strcpy(filespec, prefix);
    strcat(filespec, "/");
    strcat(filespec, fileList[filenum]);

    free(fileList);
    
    if (lstat(filespec, &sb)) 
	return 1;

    *result = 0;

    if (S_ISDIR(sb.st_mode))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME | VERIFY_LINKTO);
    else if (S_ISLNK(sb.st_mode))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME);
    else if (S_ISFIFO(sb.st_mode))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME | VERIFY_LINKTO);
    else if (S_ISCHR(sb.st_mode))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME | VERIFY_LINKTO);
    else if (S_ISBLK(sb.st_mode))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME | VERIFY_LINKTO);
    else 
	flags &= ~(VERIFY_LINKTO);

    if (flags & VERIFY_MD5) {
	getEntry(h, RPMTAG_FILEMD5S, &type, (void **) &md5List, &count);
	if (useBrokenMd5) {
	    rc = mdfileBroken(filespec, md5sum);
	} else {
	    rc = mdfile(filespec, md5sum);
	}

	if (rc || strcmp(md5sum, md5List[filenum]))
	    *result |= VERIFY_MD5;
	free(md5List);
    } 
    if (flags & VERIFY_LINKTO) {
	getEntry(h, RPMTAG_FILELINKTOS, &type, (void **) &linktoList, &count);
	size = readlink(filespec, linkto, sizeof(linkto));
	if (size == -1)
	    *result |= VERIFY_LINKTO;
	else 
	    linkto[size] = '\0';
	    if (strcmp(linkto, linktoList[filenum]))
		*result |= VERIFY_LINKTO;
	free(linktoList);
    } 
    if (flags & VERIFY_FILESIZE) {
	getEntry(h, RPMTAG_FILESIZES, &type, (void **) &sizeList, &count);
	if (sizeList[filenum] != sb.st_size)
	    *result |= VERIFY_FILESIZE;
    } 
    if (flags & VERIFY_MODE) {
	if (modeList[filenum] != sb.st_mode)
	    *result |= VERIFY_MODE;
    }
    if (flags & VERIFY_RDEV) {
	getEntry(h, RPMTAG_FILERDEVS, &type, (void **) &rdevList, &count);
	if (rdevList[filenum] != sb.st_rdev)
	    *result |= VERIFY_RDEV;
    }
    if (flags & VERIFY_MTIME) {
	getEntry(h, RPMTAG_FILEMTIMES, &type, (void **) &mtimeList, &count);
	if (mtimeList[filenum] != sb.st_mtime)
	    *result |= VERIFY_MTIME;
    }
    if (flags & VERIFY_USER) {
	getEntry(h, RPMTAG_FILEUIDS, &type, (void **) &uidList, &count);
	if (uidList[filenum] != sb.st_uid)
	    *result |= VERIFY_USER;
    }
    if (flags & VERIFY_GROUP) {
	getEntry(h, RPMTAG_FILEGIDS, &type, (void **) &gidList, &count);
	if (gidList[filenum] != sb.st_gid)
	    *result |= VERIFY_GROUP;
    }

    return 0;
}
