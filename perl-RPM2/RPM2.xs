#include <stdio.h>
#include "rpmlib.h"
#include "rpmcli.h"

#ifdef RPM2_RPM41
#include "rpmts.h"
#include "rpmte.h"
#endif

#include "header.h"
#include "rpmdb.h"
#include "misc.h"

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#if !defined(RPM2_RPM41) && !defined(RPM2_RPM40)
#error Must define one of RPM2_RPM41 or RPM2_RPM40; perhaps Makefile.PL could not guess your RPM API version?
#endif

/* Chip, this is somewhat stripped down from the default callback used by
   the rpmcli.  It has to be here to insure that we open the pkg again. 
   If we don't do this we get segfaults.  I also, kept the updating of some
   of the rpmcli static vars, but I may not have needed to do this.

   Also, we probably want to give a nice interface such that we could allow
   users of RPM2 to do their own callback, but that will have to come later.
*/
void * _null_callback(
	const void * arg, 
	const rpmCallbackType what, 
	const unsigned long amount, 
	const unsigned long total, 
	fnpyKey key, 
	rpmCallbackData data)
{
	Header h = (Header) arg;
	char * s;
	int flags = (int) ((long)data);
	void * rc = NULL;
	const char * filename = (const char *)key;
	static FD_t fd = NULL;
	int xx;

	/* Code stolen from rpminstall.c and modified */
	switch(what) {
		case RPMCALLBACK_INST_OPEN_FILE:
	 		if (filename == NULL || filename[0] == '\0')
			     return NULL;
			fd = Fopen(filename, "r.ufdio");
			/* FIX: still necessary? */
			if (fd == NULL || Ferror(fd)) {
				fprintf(stderr, "open of %s failed!\n", filename);
				if (fd != NULL) {
					xx = Fclose(fd);
					fd = NULL;
				}
			} else
				fd = fdLink(fd, "persist (showProgress)");
			return (void *)fd;
	 		break;

	case RPMCALLBACK_INST_CLOSE_FILE:
		/* FIX: still necessary? */
		fd = fdFree(fd, "persist (showProgress)");
		if (fd != NULL) {
			xx = Fclose(fd);
			fd = NULL;
		}
		break;

	case RPMCALLBACK_INST_START:
		rpmcliHashesCurrent = 0;
		if (h == NULL || !(flags & INSTALL_LABEL))
			break;
		break;

	case RPMCALLBACK_TRANS_PROGRESS:
	case RPMCALLBACK_INST_PROGRESS:
		break;

	case RPMCALLBACK_TRANS_START:
		rpmcliHashesCurrent = 0;
		rpmcliProgressTotal = 1;
		rpmcliProgressCurrent = 0;
		break;

	case RPMCALLBACK_TRANS_STOP:
		rpmcliProgressTotal = rpmcliPackagesTotal;
		rpmcliProgressCurrent = 0;
		break;

	case RPMCALLBACK_REPACKAGE_START:
		rpmcliHashesCurrent = 0;
		rpmcliProgressTotal = total;
		rpmcliProgressCurrent = 0;
		break;

	case RPMCALLBACK_REPACKAGE_PROGRESS:
		break;

	case RPMCALLBACK_REPACKAGE_STOP:
		rpmcliProgressTotal = total;
		rpmcliProgressCurrent = total;
		rpmcliProgressTotal = rpmcliPackagesTotal;
		rpmcliProgressCurrent = 0;
		break;

	case RPMCALLBACK_UNINST_PROGRESS:
		break;
	case RPMCALLBACK_UNINST_START:
		break;
	case RPMCALLBACK_UNINST_STOP:
		break;
	case RPMCALLBACK_UNPACK_ERROR:
		break;
	case RPMCALLBACK_CPIO_ERROR:
		break;
	case RPMCALLBACK_UNKNOWN:
		break;
	default:
		break;
	}
	
	return rc;	
}

void
_populate_header_tags(HV *href)
{
    int i = 0;

    for (i = 0; i < rpmTagTableSize; i++) {
        hv_store(href, rpmTagTable[i].name, strlen(rpmTagTable[i].name), newSViv(rpmTagTable[i].val), 0);
    }
}

void
_populate_constant(HV *href, char *name, int val)
{
    hv_store(href, name, strlen(name), newSViv(val), 0);
}

#define REGISTER_CONSTANT(name) _populate_constant(constants, #name, name)

MODULE = RPM2		PACKAGE = RPM2

