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
    int type, count;
    struct stat sb;
    unsigned char md5sum[40];
    char linkto[1024];
    int size;

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

    if (S_ISDIR(modeList[filenum]))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME | VERIFY_LINKTO);
    else if (S_ISLNK(modeList[filenum]))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME);
    else if (S_ISFIFO(modeList[filenum]))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME | VERIFY_LINKTO);
    else if (S_ISCHR(modeList[filenum]))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME | VERIFY_LINKTO);
    else if (S_ISBLK(modeList[filenum]))
	flags &= ~(VERIFY_MD5 | VERIFY_FILESIZE | VERIFY_MTIME | VERIFY_LINKTO);
    else 
	flags &= ~(VERIFY_LINKTO);

    if (flags & VERIFY_MD5) {
	getEntry(h, RPMTAG_FILEMD5S, &type, (void **) &md5List, &count);
	if (mdfile(filespec, md5sum))
	    *result |= VERIFY_MD5;
	else if (strcmp(md5sum, md5List[filenum]))
	    *result |= VERIFY_MD5;
	free(md5List);
    } 
    if (flags & VERIFY_LINKTO) {
	getEntry(h, RPMTAG_FILELINKTOS, &type, (void **) &linktoList, &count);
	if ((size = readlink(filespec, linkto, sizeof(linkto))))
	    *result |= VERIFY_LINKTO;
	else 
	    filespec[size] = '\0';
	    if (strcmp(filespec, linktoList[filenum]))
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

    return 0;
}
