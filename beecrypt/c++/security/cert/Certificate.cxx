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

#include "beecrypt/c++/security/cert/Certificate.h"

using namespace beecrypt::security::cert;

Certificate::Certificate(const String& type)
{
	_type = type;
}

Certificate::~Certificate()
{
}

bool Certificate::operator==(const Certificate& cmp) const
{
	if (this == &cmp)
		return true;

	if (_type != cmp._type)
		return false;

	if (getEncoded() != cmp.getEncoded())
		return false;

	return true;
}

const String& Certificate::getType() const throw ()
{
	return _type;
}
