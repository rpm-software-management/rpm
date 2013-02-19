/** \ingroup rpmio
 * \file rpmio/rpmlog.c
 */

#include "system.h"
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <rpm/rpmlog.h>
#include "debug.h"

typedef struct rpmlogCtx_s * rpmlogCtx;
struct rpmlogCtx_s {
    pthread_rwlock_t lock;
    unsigned mask;
    int nrecs;
    rpmlogRec recs;
    rpmlogCallback cbfunc;
    rpmlogCallbackData cbdata;
    FILE *stdlog;
};

struct rpmlogRec_s {
    int		code;		/* unused */
    rpmlogLvl	pri;		/* priority */ 
    char * message;		/* log message string */
};

/* Force log context acquisition through a function */
static rpmlogCtx rpmlogCtxAcquire(int write)
{
    static struct rpmlogCtx_s _globalCtx = { PTHREAD_RWLOCK_INITIALIZER,
					     RPMLOG_UPTO(RPMLOG_NOTICE),
					     0, NULL, NULL, NULL, NULL };
    rpmlogCtx ctx = &_globalCtx;

    /* XXX: errors should be handled */
    if (write)
	pthread_rwlock_wrlock(&ctx->lock);
    else
	pthread_rwlock_rdlock(&ctx->lock);

    return ctx;
}

/* Release log context */
static rpmlogCtx rpmlogCtxRelease(rpmlogCtx ctx)
{
    pthread_rwlock_unlock(&ctx->lock);
    return NULL;
}

int rpmlogGetNrecs(void)
{
    rpmlogCtx ctx = rpmlogCtxAcquire(0);
    int nrecs = ctx->nrecs;
    rpmlogCtxRelease(ctx);
    return nrecs;
}

int rpmlogCode(void)
{
    int code = -1;
    rpmlogCtx ctx = rpmlogCtxAcquire(0);
    
    if (ctx->recs != NULL && ctx->nrecs > 0)
	code = ctx->recs[ctx->nrecs-1].code;

    rpmlogCtxRelease(ctx);
    return code;
}

/* XXX: This is not thread-safe, we should return a malloced copy */
const char * rpmlogMessage(void)
{
    const char *msg = _("(no error)");
    rpmlogCtx ctx = rpmlogCtxAcquire(0);

    if (ctx->recs != NULL && ctx->nrecs > 0)
	msg = ctx->recs[ctx->nrecs-1].message;

    rpmlogCtxRelease(ctx);
    return msg;
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
    rpmlogCtx ctx = rpmlogCtxAcquire(0);

    if (f == NULL)
	f = stderr;

    for (int i = 0; i < ctx->nrecs; i++) {
	rpmlogRec rec = ctx->recs + i;
	if (rec->message && *rec->message)
	    fprintf(f, "    %s", rec->message);
    }

    rpmlogCtxRelease(ctx);
}

void rpmlogClose (void)
{
    rpmlogCtx ctx = rpmlogCtxAcquire(1);

    for (int i = 0; i < ctx->nrecs; i++) {
	rpmlogRec rec = ctx->recs + i;
	rec->message = _free(rec->message);
    }
    ctx->recs = _free(ctx->recs);
    ctx->nrecs = 0;

    rpmlogCtxRelease(ctx);
}

void rpmlogOpen (const char *ident, int option,
		int facility)
{
}

#ifdef NOTYET
static unsigned rpmlogFacility = RPMLOG_USER;
#endif

int rpmlogSetMask (int mask)
{
    rpmlogCtx ctx = rpmlogCtxAcquire(1);

    int omask = ctx->mask;
    if (mask)
        ctx->mask = mask;

    rpmlogCtxRelease(ctx);
    return omask;
}

rpmlogCallback rpmlogSetCallback(rpmlogCallback cb, rpmlogCallbackData data)
{
    rpmlogCtx ctx = rpmlogCtxAcquire(1);

    rpmlogCallback ocb = ctx->cbfunc;
    ctx->cbfunc = cb;
    ctx->cbdata = data;

    rpmlogCtxRelease(ctx);
    return ocb;
}

static int rpmlogDefault(rpmlogCtx ctx, rpmlogRec rec)
{
    FILE *msgout = (ctx->stdlog ? ctx->stdlog : stderr);

    switch (rec->pri) {
    case RPMLOG_INFO:
    case RPMLOG_NOTICE:
        msgout = (ctx->stdlog ? ctx->stdlog : stdout);
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
    rpmlogCtx ctx = rpmlogCtxAcquire(1);

    FILE * ofp = ctx->stdlog;
    ctx->stdlog = fp;

    rpmlogCtxRelease(ctx);
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
static void dolog(rpmlogCtx ctx, struct rpmlogRec_s *rec, int saverec)
{
    int cbrc = RPMLOG_DEFAULT;
    int needexit = 0;

    /* Save copy of all messages at warning (or below == "more important"). */
    if (saverec) {
	ctx->recs = xrealloc(ctx->recs, (ctx->nrecs+2) * sizeof(*ctx->recs));
	ctx->recs[ctx->nrecs].code = rec->code;
	ctx->recs[ctx->nrecs].pri = rec->pri;
	ctx->recs[ctx->nrecs].message = xstrdup(rec->message);
	ctx->recs[ctx->nrecs+1].code = 0;
	ctx->recs[ctx->nrecs+1].message = NULL;
	ctx->nrecs++;
    }

    if (ctx->cbfunc) {
	cbrc = ctx->cbfunc(rec, ctx->cbdata);
	needexit += cbrc & RPMLOG_EXIT;
    }

    if (cbrc & RPMLOG_DEFAULT) {
	cbrc = rpmlogDefault(ctx, rec);
	needexit += cbrc & RPMLOG_EXIT;
    }
    
    if (needexit)
	exit(EXIT_FAILURE);
}

void rpmlog (int code, const char *fmt, ...)
{
    unsigned pri = RPMLOG_PRI(code);
    unsigned mask = RPMLOG_MASK(pri);
    int saverec = (pri <= RPMLOG_WARNING);
    va_list ap;
    int n;

    rpmlogCtx ctx = rpmlogCtxAcquire(saverec);

    if ((mask & ctx->mask) == 0)
	goto exit;

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

	dolog(ctx, &rec, saverec);

	free(msg);
    }

exit:
    rpmlogCtxRelease(ctx);
}

