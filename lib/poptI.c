/** \ingroup rpmcli
 * \file lib/poptI.c
 *  Popt tables for install modes.
 */

#include "system.h"

#include <rpmcli.h>
#include <rpmurl.h>

#include "debug.h"

struct rpmInstallArguments_s rpmIArgs;

#define	POPT_RELOCATE		1016
#define	POPT_EXCLUDEPATH	1019

/*@exits@*/ static void argerror(const char * desc)
	/*@modifies fileSystem @*/
{
    fprintf(stderr, _("%s: %s\n"), __progname, desc);
    exit(EXIT_FAILURE);

}
/**
 */
static void installArgCallback( /*@unused@*/ poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt, const char * arg,
		/*@unused@*/ const void * data)
	/*@modifies rpmIArgs */
{
    struct rpmInstallArguments_s * ia = &rpmIArgs;

    switch (opt->val) {
    case POPT_EXCLUDEPATH:
	if (arg == NULL || *arg != '/') 
	    argerror(_("exclude paths must begin with a /"));
	ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
	/*@-temptrans@*/
	ia->relocations[ia->numRelocations].oldPath = arg;
	/*@=temptrans@*/
	ia->relocations[ia->numRelocations].newPath = NULL;
	ia->numRelocations++;
	break;
    case POPT_RELOCATE:
      {	char * newPath = NULL;
	if (arg == NULL || *arg != '/') 
	    argerror(_("relocations must begin with a /"));
	if (!(newPath = strchr(arg, '=')))
	    argerror(_("relocations must contain a ="));
	*newPath++ = '\0';
	if (*newPath != '/') 
	    argerror(_("relocations must have a / following the ="));
	ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
	/*@-temptrans@*/
	ia->relocations[ia->numRelocations].oldPath = arg;
	/*@=temptrans@*/
	/*@-kepttrans@*/
	ia->relocations[ia->numRelocations].newPath = newPath;
	/*@=kepttrans@*/
	ia->numRelocations++;
      }	break;
    }
}

/**
 */
