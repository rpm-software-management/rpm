#include "system.h"

#ifdef	__LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
#else
#include <netinet/in.h>
#endif	/* __LCLINT__ */

#include <rpmlib.h>

#include "misc.h"
#include "oldheader.h"
#include "rpmlead.h"
#include "signature.h"

#if defined(ENABLE_V1_PACKAGES)
/* 0 = success */
/* !0 = error */
static int readOldHeader(FD_t fd, /*@out@*/Header * hdr, /*@unused@*/ /*@out@*/int * isSource)
{
    struct oldrpmHeader oldheader;
    struct oldrpmHeaderSpec spec;
    Header dbentry;
    int_32 installTime = 0;
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
    int i, j;
    char ** unames, ** gnames;

    (void)Fseek(fd, 0, SEEK_SET);

    if (oldhdrReadFromStream(fd, &oldheader)) {
	return 1;
    }

    if (oldhdrParseSpec(&oldheader, &spec)) {
	return 1;
    }

    dbentry = headerNew();
    headerAddEntry(dbentry, RPMTAG_NAME, RPM_STRING_TYPE, oldheader.name, 1);
    headerAddEntry(dbentry, RPMTAG_VERSION, RPM_STRING_TYPE, oldheader.version, 1);
    headerAddEntry(dbentry, RPMTAG_RELEASE, RPM_STRING_TYPE, oldheader.release, 1);
    headerAddEntry(dbentry, RPMTAG_DESCRIPTION, RPM_STRING_TYPE, 
	     spec.description, 1);
    headerAddEntry(dbentry, RPMTAG_BUILDTIME, RPM_INT32_TYPE, &spec.buildTime, 1);
    headerAddEntry(dbentry, RPMTAG_BUILDHOST, RPM_STRING_TYPE, spec.buildHost, 1);
    headerAddEntry(dbentry, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, &installTime, 1); 
    headerAddEntry(dbentry, RPMTAG_DISTRIBUTION, RPM_STRING_TYPE, 
	     spec.distribution, 1);
    headerAddEntry(dbentry, RPMTAG_VENDOR, RPM_STRING_TYPE, spec.vendor, 1);
    headerAddEntry(dbentry, RPMTAG_SIZE, RPM_INT32_TYPE, &oldheader.size, 1);
    headerAddEntry(dbentry, RPMTAG_LICENSE, RPM_STRING_TYPE, spec.copyright, 1); 

    if (oldheader.group)
	headerAddEntry(dbentry, RPMTAG_GROUP, RPM_STRING_TYPE, oldheader.group, 1);
    else
	headerAddEntry(dbentry, RPMTAG_GROUP, RPM_STRING_TYPE, "Unknown", 1);

    if (spec.prein) 
	headerAddEntry(dbentry, RPMTAG_PREIN, RPM_STRING_TYPE, spec.prein, 1);
    if (spec.preun) 
	headerAddEntry(dbentry, RPMTAG_PREUN, RPM_STRING_TYPE, spec.preun, 1);
    if (spec.postin) 
	headerAddEntry(dbentry, RPMTAG_POSTIN, RPM_STRING_TYPE, spec.postin, 1);
    if (spec.postun) 
	headerAddEntry(dbentry, RPMTAG_POSTUN, RPM_STRING_TYPE, spec.postun, 1);

    *hdr = dbentry;

    if (spec.fileCount) {
	/* some packages have no file lists */

	fileList = xmalloc(sizeof(char *) * spec.fileCount);
	fileLinktoList = xmalloc(sizeof(char *) * spec.fileCount);
	fileMD5List = xmalloc(sizeof(char *) * spec.fileCount);
	fileSizeList = xmalloc(sizeof(int_32) * spec.fileCount);
	fileUIDList = xmalloc(sizeof(int_32) * spec.fileCount);
	fileGIDList = xmalloc(sizeof(int_32) * spec.fileCount);
	fileMtimesList = xmalloc(sizeof(int_32) * spec.fileCount);
	fileFlagsList = xmalloc(sizeof(int_32) * spec.fileCount);
	fileModesList = xmalloc(sizeof(int_16) * spec.fileCount);
	fileRDevsList = xmalloc(sizeof(int_16) * spec.fileCount);
	fileStatesList = xmalloc(sizeof(char) * spec.fileCount);
	unames = xmalloc(sizeof(char *) * spec.fileCount);
	gnames = xmalloc(sizeof(char *) * spec.fileCount);

	/* We also need to contstruct a file owner/group list. We'll just
	   hope the numbers all map to something, those that don't will
	   get set as 'id%d'. Not perfect, but this should be
	   good enough. */

	/* old packages were reverse sorted, new ones are forward sorted */
	j = spec.fileCount - 1;
	for (i = 0; i < spec.fileCount; i++, j--) {
	    fileList[j] = spec.files[i].path;
	    fileMD5List[j] = spec.files[i].md5;
	    fileSizeList[j] = spec.files[i].size;
	    fileUIDList[j] = spec.files[i].uid;
	    fileGIDList[j] = spec.files[i].gid;
	    fileMtimesList[j] = spec.files[i].mtime;
	    fileModesList[j] = spec.files[i].mode;
	    fileRDevsList[j] = spec.files[i].rdev;
	    fileStatesList[j] = spec.files[i].state;

	    if (spec.files[i].linkto)
		fileLinktoList[j] = spec.files[i].linkto;
	    else
		fileLinktoList[j] = "";
	    
	    fileFlagsList[j] = 0;
	    if (spec.files[i].isdoc) 
		fileFlagsList[j] |= RPMFILE_DOC;
	    if (spec.files[i].isconf)
		fileFlagsList[j] |= RPMFILE_CONFIG;

	    unames[j] = uidToUname(fileUIDList[j]);
	    if (unames[j])
		unames[j] = xstrdup(unames[j]);
	    else {
		unames[j] = xmalloc(20);
		sprintf(unames[j], "uid%d", fileUIDList[j]);
	    }

	    gnames[j] = gidToGname(fileGIDList[j]);
	    if (gnames[j])
		gnames[j] = xstrdup(gnames[j]);
	    else {
		gnames[j] = xmalloc(20);
		sprintf(gnames[j], "gid%d", fileGIDList[j]);
	    }
	}

	headerAddEntry(*hdr, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE, 
			fileList, spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILELINKTOS, RPM_STRING_ARRAY_TYPE, 
		 fileLinktoList, spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILEMD5S, RPM_STRING_ARRAY_TYPE, 
			fileMD5List, spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILESIZES, RPM_INT32_TYPE, fileSizeList, 
		 	spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILEUIDS, RPM_INT32_TYPE, fileUIDList, 
		 	spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILEGIDS, RPM_INT32_TYPE, fileGIDList, 
		 	spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILEMTIMES, RPM_INT32_TYPE, 
			fileMtimesList, spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILEFLAGS, RPM_INT32_TYPE, 
			fileFlagsList, spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILEMODES, RPM_INT16_TYPE, 
			fileModesList, spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILERDEVS, RPM_INT16_TYPE, 
			fileRDevsList, spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILESTATES, RPM_INT8_TYPE, 
			fileStatesList, spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILEUSERNAME, RPM_STRING_ARRAY_TYPE, 
			unames, spec.fileCount);
	headerAddEntry(*hdr, RPMTAG_FILEGROUPNAME, RPM_STRING_ARRAY_TYPE, 
			gnames, spec.fileCount);

	compressFilelist(*hdr);

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

	for (i = 0; i < spec.fileCount; i++) {
	    free(unames[i]);
	    free(gnames[i]);
	}

	free(unames);
	free(gnames);
    }

    oldhdrFree(&oldheader);

    return 0;
}
#endif	/* ENABLE_V1_PACKAGES */

