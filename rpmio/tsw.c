#include "system.h"
#include <rpmsw.h>
#include "debug.h"

int
main(int argc, char *argv[])
{
    struct rpmsw_s begin, end;
    rpmtime_t diff;
    int scale = 1000 * 1000;
    int nsecs = 5;

    diff = rpmswInit();

fprintf(stderr, "*** Sleeping for %d secs ... ", nsecs);
    (void) rpmswNow(&begin);
    sleep(nsecs);
    (void) rpmswNow(&end);

    diff = rpmswDiff(&end, &begin);
fprintf(stderr, "measured %u.%06u secs\n", (unsigned)diff/scale, (unsigned)diff%scale);

    return 0;

}
