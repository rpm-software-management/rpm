/*-
 *  Copyright (c) 2004
 *  Sleepycat Software.  All rights reserved.
 *
 *  http://www.apache.org/licenses/LICENSE-2.0.txt
 * 
 *  authors: Thies C. Arntzen <thies@php.net>
 *           Sterling Hughes <sterling@php.net>
 *           George Schlossnagle <george@omniti.com>
 */

#ifndef MOD_DB4_EXPORT_H
#define MOD_DB4_EXPORT_H

#include "db.h"

int mod_db4_db_env_create(DB_ENV **dbenvp, u_int32_t flags);
int mod_db4_db_create(DB **dbp, DB_ENV *dbenv, u_int32_t flags);
void mod_db4_child_clean_request_shutdown();
void mod_db4_child_clean_process_shutdown();

#endif
