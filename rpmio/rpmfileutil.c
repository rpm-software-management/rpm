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

#include "rpmio_internal.h"
#include <rpmfileutil.h>
#include <rpmurl.h>
#include <rpmmacro.h>
#include <argv.h>

static int open_dso(const char * path, pid_t * pidp, size_t *fsizep)
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
		*lib = path;
		unsetenv("MALLOC_CHECK_");
		xx = execve(av[0], (char *const *)av+1, environ);
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
                unsigned char * digest, size_t * fsizep)
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

	ctx = rpmDigestInit(PGPHASHALGO_MD5, RPMDIGEST_NONE);
	if (fsize)
	    xx = rpmDigestUpdate(ctx, mapped, fsize);
	xx = rpmDigestFinal(ctx, (void **)&md5sum, &md5len, asAscii);
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
    if (fsizep)
	*fsizep = fsize;
    if (!rc)
	memcpy(digest, md5sum, md5len);
    md5sum = _free(md5sum);

    return rc;
}

