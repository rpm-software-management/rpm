/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2001
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: cxx_logc.cpp,v 11.3 2001/11/08 06:18:08 mjc Exp ";
#endif /* not lint */

#include <errno.h>
#include <string.h>

#include "db_cxx.h"
#include "cxx_int.h"

#include "db_int.h"
#include "db_page.h"
#include "db_auto.h"
#include "crdel_auto.h"
#include "db_ext.h"
#include "common_ext.h"

// It's private, and should never be called,
// but some compilers need it resolved
//
DbLogc::~DbLogc()
{
}

// The name _flags prevents a name clash with __db_log_cursor::flags
int DbLogc::close(u_int32_t _flags)
{
	DB_LOGC *cursor = this;
	int err;

	if ((err = cursor->close(cursor, _flags)) != 0) {
		DB_ERROR("DbLogc::close", err, ON_ERROR_UNKNOWN);
		return (err);
	}
	return (0);
}

// The name _flags prevents a name clash with __db_log_cursor::flags
int DbLogc::get(DbLsn *lsn, Dbt *data, u_int32_t _flags)
{
	DB_LOGC *cursor = this;
	int err;

	if ((err = cursor->get(cursor, lsn, data, _flags)) != 0) {

		// DB_NOTFOUND is a "normal" returns,
		// so should not be thrown as an error
		//
		if (err != DB_NOTFOUND) {
			const char *name = "DbLogc::get";
			if (err == ENOMEM && DB_OVERFLOWED_DBT(data))
				DB_ERROR_DBT(name, data, ON_ERROR_UNKNOWN);
			else
				DB_ERROR(name, err, ON_ERROR_UNKNOWN);

			return (err);
		}
	}
	return (err);
}
