/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2003
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: cxx_multi.cpp,v 1.3 2003/02/22 01:25:43 merrells Exp $";
#endif /* not lint */

#include "db_cxx.h"

DbMultipleIterator::DbMultipleIterator(const Dbt &dbt)
 : data_((u_int8_t*)dbt.get_data()),
   p_((u_int32_t*)(data_ + dbt.get_size() - sizeof(u_int32_t)))
{
}

bool DbMultipleDataIterator::next(Dbt &data)
{
	if (*p_ == (u_int32_t)-1) {
		data.set_data(0);
		data.set_size(0);
		p_ = 0;
	} else {
		data.set_data(data_ + *p_--);
		data.set_size(*p_--);
		if (data.get_size() == 0 && data.get_data() == data_)
			data.set_data(0);
	}
	return (data.get_data() != 0);
}

bool DbMultipleKeyDataIterator::next(Dbt &key, Dbt &data)
{
	if (*p_ == (u_int32_t)-1) {
		key.set_data(0);
		key.set_size(0);
		data.set_data(0);
		data.set_size(0);
		p_ = 0;
	} else {
		key.set_data(data_ + *p_--);
		key.set_size(*p_--);
		data.set_data(data_ + *p_--);
		data.set_size(*p_--);
	}
	return (data.get_data() != 0);
}

bool DbMultipleRecnoDataIterator::next(db_recno_t &recno, Dbt &data)
{
	if (*p_ == (u_int32_t)0) {
		recno = 0;
		data.set_data(0);
		data.set_size(0);
		p_ = 0;
	} else {
		recno = *p_--;
		data.set_data(data_ + *p_--);
		data.set_size(*p_--);
	}
	return (recno != 0);
}
