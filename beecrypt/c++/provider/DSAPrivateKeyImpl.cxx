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

#include "beecrypt/c++/resource.h"
#include "beecrypt/c++/provider/DSAPrivateKeyImpl.h"
#include "beecrypt/c++/provider/BeeKeyFactory.h"

using namespace beecrypt::provider;

DSAPrivateKeyImpl::DSAPrivateKeyImpl(const DSAPrivateKey& copy)
{
	_params = new DSAParameterSpec(copy.getParams());
	_x = copy.getX();
	_enc = 0;
}

DSAPrivateKeyImpl::DSAPrivateKeyImpl(const DSAParams& params, const mpnumber& x)
{
	_params = new DSAParameterSpec(params.getP(), params.getQ(), params.getG());
	_x = x;
	_enc = 0;
}

DSAPrivateKeyImpl::DSAPrivateKeyImpl(const dsaparam& params, const mpnumber& x)
{
	_params = new DSAParameterSpec(params.p, params.q, params.g);
	_x = x;
	_enc = 0;
}

DSAPrivateKeyImpl::DSAPrivateKeyImpl(const mpbarrett& p, const mpbarrett& q, const mpnumber& g, const mpnumber& x)
{
	_params = new DSAParameterSpec(p, q, g);
	_x = x;
	_enc = 0;
}

DSAPrivateKeyImpl::~DSAPrivateKeyImpl()
{
	delete _params;
	_x.wipe();
	if (_enc)
		delete _enc;
}

DSAPrivateKey* DSAPrivateKeyImpl::clone() const
{
	return new DSAPrivateKeyImpl(*this);
}

const DSAParams& DSAPrivateKeyImpl::getParams() const throw ()
{
	return *_params;
}

const mpnumber& DSAPrivateKeyImpl::getX() const throw ()
{
	return _x;
}

const bytearray* DSAPrivateKeyImpl::getEncoded() const
{
	if (!_enc)
		_enc = BeeKeyFactory::encode(*this);

	return _enc;
}

const String& DSAPrivateKeyImpl::getAlgorithm() const throw ()
{
	static const String ALGORITHM = UNICODE_STRING_SIMPLE("DSA");
	return ALGORITHM;
}

const String* DSAPrivateKeyImpl::getFormat() const throw ()
{
	static const String FORMAT = UNICODE_STRING_SIMPLE("BEE");
	return &FORMAT;
}
