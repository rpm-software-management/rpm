
MODULE = rpm		PACKAGE = rpmdb		PREFIX = rpmdb

void
rpmdbClose(db)
  rpmdb db

unsigned int
rpmdbFirst(db)
  rpmdb db
  CODE:
	RETVAL = rpmdbFirstRecNum(db);
  OUTPUT:
	RETVAL

unsigned int
rpmdbNext(db, last_rec)
  rpmdb db
  unsigned int last_rec
  CODE:
	RETVAL = rpmdbNextRecNum(db, last_rec);
  OUTPUT:
	RETVAL

Header
rpmdbRecord(db, rec)
  rpmdb db
  unsigned int rec
  CODE:
	RETVAL = rpmdbGetRecord(db, rec);
  OUTPUT:
	RETVAL

