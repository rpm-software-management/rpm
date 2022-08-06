#include <stddef.h>
#include <limits.h>
#include <stdio.h>

#include <rpm/rpmpgp.h>

struct test {
    // A file in testing/data/keys
    char *key;
    // A file in testing/data/sigs
    char *sig;
    // A string.
    char *message;
    // The hash algorithm.
    int hashalgo;
    // Whether the signature is valid.
    int good;
};

static int parse(struct test *test)
{
    // This program is run from BUILDDIR/tests/rpmtests.dir/XXX.  The
    // data is in BUILDDIR/tests/testing.

    pgpDig dig = pgpNewDig();

    char *dirs[] = {
	"../../testing/data/keys",
	"../../testing/data/sigs",
    };
    const char *const files[sizeof(dirs) / sizeof(dirs[0])] = {
	test->key,
	test->sig,
    };

    // Read in the key and the sig.
    int i;
    for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
	const char *dir = dirs[i];
	const char *file = files[i];
	fprintf(stderr, "%s:\n", file ? file : "<none>");
	if (!file) {
	    continue;
	}

	char *path = malloc(strlen(dir) + 1 + strlen(file) + 1);
	if (!path) {
	    fprintf(stderr, "out of memory\n");
	    return 1;
	}
	sprintf(path, "%s/%s", dir, file);

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
	    fprintf(stderr, "Read error: %s\n", strerror(err));
	    free(data);
	    return 1;
	}

	int rc = pgpPrtPkts(data, bytes, dig, 1);
	if (!test->key && rc == RPMRC_NOKEY) {
	    rc = 0;
	}
	free(data);
	if (rc) {
	    fprintf(stderr, "%s was rejected by pgpPrtPkts\n", file);
	    return 1;
	}
    }

    DIGEST_CTX hashctx = rpmDigestInit(test->hashalgo, RPMDIGEST_NONE);
    rpmDigestUpdate(hashctx, test->message, strlen(test->message));

    int good = pgpVerifySig(dig, hashctx) == 0;
    if (good != test->good) {
	fprintf(stderr,
		"%s/%s: sig was %sgood (%d); sig was supposed to be %sgood (%d)\n",
		test->key ? test->key : "<none>",
		test->sig ? test->sig : "<none>",
		good ? "" : "not ", good, test->good ? "" : "not ", test->good);
    } else {
	fprintf(stderr, "%s/%s: sig is %s (expected)\n",
		test->key ? test->key : "<none>",
		test->sig ? test->sig : "<none>", good ? "good" : "invalid");
    }

    pgpFreeDig(dig);

    return 0;
}

int main(void)
{
    int rt;
    int ec = 0;

    // Note: pgpPrtPkts does *not* handle ascii armored data.
    struct test tests[] = {
	{"rpm.org-rsa-2048-test.pgp", "rpm.org-rsa-2048-test-hi.pgp",
	 "hi\n", RPM_HASH_SHA512, 1},
	// The signature is actually good, but pgpVerifySig only checks
	// that the signature came from the primary key and this signature
	// came from a subkey.
	{"neal.pgp", "neal-hi.pgp", "hi\n", RPM_HASH_SHA512, 0},
	// If we only specify the signature, then only the checksum is
	// checked, which is good.
	{NULL, "neal-hi.pgp", "hi\n", RPM_HASH_SHA512, 1},
	// Wrong hash algorithm should be bad.
	{NULL, "neal-hi.pgp", "hi\n", RPM_HASH_SHA1, 0},
    };

    int i;
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
	rt = parse(&tests[i]);
	if (rt != 0) {
	    ec = 1;
	}
    }

    return ec;
}
