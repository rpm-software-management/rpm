#include "system.h"

#include <popt.h>
#include <libgen.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include <rpm/rpmcli.h>
#include <rpm/rpmstring.h>

#include "debug.h"

static int verbose = 0;
static int extract = 0;
static int dryrun = 0;
static char *dstpath = NULL;

static struct poptOption optionsTable[] = {
    { "extract", 'x', POPT_ARG_VAL, &extract, 1,
	N_("extract an archive"), NULL },
    { "verbose", 'v', POPT_ARG_VAL, &verbose, 1,
	N_("provide more detailed output"), NULL },
    { "dry-run", 'n', POPT_ARG_VAL, &dryrun, 1,
	N_("only print what would be done"), NULL },
    { "path", 'C', POPT_ARG_STRING, &dstpath, 0,
	N_("extract into a specific path"), NULL },

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
    int setTZ;
} archiveTypes[] = {
    { COMPRESSED_NOT,	0,	"%{__cat}" ,	"",		"", 0 },
    { COMPRESSED_OTHER,	0,	"%{__gzip}",	"-dc",		"", 0 },
    { COMPRESSED_BZIP2,	0,	"%{__bzip2}",	"-dc",		"", 0 },
    { COMPRESSED_ZIP,	1,	"%{__unzip}",	"",		"-qq", 1 },
    { COMPRESSED_LZMA,	0,	"%{__xz}",	"-dc",		"", 0 },
    { COMPRESSED_XZ,	0,	"%{__xz}",	"-dc",		"", 0 },
    { COMPRESSED_LZIP,	0,	"%{__lzip}",	"-dc",		"", 0 },
    { COMPRESSED_LRZIP,	0,	"%{__lrzip}",	"-dqo-",	"", 0 },
    { COMPRESSED_7ZIP,	1,	"%{__7zip}",	"x",		"", 0 },
    { COMPRESSED_ZSTD,	0,	"%{__zstd}",	"-dc",		"", 0 },
    { COMPRESSED_GEM,	1,	"%{__gem}",	"unpack",	"", 0 },
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

/**
 * Detect if an archive has a single top level entry, and it's a directory.
 *
 * @param path	path of the archive
 * @return	1 if archive as only a directory as top level entry,
 * 		0 if it contains multiple top level entries or a single file
 * 		-1 on archive error
 */
static int singleRoot(const char *path)
{
	struct archive *a;
	struct archive_entry *entry;
	int r, ret = -1, rootLen;
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
	rootName = xstrdup(archive_entry_pathname(entry));
	rootLen = strlen(rootName);
	if (archive_entry_filetype(entry) != AE_IFDIR) {
	    /* Root entry is not a directory */
	    ret = 0;
	    goto afree;
	}
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
	    if (strncmp(rootName, archive_entry_pathname(entry), rootLen)) {
		/* multiple top level entries */
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
    const char *taropts = verbose ? "-xvvof" : "-xof";
    char *mkdir = NULL;
    char *stripcd = NULL;

    if ((at = getArchiver(fn)) == NULL)
	goto exit;

    if (dstpath) {
	    int sr = singleRoot(fn);

	    /* the trick is simple, if the archive has multiple entries,
	     * just extract it into the specified destination path, otherwise
	     * strip the first path entry and extract in the destination path
	     */
	    rasprintf(&mkdir, "mkdir '%s' ; ", dstpath);
	    rasprintf(&stripcd, " -C '%s' %s", dstpath, sr ? "--strip-components=1" : "");
    }
    tar = rpmGetPath("%{__tar}", NULL);
    if (at->compressed != COMPRESSED_NOT) {
	char *zipper = NULL;
	int needtar = (at->extractable == 0);

	zipper = rpmExpand(at->setTZ ? "TZ=UTC " : "",
			   at->cmd, " ", at->unpack, " ",
			   verbose ? "" : at->quiet, NULL);
	if (needtar) {
	    rasprintf(&buf, "%s %s '%s' | %s %s - %s", mkdir ?: "", zipper, fn, tar, taropts, stripcd ?: "");
	} else if (at->compressed == COMPRESSED_GEM) {
	    char *tmp = xstrdup(fn);
	    const char *bn = basename(tmp);
	    size_t nvlen = strlen(bn) - 3;
	    char *gem = rpmGetPath("%{__gem}", NULL);
	    char *gemspec = NULL;
	    char gemnameversion[nvlen];

	    rstrlcpy(gemnameversion, bn, nvlen);
	    gemspec = rpmGetPath("", gemnameversion, ".gemspec", NULL);

	    rasprintf(&buf, "%s '%s' && %s spec '%s' --ruby > '%s'",
			zipper, fn, gem, fn, gemspec);

	    free(gemspec);
	    free(gem);
	    free(tmp);
	} else {
	    rasprintf(&buf, "%s '%s'", zipper, fn);
	}
	free(zipper);
    } else {
	rasprintf(&buf, "%s %s %s '%s' %s", mkdir ?: "", tar, taropts, fn, stripcd ?: "");
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

    cmd = extract ? doUntar(arg) : doUncompress(arg);
    if (cmd) {
	FILE *inp = NULL;

	if (verbose || dryrun)
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
