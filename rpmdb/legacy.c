/**
 * \file rpmdb/legacy.c
 */

#include "system.h"

#if HAVE_LIBELF_GELF_H
#define	__LIBELF_INTERNAL__	1
#  undef __P
#  define __P(protos)   protos

#include <gelf.h>

#if !defined(DT_GNU_PRELINKED)
#define	DT_GNU_PRELINKED	0x6ffffdf5
#endif
#if !defined(DT_GNU_LIBLIST)
#define	DT_GNU_LIBLIST		0x6ffffef9
#endif

#endif

#include "rpmio_internal.h"
#include <rpmlib.h>
#include <rpmmacro.h>
#include "misc.h"
#include "legacy.h"
#include "debug.h"

#define alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

/**
 * Open a file descriptor to verify file MD5 and size.
 * @param path		file path
 * @retval pidp		prelink helper pid or 0
 * @retval fsizep	file size
 * @return		-1 on error, otherwise, an open file descriptor
 */ 
static int open_dso(const char * path, /*@null@*/ pid_t * pidp, /*@null@*/ size_t *fsizep)
	/*@globals rpmGlobalMacroContext, internalState @*/
	/*@modifies *pidp, *fsizep, rpmGlobalMacroContext, internalState @*/
{
/*@only@*/
    static const char * cmd = NULL;
    static int initted = 0;
    int fdno;

    if (!initted) {
	cmd = rpmExpand("%{?__prelink_undo_cmd}", NULL);
	initted++;
    }

/*@-boundswrite@*/
    if (pidp) *pidp = 0;

    if (fsizep) {
	struct stat sb, * st = &sb;
	if (stat(path, st) < 0)
	    return -1;
	*fsizep = st->st_size;
    }
/*@=boundswrite@*/

    fdno = open(path, O_RDONLY);
    if (fdno < 0)
	return fdno;

/*@-boundsread@*/
    if (!(cmd && *cmd))
	return fdno;
/*@=boundsread@*/

#if HAVE_LIBELF_GELF_H && HAVE_LIBELF
 {  Elf *elf = NULL;
    Elf_Scn *scn = NULL;
    Elf_Data *data = NULL;
    GElf_Ehdr ehdr;
    GElf_Shdr shdr;
    GElf_Dyn dyn;
    int bingo;

    (void) elf_version(EV_CURRENT);

    if ((elf = elf_begin (fdno, ELF_C_READ, NULL)) == NULL
     || elf_kind(elf) != ELF_K_ELF
     || gelf_getehdr(elf, &ehdr) == NULL
     || !(ehdr.e_type == ET_DYN || ehdr.e_type == ET_EXEC))
	goto exit;

    bingo = 0;
    /*@-branchstate -uniondef @*/
    while (!bingo && (scn = elf_nextscn(elf, scn)) != NULL) {
	(void) gelf_getshdr(scn, &shdr);
	if (shdr.sh_type != SHT_DYNAMIC)
	    continue;
	while (!bingo && (data = elf_getdata (scn, data)) != NULL) {
	    int maxndx = data->d_size / shdr.sh_entsize;
	    int ndx;

            for (ndx = 0; ndx < maxndx; ++ndx) {
		(void) gelf_getdyn (data, ndx, &dyn);
		if (!(dyn.d_tag == DT_GNU_PRELINKED || dyn.d_tag == DT_GNU_LIBLIST))
		    /*@innercontinue@*/ continue;
		bingo = 1;
		/*@innerbreak@*/ break;
	    }
	}
    }
    /*@=branchstate =uniondef @*/

/*@-boundswrite@*/
    if (pidp != NULL && bingo) {
	int pipes[2];
	pid_t pid;
	int xx;

	xx = close(fdno);
	pipes[0] = pipes[1] = -1;
	xx = pipe(pipes);
	if (!(pid = fork())) {
	    const char ** av;
	    int ac;
	    xx = close(pipes[0]);
	    xx = dup2(pipes[1], STDOUT_FILENO);
	    xx = close(pipes[1]);
	    if (!poptParseArgvString(cmd, &ac, &av)) {
		av[ac-1] = path;
		av[ac] = NULL;
		unsetenv("MALLOC_CHECK_");
		xx = execve(av[0], (char *const *)av+1, environ);
	    }
	    _exit(127);
	}
	*pidp = pid;
	fdno = pipes[0];
	xx = close(pipes[1]);
    }
/*@=boundswrite@*/

exit:
    if (elf) (void) elf_end(elf);
 }
#endif

    return fdno;
}

