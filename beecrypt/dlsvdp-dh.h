/** \ingrooup DL_m DH_m
 * \file dlsvdp-dh.h
 *
 * Discrete Logarithm Secret Value Derivation Primitive - Diffie Hellman, header.
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

#ifndef _DLSVDP_DH_H
#define _DLSVDP_DH_H

#include "dldp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI
int dlsvdp_pDHSecret(const dldp_p* dp, const mp32number* x, const mp32number* y, mp32number* s)
	/*@modifies s */;

#ifdef __cplusplus
}
#endif

#endif
