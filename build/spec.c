/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * spec.c - routines for parsing a spec file
 */

#include "header.h"
#include "spec.h"
#include "rpmerr.h"
#include <stdlib.h>
#include <string.h>

#define LINE_BUF_SIZE 1024

struct spec {
    Header header;
    char *prep;
    char *build;
    char *install;
    char *clean;
};

void free_spec(Spec s)
{
    freeHeader(s->header);
    free(s->prep);
    free(s->build);
    free(s->install);
    free(s->clean);
    free(s);
}

static int read_line(FILE *f, char *line);
static int match_arch(char *s);
static int match_os(char *s);

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

Spec parse_spec(FILE *f)
{
    char line[LINE_BUF_SIZE];
    Spec s = (struct spec *) malloc(sizeof(struct spec));
    int x;

    reading = 1;
    
    while ((x = read_line(f, line))) {
	if (!reading) continue;
	if (skip) continue;
	puts(line);
    }
    if (x < 0) {
	/* error */
	return NULL;
    }
    
    return s;
}
