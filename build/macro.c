/* macro.c - %macro handling */
#include "miscfn.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "macro.h"
#include "misc.h"

#ifdef DEBUG_MACROS
#define rpmError fprintf
#define RPMERR_BADSPEC stderr
#else
#include "lib/rpmlib.h"
#endif

static void dumpTable(struct MacroContext *mc);
static void expandMacroTable(struct MacroContext *mc);
static int compareMacros(const void *ap, const void *bp);
static struct MacroEntry *findEntry(struct MacroContext *mc, char *name);
static int handleDefine(struct MacroContext *mc, char *buf);
static int parseMacro(char *p, char **macro, char **next);

/* This should be a hash table, but I doubt anyone would ever */
/* notice the increase is speed.                              */

#define MACRO_CHUNK_SIZE 16

/*************************************************************************/
/*                                                                       */
/* Parsing routines                                                      */
/*                                                                       */
/*************************************************************************/

int expandMacros(struct MacroContext *mc, char *buf)
{
    char bufA[1024];
    char *copyTo, *copyFrom;
    char *name, *rest, *first;
    struct MacroEntry *p;
    
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
	    if (copyFrom == buf && !strcmp(name, "define")) {
		if (handleDefine(mc, rest)) {
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
		p = findEntry(mc, name);
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
	rpmError(RPMERR_BADSPEC, "Empty macro name");
	return 2;
    }
    
    if (*p == '{') {
	*next = strchr(p, '}');
	if (! *next) {
	    /* unterminated */
	    rpmError(RPMERR_BADSPEC, "Unterminated {: %s", p);
	    return 1;
	}
	**next = '\0';
	*macro = strtok(p+1, " \n\t");
	if (! *macro) {
	    /* empty macro name */
	    rpmError(RPMERR_BADSPEC, "Empty macro name");
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
	rpmError(RPMERR_BADSPEC, "Illegal %% syntax: %s", p);
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

static int handleDefine(struct MacroContext *mc, char *buf)
{
    char *last, *name, *expansion;

    /* get the name */

    name = buf;
    while (*name && isspace(*name)) {
	name++;
    }
    if (! *name) {
	/* missing macro name */
	rpmError(RPMERR_BADSPEC, "Unfinished %%define");
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

    expandMacros(mc, expansion);
    addMacro(mc, name, expansion);

    return 0;
}

/*************************************************************************/
/*                                                                       */
/* Table handling routines                                               */
/*                                                                       */
/*************************************************************************/

void initMacros(struct MacroContext *mc)
{
    mc->macrosAllocated = 0;
    mc->firstFree = 0;
    mc->macroTable = NULL;
    expandMacroTable(mc);

}

void freeMacros(struct MacroContext *mc)
{
    int i;
    
    for (i = 0; i < mc->firstFree; i++) {
	FREE(mc->macroTable[i].name);
	FREE(mc->macroTable[i].expansion);
    }
    FREE(mc->macroTable);
}

void addMacro(struct MacroContext *mc, char *name, char *expansion)
{
    struct MacroEntry *p;

    p = findEntry(mc, name);
    if (p) {
	free(p->expansion);
	p->expansion = strdup(expansion);
	return;
    }
    
    if (mc->firstFree == mc->macrosAllocated) {
	expandMacroTable(mc);
    }

    p = mc->macroTable + mc->firstFree++;
    p->name = strdup(name);
    p->expansion = strdup(expansion);

    qsort(mc->macroTable, mc->firstFree, sizeof(*(mc->macroTable)),
	  compareMacros);
}

static struct MacroEntry *findEntry(struct MacroContext *mc, char *name)
{
    struct MacroEntry key;

    if (! mc->firstFree) {
	return NULL;
    }
    
    key.name = name;
    return bsearch(&key, mc->macroTable, mc->firstFree,
		   sizeof(*(mc->macroTable)), compareMacros);
}

static int compareMacros(const void *ap, const void *bp)
{
    return strcmp(((struct MacroEntry *)ap)->name,
		  ((struct MacroEntry *)bp)->name);
}

static void expandMacroTable(struct MacroContext *mc)
{
    mc->macrosAllocated += MACRO_CHUNK_SIZE;
    if (mc->macroTable) {
	mc->macroTable = realloc(mc->macroTable, sizeof(*(mc->macroTable)) *
				 mc->macrosAllocated);
    } else {
	mc->macroTable = malloc(sizeof(*(mc->macroTable)) *
				mc->macrosAllocated);
    }
}

/***********************************************************************/

static void dumpTable(struct MacroContext *mc)
{
    int i;
    
    for (i = 0; i < mc->firstFree; i++) {
	printf("%s->%s.\n", mc->macroTable[i].name,
	       mc->macroTable[i].expansion);
    }
}

#ifdef DEBUG_MACROS
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

