/*
 * Copyright (c) 2004 Beeyond Software Holding BV
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
 */

/*!\file bstream.h
 * \brief C++ Object-to-stream output.
 * \author Bob Deblier <bob.deblier@telenet.be>
 */

#ifndef _BEECRYPT_STREAM_H
#define _BEECRYPT_STREAM_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include <iostream>
using std::cout;
using std::ostream;
using std::endl;

#include "beecrypt/c++/security/PublicKey.h"
using beecrypt::security::PublicKey;

namespace beecrypt {
	BEECRYPTCXXAPI
	ostream& operator<<(ostream& stream, const PublicKey&);
}

#endif

#endif
