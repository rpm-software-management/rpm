#include "system.h"

#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>

#include "lib/rpmplugins.h"
#include "lib/rpmchroot.h"

/* general plugin hooks */
rpmRC PLUGINHOOK_INIT_FUNC(rpmts ts, const char * name, const char * opts);
rpmRC PLUGINHOOK_CLEANUP_FUNC(void);

/* collection plugin hooks */
rpmRC PLUGINHOOK_OPENTE_FUNC(rpmte te);
rpmRC PLUGINHOOK_COLL_POST_ANY_FUNC(void);
rpmRC PLUGINHOOK_COLL_POST_ADD_FUNC(void);
rpmRC PLUGINHOOK_COLL_PRE_REMOVE_FUNC(void);

/* per transaction plugin hooks */
rpmRC PLUGINHOOK_TSM_PRE_FUNC(rpmts ts);
rpmRC PLUGINHOOK_TSM_POST_FUNC(rpmts ts);

/* per transaction element plugin hooks */
rpmRC PLUGINHOOK_PSM_PRE_FUNC(rpmte te);
rpmRC PLUGINHOOK_PSM_POST_FUNC(rpmte te);

/*per script plugin hooks */
rpmRC PLUGINHOOK_SCRIPT_SETUP_FUNC(char* path);
