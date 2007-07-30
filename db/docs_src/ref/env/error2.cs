m4_ignore([dnl
#include <sys/types.h>

#include <db.h>

int
main()
{
	DB_ENV *dbenv;
	int session_id;
	char *home, *program_name;
])
m4_indent([dnl
int ret;
dbenv-__GT__set_errpfx(dbenv, program_name);
if ((ret = dbenv-__GT__open(dbenv, home,
    DB_CREATE | DB_INIT_LOG | DB_INIT_TXN | DB_USE_ENVIRON, 0))
    != 0) {
	dbenv-__GT__err(dbenv, ret, "open: %s", home);
	dbenv-__GT__errx(dbenv,
	    "contact your system administrator: session ID was %d",
	    session_id);
	return (1);
}])
m4_ignore([dnl
return (0);
}])
