#include <assert.h>
#include <stdio.h>
#include <rpm/header.h>

static int tryReload(Header h)
{
    int rc = 0;
    unsigned int blen = 0;
    void *blob = headerExport(h, &blen);
    Header nh = headerImport(blob, blen, HEADERIMPORT_COPY);
    if (nh != NULL) {
	headerFree(nh);
	rc = 1;
    }

    free(blob);
    return rc;
}

static Header initHdr(void)
{
    uint32_t buildtime = 1234567890;
    Header h = headerNew();
    assert(h != NULL);

    /* put some basic content in there and reload to an immutable region */
    assert(headerPutString(h, RPMTAG_NAME, "test") == 1);
    assert(headerPutString(h, RPMTAG_VERSION, "1.0") == 1);
    assert(headerPutString(h, RPMTAG_RELEASE, "1") == 1);
    assert(headerPutString(h, RPMTAG_ARCH, "noarch") == 1);
    assert(headerPutUint32(h, RPMTAG_BUILDTIME, &buildtime, 1) == 1);
    assert((h = headerReload(h, RPMTAG_HEADERIMMUTABLE)) != NULL);
    assert(tryReload(h) == 1);
    return h;
}

int main(int argc, char *argv[])
{
    struct rpmtd_s td;
    Header h = initHdr();

    /* no epoch in there */
    assert(headerGet(h, RPMTAG_EPOCH, &td, HEADERGET_DEFAULT) == 0);
    assert(headerDel(h, RPMTAG_EPOCH) == 1);

    /* i18ntable an array, see that headerPutString gets it right */
    assert(headerPutString(h, RPMTAG_HEADERI18NTABLE, "C") == 1);
    assert(headerGet(h, RPMTAG_HEADERI18NTABLE, &td, HEADERGET_MINMEM) == 1);
    assert(td.type == RPM_STRING_ARRAY_TYPE);
    assert(td.count == 1);
    assert(td.flags == (RPMTD_ALLOCED|RPMTD_IMMUTABLE));
    assert(td.ix == -1);

    /* see that rpmtdFreeData() does what it should */
    rpmtdFreeData(&td);
    assert(td.data == NULL && td.count == 0 && td.type == 0 && td.tag == 0 &&
	   td.flags == 0 && td.size == 0 && td.ix == -1);

    /* can we still reload the header? */
    assert(tryReload(h) == 1);

    /* ALLOC mode */
    assert(headerGet(h, RPMTAG_HEADERI18NTABLE, &td, HEADERGET_ALLOC) == 1);
    /* XXX RPMTD_IMMUTABLE here is a bug */
    assert(td.flags == (RPMTD_ALLOCED|RPMTD_IMMUTABLE));

    /* change the type to invalid */
    free(td.data); /* XXX don't do this at home */
    td.data = rstrdup("bad");
    td.type = RPM_STRING_TYPE;
    /* mod should not find the entry now */
    assert(headerMod(h, &td) == 0);

    /* test headerIsEntry() and headerDel() */
    assert(headerIsEntry(h, RPMTAG_HEADERI18NTABLE) == 1);
    assert(headerDel(h, RPMTAG_HEADERI18NTABLE) == 0);
    assert(headerIsEntry(h, RPMTAG_HEADERI18NTABLE) == 0);
    /* see it still reloads after deletes */
    assert(tryReload(h) == 1);

    /* insert i18ntable with wrong type */
    assert(headerPut(h, &td, HEADERPUT_DEFAULT) == 1);
    rpmtdFreeData(&td);

    /* check that headerPut() didn't adjust what we did */
    assert(headerGet(h, RPMTAG_HEADERI18NTABLE, &td, HEADERGET_MINMEM) == 1);
    assert(td.type == RPM_STRING_TYPE);
    assert(td.count == 1);
    assert(td.flags == (RPMTD_IMMUTABLE));
    assert(td.ix == -1);
    rpmtdFreeData(&td);

    /* reload should fail due to type mismatch on a known tag */
    /* but rpmpack uses invalid type on headeri18n, let it pass for now */
    assert(tryReload(h) == 1);

    /* insert an invalid (numeric) name tag */
    assert(headerGet(h, RPMTAG_BUILDTIME, &td, HEADERGET_DEFAULT) == 1);
    assert(headerDel(h, RPMTAG_NAME) == 0);
    td.tag = RPMTAG_NAME;
    assert(headerPut(h, &td, HEADERPUT_DEFAULT) == 1);

    /* reload should fail due to type mismatch on a known tag */
    assert(tryReload(h) == 0);

    /* see that rpmtdFreeData() does everything we expect */
    rpmtdFreeData(&td);
    assert(td.tag == 0 && td.type == 0 && td.count == 0 && td.data == NULL && td.flags == 0 && td.ix == -1);

    h = headerFree(h);
    assert(h == NULL);
}
