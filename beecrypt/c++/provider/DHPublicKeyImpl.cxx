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

#include "beecrypt/c++/provider/BeeKeyFactory.h"
#include "beecrypt/c++/provider/DHPublicKeyImpl.h"

using namespace beecrypt::provider;

DHPublicKeyImpl::DHPublicKeyImpl(const DHPublicKey& copy)
{
	_params = new DHParameterSpec(copy.getParams());
	_y = copy.getY();
	_enc = 0;
}

DHPublicKeyImpl::DHPublicKeyImpl(const DHParams& params, const mpnumber& y)
{
	_params = new DHParameterSpec(params.getP(), params.getG(), params.getL());
	_y = y;
	_enc = 0;
}

DHPublicKeyImpl::DHPublicKeyImpl(const dhparam& params, const mpnumber& y)
{
	_params = new DHParameterSpec(params.p, params.g);
	_y = y;
	_enc = 0;
}

DHPublicKeyImpl::DHPublicKeyImpl(const mpbarrett& p, const mpnumber& g, const mpnumber& y)
{
	_params = new DHParameterSpec(p, g);
	_y = y;
	_enc = 0;
}

DHPublicKeyImpl::~DHPublicKeyImpl()
{
	delete _params;
	if (_enc)
		delete _enc;
}

DHPublicKey* DHPublicKeyImpl::clone() const
{
	return new DHPublicKeyImpl(*this);
}

const DHParams& DHPublicKeyImpl::getParams() const throw ()
{
	return *_params;
}

const mpnumber& DHPublicKeyImpl::getY() const throw ()
{
	return _y;
}

const bytearray* DHPublicKeyImpl::getEncoded() const
{
	if (!_enc)
		_enc = BeeKeyFactory::encode(*this);

    return _enc;
}

const String& DHPublicKeyImpl::getAlgorithm() const throw ()
{
	static const String ALGORITHM = UNICODE_STRING_SIMPLE("DH");
	return ALGORITHM;
}

const String* DHPublicKeyImpl::getFormat() const throw ()
{
	static const String FORMAT = UNICODE_STRING_SIMPLE("BEE");
	return &FORMAT;
}
