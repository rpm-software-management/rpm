#include <stdio.h>
#include <sqlite3.h>

static const char *dbname = "P";

static const char * stmts = "\
create table tbl1(one varchar(10), two smallint);\n\
insert into tbl1 values('hello!',10);\n\
insert into tbl1 values('goodbye', 20);\n\
select * from tbl1;\n\
";

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  for(i = 0; i<argc; i++)
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  printf("\n");
  return 0;
}

static int sql_exec(sqlite3 *db, const char *stmt)
{
    char *zErrMsg = 0;
    int rc;

fprintf(stderr, "*** %s:\n%s", __FUNCTION__, stmts);
    rc = sqlite3_exec(db, stmts, callback, 0, &zErrMsg);
    if (rc != SQLITE_OK)
	fprintf(stderr, "SQL error: %s\n", zErrMsg);
    return rc;
}

int main(int argc, char **argv)
{
  sqlite3 *db;
  char *zErrMsg = 0;
  int rc;

  (void) unlink(dbname);
  rc = sqlite3_open(dbname, &db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    exit(1);
  }

  rc = sql_exec(db, stmts);

  sqlite3_close(db);

  return 0;
}
