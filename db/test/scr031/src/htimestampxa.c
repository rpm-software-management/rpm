#include <sys/types.h>
#include <sys/time.h>

#include "htimestampxa.h"

void
GetTime(HTimestampData *ts)
{
	struct timeval timeNow;

	(void)gettimeofday(&timeNow, 0);
	ts->Sec = timeNow.tv_sec;
	ts->Usec = timeNow.tv_usec;
}
