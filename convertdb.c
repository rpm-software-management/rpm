/* This converts an old style (rpm 1.x) database to the new style */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "falloc.h"
#include "header.h"
#include "misc.h"
#include "oldrpmdb.h"
#include "oldrpmfile.h"
#include "rpmerr.h"
#include "rpmlib.h"

int reindexDB(char * dbprefix) {
    return 0;
}

int convertDB(char * dbprefix) {
    struct rpmdb olddb;
    faFile pkgs;
    struct rpmdbLabel * packageLabels, * label;
    struct rpmdbPackageInfo package;
    Header dbentry;
    unsigned int dboffset;
    char * group;
    char ** fileList;
    char ** fileMD5List;
    char ** fileLinktoList;
    int_32 * fileSizeList;
    int_32 * fileUIDList;
    int_32 * fileGIDList;
    int_32 * fileMtimesList;
    int_32 * fileFlagsList;
    int_16 * fileModesList;
    int_16 * fileRDevsList;
    char * fileStatesList;
    int i;
    
    if (rpmdbOpen(&olddb)) {
	error(RPMERR_OLDDBMISSING);
	return 0;
    }

    if (exists("/var/lib/rpm/packages.rpm")) {
#if 0
	error(RPMERR_NOCREATEDB);
	return 0;
#endif
	unlink("/var/lib/rpm/packages.rpm");
    }

    pkgs = faOpen("/var/lib/rpm/packages.rpm", O_RDWR | O_CREAT, 0644);
    if (!pkgs) {
	rpmdbClose(&olddb);
	error(RPMERR_DBOPEN);
	return 0;
    }

    packageLabels = rpmdbGetAllLabels(&olddb);
    if (!packageLabels) {
	error(RPMERR_OLDDBCORRUPT);
	faClose(pkgs);
	unlink("/var/lib/rpm/packages.rpm");
	rpmdbClose(&olddb);
	return 0;
    }

    for (label = packageLabels; label; label = label->next) {
	if (rpmdbGetPackageInfo(&olddb, *label, &package)) {
	    printf("rpmdbGetPackageInfo failed &olddb = %p olddb.packages = %p\n", &olddb, olddb.packages);
	    exit(1);
	}

	group = rpmdbGetPackageGroup(&olddb, *label);

	dbentry = newHeader();
	addEntry(dbentry, RPMTAG_NAME, STRING_TYPE, package.name, 1);
	addEntry(dbentry, RPMTAG_VERSION, STRING_TYPE, package.version, 1);
	addEntry(dbentry, RPMTAG_RELEASE, STRING_TYPE, package.release, 1);
	addEntry(dbentry, RPMTAG_DESCRIPTION, STRING_TYPE, 
		 package.description, 1);
	addEntry(dbentry, RPMTAG_BUILDTIME, INT32_TYPE, &package.buildTime, 1);
	addEntry(dbentry, RPMTAG_BUILDHOST, STRING_TYPE, package.buildHost, 1);
	addEntry(dbentry, RPMTAG_INSTALLTIME, INT32_TYPE, 
		 &package.installTime, 1);
	addEntry(dbentry, RPMTAG_DISTRIBUTION, STRING_TYPE, 
		 package.distribution, 1);
	addEntry(dbentry, RPMTAG_VENDOR, STRING_TYPE, package.vendor, 1);
	addEntry(dbentry, RPMTAG_SIZE, INT32_TYPE, &package.size, 1);
	addEntry(dbentry, RPMTAG_COPYRIGHT, STRING_TYPE, package.copyright, 1);
	addEntry(dbentry, RPMTAG_GROUP, STRING_TYPE, group, 1);

	if (package.fileCount) {
	    /* some packages have no file lists */

	    fileList = malloc(sizeof(char *) * package.fileCount);
	    fileLinktoList = malloc(sizeof(char *) * package.fileCount);
	    fileMD5List = malloc(sizeof(char *) * package.fileCount);
	    fileSizeList = malloc(sizeof(int_32) * package.fileCount);
	    fileUIDList = malloc(sizeof(int_32) * package.fileCount);
	    fileGIDList = malloc(sizeof(int_32) * package.fileCount);
	    fileMtimesList = malloc(sizeof(int_32) * package.fileCount);
	    fileFlagsList = malloc(sizeof(int_32) * package.fileCount);
	    fileModesList = malloc(sizeof(int_16) * package.fileCount);
	    fileRDevsList = malloc(sizeof(int_16) * package.fileCount);
	    fileStatesList = malloc(sizeof(char) * package.fileCount);

	    for (i = 0; i < package.fileCount; i++) {
		fileList[i] = package.files[i].path;
		fileMD5List[i] = package.files[i].md5;
		fileSizeList[i] = package.files[i].size;
		fileUIDList[i] = package.files[i].uid;
		fileGIDList[i] = package.files[i].gid;
		fileMtimesList[i] = package.files[i].mtime;
		fileModesList[i] = package.files[i].mode;
		fileRDevsList[i] = package.files[i].rdev;
		fileStatesList[i] = package.files[i].state;

		if (package.files[i].linkto)
		    fileLinktoList[i] = package.files[i].linkto;
		else
		    fileLinktoList[i] = "";
		
		fileFlagsList[i] = 0;
		if (package.files[i].isdoc) 
		    fileFlagsList[i] |= RPMFILE_DOC;
		if (package.files[i].isconf)
		    fileFlagsList[i] |= RPMFILE_CONFIG;
	    }

	    addEntry(dbentry, RPMTAG_FILENAMES, STRING_TYPE, fileList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILELINKTOS, STRING_TYPE, fileLinktoList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILEMD5S, STRING_TYPE, fileMD5List, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILESIZES, INT32_TYPE, fileSizeList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILEUIDS, INT32_TYPE, fileUIDList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILEGIDS, INT32_TYPE, fileGIDList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILEMTIMES, INT32_TYPE, fileMtimesList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILEFLAGS, INT32_TYPE, fileFlagsList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILEMODES, INT16_TYPE, fileModesList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILERDEVS, INT16_TYPE, fileRDevsList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILESTATES, CHAR_TYPE, fileStatesList, 
		     package.fileCount);

	    free(fileList);
	    free(fileLinktoList);
	    free(fileMD5List);
	    free(fileSizeList);
	    free(fileUIDList);
	    free(fileGIDList);
	    free(fileMtimesList);
	    free(fileFlagsList);
	    free(fileModesList);
	    free(fileRDevsList);
	    free(fileStatesList);
	}

	dboffset = faAlloc(pkgs, sizeofHeader(dbentry));
	lseek(pkgs->fd, dboffset, SEEK_SET);

	writeHeader(pkgs->fd, dbentry);

	free(group);
	freeHeader(dbentry);

	rpmdbFreePackageInfo(package);
    }

    rpmdbClose(&olddb);
    faClose(pkgs);

    reindexDB(dbprefix);
    
    return 1;
}
