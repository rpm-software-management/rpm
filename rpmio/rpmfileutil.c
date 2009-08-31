#include "system.h"

#if HAVE_GELF_H

#include <gelf.h>

#if !defined(DT_GNU_PRELINKED)
#define DT_GNU_PRELINKED        0x6ffffdf5
#endif
#if !defined(DT_GNU_LIBLIST)
#define DT_GNU_LIBLIST          0x6ffffef9
#endif

#endif

#include <popt.h>

#include <rpm/rpmfileutil.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmlog.h>
#include <rpm/argv.h>

#include "rpmio/rpmio_internal.h"

#include "debug.h"

static const char *rpm_config_dir = NULL;

static int open_dso(const char * path, pid_t * pidp, rpm_loff_t *fsizep)
{
    static const char * cmd = NULL;
    static int initted = 0;
    int fdno;

    if (!initted) {
	cmd = rpmExpand("%{?__prelink_undo_cmd}", NULL);
	initted++;
    }

    if (pidp) *pidp = 0;

    if (fsizep) {
	struct stat sb, * st = &sb;
	if (stat(path, st) < 0)
	    return -1;
	*fsizep = st->st_size;
    }

    fdno = open(path, O_RDONLY);
    if (fdno < 0)
	return fdno;

    if (!(cmd && *cmd))
	return fdno;

#if HAVE_GELF_H && HAVE_LIBELF
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
		    continue;
		bingo = 1;
		break;
	    }
	}
    }

    if (pidp != NULL && bingo) {
	int pipes[2];
	pid_t pid;
	int xx;

	xx = close(fdno);
	pipes[0] = pipes[1] = -1;
	xx = pipe(pipes);
	if (!(pid = fork())) {
	    ARGV_t av, lib;
	    argvSplit(&av, cmd, " ");

	    xx = close(pipes[0]);
	    xx = dup2(pipes[1], STDOUT_FILENO);
	    xx = close(pipes[1]);
	    if ((lib = argvSearch(av, "library", NULL)) != NULL) {
		*lib = (char *) path;
		unsetenv("MALLOC_CHECK_");
		xx = execve(av[0], av+1, environ);
	    }
	    _exit(127);
	}
	*pidp = pid;
	fdno = pipes[0];
	xx = close(pipes[1]);
    }

exit:
    if (elf) (void) elf_end(elf);
 }
#endif

    return fdno;
}

