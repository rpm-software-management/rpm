#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "spec.h"
#include "rpmlib.h"
#include "messages.h"
#include "macro.h"
#include "misc.h"
#include "read.h"

static int matchTok(char *token, char *line);

/* returns 0 - success */
/*         1 - EOF     */
/*        <0 - error   */

void handleComments(char *s)
{
    SKIPSPACE(s);
    if (*s == '#') {
	*s = '\0';
    }
}

int readLine(Spec spec, int strip)
{
    char *from, *to, *first, *last, *s, *arch, *os;
    int match;
    char ch;
    struct ReadLevelEntry *rl;

    /* Make sure the spec file is open */
    if (!spec->file) {
	if (!(spec->file = fopen(spec->specFile, "r"))) {
	    rpmError(RPMERR_BADSPEC, "Unable to open: %s\n", spec->specFile);
	    return RPMERR_BADSPEC;
	}
	spec->lineNum = 0;
    }

    /* Make sure we have something in the read buffer */
    if (!spec->readPtr || ! *(spec->readPtr)) {
	if (!fgets(spec->readBuf, BUFSIZ, spec->file)) {
	    /* EOF */
	    if (spec->readStack->next) {
		rpmError(RPMERR_UNMATCHEDIF, "Unclosed %%if");
	        return RPMERR_UNMATCHEDIF;
	    }
	    return 1;
	}
	spec->readPtr = spec->readBuf;
	spec->lineNum++;
	/*rpmMessage(RPMMESS_DEBUG, "LINE: %s", spec->readBuf);*/
    }
    
    /* Copy a single line to the line buffer */
    from = spec->readPtr;
    to = last = spec->line;
    ch = ' ';
    while (*from && ch != '\n') {
	ch = *to++ = *from++;
	if (!isspace(ch)) {
	    last = to;
	}
    }
    *to = '\0';
    spec->readPtr = from;
    
    if (strip & STRIP_COMMENTS) {
	handleComments(spec->line);
    }
    
    if (strip & STRIP_TRAILINGSPACE) {
	*last = '\0';
    }

    if (spec->readStack->reading) {
	expandMacros(&spec->macros, spec->line);
    }
    
    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);
    s = spec->line;
    SKIPSPACE(s);
    match = -1;
    if (! strncmp("%ifarch", s, 7)) {
	match = matchTok(arch, s);
    } else if (! strncmp("%ifnarch", s, 8)) {
	match = !matchTok(arch, s);
    } else if (! strncmp("%ifos", s, 5)) {
	match = matchTok(os, s);
    } else if (! strncmp("%ifnos", s, 6)) {
	match = !matchTok(os, s);
    } else if (! strncmp("%else", s, 5)) {
	if (! spec->readStack->next) {
	    /* Got an else with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF, "line %d: Got a %%else with no if",
		     spec->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	spec->readStack->reading =
	    spec->readStack->next->reading && ! spec->readStack->reading;
	spec->line[0] = '\0';
    } else if (! strncmp("%endif", s, 6)) {
	if (! spec->readStack->next) {
	    /* Got an end with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF, "line %d: Got a %%endif with no if",
		     spec->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	rl = spec->readStack;
	spec->readStack = spec->readStack->next;
	free(rl);
	spec->line[0] = '\0';
    }
    if (match != -1) {
	rl = malloc(sizeof(struct ReadLevelEntry));
	rl->reading = spec->readStack->reading && match;
	rl->next = spec->readStack;
	spec->readStack = rl;
	spec->line[0] = '\0';
    }
    
    if (! spec->readStack->reading) {
	spec->line[0] = '\0';
    }

    return 0;
}

static int matchTok(char *token, char *line)
{
    char buf[BUFSIZ], *tok;

    strcpy(buf, line);
    strtok(buf, " \n\t");
    while ((tok = strtok(NULL, " \n\t"))) {
	if (! strcmp(tok, token)) {
	    return 1;
	}
    }

    return 0;
}

void closeSpec(Spec spec)
{
    if (spec->file) {
	fclose(spec->file);
    }
    spec->file = NULL;
}
