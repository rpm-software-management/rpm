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
#include "beecrypt/c++/provider/DSAPublicKeyImpl.h"
#include "beecrypt/c++/provider/BeeKeyFactory.h"

using namespace beecrypt::provider;

DSAPublicKeyImpl::DSAPublicKeyImpl(const DSAPublicKey& copy)
{
	_params = new DSAParameterSpec(copy.getParams());
	_y = copy.getY();
	_enc = 0;
}

DSAPublicKeyImpl::DSAPublicKeyImpl(const DSAParams& params, const mpnumber& y)
{
	_params = new DSAParameterSpec(params.getP(), params.getQ(), params.getG());
	_y = y;
	_enc = 0;
}

DSAPublicKeyImpl::DSAPublicKeyImpl(const dsaparam& params, const mpnumber& y)
{
	_params = new DSAParameterSpec(params.p, params.q, params.g);
	_y = y;
	_enc = 0;
}

DSAPublicKeyImpl::DSAPublicKeyImpl(const mpbarrett& p, const mpbarrett& q, const mpnumber& g, const mpnumber& y)
{
	_params = new DSAParameterSpec(p, q, g);
	_y = y;
	_enc = 0;
}

DSAPublicKeyImpl::~DSAPublicKeyImpl()
{
	delete _params;
	if (_enc)
		delete _enc;
}

DSAPublicKey* DSAPublicKeyImpl::clone() const
{
	return new DSAPublicKeyImpl(*this);
}

const DSAParams& DSAPublicKeyImpl::getParams() const throw ()
{
	return *_params;
}

const mpnumber& DSAPublicKeyImpl::getY() const throw ()
{
	return _y;
}

const bytearray* DSAPublicKeyImpl::getEncoded() const
{
	if (!_enc)
		_enc = BeeKeyFactory::encode(*this);

	return _enc;
}

const String& DSAPublicKeyImpl::getAlgorithm() const throw ()
{
	static const String ALGORITHM = UNICODE_STRING_SIMPLE("DSA");
	return ALGORITHM;
}

const String* DSAPublicKeyImpl::getFormat() const throw ()
{
	static const String FORMAT = UNICODE_STRING_SIMPLE("BEE");
	return &FORMAT;
}
