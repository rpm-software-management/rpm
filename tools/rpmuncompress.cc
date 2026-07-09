#include "system.h"

#include <popt.h>
#include <libgen.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <filesystem>
#include <string>

#include <archive.h>
#include <archive_entry.h>

#include <rpm/rpmcli.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmts.h>

#include "debug.h"

namespace fs = std::filesystem;

static int extract = 0;
static int dryrun = 0;
static char *dstpath = NULL;

static struct poptOption extractTable[] = {
    { "extract", 'x', POPT_ARG_VAL, &extract, 1,
	N_("extract an archive"), NULL },
    { "dry-run", 'n', POPT_ARG_VAL, &dryrun, 1,
	N_("only print what would be done"), NULL },
    { "path", 'C', POPT_ARG_STRING, &dstpath, 0,
	N_("extract into a specific path"), NULL },
    POPT_TABLEEND
};

static struct poptOption optionsTable[] = {
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, extractTable, 0,
	N_("Extract options:"), NULL },
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"), NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

struct archiveType_s {
    int compressed;
    int extractable;
    const char *cmd;
    const char *unpack;
    const char *quiet;
    const char *dest;
    int setTZ;
} archiveTypes[] = {
    { COMPRESSED_NOT,	0,	"%{__cat}" ,	"",		"", "", 0 },
    { COMPRESSED_OTHER,	0,	"%{__gzip}",	"-dc",		"", "", 0 },
    { COMPRESSED_BZIP2,	0,	"%{__bzip2}",	"-dc",		"", "", 0 },
    { COMPRESSED_ZIP,	1,	"%{__unzip}",	"-u",		"-qq", "-d", 1 },
    { COMPRESSED_LZMA,	0,	"%{__xz}",	"-dc",		"", "", 0 },
    { COMPRESSED_XZ,	0,	"%{__xz}",	"-dc",		"", "", 0 },
    { COMPRESSED_LZIP,	0,	"%{__lzip}",	"-dc",		"", "", 0 },
    { COMPRESSED_LRZIP,	0,	"%{__lrzip}",	"-dqo-",	"", "", 0 },
    { COMPRESSED_7ZIP,	1,	"%{__7zip}",	"x -y",		"-bso0 -bsp0", "-o", 0 },
    { COMPRESSED_ZSTD,	0,	"%{__zstd}",	"-dc",		"", "", 0 },
    { COMPRESSED_GEM,	1,	"%{__gem}",	"unpack",	"", "--target=", 0 },
    { COMPRESSED_CAR,	1,	"%{__car}",	"extract -f",	"", "", 0 },
    { -1,		0,	NULL,		NULL,		NULL, 0 },
};

static const struct archiveType_s *getArchiver(const char *fn)
{
    const struct archiveType_s *archiver = NULL;
    rpmCompressedMagic compressed = COMPRESSED_NOT;

    if (rpmFileIsCompressed(fn, &compressed) == 0) {
	for (const struct archiveType_s *at = archiveTypes; at->cmd; at++) {
	    if (compressed == at->compressed) {
		archiver = at;
		break;
	    }
	}
    }

    return archiver;
}

static char *doUncompress(const char *fn)
{
    char *cmd = NULL;
    const struct archiveType_s *at = getArchiver(fn);
    if (at) {
	cmd = rpmExpand(at->setTZ ? "TZ=UTC " : "",
			at->cmd, " ", at->unpack, NULL);
	/* path must not be expanded */
	cmd = rstrscat(&cmd, " ", fn, NULL);
    }
    return cmd;
}

static int isPackage(const char *fn)
{
    static const unsigned char lead_magic[] = { 0xed, 0xab, 0xee, 0xdb };
    unsigned char magic[sizeof(lead_magic)];
    int ispkg = 0;
    FILE *f = fopen(fn, "r");

    if (f) {
	ispkg = (fread(magic, 1, sizeof(magic), f) == sizeof(magic) &&
		 memcmp(magic, lead_magic, sizeof(magic)) == 0);
	fclose(f);
    }
    return ispkg;
}

