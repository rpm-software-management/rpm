/*
** stub main for testing FICL under Win32
** 
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef linux
#include <unistd.h>
#endif

#include "ficl.h"

/*
** Ficl interface to _getcwd (Win32)
** Prints the current working directory using the VM's 
** textOut method...
*/
static void ficlGetCWD(FICL_VM *pVM)
{
    char *cp;

   cp = getcwd(NULL, 80);
    vmTextOut(pVM, cp, 1);
    free(cp);
    return;
}

/*
** Ficl interface to _chdir (Win32)
** Gets a newline (or NULL) delimited string from the input
** and feeds it to the Win32 chdir function...
** Example:
**    cd c:\tmp
*/
static void ficlChDir(FICL_VM *pVM)
{
    FICL_STRING *pFS = (FICL_STRING *)pVM->pad;
    vmGetString(pVM, pFS, '\n');
    if (pFS->count > 0)
    {
       int err = chdir(pFS->text);
       if (err)
        {
            vmTextOut(pVM, "Error: path not found", 1);
            vmThrow(pVM, VM_QUIT);
        }
    }
    else
    {
        vmTextOut(pVM, "Warning (chdir): nothing happened", 1);
    }
    return;
}

/*
** Ficl interface to system (ANSI)
** Gets a newline (or NULL) delimited string from the input
** and feeds it to the Win32 system function...
** Example:
**    system del *.*
**    \ ouch!
*/
static void ficlSystem(FICL_VM *pVM)
{
    FICL_STRING *pFS = (FICL_STRING *)pVM->pad;

    vmGetString(pVM, pFS, '\n');
    if (pFS->count > 0)
    {
        int err = system(pFS->text);
        if (err)
        {
            sprintf(pVM->pad, "System call returned %d", err);
            vmTextOut(pVM, pVM->pad, 1);
            vmThrow(pVM, VM_QUIT);
        }
    }
    else
    {
        vmTextOut(pVM, "Warning (system): nothing happened", 1);
    }
    return;
}

/*
** Ficl add-in to load a text file and execute it...
** Cheesy, but illustrative.
** Line oriented... filename is newline (or NULL) delimited.
** Example:
**    load test.ficl
*/
#define nLINEBUF 256
static void ficlLoad(FICL_VM *pVM)
{
    char    cp[nLINEBUF];
    char    filename[nLINEBUF];
    FICL_STRING *pFilename = (FICL_STRING *)filename;
    int     nLine = 0;
    FILE   *fp;
    int     result;
    CELL    id;
    struct stat buf;

    vmGetString(pVM, pFilename, '\n');

    if (pFilename->count <= 0)
    {
        vmTextOut(pVM, "Warning (load): nothing happened", 1);
        return;
    }

    /*
    ** get the file's size and make sure it exists 
    */
    result = stat( pFilename->text, &buf );

    if (result != 0)
    {
        vmTextOut(pVM, "Unable to stat file: ", 0);
        vmTextOut(pVM, pFilename->text, 1);
        vmThrow(pVM, VM_QUIT);
    }

    fp = fopen(pFilename->text, "r");
    if (!fp)
    {
        vmTextOut(pVM, "Unable to open file ", 0);
        vmTextOut(pVM, pFilename->text, 1);
        vmThrow(pVM, VM_QUIT);
    }

    id = pVM->sourceID;
    pVM->sourceID.p = (void *)fp;

    /* feed each line to ficlExec */
    while (fgets(cp, nLINEBUF, fp))
    {
        int len = strlen(cp) - 1;

        nLine++;
        if (len <= 0)
            continue;

        if (cp[len] == '\n')
            cp[len] = '\0';

        result = ficlExec(pVM, cp);
        if (result != VM_OUTOFTEXT)
        {
            pVM->sourceID = id;
            fclose(fp);
            vmThrowErr(pVM, "Error loading file <%s> line %d", pFilename->text, nLine);
            break; 
        }
    }
    /*
    ** Pass an empty line with SOURCE-ID == -1 to flush
    ** any pending REFILLs (as required by FILE wordset)
    */
    pVM->sourceID.i = -1;
    ficlExec(pVM, "");

    pVM->sourceID = id;
    fclose(fp);

    return;
}

