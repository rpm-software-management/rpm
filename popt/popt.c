#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "popt.h"

struct optionStackEntry {
    int argc;
    char ** argv;
    int next;
    struct poptAlias * currAlias;
};

struct poptContext_s {
    struct optionStackEntry optionStack[POPT_OPTION_DEPTH], * os;
    char ** leftovers;
    int numLeftovers;
    int nextLeftover;
    struct poptOption * options;
    int restLeftover;
    char * nextArg;
    char * nextCharArg;
    int aliasLocation;
    struct poptAlias * aliases;
    int numAliases;
};

poptContext poptGetContext(char * name ,int argc, char ** argv, 
			   struct poptOption * options, int flags) {
    poptContext con = malloc(sizeof(*con));
    char * envName, * envValue, * chptr;
    struct poptAlias alias;

    con->os = con->optionStack;
    con->os->argc = argc;
    con->os->argv = argv;
    con->os->currAlias = NULL;
    con->os->next = 1;			/* skip argv[0] */

    con->leftovers = malloc(sizeof(char *) * argc);
    con->numLeftovers = 0;
    con->nextLeftover = 0;
    con->restLeftover = 0;
    con->options = options;
    con->nextCharArg = NULL;
    con->nextArg = NULL;
    con->aliases = NULL;
    con->numAliases = 0;
    con->aliasLocation = 0;

    if (!name) return con;

    envName = alloca(strlen(name) + 20);
    strcpy(envName, name);
    strcat(envName, "_POPT_ALIASES");

    if ((envValue = getenv(envName))) {
	envValue = strcpy(alloca(strlen(envValue) + 1), envValue);

	while (envValue && *envValue) {
	    chptr = strchr(envValue, '=');
	    if (!chptr) {
		envValue = strchr(envValue, '\n');
		if (envValue) envValue++;
		continue;
	    }

	    *chptr = '\0';
	    alias.longName = envValue;
	    envValue = chptr + 1;
	    chptr = strchr(envValue, '\n');
	    if (chptr) *chptr = '\0';

	    poptParseArgvString(envValue, &alias.argc, &alias.argv);
	    poptAddAlias(con, alias);

	    if (chptr)
		envValue = chptr + 1;
	    else
		envValue = NULL;
	}
    }

    return con;
}

/* returns 'val' element, -1 on last item, POPT_ERROR_* on error */
int poptGetNextOpt(poptContext con) {
    char * optString, * chptr, * localOptString;
    char * longArg = NULL;
    char * origOptString;
    struct poptOption * opt = con->options;
    int done = 0;
    int i;

    while (!done) {
	if (!con->nextCharArg) {
	    while (con->os->next == con->os->argc && con->os > con->optionStack)
		con->os--;
	    if (con->os->next == con->os->argc)
		return -1;
		
	    origOptString = con->os->argv[con->os->next++];

	    if (con->restLeftover || *origOptString != '-') {
		con->leftovers[con->numLeftovers++] = origOptString;
		continue;
	    }

	    /* Make a copy we can hack at */
	    localOptString = optString = 
			strcpy(alloca(strlen(origOptString) + 1), 
			origOptString);

	    if (!optString[0])
		return POPT_ERROR_BADOPT;

	    if (optString[1] == '-' && !optString[2]) {
		con->restLeftover = 1;
		continue;
	    } else if (optString[1] == '-') {
		optString += 2;

		if (!con->os->currAlias ||
		    strcmp(con->os->currAlias->longName, optString)) {
		    i = 0;
		    while (i < con->numAliases &&
			strcmp(con->aliases[i].longName, optString)) i++;

		    if (i < con->numAliases) {
			if ((con->os - con->optionStack) == POPT_OPTION_DEPTH)
			    return POPT_ERROR_OPTSTOODEEP;

			con->os++;
			con->os->next = 0;
			con->os->currAlias = con->aliases + i;
			con->os->argc = con->os->currAlias->argc;
			con->os->argv = con->os->currAlias->argv;
			continue;
		    }
		}

		chptr = optString;
		while (*chptr && *chptr != '=') chptr++;
		if (*chptr == '=') {
		    longArg = origOptString + (chptr - localOptString);
		    *chptr = '\0';
		}

		while (opt->longName || opt->shortName) {
		    if (opt->longName && !strcmp(optString, opt->longName))
			break;
		    opt++;
		}

		if (!opt->longName && !opt->shortName) return POPT_ERROR_BADOPT;
	    } else 
		con->nextCharArg = origOptString + 1;
	}

	if (con->nextCharArg) {
	    origOptString = con->nextCharArg;

	    con->nextCharArg = NULL;

	    while ((opt->longName || opt->shortName) && 
		    *origOptString != opt->shortName) opt++;
	    if (!opt->longName && !opt->shortName) return POPT_ERROR_BADOPT;

	    origOptString++;
	    if (*origOptString)
		con->nextCharArg = origOptString;
	}

	if (opt->takesArg == POPT_ARG_YES) {
	    if (longArg) {
		con->nextArg = longArg;
	    } else if (con->nextCharArg) {
		con->nextArg = con->nextCharArg;
		con->nextCharArg = NULL;
	    } else { 
		while (con->os->next == con->os->argc && 
		       con->os > con->optionStack)
		    con->os--;
		if (con->os->next == con->os->argc)
		    return POPT_ERROR_NOARG;

		con->nextArg = con->os->argv[con->os->next++];
	    }
	}

	if (opt->flag) *opt->flag = 1;

	if (opt->val) done = 1;
    }

    return opt->val;
}

