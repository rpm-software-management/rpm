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

#include "beecrypt/c++/crypto/spec/DHParameterSpec.h"

using namespace beecrypt::crypto::spec;

DHParameterSpec::DHParameterSpec(const DHParams& copy)
{
	_p = copy.getP();
	_g = copy.getG();
	_l = copy.getL();
}

DHParameterSpec::DHParameterSpec(const mpbarrett& p, const mpnumber& g)
{
	_p = p;
	_g = g;
	_l = 0;
}

DHParameterSpec::DHParameterSpec(const mpbarrett& p, const mpnumber& g, size_t l)
{
	_p = p;
	_g = g;
	_l = l;
}

DHParameterSpec::~DHParameterSpec()
{
}

const mpbarrett& DHParameterSpec::getP() const throw ()
{
	return _p;
}

const mpnumber& DHParameterSpec::getG() const throw ()
{
	return _g;
}

size_t DHParameterSpec::getL() const throw ()
{
	return _l;
}
