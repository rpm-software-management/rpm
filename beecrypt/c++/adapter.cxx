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

#define BEECRYPT_CXX_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/c++/adapter.h"

using namespace beecrypt;

int sraSetup(SecureRandom* random)
{
        return 0;
}

int sraSeed(SecureRandom* random, const byte* data, size_t size)
{
	random->setSeed(data, size);
	return 0;
}

int sraNext(SecureRandom* random, byte* data, size_t size)
{
	random->nextBytes(data, size);
	return 0;
}

int sraCleanup(SecureRandom* random)
{
	return 0;
}

const randomGenerator sraprng = {
	"SecureRandom Adapter",
	0,
	(randomGeneratorSetup) sraSetup,
	(randomGeneratorSeed) sraSeed,
	(randomGeneratorNext) sraNext,
	(randomGeneratorCleanup) sraCleanup
};

randomGeneratorContextAdapter::randomGeneratorContextAdapter(SecureRandom* random) : randomGeneratorContext(&sraprng)
{
	param = (randomGeneratorParam*) random;
}

// SecureRandom systemsr;
