#include "system.h"

#include <pthread.h>

#include <rpmsq.h>
#include <popt.h>

#include "debug.h"

static void *
other_thread(void *dat)
{
    const char * cmd = (const char *)dat;
    const char ** av = NULL;
    int ac = 0;
    int xx = poptParseArgvString(cmd, &ac, &av);
    if (xx)
	return ((void *)xx);
fprintf(stderr, "thread(%lu): pid %u %s\n", pthread_self(), getpid(), cmd);
    return ((void *) rpmsqExecve(av));
}

int main(int argc, char *argv[])
{
    pthread_t pth;
    const char * cmd = argv[1];

    if (cmd == NULL)
	cmd = "/bin/sleep 30";
    pthread_create(&pth, NULL, other_thread, (void *)cmd);
fprintf(stderr, "  main(%lu): pid %u other(%lu)\n", pthread_self(), getpid(), pth);

    sleep(2);
    pthread_cancel(pth);

    pthread_join(pth, NULL);
    return 0;
}
