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

/*!\file adapter.h
 * \brief In-between layer for BeeCrypt C and C++ code.
 * \author Bob Deblier <bob.deblier@telenet.be>
 */

#ifndef _BEECRYPT_ADAPTER_H
#define _BEECRYPT_ADAPTER_H

#include "beecrypt/beecrypt.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/SecureRandom.h"
using beecrypt::security::SecureRandom;

namespace beecrypt {
	/*!\brief Class which transforms a SecureRandom generator into a randomGeneratorContext.
	*/
	struct BEECRYPTCXXAPI randomGeneratorContextAdapter : randomGeneratorContext
	{
		randomGeneratorContextAdapter(SecureRandom*);
	};
}

#endif

#endif
