/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: rep_common.h,v 12.2 2006/08/24 14:45:46 bostic Exp $
 */

typedef struct {
	int is_master;
	void *comm_infrastructure;
} APP_DATA;

int create_env __P((const char *progname, DB_ENV **));
int doloop __P((DB_ENV *, APP_DATA *));
int env_init __P((DB_ENV *, const char *));
void usage __P((const char *));
