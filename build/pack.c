/* The very final packaging steps */

#include "config.h"
#include "miscfn.h"

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <sys/time.h>  /* For 'select()' interfaces */

#include "cpio.h"
#include "pack.h"
#include "header.h"
#include "spec.h"
#include "specP.h"
#include "signature.h"
#include "rpmlead.h"
#include "rpmlib.h"
#include "misc.h"
#include "stringbuf.h"
#include "names.h"
#include "files.h"
#include "reqprov.h"
#include "trigger.h"
#include "messages.h"

static int writeMagic(int fd, char *name, unsigned short type);
static int cpio_gzip(int fd, char *tempdir, char *writePtr,
		     int *archiveSize, char *prefix);
static int generateRPM(char *name,       /* name-version-release         */
		       char *filename,   /* output filename              */
		       int type,         /* source or binary             */
		       Header header,    /* the header                   */
		       char *stempdir,   /* directory containing sources */
		       char *fileList,   /* list of files for cpio       */
		       char *passPhrase, /* PGP passphrase               */
		       char *prefix);


static int generateRPM(char *name,       /* name-version-release         */
		       char *filename,   /* output filename              */
		       int type,         /* source or binary             */
		       Header header,    /* the header                   */
		       char *stempdir,   /* directory containing sources */
		       char *fileList,   /* list of files for cpio       */
		       char *passPhrase,
		       char *prefix)
{
    int_32 sigtype;
    char *sigtarget;
    int fd, ifd, count, archiveSize;
    unsigned char buffer[8192];
    Header sig;

    /* Add the a bogus archive size to the Header */
    headerAddEntry(header, RPMTAG_ARCHIVESIZE, RPM_INT32_TYPE,
		   &archiveSize, 1);

    /* Write the header */
    sigtarget = tempnam(rpmGetVar(RPMVAR_TMPPATH), "rpmbuild");
    if ((fd = open(sigtarget, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
	fprintf(stderr, "Could not open %s\n", sigtarget);
	return 1;
    }
    headerWrite(fd, header, HEADER_MAGIC_YES);
    
    /* Write the archive and get the size */
    if (cpio_gzip(fd, stempdir, fileList, &archiveSize, prefix)) {
	close(fd);
	unlink(sigtarget);
	return 1;
    }

    /* Now set the real archive size in the Header */
    headerModifyEntry(header, RPMTAG_ARCHIVESIZE,
		      RPM_INT32_TYPE, &archiveSize, 1);

    /* Rewind and rewrite the Header */
    lseek(fd, 0,  SEEK_SET);
    headerWrite(fd, header, HEADER_MAGIC_YES);
    
    close(fd);
    
    /* Now write the lead */
    if ((fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
	fprintf(stderr, "Could not open %s\n", filename);
	unlink(sigtarget);
	unlink(filename);
	return 1;
    }
    if (writeMagic(fd, name, type)) {
	close(fd);
	unlink(sigtarget);
	unlink(filename);
	return 1;
    }

    /* Generate the signature */
    sigtype = rpmLookupSignatureType();
    rpmMessage(RPMMESS_VERBOSE, "Generating signature: %d\n", sigtype);
    fflush(stdout);
    sig = rpmNewSignature();
    rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, passPhrase);
    rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);
    if (sigtype>0) {
	rpmAddSignature(sig, sigtarget, sigtype, passPhrase);
    }
    if (rpmWriteSignature(fd, sig)) {
	close(fd);
	unlink(sigtarget);
	unlink(filename);
	rpmFreeSignature(sig);
	return 1;
    }
    rpmFreeSignature(sig);

    /* Append the header and archive */
    ifd = open(sigtarget, O_RDONLY);
    while ((count = read(ifd, buffer, sizeof(buffer))) > 0) {
        if (count == -1) {
	    perror("Couldn't read sigtarget");
	    close(ifd);
	    close(fd);
	    unlink(sigtarget);
	    unlink(filename);
	    return 1;
        }
        if (write(fd, buffer, count) < 0) {
	    perror("Couldn't write package");
	    close(ifd);
	    close(fd);
	    unlink(sigtarget);
	    unlink(filename);
	    return 1;
        }
    }
    close(ifd);
    close(fd);
    unlink(sigtarget);

    rpmMessage(RPMMESS_VERBOSE, "Wrote: %s\n", filename);
    
    return 0;
}

static int writeMagic(int fd, char *name,
		      unsigned short type)
{
    struct rpmlead lead;
    int arch, os;

    rpmGetArchInfo(NULL, &arch);
    rpmGetOsInfo(NULL, &os);

    /* There are the Major and Minor numbers */
    lead.major = 3;
    lead.minor = 0;
    lead.type = type;
    lead.archnum = arch;
    lead.osnum = os;
    lead.signature_type = RPMSIG_HEADERSIG;  /* New-style signature */
    strncpy(lead.name, name, sizeof(lead.name));

    writeLead(fd, &lead);

    return 0;
}

static int cpio_gzip(int fd, char *tempdir, char *writePtr,
		     int *archiveSize, char *prefix)
{
    int gzipPID;
    int toGzip[2];
    int rc;
    char * gzipbin;
    char * writebuf;
    char * chptr;
    int numMappings;
    struct cpioFileMapping * cpioList;
    int status;
    void *oldhandler;
    char savecwd[1024];
    char * failedFile;

    gzipbin = rpmGetVar(RPMVAR_GZIPBIN);

    numMappings = 0;
    chptr = writePtr;
    while (chptr && *chptr) {
	numMappings++;
	chptr = strchr(chptr, '\n');
	if (chptr) *chptr++ = '\n';
    }

    writebuf = alloca(strlen(writePtr) + 1);
    strcpy(writebuf, writePtr);

    cpioList = alloca(sizeof(*cpioList) * numMappings);
    chptr = writebuf;
    numMappings = 0;
    while (chptr && *chptr) {
	cpioList[numMappings].fsPath = chptr;
	cpioList[numMappings++].mapFlags = tempdir ? CPIO_FOLLOW_SYMLINKS : 0;
	chptr = strchr(chptr, '\n');
	if (chptr) *chptr++ = '\0';
    }
 
    oldhandler = signal(SIGPIPE, SIG_IGN);

    pipe(toGzip);
    
    /* GZIP */
    if (!(gzipPID = fork())) {
	close(toGzip[1]);
	
	dup2(toGzip[0], 0);  /* Make stdin the in pipe       */
	dup2(fd, 1);         /* Make stdout the passed-in fd */

	execlp(gzipbin, gzipbin, "-c9fn", NULL);
	rpmError(RPMERR_EXEC, "Couldn't exec gzip");
	_exit(RPMERR_EXEC);
    }
    if (gzipPID < 0) {
	rpmError(RPMERR_FORK, "Couldn't fork gzip");
	return RPMERR_FORK;
    }

    close(toGzip[0]);

    getcwd(savecwd, 1024);
    
    if (tempdir) {
	chdir(tempdir);
    } else if (rpmGetVar(RPMVAR_ROOT)) {
	if (chdir(rpmGetVar(RPMVAR_ROOT))) {
	    rpmError(RPMERR_EXEC, "Couldn't chdir to %s",
		  rpmGetVar(RPMVAR_ROOT));
	    exit(RPMERR_EXEC);
	}
    } else {
	/* This is important! */
	chdir("/");
    }
    if (prefix) {
	if (chdir(prefix)) {
	    rpmError(RPMERR_EXEC, "Couldn't chdir to %s", prefix);
	    _exit(RPMERR_EXEC);
	}
    }

    rc = cpioBuildArchive(toGzip[1], cpioList, numMappings, NULL, NULL, 
			  archiveSize, &failedFile);

    chdir(savecwd);

    close(toGzip[1]); /* Terminates the gzip process */
    
    signal(SIGPIPE, oldhandler);

    waitpid(gzipPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_GZIP, "gzip failed");
	return 1;
    }

    if (rc) {
	if (rc & CPIO_CHECK_ERRNO)
	    rpmError(RPMERR_CPIO, "cpio failed on file %s: %d: %s", failedFile, 
			rc, strerror(errno));
	else
	    rpmError(RPMERR_CPIO, "cpio failed on file %s: %d", failedFile, rc);
	return 1;
    }

    return 0;
}


