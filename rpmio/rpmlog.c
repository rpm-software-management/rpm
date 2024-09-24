/** \ingroup rpmio
 * \file rpmio/rpmlog.c
 */

#include "system.h"

#include <mutex>
#include <shared_mutex>
#include <vector>
#include <string>

#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmstring.h>
#include "debug.h"

typedef struct rpmlogCtx_s * rpmlogCtx;
struct rpmlogCtx_s {
    unsigned mask;
    int nrecsPri[RPMLOG_NPRIS];
    std::vector<rpmlogRec_s> recs;
    rpmlogCallback cbfunc;
    rpmlogCallbackData cbdata;
    FILE *stdlog;
    std::shared_mutex mutex;
};

struct rpmlogRec_s {
    int		code;		/* unused */
    rpmlogLvl	pri;		/* priority */ 
    std::string message;	/* log message string */
};

using wrlock = std::unique_lock<std::shared_mutex>;
using rdlock = std::shared_lock<std::shared_mutex>;

/* Force log context acquisition through a function */
static rpmlogCtx rpmlogCtxAcquire()
{
    static struct rpmlogCtx_s _globalCtx = { RPMLOG_UPTO(RPMLOG_NOTICE),
					     {0}, {}, NULL, NULL, NULL };
    return &_globalCtx;
}

int rpmlogGetNrecsByMask(unsigned mask)
{
    rpmlogCtx ctx = rpmlogCtxAcquire();
    int nrecs = 0;

    rdlock lock(ctx->mutex);
    if (mask) {
	for (int i = 0; i < RPMLOG_NPRIS; i++, mask >>= 1)
	    if (mask & 1)
	        nrecs += ctx->nrecsPri[i];
    } else
	nrecs = ctx->recs.size();

    return nrecs;
}

int rpmlogGetNrecs(void)
{
    return rpmlogGetNrecsByMask(0);
}

int rpmlogCode(void)
{
    int code = -1;
    rpmlogCtx ctx = rpmlogCtxAcquire();
    rdlock lock(ctx->mutex);
    
    if (ctx->recs.empty() == false)
	code = ctx->recs.back().code;

    return code;
}

const char * rpmlogMessage(void)
{
    const char *msg = _("(no error)");
    rpmlogCtx ctx = rpmlogCtxAcquire();
    rdlock lock(ctx->mutex);

    if (ctx->recs.empty() == false)
	msg = ctx->recs.back().message.c_str();

    return msg;
}

const char * rpmlogRecMessage(rpmlogRec rec)
{
    return (rec != NULL) ? rec->message.c_str() : NULL;
}

rpmlogLvl rpmlogRecPriority(rpmlogRec rec)
{
    return (rec != NULL) ? rec->pri : (rpmlogLvl)-1;
}

void rpmlogPrintByMask(FILE *f, unsigned mask)
{
    rpmlogCtx ctx = rpmlogCtxAcquire();
    rdlock lock(ctx->mutex);

    if (f == NULL)
	f = stderr;

    for (auto & rec : ctx->recs) {
	if (mask && ((RPMLOG_MASK(rec.pri) & mask) == 0))
	    continue;
	if (rec.message.empty() == false)
	    fprintf(f, "    %s", rec.message.c_str());
    }
}

void rpmlogPrint(FILE *f)
{
    rpmlogPrintByMask(f, 0);
}

void rpmlogClose (void)
{
    rpmlogCtx ctx = rpmlogCtxAcquire();
    wrlock lock(ctx->mutex);

    ctx->recs.clear();
    memset(ctx->nrecsPri, 0, sizeof(ctx->nrecsPri));
}

void rpmlogOpen (const char *ident, int option,
		int facility)
{
}

int rpmlogSetMask (int mask)
{
    rpmlogCtx ctx = rpmlogCtxAcquire();

    wrlock lock(ctx->mutex);
    int omask = ctx->mask;
    if (mask)
	ctx->mask = mask;

    return omask;
}

rpmlogCallback rpmlogSetCallback(rpmlogCallback cb, rpmlogCallbackData data)
{
    rpmlogCtx ctx = rpmlogCtxAcquire();
    wrlock lock(ctx->mutex);

    rpmlogCallback ocb = ctx->cbfunc;
    ctx->cbfunc = cb;
    ctx->cbdata = data;

    return ocb;
}

