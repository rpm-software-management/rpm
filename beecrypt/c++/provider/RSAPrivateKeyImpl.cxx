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

#include "beecrypt/c++/provider/RSAPrivateKeyImpl.h"
#include "beecrypt/c++/provider/BeeKeyFactory.h"

using namespace beecrypt::provider;

RSAPrivateKeyImpl::RSAPrivateKeyImpl(const RSAPrivateKey& copy)
{
	_n = copy.getModulus();
	_d = copy.getPrivateExponent();
	_enc = 0;
}

RSAPrivateKeyImpl::RSAPrivateKeyImpl(const mpbarrett& n, const mpnumber& d)
{
	_n = n;
	_d = d;
	_enc = 0;
}

RSAPrivateKeyImpl::~RSAPrivateKeyImpl()
{
	_d.wipe();
	if (_enc)
		delete _enc;
}

RSAPrivateKey* RSAPrivateKeyImpl::clone() const
{
	return new RSAPrivateKeyImpl(*this);
}

const mpbarrett& RSAPrivateKeyImpl::getModulus() const throw ()
{
	return _n;
}

const mpnumber& RSAPrivateKeyImpl::getPrivateExponent() const throw ()
{
	return _d;
}

const bytearray* RSAPrivateKeyImpl::getEncoded() const
{
	if (!_enc)
		_enc = BeeKeyFactory::encode(*this);

	return _enc;
}

const String& RSAPrivateKeyImpl::getAlgorithm() const throw ()
{
	static const String ALGORITHM = UNICODE_STRING_SIMPLE("RSA");
	return ALGORITHM;
}

const String* RSAPrivateKeyImpl::getFormat() const throw ()
{
	static const String FORMAT = UNICODE_STRING_SIMPLE("BEE");
	return &FORMAT;
}
