/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2001
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: cxx_dbt.cpp,v 11.49 2001/07/28 20:01:18 dda Exp ";
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

Dbt::Dbt()
{
	DBT *dbt = this;
	memset(dbt, 0, sizeof(DBT));
}

Dbt::Dbt(void *data_arg, size_t size_arg)
{
	DBT *dbt = this;
	memset(dbt, 0, sizeof(DBT));
	set_data(data_arg);
	set_size(size_arg);
}

Dbt::~Dbt()
{
}

Dbt::Dbt(const Dbt &that)
{
	const DBT *from = &that;
	DBT *to = this;
	memcpy(to, from, sizeof(DBT));
}

Dbt &Dbt::operator = (const Dbt &that)
{
	if (this != &that) {
		const DBT *from = &that;
		DBT *to = this;
		memcpy(to, from, sizeof(DBT));
	}
	return (*this);
}
