/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: cxx_lock.cpp,v 12.4 2006/08/24 14:45:13 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

#include "db_cxx.h"
#include "dbinc/cxx_int.h"

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbLock                                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbLock::DbLock(DB_LOCK value)
:	lock_(value)
{
}

DbLock::DbLock()
{
	memset(&lock_, 0, sizeof(DB_LOCK));
}

DbLock::DbLock(const DbLock &that)
:	lock_(that.lock_)
{
}

DbLock &DbLock::operator = (const DbLock &that)
{
	lock_ = that.lock_;
	return (*this);
}
