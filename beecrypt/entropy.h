/*
 * entropy.h
 *
 * Entropy gathering routine(s) for pseudo-random generator initialization, header
 *
 * Copyright (c) 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

#ifndef _ENTROPY_H
#define _ENTROPY_H

#include "beecrypt.h"

#if WIN32
#include <Windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if WIN32
BEEDLLAPI
int entropy_provider_setup(HINSTANCE);
BEEDLLAPI
int entropy_provider_cleanup(void);

BEEDLLAPI
int entropy_wavein(uint32*, int);
BEEDLLAPI
int entropy_console(uint32*, int);
BEEDLLAPI
int entropy_wincrypt(uint32*, int);
#else
#if HAVE_DEV_AUDIO
int entropy_dev_audio (uint32* data, int size)
	/*@*/;
#endif
#if HAVE_DEV_DSP
int entropy_dev_dsp   (uint32* data, int size)
	/*@modifies data */;
#endif
#if HAVE_DEV_RANDOM
int entropy_dev_random(uint32* data, int size)
	/*@modifies data */;
#endif
#if HAVE_DEV_URANDOM
int entropy_dev_urandom(uint32* data, int size)
	/*@modifies data */;
#endif
#if HAVE_DEV_TTY
int entropy_dev_tty   (uint32* data, int size)
	/*@modifies data */;
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
