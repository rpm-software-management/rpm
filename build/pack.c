/* The very final packaging steps */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "pack.h"
#include "header.h"
#include "spec.h"
#include "specP.h"
#include "signature.h"
#include "rpmerr.h"
#include "rpmlead.h"
#include "rpmlib.h"
#include "misc.h"
#include "stringbuf.h"
#include "names.h"
#include "files.h"
#include "reqprov.h"
#include "trigger.h"

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
    char *sigtarget, *archiveTemp;
    int fd, ifd, count, archiveSize;
    unsigned char buffer[8192];
    Header sig;

    /* Write the archive to a temp file so we can get the size */
    archiveTemp = tempnam(getVar(RPMVAR_TMPPATH), "rpmbuild");
    if ((fd = open(archiveTemp, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
	fprintf(stderr, "Could not open %s\n", archiveTemp);
	return 1;
    }
    if (cpio_gzip(fd, stempdir, fileList, &archiveSize, prefix)) {
	close(fd);
	unlink(archiveTemp);
	return 1;
    }
    close(fd);

    /* Add the archive size to the Header */
    addEntry(header, RPMTAG_ARCHIVESIZE, INT32_TYPE, &archiveSize, 1);
    
    /* Now write the header and append the archive */
    sigtarget = tempnam(getVar(RPMVAR_TMPPATH), "rpmbuild");
    if ((fd = open(sigtarget, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
	fprintf(stderr, "Could not open %s\n", sigtarget);
	unlink(archiveTemp);
	return 1;
    }
    writeHeader(fd, header, HEADER_MAGIC);
    ifd = open(archiveTemp, O_RDONLY, 0644);
    while ((count = read(ifd, buffer, sizeof(buffer))) > 0) {
        if (count == -1) {
	    perror("Couldn't read archiveTemp");
	    close(fd);
	    close(ifd);
	    unlink(archiveTemp);
	    unlink(sigtarget);
	    return 1;
        }
        if (write(fd, buffer, count) < 0) {
	    perror("Couldn't write package to temp file");
	    close(fd);
	    close(ifd);
	    unlink(archiveTemp);
	    unlink(sigtarget);
	    return 1;
        }
    }
    close(ifd);
    close(fd);
    unlink(archiveTemp);

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
    sigtype = sigLookupType();
    message(MESS_VERBOSE, "Generating signature: %d\n", sigtype);
    fflush(stdout);
    sig = newSignature();
    addSignature(sig, sigtarget, SIGTAG_SIZE, passPhrase);
    addSignature(sig, sigtarget, SIGTAG_MD5, passPhrase);
    if (sigtype>0) {
	addSignature(sig, sigtarget, sigtype, passPhrase);
    }
    if (writeSignature(fd, sig)) {
	close(fd);
	unlink(sigtarget);
	unlink(filename);
	freeSignature(sig);
	return 1;
    }
    freeSignature(sig);

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

    message(MESS_VERBOSE, "Wrote: %s\n", filename);
    
    return 0;
}

static int writeMagic(int fd, char *name,
		      unsigned short type)
{
    struct rpmlead lead;

    /* There are the Major and Minor numbers */
    lead.major = 3;
    lead.minor = 0;
    lead.type = type;
    lead.archnum = getArchNum();
    lead.osnum = getOsNum();
    lead.signature_type = RPMSIG_HEADERSIG;  /* New-style signature */
    strncpy(lead.name, name, sizeof(lead.name));

    writeLead(fd, &lead);

    return 0;
}

static int cpio_gzip(int fd, char *tempdir, char *writePtr,
		     int *archiveSize, char *prefix)
{
    int cpioPID, gzipPID;
    int cpioDead, gzipDead;
    int toCpio[2];
    int fromCpio[2];
    int toGzip[2];
    char * cpiobin;

    int writeBytesLeft, bytesWritten;

    int bytes;
    unsigned char buf[8192];

    int status;
    void *oldhandler;

    cpiobin = getVar(RPMVAR_CPIOBIN);
    if (!cpiobin) cpiobin = "cpio";
 
    *archiveSize = 0;
    
    pipe(toCpio);
    pipe(fromCpio);
    
    oldhandler = signal(SIGPIPE, SIG_IGN);

    /* CPIO */
    if (!(cpioPID = fork())) {
	close(0);
	close(1);
	close(toCpio[1]);
	close(fromCpio[0]);
	close(fd);
	
	dup2(toCpio[0], 0);   /* Make stdin the in pipe */
	dup2(fromCpio[1], 1); /* Make stdout the out pipe */

	if (tempdir) {
	    chdir(tempdir);
	} else if (getVar(RPMVAR_ROOT)) {
	    if (chdir(getVar(RPMVAR_ROOT))) {
		error(RPMERR_EXEC, "Couldn't chdir to %s",
		      getVar(RPMVAR_ROOT));
		exit(RPMERR_EXEC);
	    }
	} else {
	    /* This is important! */
	    chdir("/");
	}
	if (prefix) {
	    if (chdir(prefix)) {
		error(RPMERR_EXEC, "Couldn't chdir to %s", prefix);
		exit(RPMERR_EXEC);
	    }
	}

	execlp(cpiobin, cpiobin,
	       (isVerbose()) ? "-ov" : "-o",
	       (tempdir) ? "-LH" : "-H",
	       "crc", NULL);
	error(RPMERR_EXEC, "Couldn't exec cpio");
	exit(RPMERR_EXEC);
    }
    if (cpioPID < 0) {
	error(RPMERR_FORK, "Couldn't fork cpio");
	return RPMERR_FORK;
    }

    pipe(toGzip);
    
    /* GZIP */
    if (!(gzipPID = fork())) {
	close(0);
	close(1);
	close(toGzip[1]);
	close(toCpio[0]);
	close(toCpio[1]);
	close(fromCpio[0]);
	close(fromCpio[1]);
	
	dup2(toGzip[0], 0);  /* Make stdin the in pipe       */
	dup2(fd, 1);         /* Make stdout the passed-in fd */

	execlp("gzip", "gzip", "-c9fn", NULL);
	error(RPMERR_EXEC, "Couldn't exec gzip");
	exit(RPMERR_EXEC);
    }
    if (gzipPID < 0) {
	error(RPMERR_FORK, "Couldn't fork gzip");
	return RPMERR_FORK;
    }

    close(toCpio[0]);
    close(fromCpio[1]);
    close(toGzip[0]);

    /* It is OK to block writing to gzip.  But it is not OK */
    /* to block reading or writing from/to cpio.            */
    fcntl(fromCpio[0], F_SETFL, O_NONBLOCK);
    fcntl(toCpio[1], F_SETFL, O_NONBLOCK);

    writeBytesLeft = strlen(writePtr);
    
    cpioDead = 0;
    gzipDead = 0;
    do {
	if (waitpid(cpioPID, &status, WNOHANG)) {
	    cpioDead = 1;
	}
	if (waitpid(gzipPID, &status, WNOHANG)) {
	    gzipDead = 1;
	}

	/* Write some stuff to the cpio process if possible */
        if (writeBytesLeft) {
	    if ((bytesWritten =
		  write(toCpio[1], writePtr,
		    (1024<writeBytesLeft) ? 1024 : writeBytesLeft)) < 0) {
	        if (errno != EAGAIN) {
		    perror("Damn!");
	            exit(1);
		}
	        bytesWritten = 0;
	    }
	    writeBytesLeft -= bytesWritten;
	    writePtr += bytesWritten;
	} else {
	    close(toCpio[1]);
	}
	
	/* Read any data from cpio, write it to gzip */
	bytes = read(fromCpio[0], buf, sizeof(buf));
	while (bytes > 0) {
	    *archiveSize += bytes;
	    write(toGzip[1], buf, bytes);
	    bytes = read(fromCpio[0], buf, sizeof(buf));
	}

	/* while cpio is running, or we are writing to gzip */
	/* terminate if gzip dies on us in the middle       */
    } while (((!cpioDead) || bytes) && (!gzipDead));

    if (gzipDead) {
	error(RPMERR_GZIP, "gzip died");
	return 1;
    }
    
    close(toGzip[1]); /* Terminates the gzip process */
    close(toCpio[1]);
    close(fromCpio[0]);
    
    signal(SIGPIPE, oldhandler);

    if (writeBytesLeft) {
	error(RPMERR_CPIO, "failed to write all data to cpio");
	return 1;
    }
    waitpid(cpioPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_CPIO, "cpio failed");
	return 1;
    }
    waitpid(gzipPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_GZIP, "gzip failed");
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
    int prefixLen;
    int size;
    StringBuf cpioFileList;
    char **farray, *file;
    int count;
    
    if (!getEntry(s->packages->header, RPMTAG_VERSION, NULL,
		  (void *) &version, NULL)) {
	error(RPMERR_BADSPEC, "No version field");
	return RPMERR_BADSPEC;
    }
    if (!getEntry(s->packages->header, RPMTAG_RELEASE, NULL,
		  (void *) &release, NULL)) {
	error(RPMERR_BADSPEC, "No release field");
	return RPMERR_BADSPEC;
    }
    /* after the getEntry() these are just pointers into the   */
    /* header structure, which can be moved around by addEntry */
    version = strdup(version);
    release = strdup(release);

    sprintf(sourcerpm, "%s-%s-%s.%ssrc.rpm", s->name, version, release,
	    (s->numNoPatch + s->numNoSource) ? "no" : "");

    vendor = NULL;
    if (!isEntry(s->packages->header, RPMTAG_VENDOR)) {
	vendor = getVar(RPMVAR_VENDOR);
    }
    dist = NULL;
    if (!isEntry(s->packages->header, RPMTAG_DISTRIBUTION)) {
	dist = getVar(RPMVAR_DISTRIBUTION);
    }
    packager = NULL;
    if (!isEntry(s->packages->header, RPMTAG_PACKAGER)) {
	packager = getVar(RPMVAR_PACKAGER);
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
	if (!getEntry(pr->header, RPMTAG_VERSION, NULL,
		      (void *) &packageVersion, NULL)) {
	    packageVersion = version;
	}
	if (!getEntry(pr->header, RPMTAG_RELEASE, NULL,
		      (void *) &packageRelease, NULL)) {
	    packageRelease = release;
	}
        /* after the getEntry() these are just pointers into the   */
        /* header structure, which can be moved around by addEntry */
        packageVersion = strdup(packageVersion);
        packageRelease = strdup(packageRelease);
	
	/* Figure out the name of this package */
	if (!getEntry(pr->header, RPMTAG_NAME, NULL, (void *)&nametmp, NULL)) {
	    error(RPMERR_INTERNAL, "Package has no name!");
	    return RPMERR_INTERNAL;
	}
	sprintf(name, "%s-%s-%s", nametmp, packageVersion, packageRelease);

	if (doPackage == PACK_PACKAGE) {
	    message(MESS_VERBOSE, "Binary Packaging: %s\n", name);
	} else {
	    message(MESS_VERBOSE, "File List Check: %s\n", name);
	}
       
	/**** Generate the Header ****/
	
	/* Here's the plan: copy the package's header,  */
	/* then add entries from the primary header     */
	/* that don't already exist.                    */
	outHeader = copyHeader(pr->header);
	headerIter = initIterator(s->packages->header);
	while (nextIterator(headerIter, &tag, &type, &ptr, &c)) {
	    /* Some tags we don't copy */
	    switch (tag) {
	      case RPMTAG_PREIN:
	      case RPMTAG_POSTIN:
	      case RPMTAG_PREUN:
	      case RPMTAG_POSTUN:
		  continue;
		  break;  /* Shouldn't need this */
	      default:
		  if (! isEntry(outHeader, tag)) {
		      addEntry(outHeader, tag, type, ptr, c);
		  }
	    }
	}
	freeIterator(headerIter);
	
	/* Add some final entries to the header */
	addEntry(outHeader, RPMTAG_OS, STRING_TYPE, getOsName(), 1);
	addEntry(outHeader, RPMTAG_ARCH, STRING_TYPE, getArchName(), 1);
	addEntry(outHeader, RPMTAG_BUILDTIME, INT32_TYPE, getBuildTime(), 1);
	addEntry(outHeader, RPMTAG_BUILDHOST, STRING_TYPE, buildHost(), 1);
	addEntry(outHeader, RPMTAG_SOURCERPM, STRING_TYPE, sourcerpm, 1);
	addEntry(outHeader, RPMTAG_RPMVERSION, STRING_TYPE, VERSION, 1);
	if (pr->icon) {
	    sprintf(filename, "%s/%s", getVar(RPMVAR_SOURCEDIR), pr->icon);
	    stat(filename, &statbuf);
	    icon = malloc(statbuf.st_size);
	    iconFD = open(filename, O_RDONLY, 0644);
	    read(iconFD, icon, statbuf.st_size);
	    close(iconFD);
	    if (! strncmp(icon, "GIF", 3)) {
		addEntry(outHeader, RPMTAG_GIF, BIN_TYPE,
			 icon, statbuf.st_size);
	    } else if (! strncmp(icon, "/* XPM", 6)) {
		addEntry(outHeader, RPMTAG_XPM, BIN_TYPE,
			 icon, statbuf.st_size);
	    } else {
	       error(RPMERR_BADSPEC, "Unknown icon type");
	       return 1;
	    }
	    free(icon);
	}
	if (vendor) {
	    addEntry(outHeader, RPMTAG_VENDOR, STRING_TYPE, vendor, 1);
	}
	if (dist) {
	    addEntry(outHeader, RPMTAG_DISTRIBUTION, STRING_TYPE, dist, 1);
	}
	if (packager) {
	    addEntry(outHeader, RPMTAG_PACKAGER, STRING_TYPE, packager, 1);
	}
	
	/**** Process the file list ****/

	prefixSave = prefix = NULL;
	prefixLen = 0;
	if (getEntry(outHeader, RPMTAG_DEFAULTPREFIX,
		     NULL, (void **)&prefix, NULL)) {
            /* after the getEntry() this is just pointers into the     */
            /* header structure, which can be moved around by addEntry */
	    prefixSave = prefix = strdup(prefix);
	    while (*prefix && (*prefix == '/')) {
		prefix++;
	    }
	    if (! *prefix) {
		prefix = NULL;
		prefixLen = 0;
	    } else {
		prefixLen = strlen(prefix);
	        message(MESS_VERBOSE, "Package Prefix = %s\n", prefix);
	    }
	}
	
	if (process_filelist(outHeader, pr, pr->filelist, &size, nametmp,
			     packageVersion, packageRelease, RPMLEAD_BINARY,
			     prefix)) {
	    return 1;
	}

	if (!getEntry(outHeader, RPMTAG_FILENAMES, NULL, (void **) &farray,
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
		    error(RPMERR_BADSPEC, "File doesn't match prefix (%s): %s",
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
	addEntry(outHeader, RPMTAG_SIZE, INT32_TYPE, &size, 1);

	/**** Make the RPM ****/

	/* Make the output RPM filename */
	if (doPackage == PACK_PACKAGE) {
	    sprintf(filename, "%s/%s/%s.%s.rpm", getVar(RPMVAR_RPMDIR),
		    getArchName(), name, getArchName());

	    if (generateRPM(name, filename, RPMLEAD_BINARY, outHeader, NULL,
			    getStringBuf(cpioFileList), passPhrase, prefix)) {
		/* Build failed */
		return 1;
	    }
	}

	freeStringBuf(cpioFileList);
	freeHeader(outHeader);
       
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
    char src[1024], dest[1024], fullname[1024], filename[1024];
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
    int scount, pcount;
    int skipi;
    int_32 *skip;

    /**** Create links for all the sources ****/
    
    tempdir = tempnam(getVar(RPMVAR_TMPPATH), "rpmbuild");
    mkdir(tempdir, 0700);

    filelist = newStringBuf();     /* List in the header */
    cpioFileList = newStringBuf(); /* List for cpio      */

    sources = malloc((s->numSources+1) * sizeof(char *));
    patches = malloc((s->numPatches+1) * sizeof(char *));
    
    /* Link in the spec file and all the sources */
    p = strrchr(s->specfile, '/');
    sprintf(dest, "%s%s", tempdir, p);
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
	sprintf(src, "%s/%s", getVar(RPMVAR_SOURCEDIR), source->source);
	sprintf(dest, "%s/%s", tempdir, source->source);
	if (skipi < 0) {
	    symlink(src, dest);
	    appendLineStringBuf(cpioFileList, source->source);
	} else {
	    message(MESS_VERBOSE, "Skipping source/patch (%d): %s\n",
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
	    sprintf(src, "%s/%s", getVar(RPMVAR_SOURCEDIR), package->icon);
	    sprintf(dest, "%s/%s", tempdir, package->icon);
	    appendLineStringBuf(filelist, dest);
	    appendLineStringBuf(cpioFileList, package->icon);
	    symlink(src, dest);
	}
	package = package->next;
    }

    /**** Generate the Header ****/
    
    if (!getEntry(s->packages->header, RPMTAG_VERSION, NULL,
		  (void *) &version, NULL)) {
	error(RPMERR_BADSPEC, "No version field");
	return RPMERR_BADSPEC;
    }
    if (!getEntry(s->packages->header, RPMTAG_RELEASE, NULL,
		  (void *) &release, NULL)) {
	error(RPMERR_BADSPEC, "No release field");
	return RPMERR_BADSPEC;
    }

    outHeader = copyHeader(s->packages->header);
    addEntry(outHeader, RPMTAG_OS, STRING_TYPE, getOsName(), 1);
    addEntry(outHeader, RPMTAG_ARCH, STRING_TYPE, getArchName(), 1);
    addEntry(outHeader, RPMTAG_BUILDTIME, INT32_TYPE, getBuildTime(), 1);
    addEntry(outHeader, RPMTAG_BUILDHOST, STRING_TYPE, buildHost(), 1);
    addEntry(outHeader, RPMTAG_RPMVERSION, STRING_TYPE, VERSION, 1);
    if (scount) 
        addEntry(outHeader, RPMTAG_SOURCE, STRING_ARRAY_TYPE, sources, scount);
    if (pcount)
        addEntry(outHeader, RPMTAG_PATCH, STRING_ARRAY_TYPE, patches, pcount);
    if (s->numNoSource) {
	addEntry(outHeader, RPMTAG_NOSOURCE, INT32_TYPE, s->noSource,
		 s->numNoSource);
    }
    if (s->numNoPatch) {
	addEntry(outHeader, RPMTAG_NOPATCH, INT32_TYPE, s->noPatch,
		 s->numNoPatch);
    }
    if (!isEntry(s->packages->header, RPMTAG_VENDOR)) {
	if ((vendor = getVar(RPMVAR_VENDOR))) {
	    addEntry(outHeader, RPMTAG_VENDOR, STRING_TYPE, vendor, 1);
	}
    }
    if (!isEntry(s->packages->header, RPMTAG_DISTRIBUTION)) {
	if ((dist = getVar(RPMVAR_DISTRIBUTION))) {
	    addEntry(outHeader, RPMTAG_DISTRIBUTION, STRING_TYPE, dist, 1);
	}
    }

    /* Process the file list */
    if (process_filelist(outHeader, NULL, filelist, &size,
			 s->name, version, release, RPMLEAD_SOURCE,
			 NULL)) {
	return 1;
    }

    /* And add the final Header entry */
    addEntry(outHeader, RPMTAG_SIZE, INT32_TYPE, &size, 1);

    /**** Make the RPM ****/

    sprintf(fullname, "%s-%s-%s", s->name, version, release);
    sprintf(filename, "%s/%s.%ssrc.rpm", getVar(RPMVAR_SRPMDIR), fullname,
	    (s->numNoPatch + s->numNoSource) ? "no" : "");
    message(MESS_VERBOSE, "Source Packaging: %s\n", fullname);

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
