/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * spec.c - routines for parsing a spec file
 */

#include "header.h"
#include "spec.h"
#include "rpmerr.h"
#include "messages.h"
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#define LINE_BUF_SIZE 1024

struct SpecRec {
    char *prep;
    char *build;
    char *install;
    char *clean;
    struct PackageRec *packages;
};

struct PackageRec {
    Header header;
    char *filelist;
    struct PackageRec *next;
};

static int read_line(FILE *f, char *line);
static int match_arch(char *s);
static int match_os(char *s);
static void free_packagerec(struct PackageRec *p);
static void init_spec(void);

void free_packagerec(struct PackageRec *p)
{
    freeHeader(p->header);
    free(p->filelist);
    if (p->next) {
        free_packagerec(p->next);
    }
    free(p);
}

void freeSpec(Spec s)
{
    free(s->prep);
    free(s->build);
    free(s->install);
    free(s->clean);
    free_packagerec(s->packages);
    free(s);
}

static int match_arch(char *s)
{
    if (! strncmp(s, "%ifarch i386", 12)) {
	return 1;
    } else {
	return 0;
    }
}

static int match_os(char *s)
{
    if (! strncmp(s, "%ifos linux", 11)) {
	return 1;
    } else {
	return 0;
    }
}

static int reading = 0;
static int iflevels = 0;
static int skiplevels = 0;
static int skip = 0;

static int read_line(FILE *f, char *line)
{
    char *r = fgets(line, LINE_BUF_SIZE, f);

    if (! r) {
	/* the end */
	if (iflevels) return RPMERR_UNMATCHEDIF;
	return 0;
    }

    skip = 0;

    if (! strncmp("%ifarch", line, 7)) {
	iflevels++;
	skip = 1;
	if (! match_arch(line)) {
	    reading = 0;
	    skiplevels++;
	}
    }
    if (! strncmp("%ifos", line, 5)) {
	iflevels++;
	skip = 1;
	if (! match_os(line)) {
	    reading = 0;
	    skiplevels++;
	}
    }

    if (! strncmp("%endif", line, 6)) {
	iflevels--;
	skip = 1;
	if (skiplevels) {
	    if (! --skiplevels) reading = 1;
	}
    }
    
    return 1;
}

static regex_t name_regex;
static regex_t description_regex;
static regex_t package_regex;
static regex_t version_regex;
static regex_t release_regex;
static regex_t copyright_regex;
static regex_t icon_regex;
static regex_t group_regex;

struct regentry {
    regex_t *p;
    char *s;
} regexps[] = {
    {&package_regex,     "^package[[:space:]]*:[[:space:]]*"},
    {&name_regex,        "^name[[:space:]]*:[[:space:]]*"},
    {&description_regex, "^description[[:space:]]*:[[:space:]]*"},
    {&version_regex,     "^version[[:space:]]*:[[:space:]]*"},
    {&release_regex,     "^release[[:space:]]*:[[:space:]]*"},
    {&copyright_regex,   "^copyright[[:space:]]*:[[:space:]]*"},
    {&group_regex,       "^group[[:space:]]*:[[:space:]]*"},
    {&icon_regex,        "^icon[[:space:]]*:[[:space:]]*"},
    {0, 0}
};

static void init_spec()
{
    static done = 0;
    struct regentry *r = regexps;
    int opts = REG_ICASE | REG_EXTENDED;
    
    if (! done) {
        while(r->p) {
	    regcomp(r->p, r->s, opts);
	    r++;
	}
	done = 1;
    }
}

#define PACKAGE_PART 0
#define PREP_PART    1
#define BUILD_PART   2
#define INSTALL_PART 3
#define CLEAN_PART   4
#define PREIN_PART   5
#define POSTIN_PART  6
#define PREUN_PART   7
#define POSTUN_PART  8
#define FILES_PART   9

Spec parseSpec(FILE *f)
{
    char line[LINE_BUF_SIZE];
    Spec s = (struct SpecRec *) malloc(sizeof(struct SpecRec));
    int part = PACKAGE_PART;

    int x;
    regmatch_t match;

    init_spec();
    
    reading = 1;
    while ((x = read_line(f, line))) {
	if (!reading) continue;
	if (skip) continue;
	/* If we get here, this line is for us */
	printf(line);
	if (! regexec(&description_regex, line, 1, &match, 0)) {
	    message(MESS_DEBUG, "description: %s", line + match.rm_eo);
	} else if (! regexec(&name_regex, line, 1, &match, 0)) {
	    message(MESS_DEBUG, "name       : %s", line + match.rm_eo);
	}
    }
    if (x < 0) {
        fprintf(stderr, "ERROR\n");
	/* error */
	return NULL;
    }
    
    return s;
}
