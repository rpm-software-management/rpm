m4_ignore([dnl
#include <sys/types.h>
#include <stdlib.h>

#include <db.h>

int
main()
{
	DB *dbp;
	DBT key, data;
	int ret;
])
m4_indent([dnl
switch (ret = dbp-__GT__put(dbp, NULL, &key, &data, 0)) {
case 0:
	printf("db: %s: key stored.\n", (char *)key.data);
	break;
default:
	dbp-__GT__err(dbp, ret, "dbp-__GT__put");
	exit (1);
}])
m4_ignore([dnl
return (0);
}])
