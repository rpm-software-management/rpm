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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/c++/security/spec/DSAPublicKeySpec.h"

using namespace beecrypt::security::spec;

DSAPublicKeySpec::DSAPublicKeySpec(const mpbarrett& p, const mpbarrett& q, const mpnumber& g, const mpnumber& y)
{
	_p = p;
	_q = q;
	_g = g;
	_y = y;
}

DSAPublicKeySpec::~DSAPublicKeySpec()
{
}

const mpbarrett& DSAPublicKeySpec::getP() const throw ()
{
	return _p;
}

const mpbarrett& DSAPublicKeySpec::getQ() const throw ()
{
	return _q;
}

const mpnumber& DSAPublicKeySpec::getG() const throw ()
{
	return _g;
}

const mpnumber& DSAPublicKeySpec::getY() const throw ()
{
	return _y;
}
