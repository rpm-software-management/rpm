/** \ingroup rpmcli
 * \file lib/poptI.c
 *  Popt tables for install modes.
 */

#include "system.h"

#include <rpmcli.h>

#include "debug.h"

/*@-redecl@*/
extern time_t get_date(const char * p, void * now);	/* XXX expedient lies */
/*@=redecl@*/

/*@unchecked@*/
struct rpmInstallArguments_s rpmIArgs = {
    0,			/* transFlags */
			/* probFilter */
    (RPMPROB_FILTER_REPLACEOLDFILES | RPMPROB_FILTER_REPLACENEWFILES),
    0,			/* installInterfaceFlags */
    0,			/* eraseInterfaceFlags */
    0,			/* qva_flags */
    0,			/* rbtid */
    0,			/* numRelocations */
    0,			/* noDeps */
    0,			/* incldocs */
    NULL,		/* relocations */
    NULL,		/* prefix */
    NULL		/* rootdir */
};

#define	POPT_RELOCATE		-1021
#define	POPT_EXCLUDEPATH	-1022
#define	POPT_ROLLBACK		-1023

/*@exits@*/
static void argerror(const char * desc)
	/*@globals stderr, fileSystem @*/
	/*@modifies stderr, fileSystem @*/
{
    /*@-modfilesys -globs @*/
    fprintf(stderr, _("%s: %s\n"), __progname, desc);
    /*@=modfilesys =globs @*/
    exit(EXIT_FAILURE);
}

/**
 */
/*@-bounds@*/
static void installArgCallback( /*@unused@*/ poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt, const char * arg,
		/*@unused@*/ const void * data)
	/*@globals rpmIArgs, stderr, fileSystem @*/
	/*@modifies rpmIArgs, stderr, fileSystem @*/
{
    struct rpmInstallArguments_s * ia = &rpmIArgs;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    /*@-branchstate@*/
    if (opt->arg == NULL)
    switch (opt->val) {

    case 'i':
	ia->installInterfaceFlags |= INSTALL_INSTALL;
	break;

    case POPT_EXCLUDEPATH:
	if (arg == NULL || *arg != '/') 
	    argerror(_("exclude paths must begin with a /"));
	ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
/*@-temptrans@*/
	ia->relocations[ia->numRelocations].oldPath = xstrdup(arg);
/*@=temptrans@*/
	ia->relocations[ia->numRelocations].newPath = NULL;
	ia->numRelocations++;
	break;
    case POPT_RELOCATE:
      { char * oldPath = NULL;
	char * newPath = NULL;
	
	if (arg == NULL || *arg != '/') 
	    argerror(_("relocations must begin with a /"));
	oldPath = xstrdup(arg);
	if (!(newPath = strchr(oldPath, '=')))
	    argerror(_("relocations must contain a ="));
	*newPath++ = '\0';
	if (*newPath != '/') 
	    argerror(_("relocations must have a / following the ="));
	ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
/*@-temptrans@*/
	ia->relocations[ia->numRelocations].oldPath = oldPath;
/*@=temptrans@*/
/*@-kepttrans -usereleased @*/
	ia->relocations[ia->numRelocations].newPath = newPath;
/*@=kepttrans =usereleased @*/
	ia->numRelocations++;
      }	break;

    case POPT_ROLLBACK:
      {	time_t tid;
	if (arg == NULL)
	    argerror(_("rollback takes a time/date stamp argument"));

	/*@-moduncon@*/
	tid = get_date(arg, NULL);
	/*@=moduncon@*/

	if (tid == (time_t)-1 || tid == (time_t)0)
	    argerror(_("malformed rollback time/date stamp argument"));
	ia->rbtid = tid;
      }	break;

    case RPMCLI_POPT_NODIGEST:
	ia->qva_flags |= VERIFY_DIGEST;
	break;

    case RPMCLI_POPT_NOSIGNATURE:
	ia->qva_flags |= VERIFY_SIGNATURE;
	break;

    case RPMCLI_POPT_NOHDRCHK:
	ia->qva_flags |= VERIFY_HDRCHK;
	break;

    case RPMCLI_POPT_NODEPS:
	ia->noDeps = 1;
	break;

    case RPMCLI_POPT_NOMD5:
	ia->transFlags |= RPMTRANS_FLAG_NOMD5;
	break;

    case RPMCLI_POPT_NOCONTEXTS:
	ia->transFlags |= RPMTRANS_FLAG_NOCONTEXTS;
	break;

    case RPMCLI_POPT_FORCE:
	ia->probFilter |=
		( RPMPROB_FILTER_REPLACEPKG
		| RPMPROB_FILTER_REPLACEOLDFILES
		| RPMPROB_FILTER_REPLACENEWFILES
		| RPMPROB_FILTER_OLDPACKAGE );
	break;

    case RPMCLI_POPT_NOSCRIPTS:
	ia->transFlags |= (_noTransScripts | _noTransTriggers);
	break;

    }
    /*@=branchstate@*/
}
/*@=bounds@*/

/**
 */
