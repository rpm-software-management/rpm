#include "system.h"

#ifndef PATH_MAX
# define PATH_MAX 255
#endif

#include "build/rpmbuild.h"

#include "intl.h"
#include "query.h"
#include "url.h"

static char * permsString(int mode);
static void printHeader(Header h, int queryFlags, char * queryFormat);
static void showMatches(rpmdb db, dbiIndexSet matches, int queryFlags, 
			char * queryFormat);
static void printFileInfo(char * name, unsigned int size, unsigned short mode,
			  unsigned int mtime, unsigned short rdev,
			  char * owner, char * group, int uid, int gid,
			  char * linkto);

static int queryHeader(Header h, char * chptr) {
    char * str;
    char * error;

    str = headerSprintf(h, chptr, rpmTagTable, rpmHeaderFormats, &error);
    if (!str) {
	fprintf(stderr, _("error in format: %s\n"), error);
	return 1;
    }

    fputs(str, stdout);

    return 0;
}

static void printHeader(Header h, int queryFlags, char * queryFormat) {
    char * name, * version, * release;
    int_32 count, type;
    char * prefix = NULL;
    char ** fileList, ** fileMD5List;
    char * fileStatesList;
    char ** fileOwnerList = NULL;
    char ** fileGroupList = NULL;
    char ** fileLinktoList;
    int_32 * fileFlagsList, * fileMTimeList, * fileSizeList;
    int_32 * fileUIDList, * fileGIDList;
    uint_16 * fileModeList;
    uint_16 * fileRdevList;
    int i;

    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &count);
    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &count);

    if (!queryFormat && !queryFlags) {
	fprintf(stdout, "%s-%s-%s\n", name, version, release);
    } else {
	if (queryFormat)
	    queryHeader(h, queryFormat);

	if (queryFlags & QUERY_FOR_LIST) {
	    if (!headerGetEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
		 &count)) {
		fputs(_("(contains no files)"), stdout);
	    } else {
		if (!headerGetEntry(h, RPMTAG_FILESTATES, &type, 
			 (void **) &fileStatesList, &count)) {
		    fileStatesList = NULL;
		}
		headerGetEntry(h, RPMTAG_FILEFLAGS, &type, 
			 (void **) &fileFlagsList, &count);
		headerGetEntry(h, RPMTAG_FILESIZES, &type, 
			 (void **) &fileSizeList, &count);
		headerGetEntry(h, RPMTAG_FILEMODES, &type, 
			 (void **) &fileModeList, &count);
		headerGetEntry(h, RPMTAG_FILEMTIMES, &type, 
			 (void **) &fileMTimeList, &count);
		headerGetEntry(h, RPMTAG_FILERDEVS, &type, 
			 (void **) &fileRdevList, &count);
		headerGetEntry(h, RPMTAG_FILELINKTOS, &type, 
			 (void **) &fileLinktoList, &count);
		headerGetEntry(h, RPMTAG_FILEMD5S, &type, 
			 (void **) &fileMD5List, &count);

		if (!headerGetEntry(h, RPMTAG_FILEUIDS, &type, 
			 (void **) &fileUIDList, &count)) {
		    fileUIDList = NULL;
		} else {
		    headerGetEntry(h, RPMTAG_FILEGIDS, &type, 
			     (void **) &fileGIDList, &count);
		}

		if (!headerGetEntry(h, RPMTAG_FILEUSERNAME, &type, 
			 (void **) &fileOwnerList, &count)) {
		    fileOwnerList = NULL;
		} else {
		    headerGetEntry(h, RPMTAG_FILEGROUPNAME, &type, 
			     (void **) &fileGroupList, &count);
		}

		for (i = 0; i < count; i++) {
		    if (!((queryFlags & QUERY_FOR_DOCS) || 
			  (queryFlags & QUERY_FOR_CONFIG)) 
			|| ((queryFlags & QUERY_FOR_DOCS) && 
			    (fileFlagsList[i] & RPMFILE_DOC))
			|| ((queryFlags & QUERY_FOR_CONFIG) && 
			    (fileFlagsList[i] & RPMFILE_CONFIG))) {

			if (!rpmIsVerbose())
			    prefix ? fputs(prefix, stdout) : 0;

			if (queryFlags & QUERY_FOR_STATE) {
			    if (fileStatesList) {
				switch (fileStatesList[i]) {
				  case RPMFILE_STATE_NORMAL:
				    fputs(_("normal        "), stdout); break;
				  case RPMFILE_STATE_REPLACED:
				    fputs(_("replaced      "), stdout); break;
				  case RPMFILE_STATE_NETSHARED:
				    fputs(_("net shared    "), stdout); break;
				  case RPMFILE_STATE_NOTINSTALLED:
				    fputs(_("not installed "), stdout); break;
				  default:
				    fprintf(stdout, _("(unknown %3d) "), 
					  fileStatesList[i]);
				}
			    } else {
				fputs(    _("(no state)    "), stdout);
			    }
			}
			    
			if (queryFlags & QUERY_FOR_DUMPFILES) {
			    fprintf(stdout, "%s %d %d %s 0%o ", fileList[i],
				   fileSizeList[i], fileMTimeList[i],
				   fileMD5List[i], fileModeList[i]);

			    if (fileOwnerList)
				fprintf(stdout, "%s %s", fileOwnerList[i], 
						fileGroupList[i]);
			    else if (fileUIDList)
				fprintf(stdout, "%d %d", fileUIDList[i], 
						fileGIDList[i]);
			    else {
				rpmError(RPMERR_INTERNAL, _("package has "
					"neither file owner or id lists"));
			    }

			    fprintf(stdout, " %s %s %d ", 
				 fileFlagsList[i] & RPMFILE_CONFIG ? "1" : "0",
				 fileFlagsList[i] & RPMFILE_DOC ? "1" : "0",
				 fileRdevList[i]);

			    if (strlen(fileLinktoList[i]))
				fprintf(stdout, "%s\n", fileLinktoList[i]);
			    else
				fprintf(stdout, "X\n");

			} else if (!rpmIsVerbose()) {
			    fputs(fileList[i], stdout);
			} else if (fileOwnerList) 
			    printFileInfo(fileList[i], fileSizeList[i],
					  fileModeList[i], fileMTimeList[i],
					  fileRdevList[i], fileOwnerList[i], 
					  fileGroupList[i], -1, 
					  -1, fileLinktoList[i]);
			else if (fileUIDList) {
			    printFileInfo(fileList[i], fileSizeList[i],
					  fileModeList[i], fileMTimeList[i],
					  fileRdevList[i], NULL, 
					  NULL, fileUIDList[i], 
					  fileGIDList[i], fileLinktoList[i]);
			} else {
			    rpmError(RPMERR_INTERNAL, _("package has "
				    "neither file owner or id lists"));
			}
		    }
		}
	    
		free(fileList);
		free(fileLinktoList);
		free(fileMD5List);
		if (fileOwnerList) free(fileOwnerList);
		if (fileGroupList) free(fileGroupList);
	    }
	}
    }
}

