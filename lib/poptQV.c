/** \ingroup rpmcli
 * \file lib/poptQV.c
 *  Popt tables for query/verify modes.
 */

#include "system.h"

#include <rpmcli.h>
#include <rpmbuild.h>

#include "debug.h"

/*@unchecked@*/
struct rpmQVArguments_s rpmQVArgs;
/*@unchecked@*/
int specedit = 0;

#define POPT_QUERYFORMAT	1000
#define POPT_WHATREQUIRES	1001
#define POPT_WHATPROVIDES	1002
#define POPT_QUERYBYNUMBER	1003
#define POPT_TRIGGEREDBY	1004
#define POPT_DUMP		1005
#define POPT_SPECFILE		1006

/* ========== Query/Verify source popt args */
static void rpmQVSourceArgCallback( /*@unused@*/ poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt, /*@unused@*/ const char * arg, 
		/*@unused@*/ const void * data)
	/*@globals rpmQVArgs @*/
	/*@modifies rpmQVArgs @*/
{
    QVA_t qva = &rpmQVArgs;

    switch (opt->val) {
    case 'q':
    case 'Q':
    case 'V':
	if (qva->qva_mode == ' ') {
	    qva->qva_mode = opt->val;
	    qva->qva_char = ' ';
	}
	break;
    case 'a': qva->qva_source |= RPMQV_ALL; qva->qva_sourceCount++; break;
    case 'f': qva->qva_source |= RPMQV_PATH; qva->qva_sourceCount++; break;
    case 'g': qva->qva_source |= RPMQV_GROUP; qva->qva_sourceCount++; break;
    case 'p': qva->qva_source |= RPMQV_RPM; qva->qva_sourceCount++; break;
    case POPT_WHATPROVIDES: qva->qva_source |= RPMQV_WHATPROVIDES; 
			      qva->qva_sourceCount++; break;
    case POPT_WHATREQUIRES: qva->qva_source |= RPMQV_WHATREQUIRES; 
			      qva->qva_sourceCount++; break;
    case POPT_TRIGGEREDBY: qva->qva_source |= RPMQV_TRIGGEREDBY;
			      qva->qva_sourceCount++; break;

/* XXX SPECFILE is not verify sources */
    case POPT_SPECFILE:
	qva->qva_source |= RPMQV_SPECFILE;
	qva->qva_sourceCount++;
	break;
    case POPT_QUERYBYNUMBER:
	qva->qva_source |= RPMQV_DBOFFSET; 
	qva->qva_sourceCount++;
	break;
    }
}

/**
 * Common query/verify mode options.
 */
/*@unchecked@*/
struct poptOption rpmQVSourcePoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
	rpmQVSourceArgCallback, 0, NULL, NULL },
/*@=type@*/
 { "all", 'a', 0, 0, 'a',
	N_("query/verify all packages"), NULL },
 { "file", 'f', 0, 0, 'f',
	N_("query/verify package(s) owning file"), "FILE" },
 { "group", 'g', 0, 0, 'g',
	N_("query/verify package(s) in group"), "GROUP" },
 { "package", 'p', 0, 0, 'p',
	N_("query/verify a package file (i.e. a binary *.rpm file)"), NULL },
 { "query", 'q', POPT_ARGFLAG_DOC_HIDDEN, NULL, 'q',
	N_("rpm query mode"), NULL },
 { "querybynumber", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, 
	POPT_QUERYBYNUMBER, NULL, NULL },
 { "querytags", '\0', 0, 0, 'Q',
	N_("display known query tags"), NULL },
 { "specfile", '\0', 0, 0, POPT_SPECFILE,
	N_("query a spec file"), N_("<spec>") },
 { "triggeredby", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_TRIGGEREDBY, 
	N_("query the package(s) triggered by the package"), "PACKAGE" },
 { "verify", 'V', POPT_ARGFLAG_DOC_HIDDEN, NULL, 'V',
	N_("rpm verify mode"), NULL },
 { NULL, 'y',  POPT_ARGFLAG_DOC_HIDDEN, NULL, 'V',
	N_("rpm verify mode (legacy)"), NULL },
 { "whatrequires", '\0', 0, 0, POPT_WHATREQUIRES, 
	N_("query/verify the package(s) which require a dependency"), "CAPABILITY" },
 { "whatprovides", '\0', 0, 0, POPT_WHATPROVIDES, 
	N_("query/verify the package(s) which provide a dependency"), "CAPABILITY" },
   POPT_TABLEEND
};

/* ========== Query specific popt args */