/*@-bitwisesigned -compmempass @*/
/*@unchecked@*/
struct poptOption rpmInstallPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	installArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "aid", '\0', POPT_BIT_SET, &rpmIArgs.transFlags, RPMTRANS_FLAG_ADDINDEPS,
	N_("add suggested packages to transaction"), NULL },

 { "allfiles", '\0', POPT_BIT_SET,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_ALLFILES,
  N_("install all files, even configurations which might otherwise be skipped"),
	NULL},
 { "allmatches", '\0', POPT_BIT_SET,
	&rpmIArgs.eraseInterfaceFlags, UNINSTALL_ALLMATCHES,
	N_("remove all packages which match <package> (normally an error is generated if <package> specified multiple packages)"),
	NULL},

 { "anaconda", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
 	&rpmIArgs.transFlags, RPMTRANS_FLAG_ANACONDA|RPMTRANS_FLAG_DEPLOOPS,
	N_("use anaconda \"presentation order\""), NULL},

 { "apply", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &rpmIArgs.transFlags,
	(_noTransScripts|_noTransTriggers|
		RPMTRANS_FLAG_APPLYONLY|RPMTRANS_FLAG_PKGCOMMIT),
	N_("do not execute package scriptlet(s)"), NULL },

 { "badreloc", '\0', POPT_BIT_SET,
	&rpmIArgs.probFilter, RPMPROB_FILTER_FORCERELOCATE,
	N_("relocate files in non-relocatable package"), NULL},

 { "deploops", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
 	&rpmIArgs.transFlags, RPMTRANS_FLAG_DEPLOOPS,
	N_("print dependency loops as warning"), NULL},

 { "dirstash", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_DIRSTASH,
	N_("save erased package files by renaming into sub-directory"), NULL},
 { "erase", 'e', POPT_BIT_SET,
	&rpmIArgs.installInterfaceFlags, INSTALL_ERASE,
	N_("erase (uninstall) package"), N_("<package>+") },
 { "excludeconfigs", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_NOCONFIGS,
	N_("do not install configuration files"), NULL},
 { "excludedocs", '\0', POPT_BIT_SET,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_NODOCS,
	N_("do not install documentation"), NULL},
 { "excludepath", '\0', POPT_ARG_STRING, 0, POPT_EXCLUDEPATH,
	N_("skip files with leading component <path> "),
	N_("<path>") },

 { "fileconflicts", '\0', POPT_BIT_CLR, &rpmIArgs.probFilter,
	(RPMPROB_FILTER_REPLACEOLDFILES | RPMPROB_FILTER_REPLACENEWFILES),
	N_("detect file conflicts between packages"), NULL},
 { "force", '\0', 0, NULL, RPMCLI_POPT_FORCE,
	N_("short hand for --replacepkgs --replacefiles"), NULL},

 { "freshen", 'F', POPT_BIT_SET, &rpmIArgs.installInterfaceFlags,
	(INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL),
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
 { "includedocs", '\0', POPT_ARGFLAG_DOC_HIDDEN, &rpmIArgs.incldocs, 0,
	N_("install documentation"), NULL},

 { "install", 'i', 0, NULL, 'i',
	N_("install package(s)"), N_("<packagefile>+") },

 { "justdb", '\0', POPT_BIT_SET, &rpmIArgs.transFlags, RPMTRANS_FLAG_JUSTDB,
	N_("update the database, but do not modify the filesystem"), NULL},

 { "noconfigs", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_NOCONFIGS,
	N_("do not install configuration files"), NULL},
 { "nodeps", '\0', 0, NULL, RPMCLI_POPT_NODEPS,
	N_("do not verify package dependencies"), NULL },
 { "nodocs", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmIArgs.transFlags, RPMTRANS_FLAG_NODOCS,
	N_("do not install documentation"), NULL},

 { "nomd5", '\0', 0, NULL, RPMCLI_POPT_NOMD5,
	N_("don't verify MD5 digest of files"), NULL },
 { "nocontexts", '\0',0,  NULL, RPMCLI_POPT_NOCONTEXTS,
	N_("don't install file security contexts"), NULL},

 { "noorder", '\0', POPT_BIT_SET,
	&rpmIArgs.installInterfaceFlags, INSTALL_NOORDER,
	N_("do not reorder package installation to satisfy dependencies"),
	NULL},

 { "nosuggest", '\0', POPT_BIT_SET, &rpmIArgs.transFlags,
	RPMTRANS_FLAG_NOSUGGEST,
	N_("do not suggest missing dependency resolution(s)"), NULL},

 { "noscripts", '\0', 0, NULL, RPMCLI_POPT_NOSCRIPTS,
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

 { "nodigest", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, RPMCLI_POPT_NODIGEST,
        N_("don't verify package digest(s)"), NULL },
 { "nohdrchk", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, RPMCLI_POPT_NOHDRCHK,
        N_("don't verify database header(s) when retrieved"), NULL },
 { "nosignature", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, RPMCLI_POPT_NOSIGNATURE,
        N_("don't verify package signature(s)"), NULL },

 { "notriggers", '\0', POPT_BIT_SET, &rpmIArgs.transFlags, _noTransTriggers,
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
	N_("ignore file conflicts between packages"), NULL},
 { "replacepkgs", '\0', POPT_BIT_SET,
	&rpmIArgs.probFilter, RPMPROB_FILTER_REPLACEPKG,
	N_("reinstall if the package is already present"), NULL},
 { "rollback", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_ROLLBACK,
	N_("deinstall new, reinstall old, package(s), back to <date>"),
	N_("<date>") },
 { "test", '\0', POPT_BIT_SET, &rpmIArgs.transFlags, RPMTRANS_FLAG_TEST,
	N_("don't install, but tell if it would work or not"), NULL},
 { "upgrade", 'U', POPT_BIT_SET,
	&rpmIArgs.installInterfaceFlags, (INSTALL_UPGRADE|INSTALL_INSTALL),
	N_("upgrade package(s)"),
	N_("<packagefile>+") },

   POPT_TABLEEND
};
/*@=bitwisesigned =compmempass @*/