int rpmDoDigest(pgpHashAlgo algo, const char * fn,int asAscii,
                unsigned char * digest, rpm_loff_t * fsizep)
{
    const char * path;
    urltype ut = urlPath(fn, &path);
    unsigned char * dig = NULL;
    size_t diglen;
    unsigned char buf[32*BUFSIZ];
    FD_t fd;
    rpm_loff_t fsize = 0;
    pid_t pid = 0;
    int rc = 0;
    int fdno;

    fdno = open_dso(path, &pid, &fsize);
    if (fdno < 0) {
	rc = 1;
	goto exit;
    }

    /* file to large (32 MB), do not mmap file */
    if (fsize > (size_t) 32*1024*1024)
      if (ut == URL_IS_PATH || ut == URL_IS_UNKNOWN)
	ut = URL_IS_DASH; /* force fd io */

    switch(ut) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
#ifdef HAVE_MMAP
      if (pid == 0) {
	DIGEST_CTX ctx;
	void * mapped;

	if (fsize) {
	    mapped = mmap(NULL, fsize, PROT_READ, MAP_SHARED, fdno, 0);
	    if (mapped == (void *)-1) {
		xx = close(fdno);
		rc = 1;
		break;
	    }

#ifdef	MADV_SEQUENTIAL
	    xx = madvise(mapped, fsize, MADV_SEQUENTIAL);
#endif
	}

	ctx = rpmDigestInit(algo, RPMDIGEST_NONE);
	if (fsize)
	    xx = rpmDigestUpdate(ctx, mapped, fsize);
	xx = rpmDigestFinal(ctx, (void **)&dig, &diglen, asAscii);
	if (fsize)
	    xx = munmap(mapped, fsize);
	xx = close(fdno);
	break;
      }
#endif
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    case URL_IS_HKP:
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
	
	fdInitDigest(fd, algo, 0);
	fsize = 0;
	while ((rc = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	    fsize += rc;
	fdFiniDigest(fd, algo, (void **)&dig, &diglen, asAscii);
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
    if (fsizep)
	*fsizep = fsize;
    if (!rc)
	memcpy(digest, dig, diglen);
    dig = _free(dig);

    return rc;
}

FD_t rpmMkTemp(char *templ)
{
    int sfd;
    FD_t tfd = NULL;

    sfd = mkstemp(templ);
    if (sfd < 0) {
	goto exit;
    }

    tfd = fdDup(sfd);
    close(sfd);

exit:
    return tfd;
}

FD_t rpmMkTempFile(const char * prefix, char **fn)
{
    const char *tpmacro = "%{_tmppath}"; /* always set from rpmrc */
    char *tempfn;
    static int _initialized = 0;
    FD_t tfd = NULL;

    if (!prefix) prefix = "";

    /* Create the temp directory if it doesn't already exist. */
    if (!_initialized) {
	_initialized = 1;
	tempfn = rpmGenPath(prefix, tpmacro, NULL);
	if (rpmioMkpath(tempfn, 0755, (uid_t) -1, (gid_t) -1))
	    goto exit;
	free(tempfn);
    }

    tempfn = rpmGetPath(prefix, tpmacro, "/rpm-tmp.XXXXXX", NULL);
    tfd = rpmMkTemp(tempfn);

    if (tfd == NULL || Ferror(tfd)) {
	rpmlog(RPMLOG_ERR, _("error creating temporary file %s: %m\n"), tempfn);
	goto exit;
    }

exit:
    if (tfd != NULL && fn)
	*fn = tempfn;
    else
	free(tempfn);

    return tfd;
}

int rpmioMkpath(const char * path, mode_t mode, uid_t uid, gid_t gid)
{
    char *d, *de;
    int rc;

    if (path == NULL || *path == '\0')
	return -1;
    d = rstrcat(NULL, path);
    if (d[strlen(d)-1] != '/') {
	rstrcat(&d,"/");
    }
    de = d;
    for (;(de=strchr(de+1,'/'));) {
	struct stat st;
	*de = '\0';
	rc = stat(d, &st);
	if (rc) {
	    if (errno != ENOENT)
		goto exit;
	    rc = mkdir(d, mode);
	    if (rc)
		goto exit;
	    rpmlog(RPMLOG_DEBUG, "created directory(s) %s mode 0%o\n", path, mode);
	    if (!(uid == (uid_t) -1 && gid == (gid_t) -1)) {
		rc = chown(d, uid, gid);
		if (rc)
		    goto exit;
	    }
	} else if (!S_ISDIR(st.st_mode)) {
	    rc = ENOTDIR;
	    goto exit;
	}
	*de = '/';
    }
    rc = 0;
exit:
    free(d);
    return rc;
}

int rpmFileIsCompressed(const char * file, rpmCompressedMagic * compressed)
{
    FD_t fd;
    ssize_t nb;
    int rc = -1;
    unsigned char magic[13];

    *compressed = COMPRESSED_NOT;

    fd = Fopen(file, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	/* XXX Fstrerror */
	rpmlog(RPMLOG_ERR, _("File %s: %s\n"), file, Fstrerror(fd));
	if (fd) (void) Fclose(fd);
	return 1;
    }
    nb = Fread(magic, sizeof(magic[0]), sizeof(magic), fd);
    if (nb < 0) {
	rpmlog(RPMLOG_ERR, _("File %s: %s\n"), file, Fstrerror(fd));
	rc = 1;
    } else if (nb < sizeof(magic)) {
	rpmlog(RPMLOG_ERR, _("File %s is smaller than %u bytes\n"),
		file, (unsigned)sizeof(magic));
	rc = 0;
    }
    (void) Fclose(fd);
    if (rc >= 0)
	return rc;

    rc = 0;

    if ((magic[0] == 'B') && (magic[1] == 'Z')) {
	*compressed = COMPRESSED_BZIP2;
    } else if ((magic[0] == 0120) && (magic[1] == 0113) &&
	 (magic[2] == 0003) && (magic[3] == 0004)) {	/* pkzip */
	*compressed = COMPRESSED_ZIP;
    } else if ((magic[0] == 0xfd) && (magic[1] == 0x37) &&
	       (magic[2] == 0x7a) && (magic[3] == 0x58) &&
	       (magic[4] == 0x5a) && (magic[5] == 0x00)) {
	/* new style xz (lzma) with magic */
	*compressed = COMPRESSED_XZ;
    } else if (((magic[0] == 0037) && (magic[1] == 0213)) || /* gzip */
	((magic[0] == 0037) && (magic[1] == 0236)) ||	/* old gzip */
	((magic[0] == 0037) && (magic[1] == 0036)) ||	/* pack */
	((magic[0] == 0037) && (magic[1] == 0240)) ||	/* SCO lzh */
	((magic[0] == 0037) && (magic[1] == 0235))	/* compress */
	) {
	*compressed = COMPRESSED_OTHER;
    } else if (rpmFileHasSuffix(file, ".lzma")) {
	*compressed = COMPRESSED_LZMA;
    }

    return rc;
}

/* @todo "../sbin/./../bin/" not correct. */
char *rpmCleanPath(char * path)
{
    const char *s;
    char *se, *t, *te;
    int begin = 1;

    if (path == NULL)
	return NULL;

/*fprintf(stderr, "*** RCP %s ->\n", path); */
    s = t = te = path;
    while (*s != '\0') {
/*fprintf(stderr, "*** got \"%.*s\"\trest \"%s\"\n", (t-path), path, s); */
	switch(*s) {
	case ':':			/* handle url's */
	    if (s[1] == '/' && s[2] == '/') {
		*t++ = *s++;
		*t++ = *s++;
		break;
	    }
	    begin=1;
	    break;
	case '/':
	    /* Move parent dir forward */
	    for (se = te + 1; se < t && *se != '/'; se++)
		{};
	    if (se < t && *se == '/') {
		te = se;
/*fprintf(stderr, "*** next pdir \"%.*s\"\n", (te-path), path); */
	    }
	    while (s[1] == '/')
		s++;
	    while (t > path && t[-1] == '/')
		t--;
	    break;
	case '.':
	    /* Leading .. is special */
 	    /* Check that it is ../, so that we don't interpret */
	    /* ..?(i.e. "...") or ..* (i.e. "..bogus") as "..". */
	    /* in the case of "...", this ends up being processed*/
	    /* as "../.", and the last '.' is stripped.  This   */
	    /* would not be correct processing.                 */
	    if (begin && s[1] == '.' && (s[2] == '/' || s[2] == '\0')) {
/*fprintf(stderr, "    leading \"..\"\n"); */
		*t++ = *s++;
		break;
	    }
	    /* Single . is special */
	    if (begin && s[1] == '\0') {
		break;
	    }
	    /* Handle the ./ cases */
	    if (t > path && t[-1] == '/') {
		/* Trim embedded ./ */
		if (s[1] == '/') {
		    s+=2;
		    continue;
		}
		/* Trim trailing /. */
		if (s[1] == '\0') {
		    s++;
		    continue;
		}
	    }
	    /* Trim embedded /../ and trailing /.. */
	    if (!begin && t > path && t[-1] == '/' && s[1] == '.' && (s[2] == '/' || s[2] == '\0')) {
		t = te;
		/* Move parent dir forward */
		if (te > path)
		    for (--te; te > path && *te != '/'; te--)
			{};
/*fprintf(stderr, "*** prev pdir \"%.*s\"\n", (te-path), path); */
		s++;
		s++;
		continue;
	    }
	    break;
	default:
	    begin = 0;
	    break;
	}
	*t++ = *s++;
    }

    /* Trim trailing / (but leave single / alone) */
    if (t > &path[1] && t[-1] == '/')
	t--;
    *t = '\0';

/*fprintf(stderr, "\t%s\n", path); */
    return path;
}

/* Merge 3 args into path, any or all of which may be a url. */

char * rpmGenPath(const char * urlroot, const char * urlmdir,
		const char *urlfile)
{
    char * xroot = rpmGetPath(urlroot, NULL);
    const char * root = xroot;
    char * xmdir = rpmGetPath(urlmdir, NULL);
    const char * mdir = xmdir;
    char * xfile = rpmGetPath(urlfile, NULL);
    const char * file = xfile;
    char * result;
    char * url = NULL;
    int nurl = 0;
    int ut;

    ut = urlPath(xroot, &root);
    if (url == NULL && ut > URL_IS_DASH) {
	url = xroot;
	nurl = root - xroot;
    }
    if (root == NULL || *root == '\0') root = "/";

    ut = urlPath(xmdir, &mdir);
    if (url == NULL && ut > URL_IS_DASH) {
	url = xmdir;
	nurl = mdir - xmdir;
    }
    if (mdir == NULL || *mdir == '\0') mdir = "/";

    ut = urlPath(xfile, &file);
    if (url == NULL && ut > URL_IS_DASH) {
	url = xfile;
	nurl = file - xfile;
    }

    if (url && nurl > 0) {
	char *t = rstrcat(NULL, url);
	t[nurl] = '\0';
	url = t;
    } else
	url = xstrdup("");

    result = rpmGetPath(url, root, "/", mdir, "/", file, NULL);

    xroot = _free(xroot);
    xmdir = _free(xmdir);
    xfile = _free(xfile);
    free(url);
    return result;
}

/* Return concatenated and expanded canonical path. */

char * rpmGetPath(const char *path, ...)
{
    va_list ap;
    char *dest = NULL, *res;
    const char *s;

    if (path == NULL)
	return xstrdup("");

    va_start(ap, path);
    for (s = path; s; s = va_arg(ap, const char *)) {
	rstrcat(&dest, s);
    }
    va_end(ap);

    res = rpmExpand(dest, NULL);
    free(dest);

    return rpmCleanPath(res);
}

int rpmGlob(const char * patterns, int * argcPtr, ARGV_t * argvPtr)
{
    int ac = 0;
    const char ** av = NULL;
    int argc = 0;
    ARGV_t argv = NULL;
    char * globRoot = NULL;
    const char *home = getenv("HOME");
    int gflags = 0;
#ifdef ENABLE_NLS
    char * old_collate = NULL;
    char * old_ctype = NULL;
    const char * t;
#endif
    size_t maxb, nb;
    int i, j;
    int rc;

    if (home != NULL && strlen(home) > 0) 
	gflags |= GLOB_TILDE;

    /* Can't use argvSplit() here, it doesn't handle whitespace etc escapes */
    rc = poptParseArgvString(patterns, &ac, &av);
    if (rc)
	return rc;

#ifdef ENABLE_NLS
    t = setlocale(LC_COLLATE, NULL);
    if (t)
    	old_collate = xstrdup(t);
    t = setlocale(LC_CTYPE, NULL);
    if (t)
    	old_ctype = xstrdup(t);
    (void) setlocale(LC_COLLATE, "C");
    (void) setlocale(LC_CTYPE, "C");
#endif
	
    if (av != NULL)
    for (j = 0; j < ac; j++) {
	char * globURL;
	const char * path;
	int ut = urlPath(av[j], &path);
	int local = (ut == URL_IS_PATH) || (ut == URL_IS_UNKNOWN);
	glob_t gl;

	if (!local || (!glob_pattern_p(av[j], 0) && strchr(path, '~') == NULL)) {
	    argvAdd(&argv, av[j]);
	    continue;
	}
	
	gl.gl_pathc = 0;
	gl.gl_pathv = NULL;
	
	rc = glob(av[j], gflags, NULL, &gl);
	if (rc)
	    goto exit;

	/* XXX Prepend the URL leader for globs that have stripped it off */
	maxb = 0;
	for (i = 0; i < gl.gl_pathc; i++) {
	    if ((nb = strlen(&(gl.gl_pathv[i][0]))) > maxb)
		maxb = nb;
	}
	
	nb = ((ut == URL_IS_PATH) ? (path - av[j]) : 0);
	maxb += nb;
	maxb += 1;
	globURL = globRoot = xmalloc(maxb);

	switch (ut) {
	case URL_IS_PATH:
	case URL_IS_DASH:
	    strncpy(globRoot, av[j], nb);
	    break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	case URL_IS_HKP:
	case URL_IS_UNKNOWN:
	default:
	    break;
	}
	globRoot += nb;
	*globRoot = '\0';

	for (i = 0; i < gl.gl_pathc; i++) {
	    const char * globFile = &(gl.gl_pathv[i][0]);
	    if (globRoot > globURL && globRoot[-1] == '/')
		while (*globFile == '/') globFile++;
	    strcpy(globRoot, globFile);
	    argvAdd(&argv, globURL);
	}
	globfree(&gl);
	globURL = _free(globURL);
    }

    argc = argvCount(argv);
    if (argc > 0) {
	if (argvPtr)
	    *argvPtr = argv;
	if (argcPtr)
	    *argcPtr = argc;
	rc = 0;
    } else
	rc = 1;


exit:
#ifdef ENABLE_NLS	
    if (old_collate) {
	(void) setlocale(LC_COLLATE, old_collate);
	old_collate = _free(old_collate);
    }
    if (old_ctype) {
	(void) setlocale(LC_CTYPE, old_ctype);
	old_ctype = _free(old_ctype);
    }
#endif
    av = _free(av);
    if (rc || argvPtr == NULL) {
	argvFree(argv);
    }
    return rc;
}

char * rpmEscapeSpaces(const char * s)
{
    const char * se;
    char * t;
    char * te;
    size_t nb = 0;

    for (se = s; *se; se++) {
	if (isspace(*se))
	    nb++;
	nb++;
    }
    nb++;

    t = te = xmalloc(nb);
    for (se = s; *se; se++) {
	if (isspace(*se))
	    *te++ = '\\';
	*te++ = *se;
    }
    *te = '\0';
    return t;
}

int rpmFileHasSuffix(const char *path, const char *suffix)
{
    size_t plen = strlen(path);
    size_t slen = strlen(suffix);
    return (plen >= slen && rstreq(path+plen-slen, suffix));
}

char * rpmGetCwd(void)
{
    int currDirLen = 0;
    char * currDir = NULL;

    do {
	currDirLen += 128;
	currDir = xrealloc(currDir, currDirLen);
	memset(currDir, 0, currDirLen);
    } while (getcwd(currDir, currDirLen) == NULL && errno == ERANGE);

    return currDir;
}

int rpmMkdirs(const char *root, const char *pathstr)
{
    ARGV_t dirs = NULL;
    int rc = 0;
    argvSplit(&dirs, pathstr, ":");
    
    for (char **d = dirs; *d; d++) {
	char *path = rpmGetPath(root ? root : "", *d, NULL);
	if ((rc = rpmioMkpath(path, 0755, -1, -1)) != 0) {
	    const char *msg = _("failed to create directory");
	    /* try to be more informative if the failing part was a macro */
	    if (**d == '%') {
	    	rpmlog(RPMLOG_ERR, "%s %s: %s: %m\n", msg, *d, path);
	    } else {
	    	rpmlog(RPMLOG_ERR, "%s %s: %m\n", msg, path);
	    }
	}
	free(path);
	if (rc) break;
    }
    argvFree(dirs);
    return rc;
}

const char *rpmConfigDir(void)
{
    if (rpm_config_dir == NULL) {
	char *rpmenv = getenv("RPM_CONFIGDIR");
	rpm_config_dir = rpmenv ? xstrdup(rpmenv) : RPMCONFIGDIR;
    }
    return rpm_config_dir;
}
