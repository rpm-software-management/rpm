#include <errno.h>
#include <ctype.h>
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
    if ((!strcmp(un.machine, "i686")) ||
	(!strcmp(un.machine, "i586")) ||
	(!strcmp(un.machine, "i486")) ||
	(!strcmp(un.machine, "i386"))) {
	archnum = 1;
	archname = "i386";
    } else if (!strcmp(un.machine, "alpha")) {
	archnum = 2;
	archname = "axp";
    } else if (!strcmp(un.machine, "sparc")) {
       archnum = 3;
       archname = "sparc";
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

int vercmp(char * one, char * two) {
    int num1, num2;
    char oldch1, oldch2;
    char * str1, * str2;
    int rc;
    int isnum;
    
    if (!strcmp(one, two)) return 0;

    str1 = alloca(strlen(one) + 1);
    str2 = alloca(strlen(two) + 1);

    strcpy(str1, one);
    strcpy(str2, two);

    one = str1;
    two = str2;

    while (*one && *two) {
	while (*one && !isalnum(*one)) one++;
	while (*two && !isalnum(*two)) two++;

	str1 = one;
	str2 = two;

	if (isdigit(*str1)) {
	    while (*str1 && isdigit(*str1)) str1++;
	    while (*str2 && isdigit(*str2)) str2++;
	    isnum = 1;
	} else {
	    while (*str1 && isalpha(*str1)) str1++;
	    while (*str2 && isalpha(*str2)) str2++;
	    isnum = 0;
	}
		
	oldch1 = *str1;
	*str1 = '\0';
	oldch2 = *str2;
	*str2 = '\0';

	if (one == str1) return -1;	/* arbitrary */
	if (two == str2) return -1;

	if (isnum) {
	    num1 = atoi(one);
	    num2 = atoi(two);

	    if (num1 < num2) 
		return -1;
	    else if (num1 > num2)
		return 1;
	} else {
	    rc = strcmp(one, two);
	    if (rc) return rc;
	}
	
	*str1 = oldch1;
	one = str1;
	*str2 = oldch2;
	two = str2;
    }

    if ((!*one) && (!*two)) return 0;

    if (!*one) return -1; else return 1;
}
