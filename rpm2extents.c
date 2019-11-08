/* rpm2extents: convert payload to inline extents */

#include "system.h"

#include <rpm/rpmlib.h>		/* rpmReadPackageFile .. */
#include <rpm/rpmfi.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmio.h>
#include <rpm/rpmpgp.h>

#include <rpm/rpmts.h>
#include "lib/rpmlead.h"
#include "lib/signature.h"
#include "lib/header_internal.h"
#include "rpmio/rpmio_internal.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include "debug.h"

/* hash of void * (pointers) to file digests to offsets within output.
   The length of the key depends on what the FILEDIGESTALGO is.
 */
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE digestSet
#define HTKEYTYPE const unsigned char *
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"

/* magic value at end of file (64 bits) that indicates this is a transcoded rpm */
#define MAGIC 3472329499408095051

struct digestoffset {
    const unsigned char * digest;
    rpm_loff_t pos;
};

rpm_loff_t pad_to(rpm_loff_t pos, rpm_loff_t unit);

rpm_loff_t pad_to(rpm_loff_t pos, rpm_loff_t unit)
{
    return (unit - (pos % unit)) % unit;
}

static int digestor(
    FD_t fdi,
    FD_t fdo,
    FD_t validationo,
    uint8_t algos[],
    uint32_t algos_len
)
{
    ssize_t fdilength;
    const char *filedigest, *algo_name;
    size_t filedigest_len, len;
    uint32_t algo_name_len, algo_digest_len;
    int algo;
    rpmRC rc = RPMRC_FAIL;

    for (algo = 0; algo < algos_len; algo++)
    {
        fdInitDigest(fdi, algos[algo], 0);
    }
    fdilength = ufdCopy(fdi, fdo);
    if (fdilength == -1)
    {
        fprintf(stderr, _("digest cat failed\n"));
        goto exit;
    }

    len = sizeof(fdilength);
    if (Fwrite(&fdilength, len, 1, validationo) != len)
    {
        fprintf(stderr, _("Unable to write input length %zd\n"), fdilength);
        goto exit;
    }
    len = sizeof(algos_len);
    if (Fwrite(&algos_len, len, 1, validationo) != len)
    {
        fprintf(stderr, _("Unable to write number of validation digests\n"));
        goto exit;
    }
    for (algo = 0; algo < algos_len; algo++)
    {
        fdFiniDigest(fdi, algos[algo], (void **)&filedigest, &filedigest_len, 0);

        algo_name = pgpValString(PGPVAL_HASHALGO, algos[algo]);
        algo_name_len = (uint32_t)strlen(algo_name);
        algo_digest_len = (uint32_t)filedigest_len;

        len = sizeof(algo_name_len);
        if (Fwrite(&algo_name_len, len, 1, validationo) != len)
        {
            fprintf(
                stderr,
                _("Unable to write validation algo name length\n")
            );
            goto exit;
        }
        len = sizeof(algo_digest_len);
        if (Fwrite(&algo_digest_len, len, 1, validationo) != len)
        {
            fprintf(
                stderr,
                _("Unable to write number of bytes for validation digest\n")
            );
            goto exit;
        }
        if (Fwrite(algo_name, algo_name_len, 1, validationo) != algo_name_len)
        {
            fprintf(stderr, _("Unable to write validation algo name\n"));
            goto exit;
        }
        if (
            Fwrite(
                filedigest,
                algo_digest_len,
                1,
                validationo
            ) != algo_digest_len
        )
        {
            fprintf(
                stderr,
                _("Unable to write validation digest value %u, %zu\n"),
                algo_digest_len,
                filedigest_len
            );
            goto exit;
        }
    }
    rc = RPMRC_OK;
exit:
    return rc;
}

