#include "system.h"
#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmio.h>

extern int _rpmio_debug;

#define	_ETC_RPM_PLATFORM	"/etc/rpm/platform"
static const char * platform = _ETC_RPM_PLATFORM;

static const char ** platpat = NULL;
static int nplatpat = 0;

static int rpmPlatform(void)
{
    char *cpu = NULL, *vendor = NULL, *os = NULL, *gnu = NULL;
    char * b = NULL;
    ssize_t blen = 0;
    int init_platform = 0;
    char * p, * pe;
    int rc;

    rc = rpmioSlurp(platform, &b, &blen);

    if (rc || b == NULL || blen <= 0) {
	rc = -1;
	goto exit;
    }

    p = b;
    for (pe = p; p && *p; p = pe) {
	pe = strchr(p, '\n');
	if (pe)
	    *pe++ = '\0';

fprintf(stderr, "--- %s\n", p);

	while (*p && isspace(*p))
	    p++;
	if (*p == '\0' || *p == '#')
	    continue;

	if (init_platform) {
	    char * t = p + strlen(p);

	    while (--t > p && isspace(*t))
		*t = '\0';
	    if (t > p) {
		platpat = xrealloc(platpat, (nplatpat + 2) * sizeof(*platpat));
		platpat[nplatpat] = xstrdup(p);
fprintf(stderr, "\tplatpat[%d] \"%s\"\n", nplatpat, platpat[nplatpat]);
		nplatpat++;
		platpat[nplatpat] = NULL;
	    }
	    continue;
	}

	cpu = p;
	vendor = "unknown";
	os = "unknown";
	gnu = NULL;
	while (*p && !(*p == '-' || isspace(*p)))
	    p++;
	if (*p != '\0') *p++ = '\0';
fprintf(stderr, "--- cpu \"%s\"\n", cpu);

	vendor = p;
	while (*p && !(*p == '-' || isspace(*p)))
	    p++;
	if (*p != '-') {
	    if (*p != '\0') *p++ = '\0';
	    os = vendor;
	    vendor = "unknown";
	} else {
	    if (*p != '\0') *p++ = '\0';

	    os = p;
	    while (*p && !(*p == '-' || isspace(*p)))
		p++;
	    if (*p == '-') {
		*p++ = '\0';

		gnu = p;
		while (*p && !(*p == '-' || isspace(*p)))
		    p++;
	    }
	    if (*p != '\0') *p++ = '\0';
	}
fprintf(stderr, "--- vendor \"%s\"\n", vendor);
fprintf(stderr, "--- os \"%s\"\n", os);
fprintf(stderr, "--- gnu \"%s\"\n", gnu);

	addMacro(NULL, "_host_cpu", NULL, cpu, -1);
	addMacro(NULL, "_host_vendor", NULL, vendor, -1);
	addMacro(NULL, "_host_os", NULL, os, -1);

	platpat = xrealloc(platpat, (nplatpat + 2) * sizeof(*platpat));
	platpat[nplatpat] = rpmExpand("%{_host_cpu}-%{_host_vendor}-%{_host_os}", (gnu && *gnu ? "-" : NULL), gnu, NULL);
fprintf(stderr, "\tplatpat[%d] \"%s\"\n", nplatpat, platpat[nplatpat]);
	nplatpat++;
	platpat[nplatpat] = NULL;
	
	init_platform++;
    }
    rc = (init_platform ? 0 : -1);

exit:
    b = _free(b);
    return rc;
}

int main (int argc, const char * argv[])
{

    int rc;

_rpmio_debug = 0;
    rc = rpmPlatform();

    return rc;
}