int domd5(const char * fn, unsigned char * digest, int asAscii, size_t *fsizep)
{
    const char * path;
    urltype ut = urlPath(fn, &path);
    unsigned char * md5sum = NULL;
    size_t md5len;
    unsigned char buf[32*BUFSIZ];
    FD_t fd;
    size_t fsize = 0;
    pid_t pid = 0;
    int rc = 0;
    int fdno;
    int xx;

/*@-globs -internalglobs -mods @*/
    fdno = open_dso(path, &pid, &fsize);
/*@=globs =internalglobs =mods @*/
    if (fdno < 0) {
	rc = 1;
	goto exit;
    }

    switch(ut) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
#if HAVE_MMAP
      if (pid == 0) {
	DIGEST_CTX ctx;
	void * mapped;

	mapped = mmap(NULL, fsize, PROT_READ, MAP_SHARED, fdno, 0);
	if (mapped == (void *)-1) {
	    xx = close(fdno);
	    rc = 1;
	    break;
	}

#ifdef	MADV_SEQUENTIAL
        xx = madvise(mapped, fsize, MADV_SEQUENTIAL);
#endif

	ctx = rpmDigestInit(PGPHASHALGO_MD5, RPMDIGEST_NONE);
	xx = rpmDigestUpdate(ctx, mapped, fsize);
	xx = rpmDigestFinal(ctx, (void **)&md5sum, &md5len, asAscii);
	xx = munmap(mapped, fsize);
	xx = close(fdno);
	break;
      }	/*@fallthrough@*/
#endif
    case URL_IS_FTP:
    case URL_IS_HTTP:
    case URL_IS_DASH:
    default:
	/* Either use the pipe to prelink -y or open the URL. */
	fd = (pid != 0) ? fdDup(fdno) : Fopen(fn, "r.ufdio");
	(void) close(fdno);
	if (fd == NULL || Ferror(fd)) {
	    rc = 1;
	    if (fd != NULL)
		(void) Fclose(fd);
	    break;
	}
	
	fdInitDigest(fd, PGPHASHALGO_MD5, 0);
	fsize = 0;
	while ((rc = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	    fsize += rc;
	fdFiniDigest(fd, PGPHASHALGO_MD5, (void **)&md5sum, &md5len, asAscii);
	if (Ferror(fd))
	    rc = 1;

	(void) Fclose(fd);
	break;
    }

    /* Reap the prelink -y helper. */
    if (pid) {
	int status;
	(void) waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status))
	    rc = 1;
    }

exit:
/*@-boundswrite@*/
    if (fsizep)
	*fsizep = fsize;
    if (!rc)
	memcpy(digest, md5sum, md5len);
/*@=boundswrite@*/
    md5sum = _free(md5sum);

    return rc;
}

/*@-exportheadervar@*/
/*@unchecked@*/
int _noDirTokens = 0;
/*@=exportheadervar@*/

/*@-boundsread@*/
static int dncmp(const void * a, const void * b)
	/*@*/
{
    const char *const * first = a;
    const char *const * second = b;
    return strcmp(*first, *second);
}
/*@=boundsread@*/

