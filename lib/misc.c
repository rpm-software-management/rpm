#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "misc.h"

static void init_arch_os(void);

char ** splitString(char * str, int length, char sep) {
    char * s, * source, * dest;
    char ** list;
    int i;
    int fields;
   
    s = malloc(length + 1);
    
    fields = 1;
    for (source = str, dest = s, i = 0; i < length; i++, source++, dest++) {
	*dest = *source;
	if (*dest == sep) fields++;
    }

    *dest = '\0';

    list = malloc(sizeof(char *) * (fields + 1));

    dest = s;
    list[0] = dest;
    i = 1;
    while (i < fields) {
	if (*dest == sep) {
	    list[i++] = dest + 1;
	    *dest = 0;
	}
	dest++;
    }

    list[i] = NULL;

    return list;
}

void freeSplitString(char ** list) {
    free(list[0]);
    free(list);
}

int exists(char * filespec) {
    struct stat buf;

    if (stat(filespec, &buf)) {
	switch(errno) {
	   case ENOENT:
	   case EINVAL:
		return 0;
	}
    }

    return 1;
}

static int osnum = -1;
static int archnum = -1;
static char *osname = NULL;
static char *archname = NULL;

int getOsNum(void)
{
    init_arch_os();
    return osnum;
}

int getArchNum(void)
{
    init_arch_os();
    return archnum;
}

char *getOsName(void)
{
    init_arch_os();
    return osname;
}

char *getArchName(void)
{
    init_arch_os();
    return archname;
}

static void init_arch_os(void)
{
    struct utsname un;

    if (osnum >= 0) {
	return;
    }

    uname(&un);
    if ((!strcmp(un.machine, "i586")) ||
	(!strcmp(un.machine, "i486")) ||
	(!strcmp(un.machine, "i386"))) {
	archnum = 1;
	archname = "i386";
    } else if (!strcmp(un.machine, "alpha")) {
	archnum = 2;
	archname = "axp";
    } else {
	/* XXX unknown arch - how should we handle this? */
    }

    if (!strcmp(un.sysname, "Linux")) {
	osnum = 1;
	osname = "Linux";
    } else {
	/* XXX unknown os - how should we handle this? */
    }
}
