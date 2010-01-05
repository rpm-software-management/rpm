/** \ingroup rpmio
 * \file rpmio/rpmlog.c
 */

#include "system.h"
#include <stdarg.h>
#include <stdlib.h>
#include <rpm/rpmlog.h>
#include "debug.h"

static int nrecs = 0;
static rpmlogRec recs = NULL;

struct rpmlogRec_s {
    int		code;		/* unused */
    rpmlogLvl	pri;		/* priority */ 
    char * message;		/* log message string */
};

int rpmlogGetNrecs(void)
{
    return nrecs;
}

int rpmlogCode(void)
{
    if (recs != NULL && nrecs > 0)
	return recs[nrecs-1].code;
    return -1;
}


const char * rpmlogMessage(void)
{
    if (recs != NULL && nrecs > 0)
	return recs[nrecs-1].message;
    return _("(no error)");
}

const char * rpmlogRecMessage(rpmlogRec rec)
{
    assert(rec != NULL);
    return (rec->message);
}

rpmlogLvl rpmlogRecPriority(rpmlogRec rec)
{
    assert(rec != NULL);
    return (rec->pri);
}

void rpmlogPrint(FILE *f)
{
    int i;

    if (f == NULL)
	f = stderr;

    if (recs)
    for (i = 0; i < nrecs; i++) {
	rpmlogRec rec = recs + i;
	if (rec->message && *rec->message)
	    fprintf(f, "    %s", rec->message);
    }
}

void rpmlogClose (void)
{
    int i;

    if (recs)
    for (i = 0; i < nrecs; i++) {
	rpmlogRec rec = recs + i;
	rec->message = _free(rec->message);
    }
    recs = _free(recs);
    nrecs = 0;
}

void rpmlogOpen (const char *ident, int option,
		int facility)
{
}

static unsigned rpmlogMask = RPMLOG_UPTO( RPMLOG_NOTICE );

#ifdef NOTYET
static unsigned rpmlogFacility = RPMLOG_USER;
#endif

int rpmlogSetMask (int mask)
{
    int omask = rpmlogMask;
    if (mask)
        rpmlogMask = mask;
    return omask;
}

static rpmlogCallback _rpmlogCallback = NULL;
static rpmlogCallbackData _rpmlogCallbackData = NULL;

rpmlogCallback rpmlogSetCallback(rpmlogCallback cb, rpmlogCallbackData data)
{
    rpmlogCallback ocb = _rpmlogCallback;
    _rpmlogCallback = cb;
    _rpmlogCallbackData = data;
    return ocb;
}

static FILE * _stdlog = NULL;

static int rpmlogDefault(rpmlogRec rec)
{
    FILE *msgout = (_stdlog ? _stdlog : stderr);

    switch (rec->pri) {
    case RPMLOG_INFO:
    case RPMLOG_NOTICE:
        msgout = (_stdlog ? _stdlog : stdout);
        break;
    case RPMLOG_EMERG:
    case RPMLOG_ALERT:
    case RPMLOG_CRIT:
    case RPMLOG_ERR:
    case RPMLOG_WARNING:
    case RPMLOG_DEBUG:
    default:
        break;
    }

    (void) fputs(rpmlogLevelPrefix(rec->pri), msgout);

    (void) fputs(rec->message, msgout);
    (void) fflush(msgout);

    return (rec->pri <= RPMLOG_CRIT ? RPMLOG_EXIT : 0);
}


FILE * rpmlogSetFile(FILE * fp)
{
    FILE * ofp = _stdlog;
    _stdlog = fp;
    return ofp;
}

static const char * const rpmlogMsgPrefix[] = {
    N_("fatal error: "),/*!< RPMLOG_EMERG */
    N_("fatal error: "),/*!< RPMLOG_ALERT */
    N_("fatal error: "),/*!< RPMLOG_CRIT */
    N_("error: "),	/*!< RPMLOG_ERR */
    N_("warning: "),	/*!< RPMLOG_WARNING */
    "",			/*!< RPMLOG_NOTICE */
    "",			/*!< RPMLOG_INFO */
    "D: ",		/*!< RPMLOG_DEBUG */
};

const char * rpmlogLevelPrefix(rpmlogLvl pri)
{
    const char * prefix = "";
    if (rpmlogMsgPrefix[pri] && *rpmlogMsgPrefix[pri]) 
	prefix = _(rpmlogMsgPrefix[pri]);
    return prefix;
}

/* FIX: rpmlogMsgPrefix[] dependent, not unqualified */
/* FIX: rpmlogMsgPrefix[] may be NULL */
static void dolog (struct rpmlogRec_s *rec)
{
    int cbrc = RPMLOG_DEFAULT;
    int needexit = 0;

    /* Save copy of all messages at warning (or below == "more important"). */
    if (rec->pri <= RPMLOG_WARNING) {
	recs = xrealloc(recs, (nrecs+2) * sizeof(*recs));
	recs[nrecs].code = rec->code;
	recs[nrecs].pri = rec->pri;
	recs[nrecs].message = xstrdup(rec->message);
	recs[nrecs+1].code = 0;
	recs[nrecs+1].message = NULL;
	++nrecs;
    }

    if (_rpmlogCallback) {
	cbrc = _rpmlogCallback(rec, _rpmlogCallbackData);
	needexit += cbrc & RPMLOG_EXIT;
    }

    if (cbrc & RPMLOG_DEFAULT) {
	cbrc = rpmlogDefault(rec);
	needexit += cbrc & RPMLOG_EXIT;
    }
    
    if (needexit)
	exit(EXIT_FAILURE);
}

void rpmlog (int code, const char *fmt, ...)
{
    unsigned pri = RPMLOG_PRI(code);
    unsigned mask = RPMLOG_MASK(pri);
    va_list ap;
    int n;

    if ((mask & rpmlogMask) == 0)
	return;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n >= -1) {
	struct rpmlogRec_s rec;
	size_t nb = n + 1;
	char *msg = xmalloc(nb);

	va_start(ap, fmt);
	n = vsnprintf(msg, nb, fmt, ap);
	va_end(ap);

	rec.code = code;
	rec.pri = pri;
	rec.message = msg;

	dolog(&rec);

	free(msg);
    }
}

