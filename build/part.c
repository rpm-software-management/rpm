#include "system.h"

#include "part.h"

static struct PartRec {
    int part;
    int len;
    char *token;
} partList[] = {
    {PART_PREAMBLE,      0, "%package"},
    {PART_PREP,          0, "%prep"},
    {PART_BUILD,         0, "%build"},
    {PART_INSTALL,       0, "%install"},
    {PART_CLEAN,         0, "%clean"},
    {PART_PREUN,         0, "%preun"},
    {PART_POSTUN,        0, "%postun"},
    {PART_PRE,           0, "%pre"},
    {PART_POST,          0, "%post"},
    {PART_FILES,         0, "%files"},
    {PART_CHANGELOG,     0, "%changelog"},
    {PART_DESCRIPTION,   0, "%description"},
    {PART_TRIGGERPOSTUN, 0, "%triggerpostun"},
    {PART_TRIGGERUN,     0, "%triggerun"},
    {PART_TRIGGERIN,     0, "%triggerin"},
    {PART_TRIGGERIN,     0, "%trigger"},
    {PART_VERIFYSCRIPT,  0, "%verifyscript"},
    {0, 0, 0}
};

static void initParts(void)
{
    struct PartRec *p = partList;

    while (p->token) {
	p->len = strlen(p->token);
	p++;
    }
}

int isPart(char *line)
{
    char c;
    struct PartRec *p = partList;

    if (p->len == 0) {
	initParts();
    }
    
    while (p->token) {
	if (! strncmp(line, p->token, p->len)) {
	    c = *(line + p->len);
	    if (c == '\0' || isspace(c)) {
		break;
	    }
	}
	p++;
    }

    if (p->token) {
	return p->part;
    } else {
	return PART_NONE;
    }
}

