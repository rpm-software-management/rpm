/* macro.c - %macro handling */
#include "miscfn.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "macro.h"

#ifdef DEBUG
#include <stdio.h>
#define rpmError fprintf
#define RPMERR_BADSPEC stderr
static void dumpTable(void);
#else
#include "rpmlib.h"
#endif

static void expandMacroTable(void);
static int compareMacros(const void *ap, const void *bp);
static struct macroEntry *findEntry(char *name);
static int handleDefine(char *buf);
static int parseMacro(char *p, char **macro, char **next);

/* This should be a hash table, but I doubt anyone will ever notice */

#define MACRO_CHUNK_SIZE 16

struct macroEntry {
    char *name;
    char *expansion;
};

static struct macroEntry *macroTable = NULL;
static int macrosAllocated = 0;
static int firstFree = 0;

/*************************************************************************/
/*                                                                       */
/* Parsing routines                                                      */
/*                                                                       */
/*************************************************************************/

int expandMacros(char *buf)
{
    char bufA[1024];
    char *copyTo, *copyFrom;
    char *name, *rest;
    struct macroEntry *p;
    
    if (! buf) {
	return 0;
    }
    
    copyFrom = buf;
    copyTo = bufA;

    while (*copyFrom) {
	if (*copyFrom != '%') {
	    *copyTo++ = *copyFrom++;
	} else {
	    if (parseMacro(copyFrom+1, &name, &rest)) {
		return 1;
	    }
	    if (!strcmp(name, "define")) {
		if (handleDefine(rest)) {
		    return 1;
		}
		/* result is empty */
		*buf = '\0';
		return 0;
	    }
	    if (!strcmp(name, "%")) {
		*copyTo++ = '%';
		copyFrom = rest;
	    } else {
		/* a real live macro! */
		p = findEntry(name);
		if (! p) {
		    /* undefined - just leave it */
		    *copyTo++ = '%';
		    copyFrom = name;
		} else {
		    copyFrom = p->expansion;
		}
		while (*copyFrom) {
		    *copyTo++ = *copyFrom++;
		}
		copyFrom = rest;
	    }
	}
    }
    *copyTo = '\0';
    strcpy(buf, bufA);
    return 0;
}

static int parseMacro(char *p, char **macro, char **next)
{
    /* This static var is gross, but we can get away with it */
    /* because the result is used immediately, even when we  */
    /* are recursing.                                        */

    static char macroBuf[1024];
    char save;
    
    /* Find end of macro, handling %{...} construct */

    if (! p) {
	/* empty macro name */
	rpmError(RPMERR_BADSPEC, "Empty macro name\n");
	return 2;
    }
    
    if (*p == '{') {
	*next = strchr(p, '}');
	if (! *next) {
	    /* unterminated */
	    rpmError(RPMERR_BADSPEC, "Unterminated {: %s\n", p);
	    return 1;
	}
	**next = '\0';
	*macro = strtok(p+1, " \n\t");
	if (! *macro) {
	    /* empty macro name */
	    rpmError(RPMERR_BADSPEC, "Empty macro name\n");
	    return 2;
	}
	(*next)++;
	return 0;
    }

    if (*p == '%') {
	*next = p + 1;
	*macro = "%";
	return 0;
    }

    if (isspace(*p) || ! *p) {
	/* illegal % syntax */
	rpmError(RPMERR_BADSPEC, "Illegal %% syntax: %s\n", p);
	return 3;
    }

    *next = *macro = p;
    while (**next && (isalnum(**next) || **next == '_')) {
	(*next)++;
    }
    if (! **next) {
	return 0;
    }
    save = **next;
    **next = '\0';
    strcpy(macroBuf, *macro);
    **next = save;
    *macro = macroBuf;
    return 0;
}

static int handleDefine(char *buf)
{
    char *last, *name, *expansion;

    /* get the name */

    name = buf;
    while (*name && isspace(*name)) {
	name++;
    }
    if (! *name) {
	/* missing macro name */
	rpmError(RPMERR_BADSPEC, "Unfinished %%define\n");
	return 1;
    }
    expansion = name;
    while (*expansion && !isspace(*expansion)) {
	expansion++;
    }
    if (*expansion) {
	*expansion++ = '\0';
    }
    
    /* get the expansion */
    
    while (*expansion && isspace(*expansion)) {
	expansion++;
    }
    if (*expansion) {
	/* strip blanks from end */
	last = expansion + strlen(expansion) - 1;
	while (isspace(*last)) {
	    *last-- = '\0';
	}
    }

    expandMacros(expansion);
    addMacro(name, expansion);

    return 0;
}

#ifdef DEBUG
static void dumpTable()
{
    int i;
    
    for (i = 0; i < firstFree; i++) {
	printf("%s->%s.\n", macroTable[i].name,
	       macroTable[i].expansion);
    }
}

void main(void)
{
    char buf[1024];
    int x;

    while(gets(buf)) {
	x = expandMacros(buf);
	printf("%d->%s<-\n", x, buf);
    }
}
#endif

/*************************************************************************/
/*                                                                       */
/* Table handling routines                                               */
/*                                                                       */
/*************************************************************************/

void resetMacros(void)
{
    int i;
    
    if (! macrosAllocated) {
	expandMacroTable();
	return;
    }

    for (i = 0; i < firstFree; i++) {
	free(macroTable[i].name);
	free(macroTable[i].expansion);
    }
    firstFree = 0;
}

void addMacro(char *name, char *expansion)
{
    struct macroEntry *p;

    p = findEntry(name);
    if (p) {
	free(p->expansion);
	p->expansion = strdup(expansion);
	return;
    }
    
    if (firstFree == macrosAllocated) {
	expandMacroTable();
    }

    p = macroTable + firstFree++;
    p->name = strdup(name);
    p->expansion = strdup(expansion);

    qsort(macroTable, firstFree, sizeof(*macroTable), compareMacros);
}

static struct macroEntry *findEntry(char *name)
{
    struct macroEntry key;

    if (! firstFree) {
	return NULL;
    }
    
    key.name = name;
    return bsearch(&key, macroTable, firstFree,
		   sizeof(*macroTable), compareMacros);
}

static int compareMacros(const void *ap, const void *bp)
{
    return strcmp(((struct macroEntry *)ap)->name,
		  ((struct macroEntry *)bp)->name);
}

static void expandMacroTable()
{
    macrosAllocated += MACRO_CHUNK_SIZE;
    if (! macrosAllocated) {
	macroTable = malloc(sizeof(*macroTable) * macrosAllocated);
	firstFree = 0;
    } else {
	macroTable = realloc(macroTable,
			     sizeof(*macroTable) * macrosAllocated);
    }
}
