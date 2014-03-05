/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"
const char *__progname;

#include <rpm/rpmlib.h>		/* rpmReadPackageFile .. */
#include <rpm/rpmfi.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmio.h>
#include <rpm/rpmpgp.h>

#include <rpm/rpmts.h>

#include <archive.h>
#include <archive_entry.h>

#include "debug.h"

#define BUFSIZE (128*1024)

static void fill_archive_entry(struct archive * a, struct archive_entry * entry, rpmfi fi)
{
    archive_entry_clear(entry);

    char * filename = rstrscat(NULL, ".", rpmfiDN(fi), rpmfiBN(fi), NULL);
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

static int process_package(rpmts ts, char * filename)
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
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);

    if (!strcmp(filename, "-")) {
	archive_write_open_fd(a, 1);
    } else {
	char * outname = rstrscat(NULL, filename, ".tgz", NULL);
	archive_write_open_filename(a, outname);
	_free(outname);
	// XXX error handling
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

	fill_archive_entry(a, entry, fi);

	if (nlink > 1) {
	    if (rpmfiArchiveHasContent(fi)) {
		_free(hardlink);
		hardlink = rstrscat(NULL, ".", rpmfiFN(fi), NULL);
	    } else {
		archive_entry_set_hardlink(entry, hardlink);
	    }
	}

	archive_write_header(a, entry);

	if (S_ISREG(mode) && (nlink == 1 || rpmfiArchiveHasContent(fi))) {
	    write_file_content(a, buf, fi);
	}
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

int main(int argc, char *argv[])
{
    int rc;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    rpmReadConfigFiles(NULL, NULL);
    char * filename;
    if (argc == 1)
	filename = "-";
    else {
	if (rstreq(argv[1], "-h") || rstreq(argv[1], "--help")) {
	    fprintf(stderr, "Usage: rpm2archive file.rpm\n");
	    exit(EXIT_FAILURE);
	} else {
	    filename = argv[1];
	}
    }

    rpmts ts = rpmtsCreate();
    rpmVSFlags vsflags = 0;

    /* XXX retain the ageless behavior of rpm2cpio */
    vsflags |= _RPMVSF_NODIGESTS;
    vsflags |= _RPMVSF_NOSIGNATURES;
    vsflags |= RPMVSF_NOHDRCHK;
    (void) rpmtsSetVSFlags(ts, vsflags);

    rc = process_package(ts, filename);

    ts = rpmtsFree(ts);

    return rc;
}
