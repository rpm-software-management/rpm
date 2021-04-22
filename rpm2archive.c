/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include <rpm/rpmlib.h>		/* rpmReadPackageFile .. */
#include <rpm/rpmfi.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmio.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmts.h>

#include <popt.h>

#include <archive.h>
#include <archive_entry.h>
#include <unistd.h>

#include "debug.h"

#define BUFSIZE (128*1024)

int compress = 1;

static struct poptOption optionsTable[] = {
    { "nocompression", 'n', POPT_ARG_VAL, &compress, 0,
        N_("create uncompressed tar file"),
        NULL },
    POPT_AUTOHELP
    POPT_TABLEEND
};

static void fill_archive_entry(struct archive_entry * entry, rpmfi fi)
{
    archive_entry_clear(entry);
    const char * dn = rpmfiDN(fi);
    if (!strcmp(dn, "")) dn = "/";

    char * filename = rstrscat(NULL, ".", dn, rpmfiBN(fi), NULL);
    archive_entry_copy_pathname(entry, filename);
    _free(filename);

    archive_entry_set_size(entry, rpmfiFSize(fi));
    rpm_mode_t mode = rpmfiFMode(fi);
    archive_entry_set_filetype(entry, mode & S_IFMT);
    archive_entry_set_perm(entry, mode);

    archive_entry_set_uname(entry, rpmfiFUser(fi));
    archive_entry_set_gname(entry, rpmfiFGroup(fi));
    archive_entry_set_rdev(entry, rpmfiFRdev(fi));
    archive_entry_set_mtime(entry, rpmfiFMtime(fi), 0);

    if (S_ISLNK(mode))
	archive_entry_set_symlink(entry, rpmfiFLink(fi));
}

static void write_file_content(struct archive * a, char * buf, rpmfi fi)
{
    rpm_loff_t left = rpmfiFSize(fi);
    size_t len, read;

    while (left) {
	len = (left > BUFSIZE ? BUFSIZE : left);
	read = rpmfiArchiveRead(fi, buf, len);
	if (read==len) {
	    archive_write_data(a, buf, len);
	} else {
	    fprintf(stderr, "Error reading file from rpm payload\n");
	    break;
	}
	left -= len;
    }
}

