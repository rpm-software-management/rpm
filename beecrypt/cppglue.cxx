/*
 * Copyright (c) 2004 Bob Deblier
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

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/beecrypt.h"
#include "beecrypt/mpnumber.h"
#include "beecrypt/mpbarrett.h"
#include "beecrypt/dldp.h"
#include "beecrypt/dlkp.h"
#include "beecrypt/dlpk.h"
#include "beecrypt/rsakp.h"
#include "beecrypt/rsapk.h"

#include <iomanip>

#if CPPGLUE

mpnumber::mpnumber()
{
	mpnzero(this);
}

mpnumber::mpnumber(unsigned int value)
{
	mpnsize(this, 1);
	mpnsetw(this, value);
}

mpnumber::mpnumber(const mpnumber& copy)
{
	mpnzero(this);
	mpncopy(this, &copy);
}

mpnumber::~mpnumber()
{
	mpnfree(this);
}

const mpnumber& mpnumber::operator=(const mpnumber& copy)
{
	mpncopy(this, &copy);
	return *this;
}

bool mpnumber::operator==(const mpnumber& cmp)
{
	return mpeqx(size, data, cmp.size, cmp.data);
}

bool mpnumber::operator!=(const mpnumber& cmp)
{
	return mpnex(size, data, cmp.size, cmp.data);
}

void mpnumber::wipe()
{
	mpnwipe(this);
}

size_t mpnumber::bitlength() const
{
	return mpbits(size, data);
}

std::ostream& operator<<(std::ostream& stream, const mpnumber& n)
{
	if (n.size)
	{
		stream << std::hex << std::setfill('0') << n.data[0];
		for (size_t i = 1; i < n.size; i++)
			stream << std::setw(MP_WNIBBLES) << n.data[i];
	}

	return stream;
}

/*
std::istream& operator>>(std:istream& stream, mpnumber& n)
{
}
*/

mpbarrett::mpbarrett()
{
	mpbzero(this);
}

mpbarrett::mpbarrett(const mpbarrett& copy)
{
	mpbzero(this);
	mpbcopy(this, &copy);
}

mpbarrett::~mpbarrett()
{
	mpbfree(this);
}

const mpbarrett& mpbarrett::operator=(const mpbarrett& copy)
{
	mpbcopy(this, &copy);
	return *this;
}

bool mpbarrett::operator==(const mpbarrett& cmp)
{
	return mpeqx(size, modl, cmp.size, cmp.modl);
}

bool mpbarrett::operator!=(const mpbarrett& cmp)
{
	return mpnex(size, modl, cmp.size, cmp.modl);
}

void mpbarrett::wipe()
{
	mpbwipe(this);
}

size_t mpbarrett::bitlength() const
{
	return mpbits(size, modl);
}

std::ostream& operator<<(std::ostream& stream, const mpbarrett& b)
{
	stream << std::hex << std::setfill('0');

	for (size_t i = 0; i < b.size; i++)
		stream << std::setw(MP_WNIBBLES) << b.modl[i];

	return stream;
}

dldp_p::dldp_p()
{
	dldp_pInit(this);
}

dldp_p::dldp_p(const dldp_p& copy)
{
	dldp_pInit(this);
	dldp_pCopy(this, &copy);
}

dldp_p::~dldp_p()
{
	dldp_pFree(this);
}

dlkp_p::dlkp_p()
{
	dlkp_pInit(this);
}

dlkp_p::dlkp_p(const dlkp_p& copy)
{
	dlkp_pInit(this);
	dlkp_pCopy(this, &copy);
}

dlkp_p::~dlkp_p()
{
	dlkp_pFree(this);
}

dlpk_p::dlpk_p()
{
	dlpk_pInit(this);
}

dlpk_p::dlpk_p(const dlpk_p& copy)
{
	dlpk_pInit(this);
	dlpk_pCopy(this, &copy);
}

dlpk_p::~dlpk_p()
{
	dlpk_pFree(this);
}

rsakp::rsakp()
{
	rsakpInit(this);
}

rsakp::rsakp(const rsakp& copy)
{
	rsakpInit(this);
	rsakpCopy(this, &copy);
}

rsakp::~rsakp()
{
	rsakpFree(this);
}

rsapk::rsapk()
{
	rsapkInit(this);
}

rsapk::rsapk(const rsapk& copy)
{
	rsapkInit(this);
	rsapkCopy(this, &copy);
}

rsapk::~rsapk()
{
	rsapkFree(this);
}

blockCipherContext::blockCipherContext()
{
	blockCipherContextInit(this, blockCipherDefault());
}

blockCipherContext::blockCipherContext(const blockCipher* b)
{
	blockCipherContextInit(this, b);
}

blockCipherContext::~blockCipherContext()
{
	blockCipherContextFree(this);
}

hashFunctionContext::hashFunctionContext()
{
	hashFunctionContextInit(this, hashFunctionDefault());
}

hashFunctionContext::hashFunctionContext(const hashFunction* h)
{
	hashFunctionContextInit(this, h);
}

hashFunctionContext::~hashFunctionContext()
{
	hashFunctionContextFree(this);
}

keyedHashFunctionContext::keyedHashFunctionContext()
{
	keyedHashFunctionContextInit(this, keyedHashFunctionDefault());
}

keyedHashFunctionContext::keyedHashFunctionContext(const keyedHashFunction* k)
{
	keyedHashFunctionContextInit(this, k);
}

keyedHashFunctionContext::~keyedHashFunctionContext()
{
	keyedHashFunctionContextFree(this);
}

randomGeneratorContext::randomGeneratorContext()
{
	randomGeneratorContextInit(this, randomGeneratorDefault());
}

randomGeneratorContext::randomGeneratorContext(const randomGenerator* rng)
{
	randomGeneratorContextInit(this, rng);
}

randomGeneratorContext::~randomGeneratorContext()
{
	randomGeneratorContextFree(this);
}

#endif