/*@-bounds@*/
void compressFilelist(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    HFD_t hfd = headerFreeData;
    char ** fileNames;
    const char ** dirNames;
    const char ** baseNames;
    int_32 * dirIndexes;
    rpmTagType fnt;
    int count;
    int i, xx;
    int dirIndex = -1;

    /*
     * This assumes the file list is already sorted, and begins with a
     * single '/'. That assumption isn't critical, but it makes things go
     * a bit faster.
     */

    if (headerIsEntry(h, RPMTAG_DIRNAMES)) {
	xx = hre(h, RPMTAG_OLDFILENAMES);
	return;		/* Already converted. */
    }

    if (!hge(h, RPMTAG_OLDFILENAMES, &fnt, (void **) &fileNames, &count))
	return;		/* no file list */
    if (fileNames == NULL || count <= 0)
	return;

    dirNames = alloca(sizeof(*dirNames) * count);	/* worst case */
    baseNames = alloca(sizeof(*dirNames) * count);
    dirIndexes = alloca(sizeof(*dirIndexes) * count);

    if (fileNames[0][0] != '/') {
	/* HACK. Source RPM, so just do things differently */
	dirIndex = 0;
	dirNames[dirIndex] = "";
	for (i = 0; i < count; i++) {
	    dirIndexes[i] = dirIndex;
	    baseNames[i] = fileNames[i];
	}
	goto exit;
    }

    /*@-branchstate@*/
    for (i = 0; i < count; i++) {
	const char ** needle;
	char savechar;
	char * baseName;
	int len;

	if (fileNames[i] == NULL)	/* XXX can't happen */
	    continue;
	baseName = strrchr(fileNames[i], '/') + 1;
	len = baseName - fileNames[i];
	needle = dirNames;
	savechar = *baseName;
	*baseName = '\0';
/*@-compdef@*/
	if (dirIndex < 0 ||
	    (needle = bsearch(&fileNames[i], dirNames, dirIndex + 1, sizeof(dirNames[0]), dncmp)) == NULL) {
	    char *s = alloca(len + 1);
	    memcpy(s, fileNames[i], len + 1);
	    s[len] = '\0';
	    dirIndexes[i] = ++dirIndex;
	    dirNames[dirIndex] = s;
	} else
	    dirIndexes[i] = needle - dirNames;
/*@=compdef@*/

	*baseName = savechar;
	baseNames[i] = baseName;
    }
    /*@=branchstate@*/

exit:
    if (count > 0) {
	xx = hae(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE, dirIndexes, count);
	xx = hae(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			baseNames, count);
	xx = hae(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			dirNames, dirIndex + 1);
    }

    fileNames = hfd(fileNames, fnt);

    xx = hre(h, RPMTAG_OLDFILENAMES);
}
/*@=bounds@*/

/*
 * This is pretty straight-forward. The only thing that even resembles a trick
 * is getting all of this into a single xmalloc'd block.
 */
static void doBuildFileList(Header h, /*@out@*/ const char *** fileListPtr,
			    /*@out@*/ int * fileCountPtr, rpmTag baseNameTag,
			    rpmTag dirNameTag, rpmTag dirIndexesTag)
	/*@modifies *fileListPtr, *fileCountPtr @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char ** baseNames;
    const char ** dirNames;
    int * dirIndexes;
    int count;
    const char ** fileNames;
    int size;
    rpmTagType bnt, dnt;
    char * data;
    int i, xx;

    if (!hge(h, baseNameTag, &bnt, (void **) &baseNames, &count)) {
	if (fileListPtr) *fileListPtr = NULL;
	if (fileCountPtr) *fileCountPtr = 0;
	return;		/* no file list */
    }

    xx = hge(h, dirNameTag, &dnt, (void **) &dirNames, NULL);
    xx = hge(h, dirIndexesTag, NULL, (void **) &dirIndexes, &count);

    size = sizeof(*fileNames) * count;
    for (i = 0; i < count; i++)
	size += strlen(baseNames[i]) + strlen(dirNames[dirIndexes[i]]) + 1;

    fileNames = xmalloc(size);
    data = ((char *) fileNames) + (sizeof(*fileNames) * count);
    /*@-branchstate@*/
    for (i = 0; i < count; i++) {
	fileNames[i] = data;
	data = stpcpy( stpcpy(data, dirNames[dirIndexes[i]]), baseNames[i]);
	*data++ = '\0';
    }
    /*@=branchstate@*/
    baseNames = hfd(baseNames, bnt);
    dirNames = hfd(dirNames, dnt);

    /*@-branchstate@*/
    if (fileListPtr)
	*fileListPtr = fileNames;
    else
	fileNames = _free(fileNames);
    /*@=branchstate@*/
    if (fileCountPtr) *fileCountPtr = count;
}

void expandFilelist(Header h)
{
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    const char ** fileNames = NULL;
    int count = 0;
    int xx;

    /*@-branchstate@*/
    if (!headerIsEntry(h, RPMTAG_OLDFILENAMES)) {
	doBuildFileList(h, &fileNames, &count, RPMTAG_BASENAMES,
			RPMTAG_DIRNAMES, RPMTAG_DIRINDEXES);
	if (fileNames == NULL || count <= 0)
	    return;
	xx = hae(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE,
			fileNames, count);
	fileNames = _free(fileNames);
    }
    /*@=branchstate@*/

    xx = hre(h, RPMTAG_DIRNAMES);
    xx = hre(h, RPMTAG_BASENAMES);
    xx = hre(h, RPMTAG_DIRINDEXES);
}


void rpmBuildFileList(Header h, const char *** fileListPtr, int * fileCountPtr)
{
    doBuildFileList(h, fileListPtr, fileCountPtr, RPMTAG_BASENAMES,
			RPMTAG_DIRNAMES, RPMTAG_DIRINDEXES);
}

