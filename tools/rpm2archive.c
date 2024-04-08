/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include <rpm/rpmlib.h>		/* rpmReadPackageFile .. */
#include <rpm/rpmfi.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmio.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmts.h>

#include <popt.h>

#include <archive.h>
#include <archive_entry.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

#include "debug.h"

#define BUFSIZE (128*1024)

int compress = 1;
const char *format = "pax";

static struct poptOption optionsTable[] = {
    { "nocompression", 'n', POPT_ARG_VAL, &compress, 0,
        N_("create uncompressed archive"),
        NULL },
    { "format", 'f', POPT_ARG_STRING, &format, 0,
	N_("archive format (pax|cpio)"),
        NULL },
    POPT_AUTOHELP
    POPT_TABLEEND
};

static void fill_archive_entry(struct archive_entry * entry, rpmfi fi,
				char **hardlink)
{
    archive_entry_clear(entry);
    const char * dn = rpmfiDN(fi);
    if (!strcmp(dn, "")) dn = "/";
    struct stat sb;

    char * filename = rstrscat(NULL, ".", dn, rpmfiBN(fi), NULL);
    archive_entry_copy_pathname(entry, filename);
    _free(filename);

    rpmfiStat(fi, 0, &sb);
    archive_entry_copy_stat(entry, &sb);

    archive_entry_set_uname(entry, rpmfiFUser(fi));
    archive_entry_set_gname(entry, rpmfiFGroup(fi));

    if (S_ISLNK(sb.st_mode))
	archive_entry_set_symlink(entry, rpmfiFLink(fi));

    if (sb.st_nlink > 1) {
	if (rpmfiArchiveHasContent(fi)) {
	    /* hardlink sizes are special, see rpmfiStat() */
	    archive_entry_set_size(entry, rpmfiFSize(fi));
	    _free(*hardlink);
	    *hardlink = xstrdup(archive_entry_pathname(entry));
	} else {
	    archive_entry_set_hardlink(entry, *hardlink);
	}
    }

}

static int write_file_content(struct archive * a, char * buf, rpmfi fi)
{
    rpm_loff_t left = rpmfiFSize(fi);
    size_t len, read;

    while (left) {
	len = (left > BUFSIZE ? BUFSIZE : left);
	read = rpmfiArchiveRead(fi, buf, len);
	if (read==len) {
	    if (archive_write_data(a, buf, len) < 0) {
		fprintf(stderr, "Error writing archive: %s\n",
				archive_error_string(a));
		break;
	    }
	} else {
	    fprintf(stderr, "Error reading file from rpm payload\n");
	    break;
	}
	left -= len;
    }

    return (left > 0);
}

/* This code sets the charset of the open archive. Messing with the
   locale is currently the only way to do it, see:
   https://github.com/libarchive/libarchive/pull/1966
 */
static void set_archive_utf8(struct archive * a)
{
#ifdef ENABLE_NLS
    const char * t = setlocale(LC_CTYPE, NULL);
    char * old_ctype = t ? xstrdup(t) : NULL;
    (void) setlocale(LC_CTYPE, C_LOCALE);
    (void) archive_write_set_options(a, "hdrcharset=UTF-8");
    if (old_ctype) {
	(void) setlocale(LC_CTYPE, old_ctype);
	free(old_ctype);
    }
#endif
}

