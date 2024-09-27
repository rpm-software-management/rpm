#ifndef H_RPMLOG_INTERNAL
#define H_RPMLOG_INTERNAL 1


/** \ingroup rpmlog
 * Generate a log message using FMT string and option arguments.
 * Only actually log on the first time passing the key value
 * @param domain	group of messages to be reset together
 * @param key		key to match log messages together
 * @param code		rpmlogLvl
 * @param fmt		format string and parameter to render
 * @return		1 if actually logging 0 otherwise
 */
int rpmlogOnce (uint64_t domain, const char * key, int code, const char *fmt, ...) RPM_GNUC_PRINTF(4, 5);

/** \ingroup rpmlog
 * Clear memory of logmessages for a given domain
 * @param domain	group of messages to be reset together
 * @param mode		curretnly only 0 supported whihc drops everything
 */
void rpmlogReset(uint64_t domain, int mode=0);

#endif
