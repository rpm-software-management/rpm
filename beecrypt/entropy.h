/*
 * Copyright (c) 1998, 1999, 2000, 2001, 2002 Virtual Unlimited B.V.
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

/*!\file entropy.h
 * \brief Entropy sources, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup ES_m ES_audio_m ES_dsp_m ES_random_m ES_urandom_m ES_tty_m
 */

#ifndef _ENTROPY_H
#define _ENTROPY_H

#include "beecrypt/beecrypt.h"

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
int entropy_provider_cleanup();

BEECRYPTAPI
int entropy_wavein(byte*, size_t);
BEECRYPTAPI
int entropy_console(byte*, size_t);
BEECRYPTAPI
int entropy_wincrypt(byte*, size_t);
#else
#if HAVE_DEV_AUDIO
int entropy_dev_audio  (byte*, size_t);
#endif
#if HAVE_DEV_DSP
int entropy_dev_dsp    (byte*, size_t);
#endif
#if HAVE_DEV_RANDOM
int entropy_dev_random (byte*, size_t);
#endif
#if HAVE_DEV_URANDOM
int entropy_dev_urandom(byte*, size_t);
#endif
#if HAVE_DEV_TTY
int entropy_dev_tty    (byte*, size_t);
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