FILE * rpmlogSetFile(FILE * fp)
{
    rpmlogCtx ctx = rpmlogCtxAcquire();
    wrlock lock(ctx->mutex);

    FILE * ofp = ctx->stdlog;
    ctx->stdlog = fp;

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

#define ANSI_COLOR_BLACK	"\x1b[30m"
#define ANSI_COLOR_RED		"\x1b[31m"
#define ANSI_COLOR_GREEN	"\x1b[32m"
#define ANSI_COLOR_YELLOW	"\x1b[33m"
#define ANSI_COLOR_BLUE		"\x1b[34m"
#define ANSI_COLOR_MAGENTA	"\x1b[35m"
#define ANSI_COLOR_CYAN		"\x1b[36m"
#define ANSI_COLOR_WHITE	"\x1b[37m"

#define ANSI_BRIGHT_BLACK	"\x1b[30;1m"
#define ANSI_BRIGHT_RED		"\x1b[31;1m"
#define ANSI_BRIGHT_GREEN	"\x1b[32;1m"
#define ANSI_BRIGHT_YELLOW	"\x1b[33;1m"
#define ANSI_BRIGHT_BLUE	"\x1b[34;1m"
#define ANSI_BRIGHT_MAGENTA	"\x1b[35;1m"
#define ANSI_BRIGHT_CYAN	"\x1b[36;1m"
#define ANSI_BRIGHT_WHITE	"\x1b[37;1m"

#define ANSI_COLOR_BOLD		"\x1b[1m"
#define ANSI_COLOR_RESET	"\x1b[0m"

static const char *rpmlogMsgPrefixColor[] = {
    ANSI_BRIGHT_RED,	/*!< RPMLOG_EMERG */
    ANSI_BRIGHT_RED,	/*!< RPMLOG_ALERT */
    ANSI_BRIGHT_RED,	/*!< RPMLOG_CRIT */
    ANSI_BRIGHT_RED,	/*!< RPMLOG_ERR */
    ANSI_BRIGHT_MAGENTA,/*!< RPMLOG_WARNING */
    "",			/*!< RPMLOG_NOTICE */
    "",			/*!< RPMLOG_INFO */
    ANSI_BRIGHT_BLUE,	/*!< RPMLOG_DEBUG */
};

const char * rpmlogLevelPrefix(rpmlogLvl pri)
{
    const char * prefix = "";
    if (rpmlogMsgPrefix[pri] && *rpmlogMsgPrefix[pri]) 
	prefix = _(rpmlogMsgPrefix[pri]);
    return prefix;
}

static const char * rpmlogLevelColor(rpmlogLvl pri)
{
    return rpmlogMsgPrefixColor[pri&0x7];
}

enum {
    COLOR_NO = 0,
    COLOR_AUTO = 1,
    COLOR_ALWAYS = 2,
};

static int getColorConfig(void)
{
    int rc = COLOR_NO;
    char * color = rpmExpand("%{?_color_output}%{!?_color_output:auto}", NULL);
    if (rstreq(color, "auto"))
	rc = COLOR_AUTO;
    else if (rstreq(color, "always"))
	rc = COLOR_ALWAYS;
    free(color);
    return rc;
}

static void logerror(void)
{
    static __thread int lasterr = 0;
    if (errno != EPIPE && errno != lasterr) {
	lasterr = errno;
	perror(_("Error writing to log"));
    }
}

static int rpmlogDefault(FILE *stdlog, rpmlogRec rec)
{
    FILE *msgout = (stdlog ? stdlog : stderr);
    static __thread int color = -1;
    const char * colorOn = NULL;

    if (color < 0)
	color = getColorConfig();

    if (color == COLOR_ALWAYS ||
	    (color == COLOR_AUTO && isatty(fileno(msgout))))
	colorOn = rpmlogLevelColor(rec->pri);

    switch (rec->pri) {
    case RPMLOG_INFO:
    case RPMLOG_NOTICE:
	msgout = (stdlog ? stdlog : stdout);
	break;
    case RPMLOG_EMERG:
    case RPMLOG_ALERT:
    case RPMLOG_CRIT:
    case RPMLOG_ERR:
    case RPMLOG_WARNING:
    case RPMLOG_DEBUG:
	if (colorOn && *colorOn)
	    if (fputs(rpmlogLevelColor(rec->pri), msgout) == EOF)
		logerror();
	break;
    default:
	break;
    }

    if (fputs(rpmlogLevelPrefix(rec->pri), msgout) == EOF)
	logerror();

    switch (rec->pri) {
    case RPMLOG_INFO:
    case RPMLOG_NOTICE:
	break;
    case RPMLOG_EMERG:
    case RPMLOG_ALERT:
    case RPMLOG_CRIT:
    case RPMLOG_ERR:
    case RPMLOG_WARNING:
	if (colorOn && *colorOn) {
	    if (fputs(ANSI_COLOR_RESET, msgout) == EOF)
		logerror();
	    if (fputs(ANSI_COLOR_BOLD, msgout) == EOF)
		logerror();
	}
    case RPMLOG_DEBUG:
    default:
	break;
    }

    if (rec->message.empty() == false)
	if (fputs(rec->message.c_str(), msgout) == EOF)
	    logerror();

    switch (rec->pri) {
    case RPMLOG_INFO:
    case RPMLOG_NOTICE:
	break;
    case RPMLOG_EMERG:
    case RPMLOG_ALERT:
    case RPMLOG_CRIT:
    case RPMLOG_ERR:
    case RPMLOG_WARNING:
    case RPMLOG_DEBUG:
	if (colorOn && *colorOn)
	    if (fputs(ANSI_COLOR_RESET, msgout) == EOF)
		logerror();
	break;
    default:
	break;
    }

    if (fflush(msgout) == EOF)
	logerror();

    return (rec->pri <= RPMLOG_CRIT ? RPMLOG_EXIT : 0);
}

/* FIX: rpmlogMsgPrefix[] dependent, not unqualified */
/* FIX: rpmlogMsgPrefix[] may be NULL */
static void dolog(struct rpmlogRec_s *rec, int saverec)
{
    static std::mutex serial_mutex;
    int cbrc = RPMLOG_DEFAULT;
    int needexit = 0;
    FILE *clog = NULL;
    rpmlogCallbackData cbdata = NULL;
    rpmlogCallback cbfunc = NULL;
    rpmlogCtx ctx = rpmlogCtxAcquire();
    wrlock lock(ctx->mutex);

    /* Save copy of all messages at warning (or below == "more important"). */
    if (saverec) {
	ctx->recs.push_back(*rec);
	ctx->nrecsPri[rec->pri]++;
    }
    cbfunc = ctx->cbfunc;
    cbdata = ctx->cbdata;
    clog = ctx->stdlog;

    /* Free the context for callback and actual log output */
    lock.unlock();

    /* Always serialize callback and output to avoid interleaved messages. */
    std::lock_guard<std::mutex> serialize(serial_mutex);
    if (cbfunc) {
	cbrc = cbfunc(rec, cbdata);
	needexit += cbrc & RPMLOG_EXIT;
    }

    if (cbrc & RPMLOG_DEFAULT) {
	cbrc = rpmlogDefault(clog, rec);
	needexit += cbrc & RPMLOG_EXIT;
    }

    if (needexit)
	exit(EXIT_FAILURE);
}

static int rpmlogGetMask(void)
{
    /* No locking needed, reading an int is atomic */
    rpmlogCtx ctx = rpmlogCtxAcquire();
    return ctx->mask;
}

void rpmlog (int code, const char *fmt, ...)
{
    int saved_errno = errno;
    unsigned pri = RPMLOG_PRI(code);
    unsigned mask = RPMLOG_MASK(pri);
    int saverec = (pri <= RPMLOG_WARNING);
    va_list ap;
    char *msg = NULL;

    if ((mask & rpmlogGetMask()) == 0)
	goto exit;

    va_start(ap, fmt);
    if (rvasprintf(&msg, fmt, ap) >= 0) {
	struct rpmlogRec_s rec;

	rec.code = code;
	rec.pri = (rpmlogLvl)pri;
	rec.message = msg;

	dolog(&rec, saverec);

	free(msg);
    }
    va_end(ap);
exit:
    errno = saved_errno;
}
