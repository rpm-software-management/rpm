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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/c++/provider/BeeSecureRandom.h"

using namespace beecrypt::provider;

BeeSecureRandom::BeeSecureRandom()
{
}

BeeSecureRandom::BeeSecureRandom(const randomGenerator* rng) : _rngc(rng)
{
}

BeeSecureRandom::~BeeSecureRandom()
{
}

SecureRandomSpi* BeeSecureRandom::create()
{
	return new BeeSecureRandom();
}

void BeeSecureRandom::engineGenerateSeed(byte* data, size_t size)
{
	entropyGatherNext(data, size);
}

void BeeSecureRandom::engineNextBytes(byte* data, size_t size)
{
	randomGeneratorContextNext(&_rngc, data, size);
}

void BeeSecureRandom::engineSetSeed(const byte* data, size_t size)
{
	randomGeneratorContextSeed(&_rngc, data, size);
}