int packageBinaries(Spec s, char *passPhrase, int doPackage)
{
    char name[1024];
    char *nametmp;
    char filename[1024];
    char sourcerpm[1024];
    char * binrpm;
    char *icon;
    int iconFD;
    struct stat statbuf;
    struct PackageRec *pr;
    Header outHeader;
    HeaderIterator headerIter;
    int_32 tag, type, c;
    void *ptr;
    char *version;
    char *release;
    char *vendor;
    char *dist;
    char *packager;
    char *packageVersion, *packageRelease;
    char *prefix, *prefixSave;
    char * arch, * os;
    char * binformat, * errorString;
    int prefixLen;
    int size;
    StringBuf cpioFileList;
    char **farray, *file;
    int count;
    
    if (!headerGetEntry(s->packages->header, RPMTAG_VERSION, NULL,
		  (void *) &version, NULL)) {
	rpmError(RPMERR_BADSPEC, "No version field");
	return RPMERR_BADSPEC;
    }
    if (!headerGetEntry(s->packages->header, RPMTAG_RELEASE, NULL,
		  (void *) &release, NULL)) {
	rpmError(RPMERR_BADSPEC, "No release field");
	return RPMERR_BADSPEC;
    }
    /* after the headerGetEntry() these are just pointers into the   */
    /* header structure, which can be moved around by headerAddEntry */
    version = strdup(version);
    release = strdup(release);

    sprintf(sourcerpm, "%s-%s-%s.%ssrc.rpm", s->name, version, release,
	    (s->numNoPatch + s->numNoSource) ? "no" : "");

    vendor = NULL;
    if (!headerIsEntry(s->packages->header, RPMTAG_VENDOR)) {
	vendor = rpmGetVar(RPMVAR_VENDOR);
    }
    dist = NULL;
    if (!headerIsEntry(s->packages->header, RPMTAG_DISTRIBUTION)) {
	dist = rpmGetVar(RPMVAR_DISTRIBUTION);
    }
    packager = NULL;
    if (!headerIsEntry(s->packages->header, RPMTAG_PACKAGER)) {
	packager = rpmGetVar(RPMVAR_PACKAGER);
    }

    /* Look through for each package */
    pr = s->packages;
    while (pr) {
	/* A file count of -1 means no package */
	if (pr->files == -1) {
	    pr = pr->next;
	    continue;
	}

	/* Handle subpackage version/release overrides */
	if (!headerGetEntry(pr->header, RPMTAG_VERSION, NULL,
		      (void *) &packageVersion, NULL)) {
	    packageVersion = version;
	}
	if (!headerGetEntry(pr->header, RPMTAG_RELEASE, NULL,
		      (void *) &packageRelease, NULL)) {
	    packageRelease = release;
	}
        /* after the headerGetEntry() these are just pointers into the   */
        /* header structure, which can be moved around by headerAddEntry */
        packageVersion = strdup(packageVersion);
        packageRelease = strdup(packageRelease);
	
	/* Figure out the name of this package */
	if (!headerGetEntry(pr->header, RPMTAG_NAME, NULL, (void *)&nametmp, NULL)) {
	    rpmError(RPMERR_INTERNAL, "Package has no name!");
	    return RPMERR_INTERNAL;
	}
	sprintf(name, "%s-%s-%s", nametmp, packageVersion, packageRelease);

	if (doPackage == PACK_PACKAGE) {
	    rpmMessage(RPMMESS_VERBOSE, "Binary Packaging: %s\n", name);
	} else {
	    rpmMessage(RPMMESS_VERBOSE, "File List Check: %s\n", name);
	}
       
	/**** Generate the Header ****/
	
	/* Here's the plan: copy the package's header,  */
	/* then add entries from the primary header     */
	/* that don't already exist.                    */
	outHeader = headerCopy(pr->header);
	headerIter = headerInitIterator(s->packages->header);
	while (headerNextIterator(headerIter, &tag, &type, &ptr, &c)) {
	    /* Some tags we don't copy */
	    switch (tag) {
	      case RPMTAG_PREIN:
	      case RPMTAG_POSTIN:
	      case RPMTAG_PREUN:
	      case RPMTAG_POSTUN:
	      case RPMTAG_PREINPROG:
	      case RPMTAG_POSTINPROG:
	      case RPMTAG_PREUNPROG:
	      case RPMTAG_POSTUNPROG:
	      case RPMTAG_VERIFYSCRIPT:
		  continue;
		  break;  /* Shouldn't need this */
	      default:
		  if (! headerIsEntry(outHeader, tag)) {
		      headerAddEntry(outHeader, tag, type, ptr, c);
		  }
	    }
	}
	headerFreeIterator(headerIter);

	headerRemoveEntry(outHeader, RPMTAG_BUILDARCHS);

	rpmGetArchInfo(&arch, NULL);
	rpmGetOsInfo(&os, NULL);
	
	/* Add some final entries to the header */
	headerAddEntry(outHeader, RPMTAG_OS, RPM_STRING_TYPE, os, 1);
	headerAddEntry(outHeader, RPMTAG_ARCH, RPM_STRING_TYPE, arch, 1);
	headerAddEntry(outHeader, RPMTAG_BUILDTIME, RPM_INT32_TYPE, getBuildTime(), 1);
	headerAddEntry(outHeader, RPMTAG_BUILDHOST, RPM_STRING_TYPE, buildHost(), 1);
	headerAddEntry(outHeader, RPMTAG_SOURCERPM, RPM_STRING_TYPE, sourcerpm, 1);
	headerAddEntry(outHeader, RPMTAG_RPMVERSION, RPM_STRING_TYPE, VERSION, 1);
	if (pr->icon) {
	    sprintf(filename, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), pr->icon);
	    stat(filename, &statbuf);
	    icon = malloc(statbuf.st_size);
	    iconFD = open(filename, O_RDONLY, 0644);
	    read(iconFD, icon, statbuf.st_size);
	    close(iconFD);
	    if (! strncmp(icon, "GIF", 3)) {
		headerAddEntry(outHeader, RPMTAG_GIF, RPM_BIN_TYPE,
			 icon, statbuf.st_size);
	    } else if (! strncmp(icon, "/* XPM", 6)) {
		headerAddEntry(outHeader, RPMTAG_XPM, RPM_BIN_TYPE,
			 icon, statbuf.st_size);
	    } else {
	       rpmError(RPMERR_BADSPEC, "Unknown icon type");
	       return 1;
	    }
	    free(icon);
	}
	if (vendor) {
	    headerAddEntry(outHeader, RPMTAG_VENDOR, RPM_STRING_TYPE, vendor, 1);
	}
	if (dist) {
	    headerAddEntry(outHeader, RPMTAG_DISTRIBUTION, RPM_STRING_TYPE, dist, 1);
	}
	if (packager) {
	    headerAddEntry(outHeader, RPMTAG_PACKAGER, RPM_STRING_TYPE, packager, 1);
	}
	
	/**** Process the file list ****/

	prefixSave = prefix = NULL;
	prefixLen = 0;
	if (headerGetEntry(outHeader, RPMTAG_DEFAULTPREFIX,
		     NULL, (void **)&prefix, NULL)) {
            /* after the headerGetEntry() this is just pointers into the     */
            /* header structure, which can be moved around by headerAddEntry */
	    prefixSave = prefix = strdup(prefix);
	    while (*prefix && (*prefix == '/')) {
		prefix++;
	    }
	    if (! *prefix) {
		prefix = NULL;
		prefixLen = 0;
	    } else {
		prefixLen = strlen(prefix);
	        rpmMessage(RPMMESS_VERBOSE, "Package Prefix = %s\n", prefix);
	    }
	}
	
	if (process_filelist(outHeader, pr, pr->filelist, &size, nametmp,
			     packageVersion, packageRelease, RPMLEAD_BINARY,
			     prefix, NULL)) {
	    return 1;
	}

	if (!headerGetEntry(outHeader, RPMTAG_FILENAMES, NULL, (void **) &farray,
		      &count)) {
	    /* count may already be 0, but this is safer */
	    count = 0;
	}

	cpioFileList = newStringBuf();
	while (count--) {
	    file = *farray++;
	    while (*file == '/') {
		file++;  /* Skip leading "/" */
	    }
	    if (prefix) {
		if (strncmp(prefix, file, prefixLen)) {
		    rpmError(RPMERR_BADSPEC, "File doesn't match prefix (%s): %s",
			  prefix, file);
		    return 1;
		}
		file += prefixLen + 1; /* 1 for "/" */
	    }
	    appendLineStringBuf(cpioFileList, file);
	}
	
	/* Generate any automatic require/provide entries */
	/* Then add the whole thing to the header         */
	if (s->autoReqProv) {
	    generateAutoReqProv(outHeader, pr);
	}
	processReqProv(outHeader, pr);

	/* Generate the any trigger entries */
	generateTriggerEntries(outHeader, pr);
	
	/* And add the final Header entry */
	headerAddEntry(outHeader, RPMTAG_SIZE, RPM_INT32_TYPE, &size, 1);

	/**** Make the RPM ****/

	/* Make the output RPM filename */
	if (doPackage == PACK_PACKAGE) {
	    binformat = rpmGetVar(RPMVAR_RPMFILENAME);
	    binrpm = headerSprintf(outHeader, binformat, rpmTagTable,
				   rpmHeaderFormats, &errorString);

	    if (!binrpm) {
		rpmError(RPMERR_BADFILENAME, "could not generate output "
			 "filename for package %s: %s\n", name, binrpm);
	    }

	    sprintf(filename, "%s/%s", rpmGetVar(RPMVAR_RPMDIR), binrpm);
	    free(binrpm);

	    if (generateRPM(name, filename, RPMLEAD_BINARY, outHeader, NULL,
			    getStringBuf(cpioFileList), passPhrase, prefix)) {
		/* Build failed */
		return 1;
	    }
	}

	freeStringBuf(cpioFileList);
	headerFree(outHeader);
       
        if (prefixSave)
	    free(prefixSave);
        free(packageVersion);
        free(packageRelease);
       
	pr = pr->next;
    }

    free(version);
    free(release);
   
    return 0;
}

