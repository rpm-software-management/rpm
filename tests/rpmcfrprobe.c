#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/fs.h>
#include <sys/ioctl.h>
#endif

int main(int argc, char **argv)
{
    int infd = -1, outfd = -1;
    off_t in = 0, out = 0;
    size_t left = 4096;
    int rc = EXIT_FAILURE;

    if (argc != 3 && argc != 5)
	return EXIT_FAILURE;

    if (argc == 5) {
	char *end = NULL;
	long long offset;
	unsigned long long length;

	errno = 0;
	offset = strtoll(argv[3], &end, 0);
	if (errno || offset < 0 || end == argv[3] || *end != '\0' ||
	    (long long)(off_t)offset != offset)
	    return EXIT_FAILURE;
	in = (off_t)offset;
	errno = 0;
	length = strtoull(argv[4], &end, 0);
	if (errno || length == 0 || end == argv[4] || *end != '\0' ||
	    length > SIZE_MAX || (off_t)length < 0 ||
	    (unsigned long long)(off_t)length != length)
	    return EXIT_FAILURE;
	left = (size_t)length;
    }

    infd = open(argv[1], O_RDONLY);
    outfd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (infd < 0 || outfd < 0)
	goto exit;

    if (argc == 5) {
#if defined(__linux__) && defined(FICLONERANGE)
	struct file_clone_range range = {
	    .src_fd = infd,
	    .src_offset = in,
	    .src_length = left,
	    .dest_offset = 0,
	};
	/* This mode succeeds only when the exact range rejects cloning but
	 * remains eligible for copy_file_range(), proving the fallback path.
	 * A successful clone would bypass the bounded progress path under
	 * test. */
	if (ioctl(outfd, FICLONERANGE, &range) == 0)
	    goto exit;
#endif
	if (ftruncate(outfd, 0))
	    goto exit;
    }

    while (left > 0) {
	ssize_t n = copy_file_range(infd, &in, outfd, &out, left, 0);
	if (n < 0 && errno == EINTR)
	    continue;
	if (n <= 0)
	    goto exit;
	left -= n;
    }
    rc = EXIT_SUCCESS;

exit:
    if (infd >= 0)
	close(infd);
    if (outfd >= 0)
	close(outfd);
    return rc;
}
