
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <popt.h>
#include "lxlist.h"

#include <rpm/rpmstring.h>
#include <rpm/argv.h>

/**
 * Extract OS/2 LX dependencies.
 * @param fc		file classifier
 */
static void processFile(const char *fn, int dtype)
{
    char fname[_MAX_FNAME], ext[_MAX_EXT];
    FILE *in;
    long beg = 0;
    LXheader hdr;
    int i;
    word sign;

    in = fopen(fn, "rb");
    if (!in)
        return;

    // Verify signature
    fread(&sign, sizeof(sign), 1, in);
    if (sign != 0x4D5A && sign != 0x5A4D	/* MZ or ZM !!! Yes this is also valid */
            && sign != 0x584C)    { /* LX !!! Yes this is also valid */
        // printf("This is not an executable file");
        fclose(in);
        return;
    }

    // get LX header
    fseek(in, OffsetToLX, SEEK_SET);
    fread(&beg, sizeof(beg), 1, in);
    fseek(in, beg, SEEK_SET);
    fread(&hdr, sizeof(hdr), 1, in);
    if (hdr._L != 'L' || hdr._X != 'X') {
        // printf("Unknow format '%c%c'", hdr._L, hdr._X);
        fclose(in);
        return;
    }

    // scan header
    fseek(in, beg + hdr.ImportModuleTblOff, SEEK_SET);
    for (i = 0; i < (hdr.ImportProcTblOff - hdr.ImportModuleTblOff); i++) {
        int j;
        char *name;
        j = getc(in);
        if (j > 0) {
            char *ptr;
            name = (char *) malloc(j + 1 + 4);
            if (name) {
                ptr = name;
                for (; j > 0; j--, i++)
                    *ptr++ = getc(in);
                *ptr = 0;
                strlwr( name);
                strcat( name, ".dll");
                if (dtype == -1)
                    fprintf(stdout, "%s\n", name);
            }
            free(name);
        }
    }
    fclose(in);

    // add provides for DLL
    if (dtype == 0) {
        _splitpath( fn, NULL, NULL, fname, ext);
        strlwr( fname);
        strlwr( ext);
        if (strcmp( ext, ".dll") == 0) {
            char fullname[_MAX_PATH];
            strcpy( fullname, fname);
            strcat( fullname, ext);
            fprintf(stdout, "%s\n", fullname);
        }
    }

    return;
}

int main(int argc, char *argv[])
{
    int provides = 0;
    int requires = 0;
    poptContext optCon;

    struct poptOption opts[] = {
	{ "provides", 'P', POPT_ARG_VAL, &provides, -1, NULL, NULL },
	{ "requires", 'R', POPT_ARG_VAL, &requires, -1, NULL, NULL },
	POPT_AUTOHELP
	POPT_TABLEEND
};

    optCon = poptGetContext(argv[0], argc, (const char **) argv, opts, 0);
    if (argc < 2 || poptGetNextOpt(optCon) == 0) {
        poptPrintUsage(optCon, stderr, 0);
        exit(EXIT_FAILURE);
    }

    /* Normally our data comes from stdin, but permit args too */
    if (poptPeekArg(optCon)) {
        const char *fn;
        while ((fn = poptGetArg(optCon)) != NULL) {
            (void) processFile(fn, requires);
        }
    } else {
        char fn[BUFSIZ];
        while (fgets(fn, sizeof(fn), stdin) != NULL) {
            fn[strlen(fn)-1] = '\0';
            (void) processFile(fn, requires);
        }
    }

    poptFreeContext(optCon);
    return 0;
}
