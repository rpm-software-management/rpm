/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2001
 *	Sleepycat Software.  All rights reserved.
 *
 * Id: TestExceptInclude.cpp,v 1.1 2001/05/31 23:09:12 dda Exp 
 */

/* We should be able to include cxx_except.h without db_cxx.h,
 * and use the DbException class.
 *
 * This program does nothing, it's just here to make sure
 * the compilation works.
 */
#include "cxx_except.h"

int main(int argc, char *argv[])
{
	DbException *dbe = new DbException("something");
	DbMemoryException *dbme = new DbMemoryException("anything");

	dbe = dbme;
}

