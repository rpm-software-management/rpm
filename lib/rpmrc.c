/** \ingroup rpmrc
 * \file lib/rpmrc.c
 */

#include "system.h"

#include "rpmlib.h"
#include "rpmmacro.h"

static struct _arch_canon {
    const char *ac_arch;
    int		ac_num;
} actbl[] = {
    { "i386",		1 },
    { "alpha",		2 },
    { "sparc",		3 },
    { "mipseb",		4 },
    { "ppc",		5 },
    { "m68k",		6 },
    { "IP",		7 },
    { "rs6000",		8 },
    { "ia64",		9 },
    { "sparc64",	10 },
    { "mipsel",		11 },
    { "armv4l",		12 },
    { "m68kmint",	13 },
    { "s390",		14 },
    { NULL,	0 }
};

static struct _os_canon {
    const char *oc_os;
    int		oc_num;
} octbl[] = {
    { "linux",		1 },	/* XXX was Linux */
    { "irix",		2 },
    { "solaris2",	3 },	/* XXX was solaris */
    { "sunos",		4 },
    { "aix",		5 },
    { "hpux",		6 },	/* XXX was HP-UX */
    { "osf1",		7 },
    { "freebsd",	8 },
    { "sco",		9 },	/* XXX was SCO_SV */
    { "irix",		10 },	/* XXX was IRIX64 */
    { "nextstep",	11 },
    { "bsdi",		12 },	/* XXX was BSD_OS */
    { "machten",	13 },
    { "cygwin",		14 },	/* XXX was cygwin32_NT */
    { "cygwin",		15 },	/* XXX was cygwin32_95 */
    { "sysv4.2uw",	16 },	/* XXX was UNIX_SV (unixware) */
    { "mint",		17 },	/* XXX was MiNT */
    { "openedition",	18 },	/* XXX was OS/390 */
    { "openedition",	19 },	/* XXX was VM/ESA */
    { "linux",		20 },	/* XXX was Linux/390 */
    { NULL,		0 }
};

const char *macrofiles = MACROFILES;

#ifndef	DYING

void XrpmGetArchInfo(const char ** name, int * num, const char *_f, unsigned _l)
{
fprintf(stderr, "*** rpmGetArchInfo(%p,%p) %s:%u\n", name, num, _f, _l);
    if (name)
	*name = rpmExpand("%{_target_cpu}", NULL);
}

void XrpmGetOsInfo(const char ** name, int * num, const char *_f, unsigned _l)
{
fprintf(stderr, "*** rpmGetOsInfo(%p,%p) %s:%u\n", name, num, _f, _l);
    if (name)
	*name = rpmExpand("%{_target_os}", NULL);
}

void XrpmGetMachine(const char **arch, const char **os, const char *_f, unsigned _l)
{
fprintf(stderr, "*** rpmGetMachine(%p,%p) %s:%u\n", arch, os, _f, _l);
}

void XrpmSetMachine(const char * arch, const char * os, const char *_f, unsigned _l)
{
fprintf(stderr, "*** rpmSetMachine(%p,%p) %s:%u\n", arch, os, _f, _l);
}
#endif

int XrpmMachineScore(int type, const char * name, const char * _f, unsigned _l)
{
fprintf(stderr, "*** rpmMachineScore(%d,\"%s\") %s:%u\n", type, name, _f, _l);
    return 0;
}

void rpmFreeRpmrc(void)
{
fprintf(stderr, "*** rpmFreeRpmrc()\n");
}

static void defaultMachine(/*@out@*/ char ** arch, /*@out@*/ char ** os)
{
    static struct utsname un;
    static int gotDefaults = 0;

    if (!gotDefaults) {
	uname(&un);

	/* get rid of the hyphens in the sysname */
	{   char *s;
	    for (s = un.machine; *s; s++)
		if (*s == '/') *s = '-';
	}

#	if HAVE_PERSONALITY && defined(__linux__) && defined(__sparc__)
	if (!strcmp(un.machine, "sparc")) {
	    #define PERS_LINUX		0x00000000
	    #define PERS_LINUX_32BIT	0x00800000
	    #define PERS_LINUX32	0x00000008

	    extern int personality(unsigned long);
	    int oldpers;
	    
	    oldpers = personality(PERS_LINUX_32BIT);
	    if (oldpers != -1) {
		if (personality(PERS_LINUX) != -1) {
		    uname(&un);
		    if (! strcmp(un.machine, "sparc64")) {
			strcpy(un.machine, "sparcv9");
			oldpers = PERS_LINUX32;
		    }
		}
		personality(oldpers);
	    }
	}
#	endif	/* sparc*-linux */

	gotDefaults = 1;
    }

    if (arch && *arch == NULL) *arch = un.machine;
    if (os && *os == NULL) *os = un.sysname;
}