static int process_package(rpmts ts, const char * filename)
{
    FD_t fdi;
    FD_t gzdi;
    Header h;
    int rc = 0;
    int e;	/* libarchive return code */
    int format_code = 0;
    char * rpmio_flags = NULL;
    struct archive *a;
    struct archive_entry *entry;

    if (!strcmp(filename, "-")) {
        if(isatty(STDIN_FILENO)) {
            fprintf(stderr, "Error: missing input RPM package\n");
            exit(EXIT_FAILURE);
        }
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

    /* create archive */
    a = archive_write_new();
    if (compress) {
	if (archive_write_add_filter_gzip(a) != ARCHIVE_OK) {
	    fprintf(stderr, "%s\n", archive_error_string(a));
	    exit(EXIT_FAILURE);
	}
    }

    if (rstreq(format, "pax")) {
	format_code = ARCHIVE_FORMAT_TAR_PAX_RESTRICTED;
    } else if (rstreq(format, "cpio")) {
	format_code = ARCHIVE_FORMAT_CPIO_SVR4_NOCRC;
    } else {
	fprintf(stderr, "Error: Format %s is not supported\n", format);
	exit(EXIT_FAILURE);
    }

    if (archive_write_set_format(a, format_code) != ARCHIVE_OK) {
	fprintf(stderr, "Error: Format %s is not supported\n", format);
	exit(EXIT_FAILURE);
    }
    if (format_code == ARCHIVE_FORMAT_TAR_PAX_RESTRICTED)
	set_archive_utf8(a);

    if (!isatty(STDOUT_FILENO)) {
	archive_write_open_fd(a, STDOUT_FILENO);
    } else {
        if (!strcmp(filename, "-")) {
	    fprintf(stderr, "Error: refusing to output archive data to a terminal.\n");
	    exit(EXIT_FAILURE);
	}
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

    char * buf = (char *)xmalloc(BUFSIZE);
    char * hardlink = NULL;

    rpmfiles files = rpmfilesNew(NULL, h, 0, RPMFI_KEEPHEADER);
    rpmfi fi = rpmfiNewArchiveReader(gzdi, files, format_code == ARCHIVE_FORMAT_CPIO_SVR4_NOCRC ? RPMFI_ITER_READ_ARCHIVE : RPMFI_ITER_READ_ARCHIVE_CONTENT_FIRST);

    while ((rc = rpmfiNext(fi)) >= 0) {
	fill_archive_entry(entry, fi, &hardlink);

	e = archive_write_header(a, entry);
	if (e == ARCHIVE_FAILED && archive_errno(a) == ERANGE) {
	    fprintf(stderr, "Warning: file too large for format, skipping: %s\n",
			    rpmfiFN(fi));
	    continue;
	}
	if (e == ARCHIVE_WARN) {
	    fprintf(stderr, "Warning writing archive: %s (%d)\n",
			    archive_error_string(a), archive_errno(a));
	} else if (e != ARCHIVE_OK) {
	    fprintf(stderr, "Error writing archive: %s (%d)\n",
			    archive_error_string(a), archive_errno(a));
	    break;
	}
	if (S_ISREG(archive_entry_mode(entry)) && rpmfiArchiveHasContent(fi)) {
	    if (write_file_content(a, buf, fi)) {
		break;
	    }
	}
    }
    /* End of iteration is not an error, everything else is */
    if (rc == RPMERR_ITER_END) {
	rc = 0;
    } else {
	rc = 1;
    }

    _free(hardlink);

    Fclose(gzdi);	/* XXX gzdi == fdi */
    archive_entry_free(entry);
    if (archive_write_close(a) != ARCHIVE_OK) {
	fprintf(stderr, "Error writing archive: %s\n", archive_error_string(a));
	rc = 1;
    }
    archive_write_free(a);
    buf = _free(buf);
    rpmfilesFree(files);
    rpmfiFree(fi);
    headerFree(h);
    return rc;
}

int main(int argc, char *argv[])
{
    int rc = 0;
    poptContext optCon;
    const char *fn;

    xsetprogname(argv[0]);	/* Portability call -- see system.h */
    rpmReadConfigFiles(NULL, NULL);

    optCon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "[OPTIONS]* <FILES>");

    if (rstreq(basename(argv[0]), "rpm2cpio")) {
	format = "cpio";
	compress = 0;
    }

    while ((rc = poptGetNextOpt(optCon)) != -1) {
	if (rc < 0) {
	    fprintf(stderr, "%s: %s\n",
		    poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		    poptStrerror(rc));
	    exit(EXIT_FAILURE);
	}
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
