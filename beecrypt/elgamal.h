/*
 * elgamal.h
 *
 * ElGamal signature scheme, header
 *
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

#ifndef _ELGAMAL_H
#define _ELGAMAL_H

#include "mp32number.h"
#include "mp32barrett.h"

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
void elgv1sign(const mp32barrett* p, const mp32barrett* n, const mp32number* g, randomGeneratorContext*, const mp32number* hm, const mp32number* x, mp32number* r, mp32number* s);
BEEDLLAPI
void elgv3sign(const mp32barrett* p, const mp32barrett* n, const mp32number* g, randomGeneratorContext*, const mp32number* hm, const mp32number* x, mp32number* r, mp32number* s);

BEEDLLAPI
int elgv1vrfy(const mp32barrett* p, const mp32barrett* n, const mp32number* g, const mp32number* hm, const mp32number* y, const mp32number* r, const mp32number* s);
BEEDLLAPI
int elgv3vrfy(const mp32barrett* p, const mp32barrett* n, const mp32number* g, const mp32number* hm, const mp32number* y, const mp32number* r, const mp32number* s);

#ifdef __cplusplus
}
#endif

#endif
