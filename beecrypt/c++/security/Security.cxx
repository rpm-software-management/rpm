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

#include "beecrypt/c++/resource.h"
#include "beecrypt/c++/security/Security.h"
#include "beecrypt/c++/io/FileInputStream.h"
using beecrypt::io::FileInputStream;

#include <iostream>
#include <unicode/ustream.h>

using namespace beecrypt::security;

namespace {
	const String KEYSTORE_DEFAULT_TYPE = UNICODE_STRING_SIMPLE("BEE");
}

bool Security::_init = false;
mutex Security::_lock;
Properties Security::_props;
Security::provider_vector Security::_providers;

/* Have to use lazy initialization here; static initialization doesn't work.
 * Initialization adds a provider, apparently in another copy of Security,
 * instead of where we would expect it.
 *
 * Don't dlclose the libraries or uninstall the providers. They'll
 * disappear when the program closes. Since this happens only once per
 * application which uses this library, that's acceptable.
 *
 * What we eventually need to do is the following:
 * - treat the beecrypt.conf file as a collection of Properties, loaded from
 *   file with loadProperties.
 * - get appropriate properties to do the initialization
 */

void Security::initialize()
{
	_lock.init();
	_lock.lock();
	_init = true;
	_lock.unlock();

	/* get the configuration file here and load providers */
	const char* path = getenv("BEECRYPT_CONF_FILE");

	FILE* props;

	if (path)
		props = fopen(path, "r");
	else
		props = fopen(BEECRYPT_CONF_FILE, "r");

	if (!props)
	{
		std::cerr << "couldn't open beecrypt configuration file" << std::endl;
	}
	else
	{
		FileInputStream fis(props);

		try
		{
			// load properties from fis
			_props.load(fis);

			for (int32_t index = 1; true; index++)
			{
				char num[32];

				sprintf(num, "provider.%d", index);

				String key(num);

				const String* value = _props.getProperty(key);

				if (value)
				{
					int32_t reqlen = value->extract(0, value->length(), (char*) 0, (const char*) 0);

					char* shared_library = new char[reqlen+1];

					value->extract(0, value->length(), shared_library, (const char*) 0);

					#if WIN32
					HANDLE handle = LoadLibraryEx(shared_library, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
					#elif HAVE_DLFCN_H
					void *handle = dlopen(shared_library, RTLD_NOW);
					#else
					# error
					#endif

					if (handle)
					{
						#if WIN32
						const Provider& (*inst)(void*) = (const Provider& (*)(void*)) GetProcAddress((HMODULE) handle, "provider_const_ref");
						#elif HAVE_PTHREAD_H
						const Provider& (*inst)(void*) = (const Provider& (*)(void*)) dlsym(handle, "provider_const_ref");
						#else
						# error
						#endif

						if (inst)
						{
							addProvider(inst(handle));
						}
						else
						{
							std::cerr << "library doesn't contain symbol provider_const_ref" << std::endl;
							#if HAVE_DLFCN_H
							std::cerr << "dlerror: " << dlerror() << std::endl;
							#endif
						}
					}
					else
					{
						std::cerr << "unable to open shared library " << shared_library << std::endl;
						#if HAVE_DLFCN_H
						std::cerr << "dlerror: " << dlerror() << std::endl;
						#endif
					}

					delete[] shared_library;
				}
				else
					break;
			}
		}
		catch (IOException)
		{
		}
	}
}

Security::spi::spi(void* cspi, const String& name, const Provider& prov) : cspi(cspi), name(name), prov(prov)
{
}

Security::spi* Security::getSpi(const String& name, const String& type) throw (NoSuchAlgorithmException)
{
	if (!_init)
		initialize();

	String afind = type + "." + name;
	String alias = "Alg.Alias." + type + "." + name;

	_lock.lock();
	for (size_t i = 0; i < _providers.size(); i++)
	{
		Provider::instantiator inst = 0;

		const Provider* p = _providers[i];

		if (p->getProperty(afind))
		{
			inst = p->getInstantiator(afind);
		}
		else
		{
			const String* alias_of = p->getProperty(alias);

			if (alias_of)
				inst = p->getInstantiator(*alias_of);
		}

		if (inst)
		{
			register spi* result = new spi(inst(), name, *p);
			_lock.unlock();
			return result;
		}
	}

	_lock.unlock();

	throw NoSuchAlgorithmException(name + " " + type + " not available");
}

Security::spi* Security::getSpi(const String& name, const String& type, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException)
{
	if (!_init)
		initialize();

	String afind = type + "." + name;
	String alias = "Alg.Alias." + type + "." + name;

	_lock.lock();
	for (size_t i = 0; i < _providers.size(); i++)
	{
		const Provider* p = _providers[i];

		if (p->getName() == provider)
		{
			Provider::instantiator inst = 0;

			const Provider* p = _providers[i];

			if (p->getProperty(afind))
			{
				inst = p->getInstantiator(afind);
			}
			else
			{
				const String* alias_of = p->getProperty(alias);

				if (alias_of)
					inst = p->getInstantiator(*alias_of);
			}

			if (inst)
			{
				register spi* result = new spi(inst(), name, *p);
				_lock.unlock();
				return result;
			}

			_lock.unlock();

			throw NoSuchAlgorithmException(name + " " + type + " not available");
		}
	}

	_lock.unlock();

	throw NoSuchProviderException(provider + " Provider not available");
}

Security::spi* Security::getSpi(const String& name, const String& type, const Provider& provider) throw (NoSuchAlgorithmException)
{
	if (!_init)
		initialize();

	String afind = type + "." + name;
	String alias = "Alg.Alias." + type + "." + name;

	Provider::instantiator inst = 0;
	
	if (provider.getProperty(afind))
	{
		inst = provider.getInstantiator(afind);
	}
	else
	{
		const String* alias_of = provider.getProperty(alias);

		if (alias_of)
			inst = provider.getInstantiator(*alias_of);
	}

	if (inst)
		return new spi(inst(), name, provider);

	throw NoSuchAlgorithmException(name + " " + type + " not available");
}

Security::spi* Security::getFirstSpi(const String& type)
{
	if (!_init)
		initialize();

	String afind = type + ".";

	for (size_t i = 0; i < _providers.size(); i++)
	{
		const Provider* p = _providers[i];

		Enumeration* e = p->propertyNames();

		while (e->hasMoreElements())
		{
			const String* s = (const String*) e->nextElement();

			if (s->startsWith(afind))
			{
				String name;

				name.setTo(*s, afind.length());

				Provider::instantiator inst = p->getInstantiator(*s);

				if (inst)
				{
					delete e;

					return new spi(inst(), name, *p);
				}
			}
		}

		delete e;
	}
	return 0;
}

const String& Security::getKeyStoreDefault()
{
	return *_props.getProperty("keystore.default", KEYSTORE_DEFAULT_TYPE);
}

int Security::addProvider(const Provider& provider)
{
	if (!_init)
		initialize();

	if (getProvider(provider.getName()))
		return -1;

	_lock.lock();

	size_t rc = (int) _providers.size();

	_providers.push_back(&provider);

	_lock.unlock();

	return rc;
}

int Security::insertProviderAt(const Provider& provider, size_t position)
{
	if (!_init)
		initialize();

	if (getProvider(provider.getName()))
		return -1;

	_lock.lock();

	size_t size = _providers.size();

	if (position > size || position <= 0)
		position = size+1;

	_providers.insert(_providers.begin()+position-1, &provider);

	_lock.unlock();

	return (int) position;
}

void Security::removeProvider(const String& name)
{
	if (!_init)
		initialize();

	_lock.lock();
	for (provider_vector_iterator it = _providers.begin(); it != _providers.end(); it++)
	{
		const Provider* p = *it;

		if (p->getName() == name)
		{
			_providers.erase(it);
			_lock.unlock();
			return;
		}
	}
	_lock.unlock();
}

const Security::provider_vector& Security::getProviders()
{
	if (!_init)
		initialize();

	return _providers;
}

const Provider* Security::getProvider(const String& name)
{
	if (!_init)
		initialize();

	for (size_t i = 0; i < _providers.size(); i++)
	{
		const Provider* tmp = _providers[i];

		if (tmp->getName() == name)
			return _providers[i];
	}

	return 0;
}
