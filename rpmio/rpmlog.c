/** \ingroup rpmio
 * \file rpmio/rpmlog.c
 */

#include "system.h"
#include <stdarg.h>
#include "rpmlog.h"
#include "debug.h"

/*@access rpmlogRec @*/

static int nrecs = 0;
static /*@only@*/ /*@null@*/ rpmlogRec recs = NULL;

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @retval		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ const void * p) /*@modifies p@*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

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

void rpmlogPrint(FILE *f)
{
    int i;

    if (f == NULL)
	f = stderr;

    for (i = 0; i < nrecs; i++) {
	rpmlogRec rec = recs + i;
	if (rec->message && *rec->message)
	    fprintf(f, "    %s", rec->message);
    }
}

void rpmlogClose (void)
{
    int i;

    for (i = 0; i < nrecs; i++) {
	rpmlogRec rec = recs + i;
	rec->message = _free(rec->message);
    }
    recs = _free(recs);
    nrecs = 0;
}

void rpmlogOpen (/*@unused@*/ const char *ident, /*@unused@*/ int option,
		/*@unused@*/ int facility)
{
}

static int rpmlogMask = RPMLOG_UPTO( RPMLOG_NOTICE );
static /*@unused@*/ int rpmlogFacility = RPMLOG_USER;

int rpmlogSetMask (int mask)
{
    int omask = rpmlogMask;
    if (mask)
        rpmlogMask = mask;
    return omask;
}

static /*@null@*/ rpmlogCallback _rpmlogCallback = NULL;

rpmlogCallback rpmlogSetCallback(rpmlogCallback cb)
{
    rpmlogCallback ocb = _rpmlogCallback;
    _rpmlogCallback = cb;
    return ocb;
}

static char *rpmlogMsgPrefix[] = {
    N_("fatal error: "),/*!< RPMLOG_EMERG */
    N_("fatal error: "),/*!< RPMLOG_ALERT */
    N_("fatal error: "),/*!< RPMLOG_CRIT */
    N_("error: "),	/*!< RPMLOG_ERR */
    N_("warning: "),	/*!< RPMLOG_WARNING */
    "",			/*!< RPMLOG_NOTICE */
    "",			/*!< RPMLOG_INFO */
    "D: ",		/*!< RPMLOG_DEBUG */
};

#if !defined(HAVE_VSNPRINTF)
static inline int vsnprintf(char * buf, /*@unused@*/ int nb,
	const char * fmt, va_list ap)
{
    return vsprintf(buf, fmt, ap);
}
#endif

static void vrpmlog (unsigned code, const char *fmt, va_list ap)
{
    int pri = RPMLOG_PRI(code);
    int mask = RPMLOG_MASK(pri);
    /*@unused@*/ int fac = RPMLOG_FAC(code);
    char *msgbuf, *msg;
    int msgnb = BUFSIZ, nb;
    FILE * msgout = stderr;

    if ((mask & rpmlogMask) == 0)
	return;

    msgbuf = xmalloc(msgnb);
    *msgbuf = '\0';

    /* Allocate a sufficently large buffer for output. */
    while (1) {
	va_list apc;
	/*@-sysunrecog -usedef@*/ __va_copy(apc, ap); /*@=sysunrecog =usedef@*/
	/*@-unrecog@*/ nb = vsnprintf(msgbuf, msgnb, fmt, apc); /*@=unrecog@*/
	if (nb > -1 && nb < msgnb)
	    break;
	if (nb > -1)		/* glibc 2.1 */
	    msgnb = nb+1;
	else			/* glibc 2.0 */
	    msgnb *= 2;
	msgbuf = xrealloc(msgbuf, msgnb);
    }
    msgbuf[msgnb - 1] = '\0';
    msg = msgbuf;

    /* Save copy of all messages at warning (or below == "more important"). */
    if (pri <= RPMLOG_WARNING) {

	if (recs == NULL)
	    recs = xmalloc((nrecs+2) * sizeof(*recs));
	else
	    recs = xrealloc(recs, (nrecs+2) * sizeof(*recs));
	recs[nrecs].code = code;
	recs[nrecs].message = msg = msgbuf;
	msgbuf = NULL;
	recs[nrecs+1].code = 0;
	recs[nrecs+1].message = NULL;
	++nrecs;


	if (_rpmlogCallback) {
	    _rpmlogCallback();
	    return;	/* XXX Preserve legacy rpmError behavior. */
	}
    }

    /* rpmMessage behavior */

    switch (pri) {
    case RPMLOG_INFO:
    case RPMLOG_NOTICE:
	msgout = stdout;
	break;

    case RPMLOG_EMERG:
    case RPMLOG_ALERT:
    case RPMLOG_CRIT:
    case RPMLOG_ERR: /* XXX Legacy rpmError behavior used stdout w/o prefix. */
    case RPMLOG_WARNING:
    case RPMLOG_DEBUG:
	break;
    }

    if (rpmlogMsgPrefix[pri] && *rpmlogMsgPrefix[pri])
	(void) fputs(_(rpmlogMsgPrefix[pri]), msgout);

    (void) fputs(msg, msgout);
    (void) fflush(msgout);
    msgbuf = _free(msgbuf);
    if (pri <= RPMLOG_CRIT)
	exit(EXIT_FAILURE);
}

void rpmlog (int code, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vrpmlog(code, fmt, ap);
    va_end(ap);
}

int rpmErrorCode(void)
{
    return rpmlogCode();
}

const char * rpmErrorString(void)
{
    return rpmlogMessage();
}

rpmlogCallback rpmErrorSetCallback(rpmlogCallback cb)
{
    return rpmlogSetCallback(cb);
}
