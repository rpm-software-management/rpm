/* This converts an old style (rpm 1.x) database to the new style */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "dbindex.h"
#include "falloc.h"
#include "header.h"
#include "misc.h"
#include "oldrpmdb.h"
#include "oldheader.h"
#include "rpmerr.h"
#include "rpmlib.h"

int convertDB(void);

int convertDB(void) {
    struct oldrpmdb olddb;
    rpmdb db;
    struct oldrpmdbLabel * packageLabels, * label;
    struct oldrpmdbPackageInfo package;
    Header dbentry;
    char * group;
    char * gif;
    int gifSize;
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
    char * preun, * postun;
    int i;
    
    if (oldrpmdbOpen(&olddb)) {
	error(RPMERR_OLDDBMISSING, "");
	return 0;
    }

    if (exists("/var/lib/rpm/packages.rpm")) {
#if 0
	error(RPMERR_NOCREATEDB, "RPM database already exists");
	return 0;
#endif
	unlink("/var/lib/rpm/packages.rpm");
    }

    /* if any of the indexes exist, get rid of them */
    unlink("/var/lib/rpm/nameindex.rpm");
    unlink("/var/lib/rpm/groupindex.rpm");
    unlink("/var/lib/rpm/packageindex.rpm");
    unlink("/var/lib/rpm/fileindex.rpm");

    if (!rpmdbOpen("", &db, O_RDWR | O_EXCL | O_CREAT, 0644)) {
	error(RPMERR_DBOPEN, "failed to create RPM database /var/lib/rpm");
	return 0;
    }

    packageLabels = oldrpmdbGetAllLabels(&olddb);
    if (!packageLabels) {
	error(RPMERR_OLDDBCORRUPT, "");
	rpmdbClose(db);
	unlink("/var/lib/rpm/packages.rpm");
	oldrpmdbClose(&olddb);
	return 0;
    }

    for (label = packageLabels; label; label = label->next) {
	if (oldrpmdbGetPackageInfo(&olddb, *label, &package)) {
	    fprintf(stderr, "oldrpmdbGetPackageInfo failed &olddb = %p olddb.packages = %p\n", &olddb, olddb.packages);
	    exit(1);
	}

	group = oldrpmdbGetPackageGroup(&olddb, *label);
	preun = oldrpmdbGetPackagePreun(&olddb, *label);
	postun = oldrpmdbGetPackagePostun(&olddb, *label);

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

	if (preun) {
	    addEntry(dbentry, RPMTAG_PREUN, STRING_TYPE, preun, 1);
	    free(preun);
	}
	if (postun) {
	    addEntry(dbentry, RPMTAG_POSTUN, STRING_TYPE, postun, 1);
	    free(postun);
	}

	gif = oldrpmdbGetPackageGif(&olddb, *label, &gifSize);
	if (gif) {
	    /*addEntry(dbentry, RPMTAG_GIF, BIN_TYPE, gif, gifSize);*/
	    free(gif);
	}

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

	    addEntry(dbentry, RPMTAG_FILENAMES, STRING_ARRAY_TYPE, fileList, 
		     package.fileCount);
	    addEntry(dbentry, RPMTAG_FILELINKTOS, STRING_ARRAY_TYPE, 
		     fileLinktoList, package.fileCount);
	    addEntry(dbentry, RPMTAG_FILEMD5S, STRING_ARRAY_TYPE, fileMD5List, 
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
	    addEntry(dbentry, RPMTAG_FILESTATES, INT8_TYPE, fileStatesList, 
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

        rpmdbAdd(db, dbentry);

	free(group);
	freeHeader(dbentry);

	oldrpmdbFreePackageInfo(package);
    }

    oldrpmdbClose(&olddb);
    rpmdbClose(db);

    return 1;
}

int main(int argc, char ** argv) {
    if (argc != 1) {
	fprintf(stderr, "rpmconvert: no arguments expected");
	exit(1);
    }

    printf("rpmconvert 1.0 - converting database in /var/lib/rpm\n");
    convertDB();

    exit(0);
}