static int doPackage(const char *fn)
{
    int ec = EXIT_FAILURE;
    rpmts ts = NULL;
    FD_t fdi = NULL;
    FD_t fdo = NULL;
    char *tmppath = NULL;
    char buf[BUFSIZ];
    ssize_t n;

    if (rpmIsVerbose() || dryrun)
	fprintf(stderr, _("uncompressing rpm package %s\n"), fn);

    if (dryrun)
	return EXIT_SUCCESS;

    ts = rpmtsCreate();
    if (ts == NULL || rpmtsSetRootDir(ts, "/"))
	goto exit;

    fdi = Fopen(fn, "r.ufdio");
    if (fdi == NULL || Ferror(fdi)) {
	fprintf(stderr, _("cannot open %s: %s\n"), fn, Fstrerror(fdi));
	goto exit;
    }

    /* Materialization needs a distinct seekable regular file for output. */
    fdo = rpmMkTempFile(NULL, &tmppath);
    if (fdo == NULL || Ferror(fdo)) {
	fprintf(stderr, _("cannot create temporary file: %s\n"),
		Fstrerror(fdo));
	goto exit;
    }

    if (rpmUncompressPackage(ts, fdi, fdo) != RPMRC_OK)
	goto exit;

    if (Fseek(fdo, 0, SEEK_SET) < 0)
	goto exit;
    while ((n = Fread(buf, 1, sizeof(buf), fdo)) > 0) {
	if (fwrite(buf, 1, n, stdout) != (size_t)n)
	    goto exit;
    }
    if (n < 0 || Ferror(fdo) || fflush(stdout))
	goto exit;

    ec = EXIT_SUCCESS;

exit:
    if (fdo)
	Fclose(fdo);
    if (fdi)
	Fclose(fdi);
    if (tmppath) {
	unlink(tmppath);
	free(tmppath);
    }
    rpmtsFree(ts);
    return ec;
}

/**
 * Detect if an archive has a single top level entry, and it's a directory.
 *
 * @param path	path of the archive
 * @return	1 if the archive has a single top level, directory entry,
 * 		0 otherwise,
 * 		-1 on archive error
 */
static int singleRoot(const char *path)
{
	struct archive *a;
	struct archive_entry *entry;
	int r, ret = -1, rootLen;
	const char *p = NULL;
	const char *sep = NULL;
	char *rootName = NULL;

	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);
	r = archive_read_open_filename(a, path, 10240);
	if (r != ARCHIVE_OK) {
	    goto afree;
	}
	if (archive_read_next_header(a, &entry) != ARCHIVE_OK) {
	    goto afree;
	}

	/* Extract the lead directory from the first entry */
	p = archive_entry_pathname(entry);
	sep = strchr(p, '/');
	if (sep) {
	    rootName = xstrdup(p);
	    rootLen = sep - p + 1;
	} else if (archive_entry_filetype(entry) == AE_IFDIR) {
	    rootName = rstrscat(NULL, p, "/", NULL);
	    rootLen = strlen(rootName);
	} else {
	    /* No directories in the pathname */
	    ret = 0;
	    goto afree;
	}

	/* Do all entries in the archive start with the same lead directory? */
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
	    p = archive_entry_pathname(entry);
	    if (strncmp(rootName, p, rootLen)) {
		ret = 0;
		goto afree;
	    }
	}

	ret = 1;

afree:
	free(rootName);
	r = archive_read_free(a);
	if (r != ARCHIVE_OK)
	    ret = -1;

	return ret;
}

