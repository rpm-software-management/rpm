/*@-bounds@*/
#include "system.h"

#include <rpmbuild.h>
#include <argv.h>
#include <rpmfc.h>

#include "debug.h"

/**
 */
/*@unchecked@*/ /*@observer@*/
static struct rpmfcTokens_s rpmfcTokens[] = {
  { "directory",		RPMFC_DIRECTORY|RPMFC_INCLUDE },

  { " shared object",		RPMFC_LIBRARY },
  { " executable",		RPMFC_EXECUTABLE },
  { " statically linked",	RPMFC_STATIC },
  { " not stripped",		RPMFC_NOTSTRIPPED },
  { " archive",			RPMFC_ARCHIVE },

  { "ELF 32-bit",		RPMFC_ELF32|RPMFC_INCLUDE },
  { "ELF 64-bit",		RPMFC_ELF64|RPMFC_INCLUDE },

  { " script",			RPMFC_SCRIPT },
  { " text",			RPMFC_TEXT },
  { " document",		RPMFC_DOCUMENT },

  { " compressed",		RPMFC_COMPRESSED },

  { "troff or preprocessor input",		RPMFC_MANPAGE },

  { "current ar archive",	RPMFC_STATIC|RPMFC_LIBRARY|RPMFC_ARCHIVE|RPMFC_INCLUDE },

  { "Zip archive data",		RPMFC_COMPRESSED|RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "tar archive",		RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "cpio archive",		RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "RPM v3",			RPMFC_ARCHIVE|RPMFC_INCLUDE },

  { " image",			RPMFC_IMAGE|RPMFC_INCLUDE },
  { " font",			RPMFC_FONT|RPMFC_INCLUDE },
  { " Font",			RPMFC_FONT|RPMFC_INCLUDE },

  { " commands",		RPMFC_SCRIPT|RPMFC_INCLUDE },
  { " script",			RPMFC_SCRIPT|RPMFC_INCLUDE },

  { "python compiled",		RPMFC_WHITE|RPMFC_INCLUDE },

  { "empty",			RPMFC_WHITE|RPMFC_INCLUDE },

  { "HTML",			RPMFC_WHITE|RPMFC_INCLUDE },
  { "SGML",			RPMFC_WHITE|RPMFC_INCLUDE },
  { "XML",			RPMFC_WHITE|RPMFC_INCLUDE },

  { " program text",		RPMFC_WHITE|RPMFC_INCLUDE },
  { " source",			RPMFC_WHITE|RPMFC_INCLUDE },
  { "GLS_BINARY_LSB_FIRST",	RPMFC_WHITE|RPMFC_INCLUDE },
  { " DB ",			RPMFC_WHITE|RPMFC_INCLUDE },

  { "ASCII English text",	RPMFC_WHITE|RPMFC_INCLUDE },
  { "ASCII text",		RPMFC_WHITE|RPMFC_INCLUDE },
  { "ISO-8859 text",		RPMFC_WHITE|RPMFC_INCLUDE },

  { "symbolic link to",		RPMFC_SYMLINK },
  { "socket",			RPMFC_DEVICE },
  { "special",			RPMFC_DEVICE },

  { "ASCII",			RPMFC_WHITE },
  { "ISO-8859",			RPMFC_WHITE },

  { "data",			RPMFC_WHITE },

  { "application",		RPMFC_WHITE },
  { "boot",			RPMFC_WHITE },
  { "catalog",			RPMFC_WHITE },
  { "code",			RPMFC_WHITE },
  { "file",			RPMFC_WHITE },
  { "format",			RPMFC_WHITE },
  { "message",			RPMFC_WHITE },
  { "program",			RPMFC_WHITE },

  { "broken symbolic link to ",	RPMFC_WHITE|RPMFC_ERROR },
  { "can't read",		RPMFC_WHITE|RPMFC_ERROR },
  { "can't stat",		RPMFC_WHITE|RPMFC_ERROR },
  { "executable, can't read",	RPMFC_WHITE|RPMFC_ERROR },
  { "core file",		RPMFC_WHITE|RPMFC_ERROR },

  { NULL,			RPMFC_BLACK }
};

/*@unchecked@*/
static int fcolorIgnore =
    (RPMFC_ELF32|RPMFC_ELF64|RPMFC_DIRECTORY|RPMFC_LIBRARY|RPMFC_ARCHIVE|RPMFC_FONT|RPMFC_SCRIPT|RPMFC_IMAGE|RPMFC_WHITE);

int rpmfcColoring(const char * fmstr)
{
    rpmfcToken fct;
    int fcolor = RPMFC_BLACK;

    for (fct = rpmfcTokens; fct->token != NULL; fct++) {
	if (strstr(fmstr, fct->token) == NULL)
	    continue;
	fcolor |= fct->colors;
	if (fcolor & RPMFC_INCLUDE)
	    return fcolor;
    }
    return fcolor;
}