PROTOTYPES: ENABLE
BOOT:
    {
	HV *header_tags, *constants;
	rpmReadConfigFiles(NULL, NULL);

	header_tags = perl_get_hv("RPM2::header_tag_map", TRUE);
	_populate_header_tags(header_tags);

	constants = perl_get_hv("RPM2::constants", TRUE);

	/* not the 'standard' way of doing perl constants, but a lot easier to maintain */
#ifdef RPM2_RPM41
	REGISTER_CONSTANT(RPMVSF_DEFAULT);
	REGISTER_CONSTANT(RPMVSF_NOHDRCHK);
	REGISTER_CONSTANT(RPMVSF_NEEDPAYLOAD);
	REGISTER_CONSTANT(RPMVSF_NOSHA1HEADER);
	REGISTER_CONSTANT(RPMVSF_NOMD5HEADER);
	REGISTER_CONSTANT(RPMVSF_NODSAHEADER);
	REGISTER_CONSTANT(RPMVSF_NORSAHEADER);
	REGISTER_CONSTANT(RPMVSF_NOSHA1);
	REGISTER_CONSTANT(RPMVSF_NOMD5);
	REGISTER_CONSTANT(RPMVSF_NODSA);
	REGISTER_CONSTANT(RPMVSF_NORSA);
	REGISTER_CONSTANT(_RPMVSF_NODIGESTS);
	REGISTER_CONSTANT(_RPMVSF_NOSIGNATURES);
	REGISTER_CONSTANT(_RPMVSF_NOHEADER);
	REGISTER_CONSTANT(_RPMVSF_NOPAYLOAD);
	REGISTER_CONSTANT(TR_ADDED);
	REGISTER_CONSTANT(TR_REMOVED);
#endif

	REGISTER_CONSTANT(RPMSENSE_ANY);
        REGISTER_CONSTANT(RPMSENSE_SERIAL);
        REGISTER_CONSTANT(RPMSENSE_LESS);
        REGISTER_CONSTANT(RPMSENSE_GREATER);
        REGISTER_CONSTANT(RPMSENSE_EQUAL);
        REGISTER_CONSTANT(RPMSENSE_PROVIDES);
        REGISTER_CONSTANT(RPMSENSE_CONFLICTS);
        REGISTER_CONSTANT(RPMSENSE_PREREQ);
        REGISTER_CONSTANT(RPMSENSE_OBSOLETES);
        REGISTER_CONSTANT(RPMSENSE_INTERP);
        REGISTER_CONSTANT(RPMSENSE_SCRIPT_PRE);
        REGISTER_CONSTANT(RPMSENSE_SCRIPT_POST);
        REGISTER_CONSTANT(RPMSENSE_SCRIPT_PREUN);
        REGISTER_CONSTANT(RPMSENSE_SCRIPT_POSTUN);
        REGISTER_CONSTANT(RPMSENSE_SCRIPT_VERIFY);
        REGISTER_CONSTANT(RPMSENSE_FIND_REQUIRES);
        REGISTER_CONSTANT(RPMSENSE_FIND_PROVIDES);
        REGISTER_CONSTANT(RPMSENSE_TRIGGERIN);
        REGISTER_CONSTANT(RPMSENSE_TRIGGERUN);
        REGISTER_CONSTANT(RPMSENSE_TRIGGERPOSTUN);
        REGISTER_CONSTANT(RPMSENSE_SCRIPT_PREP);
        REGISTER_CONSTANT(RPMSENSE_SCRIPT_BUILD);
        REGISTER_CONSTANT(RPMSENSE_SCRIPT_INSTALL);
        REGISTER_CONSTANT(RPMSENSE_SCRIPT_CLEAN);
        REGISTER_CONSTANT(RPMSENSE_RPMLIB);
        REGISTER_CONSTANT(RPMSENSE_TRIGGERPREIN);
        REGISTER_CONSTANT(RPMSENSE_KEYRING);
        REGISTER_CONSTANT(RPMSENSE_PATCHES);
        REGISTER_CONSTANT(RPMSENSE_CONFIG);

    }

double
rpm_api_version(pkg)
	char * pkg
    CODE:
#if defined(RPM2_RPM41) && ! defined(RPM2_RPM40)
	RETVAL = (double)4.1;
#endif
#if ! defined(RPM2_RPM41) && defined(RPM2_RPM40)
	RETVAL = (double)4.0;
#endif
    OUTPUT:
	RETVAL


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

void
expand_macro(pkg, str)
	char * pkg
	char * str
    PREINIT:
	char *ret;
    PPCODE:
	ret = rpmExpand(str, NULL);
	PUSHs(sv_2mortal(newSVpv(ret, 0)));
	free(ret);

int
rpmvercmp(one, two)
	char* one
	char* two

void
_read_package_info(fp, vsflags)
	FILE *fp
	int vsflags
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
	rpmtsSetVSFlags(ts, vsflags);
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
	    croak("error reading package (%d)", rc);
	}
#ifdef RPM2_RPM41
	ts = rpmtsFree(ts);
#endif

void
_create_transaction(vsflags)
	int vsflags
    PREINIT:
	rpmts ret;
	SV *h_sv;
    PPCODE:
	/* Looking at librpm, it does not look like this ever
	   returns error (though maybe it should).
	*/
	ret = rpmtsCreate();

	/* Should I save the old vsflags aside? */
	rpmtsSetVSFlags(ret, vsflags);

	/* Convert and throw the results on the stack */	
	EXTEND(SP, 1);

	h_sv = sv_newmortal();
	sv_setref_pv(h_sv, "RPM2::C::Transaction", (void *)ret);

	PUSHs(h_sv);

