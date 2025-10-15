#include "system.h"

#include <popt.h>
#include <libgen.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include <rpm/rpmcli.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>

#include "debug.h"

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
 * @return	only top level directory (if any),
 * 		NULL if it contains multiple top level entries or a single file
 * 		or on archive error
 */
static char * singleRoot(const char *path)
{
	struct archive *a;
	struct archive_entry *entry;
	int r, ret = -1, rootLen;
	char *rootName = NULL;
	char *sep = NULL;

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
	sep = strchr(rootName, '/');
	if (sep == NULL) {
	    /* No directories in the pathname */
	    ret = 0;
	    goto afree;
	}

	/* Do all entries in the archive start with the same lead directory? */
	rootLen = sep - rootName + 1;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
	    const char *p = archive_entry_pathname(entry);
	    if (strncmp(rootName, p, rootLen)) {
		ret = 0;
		goto afree;
	    }
	}
	*sep = '\0';
	ret = 1;

afree:
	r = archive_read_free(a);
	if (r != ARCHIVE_OK)
	    ret = -1;

	if (ret != 1)
	    rootName = _free(rootName);

	return rootName;
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
	char * sr = singleRoot(fn);

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
		    "(shopt -s dotglob; mv \"$tmp\"/'%s'/* '%s') && "
		    "rmdir \"$tmp\"/'%s' \"$tmp\" ", sr, dstpath, sr);
		rasprintf(&stripcd, "%s\"$tmp\" %s", at->dest, moveup);
		free(moveup);
	    } else {
		rasprintf(&mkdir, "mkdir -p '%s' ; ", dstpath);
		rasprintf(&stripcd, "%s'%s'", at->dest, dstpath);
	    }
	}
	free(sr);
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