static char * permsString(int mode) {
    static char perms[11];

    strcpy(perms, "-----------");
   
    if (mode & S_ISVTX) perms[10] = 't';

    if (mode & S_IRUSR) perms[1] = 'r';
    if (mode & S_IWUSR) perms[2] = 'w';
    if (mode & S_IXUSR) perms[3] = 'x';
 
    if (mode & S_IRGRP) perms[4] = 'r';
    if (mode & S_IWGRP) perms[5] = 'w';
    if (mode & S_IXGRP) perms[6] = 'x';

    if (mode & S_IROTH) perms[7] = 'r';
    if (mode & S_IWOTH) perms[8] = 'w';
    if (mode & S_IXOTH) perms[9] = 'x';

    if (mode & S_ISUID) {
	if (mode & S_IXUSR) 
	    perms[3] = 's'; 
	else
	    perms[3] = 'S'; 
    }

    if (mode & S_ISGID) {
	if (mode & S_IXGRP) 
	    perms[6] = 's'; 
	else
	    perms[6] = 'S'; 
    }

    if (S_ISDIR(mode)) 
	perms[0] = 'd';
    else if (S_ISLNK(mode)) {
	perms[0] = 'l';
    }
    else if (S_ISFIFO(mode)) 
	perms[0] = 'p';
    else if (S_ISSOCK(mode)) 
	perms[0] = 's';
    else if (S_ISCHR(mode)) {
	perms[0] = 'c';
    } else if (S_ISBLK(mode)) {
	perms[0] = 'b';
    }

    return perms;
}

