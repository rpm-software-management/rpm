/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * build.c - routines for building a package
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "build.h"
#include "header.h"
#include "spec.h"
#include "specP.h"
#include "rpmerr.h"
#include "messages.h"
#include "stringbuf.h"

struct Script {
    char *name;
    FILE *file;
};

struct Script *openScript(void);
void writeScript(struct Script *script, char *s);
int execScript(struct Script *script);
void closeScript(struct Script *script);

struct Script *openScript(void)
{
    struct Script *script = malloc(sizeof(struct Script));
    
    script->name = tempnam("/usr/tmp", "rpmbuild");
    script->file = fopen(script->name, "w");
    fprintf(script->file, "#!/bin/sh\n");

    return script;
}

void writeScript(struct Script *script, char *s)
{
    fprintf(script->file, "%s", s);
}

int execScript(struct Script *script)
{
    int pid;
    int status;
    
    fclose(script->file);
    chmod(script->name, 0700);
    if (!(pid = fork())) {
	execl(script->name, script->name, NULL);
	error(RPMERR_SCRIPT, "Exec failed");
	exit(RPMERR_SCRIPT);
    }
    wait(&status);
    if (! WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_SCRIPT, "Bad exit status");
	exit(RPMERR_SCRIPT);
    }
    return 0;
}

void closeScript(struct Script *script)
{
    fclose(script->file);
    unlink(script->name);
    free(script->name);
    free(script);
}

int execPrep(Spec s)
{
    struct Script *script;

    script = openScript();
    writeScript(script, getStringBuf(s->prep));
    execScript(script);
    closeScript(script);
    return 0;
}

int execBuild(Spec s)
{
    struct Script *script;

    script = openScript();
    writeScript(script, getStringBuf(s->build));
    execScript(script);
    closeScript(script);
    return 0;
}

int execInstall(Spec s)
{
    struct Script *script;

    script = openScript();
    writeScript(script, getStringBuf(s->install));
    execScript(script);
    closeScript(script);
    return 0;
}

int execClean(Spec s)
{
    struct Script *script;

    script = openScript();
    writeScript(script, getStringBuf(s->clean));
    execScript(script);
    closeScript(script);
    return 0;
}

int verifyList(Spec s)
{
}

int packageBinaries(Spec s)
{
}

int packageSource(Spec s)
{
}
