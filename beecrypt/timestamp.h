/*
 * timestamp.h
 *
 * Java-compatible 64 bit timestamp, header
 *
 * Copyright (c) 1999-2000 Virtual Unlimited B.V.
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

#ifndef _TIMESTAMP_H
#define _TIMESTAMP_H

#include "beecrypt.h"

#define ONE_SECOND	1000LL
#define ONE_MINUTE	60000LL
#define ONE_HOUR	3600000LL
#define ONE_DAY		86400000LL

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
javalong timestamp();

#ifdef __cplusplus
}
#endif

#endif