static void printFileInfo(char * name, unsigned int size, unsigned short mode,
			  unsigned int mtime, unsigned short rdev,
			  char * owner, char * group, int uid, int gid,
			  char * linkto) {
    char sizefield[15];
    char ownerfield[9], groupfield[9];
    char timefield[100] = "";
    time_t themtime;
    time_t currenttime;
    static int thisYear = 0;
    static int thisMonth = 0;
    struct tm * tstruct;
    char * namefield = name;
    char * perms;

    perms = permsString(mode);

    if (!thisYear) {
	currenttime = time(NULL);
	tstruct = localtime(&currenttime);
	thisYear = tstruct->tm_year;
	thisMonth = tstruct->tm_mon;
    }

    ownerfield[8] = groupfield[8] = '\0';

    if (owner) 
	strncpy(ownerfield, owner, 8);
    else
	sprintf(ownerfield, "%-8d", uid);

    if (group) 
	strncpy(groupfield, group, 8);
    else 
	sprintf(groupfield, "%-8d", gid);

    /* this is normally right */
    sprintf(sizefield, "%10d", size);

    /* this knows too much about dev_t */

    if (S_ISLNK(mode)) {
	namefield = alloca(strlen(name) + strlen(linkto) + 10);
	sprintf(namefield, "%s -> %s", name, linkto);
    } else if (S_ISCHR(mode)) {
	perms[0] = 'c';
	sprintf(sizefield, "%3d, %3d", rdev >> 8, rdev & 0xFF);
    } else if (S_ISBLK(mode)) {
	perms[0] = 'b';
	sprintf(sizefield, "%3d, %3d", rdev >> 8, rdev & 0xFF);
    }

    /* this is important if sizeof(int_32) ! sizeof(time_t) */
    themtime = mtime;
    tstruct = localtime(&themtime);

    if (tstruct->tm_year == thisYear || 
	((tstruct->tm_year + 1) == thisYear && tstruct->tm_mon > thisMonth)) 
	strftime(timefield, sizeof(timefield) - 1, "%b %d %H:%M", tstruct);
    else
	strftime(timefield, sizeof(timefield) - 1, "%b %d  %Y", tstruct);

    fprintf(stdout, "%s %8s %8s %10s %s %s\n", perms, ownerfield, groupfield, 
		sizefield, timefield, namefield);
}

static void showMatches(rpmdb db, dbiIndexSet matches, int queryFlags, 
			char * queryFormat) {
    int i;
    Header h;

    for (i = 0; i < matches.count; i++) {
	if (matches.recs[i].recOffset) {
	    rpmMessage(RPMMESS_DEBUG, _("querying record number %d\n"),
			matches.recs[i].recOffset);
	    
	    h = rpmdbGetRecord(db, matches.recs[i].recOffset);
	    if (!h) {
		fprintf(stderr, _("error: could not read database record\n"));
	    } else {
		printHeader(h, queryFlags, queryFormat);
		headerFree(h);
	    }
	}
    }
}

