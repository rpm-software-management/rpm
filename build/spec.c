/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * spec.c - routines for parsing a spec file
 */

/*****************************
TODO:

. strip blank lines, leading/trailing spaces in %preamble
. %doc globbing
. should be able to drop the -n in non-%package parts

******************************/

#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <limits.h>

#include "header.h"
#include "spec.h"
#include "specP.h"
#include "rpmerr.h"
#include "messages.h"
#include "rpmlib.h"
#include "stringbuf.h"
#include "misc.h"

#define LINE_BUF_SIZE 1024
#define FREE(x) { if (x) free(x); }

static struct PackageRec *new_packagerec(void);
static int read_line(FILE *f, char *line);
static int match_arch(char *s);
static int match_os(char *s);
static void free_packagerec(struct PackageRec *p);
static void generateNamesAndDocScript(Spec s);
static void reset_spec(void);
static int find_preamble_line(char *line, char **s);
static int check_part(char *line, char **s);
static int lookup_package(Spec s, struct PackageRec **pr,
			  char *name, int flags);
static void dumpPackage(struct PackageRec *p, FILE *f);
static char *chop_line(char *s);
static void parseForDocFiles(struct PackageRec *package, char *line);

/**********************************************************************/
/*                                                                    */
/* Source and patch structure creation/deletion/lookup                */
/*                                                                    */
/**********************************************************************/

static int addSource(Spec spec, char *line)
{
    struct sources *p;
    char *s, *s1, c;
    char *file;
    unsigned long int x;

    p = malloc(sizeof(struct sources));
    p->next = spec->sources;
    spec->sources = p;

    if (! strncasecmp(line, "source", 6)) {
	spec->numSources++;
	p->ispatch = 0;
	s = line + 6;
    } else if (! strncasecmp(line, "patch", 5)) {
	spec->numPatches++;
	p->ispatch = 1;
	s = line + 5;
    } else {
	error(RPMERR_BADSPEC, "Not a source/patch line: %s\n", line);
	return(RPMERR_BADSPEC);
    }

    s += strspn(s, " \t\n");
    p->num = 0;
    if (*s != ':') {
        x = strspn(s, "0123456789");
	if (! x) {
	    error(RPMERR_BADSPEC, "Bad source/patch line: %s\n", line);
	    return(RPMERR_BADSPEC);
	}
	c = s[x];
	s[x] = '\0';
	s1 = NULL;
	p->num = strtoul(s, &s1, 10);
	if ((*s1) || (s1 == s) || (p->num == ULONG_MAX)) {
	    s[x] = c;
	    error(RPMERR_BADSPEC, "Bad source/patch number: %s\n", s);
	    return(RPMERR_BADSPEC);
	}
	s[x] = c;
	s += x;
	/* skip spaces */
	s += strspn(s, " \t\n");
    }

    if (*s != ':') {
	error(RPMERR_BADSPEC, "Bad source/patch line: %s\n", line);
	return(RPMERR_BADSPEC);
    }
    
    /* skip to actual source */
    s++;
    s += strspn(s, " \t\n");

    file = strtok(s, " \t\n");
    if (! file) {
	error(RPMERR_BADSPEC, "Bad source/patch line: %s\n", line);
	return(RPMERR_BADSPEC);
    }
    p->fullSource = strdup(file);
    p->source = strrchr(p->fullSource, '/');
    if (p->source) {
	p->source++;
    } else {
	p->source = p->fullSource;
    }

    if (p->ispatch) {
	message(MESS_DEBUG, "Patch(%d) = %s\n", p->num, p->fullSource);
    } else {
	message(MESS_DEBUG, "Source(%d) = %s\n", p->num, p->fullSource);
    }
    
    return 0;
}

static void freeSources(Spec s)
{
    struct sources *p1, *p2;

    p1 = s->sources;
    while (p1) {
	p2 = p1;
	p1 = p1->next;
	free(p2->fullSource);
	free(p2);
    }
}

char *getSource(Spec s, int ispatch, int num)
{
    struct sources *p = s->sources;

    while (p) {
	if ((ispatch == p->ispatch) &&
	    (num == p->num)) {
	    break;
	} else {
	    p = p->next;
	}
    }

    if (p) {
	return(p->source);
    } else {
	return(NULL);
    }
}

