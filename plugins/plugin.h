#include "system.h"

#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>

#include "lib/rpmplugins.h"
#include "lib/rpmchroot.h"

rpmRC PLUGINHOOK_INIT_FUNC(rpmts ts, const char * name, const char * opts);
rpmRC PLUGINHOOK_CLEANUP_FUNC(void);
rpmRC PLUGINHOOK_OPENTE_FUNC(rpmte te);
rpmRC PLUGINHOOK_COLL_POST_ANY_FUNC(void);
rpmRC PLUGINHOOK_COLL_POST_ADD_FUNC(void);
rpmRC PLUGINHOOK_COLL_PRE_REMOVE_FUNC(void);
