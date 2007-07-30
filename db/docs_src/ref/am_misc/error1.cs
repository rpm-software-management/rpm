m4_ignore([dnl
#include <sys/types.h>

#include <db.h>

int foo();

DB *dbp;
DBT key, data;

int
main()
{
	(void)foo();
	return (0);
}

int
foo()
{])
m4_indent([dnl
int ret;
if ((ret = dbp-__GT__put(dbp, NULL, &key, &data, 0)) != 0) {
	fprintf(stderr, "put failed: %s\n", db_strerror(ret));
	return (1);
}])
m4_ignore([dnl
	return (0);
}])
