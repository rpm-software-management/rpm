/** \ingreoup ES_m
 * \file entropy.h
 *
 * Entropy gathering routine(s) for pseudo-random generator initialization, header.
 */

/*
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
BEECRYPTAPI
int entropy_provider_setup(HINSTANCE);
BEECRYPTAPI
int entropy_provider_cleanup(void);

BEECRYPTAPI
int entropy_wavein(uint32* data, int size);
BEECRYPTAPI
int entropy_console(uint32* data, int size);
BEECRYPTAPI
int entropy_wincrypt(uint32* data, int size);
#else

#if HAVE_DEV_AUDIO
/** \ingroup ES_audio_m ES_m
 */
int entropy_dev_audio (uint32* data, int size)
	/*@globals fileSystem, internalState @*/
	/*@modifies data, fileSystem, internalState @*/;
#endif

#if HAVE_DEV_DSP
/** \ingroup ES_dsp_m ES_m
 */
int entropy_dev_dsp   (uint32* data, int size)
	/*@globals fileSystem, internalState @*/
	/*@modifies data, fileSystem, internalState @*/;
#endif

#if HAVE_DEV_RANDOM
/** \ingroup ES_random_m ES_m
 */
int entropy_dev_random(uint32* data, int size)
	/*@globals fileSystem, internalState @*/
	/*@modifies data, fileSystem, internalState @*/;
#endif

#if HAVE_DEV_URANDOM
/** \ingroup ES_urandom_m ES_m
 */
int entropy_dev_urandom(uint32* data, int size)
	/*@globals fileSystem, internalState @*/
	/*@modifies data, fileSystem, internalState @*/;
#endif

#if HAVE_DEV_TTY
/** \ingroup ES_tty_m ES_m
 */
int entropy_dev_tty   (uint32* data, int size)
	/*@globals fileSystem, internalState @*/
	/*@modifies data, fileSystem, internalState @*/;
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
