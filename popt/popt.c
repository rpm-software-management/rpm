#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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
    char * appName;
    struct poptAlias * aliases;
    int numAliases;
};

poptContext poptGetContext(char * name ,int argc, char ** argv, 
			   struct poptOption * options, int flags) {
    poptContext con = malloc(sizeof(*con));

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
    
    if (!name)
	con->appName = NULL;
    else
	con->appName = strcpy(malloc(strlen(name) + 1), name);

    return con;
}

void poptResetContext(poptContext con) {
    con->os = con->optionStack;
    con->os->currAlias = NULL;
    con->os->next = 1;			/* skip argv[0] */

    con->numLeftovers = 0;
    con->nextLeftover = 0;
    con->restLeftover = 0;
    con->nextCharArg = NULL;
    con->nextArg = NULL;
}

/* returns 'val' element, -1 on last item, POPT_ERROR_* on error */
int poptGetNextOpt(poptContext con) {
    char * optString, * chptr, * localOptString;
    char * longArg = NULL;
    char * origOptString;
    struct poptOption * opt = NULL;
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

		opt = con->options;
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

	    opt = con->options;
	    while ((opt->longName || opt->shortName) && 
		    *origOptString != opt->shortName) opt++;
	    if (!opt->longName && !opt->shortName) return POPT_ERROR_BADOPT;

	    origOptString++;
	    if (*origOptString)
		con->nextCharArg = origOptString;
	}

	if (opt->arg && opt->argInfo == POPT_ARG_NONE) 
	    *((int *)opt->arg) = 1;
	else if (opt->argInfo != POPT_ARG_NONE) {
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

	    if (opt->arg) {
		switch (opt->argInfo) {
		  case POPT_ARG_STRING:
		    *((char **) opt->arg) = con->nextArg;
		    break;
		  default:
		    printf("option type not implemented in popt\n");
		    exit(1);
		}
	    }
	}

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
    if (con->appName) free(con->appName);
    free(con);
}

int poptAddAlias(poptContext con, struct poptAlias newAlias) {
    int aliasNum = con->numAliases++;
    struct poptAlias * alias;

    con->aliases = realloc(con->aliases, sizeof(newAlias) * con->numAliases);
    alias = con->aliases + aliasNum;
    
    *alias = newAlias;
    alias->longName = strcpy(malloc(strlen(alias->longName) + 1), 
				alias->longName);

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
	if (quote == *src) {
	    quote = '\0';
	} else if (quote) {
	    if (*src == '\\') {
		src++;
		if (!*src) {
		    free(argv);
		    return POPT_ERROR_BADQUOTE;
		}
		if (*src != quote) *buf++ = '\\';
	    }
	    *buf++ = *src;
	} else if (isspace(*src)) {
	    if (*argv[argc]) {
		buf++, argc++;
		if (argc == argvAlloced) {
		    argvAlloced += 5;
		    argv = realloc(argv, sizeof(*argv) * argvAlloced);
		}
		argv[argc] = buf;
	    }
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

static void configLine(poptContext con, char * line) {
    int nameLength = strlen(con->appName);
    char * opt;
    struct poptAlias alias;
    
    if (strncmp(line, con->appName, nameLength)) return;
    line += nameLength;
    if (!*line || !isspace(*line)) return;
    while (*line && isspace(*line)) line++;

    if (!strncmp(line, "alias", 5)) {
	line += 5;
	if (!*line || !isspace(*line)) return;
	while (*line && isspace(*line)) line++;
	if (!*line) return;

	opt = line;
	while (*line && !isspace(*line)) line++;
	if (!*line) return;
	*line++ = '\0';
	while (*line && isspace(*line)) line++;
	if (!*line) return;

	if (poptParseArgvString(line, &alias.argc, &alias.argv)) return;
	alias.longName = opt;
	
	poptAddAlias(con, alias);
    }
}

int poptReadConfigFile(poptContext con, char * fn) {
    char * file, * chptr, * end;
    char * buf, * dst;
    int fd, rc;
    int fileLength;

    fd = open(fn, O_RDONLY);
    if (fd < 0) {
	if (errno == ENOENT)
	    return 0;
	else 
	    return POPT_ERROR_ERRNO;
    }

    fileLength = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, 0);

    file = mmap(NULL, fileLength, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == (void *) -1) {
	rc = errno;
	close(fd);
	errno = rc;
	return POPT_ERROR_ERRNO;
    }
    close(fd);

    dst = buf = alloca(fileLength + 1);

    chptr = file;
    end = (file + fileLength);
    while (chptr < end) {
	switch (*chptr) {
	  case '\n':
	    *dst = '\0';
	    dst = buf;
	    while (*dst && isspace(*dst)) dst++;
	    if (*dst && *dst != '#') {
		configLine(con, dst);
	    }
	    chptr++;
	    break;
	  case '\\':
	    *dst++ = *chptr++;
	    if (chptr < end) {
		if (*chptr == '\n') 
		    *(dst - 1) = *chptr++;
		else
		    *dst++ = *chptr++;
	    }
	    break;
	  default:
	    *dst++ = *chptr++;
	}
    }

    return 0;
}

int poptReadDefaultConfig(poptContext con, int useEnv) {
    char * envName, * envValue;
    char * fn, * home, * chptr;
    int rc;
    struct poptAlias alias;

    if (!con->appName) return 0;

    rc = poptReadConfigFile(con, "/etc/popt");
    if (rc) return rc;
    if ((home = getenv("HOME"))) {
	fn = alloca(strlen(home) + 20);
	sprintf(fn, "%s/.popt", home);
	rc = poptReadConfigFile(con, fn);
	if (rc) return rc;
    }

    envName = alloca(strlen(con->appName) + 20);
    strcpy(envName, con->appName);
    chptr = envName;
    while (*chptr) {
	*chptr = toupper(*chptr);
	chptr++;
    }
    strcat(envName, "_POPT_ALIASES");

    if (useEnv && (envValue = getenv(envName))) {
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

    return 0;
}