static char *doUntar(const char *fn)
{
    const struct archiveType_s *at = NULL;
    char *buf = NULL;
    char *tar = NULL;
    const char *taropts = rpmIsVerbose() ? "-xvvof" : "-xof";
    char *mkdir = NULL;
    char *stripcd = NULL;
    int needtar = 0;

    if ((at = getArchiver(fn)) == NULL)
	goto exit;

    needtar = (at->extractable == 0);

    if (dstpath) {
	int sr = singleRoot(fn);

	/* if the archive has multiple entries, just extract it into the
	 * specified destination path, otherwise also strip the first path
	 * entry
	 */
	if (needtar) {
	    rasprintf(&mkdir, "mkdir -p '%s' ; ", dstpath);
	    rasprintf(&stripcd, " -C '%s' %s", dstpath, sr ? "--strip-components=1" : "");
	} else {
	    if (sr) {
		rasprintf(&mkdir, "mkdir -p '%s' ; tmp=`mktemp -d -p'%s'` ; ",
			  dstpath, dstpath);
		char * moveup;
		/* Extract into temp directory to avoid collisions */
		/* then move files in top dir two levels up */
		rasprintf(
		    &moveup,
		    " && "
		    "(shopt -s dotglob; mv \"$tmp\"/*/* '%s') && "
		    "rmdir \"$tmp\"/* \"$tmp\" ", dstpath);
		rasprintf(&stripcd, "%s\"$tmp\" %s", at->dest, moveup);
		free(moveup);
	    } else {
		rasprintf(&mkdir, "mkdir -p '%s' ; ", dstpath);
		rasprintf(&stripcd, "%s'%s'", at->dest, dstpath);
	    }
	}
    } else {
	mkdir = xstrdup("");
	stripcd = xstrdup("");
    }
    tar = rpmGetPath("%{__tar}", NULL);
    if (at->compressed != COMPRESSED_NOT) {
	char *zipper = NULL;

	zipper = rpmExpand(at->setTZ ? "TZ=UTC " : "",
			   at->cmd, " ", at->unpack, " ",
			   rpmIsVerbose() ? "" : at->quiet, NULL);
	if (needtar) {
	    rasprintf(&buf, "%s %s '%s' | %s %s - %s", mkdir, zipper, fn, tar, taropts, stripcd);
	} else if (at->compressed == COMPRESSED_GEM) {
	    auto bn = fs::path(fn).stem();
	    char *gem = rpmGetPath("%{__gem}", NULL);

	    rasprintf(&buf, "%s '%s' && %s spec '%s' --ruby > '%s.gemspec'",
			zipper, fn, gem, fn, bn.c_str());

	    free(gem);
	} else {
	    rasprintf(&buf, "%s%s '%s' %s", mkdir, zipper, fn, stripcd);
	}
	free(zipper);
    } else {
	rasprintf(&buf, "%s %s %s '%s' %s", mkdir, tar, taropts, fn, stripcd);
    }

exit:
    free(tar);
    free(mkdir);
    free(stripcd);
    return buf;
}

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = NULL;
    const char *arg = NULL;
    char *cmd = NULL;

    optCon = rpmcliInit(argc, argv, optionsTable);

    if (optCon == NULL || ((arg = poptGetArg(optCon)) == NULL)) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    if (isPackage(arg)) {
	if (extract) {
	    fprintf(stderr,
		    _("rpm packages cannot be extracted, use rpm2archive(1)\n"));
	    goto exit;
	}
	ec = doPackage(arg);
	goto exit;
    }

    cmd = extract ? doUntar(arg) : doUncompress(arg);
    if (cmd) {
	FILE *inp = NULL;

	if (rpmIsVerbose() || dryrun)
	    fprintf(stderr, "%s\n", cmd);

	if (dryrun) {
	    ec = EXIT_SUCCESS;
	    goto exit;
	}

	inp = popen(cmd, "r");
	if (inp) {
	    int status, c;
	    while ((c = fgetc(inp)) != EOF)
		fputc(c, stdout);
	    status = pclose(inp);
	    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		ec = EXIT_SUCCESS;
	}
    }

exit:
    free(cmd);
    rpmcliFini(optCon);
    return ec;
}
