/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_open.c,v 1.5 2007/05/17 15:15:47 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_open --
 *	Open a file descriptor (including page size and log size information).
 */
int
__os_open(dbenv, name, page_size, flags, mode, fhpp)
	DB_ENV *dbenv;
	const char *name;
	u_int32_t page_size, flags;
	int mode;
	DB_FH **fhpp;
{
	OpenFileMode oflags;
	int ret;

	COMPQUIET(page_size, 0);

#define	OKFLAGS								\
	(DB_OSO_ABSMODE | DB_OSO_CREATE | DB_OSO_DIRECT | DB_OSO_DSYNC |\
	DB_OSO_EXCL | DB_OSO_RDONLY | DB_OSO_REGION |	DB_OSO_SEQ |	\
	DB_OSO_TEMP | DB_OSO_TRUNC)
	if ((ret = __db_fchk(dbenv, "__os_open", flags, OKFLAGS)) != 0)
		return (ret);

	oflags = 0;
	if (LF_ISSET(DB_OSO_CREATE))
		oflags |= _OFM_CREATE;

	if (LF_ISSET(DB_OSO_RDONLY))
		oflags |= _OFM_READ;
	else
		oflags |= _OFM_READWRITE;

	return (__os_openhandle(dbenv, name, oflags, mode, fhpp));
}
