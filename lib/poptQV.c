#include "system.h"

#include "rpmbuild.h"
#include <rpmurl.h>

struct rpmQVArguments rpmQVArgs;
int specedit = 0;

/* ======================================================================== */
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
	const void * data)
{
    QVA_t *qva = (QVA_t *) data;

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

struct poptOption rpmQVSourcePoptTable[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
		rpmQVSourceArgCallback, 0, NULL, NULL },
	{ "file", 'f', 0, 0, 'f',
		N_("query package owning file"), "FILE" },
	{ "group", 'g', 0, 0, 'g',
		N_("query packages in group"), "GROUP" },
	{ "package", 'p', 0, 0, 'p',
		N_("query a package file"), NULL },
	{ "query", 'q', 0, NULL, 'q',
		N_("rpm query mode"), NULL },
	{ "querybynumber", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, 
		POPT_QUERYBYNUMBER, NULL, NULL },
	{ "querytags", '\0', 0, 0, 'Q',
		N_("display known query tags"), NULL },
	{ "specfile", '\0', 0, 0, POPT_SPECFILE,
		N_("query a spec file"), NULL },
	{ "triggeredby", '\0', 0, 0, POPT_TRIGGEREDBY, 
		N_("query the pacakges triggered by the package"), "PACKAGE" },
	{ "verify", 'V', 0, NULL, 'V',
		N_("rpm verify mode"), NULL },
	{ NULL, 'y',  POPT_ARGFLAG_DOC_HIDDEN, NULL, 'V',
		N_("rpm verify mode (legacy)"), NULL },
	{ "whatrequires", '\0', 0, 0, POPT_WHATREQUIRES, 
		N_("query the packages which require a capability"), "CAPABILITY" },
	{ "whatprovides", '\0', 0, 0, POPT_WHATPROVIDES, 
		N_("query the packages which provide a capability"), "CAPABILITY" },
	{ 0, 0, 0, 0, 0,	NULL, NULL }
};

/* ========== Query specific popt args */

static void queryArgCallback(/*@unused@*/poptContext con, /*@unused@*/enum poptCallbackReason reason,
			     const struct poptOption * opt, const char * arg, 
			     const void * data)
{
    QVA_t *qva = (QVA_t *) data;

    switch (opt->val) {
    case 'c': qva->qva_flags |= QUERY_FOR_CONFIG | QUERY_FOR_LIST; break;
    case 'd': qva->qva_flags |= QUERY_FOR_DOCS | QUERY_FOR_LIST; break;
    case 'l': qva->qva_flags |= QUERY_FOR_LIST; break;
    case 's': qva->qva_flags |= QUERY_FOR_STATE | QUERY_FOR_LIST;
	break;
    case POPT_DUMP: qva->qva_flags |= QUERY_FOR_DUMPFILES | QUERY_FOR_LIST; break;
    case 'v': rpmIncreaseVerbosity();	 break;

    case POPT_QUERYFORMAT:
      {	char *qf = (char *)qva->qva_queryFormat;
	if (qf) {
	    int len = strlen(qf) + strlen(arg) + 1;
	    qf = xrealloc(qf, len);
	    strcat(qf, arg);
	} else {
	    qf = xmalloc(strlen(arg) + 1);
	    strcpy(qf, arg);
	}
	qva->qva_queryFormat = qf;
      }	break;
    }
}

struct poptOption rpmQueryPoptTable[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
		queryArgCallback, 0, NULL, NULL },
	{ "configfiles", 'c', 0, 0, 'c',
		N_("list all configuration files"), NULL },
	{ "docfiles", 'd', 0, 0, 'd',
		N_("list all documentation files"), NULL },
	{ "dump", '\0', 0, 0, POPT_DUMP,
		N_("dump basic file information"), NULL },
	{ "list", 'l', 0, 0, 'l',
		N_("list files in package"), NULL },
	{ "qf", '\0', POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN, 0, 
		POPT_QUERYFORMAT, NULL, NULL },
	{ "queryformat", '\0', POPT_ARG_STRING, 0, POPT_QUERYFORMAT,
		N_("use the following query format"), "QUERYFORMAT" },
	{ "specedit", '\0', POPT_ARG_VAL, &specedit, -1,
		N_("substitute i18n sections into spec file"), NULL },
	{ "state", 's', 0, 0, 's',
		N_("display the states of the listed files"), NULL },
	{ "verbose", 'v', 0, 0, 'v',
		N_("display a verbose file listing"), NULL },
	{ 0, 0, 0, 0, 0,	NULL, NULL }
};

/* ======================================================================== */
