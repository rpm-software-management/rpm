/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * spec.c - routines for parsing a spec file
 */

/*****************************
TODO:

. skip blank lines
. strip leading/trailing spaces in %preamble and %files
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
#include "specP.h"
#include "rpmerr.h"
#include "messages.h"
#include "rpmlib.h"
#include "stringbuf.h"

#define LINE_BUF_SIZE 1024
#define FREE(x) { if (x) free(x); }

static struct PackageRec *new_packagerec(void);
static int read_line(FILE *f, char *line);
static int match_arch(char *s);
static int match_os(char *s);
static void free_packagerec(struct PackageRec *p);
static void reset_spec(void);
static int find_preamble_line(char *line, char **s);
static int check_part(char *line, char **s);
int lookup_package(Spec s, struct PackageRec **pr, char *name, int flags);
static void dumpPackage(struct PackageRec *p, FILE *f);
char *chop_line(char *s);

/**********************************************************************/
/*                                                                    */
/* Spec and package structure creation/deletion/lookup                */
/*                                                                    */
/**********************************************************************/

static struct PackageRec *new_packagerec(void)
{
    struct PackageRec *p = malloc(sizeof(struct PackageRec));

    p->subname = NULL;
    p->newname = NULL;
    p->header = newHeader();
    p->filelist = newStringBuf();
    p->files = 0;
    p->next = NULL;

    return p;
}

void free_packagerec(struct PackageRec *p)
{
    freeHeader(p->header);
    freeStringBuf(p->filelist);
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
    freeStringBuf(s->prep);
    freeStringBuf(s->build);
    freeStringBuf(s->install);
    freeStringBuf(s->clean);
    free_packagerec(s->packages);
    free(s);
}

#define LP_CREATE           1
#define LP_FAIL_EXISTS     (1 << 1)
#define LP_SUBNAME         (1 << 2)
#define LP_NEWNAME         (1 << 3)

int lookup_package(Spec s, struct PackageRec **pr, char *name, int flags)
{
    struct PackageRec *package;
    struct PackageRec **ppp;

    package = s->packages;
    while (package) {
	if (flags & (LP_SUBNAME | LP_NEWNAME)) {
	    if ((! package->newname) & (! package->subname)) {
		package = package->next;
		continue;
	    }
	}
	if (flags & LP_SUBNAME) {
	    if (! strcmp(package->subname, name)) {
		break;
	    }
	} else if (flags & LP_NEWNAME) {
	    if (! strcmp(package->newname, name)) {
		break;
	    }
	} else {
	    /* Base package */
	    if ((! package->newname) & (! package->subname)) {
		break;
	    }
	}
	package = package->next;
    }
    
    if (package && (flags & LP_FAIL_EXISTS)) {
	return 0;
    }

    if (package) {
	*pr = package;
	return 1;
    }

    /* At this point the package does not exist */

    if (! (flags & LP_CREATE)) {
	return 0;
    }

    /* Create it */
    package = new_packagerec();
    if (flags & LP_SUBNAME) {
	package->subname = strdup(name);
    } else if (flags & LP_NEWNAME) {
	package->newname = strdup(name);
    }

    /* Link it in to the spec */
    ppp = &(s->packages);
    while (*ppp) {
	ppp = &((*ppp)->next);
    }
    *ppp = package;

    *pr = package;
    return 1;
}

/**********************************************************************/
/*                                                                    */
/* Line reading                                                       */
/*                                                                    */
/**********************************************************************/

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

/**********************************************************************/
/*                                                                    */
/* Line parsing                                                       */
/*                                                                    */
/**********************************************************************/

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

char *chop_line(char *s)
{
    char *p, *e;

    p = s;
    p += strspn(s, " \t");
    if (*p == '\0') {
	return NULL;
    }
    e = s + strlen(s) - 1;
    while (index(" \t", *e)) {
	e--;
    }
    return p;
}

/**********************************************************************/
/*                                                                    */
/* Main specfile parsing routine                                      */
/*                                                                    */
/**********************************************************************/

