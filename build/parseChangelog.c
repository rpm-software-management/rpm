#include "system.h"

#include "rpmbuild.h"

static void addChangelogEntry(Header h, int time, char *name, char *text)
{
    if (headerIsEntry(h, RPMTAG_CHANGELOGTIME)) {
	headerAppendEntry(h, RPMTAG_CHANGELOGTIME, RPM_INT32_TYPE,
			  &time, 1);
	headerAppendEntry(h, RPMTAG_CHANGELOGNAME, RPM_STRING_ARRAY_TYPE,
			  &name, 1);
	headerAppendEntry(h, RPMTAG_CHANGELOGTEXT, RPM_STRING_ARRAY_TYPE,
			 &text, 1);
    } else {
	headerAddEntry(h, RPMTAG_CHANGELOGTIME, RPM_INT32_TYPE,
		       &time, 1);
	headerAddEntry(h, RPMTAG_CHANGELOGNAME, RPM_STRING_ARRAY_TYPE,
		       &name, 1);
	headerAddEntry(h, RPMTAG_CHANGELOGTEXT, RPM_STRING_ARRAY_TYPE,
		       &text, 1);
    }
}

/* datestr is of the form 'Wed Jan 1 1997' */
static int dateToTimet(const char * datestr, time_t * secs)
{
    struct tm time;
    char * chptr, * end, ** idx;
    char * date = strcpy(alloca(strlen(datestr) + 1), datestr);
    static char * days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", 
				NULL };
    static char * months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
			     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL };
    static char lengths[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    
    memset(&time, 0, sizeof(time));

    end = chptr = date;

    /* day of week */
    if ((chptr = strtok(date, " \t\n")) == NULL) return -1;
    idx = days;
    while (*idx && strcmp(*idx, chptr)) idx++;
    if (!*idx) return -1;

    /* month */
    if ((chptr = strtok(NULL, " \t\n")) == NULL) return -1;
    idx = months;
    while (*idx && strcmp(*idx, chptr)) idx++;
    if (!*idx) return -1;

    time.tm_mon = idx - months;

    /* day */
    if ((chptr = strtok(NULL, " \t\n")) == NULL) return -1;

    /* make this noon so the day is always right (as we make this UTC) */
    time.tm_hour = 12;

    time.tm_mday = strtol(chptr, &end, 10);
    if (!(end && *end == '\0')) return -1;
    if (time.tm_mday < 0 || time.tm_mday > lengths[time.tm_mon]) return -1;

    /* year */
    if ((chptr = strtok(NULL, " \t\n")) == NULL) return -1;

    time.tm_year = strtol(chptr, &end, 10);
    if (!(end && *end == '\0')) return -1;
    if (time.tm_year < 1997 || time.tm_year >= 3000) return -1;
    time.tm_year -= 1900;

    *secs = mktime(&time);
    if (*secs == -1) return -1;

    /* adjust to GMT */
    *secs += timezone;

    return 0;
}

static int addChangelog(Header h, StringBuf sb)
{
    char *s;
    int i;
    int time, lastTime = 0;
    char *date, *name, *text, *next;

    s = getStringBuf(sb);

    /* skip space */
    SKIPSPACE(s);

    while (*s) {
	if (*s != '*') {
	    rpmError(RPMERR_BADSPEC, _("%%changelog entries must start with *"));
	    return RPMERR_BADSPEC;
	}

	/* find end of line */
	date = s;
	SKIPTONEWLINE(s);
	if (! *s) {
	    rpmError(RPMERR_BADSPEC, _("incomplete %%changelog entry"));
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
	if (dateToTimet(date, (time_t *)&time)) {
	    rpmError(RPMERR_BADSPEC, _("bad date in %%changelog: %s"), date);
	    return RPMERR_BADSPEC;
	}
	if (lastTime && lastTime < time) {
	    rpmError(RPMERR_BADSPEC,
		     _("%%changelog not in decending chronological order"));
	    return RPMERR_BADSPEC;
	}
	lastTime = time;

	/* skip space to the name */
	SKIPSPACE(s);
	if (! *s) {
	    rpmError(RPMERR_BADSPEC, _("missing name in %%changelog"));
	    return RPMERR_BADSPEC;
	}

	/* name */
	name = s;
	while (*s) s++;
	while (s > name && isspace(*s)) {
	    *s-- = '\0';
	}
	if (s == name) {
	    rpmError(RPMERR_BADSPEC, _("missing name in %%changelog"));
	    return RPMERR_BADSPEC;
	}

	/* text */
	SKIPSPACE(text);
	if (! *text) {
	    rpmError(RPMERR_BADSPEC, _("no description in %%changelog"));
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
    if (rc) {
	return rc;
    }
    
    while (! (nextPart = isPart(spec->line))) {
	appendStringBuf(sb, spec->line);
	if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
	    nextPart = PART_NONE;
	    break;
	}
	if (rc) {
	    return rc;
	}
    }

    res = addChangelog(spec->packages->header, sb);
    freeStringBuf(sb);

    return (res) ? res : nextPart;
}
