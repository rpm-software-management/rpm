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

#include "beecrypt/c++/util/Properties.h"
using beecrypt::util::Properties;
#include "beecrypt/c++/io/DataInputStream.h"
using beecrypt::io::DataInputStream;
#include "beecrypt/c++/io/PrintStream.h"
using beecrypt::io::PrintStream;

using namespace beecrypt::util;

Properties::PropEnum::PropEnum(const properties_map& _map) throw ()
{
	_it = _map.begin();
	_end = _map.end();
}

Properties::PropEnum::~PropEnum() throw ()
{
}

bool Properties::PropEnum::hasMoreElements() throw ()
{
	return _it != _end;
}

const void* Properties::PropEnum::nextElement() throw (NoSuchElementException)
{
	if (_it == _end)
		throw NoSuchElementException();

	return (const void*) &((_it++)->first);
}

Properties::Properties()
{
	_lock.init();
	defaults = 0;
}

Properties::Properties(const Properties& copy)
{
	_lock.init();
	/* copy every item in the map */
	_pmap = copy._pmap;
	defaults = copy.defaults;
}

Properties::Properties(const Properties* defaults) : defaults(defaults)
{
	_lock.init();
}

Properties::~Properties()
{
	_lock.destroy();
}

const String* Properties::getProperty(const String& key) const throw ()
{
	properties_map::const_iterator it = _pmap.find(key);

	if (it != _pmap.end())
		return &(it->second);
	else if (defaults)
		return defaults->getProperty(key);

	return 0;
}

const String* Properties::getProperty(const String& key, const String& defaultValue) const throw ()
{
	const String* result = getProperty(key);

	if (result)
		return result;
	else
		return &defaultValue;
}

void Properties::setProperty(const String& key, const String& value) throw ()
{
	_lock.lock();
	_pmap[key] = value;
	_lock.unlock();
}

Enumeration* Properties::propertyNames() const
{
	return new PropEnum(_pmap);
}

void Properties::load(InputStream& in) throw (IOException)
{
	String line;
	String key;
	String value;

	DataInputStream dis(in);

	_lock.lock();
	try
	{
		while (dis.available())
		{
			dis.readLine(line);

			if (line.indexOf((UChar) 0x23) != 0)
			{
				// more advanced parsing can come later
				// see if we can find an '=' somewhere inside the string
				int32_t eqidx = line.indexOf((UChar) 0x3D);
				if (eqidx >= 0)
				{
					// we can split the line into two parts
					key.setTo(line, 0, eqidx);
					value.setTo(line, eqidx+1);
					_pmap[key] = value;
				}
			}
			// else it's a comment line which we discard
		}
		_lock.unlock();
	}
	catch (IOException)
	{
		_lock.unlock();
		throw;
	}
}

void Properties::store(OutputStream& out, const String& header) throw (IOException)
{
	properties_map::const_iterator pit;
	PrintStream ps(out);

	ps.println("# " + header);

	_lock.lock();

	for (pit = _pmap.begin(); pit != _pmap.end(); ++pit)
	{
		ps.print(pit->first);
		ps.print((javachar) 0x3D);
		ps.println(pit->second);
	}

	_lock.unlock();
}
