m4_ignore([dnl
#include <sys/types.h>
#include <string.h>

#include <db.h>

void	foo();
void	handle_error(int);

DB *sdbp;
DB_TXN *txn;
int ret;

int
main()
{
	foo();
	return (0);
}

void
handle_error(ret)
	int ret;
{}

void
foo()
{])

m4_indent([dnl
DBT data, pkey, skey;
m4_blank
memset(&skey, 0, sizeof(DBT));
memset(&pkey, 0, sizeof(DBT));
memset(&data, 0, sizeof(DBT));
skey.data = "Churchill      ";
skey.size = 15;
if ((ret = sdbp-__GT__pget(sdbp, txn, &skey, &pkey, &data, 0)) != 0)
	handle_error(ret);
/*
 * Now pkey contains "WC42" and data contains Winston's record.
 */])
m4_ignore([}])
