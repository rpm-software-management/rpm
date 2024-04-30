#include <stddef.h>
#include <limits.h>
#include <stdio.h>

#include <rpm/rpmpgp.h>

struct test {
	char *filename;
	// If NULL, then filename does not contain a valid fingerprint.
	char *fingerprint;
};

#define LEN	 102400 // 100 * 1024

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof(a[0]))

// This program is run from a container, the data is in /data.
#define DIR "/data"

#define pr_err(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

static int test(const struct test *test)
{
	FILE *f = NULL;
	char *got = NULL;
	char *path;
	uint8_t *data = NULL;
	uint8_t *fp = NULL;
	size_t fplen = 0;
	size_t bytes;
	int err;
	int rc;
	int ret = 1;

	if (!test) {
		pr_err("Invalid arg\n");
		return ret;
	}

	const char *filename = test->filename;
	const char *fpr = test->fingerprint;
	int path_len = sizeof(DIR) + 1 + strlen(filename) + 1;

	path = (char *)calloc(path_len, sizeof(char));
	if (!path) {
		pr_err("out of memory\n");
		return ret;
	}

	rc = snprintf(path, path_len, "%s/%s", DIR, filename);
	if (rc < 0 || rc >= path_len) {
		pr_err("snprintf error\n");
		free(path);
		return ret;
	}

	f = fopen(path, "r");
	if (!f) {
		char buffer[PATH_MAX] = {};
		char *cwd = getcwd(buffer, sizeof(buffer));

		pr_err("Failed to open %s (cwd: %s)\n", path, cwd ? : "<unknown>");
		free(path);
		goto end;
	}
	free(path);

	data = (uint8_t *)calloc(LEN, sizeof(uint8_t));
	if (!data) {
		pr_err("out of memory\n");
		goto end;
	}

	bytes = fread((char *)data, 1, LEN, f);
	err = ferror(f);
	if (err) {
		pr_err("%s: Read error: %s\n", filename, strerror(err));
		goto end;
	}

	rc = pgpPubkeyFingerprint(data, bytes, &fp, &fplen);
	free(data);
	if (rc) {
		if (!fpr) {
			// This test expects the parser to fail.
			ret = 0;
		} else {
			pr_err("pgpPubkeyFingerprint failed on %s\n", filename);
		}
		goto end;
	}

	// We expect success now.
	got = rpmhex(fp, fplen);
	if (!got) {
		pr_err("%s: rpmhex failed\n", filename);
		goto end;
	}

	if (!fpr || strcmp(got, fpr)) {
		pr_err("%s:\n     got '%s'\nexpected '%s'\n",
				filename, got, fpr ? fpr : "<none>");
		goto end;
	}

	ret = 0;

end:
	if (got)
		free(got);

	if (fp)
		free(fp);

	if (f)
		fclose(f);

	return ret;
}

int main(void)
{
	int ec = 0;
	int i;

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

	for (i = 0; i < (int) ARRAY_SIZE(tests); i++) {
		if (test(&tests[i])) {
			pr_err("%s failed\n", tests[i].filename);
			if (!ec)
				ec = 1;
		}
	}

	pr_err("Exit code: %d\n", ec);
	return ec;
}