/* 0 = success */
/* 1 = bad magic */
/* 2 = error */
static int readPackageHeaders(FD_t fd, /*@out@*/struct rpmlead * leadPtr, 
			      /*@out@*/Header * sigs, /*@out@*/Header * hdrPtr)
{
    Header hdrBlock;
    struct rpmlead leadBlock;
    Header * hdr = NULL;
    struct rpmlead * lead;
    int_8 arch;
    int isSource;
    char * defaultPrefix;
    struct stat sb;
    int_32 true = 1;

    hdr = hdrPtr ? hdrPtr : &hdrBlock;
    lead = leadPtr ? leadPtr : &leadBlock;

    fstat(Fileno(fd), &sb);
    /* if fd points to a socket, pipe, etc, sb.st_size is *always* zero */
    if (S_ISREG(sb.st_mode) && sb.st_size < sizeof(*lead)) return 1;

    if (readLead(fd, lead)) {
	return 2;
    }

    if (lead->magic[0] != RPMLEAD_MAGIC0 || lead->magic[1] != RPMLEAD_MAGIC1 ||
	lead->magic[2] != RPMLEAD_MAGIC2 || lead->magic[3] != RPMLEAD_MAGIC3) {
	return 1;
    }

    switch(lead->major) {
#if defined(ENABLE_V1_PACKAGES)
    case 1:
	rpmMessage(RPMMESS_DEBUG, _("package is a version one package!\n"));

	if (lead->type == RPMLEAD_SOURCE) {
	    struct oldrpmlead * oldLead = (struct oldrpmlead *) lead;

	    rpmMessage(RPMMESS_DEBUG, _("old style source package -- "
			"I'll do my best\n"));
	    oldLead->archiveOffset = ntohl(oldLead->archiveOffset);
	    rpmMessage(RPMMESS_DEBUG, _("archive offset is %d\n"), 
			oldLead->archiveOffset);

	    (void)Fseek(fd, oldLead->archiveOffset, SEEK_SET);
	    
	    /* we can't put togeher a header for old format source packages,
	       there just isn't enough information there. We'll return
	       NULL <gulp> */

	    *hdr = NULL;
	} else {
	    rpmMessage(RPMMESS_DEBUG, _("old style binary package\n"));
	    readOldHeader(fd, hdr, &isSource);
	    arch = lead->archnum;
	    headerAddEntry(*hdr, RPMTAG_ARCH, RPM_INT8_TYPE, &arch, 1);
	    arch = 1;		  /* old versions of RPM only supported Linux */
	    headerAddEntry(*hdr, RPMTAG_OS, RPM_INT8_TYPE, &arch, 1);
	}
	break;
#endif	/* ENABLE_V1_PACKAGES */

    case 2:
    case 3:
	if (rpmReadSignature(fd, sigs, lead->signature_type)) {
	   return 2;
	}
	*hdr = headerRead(fd, (lead->major >= 3) ?
			  HEADER_MAGIC_YES : HEADER_MAGIC_NO);
	if (*hdr == NULL) {
	    if (sigs != NULL) {
		headerFree(*sigs);
	    }
	    return 2;
	}

	/* We don't use these entries (and rpm >= 2 never have) and they are
	   pretty misleading. Let's just get rid of them so they don't confuse
	   anyone. */
	if (headerIsEntry(*hdr, RPMTAG_FILEUSERNAME))
	    headerRemoveEntry(*hdr, RPMTAG_FILEUIDS);
	if (headerIsEntry(*hdr, RPMTAG_FILEGROUPNAME))
	    headerRemoveEntry(*hdr, RPMTAG_FILEGIDS);

	/* We switched the way we do relocateable packages. We fix some of
	   it up here, though the install code still has to be a bit 
	   careful. This fixup makes queries give the new values though,
	   which is quite handy. */
	if (headerGetEntry(*hdr, RPMTAG_DEFAULTPREFIX, NULL,
			   (void **) &defaultPrefix, NULL)) {
	    defaultPrefix = strcpy(alloca(strlen(defaultPrefix) + 1), 
				   defaultPrefix);
	    stripTrailingSlashes(defaultPrefix);
	    headerAddEntry(*hdr, RPMTAG_PREFIXES, RPM_STRING_ARRAY_TYPE,
			   &defaultPrefix, 1); 
	}

	/* The file list was moved to a more compressed format which not
	   only saves memory (nice), but gives fingerprinting a nice, fat
	   speed boost (very nice). Go ahead and convert old headers to
	   the new style (this is a noop for new headers) */
	compressFilelist(*hdr);

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
        if (lead->type == RPMLEAD_SOURCE) {
	    if (!headerIsEntry(*hdr, RPMTAG_SOURCEPACKAGE))
	    	headerAddEntry(*hdr, RPMTAG_SOURCEPACKAGE, RPM_INT32_TYPE,
				&true, 1);
	}
	break;

    default:
	rpmError(RPMERR_NEWPACKAGE, _("only packages with major numbers <= 3 "
		"are supported by this version of RPM"));
	return 2;
	/*@notreached@*/ break;
    } 

    if (hdrPtr == NULL) {
	headerFree(*hdr);
    }
    
    return 0;
}

/* 0 = success */
/* 1 = bad magic */
/* 2 = error */
int rpmReadPackageInfo(FD_t fd, Header * signatures, Header * hdr)
{
    return readPackageHeaders(fd, NULL, signatures, hdr);
}

/* 0 = success */
/* 1 = bad magic */
/* 2 = error */
int rpmReadPackageHeader(FD_t fd, Header * hdr, int * isSource, int * major,
		  int * minor)
{
    int rc;
    struct rpmlead lead;

    rc = readPackageHeaders(fd, &lead, NULL, hdr);
    if (rc) return rc;
   
    if (isSource) *isSource = lead.type == RPMLEAD_SOURCE;
    if (major) *major = lead.major;
    if (minor) *minor = lead.minor;
   
    return 0;
}