static int process_package(rpmts ts, const char * filename)
{
    FD_t fdi;
    FD_t gzdi;
    Header h;
    int rc = 0;
    char * rpmio_flags = NULL;
    struct archive *a;
    struct archive_entry *entry;

    if (!strcmp(filename, "-")) {
	fdi = fdDup(STDIN_FILENO);
    } else {
	fdi = Fopen(filename, "r.ufdio");
    }

    if (Ferror(fdi)) {
	fprintf(stderr, "rpm2archive: %s: %s\n",
		filename, Fstrerror(fdi));
	exit(EXIT_FAILURE);
    }

    rc = rpmReadPackageFile(ts, fdi, "rpm2cpio", &h);

    switch (rc) {
    case RPMRC_OK:
    case RPMRC_NOKEY:
    case RPMRC_NOTTRUSTED:
	break;
    case RPMRC_NOTFOUND:
	fprintf(stderr, _("argument is not an RPM package\n"));
	exit(EXIT_FAILURE);
	break;
    case RPMRC_FAIL:
    default:
	fprintf(stderr, _("error reading header from package\n"));
	exit(EXIT_FAILURE);
	break;
    }


    /* Retrieve payload size and compression type. */
    {	const char *compr = headerGetString(h, RPMTAG_PAYLOADCOMPRESSOR);
	rpmio_flags = rstrscat(NULL, "r.", compr ? compr : "gzip", NULL);
    }

    gzdi = Fdopen(fdi, rpmio_flags);	/* XXX gzdi == fdi */
    free(rpmio_flags);

    if (gzdi == NULL) {
	fprintf(stderr, _("cannot re-open payload: %s\n"), Fstrerror(gzdi));
	exit(EXIT_FAILURE);
    }

    rpmfiles files = rpmfilesNew(NULL, h, 0, RPMFI_KEEPHEADER);
    rpmfi fi = rpmfiNewArchiveReader(gzdi, files, RPMFI_ITER_READ_ARCHIVE_CONTENT_FIRST);

    /* create archive */
    a = archive_write_new();
    if (compress) {
	if (archive_write_add_filter_gzip(a) != ARCHIVE_OK) {
	    fprintf(stderr, "%s\n", archive_error_string(a));
	    exit(EXIT_FAILURE);
	}
    }
    if (archive_write_set_format_pax_restricted(a) != ARCHIVE_OK) {
	fprintf(stderr, "Error: Format pax restricted is not supported\n");
	exit(EXIT_FAILURE);
    }
    if (!strcmp(filename, "-")) {
	if (isatty(STDOUT_FILENO)) {
	    fprintf(stderr, "Error: refusing to output archive data to a terminal.\n");
	    exit(EXIT_FAILURE);
	}
	archive_write_open_fd(a, STDOUT_FILENO);
    } else {
	char * outname;
	if (urlIsURL(filename)) {
	    const char * fname = strrchr(filename, '/');
	    if (fname != NULL) {
		fname++;
	    } else {
		fname = filename;
	    }
	    outname = rstrscat(NULL, fname, NULL);
	} else {
	    outname = rstrscat(NULL, filename, NULL);
	}
	if (compress) {
	    outname = rstrscat(&outname, ".tgz", NULL);
	} else {
	    outname = rstrscat(&outname, ".tar", NULL);
	}
	if (archive_write_open_filename(a, outname) != ARCHIVE_OK) {
	    fprintf(stderr, "Error: Can't open output file: %s\n", outname);
	    exit(EXIT_FAILURE);
	}
	_free(outname);
    }

    entry = archive_entry_new();

    char * buf = xmalloc(BUFSIZE);
    char * hardlink = NULL;

    rc = 0;
    while (rc >= 0) {
	rc = rpmfiNext(fi);
	if (rc == RPMERR_ITER_END) {
	    break;
	}

	rpm_mode_t mode = rpmfiFMode(fi);
	int nlink = rpmfiFNlink(fi);

	fill_archive_entry(entry, fi);

	if (nlink > 1) {
	    if (rpmfiArchiveHasContent(fi)) {
		_free(hardlink);
		hardlink = xstrdup(archive_entry_pathname(entry));
	    } else {
		archive_entry_set_hardlink(entry, hardlink);
	    }
	}

	archive_write_header(a, entry);

	if (S_ISREG(mode) && (nlink == 1 || rpmfiArchiveHasContent(fi))) {
	    write_file_content(a, buf, fi);
	}
    }
    /* End of iteration is not an error */
    if (rc == RPMERR_ITER_END) {
	rc = 0;
    }

    _free(hardlink);

    Fclose(gzdi);	/* XXX gzdi == fdi */
    archive_entry_free(entry);
    archive_write_close(a);
    archive_write_free(a);
    buf = _free(buf);
    rpmfilesFree(files);
    rpmfiFree(fi);
    headerFree(h);
    return rc;
}

int main(int argc, const char *argv[])
{
    int rc = 0;
    poptContext optCon;
    const char *fn;

    xsetprogname(argv[0]);	/* Portability call -- see system.h */
    rpmReadConfigFiles(NULL, NULL);

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "[OPTIONS]* <FILES>");
    if (argc < 2 || poptGetNextOpt(optCon) == 0) {
	poptPrintUsage(optCon, stderr, 0);
	exit(EXIT_FAILURE);
    }

    rpmts ts = rpmtsCreate();
    rpmVSFlags vsflags = 0;

    /* XXX retain the ageless behavior of rpm2cpio */
    vsflags |= RPMVSF_MASK_NODIGESTS;
    vsflags |= RPMVSF_MASK_NOSIGNATURES;
    vsflags |= RPMVSF_NOHDRCHK;
    (void) rpmtsSetVSFlags(ts, vsflags);

    /* if no file name is given use stdin/stdout */
    if (!poptPeekArg(optCon)) {
	rc = process_package(ts, "-");
	if (rc != 0)
	    goto exit;
    }

    while ((fn = poptGetArg(optCon)) != NULL) {
	rc = process_package(ts, fn);
	if (rc != 0)
	    goto exit;
    }

 exit:
    poptFreeContext(optCon);
    (void) rpmtsFree(ts);
    return rc;
}