char * poptGetOptArg(poptContext con) {
    char * ret = con->nextArg;
    con->nextArg = NULL;
    return ret;
}

char * poptGetArg(poptContext con) {
    if (con->numLeftovers == con->nextLeftover) return NULL;
    return (con->leftovers[con->nextLeftover++]);
}

char * poptPeekArg(poptContext con) {
    if (con->numLeftovers == con->nextLeftover) return NULL;
    return (con->leftovers[con->nextLeftover]);
}

char ** poptGetArgs(poptContext con) {
    if (con->numLeftovers == con->nextLeftover) return NULL;
    return (con->leftovers + con->nextLeftover);
}

void poptFreeContext(poptContext con) {
    free(con->leftovers);
    free(con);
}

int poptAddAlias(poptContext con, struct poptAlias newAlias) {
    int aliasNum = con->numAliases++;
    struct poptAlias * alias;

    con->aliases = realloc(con->aliases, sizeof(newAlias) * con->numAliases);
    alias = con->aliases + aliasNum;
    
    *alias = newAlias;

    return 0;
}

int poptParseArgvString(char * s, int * argcPtr, char *** argvPtr) {
    char * buf = strcpy(alloca(strlen(s) + 1), s);
    char * bufStart = buf;
    char * src, * dst;
    char quote = '\0';
    int argvAlloced = 5;
    char ** argv = malloc(sizeof(*argv) * argvAlloced);
    char ** argv2;
    int argc = 0;
    int i;

    src = s;
    dst = buf;
    argv[argc] = buf;

    memset(buf, '\0', strlen(s) + 1);

    while (*src) {
	if (isspace(*src)) {
	    if (*argv[argc]) {
		buf++, argc++;
		if (argc == argvAlloced) {
		    argvAlloced += 5;
		    argv = realloc(argv, sizeof(*argv) * argvAlloced);
		}
		argv[argc] = buf;
	    }
	} else if (quote == *src) {
	    quote = '\0';
	} else if (quote) {
	    if (*src == '\\') {
		src++;
		if (!*src) {
		    free(argv);
		    return POPT_ERROR_BADQUOTE;
		}
	    }
	    *buf++ = *src;
	} else switch (*src) {
	  case '"':
	  case '\'':
	    quote = *src;
	    break;
	  case '\\':
	    src++;
	    if (!*src) {
		free(argv);
		return POPT_ERROR_BADQUOTE;
	    }
	    /* fallthrough */
	  default:
	    *buf++ = *src;
	}

	src++;
    }

    if (strlen(argv[argc])) {
	argc++, buf++;
    }

    argv2 = (void * )dst = malloc(argc * sizeof(*argv) + (buf - bufStart));
    dst += argc * sizeof(*argv);
    memcpy(argv2, argv, argc * sizeof(*argv));
    memcpy(dst, bufStart, buf - bufStart);

    for (i = 0; i < argc; i++) {
	argv2[i] = dst + (argv[i] - bufStart);
    }

    free(argv);

    *argvPtr = argv2;
    *argcPtr = argc;

    return 0;
}