static rpmRC process_package(FD_t fdi, FD_t validationi)
{
    uint32_t diglen;
    /* GNU C extension: can use diglen from outer context */
    int digestSetCmp(const unsigned char * a, const unsigned char * b)
    {
        return memcmp(a, b, diglen);
    }

    unsigned int digestSetHash(const unsigned char * digest)
    {
        /* assumes sizeof(unsigned int) < diglen */
        return *(unsigned int *)digest;
    }

    int digestoffsetCmp(const void * a, const void * b)
    {
        return digestSetCmp(
            ((struct digestoffset *)a)->digest,
            ((struct digestoffset *)b)->digest
        );
    }

    FD_t fdo;
    FD_t gzdi;
    Header h, sigh;
    long fundamental_block_size = sysconf(_SC_PAGESIZE);
    rpmRC rc = RPMRC_OK;
    rpm_mode_t mode;
    char *rpmio_flags = NULL, *zeros;
    const unsigned char *digest;
    rpm_loff_t pos, size, pad, validation_pos;
    uint32_t offset_ix = 0;
    size_t len;
    int next = 0;

    fdo = fdDup(STDOUT_FILENO);

    if (rpmReadPackageRaw(fdi, &sigh, &h))
    {
        fprintf(stderr, _("Error reading package\n"));
        exit(EXIT_FAILURE);
    }

    if (rpmLeadWrite(fdo, h))
    {
        fprintf(
            stderr,
            _("Unable to write package lead: %s\n"),
            Fstrerror(fdo)
        );
        exit(EXIT_FAILURE);
    }

    if (rpmWriteSignature(fdo, sigh))
    {
        fprintf(stderr, _("Unable to write signature: %s\n"), Fstrerror(fdo));
        exit(EXIT_FAILURE);
    }

    if (headerWrite(fdo, h, HEADER_MAGIC_YES))
    {
        fprintf(stderr, _("Unable to write headers: %s\n"), Fstrerror(fdo));
        exit(EXIT_FAILURE);
    }

    /* Retrieve payload size and compression type. */
    {	const char *compr = headerGetString(h, RPMTAG_PAYLOADCOMPRESSOR);
        rpmio_flags = rstrscat(NULL, "r.", compr ? compr : "gzip", NULL);
    }

    gzdi = Fdopen(fdi, rpmio_flags);	/* XXX gzdi == fdi */
    free(rpmio_flags);

    if (gzdi == NULL)
    {
        fprintf(stderr, _("cannot re-open payload: %s\n"), Fstrerror(gzdi));
        exit(EXIT_FAILURE);
    }

    rpmfiles files = rpmfilesNew(NULL, h, 0, RPMFI_KEEPHEADER);
    rpmfi fi = rpmfiNewArchiveReader(
        gzdi,
        files,
        RPMFI_ITER_READ_ARCHIVE_CONTENT_FIRST
    );

    /* this is encoded in the file format, so needs to be fixed size (for
        now?)
    */
    diglen = (uint32_t)rpmDigestLength(rpmfiDigestAlgo(fi));
    digestSet ds = digestSetCreate(
        rpmfiFC(fi),
        digestSetHash,
        digestSetCmp,
        NULL
    );
    struct digestoffset offsets[rpmfiFC(fi)];
    pos = RPMLEAD_SIZE + headerSizeof(sigh, HEADER_MAGIC_YES);

    /* main headers are aligned to 8 byte boundry */
    pos += pad_to(pos, 8);
    pos += headerSizeof(h, HEADER_MAGIC_YES);

    zeros = xcalloc(fundamental_block_size, 1);

    while (next >= 0)
    {
        next = rpmfiNext(fi);
        if (next == RPMERR_ITER_END)
        {
            rc = RPMRC_OK;
            break;
        }
        mode = rpmfiFMode(fi);
        if (!S_ISREG(mode) || !rpmfiArchiveHasContent(fi))
        {
            /* not a regular file, or the archive doesn't contain any content for
               this entry
            */
            continue;
        }
        digest = rpmfiFDigest(fi, NULL, NULL);
        if (digestSetGetEntry(ds, digest, NULL))
        {
            /* This specific digest has already been included, so skip it */
            continue;
        }
        pad = pad_to(pos, fundamental_block_size);
        if (Fwrite(zeros, sizeof(char), pad, fdo) != pad)
        {
            fprintf(stderr, _("Unable to write padding\n"));
            rc = RPMRC_FAIL;
            goto exit;
        }
        /* round up to next fundamental_block_size */
        pos += pad;
        digestSetAddEntry(ds, digest);
        offsets[offset_ix].digest = digest;
        offsets[offset_ix].pos = pos;
        offset_ix++;
        size = rpmfiFSize(fi);
        rc = rpmfiArchiveReadToFile(fi, fdo, 0);
        if (rc != RPMRC_OK)
        {
            fprintf(stderr, _("rpmfiArchiveReadToFile failed with %d\n"), rc);
            goto exit;
        }
        pos += size;
    }
    Fclose(gzdi);	/* XXX gzdi == fdi */

    qsort(
        offsets,
        (size_t)offset_ix,
        sizeof(struct digestoffset),
        digestoffsetCmp
    );

    len = sizeof(offset_ix);
    if (Fwrite(&offset_ix, len, 1, fdo) != len)
    {
        fprintf(stderr, _("Unable to write length of table\n"));
        rc = RPMRC_FAIL;
        goto exit;
    }
    len = sizeof(diglen);
    if (Fwrite(&diglen, len, 1, fdo) != len)
    {
        fprintf(stderr, _("Unable to write length of digest\n"));
        rc = RPMRC_FAIL;
        goto exit;
    }
    len = sizeof(rpm_loff_t);
    for (int x = 0; x < offset_ix; x++)
    {
        if (Fwrite(offsets[x].digest, diglen, 1, fdo) != diglen)
        {
            fprintf(stderr, _("Unable to write digest\n"));
            rc = RPMRC_FAIL;
            goto exit;
        }
        if (Fwrite(&offsets[x].pos, len, 1, fdo) != len)
        {
            fprintf(stderr, _("Unable to write offset\n"));
            rc = RPMRC_FAIL;
            goto exit;
        }
    }
    validation_pos = (
        pos + sizeof(offset_ix) + sizeof(diglen) +
        offset_ix * (diglen + sizeof(rpm_loff_t))
    );

    ssize_t validation_len = ufdCopy(validationi, fdo);
    if (validation_len == -1)
    {
        fprintf(stderr, _("digest table ufdCopy failed\n"));
        rc = RPMRC_FAIL;
        goto exit;
    }
    /* add more padding so the last file can be cloned. It doesn't matter that
       the table and validation etc are in this space. In fact, it's pretty
       efficient if it is
    */

    pad = pad_to(
        (
            validation_pos + validation_len + 2 * sizeof(rpm_loff_t) +
            sizeof(uint64_t)
        ),
        fundamental_block_size
    );
    if (Fwrite(zeros, sizeof(char), pad, fdo) != pad)
    {
        fprintf(stderr, _("Unable to write final padding\n"));
        rc = RPMRC_FAIL;
        goto exit;
    }
    zeros = _free(zeros);
    if (Fwrite(&pos, len, 1, fdo) != len)
    {
        fprintf(stderr, _("Unable to write offset of digest table\n"));
        rc = RPMRC_FAIL;
        goto exit;
    }
    if (Fwrite(&validation_pos, len, 1, fdo) != len)
    {
        fprintf(stderr, _("Unable to write offset of validation table\n"));
        rc = RPMRC_FAIL;
        goto exit;
    }
    uint64_t magic = MAGIC;
    len = sizeof(magic);
    if (Fwrite(&magic, len, 1, fdo) != len)
    {
        fprintf(stderr, _("Unable to write magic\n"));
        rc = RPMRC_FAIL;
        goto exit;
    }

exit:
    rpmfilesFree(files);
    rpmfiFree(fi);
    headerFree(h);
    return rc;
}

