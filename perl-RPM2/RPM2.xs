#include "rpmcli.h"
#include "rpmlib.h"
#include "misc.h"

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

const char *CLASS = "RPM2";
MODULE = RPM2		PACKAGE = RPM2

int
rpmvercmp(one, two)
	char* one
	char* two

void
_init_rpm()
    CODE:
	rpmReadConfigFiles(NULL, NULL);

void
_close_rpm_db(db)
	rpmdb db
    CODE:
	rpmdbClose(db);

rpmdb
_open_rpm_db(path,for_write)
        char  *path
	int   for_write
    PREINIT:
	 rpmdb db;
    CODE:
	if (rpmdbOpen(path, &db, for_write ? O_RDWR | O_CREAT : O_RDONLY, 0644)) {
		croak("rpmdbOpen failed");
		RETVAL = NULL;
	}
	RETVAL = db;		
    OUTPUT:
	RETVAL

rpmdbMatchIterator
_init_iterator(db, rpmtag, key, len)
	rpmdb db
	int rpmtag
	char *key
	size_t len
    CODE:
	RETVAL = rpmdbInitIterator(db, rpmtag, key && *key ? key : NULL, len);
    OUTPUT:
	RETVAL

void
_destroy_iterator(i)
	rpmdbMatchIterator i
    CODE:
	rpmdbFreeIterator(i);

Header
_iterator_next(i)
	rpmdbMatchIterator i
    PREINIT:
	Header ret;
    CODE:
	ret = rpmdbNextIterator(i);
	if (ret)
		headerLink(ret);
	RETVAL = ret;
    OUTPUT:
	RETVAL

void
_read_package_info(fp)
	FILE *fp
    PREINIT:
	Header ret;
	Header sigs;
	rpmRC rc;
	FD_t fd;
    PPCODE:
	fd = fdDup(fileno(fp));
	rc = rpmReadPackageInfo(fd, &sigs, &ret);
	Fclose(fd);

	if (rc == RPMRC_OK) {
	    SV *h_sv, *s_sv;

	    EXTEND(SP, 2);

	    h_sv = sv_newmortal();
	    s_sv = sv_newmortal();
            sv_setref_pv(h_sv, "Header", (void *)ret);
            sv_setref_pv(s_sv, "Header", (void *)sigs);

	    PUSHs(h_sv);
	    PUSHs(s_sv);
	}
	else {
	    croak("error reading package");
	}

void
_free_header(h)
	Header h
    CODE:
	headerFree(h);

void
_header_tag(h, tag)
	Header h
	int tag
    PREINIT:
	void *ret = NULL;
	int type;
	int n;
	int ok;
    PPCODE:
	ok = headerGetEntry(h, tag, &type, &ret, &n);

	if (!ok) {
		/* nop, empty stack */
	}
	else {
		switch(type)
		{
		case RPM_STRING_ARRAY_TYPE:
			{
			int i;
			char **s;

			EXTEND(SP, n);
			s = (char **)ret;

			for (i = 0; i < n; i++) {
				PUSHs(sv_2mortal(newSVpv(s[i], 0)));
			}
			}
			break;
		case RPM_STRING_TYPE:
			PUSHs(sv_2mortal(newSVpv((char *)ret, 0)));
			break;
		case RPM_CHAR_TYPE:
		case RPM_INT8_TYPE:
		case RPM_INT16_TYPE:
		case RPM_INT32_TYPE:
			{
			int i;
			int *r;

			EXTEND(SP, n);
			r = (int *)ret;

			for (i = 0; i < n; i++) {
				PUSHs(sv_2mortal(newSViv(r[i])));
			}
			}
			break;
		default:
			croak("unknown rpm tag type %d", type);
		}
	}
	headerFreeData(ret, type);

int
_header_is_source(h)
	Header h
    CODE:
	RETVAL = headerIsEntry(h, RPMTAG_SOURCEPACKAGE);
    OUTPUT:
	RETVAL

void
_populate_header_tags(href)
	SV *href
    PREINIT:
	int i = 0;
	HV *h;
    CODE:
	h = (HV *)SvRV(href);
	for (i = 0; i < rpmTagTableSize; i++) {
	    hv_store(h, rpmTagTable[i].name, strlen(rpmTagTable[i].name), newSViv(rpmTagTable[i].val), 0);
	}
