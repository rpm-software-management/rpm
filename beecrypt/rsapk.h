/** \ingroup RSA_m
 * \file rsapk.h
 *
 * RSA Public Key, header.
 */

/*
 * Copyright (c) 2000 Virtual Unlimited B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _RSAPK_H
#define _RSAPK_H

#include "mp32barrett.h"

/**
 */
typedef struct
{
	mp32barrett n;
	mp32number e;
} rsapk;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI /*@unused@*/
int rsapkInit(rsapk* pk)
	/*@modifies pk @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int rsapkFree(rsapk* pk)
	/*@modifies pk @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int rsapkCopy(rsapk* dst, const rsapk* src)
	/*@modifies dst @*/;

#ifdef __cplusplus
}
#endif

#endif