struct poptOption rpmInstallPoptTable[] = {
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA,
	installArgCallback, 0, NULL, NULL },

 { "allfiles", '\0', POPT_BIT_SET,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_ALLFILES,
  N_("install all files, even configurations which might otherwise be skipped"),
	NULL},
 { "allmatches", '\0', POPT_BIT_SET,
	&rpmIArgs.eraseInterfaceFlags, UNINSTALL_ALLMATCHES,
	N_("remove all packages which match <package> (normally an error is generated if <package> specified multiple packages)"),
	NULL},

 { "apply", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &rpmIArgs.transFlags,
	(_noTransScripts|_noTransTriggers|
		RPMTRANS_FLAG_APPLYONLY|RPMTRANS_FLAG_PKGCOMMIT),
	N_("do not execute package scriptlet(s)"), NULL },

 { "badreloc", '\0', POPT_BIT_SET,
	&rpmIArgs.probFilter, RPMPROB_FILTER_FORCERELOCATE,
	N_("relocate files in non-relocateable package"), NULL},
 { "dirstash", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_DIRSTASH,
	N_("save erased package files by renaming into sub-directory"), NULL},
 { "erase", 'e', 0, 0, 'e',
	N_("erase (uninstall) package"), N_("<package>+") },
 { "excludedocs", '\0', POPT_BIT_SET,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_NODOCS,
	N_("do not install documentation"), NULL},
 { "excludepath", '\0', POPT_ARG_STRING, 0, POPT_EXCLUDEPATH,
	N_("skip files with leading component <path> "),
	N_("<path>") },
 { "force", '\0', POPT_BIT_SET, &rpmIArgs.probFilter,
	(RPMPROB_FILTER_REPLACEPKG | RPMPROB_FILTER_REPLACEOLDFILES | RPMPROB_FILTER_REPLACENEWFILES | RPMPROB_FILTER_OLDPACKAGE),
	N_("short hand for --replacepkgs --replacefiles"), NULL},
 { "freshen", 'F', POPT_BIT_SET, &rpmIArgs.installInterfaceFlags,
	(INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_NOERASE),
	N_("upgrade package(s) if already installed"),
	N_("<packagefile>+") },
 { "hash", 'h', POPT_BIT_SET, &rpmIArgs.installInterfaceFlags, INSTALL_HASH,
	N_("print hash marks as package installs (good with -v)"), NULL},
 { "ignorearch", '\0', POPT_BIT_SET,
	&rpmIArgs.probFilter, RPMPROB_FILTER_IGNOREARCH,
	N_("don't verify package architecture"), NULL},
 { "ignoreos", '\0', POPT_BIT_SET,
	&rpmIArgs.probFilter, RPMPROB_FILTER_IGNOREOS,
	N_("don't verify package operating system"), NULL},
 { "ignoresize", '\0', POPT_BIT_SET, &rpmIArgs.probFilter,
	(RPMPROB_FILTER_DISKSPACE|RPMPROB_FILTER_DISKNODES),
	N_("don't check disk space before installing"), NULL},
 { "includedocs", '\0', 0, &rpmIArgs.incldocs, 0,
	N_("install documentation"), NULL},
 { "install", '\0', POPT_BIT_SET,
	&rpmIArgs.installInterfaceFlags, INSTALL_NOERASE,
	N_("install package"), N_("<packagefile>+") },
 { "justdb", '\0', POPT_BIT_SET, &rpmIArgs.transFlags, RPMTRANS_FLAG_JUSTDB,
	N_("update the database, but do not modify the filesystem"), NULL},
 { "nodeps", '\0', 0, &rpmIArgs.noDeps, 0,
	N_("do not verify package dependencies"), NULL },
 { "noorder", '\0', POPT_BIT_SET,
	&rpmIArgs.installInterfaceFlags, INSTALL_NOORDER,
	N_("do not reorder package installation to satisfy dependencies"),
	NULL},

 { "noscripts", '\0', POPT_BIT_SET, &rpmIArgs.transFlags,
	(_noTransScripts|_noTransTriggers),
	N_("do not execute package scriptlet(s)"), NULL },
 { "nopre", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &rpmIArgs.transFlags,
	RPMTRANS_FLAG_NOPRE,
	N_("do not execute %%pre scriptlet (if any)"), NULL },
 { "nopost", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &rpmIArgs.transFlags,
	RPMTRANS_FLAG_NOPOST,
	N_("do not execute %%post scriptlet (if any)"), NULL },
 { "nopreun", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &rpmIArgs.transFlags,
	RPMTRANS_FLAG_NOPREUN,
	N_("do not execute %%preun scriptlet (if any)"), NULL },
 { "nopostun", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &rpmIArgs.transFlags,
	RPMTRANS_FLAG_NOPOSTUN,
	N_("do not execute %%postun scriptlet (if any)"), NULL },

 { "notriggers", '\0', POPT_BIT_SET, &rpmIArgs.transFlags,
	_noTransTriggers,
	N_("do not execute any scriptlet(s) triggered by this package"), NULL},
 { "notriggerprein", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_NOTRIGGERPREIN,
	N_("do not execute any %%triggerprein scriptlet(s)"), NULL},
 { "notriggerin", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_NOTRIGGERIN,
	N_("do not execute any %%triggerin scriptlet(s)"), NULL},
 { "notriggerun", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_NOTRIGGERUN,
	N_("do not execute any %%triggerun scriptlet(s)"), NULL},
 { "notriggerpostun", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_NOTRIGGERPOSTUN,
	N_("do not execute any %%triggerpostun scriptlet(s)"), NULL},

 { "oldpackage", '\0', POPT_BIT_SET,
	&rpmIArgs.probFilter, RPMPROB_FILTER_OLDPACKAGE,
	N_("upgrade to an old version of the package (--force on upgrades does this automatically)"),
	NULL},
 { "percent", '\0', POPT_BIT_SET,
	&rpmIArgs.installInterfaceFlags, INSTALL_PERCENT,
	N_("print percentages as package installs"), NULL},
 { "prefix", '\0', POPT_ARG_STRING, &rpmIArgs.prefix, 0,
	N_("relocate the package to <dir>, if relocatable"),
	N_("<dir>") },
 { "relocate", '\0', POPT_ARG_STRING, 0, POPT_RELOCATE,
	N_("relocate files from path <old> to <new>"),
	N_("<old>=<new>") },
 { "repackage", '\0', POPT_BIT_SET,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_REPACKAGE,
	N_("save erased package files by repackaging"), NULL},
 { "replacefiles", '\0', POPT_BIT_SET, &rpmIArgs.probFilter,
	(RPMPROB_FILTER_REPLACEOLDFILES | RPMPROB_FILTER_REPLACENEWFILES),
	N_("install even if the package replaces installed files"), NULL},
 { "replacepkgs", '\0', POPT_BIT_SET,
	&rpmIArgs.probFilter, RPMPROB_FILTER_REPLACEPKG,
	N_("reinstall if the package is already present"), NULL},
 { "test", '\0', POPT_BIT_SET, &rpmIArgs.transFlags, RPMTRANS_FLAG_TEST,
	N_("don't install, but tell if it would work or not"), NULL},
 { "upgrade", 'U', POPT_BIT_SET,
	&rpmIArgs.installInterfaceFlags, (INSTALL_UPGRADE|INSTALL_NOERASE),
	N_("upgrade package(s)"),
	N_("<packagefile>+") },

   POPT_TABLEEND
};