/*
** Dump a tab delimited file that summarizes the contents of the
** dictionary hash table by hashcode...
*/
static void spewHash(FICL_VM *pVM)
{
    FICL_HASH *pHash = ficlGetDict()->pForthWords;
    FICL_WORD *pFW;
    FILE *pOut;
    unsigned i;
    unsigned nHash = pHash->size;

    if (!vmGetWordToPad(pVM))
        vmThrow(pVM, VM_OUTOFTEXT);

    pOut = fopen(pVM->pad, "w");
    if (!pOut)
    {
        vmTextOut(pVM, "unable to open file", 1);
        return;
    }

    for (i=0; i < nHash; i++)
    {
        int n = 0;

        pFW = pHash->table[i];
        while (pFW)
        {
            n++;
            pFW = pFW->link;
        }

        fprintf(pOut, "%d\t%d", i, n);

        pFW = pHash->table[i];
        while (pFW)
        {
            fprintf(pOut, "\t%s", pFW->name);
            pFW = pFW->link;
        }

        fprintf(pOut, "\n");
    }

    fclose(pOut);
    return;
}

static void ficlBreak(FICL_VM *pVM)
{
    pVM->state = pVM->state;
    return;
}

static void ficlClock(FICL_VM *pVM)
{
    clock_t now = clock();
    stackPushUNS(pVM->pStack, (UNS32)now);
    return;
}

static void clocksPerSec(FICL_VM *pVM)
{
    stackPushUNS(pVM->pStack, CLOCKS_PER_SEC);
    return;
}


static void execxt(FICL_VM *pVM)
{
    FICL_WORD *pFW;
#if FICL_ROBUST > 1
    vmCheckStack(pVM, 1, 0);
#endif

    pFW = stackPopPtr(pVM->pStack);
    ficlExecXT(pVM, pFW);

    return;
}


static void buildTestInterface(void)
{
    ficlBuild("break",    ficlBreak,    FW_DEFAULT);
    ficlBuild("clock",    ficlClock,    FW_DEFAULT);
    ficlBuild("cd",       ficlChDir,    FW_DEFAULT);
    ficlBuild("execxt",   execxt,       FW_DEFAULT);
    ficlBuild("load",     ficlLoad,     FW_DEFAULT);
    ficlBuild("pwd",      ficlGetCWD,   FW_DEFAULT);
    ficlBuild("system",   ficlSystem,   FW_DEFAULT);
    ficlBuild("spewhash", spewHash,     FW_DEFAULT);
    ficlBuild("clocks/sec", 
                          clocksPerSec, FW_DEFAULT);

    return;
}


static int quiet = 0;

int main(int argc, char **argv)
{
    char in[BUFSIZ], * s;
    FICL_VM *pVM;
    extern char * optarg;
    extern int optind, opterr, optopt;
    int errflg = 0;
    int ret;
    int c;

    while ((c = getopt(argc, argv, "q")) != EOF)
    switch (c) {
    case 'q':
	quiet++;
	break;
    case '?':
    default:
	errflg++;
	break;
    }

    ficlInitSystem(10000);
    buildTestInterface();
    pVM = ficlNewVM();

    if (!quiet)
	ficlExec(pVM, ".ver .( " __DATE__ " ) cr quit");

    for ( ; optind < argc; optind++) {
        sprintf(in, ".( loading %s ) cr load %s\n cr", argv[optind], argv[optind]);
        ficlExec(pVM, in);
    }

    s = in;
    if (!quiet)
	*s++ = '\n';
    *s++ = '\0';
    ret = 0;
    do {
	if (in[0])
	    ret = ficlExec(pVM, in);
    } while (ret != VM_USEREXIT && (s = fgets(in, sizeof(in)-1, stdin)) != NULL);

    ficlTermSystem();

    return 0;
}
