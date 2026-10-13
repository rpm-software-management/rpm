#include <rpm/header.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmio.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmts.h>

static Header readPkg(const char *path)
{
    Header h = NULL;
    FD_t fd = Fopen(path, "r.ufdio");
    if (fd) {
	rpmts ts = rpmtsCreate();
	rpmtsSetVSFlags(ts, RPMVSF_MASK_NOSIGNATURES);
	rpmReadPackageFile(ts, fd, path, &h);
	Fclose(fd);
	rpmtsFree(ts);
    }
    return h;
}

int main(int argc, char *argv[])
{
    int rc = EXIT_FAILURE;
    rpmfiles fi = NULL;
    Header h = NULL;
    struct rpmtd_s td;

    if (argc < 2 || rpmReadConfigFiles(NULL, NULL))
	goto exit;

    h = readPkg(argv[1]);
    if (h == NULL)
	goto exit;

    printf("%s\n", headerGetString(h, RPMTAG_NAME));

    fi = rpmfilesNew(NULL, h, 0, 0);
    printf("flags 0 fc %u\n", rpmfilesFC(fi));
    fi = rpmfilesFree(fi);

    /* insert invalid filesignatures tag */
    headerGet(h, RPMTAG_BASENAMES, &td, HEADERGET_ALLOC);
    printf("td size %u\n", rpmtdSize(&td));
    td.tag = RPMTAG_FILESIGNATURES;
    td.type = RPM_I18NSTRING_TYPE;
    headerPut(h, &td, HEADERPUT_DEFAULT);
    rpmtdFreeData(&td);

    printf("begin invalid ima\n");
    headerGet(h, RPMTAG_FILESIGNATURES, &td, HEADERGET_ALLOC);
    printf("td size %u\n", rpmtdSize(&td));
    rpmtdFreeData(&td);

    fi = rpmfilesNew(NULL, h, 0, RPMFI_NOFILESIGNATURES);
    printf("flags nosignature fc %u\n", rpmfilesFC(fi));
    fi = rpmfilesFree(fi);

    fi = rpmfilesNew(NULL, h, 0, 0);
    printf("flags 0 fc %u\n", rpmfilesFC(fi));
    fi = rpmfilesFree(fi);

    /* see if we can reload the bad header */
    h = headerReload(h, RPMTAG_HEADERIMMUTABLE);
    if (h)
	printf("%s\n", headerGetString(h, RPMTAG_NAME));

    rc = 0;

exit:
    headerFree(h);
    rpmfilesFree(fi);
    rpmFreeRpmrc();
    rpmFreeMacros(NULL);
    return rc;
}
