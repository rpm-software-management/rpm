#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "sqlite.c"

static int sql_query(sqlite3 *db, const char *cmd)
{
    SCP_t scp = scpNew();
    const char * zTail;
    int rc;
    int i;

fprintf(stderr, "*** %s:\n%s\n", __FUNCTION__, cmd);

#if 0
    rc = sqlite3_prepare(db, cmd, strlen(cmd), &scp->pStmt, &zTail);
    if (rc) fprintf(stderr, "prepare: %d %s\n", rc, sqlite3_errmsg(db));

    rc = sql_step(db, scp);
#else
    rc = sqlite3_get_table(db, cmd,
        &scp->av, &scp->nr, &scp->nc, &scp->pzErrmsg);
    if (rc) fprintf(stderr, "get_table: %d %s\n", rc, sqlite3_errmsg(db));

fprintf(stderr, "%s: DONE scp %p [%d:%d] av %p avlen %p nr %d nc %d\n", __FUNCTION__, scp, scp->ac, scp->nalloc, scp->av, scp->avlen, scp->nr, scp->nc);

    fprintf(stderr, "\tav[0] %p %s\n", scp->av[0], scp->av[0]);
    for (i = 1; i < scp->nr; i++) {
	fprintf(stderr, "\tav[%d] %p %d\n", i, scp->av[i], *(int *)scp->av[i]);
    }
#endif

    scp = scpReset(scp);

    return rc;
}

static const char *dbname = "/var/lib/rpm/Packages";
static const char *qcmd = "SELECT key from 'Packages';";

int main(int argc, char **argv)
{
  sqlite3 *db;
  int rc;

  rc = sqlite3_open(dbname, &db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    exit(1);
  }

  rc = sql_query(db, qcmd);

  sqlite3_close(db);

  return 0;
}
