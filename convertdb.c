/* This converts an old style (rpm 1.x) database to the new style */

#include "system.h"

#include "build/rpmbuild.h"

#include "falloc.h"
#include "oldrpmdb.h"
#include "oldheader.h"
#include "lib/rpmdb.h"

int convertDB(void);

int convertDB(void)
{
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
    int i, j;
    
    if (rpmfileexists("/var/lib/rpm/packages.rpm")) {
	rpmError(RPMERR_NOCREATEDB, _("RPM database already exists"));
	return 0;
    }

    if (oldrpmdbOpen(&olddb)) {
	rpmError(RPMERR_OLDDBMISSING, _("Old db is missing"));
	return 0;
    }

    /* if any of the indexes exist, get rid of them */
    unlink("/var/lib/rpm/nameindex.rpm");
    unlink("/var/lib/rpm/groupindex.rpm");
    unlink("/var/lib/rpm/packageindex.rpm");
    unlink("/var/lib/rpm/fileindex.rpm");

    if (rpmdbOpen("", &db, O_RDWR | O_CREAT, 0644)) {
	rpmError(RPMERR_DBOPEN, _("failed to create RPM database /var/lib/rpm"));
	return 0;
    }

    packageLabels = oldrpmdbGetAllLabels(&olddb);
    if (!packageLabels) {
	rpmError(RPMERR_OLDDBCORRUPT, _("Old db is corrupt"));
	rpmdbClose(db);
	unlink("/var/lib/rpm/packages.rpm");
	oldrpmdbClose(&olddb);
	return 0;
    }

    for (label = packageLabels; label; label = label->next) {
	if (oldrpmdbGetPackageInfo(&olddb, *label, &package)) {
	    fprintf(stderr, _("oldrpmdbGetPackageInfo failed &olddb = %p olddb.packages = %p\n"), &olddb, olddb.packages);
	    exit(EXIT_FAILURE);
	}

	group = oldrpmdbGetPackageGroup(&olddb, *label);
	preun = oldrpmdbGetPackagePreun(&olddb, *label);
	postun = oldrpmdbGetPackagePostun(&olddb, *label);

	dbentry = headerNew();
	headerAddEntry(dbentry, RPMTAG_NAME, RPM_STRING_TYPE, package.name, 1);
	headerAddEntry(dbentry, RPMTAG_VERSION, RPM_STRING_TYPE, package.version, 1);
	headerAddEntry(dbentry, RPMTAG_RELEASE, RPM_STRING_TYPE, package.release, 1);
	headerAddEntry(dbentry, RPMTAG_DESCRIPTION, RPM_STRING_TYPE, 
		 package.description, 1);
	headerAddEntry(dbentry, RPMTAG_BUILDTIME, RPM_INT32_TYPE, &package.buildTime, 1);
	headerAddEntry(dbentry, RPMTAG_BUILDHOST, RPM_STRING_TYPE, package.buildHost, 1);
	headerAddEntry(dbentry, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, 
		 &package.installTime, 1);
	headerAddEntry(dbentry, RPMTAG_DISTRIBUTION, RPM_STRING_TYPE, 
		 package.distribution, 1);
	headerAddEntry(dbentry, RPMTAG_VENDOR, RPM_STRING_TYPE, package.vendor, 1);
	headerAddEntry(dbentry, RPMTAG_SIZE, RPM_INT32_TYPE, &package.size, 1);
	headerAddEntry(dbentry, RPMTAG_LICENSE, RPM_STRING_TYPE, package.copyright, 1);
	headerAddEntry(dbentry, RPMTAG_GROUP, RPM_STRING_TYPE, group, 1);

	if (preun) {
	    headerAddEntry(dbentry, RPMTAG_PREUN, RPM_STRING_TYPE, preun, 1);
	    free(preun);
	}
	if (postun) {
	    headerAddEntry(dbentry, RPMTAG_POSTUN, RPM_STRING_TYPE, postun, 1);
	    free(postun);
	}

	gif = oldrpmdbGetPackageGif(&olddb, *label, &gifSize);
	if (gif) {
	    headerAddEntry(dbentry, RPMTAG_GIF, RPM_BIN_TYPE, gif, gifSize);
	    free(gif);
	}

	if (package.fileCount) {
	    /* some packages have no file lists */

	    fileList = xmalloc(sizeof(char *) * package.fileCount);
	    fileLinktoList = xmalloc(sizeof(char *) * package.fileCount);
	    fileMD5List = xmalloc(sizeof(char *) * package.fileCount);
	    fileSizeList = xmalloc(sizeof(int_32) * package.fileCount);
	    fileUIDList = xmalloc(sizeof(int_32) * package.fileCount);
	    fileGIDList = xmalloc(sizeof(int_32) * package.fileCount);
	    fileMtimesList = xmalloc(sizeof(int_32) * package.fileCount);
	    fileFlagsList = xmalloc(sizeof(int_32) * package.fileCount);
	    fileModesList = xmalloc(sizeof(int_16) * package.fileCount);
	    fileRDevsList = xmalloc(sizeof(int_16) * package.fileCount);
	    fileStatesList = xmalloc(sizeof(char) * package.fileCount);

	    /* reverse the file list while we're at it */
	    j = package.fileCount - 1;
	    for (i = 0; i < package.fileCount; i++, j--) {
		fileList[j] = package.files[i].path;
		fileMD5List[j] = package.files[i].md5;
		fileSizeList[j] = package.files[i].size;
		fileUIDList[j] = package.files[i].uid;
		fileGIDList[j] = package.files[i].gid;
		fileMtimesList[j] = package.files[i].mtime;
		fileModesList[j] = package.files[i].mode;
		fileRDevsList[j] = package.files[i].rdev;
		fileStatesList[j] = package.files[i].state;

		if (package.files[i].linkto)
		    fileLinktoList[j] = package.files[i].linkto;
		else
		    fileLinktoList[j] = "";
		
		fileFlagsList[j] = 0;
		if (package.files[i].isdoc) 
		    fileFlagsList[j] |= RPMFILE_DOC;
		if (package.files[i].isconf)
		    fileFlagsList[j] |= RPMFILE_CONFIG;
	    }

	    headerAddEntry(dbentry, RPMTAG_FILENAMES, RPM_STRING_ARRAY_TYPE, fileList, 
		     package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILELINKTOS, RPM_STRING_ARRAY_TYPE, 
		     fileLinktoList, package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILEMD5S, RPM_STRING_ARRAY_TYPE, fileMD5List, 
		     package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILESIZES, RPM_INT32_TYPE, fileSizeList, 
		     package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILEUIDS, RPM_INT32_TYPE, fileUIDList, 
		     package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILEGIDS, RPM_INT32_TYPE, fileGIDList, 
		     package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILEMTIMES, RPM_INT32_TYPE, fileMtimesList, 
		     package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILEFLAGS, RPM_INT32_TYPE, fileFlagsList, 
		     package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILEMODES, RPM_INT16_TYPE, fileModesList, 
		     package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILERDEVS, RPM_INT16_TYPE, fileRDevsList, 
		     package.fileCount);
	    headerAddEntry(dbentry, RPMTAG_FILESTATES, RPM_INT8_TYPE, fileStatesList, 
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
	headerFree(dbentry);

	oldrpmdbFreePackageInfo(package);
    }

    oldrpmdbClose(&olddb);
    rpmdbClose(db);

    return 1;
}

int main(int argc, char ** argv)
{
    setprogname(argv[0]);
    if (argc != 1) {
	fprintf(stderr, _("rpmconvert: no arguments expected"));
	exit(EXIT_FAILURE);
    }

    rpmReadConfigFiles(NULL, NULL);

    printf(_("rpmconvert 1.0 - converting database in /var/lib/rpm\n"));
    convertDB();

    exit(EXIT_SUCCESS);
}