Spec parseSpec(FILE *f)
{
    char buf[LINE_BUF_SIZE]; /* read buffer          */
    char *line;              /* "parsed" read buffer */
    
    int x, tag, cur_part, t1;
    int lookupopts;
    StringBuf sb;
    char *s = NULL;

    struct PackageRec *cur_package = NULL;
    Spec spec = (struct SpecRec *) malloc(sizeof(struct SpecRec));

    spec->name = NULL;
    spec->prep = newStringBuf();
    spec->build = newStringBuf();
    spec->install = newStringBuf();
    spec->clean = newStringBuf();
    spec->packages = NULL;

    sb = newStringBuf();
    reset_spec();         /* Reset the parser */
    
    cur_part = PREAMBLE_PART;
    while ((x = read_line(f, buf)) > 0) {
	line = buf;
	s = NULL;
        if ((tag = check_part(line, &s))) {
	    printf("Switching to part: %d\n", tag);
	    switch (cur_part) {
	      case PREIN_PART:
		t1 = RPMTAG_PREIN;
	      case POSTIN_PART:
		t1 = RPMTAG_POSTIN;
	      case PREUN_PART:
		t1 = RPMTAG_PREUN;
	      case POSTUN_PART:
		t1 = RPMTAG_POSTUN;
		addEntry(cur_package->header, t1,
			 STRING_TYPE, getStringBuf(sb), 1);
		break;
	    }
	    cur_part = tag;
	    truncStringBuf(sb);

	    /* Now switch the current package to s */
	    lookupopts = (s) ? LP_SUBNAME : 0;
	    if (tag == PREAMBLE_PART) {
		lookupopts |= LP_CREATE | LP_FAIL_EXISTS;
	    }
	    if (s) {
	        switch (tag) {
		  case PREP_PART:
		  case BUILD_PART:
		  case INSTALL_PART:
		  case CLEAN_PART:
		    error(RPMERR_BADARG, "Tag takes no arguments: %s", s);
		    return NULL;
	        }
	    }

	    if (! lookup_package(spec, &cur_package, s, lookupopts)) {
		error(RPMERR_INTERNAL, "Package lookup failed!");
		return NULL;
	    }

	    printf("Switched to package: %s\n", s);
	    continue;
        }

	/* Check for implicit "base" package */
	if (! cur_package) {
	    lookupopts = 0;
	    if (cur_part == PREAMBLE_PART) {
		lookupopts = LP_CREATE | LP_FAIL_EXISTS;
	    }
	    if (! lookup_package(spec, &cur_package, NULL, lookupopts)) {
		error(RPMERR_INTERNAL, "Base package lookup failed!");
		return NULL;
	    }
	    printf("Switched to BASE package\n");
	}
	
        switch (cur_part) {
	  case PREAMBLE_PART:
	    if ((tag = find_preamble_line(line, &s))) {
	        switch (tag) {
		  case RPMTAG_NAME:
		    if (!spec->name) {
			spec->name = strdup(s);
		    }
		  case RPMTAG_VERSION:
		  case RPMTAG_RELEASE:
		  case RPMTAG_SERIAL:
		  case RPMTAG_SUMMARY:
		  case RPMTAG_DESCRIPTION:
		  case RPMTAG_DISTRIBUTION:
		  case RPMTAG_VENDOR:
		  case RPMTAG_COPYRIGHT:
		  case RPMTAG_PACKAGER:
		  case RPMTAG_GROUP:
		  case RPMTAG_URL:
		    addEntry(cur_package->header, tag, STRING_TYPE, s, 1);
		    break;
		  default:
		    printf("Skipping: %s\n", line);
		}		
	    } else {
	        /* Not a recognized preamble part */
	        printf("Unknown Field: %s\n", line);
	    }
	    break;
	  case PREP_PART:
	    appendLineStringBuf(spec->prep, line);
	    break;
	  case BUILD_PART:
	    appendLineStringBuf(spec->build, line);
	    break;
	  case INSTALL_PART:
	    appendLineStringBuf(spec->install, line);
	    break;
	  case CLEAN_PART:
	    appendLineStringBuf(spec->clean, line);
	    break;
	  case PREIN_PART:
	  case POSTIN_PART:
	  case PREUN_PART:
	  case POSTUN_PART:
	    appendLineStringBuf(sb, line);
	    break;
	  case FILES_PART:
	    cur_package->files++;
	    appendLineStringBuf(cur_package->filelist, line);
	    break;
	  default:
	    error(RPMERR_INTERNAL, "Bad part");
	    return NULL;
	} /* switch */
    }
    if (x < 0) {
	error(RPMERR_READERROR, "Error reading specfile");
	return NULL;
    }

    return spec;
}

/**********************************************************************/
/*                                                                    */
/* Resets the parser                                                  */
/*                                                                    */
/**********************************************************************/

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

/**********************************************************************/
/*                                                                    */
/* Spec struct dumping (for debugging)                                */
/*                                                                    */
/**********************************************************************/

void dumpSpec(Spec s, FILE *f)
{
    struct PackageRec *p;
    
    fprintf(f, "########################################################\n");
    fprintf(f, "SPEC NAME = (%s)\n", s->name);
    fprintf(f, "PREP =v\n");
    fprintf(f, "%s", getStringBuf(s->prep));
    fprintf(f, "PREP =^\n");
    fprintf(f, "BUILD =v\n");
    fprintf(f, "%s", getStringBuf(s->build));
    fprintf(f, "BUILD =^\n");
    fprintf(f, "INSTALL =v\n");
    fprintf(f, "%s", getStringBuf(s->install));
    fprintf(f, "INSTALL =^\n");
    fprintf(f, "CLEAN =v\n");
    fprintf(f, "%s", getStringBuf(s->clean));
    fprintf(f, "CLEAN =^\n");

    p = s->packages;
    while (p) {
	dumpPackage(p, f);
	p = p->next;
    }
}

static void dumpPackage(struct PackageRec *p, FILE *f)
{
    fprintf(f, "_________________________________________________________\n");
    fprintf(f, "SUBNAME = (%s)\n", p->subname);
    fprintf(f, "NEWNAME = (%s)\n", p->newname);
    fprintf(f, "FILES = %d\n", p->files);
    fprintf(f, "FILES =v\n");
    fprintf(f, "%s", getStringBuf(p->filelist));
    fprintf(f, "FILES =^\n");
    fprintf(f, "HEADER =v\n");
    dumpHeader(p->header, f, 1);
    fprintf(f, "HEADER =^\n");

}
