#include "rpmlib.h"
#include "rpmcli.h"

#ifdef RPM2_RPM41
#include "rpmts.h"
#endif

#include "header.h"
#include "rpmdb.h"
#include "misc.h"

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

void
_populate_header_tags(HV *href)
{
    int i = 0;

    for (i = 0; i < rpmTagTableSize; i++) {
        hv_store(href, rpmTagTable[i].name, strlen(rpmTagTable[i].name), newSViv(rpmTagTable[i].val), 0);
    }
}

MODULE = RPM2		PACKAGE = RPM2

PROTOTYPES: ENABLE
BOOT:
    {
	HV *header_tags;
	rpmReadConfigFiles(NULL, NULL);
	header_tags = perl_get_hv("RPM2::header_tag_map", TRUE);
	_populate_header_tags(header_tags);
    }

void
add_macro(pkg, name, val)
	char * pkg
	char * name
	char * val
    CODE:
	addMacro(NULL, name, NULL, val, RMIL_DEFAULT);

void
delete_macro(pkg, name)
	char * pkg
	char * name
    CODE:
	delMacro(NULL, name);

int
rpmvercmp(one, two)
	char* one
	char* two

void
_read_package_info(fp)
	FILE *fp
    PREINIT:
#ifdef RPM2_RPM41
	rpmts ts;
#endif
	Header ret;
	Header sigs;
	rpmRC rc;
	FD_t fd;
    PPCODE:
#ifdef RPM2_RPM41
	ts = rpmtsCreate();
#endif

        /* XXX Determine type of signature verification when reading
	vsflags |= _RPMTS_VSF_NOLEGACY;
	vsflags |= _RPMTS_VSF_NODIGESTS;
	vsflags |= _RPMTS_VSF_NOSIGNATURES;
	xx = rpmtsSetVerifySigFlags(ts, vsflags);
        */ 

	fd = fdDup(fileno(fp));
#ifdef RPM2_RPM41
	rpmtsSetVSFlags(ts, _RPMVSF_NOSIGNATURES);
	rc = rpmReadPackageFile(ts, fd, "filename or other identifier", &ret);
#else
	rc = rpmReadPackageInfo(fd, NULL, &ret);
#endif

	Fclose(fd);

	if (rc == RPMRC_OK) {
	    SV *h_sv;

	    EXTEND(SP, 1);

	    h_sv = sv_newmortal();
            sv_setref_pv(h_sv, "RPM2::C::Header", (void *)ret);

	    PUSHs(h_sv);
	}
	else {
	    croak("error reading package");
	}
#ifdef RPM2_RPM41
	ts = rpmtsFree(ts);
#endif

rpmdb
_open_rpm_db(for_write)
	int   for_write
    PREINIT:
	 rpmdb db;
    CODE:
	if (rpmdbOpen(NULL, &db, for_write ? O_RDWR | O_CREAT : O_RDONLY, 0644)) {
		croak("rpmdbOpen failed");
		RETVAL = NULL;
	}
	RETVAL = db;		
    OUTPUT:
	RETVAL

MODULE = RPM2		PACKAGE = RPM2::C::DB

void
DESTROY(db)
	rpmdb db
    CODE:
	rpmdbClose(db);

void
_close_rpm_db(self)
	rpmdb self
    CODE:
	rpmdbClose(self);

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

MODULE = RPM2		PACKAGE = RPM2::C::PackageIterator
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
DESTROY(i)
	rpmdbMatchIterator i
    CODE:
	rpmdbFreeIterator(i);


MODULE = RPM2		PACKAGE = RPM2::C::Header

void
DESTROY(h)
	Header h
    CODE:
	headerFree(h);

void
tag_by_id(h, tag)
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
_header_compare(h1, h2)
	Header h1
	Header h2
    CODE:
	RETVAL = rpmVersionCompare(h1, h2);
    OUTPUT:
        RETVAL

void
_header_sprintf(h, format)
	Header h
	char * format
    PREINIT:
	char * s;
    PPCODE:
	s =  headerSprintf(h, format, rpmTagTable, rpmHeaderFormats, NULL);
	PUSHs(sv_2mortal(newSVpv((char *)s, 0)));
	s = _free(s);