char *getFullSource(Spec s, int ispatch, int num)
{
    struct sources *p = s->sources;

    while (p) {
	if ((ispatch == p->ispatch) &&
	    (num == p->num)) {
	    break;
	} else {
	    p = p->next;
	}
    }

    if (p) {
	return(p->fullSource);
    } else {
	return(NULL);
    }
}

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
    p->icon = NULL;
    p->header = newHeader();
    p->filelist = newStringBuf();
    p->files = -1;  /* -1 means no %files, thus no package */
    p->doc = newStringBuf();
    p->next = NULL;

    return p;
}

void free_packagerec(struct PackageRec *p)
{
    freeHeader(p->header);
    freeStringBuf(p->filelist);
    freeStringBuf(p->doc);
    FREE(p->subname);
    FREE(p->newname);
    FREE(p->icon);
    if (p->next) {
        free_packagerec(p->next);
    }
    free(p);
}

void freeSpec(Spec s)
{
    FREE(s->name);
    FREE(s->specfile);
    freeSources(s);
    freeStringBuf(s->prep);
    freeStringBuf(s->build);
    freeStringBuf(s->install);
    freeStringBuf(s->doc);
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
	if (flags & LP_SUBNAME) {
	    if (! package->subname) {
	        package = package->next;
	        continue;
	    }
	    if (! strcmp(package->subname, name)) {
		break;
	    }
	} else if (flags & LP_NEWNAME) {
	    if (! package->newname) {
	        package = package->next;
	        continue;
	    }
	    if (! strcmp(package->newname, name)) {
		break;
	    }
	} else {
	    /* Base package */
	    if ((! package->newname) && (! package->subname)) {
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
    if (name) {
	if (flags & LP_SUBNAME) {
	    package->subname = strdup(name);
	} else if (flags & LP_NEWNAME) {
	    package->newname = strdup(name);
	}
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

static void generateNamesAndDocScript(Spec s)
{
    struct PackageRec *package;
    char buf[1024];
    char fullname[1024];
    char *name, *version, *release, *packageVersion, *packageRelease, *docs;

    getEntry(s->packages->header, RPMTAG_VERSION, NULL,
	     (void *) &version, NULL);
    getEntry(s->packages->header, RPMTAG_RELEASE, NULL,
	     (void *) &release, NULL);
    
    package = s->packages;
    while (package) {
	if (package->subname) {
	    sprintf(buf, "%s-%s", s->name, package->subname);
	    name = buf;
	} else if (package->newname) {
	    name = package->newname;
	} else {
	    /* Must be the main package */
	    name = s->name;
	}
	addEntry(package->header, RPMTAG_NAME, STRING_TYPE, name, 1);

	/* Handle subpackage version/release overrides */
        if (!getEntry(package->header, RPMTAG_VERSION, NULL,
		      (void *) &packageVersion, NULL)) {
            packageVersion = version;
	}
        if (!getEntry(package->header, RPMTAG_RELEASE, NULL,
		      (void *) &packageRelease, NULL)) {
            packageRelease = release;
	}

	/* Generate the doc script */
	appendStringBuf(s->doc, "DOCDIR=$RPM_ROOT_DIR/$RPM_DOC_DIR/");
	sprintf(fullname, "%s-%s-%s", name, packageVersion, packageRelease);
	appendLineStringBuf(s->doc, fullname);
	appendLineStringBuf(s->doc, "rm -rf $DOCDIR");
	docs = getStringBuf(package->doc);
	appendLineStringBuf(s->doc, docs);
	
	package = package->next;
    }
}

/**********************************************************************/
/*                                                                    */
/* Line reading                                                       */
/*                                                                    */
/**********************************************************************/

static int match_arch(char *s)
{
    char *tok, *arch;
    int sense, match;

    arch = getArchName();
    match = 0;
    
    tok = strtok(s, " \n\t");
    sense = (! strcmp(tok, "%ifarch")) ? 1 : 0;

    while ((tok = strtok(NULL, " \n\t"))) {
	if (! strcmp(tok, arch)) {
	    match |= 1;
	}
    }

    return (sense == match);
}

static int match_os(char *s)
{
    char *tok, *os;
    int sense, match;

    os = getOsName();
    match = 0;
    
    tok = strtok(s, " \n\t");
    sense = (! strcmp(tok, "%ifos")) ? 1 : 0;

    while ((tok = strtok(NULL, " \n\t"))) {
	if (! strcmp(tok, os)) {
	    match |= 1;
	}
    }

    return (sense == match);
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
		error(RPMERR_UNMATCHEDIF, "Unclosed %%if");
	        return RPMERR_UNMATCHEDIF;
	    } else {
	        return 0;
	    }
	}
	if ((! strncmp("%ifarch", line, 7)) ||
	    (! strncmp("%ifnarch", line, 8))) {
	    rl = malloc(sizeof(struct read_level_entry));
	    rl->next = read_level;
	    rl->reading = read_level->reading && match_arch(line);
	    read_level = rl;
	} else if ((! strncmp("%ifos", line, 5)) ||
		   (! strncmp("%ifnos", line, 6))) {
	    rl = malloc(sizeof(struct read_level_entry));
	    rl->next = read_level;
	    rl->reading = read_level->reading && match_os(line);
	    read_level = rl;
	} else if (! strncmp("%else", line, 5)) {
	    if (! read_level->next) {
	        /* Got an else with no %if ! */
		error(RPMERR_UNMATCHEDIF, "Got a %%else with no if");
	        return RPMERR_UNMATCHEDIF;
	    }
	    read_level->reading =
	        read_level->next->reading && ! read_level->reading;
	} else if (! strncmp("%endif", line, 6)) {
	    if (! read_level->next) {
		error(RPMERR_UNMATCHEDIF, "Got a %%endif with no if");
		return RPMERR_UNMATCHEDIF;
	    }
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
    {RPMTAG_URL,          0, "url"},
    {RPMTAG_ROOT,         0, "root"},
    {RPMTAG_SOURCE,       0, "source"},
    {RPMTAG_PATCH,        0, "patch"},
    {RPMTAG_EXCLUDE,      0, "exclude"},
    {RPMTAG_EXCLUSIVE,    0, "exclusive"},
    {RPMTAG_ICON,         0, "icon"},
    {RPMTAG_ICON,         0, "provides"},
    {RPMTAG_ICON,         0, "requires"},
    {0, 0, 0}
};

static int find_preamble_line(char *line, char **s)
{
    struct preamble_line *p = preamble_spec;

    while (p->token && strncasecmp(line, p->token, p->len)) {
        p++;
    }
    if (!p->token) return 0;
    *s = line + p->len;

    /* Unless this is a source or a patch, a ':' better be next */
    if (p->tag != RPMTAG_SOURCE && p->tag != RPMTAG_PATCH) {
	*s += strspn(*s, " \t");
	if (**s != ':') {
	    return 0;
	}
    }
    
    *s += strspn(*s, ": \t");
    return p->tag;
}

/* None of these can be 0 !! */
#define PREAMBLE_PART  1
#define PREP_PART      2
#define BUILD_PART     3
#define INSTALL_PART   4
#define CLEAN_PART     5
#define PREIN_PART     6
#define POSTIN_PART    7
#define PREUN_PART     8
#define POSTUN_PART    9
#define FILES_PART     10
#define CHANGELOG_PART 11

static struct part_rec {
    int part;
    int len;
    char *s;
} part_list[] = {
    {PREAMBLE_PART,  0, "%package"},
    {PREP_PART,      0, "%prep"},
    {BUILD_PART,     0, "%build"},
    {INSTALL_PART,   0, "%install"},
    {CLEAN_PART,     0, "%clean"},
    {PREUN_PART,     0, "%preun"},
    {POSTUN_PART,    0, "%postun"},
    {PREIN_PART,     0, "%pre"},
    {POSTIN_PART,    0, "%post"},
    {FILES_PART,     0, "%files"},
    {CHANGELOG_PART, 0, "%changelog"},
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

static char *chop_line(char *s)
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

static void parseForDocFiles(struct PackageRec *package, char *line)
{
    if (strncmp(line, "%doc", 4)) {
	return;
    }

    line += 4;
    if ((*line != ' ') && (*line != '\t')) {
	return;
    }
    line += strspn(line, " \t\n");
    if ((! *line) || (*line == '/')) {
	return;
    }
    
    appendLineStringBuf(package->doc, "mkdir -p $DOCDIR");
    appendStringBuf(package->doc, "cp -ar ");
    appendStringBuf(package->doc, line);
    appendLineStringBuf(package->doc, " $DOCDIR");
}

/**********************************************************************/
/*                                                                    */
/* Main specfile parsing routine                                      */
/*                                                                    */
/**********************************************************************/

Spec parseSpec(FILE *f, char *specfile)
{
    char buf[LINE_BUF_SIZE]; /* read buffer          */
    char buf2[LINE_BUF_SIZE];
    char *line;              /* "parsed" read buffer */
    
    int x, serial, tag, cur_part, t1;
    int lookupopts;
    StringBuf sb;
    char *s = NULL;
    char *s1;

    struct PackageRec *cur_package = NULL;
    Spec spec = (struct SpecRec *) malloc(sizeof(struct SpecRec));

    spec->name = NULL;
    spec->specfile = strdup(specfile);
    spec->numSources = 0;
    spec->numPatches = 0;
    spec->sources = NULL;
    spec->prep = newStringBuf();
    spec->build = newStringBuf();
    spec->install = newStringBuf();
    spec->doc = newStringBuf();
    spec->clean = newStringBuf();
    spec->packages = NULL;

    sb = newStringBuf();
    reset_spec();         /* Reset the parser */

    cur_part = PREAMBLE_PART;
    while ((x = read_line(f, buf)) > 0) {
	line = buf;
	s = NULL;
        if ((tag = check_part(line, &s))) {
	    message(MESS_DEBUG, "Switching to part: %d\n", tag);
	    t1 = 0;
	    switch (cur_part) {
	      case PREIN_PART:
		t1 = RPMTAG_PREIN;
		break;
	      case POSTIN_PART:
		t1 = RPMTAG_POSTIN;
		break;
	      case PREUN_PART:
		t1 = RPMTAG_PREUN;
		break; 
	      case POSTUN_PART:
		t1 = RPMTAG_POSTUN;
		break;
	      case CHANGELOG_PART:
		t1 = RPMTAG_CHANGELOG;
		break;
	    }
	    if (t1) {
		addEntry(cur_package->header, t1,
			 STRING_TYPE, getStringBuf(sb), 1);
	    }
	    cur_part = tag;
	    truncStringBuf(sb);

	    /* Now switch the current package to s */
	    if (s) {
	        switch (tag) {
		  case PREP_PART:
		  case BUILD_PART:
		  case INSTALL_PART:
		  case CLEAN_PART:
		  case CHANGELOG_PART:
		    error(RPMERR_BADARG, "Tag takes no arguments: %s", s);
		    return NULL;
	        }
	    }

	    /* Handle -n in part tags */
	    lookupopts = 0;
	    if (s) {
		if (!strncmp(s, "-n", 2)) {
		    s += 2;
		    s += strspn(s, ": \t");
		    if (*s == '\0') {
			error(RPMERR_BADARG, "-n takes argument");
			return NULL;
		    }
		    lookupopts = LP_NEWNAME;
		} else {
		    lookupopts = LP_SUBNAME;
		}
	        /* Handle trailing whitespace */
	        s1 = s + strlen(s) - 1;
	        while (*s1 == ' ' || *s1 == '\n' || *s1 == '\t') {
		   s1--;
		}
	        s1++;
	        *s1 = '\0';
	    }

	    switch (tag) {
	      case PREP_PART:
	      case BUILD_PART:
	      case INSTALL_PART:
	      case CLEAN_PART:
	      case CHANGELOG_PART:
	        /* Do not switch parts for these */
	        break;
	      case PREAMBLE_PART:
	        lookupopts |= LP_CREATE | LP_FAIL_EXISTS;
	        /* Fall through */
	      default:
	        /* XXX - should be able to drop the -n in non-%package parts */
	        if (! lookup_package(spec, &cur_package, s, lookupopts)) {
		    error(RPMERR_INTERNAL, "Package lookup failed: %s", s);
		    return NULL;
	        }
	        message(MESS_DEBUG, "Switched to package: %s\n", s);
	    }

	    if (cur_part == FILES_PART) {
		/* set files to 0 (current -1 means no %files, no package */
		cur_package->files = 0;
	    }
	    
	    continue;
        }

	/* Check for implicit "base" package.                        */
        /* That means that the specfile does not start with %package */
	if (! cur_package) {
	    lookupopts = 0;
	    if (cur_part == PREAMBLE_PART) {
		lookupopts = LP_CREATE | LP_FAIL_EXISTS;
	    }
	    if (! lookup_package(spec, &cur_package, NULL, lookupopts)) {
		error(RPMERR_INTERNAL, "Base package lookup failed!");
		return NULL;
	    }
	    message(MESS_DEBUG, "Switched to BASE package\n");
	}
	
        switch (cur_part) {
	  case PREAMBLE_PART:
	    if ((tag = find_preamble_line(line, &s))) {
	        switch (tag) {
		  case RPMTAG_EXCLUDE:
		  case RPMTAG_EXCLUSIVE:
		      sprintf(buf2, "%s %s",
			      (tag == RPMTAG_EXCLUDE) ? "%ifarch" : "%ifnarch",
			      s);
		      if (match_arch(buf2)) {
			  error(RPMERR_BADARCH, "Arch mismatch!");
			  return NULL;
		      }
		      break;
		  case RPMTAG_NAME:
		    s1 = s;
		    while (*s1 && *s1 != ' ' && *s1 != '\t') s1++;
		    *s1 = '\0';
		    if (!spec->name) {
			spec->name = strdup(s);
		    }
		    /* The NAME entries must be generated after */
		    /* the whole spec file is parsed.           */
		    break;
		  case RPMTAG_VERSION:
		  case RPMTAG_RELEASE:
		  case RPMTAG_SUMMARY:
		    s1 = s;
		    while (*s1 && *s1 != ' ' && *s1 != '\t') s1++;
		    *s1 = '\0';
		  case RPMTAG_DISTRIBUTION:
		  case RPMTAG_VENDOR:
		  case RPMTAG_COPYRIGHT:
		  case RPMTAG_PACKAGER:
		  case RPMTAG_GROUP:
		  case RPMTAG_URL:
		    addEntry(cur_package->header, tag, STRING_TYPE, s, 1);
		    break;
		  case RPMTAG_SERIAL:
		    serial = atoi(s);
		    addEntry(cur_package->header, tag, INT32_TYPE, &serial, 1);
		    break;
		  case RPMTAG_DESCRIPTION:
		    /* Special case -- need to handle backslash */
		    truncStringBuf(sb);
		    while (1) {
		        s1 = s + strlen(s) - 1;
			if (*s1 != '\\') {
			    break;
			}
		        *s1 = '\0';
			appendLineStringBuf(sb, s);
			read_line(f, buf);
			s = buf;
		    }
		    appendStringBuf(sb, s);
		    addEntry(cur_package->header, RPMTAG_DESCRIPTION,
			     STRING_TYPE, getStringBuf(sb), 1);
		    break;
		  case RPMTAG_ROOT:
		      /* special case */
		      message(MESS_DEBUG, "Got root: %s\n", s);
		      setVar(RPMVAR_ROOT, s);
		      break;
		  case RPMTAG_ICON:
		      cur_package->icon = strdup(s);
		      break;
		  case RPMTAG_SOURCE:
		  case RPMTAG_PATCH:
		      if (addSource(spec, line)) {
			  return NULL;
		      }
		      break;
		  case RPMTAG_PROVIDES:
		      if (parseProvides(line)) {
		      }
		      break;
		  default:
		      /* message(MESS_DEBUG, "Skipping: %s\n", line); */
		      /* This shouldn't happen? */
		      error(RPMERR_INTERNAL, "Bogus token");
		      return NULL;
		}		
	    } else {
	        /* Not a recognized preamble part */
		s1 = line;
		while (*s1 && *s1 != ' ' && *s1 != '\t') s1++;
		if (*s1) {
		    /*message(MESS_WARNING, "Unknown Field: %s\n", line);*/
		    error(RPMERR_BADSPEC, "Unknown Field: %s\n", line);
		    return NULL;
		}
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
	  case CHANGELOG_PART:
	  case PREIN_PART:
	  case POSTIN_PART:
	  case PREUN_PART:
	  case POSTUN_PART:
	    appendLineStringBuf(sb, line);
	    break;
	  case FILES_PART:
	    cur_package->files++;
	    appendLineStringBuf(cur_package->filelist, line);
	    parseForDocFiles(cur_package, line);
	    break;
	  default:
	    error(RPMERR_INTERNAL, "Bad part");
	    return NULL;
	} /* switch */
    }
    if (x < 0) {
	return NULL;
    }

    generateNamesAndDocScript(spec);
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

    setVar(RPMVAR_ROOT, NULL);

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
