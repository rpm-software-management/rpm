#include <assert.h>
#include <rpm/header.h>
#include <rpm/rpmstrpool.h>

static size_t try2pool(Header h, int raw, rpmTag tag, rpm_count_t expect)
{
    size_t npool = 0;
    struct rpmtd_s td;
    headerGetFlags hgflags = raw ? HEADERGET_RAW : HEADERGET_DEFAULT;
    if (headerGet(h, tag, &td, hgflags)) {
	rpmsid *sids = NULL;
	rpmstrPool pool = rpmstrPoolCreate();
	assert(pool != NULL);

        if (rpmtdCount(&td) == expect)
            sids = rpmtdToPool(&td, pool);
	npool = rpmstrPoolNumStr(pool);

        rpmtdFreeData(&td);
	pool = rpmstrPoolFree(pool);
	assert(pool == NULL);
	free(sids);
    }
    return npool;

}

int main(int argc, char *argv[])
{
    const char *strings[] = { "/and/one", "/and/two", "/and/three" };
    Header h = headerNew();
    assert(h != NULL);

    assert(headerPutString(h, RPMTAG_NAME, "name") == 1);
    assert(headerPutStringArray(h, RPMTAG_OLDFILENAMES, strings, 3) == 1);
    assert(headerAddI18NString(h, RPMTAG_GROUP, "ryhmä", "fi_FI") == 1);
    /* XXX the return code is buggy, this succeeds */
    assert(headerAddI18NString(h, RPMTAG_GROUP, "group", "C") == 0);

    assert(try2pool(h, 0, RPMTAG_NAME, 1) == 0); /* not implemented */
    assert(try2pool(h, 0, RPMTAG_OLDFILENAMES, 3) == 3);
    /* normal i18n lookup returns a string, raw get the actual i18n array */
    assert(try2pool(h, 0, RPMTAG_GROUP, 1) == 0); /* not implemented */
    assert(try2pool(h, 1, RPMTAG_GROUP, 2) == 2);

    h = headerFree(h);
    assert(h == NULL);
    return 0;
}
