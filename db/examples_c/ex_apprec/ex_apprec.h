/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: ex_apprec.h,v 12.5 2007/05/17 15:15:13 bostic Exp $
 */

#ifndef _EX_APPREC_H_
#define	_EX_APPREC_H_

#include "ex_apprec_auto.h"

int ex_apprec_mkdir_log
    __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, const DBT *));
int ex_apprec_mkdir_print
    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int ex_apprec_mkdir_read
    __P((DB_ENV *, void *, ex_apprec_mkdir_args **));
int ex_apprec_mkdir_recover
    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int ex_apprec_init_print __P((DB_ENV *,
    int (***)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *), size_t *));

#endif /* !_EX_APPREC_H_ */