static void queryArgCallback(/*@unused@*/poptContext con,
		/*@unused@*/enum poptCallbackReason reason,
		const struct poptOption * opt, const char * arg, 
		/*@unused@*/ const void * data)
	/*@globals rpmQVArgs @*/
	/*@modifies rpmQVArgs @*/
{
    QVA_t qva = &rpmQVArgs;

    switch (opt->val) {
    case 'c': qva->qva_flags |= QUERY_FOR_CONFIG | QUERY_FOR_LIST; break;
    case 'd': qva->qva_flags |= QUERY_FOR_DOCS | QUERY_FOR_LIST; break;
    case 'l': qva->qva_flags |= QUERY_FOR_LIST; break;
    case 's': qva->qva_flags |= QUERY_FOR_STATE | QUERY_FOR_LIST;
	break;
    case POPT_DUMP: qva->qva_flags |= QUERY_FOR_DUMPFILES | QUERY_FOR_LIST;
	break;
    case 'v':
	/*@-internalglobs@*/ /* FIX: shrug */
	rpmIncreaseVerbosity();
	/*@=internalglobs@*/
	break;

    case POPT_QUERYFORMAT:
	if (arg) {
	    char * qf = (char *)qva->qva_queryFormat;
	    /*@-branchstate@*/
	    if (qf) {
		int len = strlen(qf) + strlen(arg) + 1;
		qf = xrealloc(qf, len);
		strcat(qf, arg);
	    } else {
		qf = xmalloc(strlen(arg) + 1);
		strcpy(qf, arg);
	    }
	    /*@=branchstate@*/
	    qva->qva_queryFormat = qf;
	}
	break;
    }
}

/**
 * Query mode options.
 */
/*@unchecked@*/
struct poptOption rpmQueryPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
	queryArgCallback, 0, NULL, NULL },
/*@=type@*/
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQVSourcePoptTable, 0,
	NULL, NULL },
 { "configfiles", 'c', 0, 0, 'c',
	N_("list all configuration files"), NULL },
 { "docfiles", 'd', 0, 0, 'd',
	N_("list all documentation files"), NULL },
 { "dump", '\0', 0, 0, POPT_DUMP,
	N_("dump basic file information"), NULL },
 { "list", 'l', 0, 0, 'l',
	N_("list files in package"), NULL },

 /* Duplicate file attr flags from packages into command line options. */
 { "noghost", '\0', POPT_BIT_CLR|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_fflags, RPMFILE_GHOST,
        N_("skip %%ghost files"), NULL },
#ifdef	NOTEVER		/* XXX there's hardly a need for these */
 { "nolicense", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_fflags, RPMFILE_LICENSE,
        N_("skip %%license files"), NULL },
 { "noreadme", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_fflags, RPMFILE_README,
        N_("skip %%readme files"), NULL },
#endif

 { "qf", '\0', POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN, 0, 
	POPT_QUERYFORMAT, NULL, NULL },
 { "queryformat", '\0', POPT_ARG_STRING, 0, POPT_QUERYFORMAT,
	N_("use the following query format"), "QUERYFORMAT" },
 { "specedit", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &specedit, -1,
	N_("substitute i18n sections into spec file"), NULL },
 { "state", 's', 0, 0, 's',
	N_("display the states of the listed files"), NULL },
 { "verbose", 'v', 0, 0, 'v',
	N_("display a verbose file listing"), NULL },
   POPT_TABLEEND
};

/**
 * Verify mode options.
 */
struct poptOption rpmVerifyPoptTable[] = {
#ifdef	DYING
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
	verifyArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* DYING */
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQVSourcePoptTable, 0,
	NULL, NULL },

 /* Duplicate file verify flags from packages into command line options. */
 { "nomd5", '\0', POPT_BIT_SET, &rpmQVArgs.qva_flags, VERIFY_MD5,
	N_("don't verify MD5 digest of files"), NULL },
 { "nosize", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_flags, VERIFY_SIZE,
        N_("don't verify size of files"), NULL },
 { "nolinkto", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_flags, VERIFY_LINKTO,
        N_("don't verify symlink path of files"), NULL },
 { "nouser", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_flags, VERIFY_USER,
        N_("don't verify owner of files"), NULL },
 { "nogroup", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_flags, VERIFY_GROUP,
        N_("don't verify group of files"), NULL },
 { "nomtime", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_flags, VERIFY_MTIME,
        N_("don't verify modification time of files"), NULL },
 { "nomode", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_flags, VERIFY_MODE,
        N_("don't verify mode of files"), NULL },
 { "nordev", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_flags, VERIFY_RDEV,
        N_("don't verify mode of files"), NULL },

 { "nofiles", '\0', POPT_BIT_SET, &rpmQVArgs.qva_flags, VERIFY_FILES,
	N_("don't verify files in package"), NULL},
 { "nodeps", '\0', POPT_BIT_SET, &rpmQVArgs.qva_flags, VERIFY_DEPS,
	N_("don't verify package dependencies"), NULL },
 { "noscript", '\0', POPT_BIT_SET,&rpmQVArgs.qva_flags, VERIFY_SCRIPT,
        N_("don't execute %verifyscript (if any)"), NULL },
 /* XXX legacy had a trailing s on --noscript */
 { "noscripts", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_flags, VERIFY_SCRIPT,
        N_("don't execute %verifyscript (if any)"), NULL },
 { "nodigest", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVArgs.qva_flags, VERIFY_DIGEST,
        N_("don't verify header SHA1 digest"), NULL },

    POPT_TABLEEND
};
