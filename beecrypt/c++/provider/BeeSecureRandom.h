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

/*!\file BeeSecureRandom.h
 * \ingroup CXX_PROVIDER_m
 */

#ifndef _CLASS_BEESECURERANDOM_H
#define _CLASS_BEESECURERANDOM_H

#include "beecrypt/beecrypt.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/SecureRandomSpi.h"
using beecrypt::security::SecureRandomSpi;

namespace beecrypt {
	namespace provider {
		class BeeSecureRandom : public SecureRandomSpi
		{
		private:
			randomGeneratorContext _rngc;

		protected:
			BeeSecureRandom(const randomGenerator*);

		private:
			static SecureRandomSpi* create();

			virtual void engineGenerateSeed(byte*, size_t);
			virtual void engineNextBytes(byte*, size_t);
			virtual void engineSetSeed(const byte*, size_t);

		public:
			BeeSecureRandom();
			virtual ~BeeSecureRandom();
		};
	}
}

#endif

#endif