/**************** SOURCE PACKAGING ************************/

int packageSource(Spec s, char *passPhrase)
{
    struct sources *source;
    struct PackageRec *package;
    char *tempdir;
    char src[1024], dest[1024], fullname[1024], filename[1024], specFile[1024];
    char *version;
    char *release;
    char *vendor;
    char *dist;
    char *p;
    Header outHeader;
    StringBuf filelist;
    StringBuf cpioFileList;
    int size;
    char **sources;
    char **patches;
    char * arch, * os;
    int scount, pcount;
    int skipi;
    int_32 *skip;

    /**** Create links for all the sources ****/
    
    tempdir = tempnam(rpmGetVar(RPMVAR_TMPPATH), "rpmbuild");
    mkdir(tempdir, 0700);

    filelist = newStringBuf();     /* List in the header */
    cpioFileList = newStringBuf(); /* List for cpio      */

    sources = malloc((s->numSources+1) * sizeof(char *));
    patches = malloc((s->numPatches+1) * sizeof(char *));
    
    /* Link in the spec file and all the sources */
    p = strrchr(s->specfile, '/');
    sprintf(dest, "%s%s", tempdir, p);
    strcpy(specFile, dest);
    symlink(s->specfile, dest);
    appendLineStringBuf(filelist, dest);
    appendLineStringBuf(cpioFileList, p+1);
    source = s->sources;
    scount = 0;
    pcount = 0;
    while (source) {
	if (source->ispatch) {
	    skipi = s->numNoPatch - 1;
	    skip = s->noPatch;
	} else {
	    skipi = s->numNoSource - 1;
	    skip = s->noSource;
	}
	while (skipi >= 0) {
	    if (skip[skipi] == source->num) {
		break;
	    }
	    skipi--;
	}
	sprintf(src, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), source->source);
	sprintf(dest, "%s/%s", tempdir, source->source);
	if (skipi < 0) {
	    symlink(src, dest);
	    appendLineStringBuf(cpioFileList, source->source);
	} else {
	    rpmMessage(RPMMESS_VERBOSE, "Skipping source/patch (%d): %s\n",
		    source->num, source->source);
	}
	appendLineStringBuf(filelist, src);
	if (source->ispatch) {
	    patches[pcount++] = source->fullSource;
	} else {
	    sources[scount++] = source->fullSource;
	}
	source = source->next;
    }
    /* ... and icons */
    package = s->packages;
    while (package) {
	if (package->icon) {
	    sprintf(src, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), package->icon);
	    sprintf(dest, "%s/%s", tempdir, package->icon);
	    appendLineStringBuf(filelist, dest);
	    appendLineStringBuf(cpioFileList, package->icon);
	    symlink(src, dest);
	}
	package = package->next;
    }

    /**** Generate the Header ****/
    
    if (!headerGetEntry(s->packages->header, RPMTAG_VERSION, NULL,
		  (void *) &version, NULL)) {
	rpmError(RPMERR_BADSPEC, "No version field");
	return RPMERR_BADSPEC;
    }
    if (!headerGetEntry(s->packages->header, RPMTAG_RELEASE, NULL,
		  (void *) &release, NULL)) {
	rpmError(RPMERR_BADSPEC, "No release field");
	return RPMERR_BADSPEC;
    }

    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);

    outHeader = headerCopy(s->packages->header);
    headerAddEntry(outHeader, RPMTAG_OS, RPM_STRING_TYPE, os, 1);
    headerAddEntry(outHeader, RPMTAG_ARCH, RPM_STRING_TYPE, arch, 1);
    headerAddEntry(outHeader, RPMTAG_BUILDTIME, RPM_INT32_TYPE, getBuildTime(), 1);
    headerAddEntry(outHeader, RPMTAG_BUILDHOST, RPM_STRING_TYPE, buildHost(), 1);
    headerAddEntry(outHeader, RPMTAG_RPMVERSION, RPM_STRING_TYPE, VERSION, 1);
    if (scount) 
        headerAddEntry(outHeader, RPMTAG_SOURCE, RPM_STRING_ARRAY_TYPE, sources, scount);
    if (pcount)
        headerAddEntry(outHeader, RPMTAG_PATCH, RPM_STRING_ARRAY_TYPE, patches, pcount);
    if (s->numNoSource) {
	headerAddEntry(outHeader, RPMTAG_NOSOURCE, RPM_INT32_TYPE, s->noSource,
		 s->numNoSource);
    }
    if (s->numNoPatch) {
	headerAddEntry(outHeader, RPMTAG_NOPATCH, RPM_INT32_TYPE, s->noPatch,
		 s->numNoPatch);
    }
    if (!headerIsEntry(s->packages->header, RPMTAG_VENDOR)) {
	if ((vendor = rpmGetVar(RPMVAR_VENDOR))) {
	    headerAddEntry(outHeader, RPMTAG_VENDOR, RPM_STRING_TYPE, vendor, 1);
	}
    }
    if (!headerIsEntry(s->packages->header, RPMTAG_DISTRIBUTION)) {
	if ((dist = rpmGetVar(RPMVAR_DISTRIBUTION))) {
	    headerAddEntry(outHeader, RPMTAG_DISTRIBUTION, RPM_STRING_TYPE, dist, 1);
	}
    }

    /* Process the file list */
    if (process_filelist(outHeader, NULL, filelist, &size,
			 s->name, version, release, RPMLEAD_SOURCE,
			 NULL, specFile)) {
	return 1;
    }

    /* And add the final Header entry */
    headerAddEntry(outHeader, RPMTAG_SIZE, RPM_INT32_TYPE, &size, 1);

    /**** Make the RPM ****/

    sprintf(fullname, "%s-%s-%s", s->name, version, release);
    sprintf(filename, "%s/%s.%ssrc.rpm", rpmGetVar(RPMVAR_SRPMDIR), fullname,
	    (s->numNoPatch + s->numNoSource) ? "no" : "");
    rpmMessage(RPMMESS_VERBOSE, "Source Packaging: %s\n", fullname);

    if (generateRPM(fullname, filename, RPMLEAD_SOURCE, outHeader,
		    tempdir, getStringBuf(cpioFileList), passPhrase, NULL)) {
	return 1;
    }
    
    /**** Now clean up ****/

    freeStringBuf(filelist);
    freeStringBuf(cpioFileList);
    
    source = s->sources;
    while (source) {
	sprintf(dest, "%s/%s", tempdir, source->source);
	unlink(dest);
	source = source->next;
    }
    package = s->packages;
    while (package) {
	if (package->icon) {
	    sprintf(dest, "%s/%s", tempdir, package->icon);
	    unlink(dest);
	}
	package = package->next;
    }
    sprintf(dest, "%s%s", tempdir, strrchr(s->specfile, '/'));
    unlink(dest);
    rmdir(tempdir);
    
    return 0;
}