void buildOrigFileList(Header h, const char *** fileListPtr, int * fileCountPtr)
{
    doBuildFileList(h, fileListPtr, fileCountPtr, RPMTAG_ORIGBASENAMES,
			RPMTAG_ORIGDIRNAMES, RPMTAG_ORIGDIRINDEXES);
}

/*
 * Up to rpm 3.0.4, packages implicitly provided their own name-version-release.
 * Retrofit an explicit "Provides: name = epoch:version-release.
 */
void providePackageNVR(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char *name, *version, *release;
    int_32 * epoch;
    const char *pEVR;
    char *p;
    int_32 pFlags = RPMSENSE_EQUAL;
    const char ** provides = NULL;
    const char ** providesEVR = NULL;
    rpmTagType pnt, pvt;
    int_32 * provideFlags = NULL;
    int providesCount;
    int i, xx;
    int bingo = 1;

    /* Generate provides for this package name-version-release. */
    xx = headerNVR(h, &name, &version, &release);
    if (!(name && version && release))
	return;
    pEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p != '\0')
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides at all are available, we can just add.
     */
    if (!hge(h, RPMTAG_PROVIDENAME, &pnt, (void **) &provides, &providesCount))
	goto exit;

    /*
     * Otherwise, fill in entries on legacy packages.
     */
    if (!hge(h, RPMTAG_PROVIDEVERSION, &pvt, (void **) &providesEVR, NULL)) {
	for (i = 0; i < providesCount; i++) {
	    char * vdummy = "";
	    int_32 fdummy = RPMSENSE_ANY;
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
			&vdummy, 1);
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
			&fdummy, 1);
	}
	goto exit;
    }

    xx = hge(h, RPMTAG_PROVIDEFLAGS, NULL, (void **) &provideFlags, NULL);

    /*@-nullderef@*/	/* LCL: providesEVR is not NULL */
    if (provides && providesEVR && provideFlags)
    for (i = 0; i < providesCount; i++) {
        if (!(provides[i] && providesEVR[i]))
            continue;
	if (!(provideFlags[i] == RPMSENSE_EQUAL &&
	    !strcmp(name, provides[i]) && !strcmp(pEVR, providesEVR[i])))
	    continue;
	bingo = 0;
	break;
    }
    /*@=nullderef@*/

exit:
    provides = hfd(provides, pnt);
    providesEVR = hfd(providesEVR, pvt);

    if (bingo) {
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
		&name, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
		&pFlags, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
		&pEVR, 1);
    }
}

void legacyRetrofit(Header h, const struct rpmlead * lead)
{
    const char * prefix;

    /*
     * We don't use these entries (and rpm >= 2 never has) and they are
     * pretty misleading. Let's just get rid of them so they don't confuse
     * anyone.
     */
    if (headerIsEntry(h, RPMTAG_FILEUSERNAME))
	(void) headerRemoveEntry(h, RPMTAG_FILEUIDS);
    if (headerIsEntry(h, RPMTAG_FILEGROUPNAME))
	(void) headerRemoveEntry(h, RPMTAG_FILEGIDS);

    /*
     * We switched the way we do relocateable packages. We fix some of
     * it up here, though the install code still has to be a bit 
     * careful. This fixup makes queries give the new values though,
     * which is quite handy.
     */
    /*@=branchstate@*/
    if (headerGetEntry(h, RPMTAG_DEFAULTPREFIX, NULL, (void **) &prefix, NULL))
    {
	const char * nprefix = stripTrailingChar(alloca_strdup(prefix), '/');
	(void) headerAddEntry(h, RPMTAG_PREFIXES, RPM_STRING_ARRAY_TYPE,
		&nprefix, 1); 
    }
    /*@=branchstate@*/

    /*
     * The file list was moved to a more compressed format which not
     * only saves memory (nice), but gives fingerprinting a nice, fat
     * speed boost (very nice). Go ahead and convert old headers to
     * the new style (this is a noop for new headers).
     */
    if (lead->major < 4)
	compressFilelist(h);

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (lead->type == RPMLEAD_SOURCE) {
	int_32 one = 1;
	if (!headerIsEntry(h, RPMTAG_SOURCEPACKAGE))
	    (void) headerAddEntry(h, RPMTAG_SOURCEPACKAGE, RPM_INT32_TYPE,
				&one, 1);
    } else if (lead->major < 4) {
	/* Retrofit "Provide: name = EVR" for binary packages. */
	providePackageNVR(h);
    }
}
