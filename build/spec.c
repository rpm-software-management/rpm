/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * spec.c - routines for parsing a spec file
 */

/*****************************
TODO:

. multiline descriptions (general backslash processing)
. real arch/os checking
. %exclude
. %doc
. %config
. %setup
. %patch
. %dir and real directory handling
. root: option

******************************/

#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "header.h"
#include "spec.h"
#include "rpmerr.h"
#include "messages.h"
#include "rpmlib.h"

#define LINE_BUF_SIZE 1024
#define FREE(x) { if (x) free(x); }

struct SpecRec {
    char *name;      /* package base name */
    char *prep;
    char *build;
    char *install;
    char *clean;
    struct PackageRec *packages;
};

struct PackageRec {
    char *subname;   /* If both of these are NULL, then this is      */
    char *newname;   /* the main package.  subname concats with name */
    Header header;
    char *filelist;
    struct PackageRec *next;
};

static int read_line(FILE *f, char *line);
static int match_arch(char *s);
static int match_os(char *s);
static void free_packagerec(struct PackageRec *p);
static void reset_spec(void);
static int find_preamble_line(char *line, char **s);
static int check_part(char *line, char **s);

void free_packagerec(struct PackageRec *p)
{
    freeHeader(p->header);
    FREE(p->filelist);
    FREE(p->subname);
    FREE(p->newname);
    if (p->next) {
        free_packagerec(p->next);
    }
    free(p);
}

