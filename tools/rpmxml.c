#include "system.h"
#include <rpmxp.h>
#include "debug.h"

int main(int argc, char ** argv)
{
    const char * fn = "time.xml";
    rpmxp xp;
    int ec = 0;

    xp = rpmxpNew(fn);
    rpmxpParseFile(xp);
    xp = rpmxpFree(xp);

    return ec;
}
