#include "system.h"

#include <popt.h>
#include <libgen.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <rpm/rpmcli.h>
#include <rpm/rpmstring.h>

#include "debug.h"

static int verbose = 0;
static int extract = 0;
static int dryrun = 0;

static struct poptOption optionsTable[] = {
    { "extract", 'x', POPT_ARG_VAL, &extract, 1,
	N_("extract an archive"), NULL },
    { "verbose", 'v', POPT_ARG_VAL, &verbose, 1,
	N_("provide more detailed output"), NULL },
    { "dry-run", 'n', POPT_ARG_VAL, &dryrun, 1,
	N_("only print what would be done"), NULL },

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
} archiveTypes[] = {
    { COMPRESSED_NOT,	0,	"%{__cat}" ,	"",		"" },
    { COMPRESSED_OTHER,	0,	"%{__gzip}",	"-dc",		""  },
    { COMPRESSED_BZIP2,	0,	"%{__bzip2}",	"-dc",		"" },
    { COMPRESSED_ZIP,	1,	"%{__unzip}",	"",		"-qq" },
    { COMPRESSED_LZMA,	0,	"%{__xz}",	"-dc",		"" },
    { COMPRESSED_XZ,	0,	"%{__xz}",	"-dc",		"" },
    { COMPRESSED_LZIP,	0,	"%{__lzip}",	"-dc",		"" },
    { COMPRESSED_LRZIP,	0,	"%{__lrzip}",	"-dqo-",	"" },
    { COMPRESSED_7ZIP,	1,	"%{__7zip}",	"x",		"" },
    { COMPRESSED_ZSTD,	0,	"%{__zstd}",	"-dc",		"" },
    { COMPRESSED_GEM,	1,	"%{__gem}",	"unpack",	"" },
    { -1,		0,	NULL,		NULL,		NULL },
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
	cmd = rpmExpand(at->cmd, " ", at->unpack, NULL);
	/* path must not be expanded */
	cmd = rstrscat(&cmd, " ", fn, NULL);
    }
    return cmd;
}

static char *doUntar(const char *fn)
{
    const struct archiveType_s *at = NULL;
    char *buf = NULL;
    char *tar = NULL;
    const char *taropts = verbose ? "-xvvof" : "-xof";

    if ((at = getArchiver(fn)) == NULL)
	goto exit;

    tar = rpmGetPath("%{__tar}", NULL);
    if (at->compressed != COMPRESSED_NOT) {
	char *zipper = NULL;
	int needtar = (at->extractable == 0);

	zipper = rpmExpand(at->cmd, " ", at->unpack, " ",
			   verbose ? "" : at->quiet, NULL);
	if (needtar) {
	    rasprintf(&buf, "%s '%s' | %s %s -", zipper, fn, tar, taropts);
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
	rasprintf(&buf, "%s %s '%s'", tar, taropts, fn);
    }

exit:
    free(tar);
    return buf;
}

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = NULL;
    const char *arg = NULL;

    optCon = rpmcliInit(argc, argv, optionsTable);

    if (optCon == NULL || ((arg = poptGetArg(optCon)) == NULL)) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    char *cmd = extract ? doUntar(arg) : doUncompress(arg);
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
	free(cmd);
    }

exit:
    rpmcliFini(optCon);
    return ec;
}
