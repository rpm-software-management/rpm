/** \ingroup rpmcli
 * \file lib/poptBT.c
 *  Popt tables for build modes.
 */

#include "system.h"

#include <rpmbuild.h>
#include <rpmurl.h>

#include "build.h"
#include "debug.h"

struct rpmBuildArguments         rpmBTArgs;

#define	POPT_USECATALOG		1000
#define	POPT_NOLANG		1001
#define	POPT_RMSOURCE		1002
#define	POPT_RMBUILD		1003
#define	POPT_BUILDROOT		1004
#define	POPT_TARGETPLATFORM	1007
#define	POPT_NOBUILD		1008
#define	POPT_SHORTCIRCUIT	1009
#define	POPT_RMSPEC		1010

#define	POPT_REBUILD		0x4220
#define	POPT_RECOMPILE		0x4320
#define	POPT_BA			0x6261
#define	POPT_BB			0x6262
#define	POPT_BC			0x6263
#define	POPT_BI			0x6269
#define	POPT_BL			0x626c
#define	POPT_BP			0x6270
#define	POPT_BS			0x6273
#define	POPT_TA			0x7461
#define	POPT_TB			0x7462
#define	POPT_TC			0x7463
#define	POPT_TI			0x7469
#define	POPT_TL			0x746c
#define	POPT_TP			0x7470
#define	POPT_TS			0x7473

int noLang = 0;
static int noBuild = 0;
static int useCatalog = 0;

static void buildArgCallback( /*@unused@*/ poptContext con,
	/*@unused@*/ enum poptCallbackReason reason,
	const struct poptOption * opt, const char * arg, const void * data)
{
    struct rpmBuildArguments * rba = (struct rpmBuildArguments *) data;

    switch (opt->val) {
    case POPT_REBUILD:
    case POPT_RECOMPILE:
    case POPT_BA:
    case POPT_BB:
    case POPT_BC:
    case POPT_BI:
    case POPT_BL:
    case POPT_BP:
    case POPT_BS:
    case POPT_TA:
    case POPT_TB:
    case POPT_TC:
    case POPT_TI:
    case POPT_TL:
    case POPT_TP:
    case POPT_TS:
	if (rba->buildMode == ' ') {
	    rba->buildMode = (opt->val >> 8) & 0xff;
	    rba->buildChar = (opt->val     ) & 0xff;
	}
	break;
    case POPT_USECATALOG: rba->useCatalog = 1; break;
    case POPT_NOBUILD: rba->noBuild = 1; break;
    case POPT_NOLANG: rba->noLang = 1; break;
    case POPT_SHORTCIRCUIT: rba->shortCircuit = 1; break;
    case POPT_RMSOURCE: rba->buildAmount |= RPMBUILD_RMSOURCE; break;
    case POPT_RMSPEC: rba->buildAmount |= RPMBUILD_RMSPEC; break;
    case POPT_RMBUILD: rba->buildAmount |= RPMBUILD_RMBUILD; break;
    case POPT_BUILDROOT:
	if (rba->buildRootOverride) {
	    fprintf(stderr, _("buildroot already specified"));
	    exit(EXIT_FAILURE);
	    /*@notreached@*/
	}
	rba->buildRootOverride = xstrdup(arg);
	break;
    case POPT_TARGETPLATFORM:
	if (rba->targets) {
	    int len = strlen(rba->targets) + 1 + strlen(arg) + 1;
	    rba->targets = xrealloc(rba->targets, len);
	    strcat(rba->targets, ",");
	} else {
	    rba->targets = xmalloc(strlen(arg) + 1);
	    rba->targets[0] = '\0';
	}
	strcat(rba->targets, arg);
	break;
    }
}

/** */
struct poptOption rpmBuildPoptTable[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA,
		buildArgCallback, 0, NULL, NULL },

	{ "bp", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BP,
		N_("build through %%prep stage from spec file"), NULL},
	{ "bc", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BC,
		N_("build through %%build stage from spec file"), NULL},
	{ "bi", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BI,
		N_("build through %%install stage from spec file"), NULL},
	{ "bl", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BL,
		N_("verify %%files section from spec file"), NULL},
	{ "ba", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BA,
		N_("build source and binary package from spec file"), NULL},
	{ "bb", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BB,
		N_("build binary package from spec file"), NULL},
	{ "bs", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BS,
		N_("build source package from spec file"), NULL},

	{ "tp", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TP,
		N_("build through %%prep stage from tar ball"), NULL},
	{ "tc", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TC,
		N_("build through %%build stage from tar ball"), NULL},
	{ "ti", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TI,
		N_("build through %%install stage from tar ball"), NULL},
	{ "tl", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TL,
		N_("verify %%files section from tar ball"), NULL},
	{ "ta", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TA,
		N_("build source and binary package from tar ball"), NULL},
	{ "tb", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TB,
		N_("build binary package from tar ball"), NULL},
	{ "ts", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TS,
		N_("build source package from tar ball"), NULL},

	{ "rebuild", '\0', 0, 0, POPT_REBUILD,
		N_("build binary package from source package"), NULL},
	{ "recompile", '\0', 0, 0, POPT_REBUILD,
		N_("build through %%install stage from source package"), NULL},

	{ "buildroot", '\0', POPT_ARG_STRING, 0,  POPT_BUILDROOT,
		N_("override build root"), "DIRECTORY" },
	{ "clean", '\0', 0, 0, POPT_RMBUILD,
		N_("remove build tree when done"), NULL},
	{ "nobuild", '\0', 0, &noBuild,  POPT_NOBUILD,
		N_("do not execute any stages of the build"), NULL },
	{ "nolang", '\0', 0, &noLang, POPT_NOLANG,
		N_("do not accept I18N msgstr's from specfile"), NULL},
	{ "rmsource", '\0', 0, 0, POPT_RMSOURCE,
		N_("remove sources when done"), NULL},
	{ "rmspec", '\0', 0, 0, POPT_RMSPEC,
		N_("remove specfile when done"), NULL},
	{ "short-circuit", '\0', 0, 0,  POPT_SHORTCIRCUIT,
		N_("skip straight to specified stage (only for c,i)"), NULL },
	{ "target", '\0', POPT_ARG_STRING, 0,  POPT_TARGETPLATFORM,
		N_("override target platform"), "CPU-VENDOR-OS" },
	{ "usecatalog", '\0', 0, &useCatalog, POPT_USECATALOG,
		N_("lookup I18N strings in specfile catalog"), NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL }
};