int main(int argc, char *argv[])
{
    rpmRC rc;
    int cprc = 0;
    uint8_t algos[argc - 1];
    int mainpipefd[2];
    int metapipefd[2];
    pid_t cpid, w;
    int wstatus;

    xsetprogname(argv[0]);	/* Portability call -- see system.h */
    rpmReadConfigFiles(NULL, NULL);

    if (argc > 1 && (rstreq(argv[1], "-h") || rstreq(argv[1], "--help")))
    {
        fprintf(stderr, _("Usage: %s [DIGESTALGO]...\n"), argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc == 1)
    {
        fprintf(
            stderr,
            _("Need at least one DIGESTALGO parameter, e.g. 'SHA256'\n")
        );
        exit(EXIT_FAILURE);
    }

    for (int x = 0; x < (argc - 1); x++)
    {
        if (pgpStringVal(PGPVAL_HASHALGO, argv[x + 1], &algos[x]) != 0)
        {
            fprintf(
                stderr,
                _("Unable to resolve '%s' as a digest algorithm, exiting\n"),
                argv[x + 1]
            );
            exit(EXIT_FAILURE);
        }
    }


    if (pipe(mainpipefd) == -1)
    {
        fprintf(stderr, _("Main pipe failure\n"));
        exit(EXIT_FAILURE);
    }
    if (pipe(metapipefd) == -1)
    {
        fprintf(stderr, _("Meta pipe failure\n"));
        exit(EXIT_FAILURE);
    }
    cpid = fork();
    if (cpid == 0)
    {
        /* child: digestor */
        close(mainpipefd[0]);
        close(metapipefd[0]);
        FD_t fdi = fdDup(STDIN_FILENO);
        FD_t fdo = fdDup(mainpipefd[1]);
        FD_t validationo = fdDup(metapipefd[1]);
        rc = digestor(fdi, fdo, validationo, algos, argc - 1);
        Fclose(validationo);
        Fclose(fdo);
        Fclose(fdi);
    } else {
        /* parent: main program */
        close(mainpipefd[1]);
        close(metapipefd[1]);
        FD_t fdi = fdDup(mainpipefd[0]);
        FD_t validationi = fdDup(metapipefd[0]);
        rc = process_package(fdi, validationi);
        Fclose(validationi);
        /* fdi is normally closed through the stacked file gzdi in the function. */
        /* wait for child process (digestor for stdin) to complete. */
        if (rc != RPMRC_OK)
        {
            if (kill(cpid, SIGTERM) != 0)
            {
                fprintf(
                    stderr,
                    _("Failed to kill digest process when main process failed: %s\n"),
                    strerror(errno)
                );
            }
        }
        w = waitpid(cpid, &wstatus, 0);
        if (w == -1)
        {
            fprintf(stderr, _("waitpid failed\n"));
            cprc = EXIT_FAILURE;
        } else if (WIFEXITED(wstatus))
        {
            cprc = WEXITSTATUS(wstatus);
            if (cprc != 0)
            {
                fprintf(
                    stderr,
                    _("Digest process non-zero exit code %d\n"),
                    cprc
                );
            }
        } else if (WIFSIGNALED(wstatus))
        {
            fprintf(
                stderr,
                _("Digest process was terminated with a signal: %d\n"),
                WTERMSIG(wstatus)
            );
            cprc = EXIT_FAILURE;
        } else
        {
            /* don't think this can happen, but covering all bases */
            fprintf(stderr, _("Unhandled circumstance in waitpid\n"));
            cprc = EXIT_FAILURE;
        }
        if (cprc != EXIT_SUCCESS)
        {
            rc = RPMRC_FAIL;
        }
    }
    if (rc != RPMRC_OK)
    {
        /* translate rpmRC into generic failure return code. */
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
