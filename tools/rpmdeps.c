#include "system.h"

#include <rpmbuild.h>
#include <argv.h>
#include <rpmfc.h>
#include "file.h"

#include "debug.h"

/*@unchecked@*/
extern fmagic global_fmagic;

/*@unchecked@*//*@observer@*/
extern const char * default_magicfile;

/*@unchecked@*/
char *progname;

/*@unchecked@*/
static int print_provides;

/*@unchecked@*/
static int print_requires;

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL }, 

 { "rpmfcdebug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmfc_debug, -1,
        NULL, NULL },

 { "provides", 'P', POPT_ARG_VAL, &print_provides, -1,
        NULL, NULL },
 { "requires", 'R', POPT_ARG_VAL, &print_requires, -1,
        NULL, NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon;
    ARGV_t xav;
    ARGV_t av = NULL;
    rpmfc fc;
    int ac = 0;
    int ec = 1;
    int xx;
char buf[BUFSIZ];
fmagic fm = global_fmagic;
StringBuf sb_stdout;
int action = 0;
int wid = 1;
int i;

/*@-modobserver@*/
    if ((progname = strrchr(argv[0], '/')) != NULL)
	progname++;
    else
	progname = argv[0];
/*@=modobserver@*/

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	goto exit;

    av = poptGetArgs(optCon);
    ac = argvCount(av);

    if (ac == 0) {
	char * b, * be;
	av = NULL;
	while ((b = fgets(buf, sizeof(buf), stdin)) != NULL) {
	    buf[sizeof(buf)-1] = '\0';
	    be = b + strlen(buf) - 1;
	    while (strchr("\r\n", *be) != NULL)
		*be-- = '\0';
	    xx = argvAdd(&av, b);
	}
	ac = argvCount(av);
    }

    xx = argvSort(av, NULL);

    /* Output of file(1) in sb_stdout. */
    sb_stdout = newStringBuf();
    fm->magicfile = default_magicfile;
    /* XXX TODO fm->flags = ??? */

    xx = fmagicSetup(fm, fm->magicfile, action);

    for (i = 0; i < ac; i++) {
	fm->obp = fm->obuf;
	*fm->obp = '\0';
	fm->nob = sizeof(fm->obuf);
	xx = fmagicProcess(fm, av[i], wid);
	appendLineStringBuf(sb_stdout, fm->obuf);
    }

    xx = argvSplit(&xav, getStringBuf(sb_stdout), "\n");
    sb_stdout = freeStringBuf(sb_stdout);

    /* Build file class dictionary. */
    fc = rpmfcNew();
    xx = rpmfcClassify(fc, xav);

    /* Build file/package dependency dictionary. */
    xx = rpmfcApply(fc);

if (_rpmfc_debug) {
sprintf(buf, "final: files %d cdict[%d] %d%% ddictx[%d]", fc->nfiles, argvCount(fc->cdict), ((100 * fc->fknown)/fc->nfiles), argiCount(fc->ddictx));
rpmfcPrint(buf, fc, NULL);
}

    if (print_provides)
	argvPrint(NULL, fc->provides, stdout);
    if (print_requires)
	argvPrint(NULL, fc->requires, stdout);

    fc = rpmfcFree(fc);

    xav = argvFree(xav);

exit:
    optCon = rpmcliFini(optCon);
    return ec;
}
