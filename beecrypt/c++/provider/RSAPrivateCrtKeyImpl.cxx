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

#include "beecrypt/c++/provider/RSAPrivateCrtKeyImpl.h"
#include "beecrypt/c++/provider/BeeKeyFactory.h"

using namespace beecrypt::provider;

RSAPrivateCrtKeyImpl::RSAPrivateCrtKeyImpl(const RSAPrivateCrtKey& copy)
{
	_n = copy.getModulus();
	_e = copy.getPublicExponent();
	_d = copy.getPrivateExponent();
	_p = copy.getPrimeP();
	_q = copy.getPrimeQ();
	_dp = copy.getPrimeExponentP();
	_dq = copy.getPrimeExponentQ();
	_qi = copy.getCrtCoefficient();
	_enc = 0;
}

RSAPrivateCrtKeyImpl::RSAPrivateCrtKeyImpl(const mpbarrett& n, const mpnumber& e, const mpnumber& d, const mpbarrett& p, const mpbarrett& q, const mpnumber& dp, const mpnumber& dq, const mpnumber& qi)
{
	_n = n;
	_e = e;
	_d = d;
	_p = p;
	_q = q;
	_dp = dp;
	_dq = dq;
	_qi = qi;
	_enc = 0;
}

RSAPrivateCrtKeyImpl::~RSAPrivateCrtKeyImpl()
{
	_d.wipe();
	_p.wipe();
	_q.wipe();
	_dp.wipe();
	_dq.wipe();
	_qi.wipe();
	if (_enc)
		delete _enc;
}

RSAPrivateCrtKey* RSAPrivateCrtKeyImpl::clone() const
{
	return new RSAPrivateCrtKeyImpl(*this);
}

const mpbarrett& RSAPrivateCrtKeyImpl::getModulus() const throw ()
{
	return _n;
}

const mpnumber& RSAPrivateCrtKeyImpl::getPrivateExponent() const throw ()
{
	return _d;
}

const mpnumber& RSAPrivateCrtKeyImpl::getPublicExponent() const throw ()
{
	return _e;
}

const mpbarrett& RSAPrivateCrtKeyImpl::getPrimeP() const throw ()
{
	return _p;
}

const mpbarrett& RSAPrivateCrtKeyImpl::getPrimeQ() const throw ()
{
	return _q;
}

const mpnumber& RSAPrivateCrtKeyImpl::getPrimeExponentP() const throw ()
{
	return _dp;
}

const mpnumber& RSAPrivateCrtKeyImpl::getPrimeExponentQ() const throw ()
{
	return _dq;
}

const mpnumber& RSAPrivateCrtKeyImpl::getCrtCoefficient() const throw ()
{
	return _qi;
}

const bytearray* RSAPrivateCrtKeyImpl::getEncoded() const
{
	if (!_enc)
		_enc = BeeKeyFactory::encode(*this);

	return _enc;
}

const String& RSAPrivateCrtKeyImpl::getAlgorithm() const throw ()
{
	static const String ALGORITHM = UNICODE_STRING_SIMPLE("RSA");
	return ALGORITHM;
}

const String* RSAPrivateCrtKeyImpl::getFormat() const throw ()
{
	static const String FORMAT = UNICODE_STRING_SIMPLE("BEE");
	return &FORMAT;
}
