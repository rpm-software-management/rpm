m4_ignore([dnl
#include <sys/types.h>
#include <string.h>

#include <db.h>

void	foo();
void	handle_error(int);

DB *dbp;
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
{
	struct student_record {
		char student_id__LB__4__RB__;
		char last_name__LB__15__RB__;
		char first_name__LB__15__RB__;
	};])

m4_indent([dnl
struct student_record s;
DBT data, key;
m4_blank
memset(&key, 0, sizeof(DBT));
memset(&data, 0, sizeof(DBT));
memset(&s, 0, sizeof(struct student_record));
key.data = "WC42";
key.size = 4;
memcpy(&s.student_id, "WC42", sizeof(s.student_id));
memcpy(&s.last_name, "Churchill      ", sizeof(s.last_name));
memcpy(&s.first_name, "Winston        ", sizeof(s.first_name));
data.data = &s;
data.size = sizeof(s);
if ((ret = dbp-__GT__put(dbp, txn, &key, &data, 0)) != 0)
	handle_error(ret);])
m4_ignore([}])
