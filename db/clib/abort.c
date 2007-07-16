/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: abort.c,v 1.3 2006/09/07 14:12:59 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * abort --
 *
 * PUBLIC: #ifndef HAVE_ABORT
 * PUBLIC: void abort __P((void));
 * PUBLIC: #endif
 */
void
abort()
{
#ifdef SIGABRT
	raise(SIGABRT);			/* Try and drop core. */
#endif
	exit(1);
	/* NOTREACHED */
}
