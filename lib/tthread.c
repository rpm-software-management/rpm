#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>
#include "rpmlib.h"
#include "rpmts.h"
#include "rpmio.h"

extern int _psm_debug;

static void *other_notify(const void *h,
			  const rpmCallbackType what,
			  const unsigned long amount,
			  const unsigned long total,
			  fnpyKey key,
			  rpmCallbackData data)
{
  printf("notify %d %ld %ld\n", what, amount, total);

  if(what == RPMCALLBACK_INST_OPEN_FILE)
    return Fopen(key, "r");

  return NULL;
}

static void *
other_thread(void *dat)
{
  rpmts ts;
  int fd, err;
  FD_t fdt;
  Header h = NULL;

  rpmReadConfigFiles(NULL, NULL);
  ts = rpmtsCreate();
  assert(ts);

  rpmIncreaseVerbosity();
  rpmIncreaseVerbosity();

  fd = open(dat, O_RDONLY);
  assert(fd >= 0);
  fdt = fdDup(fd);
  rpmReadPackageFile(ts, fdt, "other_thread", &h);
  Fclose(fdt);
  close(fd);

#if 0
  err = rpmtsOpenDB(ts, O_RDWR);
  assert(!err);
#endif

  err = rpmtsAddInstallElement(ts, h, dat, 1, NULL);

  err = rpmtsSetNotifyCallback(ts, other_notify, NULL);
  assert(!err);

  err = rpmtsRun(ts, NULL, RPMPROB_FILTER_REPLACEPKG);
  if(err)
    printf("Run failed: %d\n", err);

#if 0
  err = rpmtsCloseDB(ts);
  assert(!err);
#endif

  return NULL;
}

int main(int argc, char *argv[])
{
  pthread_t pth;

  _psm_debug = 1;
  pthread_create(&pth, NULL, other_thread, argv[1]);

  pthread_join(pth, NULL);

  return 0;
}
