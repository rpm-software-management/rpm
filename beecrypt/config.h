/*
 * config.h
 *
 * Config.h generic config file
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

#ifndef _CONFIG_H
#define _CONFIG_H

#if defined(_WIN32) && !defined(WIN32)
# define WIN32 1
#endif


#if WIN32 && !__CYGWIN32__
# include "config.win.h"
# ifdef BEECRYPT_DLL_EXPORT
#  define BEEDLLAPI
# else
#  define BEEDLLAPI __declspec(dllimport)
# endif
/*typedef UINT8_TYPE	byte;*/
#else
# include "config.gnu.h"
# define BEEDLLAPI
typedef UINT8_TYPE	byte;
#endif

#ifndef ROTL32
# define ROTL32(x, s) (((x) << (s)) | ((x) >> (32 - (s))))
#endif
#ifndef ROTR32
# define ROTR32(x, s) (((x) >> (s)) | ((x) << (32 - (s))))
#endif

/*@-typeuse@*/
typedef INT8_TYPE	int8;
/*@=typeuse@*/
typedef INT16_TYPE	int16;
typedef INT32_TYPE	int32;
typedef INT64_TYPE	int64;

typedef UINT8_TYPE	uint8;
typedef UINT16_TYPE	uint16;
typedef UINT32_TYPE	uint32;
/*@-duplicatequals@*/
typedef UINT64_TYPE	uint64;
/*@=duplicatequals@*/

typedef INT8_TYPE	javabyte;
typedef INT16_TYPE	javashort;
typedef INT32_TYPE	javaint;
typedef INT64_TYPE	javalong;

typedef UINT16_TYPE	javachar;

typedef FLOAT4_TYPE	javafloat;
typedef DOUBLE8_TYPE	javadouble;

#endif
