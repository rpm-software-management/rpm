#include <stddef.h>
#include <limits.h>
#include <stdio.h>

#include <rpm/rpmpgp.h>
#include <rpm/rpmfileutil.h>

struct test {
  char *filename;
  // If NULL, then filename does not contain a valid fingerprint.
  char *fingerprint;
};

// This program is run from a container, the data is in /data.
#define DIR     "/data"

#define LEN     102400 // 100 * 1024

static int test(const struct test *test)
{
    int ret = 1;
    char *path = NULL;
    FILE *f;
    uint8_t data[LEN] = {};
    ssize_t bytes;
    int rc;

    if (!test) {
    fprintf(stderr, "Invalid arg\n");
    return ret;
    }

    const char *filename = test->filename;
    const char *fpr = test->fingerprint;

    rasprintf(&path, "%s/%s", DIR, filename);

    f = fopen(path, "r");
    if (!f) {
    char *cwd = rpmGetCwd();
    fprintf(stderr, "Failed to open %s (cwd: %s)\n",
        path, cwd ? : "<unknown>");
    free(cwd);
    free(path);
    goto end;
    }
    free(path);

    bytes = fread((char *)data, 1, LEN, f);
    rc = ferror(f);
    if (rc) {
	fprintf(stderr, "%s: Read error: %s\n",
		filename, strerror(rc));
	return ret;
    }

    uint8_t *fp = NULL;
    size_t fplen = 0;
    rc = pgpPubkeyFingerprint(data, bytes, &fp, &fplen);
    if (rc) {
	if (! fpr) {
	    // This test expects the parser to fail.
	    ret = 0;
	    goto end;
	}
	fprintf(stderr, "pgpPubkeyFingerprint failed on %s\n", filename);
    goto end;
    }

    // We expect success now.
    char *got = rpmhex(fp, fplen);
    if (! got) {
	fprintf(stderr, "%s: rpmhex failed\n", filename);
	return ret;
    }

    if (!fpr || strcmp(got, fpr) != 0) {
	fprintf(stderr, "%s:\n     got '%s'\nexpected '%s'\n",
                filename, got, fpr ? fpr : "<none>");
	free(got);
	return ret;
    }
    free(got);
    free(fp);

    ret = 0;

end:
    return ret;
}

int main(void)
{
    int rt;
    int ec = 0;

    const struct test tests[] = {
      // Make sure this fails.
      { "RPMS/hello-1.0-1.i386.rpm", NULL },
      // ASCII-armor
      { "keys/rpm.org-rsa-2048-test.pub",
	NULL },
      { "keys/CVE-2021-3521-badbind.asc",
	NULL },
      // Binary.
      { "keys/rpm.org-rsa-2048-test.pgp",
	"771b18d3d7baa28734333c424344591e1964c5fc" },
    };

    int i;
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
	rt = test(&tests[i]);
	if (rt != 0) {
	    fprintf(stderr, "%s failed\n", tests[i].filename);
	    ec = 1;
	}
    }

    fprintf(stderr, "Exit code: %d\n", ec);
    return ec;
}
