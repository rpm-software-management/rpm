/** \ingroup rpmbuild
 * \file build/parseChangelog.c
 *  Parse %changelog section from spec file.
 */

#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

void addChangelogEntry(Header h, time_t time, const char *name, const char *text)
{
    int_32 mytime = time;	/* XXX convert to header representation */
    if (headerIsEntry(h, RPMTAG_CHANGELOGTIME)) {
	headerAppendEntry(h, RPMTAG_CHANGELOGTIME, RPM_INT32_TYPE,
			  &mytime, 1);
	headerAppendEntry(h, RPMTAG_CHANGELOGNAME, RPM_STRING_ARRAY_TYPE,
			  &name, 1);
	headerAppendEntry(h, RPMTAG_CHANGELOGTEXT, RPM_STRING_ARRAY_TYPE,
			 &text, 1);
    } else {
	headerAddEntry(h, RPMTAG_CHANGELOGTIME, RPM_INT32_TYPE,
		       &mytime, 1);
	headerAddEntry(h, RPMTAG_CHANGELOGNAME, RPM_STRING_ARRAY_TYPE,
		       &name, 1);
	headerAddEntry(h, RPMTAG_CHANGELOGTEXT, RPM_STRING_ARRAY_TYPE,
		       &text, 1);
    }
}

/**
 * Parse date string to seconds.
 * @param datestr	date string (e.g. 'Wed Jan 1 1997')
 * @retval secs		secs since the unix epoch
 * @return 		0 on success, -1 on error
 */
static int dateToTimet(const char * datestr, /*@out@*/ time_t * secs)
{
    struct tm time;
    char *p, *pe, *q, ** idx;
    char * date = strcpy(alloca(strlen(datestr) + 1), datestr);
    static char * days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", 
				NULL };
    static char * months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
			     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL };
    static char lengths[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    
    memset(&time, 0, sizeof(time));

    pe = date;

    /* day of week */
    p = pe; SKIPSPACE(p);
    if (*p == '\0') return -1;
    pe = p; SKIPNONSPACE(pe); if (*pe) *pe++ = '\0';
    for (idx = days; *idx && strcmp(*idx, p); idx++)
	;
    if (*idx == NULL) return -1;

    /* month */
    p = pe; SKIPSPACE(p);
    if (*p == '\0') return -1;
    pe = p; SKIPNONSPACE(pe); if (*pe) *pe++ = '\0';
    for (idx = months; *idx && strcmp(*idx, p); idx++)
	;
    if (*idx == NULL) return -1;
    time.tm_mon = idx - months;

    /* day */
    p = pe; SKIPSPACE(p);
    if (*p == '\0') return -1;
    pe = p; SKIPNONSPACE(pe); if (*pe) *pe++ = '\0';

    /* make this noon so the day is always right (as we make this UTC) */
    time.tm_hour = 12;

    time.tm_mday = strtol(p, &q, 10);
    if (!(q && *q == '\0')) return -1;
    if (time.tm_mday < 0 || time.tm_mday > lengths[time.tm_mon]) return -1;

    /* year */
    p = pe; SKIPSPACE(p);
    if (*p == '\0') return -1;
    pe = p; SKIPNONSPACE(pe); if (*pe) *pe++ = '\0';
    time.tm_year = strtol(p, &q, 10);
    if (!(q && *q == '\0')) return -1;
    if (time.tm_year < 1997 || time.tm_year >= 3000) return -1;
    time.tm_year -= 1900;

    *secs = mktime(&time);
    if (*secs == -1) return -1;

    /* adjust to GMT */
    *secs += timezone;

    return 0;
}

/**
 * Add %changelog section to header.
 * @param h		header
 * @param sb		changelog strings
 * @return		0 on success
 */
static int addChangelog(Header h, StringBuf sb)
{
    char *s;
    int i;
    time_t time;
    time_t lastTime = 0;
    char *date, *name, *text, *next;

    s = getStringBuf(sb);

    /* skip space */
    SKIPSPACE(s);

    while (*s) {
	if (*s != '*') {
	    rpmError(RPMERR_BADSPEC,
			_("%%changelog entries must start with *\n"));
	    return RPMERR_BADSPEC;
	}

	/* find end of line */
	date = s;
	while(*s && *s != '\n') s++;
	if (! *s) {
	    rpmError(RPMERR_BADSPEC, _("incomplete %%changelog entry\n"));
	    return RPMERR_BADSPEC;
	}
	*s = '\0';
	text = s + 1;
	
	/* 4 fields of date */
	date++;
	s = date;
	for (i = 0; i < 4; i++) {
	    SKIPSPACE(s);
	    SKIPNONSPACE(s);
	}
	SKIPSPACE(date);
	if (dateToTimet(date, &time)) {
	    rpmError(RPMERR_BADSPEC, _("bad date in %%changelog: %s\n"), date);
	    return RPMERR_BADSPEC;
	}
	if (lastTime && lastTime < time) {
	    rpmError(RPMERR_BADSPEC,
		     _("%%changelog not in decending chronological order\n"));
	    return RPMERR_BADSPEC;
	}
	lastTime = time;

	/* skip space to the name */
	SKIPSPACE(s);
	if (! *s) {
	    rpmError(RPMERR_BADSPEC, _("missing name in %%changelog\n"));
	    return RPMERR_BADSPEC;
	}

	/* name */
	name = s;
	while (*s) s++;
	while (s > name && isspace(*s)) {
	    *s-- = '\0';
	}
	if (s == name) {
	    rpmError(RPMERR_BADSPEC, _("missing name in %%changelog\n"));
	    return RPMERR_BADSPEC;
	}

	/* text */
	SKIPSPACE(text);
	if (! *text) {
	    rpmError(RPMERR_BADSPEC, _("no description in %%changelog\n"));
	    return RPMERR_BADSPEC;
	}
	    
	/* find the next leading '*' (or eos) */
	s = text;
	do {
	   s++;
	} while (*s && (*(s-1) != '\n' || *s != '*'));
	next = s;
	s--;

	/* backup to end of description */
	while ((s > text) && isspace(*s)) {
	    *s-- = '\0';
	}
	
	addChangelogEntry(h, time, name, text);
	s = next;
    }

    return 0;
}

int parseChangelog(Spec spec)
{
    int nextPart, res, rc;
    StringBuf sb;

    sb = newStringBuf();
    
    /* There are no options to %changelog */
    if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
	freeStringBuf(sb);
	return PART_NONE;
    }
    if (rc)
	return rc;
    
    while (! (nextPart = isPart(spec->line))) {
	appendStringBuf(sb, spec->line);
	if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
	    nextPart = PART_NONE;
	    break;
	}
	if (rc)
	    return rc;
    }

    res = addChangelog(spec->packages->header, sb);
    freeStringBuf(sb);

    return (res) ? res : nextPart;
}
