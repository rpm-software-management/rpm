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
#include "beecrypt/c++/provider/DHPrivateKeyImpl.h"
#include "beecrypt/c++/provider/BeeKeyFactory.h"

using namespace beecrypt::provider;

DHPrivateKeyImpl::DHPrivateKeyImpl(const DHPrivateKey& copy)
{
	_params = new DHParameterSpec(copy.getParams());
	_x = copy.getX();
	_enc = 0;
}

DHPrivateKeyImpl::DHPrivateKeyImpl(const DHParams& params, const mpnumber& x)
{
	_params = new DHParameterSpec(params.getP(), params.getG(), params.getL());
	_x = x;
	_enc = 0;
}

DHPrivateKeyImpl::DHPrivateKeyImpl(const dhparam& params, const mpnumber& x)
{
	_params = new DHParameterSpec(params.p, params.g);
	_x = x;
	_enc = 0;
}

DHPrivateKeyImpl::DHPrivateKeyImpl(const mpbarrett& p, const mpnumber& g, const mpnumber& x)
{
	_params = new DHParameterSpec(p, g);
	_x = x;
	_enc = 0;
}

DHPrivateKeyImpl::~DHPrivateKeyImpl()
{
	delete _params;
	_x.wipe();
	if (_enc);
		delete _enc;
}

DHPrivateKey* DHPrivateKeyImpl::clone() const
{
	return new DHPrivateKeyImpl(*this);
}

const DHParams& DHPrivateKeyImpl::getParams() const throw ()
{
	return *_params;
}

const mpnumber& DHPrivateKeyImpl::getX() const throw ()
{
	return _x;
}

const bytearray* DHPrivateKeyImpl::getEncoded() const
{
	if (!_enc)
		_enc = BeeKeyFactory::encode(*this);

	return _enc;
}

const String& DHPrivateKeyImpl::getAlgorithm() const throw ()
{
	static const String ALGORITHM = UNICODE_STRING_SIMPLE("DH");
	return ALGORITHM;
}

const String* DHPrivateKeyImpl::getFormat() const throw ()
{
	static const String FORMAT = UNICODE_STRING_SIMPLE("BEE");
	return &FORMAT;
}
