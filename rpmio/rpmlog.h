#ifndef H_RPMLOG
#define H_RPMLOG 1

/** \ingroup rpmio
 * \file rpmio/rpmlog.h
 * Yet Another syslog(3) API clone.
 * Used by rpm to unify rpmError() and rpmMessage().
 */

#include <stdarg.h>

/**
 * RPM Log levels.
 * priorities/facilities are encoded into a single 32-bit quantity, where the
 * bottom 3 bits are the priority (0-7) and the top 28 bits are the facility
 * (0-big number).  Both the priorities and the facilities map roughly
 * one-to-one to strings in the syslogd(8) source code.  This mapping is
 * included in this file.
 *
 * priorities (these are ordered)
 */
typedef enum rpmlogLvl_e {
    RPMLOG_EMERG	= 0,	/*!< system is unusable */
    RPMLOG_ALERT	= 1,	/*!< action must be taken immediately */
    RPMLOG_CRIT		= 2,	/*!< critical conditions */
    RPMLOG_ERR		= 3,	/*!< error conditions */
    RPMLOG_WARNING	= 4,	/*!< warning conditions */
    RPMLOG_NOTICE	= 5,	/*!< normal but significant condition */
    RPMLOG_INFO		= 6,	/*!< informational */
    RPMLOG_DEBUG	= 7	/*!< debug-level messages */
} rpmlogLvl;

#define	RPMLOG_PRIMASK	0x07	/* mask to extract priority part (internal) */
				/* extract priority */
#define	RPMLOG_PRI(p)	((p) & RPMLOG_PRIMASK)
#define	RPMLOG_MAKEPRI(fac, pri)	((((unsigned)(fac)) << 3) | (pri))

#ifdef RPMLOG_NAMES
#define	_RPMLOG_NOPRI	0x10	/* the "no priority" priority */
				/* mark "facility" */
#define	_RPMLOG_MARK	RPMLOG_MAKEPRI(RPMLOG_NFACILITIES, 0)
typedef struct _rpmcode {
	const char	*c_name;
	int		c_val;
} RPMCODE;

RPMCODE rpmprioritynames[] =
  {
    { "alert",	RPMLOG_ALERT },
    { "crit",	RPMLOG_CRIT },
    { "debug",	RPMLOG_DEBUG },
    { "emerg",	RPMLOG_EMERG },
    { "err",	RPMLOG_ERR },
    { "error",	RPMLOG_ERR },		/* DEPRECATED */
    { "info",	RPMLOG_INFO },
    { "none",	_RPMLOG_NOPRI },	/* INTERNAL */
    { "notice",	RPMLOG_NOTICE },
    { "panic",	RPMLOG_EMERG },		/* DEPRECATED */
    { "warn",	RPMLOG_WARNING },	/* DEPRECATED */
    { "warning",RPMLOG_WARNING },
    { NULL, -1 }
  };
#endif

/**
 * facility codes
 */
typedef	enum rpmlogFac_e {
    RPMLOG_KERN		= (0<<3),	/*!< kernel messages */
    RPMLOG_USER		= (1<<3),	/*!< random user-level messages */
    RPMLOG_MAIL		= (2<<3),	/*!< mail system */
    RPMLOG_DAEMON	= (3<<3),	/*!< system daemons */
    RPMLOG_AUTH		= (4<<3),	/*!< security/authorization messages */
    RPMLOG_SYSLOG	= (5<<3),	/*!< messages generated internally by syslogd */
    RPMLOG_LPR		= (6<<3),	/*!< line printer subsystem */
    RPMLOG_NEWS		= (7<<3),	/*!< network news subsystem */
    RPMLOG_UUCP		= (8<<3),	/*!< UUCP subsystem */
    RPMLOG_CRON		= (9<<3),	/*!< clock daemon */
    RPMLOG_AUTHPRIV	= (10<<3),	/*!< security/authorization messages (private) */
    RPMLOG_FTP		= (11<<3),	/*!< ftp daemon */

	/* other codes through 15 reserved for system use */
    RPMLOG_LOCAL0	= (16<<3),	/*!< reserved for local use */
    RPMLOG_LOCAL1	= (17<<3),	/*!< reserved for local use */
    RPMLOG_LOCAL2	= (18<<3),	/*!< reserved for local use */
    RPMLOG_LOCAL3	= (19<<3),	/*!< reserved for local use */
    RPMLOG_LOCAL4	= (20<<3),	/*!< reserved for local use */
    RPMLOG_LOCAL5	= (21<<3),	/*!< reserved for local use */
    RPMLOG_LOCAL6	= (22<<3),	/*!< reserved for local use */
    RPMLOG_LOCAL7	= (23<<3),	/*!< reserved for local use */

#define	RPMLOG_NFACILITIES 24	/*!< current number of facilities */
    RPMLOG_ERRMSG	= (((unsigned)(RPMLOG_NFACILITIES+0))<<3)
} rpmlogFac;