void freeSpec(Spec s)
{
    FREE(s->name);
    FREE(s->prep);
    FREE(s->build);
    FREE(s->install);
    FREE(s->clean);
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

static struct read_level_entry {
    int reading;  /* 1 if we are reading at this level */
    struct read_level_entry *next;
} *read_level = NULL;

static int read_line(FILE *f, char *line)
{
    static struct read_level_entry *rl;
    int gotline;
    char *r;

    do {
        gotline = 0;
	if (! fgets(line, LINE_BUF_SIZE, f)) {
	    /* the end */
	    if (read_level->next) {
	        return RPMERR_UNMATCHEDIF;
	    } else {
	        return 0;
	    }
	}
	if (! strncmp("%ifarch", line, 7)) {
	    rl = malloc(sizeof(struct read_level_entry));
	    rl->next = read_level;
	    rl->reading = read_level->reading && match_arch(line);
	    read_level = rl;
	} else if (! strncmp("%ifos", line, 5)) {
	    rl = malloc(sizeof(struct read_level_entry));
	    rl->next = read_level;
	    rl->reading = read_level->reading && match_os(line);
	    read_level = rl;
	} else if (! strncmp("%else", line, 5)) {
	    if (! read_level->next) {
	        /* Got an else with no %if ! */
	        return RPMERR_UNMATCHEDIF;
	    }
	    read_level->reading =
	        read_level->next->reading && ! read_level->reading;
	} else if (! strncmp("%endif", line, 6)) {
	    rl = read_level;
	    read_level = rl->next;
	    free(rl);
	} else {
            gotline = 1;
        }
    } while (! (gotline && read_level->reading));
    
    r = line + (strlen(line)) - 1;
    if (*r == '\n') {
        *r = '\0';
    }
    return 1;
}

struct preamble_line {
    int tag;
    int len;
    char *token;
} preamble_spec[] = {
    {RPMTAG_NAME,         0, "name"},
    {RPMTAG_VERSION,      0, "version"},
    {RPMTAG_RELEASE,      0, "release"},
    {RPMTAG_SERIAL,       0, "serial"},
    {RPMTAG_DESCRIPTION,  0, "description"},
    {RPMTAG_SUMMARY,      0, "summary"},
    {RPMTAG_COPYRIGHT,    0, "copyright"},
    {RPMTAG_DISTRIBUTION, 0, "distribution"},
    {RPMTAG_VENDOR,       0, "vendor"},
    {RPMTAG_GROUP,        0, "group"},
    {RPMTAG_PACKAGER,     0, "packager"},
    {0, 0, 0}
};

static int find_preamble_line(char *line, char **s)
{
    struct preamble_line *p = preamble_spec;

    while (p->token && strncasecmp(line, p->token, p->len)) {
        p++;
    }
    if (!p) return 0;
    *s = line + p->len;
    *s += strspn(*s, ": \t");
    return p->tag;
}

/* None of these can be 0 !! */
#define PREAMBLE_PART 1
#define PREP_PART     2
#define BUILD_PART    3
#define INSTALL_PART  4
#define CLEAN_PART    5
#define PREIN_PART    6
#define POSTIN_PART   7
#define PREUN_PART    8
#define POSTUN_PART   9
#define FILES_PART    10

static struct part_rec {
    int part;
    int len;
    char *s;
} part_list[] = {
    {PREAMBLE_PART, 0, "%package"},
    {PREP_PART,     0, "%prep"},
    {BUILD_PART,    0, "%build"},
    {INSTALL_PART,  0, "%install"},
    {CLEAN_PART,    0, "%clean"},
    {PREIN_PART,    0, "%prein"},
    {POSTIN_PART,   0, "%postin"},
    {PREUN_PART,    0, "%preun"},
    {POSTUN_PART,   0, "%postun"},
    {FILES_PART,    0, "%files"},
    {0, 0, 0}
};

static int check_part(char *line, char **s)
{
    struct part_rec *p = part_list;

    while (p->s && strncasecmp(line, p->s, p->len)) {
        p++;
    }
    if (!p) return 0;
    *s = line + p->len;
    *s += strspn(*s, " \t");
    if (**s == '\0') {
        *s = NULL;
    }
    return p->part;
}

Spec parseSpec(FILE *f)
{
    char line[LINE_BUF_SIZE];
    char *s;
    int cur_part = PREAMBLE_PART;
    struct PackageRec *cur_package = NULL;

    int x, tag;

    Spec spec = (struct SpecRec *) malloc(sizeof(struct SpecRec));
    spec->name = NULL;

    reset_spec();
    
    while ((x = read_line(f, line)) > 0) {
        if ((tag = check_part(line, &s))) {
	    cur_part = tag;
	    printf("Switching to: %d\n", cur_part);
	    if (s) {
	        printf("Subname: %s\n", s);
	    }
	    continue;
        }
      
        switch (cur_part) {
	  case PREAMBLE_PART:
	    if ((tag = find_preamble_line(line, &s))) {
	        switch (tag) {
		  case RPMTAG_NAME:
		  case RPMTAG_VERSION:
		  case RPMTAG_RELEASE:
		  case RPMTAG_SERIAL:
		  case RPMTAG_SUMMARY:
		  case RPMTAG_DESCRIPTION:
		  case RPMTAG_INSTALLTIME:
		  case RPMTAG_DISTRIBUTION:
		  case RPMTAG_VENDOR:
		  case RPMTAG_COPYRIGHT:
		  case RPMTAG_PACKAGER:
		  case RPMTAG_GROUP:
		  case RPMTAG_URL:
		    printf("%d: %s\n", tag, s);
		    break;
		  default:
		    printf("Skipping: %s\n", line);
		}		
	    } else {
	        /* Not a recognized preamble part */
	        printf("Unknown: %s\n", line);
	    }
	    break;
	  case PREP_PART:
	  case BUILD_PART:
	  case INSTALL_PART:
	  case CLEAN_PART:
	    break;
	  case PREIN_PART:
	  case POSTIN_PART:
	  case PREUN_PART:
	  case POSTUN_PART:
	    break;
	  case FILES_PART:
	    break;
	  default:
	    /* error(RPMERR_INTERNALBADPART, "Internal error"); */
	    printf("%s\n", line);
	}
    }
    if (x < 0) {
        fprintf(stderr, "ERROR\n");
	/* error */
	return NULL;
    }
    
    return spec;
}

static void reset_spec()
{
    static done = 0;
    struct read_level_entry *rl;
    struct preamble_line *p = preamble_spec;
    struct part_rec *p1 = part_list;

    while (read_level) {
	rl = read_level;
        read_level = read_level->next;
	free(rl);
    }
    read_level = malloc(sizeof(struct read_level_entry));
    read_level->next = NULL;
    read_level->reading = 1;
    
    if (! done) {
        /* Put one time only things in here */
        while (p->tag) {
	    p->len = strlen(p->token);
	    p++;
	}
        while (p1->part) {
	    p1->len = strlen(p1->s);
	    p1++;
	}

	done = 1;
    }
}
