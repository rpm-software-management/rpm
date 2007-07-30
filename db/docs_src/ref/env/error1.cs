m4_ignore([dnl
#include <sys/types.h>

#include <db.h>

int
main()
{
	DB_ENV *dbenv;
])
m4_indent([dnl
int ret;
if ((ret = dbenv-__GT__set_cachesize(dbenv, 0, 32 * 1024, 1)) != 0) {
	fprintf(stderr, "set_cachesize failed: %s\n", db_strerror(ret));
	return (1);
}])
m4_ignore([dnl
return (0);
}])
