/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2001
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: cxx_dbc.cpp,v 11.50 2001/09/29 15:48:05 dda Exp ";
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

// It's private, and should never be called, but VC4.0 needs it resolved
//
Dbc::~Dbc()
{
}

int Dbc::close()
{
	DBC *cursor = this;
	int err;

	if ((err = cursor->c_close(cursor)) != 0) {
		DB_ERROR("Dbc::close", err, ON_ERROR_UNKNOWN);
		return (err);
	}
	return (0);
}

int Dbc::count(db_recno_t *countp, u_int32_t flags_arg)
{
	DBC *cursor = this;
	int err;

	if ((err = cursor->c_count(cursor, countp, flags_arg)) != 0) {
		DB_ERROR("Db::count", err, ON_ERROR_UNKNOWN);
		return (err);
	}
	return (0);
}

int Dbc::del(u_int32_t flags_arg)
{
	DBC *cursor = this;
	int err;

	if ((err = cursor->c_del(cursor, flags_arg)) != 0) {

		// DB_KEYEMPTY is a "normal" return, so should not be
		// thrown as an error
		//
		if (err != DB_KEYEMPTY) {
			DB_ERROR("Dbc::del", err, ON_ERROR_UNKNOWN);
			return (err);
		}
	}
	return (err);
}

int Dbc::dup(Dbc** cursorp, u_int32_t flags_arg)
{
	DBC *cursor = this;
	DBC *new_cursor = 0;
	int err;

	if ((err = cursor->c_dup(cursor, &new_cursor, flags_arg)) != 0) {
		DB_ERROR("Dbc::dup", err, ON_ERROR_UNKNOWN);
		return (err);
	}

	// The following cast implies that Dbc can be no larger than DBC
	*cursorp = (Dbc*)new_cursor;
	return (0);
}

int Dbc::get(Dbt* key, Dbt *data, u_int32_t flags_arg)
{
	DBC *cursor = this;
	int err;

	if ((err = cursor->c_get(cursor, key, data, flags_arg)) != 0) {

		// DB_NOTFOUND and DB_KEYEMPTY are "normal" returns,
		// so should not be thrown as an error
		//
		if (err != DB_NOTFOUND && err != DB_KEYEMPTY) {
			const char *name = "Dbc::get";
			if (err == ENOMEM && DB_OVERFLOWED_DBT(key))
				DB_ERROR_DBT(name, key, ON_ERROR_UNKNOWN);
			else if (err == ENOMEM && DB_OVERFLOWED_DBT(data))
				DB_ERROR_DBT(name, data, ON_ERROR_UNKNOWN);
			else
				DB_ERROR(name, err, ON_ERROR_UNKNOWN);

			return (err);
		}
	}
	return (err);
}

int Dbc::pget(Dbt* key, Dbt *pkey, Dbt *data, u_int32_t flags_arg)
{
	DBC *cursor = this;
	int err;

	if ((err = cursor->c_pget(cursor, key, pkey, data, flags_arg)) != 0) {

		// DB_NOTFOUND and DB_KEYEMPTY are "normal" returns,
		// so should not be thrown as an error
		//
		if (err != DB_NOTFOUND && err != DB_KEYEMPTY) {
			const char *name = "Dbc::pget";
			if (err == ENOMEM && DB_OVERFLOWED_DBT(key))
				DB_ERROR_DBT(name, key, ON_ERROR_UNKNOWN);
			else if (err == ENOMEM && DB_OVERFLOWED_DBT(data))
				DB_ERROR_DBT(name, data, ON_ERROR_UNKNOWN);
			else
				DB_ERROR(name, err, ON_ERROR_UNKNOWN);

			return (err);
		}
	}
	return (err);
}

int Dbc::put(Dbt* key, Dbt *data, u_int32_t flags_arg)
{
	DBC *cursor = this;
	int err;

	if ((err = cursor->c_put(cursor, key, data, flags_arg)) != 0) {

		// DB_KEYEXIST is a "normal" return, so should not be
		// thrown as an error
		//
		if (err != DB_KEYEXIST) {
			DB_ERROR("Dbc::put", err, ON_ERROR_UNKNOWN);
			return (err);
		}
	}
	return (err);
}
