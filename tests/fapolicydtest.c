#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "../plugins/fapolicyd.c"

/* Return success when a full nonblocking pipe fails without spinning. */
int main(void)
{
    struct fapolicyd_data state = {
	.fd = -1,
	.fifo_path = "test fifo",
    };
    char data[4096] = { 0 };
    int pipefd[2] = { -1, -1 };
    int rc = EXIT_FAILURE;
    ssize_t n;

    if (pipe(pipefd) < 0)
	goto exit;
    if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK) < 0)
	goto exit;

    state.fd = pipefd[1];
    do {
	n = write(state.fd, data, sizeof(data));
    } while (n >= 0);

    if (errno != EAGAIN)
	goto exit;

    alarm(2);
    for (int i = 0; i <= MAX_WRITE_ERROR_LOGS; i++) {
	if (write_fifo(&state, "x") != RPMRC_FAIL)
	    goto exit;
    }
    if (state.write_error_logs == MAX_WRITE_ERROR_LOGS)
	rc = EXIT_SUCCESS;

exit:
    alarm(0);
    if (pipefd[0] >= 0)
	close(pipefd[0]);
    if (pipefd[1] >= 0)
	close(pipefd[1]);
    return rc;
}