void rpmfcPrint(const char * msg, rpmfc fc, FILE * fp)
{
    int ac = 0;
    int fcolor;
    int ix;

    if (fp == NULL) fp = stderr;

    if (msg)
	fprintf(fp, "===================================== %s\n", msg);

    if (fc)
    while (1) {
	if (fc->fn[ac] == NULL)
	    break;
	if (ac >= fc->fdictx->nvals)
	    break;
	ix = fc->fdictx->vals[ac];
	if (ix < 0)
	    break;

	fcolor = fc->fcolor->vals[ac];
	if (ix > 0 && !(fcolor & fcolorIgnore)) {
	    fprintf(fp, "%3d %s", ix, fc->fn[ac]);
	    if (fcolor != RPMFC_BLACK)
		fprintf(fp, "\t0x%x", fc->fcolor->vals[ac]);
	    else
		fprintf(fp, "\t%s", fc->dict[ix]);
	    fprintf(fp, "\n");
	}

	ac++;
    }
}

rpmfc rpmfcFree(rpmfc fc)
{
    if (fc) {
	fc->fn = argvFree(fc->fn);
	fc->fdictx = argiFree(fc->fdictx);
	fc->fcolor = argiFree(fc->fcolor);
	fc->dict = argvFree(fc->dict);
    }
    fc = _free(fc);
    return NULL;
}

rpmfc rpmfcNew(void)
{
    rpmfc fc = xcalloc(1, sizeof(*fc));
    return fc;
}

static int rpmfcELF(rpmfc fc)
{
    return 0;
}

static int rpmfcSCRIPT(rpmfc fc)
{
    return 0;
}

int rpmfcClassify(rpmfc *fcp, ARGV_t argv)
{
    rpmfc fc;
    char buf[BUFSIZ];
    ARGV_t dav;
    const char * s, * se;
    char * t;
    int fcolor;
    int xx;
    int fknown, fwhite;


    if (fcp == NULL || argv == NULL)
	return 0;

    if (*fcp == NULL)
	*fcp = rpmfcNew();
    fc = *fcp;

    /* Set up the file class dictionary. */
    xx = argvAdd(&fc->dict, "");
    xx = argvAdd(&fc->dict, "directory");

/*@-temptrans@*/
    fc->av = argv;
/*@=temptrans@*/
    while ((s = *fc->av++) != NULL) {
	se = s;
	while (*se && !(se[0] == ':' && se[1] == ' '))
	    se++;
	if (*se == '\0')
	    return -1;

	se++;
	while (*se && (*se == ' ' || *se == '\t'))
	    se++;
	if (*se == '\0')
	    return -1;

	fcolor = rpmfcColoring(se);
	if (fcolor == RPMFC_WHITE || !(fcolor & RPMFC_INCLUDE))
	    continue;

	dav = argvSearch(fc->dict, se, NULL);
	if (dav == NULL) {
	    xx = argvAdd(&fc->dict, se);
	    xx = argvSort(fc->dict, NULL);
	}
    }

    /* Classify files. */
/*@-kepttrans@*/
    fc->av = argv;
/*@=kepttrans@*/
    fc->ix = 0;
    fknown = 0;
    while ((s = *fc->av++) != NULL) {
	se = s;
	while (*se && !(se[0] == ':' && se[1] == ' '))
	    se++;
	if (*se == '\0')
	    return -1;

	t = stpncpy(buf, s, (se - s));
	*t = '\0';

	xx = argvAdd(&fc->fn, buf);

	se++;
	while (*se && (*se == ' ' || *se == '\t'))
	    se++;
	if (*se == '\0')
	    return -1;

	dav = argvSearch(fc->dict, se, NULL);
	if (dav) {
	    xx = argiAdd(&fc->fdictx, fc->ix, (dav - fc->dict));
	    fknown++;
	} else {
	    xx = argiAdd(&fc->fdictx, fc->ix, 0);
	    fwhite++;
	}

	fcolor = rpmfcColoring(se);
	xx = argiAdd(&fc->fcolor, fc->ix, fcolor);

	if (fcolor & RPMFC_ELF) {
	    xx = rpmfcELF(fc);
	} else if (fcolor & RPMFC_SCRIPT) {
	    xx = rpmfcSCRIPT(fc);
	}

	fc->ix++;
    }
    fc->av = NULL;

/*@-modfilesys@*/
sprintf(buf, "final: files %d dict[%d] %d%%", argvCount(fc->fn), argvCount(fc->dict), ((100 * fknown)/fc->ix));
rpmfcPrint(buf, fc, NULL);
/*@=modfilesys@*/

    return 0;
}
/*@=bounds@*/
