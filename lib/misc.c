#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <string.h>

#include "misc.h"
#include "rpmerr.h"
#include "messages.h"

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

#define FAIL_IF_NOT_INIT \
{\
    if (osnum < 0) {\
	error(RPMERR_INTERNAL, "Internal error: Arch/OS not initialized!");\
        error(RPMERR_INTERNAL, "Arch: %d\nOS: %d", archnum, osnum);\
	exit(1);\
    }\
}

int getOsNum(void)
{
    FAIL_IF_NOT_INIT;
    return osnum;
}

int getArchNum(void)
{
    FAIL_IF_NOT_INIT;
    return archnum;
}

char *getOsName(void)
{
    FAIL_IF_NOT_INIT;
    return osname;
}

char *getArchName(void)
{
    FAIL_IF_NOT_INIT;
    return archname;
}

void initArchOs(char *arch, char *os)
{
    struct utsname un;

    uname(&un);
    
    if (! arch) {
	arch = un.machine;
    }
    if (! os) {
	os = un.sysname;
    }

    if ((!strcmp(arch, "osfmach3_i986")) ||
	(!strcmp(arch, "osfmach3_i886")) ||
	(!strcmp(arch, "osfmach3_i786")) ||
	(!strcmp(arch, "osfmach3_i686")) ||
	(!strcmp(arch, "osfmach3_i586")) ||
	(!strcmp(arch, "osfmach3_i486")) ||
	(!strcmp(arch, "osfmach3_i386"))) {
	archnum = 1;
	archname = strdup(arch + 9);
    } else if ((!strcmp(arch, "i986")) ||
	       (!strcmp(arch, "i886")) ||
	       (!strcmp(arch, "i786")) ||
	       (!strcmp(arch, "i686")) ||
	       (!strcmp(arch, "i586")) ||
	       (!strcmp(arch, "i486")) ||
	       (!strcmp(arch, "i386"))) {
	archnum = 1;
	archname = strdup(arch);
    } else if (!strcmp(arch, "alpha")) {
	archnum = 2;
	archname = strdup("axp");
    } else if ((!strcmp(arch, "sparc")) ||
	       (!strncmp(arch, "sun4", 4))) {
	archnum = 3;
	archname = strdup("sparc");
    } else if (!strcmp(arch, "mips")) {
	/* This is just a place holder for MIPS */
       archnum = 4;
       archname = strdup("mips");
    } else if ((!strcmp(arch, "osfmach3_ppc")) ||
	       (!strcmp(arch, "ppc"))) {
       archnum = 5;
       archname = strdup("ppc");
    } else if ((!strncmp(arch, "68000", 5))) {
	/* This is just a place holder for 68k */
	archnum = 6;
	archname = strdup("68k");
    } else if ((!strncmp(arch, "IP", 2))) {
	archnum = 7;
	archname = strdup("sgi");
    } else {
	/* unknown arch */
	message(MESS_WARNING, "Unknown architecture: %s\n", arch);
	message(MESS_WARNING, "Please contact bugs@redhat.com\n");
	archnum = 255;
	archname = strdup(arch);
    }

    if (!strcmp(os, "Linux")) {
	osnum = 1;
	osname = strdup("Linux");
    } else if ((!strcmp(os, "IRIX"))) {
	osnum = 2;
	osname = strdup("Irix");
    } else if ((!strcmp(os, "SunOS")) &&
	       (!strncmp(un.release, "5.", 2))) {
	osnum = 3;
	osname = strdup("Solaris");
    } else if ((!strcmp(os, "SunOS")) &&
	       (!strncmp(un.release, "4.", 2))) {
	osnum = 4;
	osname = strdup("SunOS");
    } else {
	/* unknown os */
	message(MESS_WARNING, "Unknown OS: %s\n", os);
	message(MESS_WARNING, "Please contact bugs@redhat.com\n");
	osnum = 255;
	osname = strdup(os);
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

void stripTrailingSlashes(char * str) {
    char * chptr;

    chptr = str + strlen(str) - 1;
    while (*chptr == '/' && chptr >= str) {
	*chptr = '\0';
	chptr--;
    }
}