static void rpmRebuildTargetVars(const char **ct)
{
    const char *target;
    char * ca, * cv, * co;
    char * ce;

fprintf(stderr, "*** rpmRebuildTargetVars(%p) %s\n", ct, (ct ? *ct : NULL));
    ca = cv = co = NULL;
    if (ct && *ct) {
	char * c = strcpy(alloca(strlen(*ct)), *ct);
	char ** av;
	int ac;

	/* Set arch, vendor and os from specified build target */
	ac = 1;
	for (ce = c; (ce = strchr(ce, '-')) != NULL; ce++)
	    ac++;
	av = alloca(ac * sizeof(*av));
	ac = 0;
	av[ac++] = c;
	for (ce = c; (ce = strchr(ce, '-')) != NULL;) {
	    *ce++ = '\0';
	    av[ac++] = ce;
	}

	do {
	    ca = av[0];
	    if (ac == 1) break;
	    co = av[(!strcasecmp(av[ac-1], "gnu") ? (ac-2) : (ac-1))];
	    if (ac == 2) break;
	    cv = av[1];
	    if (ac == 3) break;
	    if (!strcasecmp(av[1], "pc"))
		cv = av[2];
	} while (0);
    }

    /* If still not set, set target arch/os from uname(2) values */
    defaultMachine(&ca, &co);

    for (ce = ca; *ce; ce++) *ce = tolower(*ce);
    delMacro(NULL, "_target_cpu");
    addMacro(NULL, "_target_cpu", NULL, ca, RMIL_RPMRC);
fprintf(stderr, "*** _target_cpu: \"%s\"\n", ca);

    if (cv) {
	for (ce = cv; *ce; ce++) *ce = tolower(*ce);
	delMacro(NULL, "_target_vendor");
 	addMacro(NULL, "_target_vendor", NULL, cv, RMIL_RPMRC);
fprintf(stderr, "*** _target_vendor: \"%s\"\n", cv);
    }

    for (ce = co; *ce; ce++) *ce = tolower(*ce);
    delMacro(NULL, "_target_os");
    addMacro(NULL, "_target_os", NULL, co, RMIL_RPMRC);
fprintf(stderr, "*** _target_os: \"%s\"\n", co);

    if (!(ct && *ct)) {
	char * t;
	int nb = strlen(ca) + strlen(co) + sizeof("-");
	if (cv) nb += strlen(cv) + 1;
	target = t = alloca(nb);
	t = stpcpy( stpcpy(t, ca), "-");
	if (cv)
	    t = stpcpy( stpcpy(t, cv), "-");
	t = stpcpy(t, co);
	ct = &target;
    }

    if (ct && *ct) {
	delMacro(NULL, "_target");
	addMacro(NULL, "_target", NULL, *ct, RMIL_RPMRC);
fprintf(stderr, "*** _target: \"%s\"\n", *ct);
    }
}

int rpmReadConfigFiles(const char * mfiles, const char * target)
{

    if (mfiles == NULL)
	mfiles = macrofiles;

    /* Preset target macros */
    rpmRebuildTargetVars(&target);

    if (mfiles) {
	mfiles = rpmExpand(mfiles, NULL);
fprintf(stderr, "*** rpmInitMacros(NULL,\"%s\")\n", mfiles);
	rpmInitMacros(NULL, mfiles);
	xfree(mfiles);

#ifdef	DYING
	/* Reset target macros */
	rpmRebuildTargetVars(&target);
#endif
    }

    return 0;
}

int rpmShowRC(FILE *fp)
{
fprintf(stderr, "*** rpmShowRC(%p)\n", fp);
    return 0;
}