#define	RPMLOG_FACMASK	0x03f8	/*!< mask to extract facility part */
#define	RPMLOG_FAC(p)	(((p) & RPMLOG_FACMASK) >> 3)


#ifdef RPMLOG_NAMES
CODE facilitynames[] =
  {
    { "auth",	RPMLOG_AUTH },
    { "authpriv",RPMLOG_AUTHPRIV },
    { "cron",	RPMLOG_CRON },
    { "daemon",	RPMLOG_DAEMON },
    { "ftp",	RPMLOG_FTP },
    { "kern",	RPMLOG_KERN },
    { "lpr",	RPMLOG_LPR },
    { "mail",	RPMLOG_MAIL },
    { "mark",	_RPMLOG_MARK },		/* INTERNAL */
    { "news",	RPMLOG_NEWS },
    { "security",RPMLOG_AUTH },		/* DEPRECATED */
    { "syslog",	RPMLOG_SYSLOG },
    { "user",	RPMLOG_USER },
    { "uucp",	RPMLOG_UUCP },
    { "local0",	RPMLOG_LOCAL0 },
    { "local1",	RPMLOG_LOCAL1 },
    { "local2",	RPMLOG_LOCAL2 },
    { "local3",	RPMLOG_LOCAL3 },
    { "local4",	RPMLOG_LOCAL4 },
    { "local5",	RPMLOG_LOCAL5 },
    { "local6",	RPMLOG_LOCAL6 },
    { "local7",	RPMLOG_LOCAL7 },
    { NULL, -1 }
  };
#endif

/*
 * arguments to setlogmask.
 */
#define	RPMLOG_MASK(pri) (1 << (pri))		/*!< mask for one priority */
#define	RPMLOG_UPTO(pri) ((1 << ((pri)+1)) - 1)	/*!< all priorities through pri */

/*
 * Option flags for openlog.
 *
 * RPMLOG_ODELAY no longer does anything.
 * RPMLOG_NDELAY is the inverse of what it used to be.
 */
#define	RPMLOG_PID	0x01	/*!< log the pid with each message */
#define	RPMLOG_CONS	0x02	/*!< log on the console if errors in sending */
#define	RPMLOG_ODELAY	0x04	/*!< delay open until first syslog() (default) */
#define	RPMLOG_NDELAY	0x08	/*!< don't delay open */
#define	RPMLOG_NOWAIT	0x10	/*!< don't wait for console forks: DEPRECATED */
#define	RPMLOG_PERROR	0x20	/*!< log to stderr as well */

/**
 * @todo Add argument(s), integrate with other types of callbacks.
 */
typedef void (*rpmlogCallback) (void);

/**
 */
typedef /*@abstract@*/ struct rpmlogRec_s {
    int		code;
    const char	* message;
} * rpmlogRec;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return number of rpmError() ressages.
 * @return		number of messages
 */
int rpmlogGetNrecs(void);

/**
 * Return text of last rpmError() message.
 * @return		text of last message
 */
/*@observer@*/ const char * rpmlogMessage(void);

/**
 * Return error code from last rpmError() message.
 * @deprecated Perl-RPM needs, what's really needed is predictable, non-i18n
 *	encumbered, error text that can be retrieved through rpmlogMessage()
 *	and parsed IMHO.
 * @return		code from last message
 */
int rpmlogCode(void);

/**
 * Print all rpmError() messages.
 * @param f		file handle (NULL uses stderr)
 */
void rpmlogPrint(FILE *f);

/**
 * Close desriptor used to write to system logger.
 * @todo Implement.
 */
void rpmlogClose (void);

/**
 * Open connection to system logger.
 * @todo Implement.
 */
void rpmlogOpen (const char *ident, int option, int facility);

/**
 * Set the log mask level.
 */
int rpmlogSetMask (int mask);

/**
 * Generate a log message using FMT string and option arguments.
 */
void rpmlog (int pri, const char *fmt, ...);

/**
 * Set rpmlog callback function.
 */
rpmlogCallback rpmlogSetCallback(rpmlogCallback cb);

/**
 * Set rpmlog callback function.
 * @deprecated gnorpm needs, use rpmlogSetCallback() instead.
 */
rpmlogCallback rpmErrorSetCallback(rpmlogCallback cb);

/**
 * Return error code from last rpmError() message.
 * @deprecated Perl-RPM needs, use rpmlogCode() instead.
 * @return		code from last message
 */
int rpmErrorCode(void);

/**
 * Return text of last rpmError() message.
 * @deprecated gnorpm needs, use rpmlogMessage() instead.
 * @return		text of last message
 */
/*@observer@*/ const char * rpmErrorString(void);

#ifdef __cplusplus
}
#endif

#endif /* H_RPMLOG */
