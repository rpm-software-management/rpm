#include "system.h"

#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>

#include "lib/collections.h"
#include "lib/rpmchroot.h"

rpmRC COLLHOOK_POST_ANY_FUNC(rpmts ts, const char * collname, const char * options);
rpmRC COLLHOOK_POST_ADD_FUNC(rpmts ts, const char * collname, const char * options);
rpmRC COLLHOOK_PRE_REMOVE_FUNC(rpmts ts, const char * collname, const char * options);