int doQuery(char * prefix, enum querysources source, int queryFlags, 
	     char * arg, char * queryFormat) {
    Header h;
    int offset;
    int fd;
    int rc;
    int isSource;
    rpmdb db;
    dbiIndexSet matches;
    int recNumber;
    int retcode = 0;
    char *end = NULL;
    struct urlContext context;
    int isUrl = 0;
    char path[PATH_MAX];

    if (source != QUERY_RPM) {
	if (rpmdbOpen(prefix, &db, O_RDONLY, 0644)) {
	    exit(1);
	}
    }

    switch (source) {
      case QUERY_RPM:
	if (urlIsURL(arg)) {
	    isUrl = 1;
	    if ((fd = urlGetFd(arg, &context)) < 0) {
		fprintf(stderr, _("open of %s failed: %s\n"), arg, 
			ftpStrerror(fd));
	    }
	} else if (!strcmp(arg, "-")) {
	    fd = 0;
	} else {
	    if ((fd = open(arg, O_RDONLY)) < 0) {
		fprintf(stderr, _("open of %s failed: %s\n"), arg, 
			strerror(errno));
	    }
	}

	if (fd >= 0) {
	    rc = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);

	    close(fd);
	    if (isUrl) {
		urlFinishedFd(&context);
	    }

	    switch (rc) {
		case 0:
		    if (!h) {
			fprintf(stderr, _("old format source packages cannot "
				"be queried\n"));
		    } else {
			printHeader(h, queryFlags, queryFormat);
			headerFree(h);
		    }
		    break;
		case 1:
		    fprintf(stderr, 
			    _("%s does not appear to be a RPM package\n"), 
			    arg);
		    /* fallthrough */
		case 2:
		    fprintf(stderr, _("query of %s failed\n"), arg);
		    retcode = 1;
	    }

	}
		
	break;

      case QUERY_ALL:
	offset = rpmdbFirstRecNum(db);
	while (offset) {
	    h = rpmdbGetRecord(db, offset);
	    if (!h) {
		fprintf(stderr, _("could not read database record!\n"));
		return 1;
	    }
	    printHeader(h, queryFlags, queryFormat);
	    headerFree(h);
	    offset = rpmdbNextRecNum(db, offset);
	}
	break;

      case QUERY_GROUP:
	if (rpmdbFindByGroup(db, arg, &matches)) {
	    fprintf(stderr, _("group %s does not contain any packages\n"), arg);
	    retcode = 1;
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case QUERY_WHATPROVIDES:
	if (rpmdbFindByProvides(db, arg, &matches)) {
	    fprintf(stderr, _("no package provides %s\n"), arg);
	    retcode = 1;
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case QUERY_TRIGGEREDBY:
	if (rpmdbFindByTriggeredBy(db, arg, &matches)) {
	    fprintf(stderr, _("no package triggers %s\n"), arg);
	    retcode = 1;
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case QUERY_WHATREQUIRES:
	if (rpmdbFindByRequiredBy(db, arg, &matches)) {
	    fprintf(stderr, _("no package requires %s\n"), arg);
	    retcode = 1;
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case QUERY_PATH:
	if (*arg != '/') {
		/* Using realpath on the arg isn't correct if the arg is a symlink,
		 * especially if the symlink is a dangling link.  What we should
		 * instead do is use realpath() on `.' and then append arg to
		 * it.
		 */
	    if (realpath(".", path) != NULL) {
		if (path[strlen(path)] != '/') {
			if (strncat(path, "/", PATH_MAX - strlen(path) - 1) == NULL) {
	    		fprintf(stderr, _("maximum path length exceeded\n"));
	    		return 1;
			}
		}
		/* now append the original file name to the real path */
		if (strncat(path, arg, PATH_MAX - strlen(path) - 1) == NULL) {
	    		fprintf(stderr, _("maximum path length exceeded\n"));
	    		return 1;
		}
		arg = path;
	    }
	}
	if (rpmdbFindByFile(db, arg, &matches)) {
	    int myerrno = 0;
	    if (access(arg, F_OK) != 0)
		myerrno = errno;
	    switch (myerrno) {
	    default:
		fprintf(stderr, _("file %s: %s\n"), arg, strerror(myerrno));
		break;
	    case 0:
		fprintf(stderr, _("file %s is not owned by any package\n"), arg);
		break;
	    }
	    retcode = 1;
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case QUERY_DBOFFSET:
	recNumber = strtoul(arg, &end, 10);
	if ((*end) || (end == arg) || (recNumber == ULONG_MAX)) {
	    fprintf(stderr, _("invalid package number: %s\n"), arg);
	    return 1;
	}
	rpmMessage(RPMMESS_DEBUG, _("showing package: %d\n"), recNumber);
	h = rpmdbGetRecord(db, recNumber);

	if (!h)  {
	    fprintf(stderr, _("record %d could not be read\n"), recNumber);
	    retcode = 1;
	} else {
	    printHeader(h, queryFlags, queryFormat);
	    headerFree(h);
	}
	break;

      case QUERY_PACKAGE:
	rc = rpmdbFindByLabel(db, arg, &matches);
	if (rc == 1) {
	    retcode = 1;
	    fprintf(stderr, _("package %s is not installed\n"), arg);
	} else if (rc == 2) {
	    retcode = 1;
	    fprintf(stderr, _("error looking for package %s\n"), arg);
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    dbiFreeIndexRecord(matches);
	}
	break;
    }
   
    if (source != QUERY_RPM) {
	rpmdbClose(db);
    }

    return retcode;
}

void queryPrintTags(void) {
    const struct headerTagTableEntry * t;
    int i;
    struct headerSprintfExtension * ext = rpmHeaderFormats;

    for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	fprintf(stdout, "%s\n", t->name + 7);
    }

    while (ext->name) {
	if (ext->type == HEADER_EXT_TAG)
	    fprintf(stdout, "%s\n", ext->name + 7), ext++;
	else if (ext->type == HEADER_EXT_MORE)
	    ext = ext->u.more;
	else
	    ext++;
    }
}
