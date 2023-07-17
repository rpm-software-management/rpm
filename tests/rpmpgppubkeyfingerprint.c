#include <stddef.h>
#include <limits.h>
#include <stdio.h>

#include <rpm/rpmpgp.h>

struct test {
  char *filename;
  // If NULL, then filename does not contain a valid fingerprint.
  char *fingerprint;
};

static int test(struct test *test)
{
    // This program is run from a container, the data is in /data.
    const char *dir = "/data/";

    const char *filename = test->filename;
    const char *fpr = test->fingerprint;

    char *path = malloc(strlen(dir) + 1 + strlen(filename) + 1);
    if (!path) {
	fprintf(stderr, "out of memory\n");
	return 1;
    }
    sprintf(path, "%s/%s", dir, filename);


    FILE *f = fopen(path, "r");
    if (!f) {
	char buffer[PATH_MAX];
	char *cwd = getcwd(buffer, sizeof(buffer));

	fprintf(stderr, "Failed to open %s (cwd: %s)\n",
		path, cwd ? : "<unknown>");
	free(path);
	return 1;
    }
    free(path);

    const int len = 100 * 1024;
    uint8_t *data = malloc(len);
    if (!data) {
	fprintf(stderr, "out of memory\n");
	return 1;
    }

    ssize_t bytes = fread((char *)data, 1, len, f);
    int err = ferror(f);
    if (err) {
	fprintf(stderr, "%s: Read error: %s\n",
		filename, strerror(err));
	free(data);
	return 1;
    }

    uint8_t *fp = NULL;
    size_t fplen = 0;
    int rc = pgpPubkeyFingerprint(data, bytes, &fp, &fplen);
    if (rc) {
	if (! fpr) {
	    // This test expects the parser to fail.
	    return 0;
	}
	fprintf(stderr, "pgpPubkeyFingerprint failed on %s\n", filename);
	return 1;
    }

    // We expect success now.
    char *got = rpmhex(fp, fplen);
    if (! got) {
	fprintf(stderr, "%s: rpmhex failed\n", filename);
	return 1;
    }

    if (!fpr || strcmp(got, fpr) != 0) {
	fprintf(stderr, "%s:\n     got '%s'\nexpected '%s'\n",
                filename, got, fpr ? fpr : "<none>");
	free(got);
	return 1;
    }
    free(got);

    return 0;
}

int main(void)
{
    int rt;
    int ec = 0;

    struct test tests[] = {
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
