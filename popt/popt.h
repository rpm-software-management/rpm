#ifndef H_POPT
#define H_POPT

#define POPT_OPTION_DEPTH	10

#define POPT_ARG_NONE		0
#define POPT_ARG_YES		1

#define POPT_ERROR_NOARG	-10
#define POPT_ERROR_BADOPT	-11
#define POPT_ERROR_BADALIAS	-12
#define POPT_ERROR_OPTSTOODEEP	-13
#define POPT_ERROR_UNEXPARG	-14
#define POPT_ERROR_BADQUOTE	-15	/* only from poptParseArgString() */

struct poptOption {
    const char * longName;	/* may be NULL */
    char shortName;		/* may be '\0' */
    int takesArg;
    int *flag;			/* may be NULL */
    int val;			/* 0 means don't return, just update flag */
};

struct poptAlias {
    const char * longName;
    int argc;
    char ** argv;
};

typedef struct poptContext_s * poptContext;

poptContext poptGetContext(char * name, int argc, char ** argv, 
			   struct poptOption * options, int flags);

/* returns 'val' element, -1 on last item, POPT_ERROR_* on error */
int poptGetNextOpt(poptContext con);
/* returns NULL if no argument is available */
char * poptGetOptArg(poptContext con);
/* returns NULL if no more options are available */
char * poptGetArg(poptContext con);
char * poptPeekArg(poptContext con);
char ** poptGetArgs(poptContext con);
void poptFreeContext(poptContext con);
int poptAddAlias(poptContext con, struct poptAlias alias);
/* argv should be freed -- this allows ', ", and \ quoting, but ' is treated
   the same as " and both may include \ quotes */
int poptParseArgvString(char * s, int * argcPtr, char *** argvPtr);

#endif
