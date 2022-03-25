#include "system.h"

#include <popt.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <rpm/rpmcli.h>
#include <rpm/rpmstring.h>

#include "debug.h"

static int verbose = 0;
static int extract = 0;

static struct poptOption optionsTable[] = {
    { "extract", 'x', POPT_ARG_VAL, &extract, 1,
	N_("extract an archive"), NULL },
    { "verbose", 'v', POPT_ARG_VAL, &verbose, 1,
	N_("provide more detailed output"), NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

/* XXX this is silly, merge with the doUntar() uncompress logic */
static char *doUncompress(const char *fn)
{
    return rpmExpand("%{uncompress:", fn, "}", NULL);
}

static char *doUntar(const char *fn)
{
    char *buf = NULL;
    char *tar = NULL;
    const char *taropts = verbose ? "-xvvof" : "-xof";
    rpmCompressedMagic compressed = COMPRESSED_NOT;

    /* XXX On non-build parse's, file cannot be stat'd or read */
    if (rpmFileIsCompressed(fn, &compressed)) {
	goto exit;
    }

    tar = rpmGetPath("%{__tar}", NULL);
    if (compressed != COMPRESSED_NOT) {
	char *zipper = NULL;
	const char *t = NULL;
	int needtar = 1;
	int needgemspec = 0;

	switch (compressed) {
	case COMPRESSED_NOT:	/* XXX can't happen */
	case COMPRESSED_OTHER:
	    t = "%{__gzip} -dc";
	    break;
	case COMPRESSED_BZIP2:
	    t = "%{__bzip2} -dc";
	    break;
	case COMPRESSED_ZIP:
	    if (verbose)
		t = "%{__unzip}";
	    else
		t = "%{__unzip} -qq";
	    needtar = 0;
	    break;
	case COMPRESSED_LZMA:
	case COMPRESSED_XZ:
	    t = "%{__xz} -dc";
	    break;
	case COMPRESSED_LZIP:
	    t = "%{__lzip} -dc";
	    break;
	case COMPRESSED_LRZIP:
	    t = "%{__lrzip} -dqo-";
	    break;
	case COMPRESSED_7ZIP:
	    t = "%{__7zip} x";
	    needtar = 0;
	    break;
	case COMPRESSED_ZSTD:
	    t = "%{__zstd} -dc";
	    break;
	case COMPRESSED_GEM:
	    t = "%{__gem} unpack";
	    needtar = 0;
	    needgemspec = 1;
	    break;
	}

	zipper = rpmGetPath(t, NULL);
	if (needtar) {
	    rasprintf(&buf, "%s '%s' | %s %s -", zipper, fn, tar, taropts);
	} else if (needgemspec) {
	    size_t nvlen = strlen(fn) - 3;
	    char *gem = rpmGetPath("%{__gem}", NULL);
	    char *gemspec = NULL;
	    char gemnameversion[nvlen];

	    rstrlcpy(gemnameversion, fn, nvlen);
	    gemspec = rpmGetPath("", gemnameversion, ".gemspec", NULL);

	    rasprintf(&buf, "%s '%s' && %s spec '%s' --ruby > '%s'",
			zipper, fn, gem, fn, gemspec);

	    free(gemspec);
	    free(gem);
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

	if (verbose)
	    fprintf(stderr, "%s\n", cmd);

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