void
_read_from_file(fp)
	FILE *fp
PREINIT:
	SV *h_sv;
	FD_t fd;
	Header h;
PPCODE:
	fd = fdDup(fileno(fp));
	h = headerRead(fd, HEADER_MAGIC_YES);

	if (h) {
	    EXTEND(SP, 1);

	    h_sv = sv_newmortal();
	    sv_setref_pv(h_sv, "RPM2::C::Header", (void *)h);

	    PUSHs(h_sv);
	}
	Fclose(fd);


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
	Header       ret;
        SV *         h_sv;
	unsigned int offset;
    PPCODE:
	ret = rpmdbNextIterator(i);
	if (ret)
		headerLink(ret);
	if(ret != NULL) 
		offset = rpmdbGetIteratorOffset(i);
	else
		offset = 0;
	
	EXTEND(SP, 2);
	h_sv = sv_newmortal();
	sv_setref_pv(h_sv, "RPM2::C::Header", (void *)ret);
	PUSHs(h_sv);
	PUSHs(sv_2mortal(newSViv(offset)));

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

int
_header_is_source(h)
	Header h
    CODE:
	RETVAL = headerIsEntry(h, RPMTAG_SOURCEPACKAGE);
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

void
_unload(h)
	Header h
    PREINIT:
	char *buf;
	unsigned int size;
    PPCODE:
	size = headerSizeof(h, 0);
	buf = headerUnload(h);
	PUSHs(sv_2mortal(newSVpv(buf, size)));

MODULE = RPM2		PACKAGE = RPM2::C::Transaction

void
DESTROY(t)
	rpmts t
    CODE:
	t = rpmtsFree(t);

# XXX:  Add relocations some day. 
int
_add_install(t, h, fn, upgrade)
	rpmts  t
	Header h
	char * fn
	int    upgrade
    PREINIT:
	rpmRC rc = 0;
    CODE:
	rc = rpmtsAddInstallElement(t, h, (fnpyKey) fn, upgrade, NULL);
	RETVAL = (rc == RPMRC_OK) ? 1 : 0;
    OUTPUT:
	RETVAL	

int
_add_delete(t, h, offset)
	rpmts        t
	Header       h
	unsigned int offset
    PREINIT:
	rpmRC rc = 0;
    CODE:
	rc = rpmtsAddEraseElement(t, h, offset);
	RETVAL = (rc == RPMRC_OK) ? 1 : 0;
    OUTPUT:
	RETVAL	

int
_element_count(t)
	rpmts t
PREINIT:
	int ret;
CODE:
	ret    = rpmtsNElements(t);
	RETVAL = ret;
OUTPUT:
	RETVAL

int
_close_db(t)
	rpmts t
PREINIT:
	int ret;
CODE:
	ret    = rpmtsCloseDB(t);
	RETVAL = (ret == 0) ? 1 : 0;
OUTPUT:
	RETVAL

int
_check(t)
	rpmts t
PREINIT:
	int ret;
CODE:
	ret    = rpmtsCheck(t);
	RETVAL = (ret == 0) ? 1 : 0;
OUTPUT:
	RETVAL

int
_order(t)
	rpmts t
PREINIT:
	int ret;
CODE:
	ret    = rpmtsOrder(t);
	/* XXX:  May want to do something different here.  It actually
	         returns the number of non-ordered elements...maybe we
	         want this?
	*/
	RETVAL = (ret == 0) ? 1 : 0;
OUTPUT:
	RETVAL

void
_elements(t, type)
	rpmts t;
	rpmElementType type;
PREINIT:
	rpmtsi       i;
	rpmte        te;
	const char * NEVR;
PPCODE:
	i = rpmtsiInit(t);
	if(i == NULL) {
		printf("Did not get a thing!\n");
		return;	
	} else {
		while((te = rpmtsiNext(i, type)) != NULL) {
			NEVR = rpmteNEVR(te);
			XPUSHs(sv_2mortal(newSVpv(NEVR,	0)));
		}
		i = rpmtsiFree(i);
	}

int
_run(t, ok_probs, prob_filter)
	rpmts t
	rpmprobFilterFlags prob_filter 
    PREINIT:
	int i;
	rpmProblem p;
	int ret;
    CODE:
	/* Make sure we could run this transactions */
	ret = rpmtsCheck(t);
	if (ret != 0) {
		RETVAL = 0;
		return;
	}
	ret = rpmtsOrder(t);
	if (ret != 0) {
		RETVAL = 0;
		return;
	}

	/* XXX:  Should support callbacks eventually */
	(void) rpmtsSetNotifyCallback(t, _null_callback, (void *) ((long)0));
	ret    = rpmtsRun(t, NULL, prob_filter);
	RETVAL = (ret == 0) ? 1 : 0;
    OUTPUT:
	RETVAL


