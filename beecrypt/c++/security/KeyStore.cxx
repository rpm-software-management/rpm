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

#include "beecrypt/c++/security/KeyStore.h"
#include "beecrypt/c++/security/Security.h"

using namespace beecrypt::security;

KeyStore::KeyStore(KeyStoreSpi* spi, const String& type, const Provider& provider)
{
	_kspi = spi;
	_type = type;
	_prov = &provider;
	_init = false;
}

KeyStore::~KeyStore()
{
	delete _kspi;
}

KeyStore* KeyStore::getInstance(const String& type) throw (KeyStoreException)
{
	try
	{
		Security::spi* tmp = Security::getSpi(type, "KeyStore");

		KeyStore* result = new KeyStore((KeyStoreSpi*) tmp->cspi, tmp->name, tmp->prov);

		delete tmp;

		return result;
	}
	catch (NoSuchAlgorithmException& ex)
	{
		throw KeyStoreException(ex.getMessage());
	}
}

KeyStore* KeyStore::getInstance(const String& type, const String& provider) throw (KeyStoreException, NoSuchProviderException)
{
	try
	{
		Security::spi* tmp = Security::getSpi(type, "KeyStore", provider);

		KeyStore* result = new KeyStore((KeyStoreSpi*) tmp->cspi, tmp->name, tmp->prov);

		delete tmp;

		return result;
	}
	catch (NoSuchAlgorithmException& ex)
	{
		throw KeyStoreException(ex.getMessage());
	}
}

KeyStore* KeyStore::getInstance(const String& type, const Provider& provider) throw (KeyStoreException)
{
	try
	{
		Security::spi* tmp = Security::getSpi(type, "KeyStore", provider);

		KeyStore* result = new KeyStore((KeyStoreSpi*) tmp->cspi, tmp->name, tmp->prov);

		delete tmp;

		return result;
	}
	catch (NoSuchAlgorithmException& ex)
	{
		throw KeyStoreException(ex.getMessage());
	}
}

const String& KeyStore::getDefaultType()
{
	return Security::getKeyStoreDefault();
}

Key* KeyStore::getKey(const String& alias, const array<javachar>& password) throw (KeyStoreException, NoSuchAlgorithmException, UnrecoverableKeyException)
{
	return _kspi->engineGetKey(alias, password);
}

void KeyStore::setKeyEntry(const String& alias, const bytearray& key, const vector<Certificate*>& chain) throw (KeyStoreException)
{
	_kspi->engineSetKeyEntry(alias, key, chain);
}

void KeyStore::setKeyEntry(const String& alias, const Key& key, const array<javachar>& password, const vector<Certificate*>& chain) throw (KeyStoreException)
{
	_kspi->engineSetKeyEntry(alias, key, password, chain);
}

Enumeration* KeyStore::aliases()
{
	if (!_init)
		throw KeyStoreException("Uninitialized keystore");

	return _kspi->engineAliases();
}

bool KeyStore::containsAlias(const String& alias) throw (KeyStoreException)
{
	if (!_init)
		throw KeyStoreException("Uninitialized keystore");

	return _kspi->engineContainsAlias(alias);
}

const Certificate* KeyStore::getCertificate(const String& alias) throw (KeyStoreException)
{
	if (!_init)
		throw KeyStoreException("Uninitialized keystore");

	return _kspi->engineGetCertificate(alias);
}

bool KeyStore::isCertificateEntry(const String& alias) throw (KeyStoreException)
{
	if (!_init)
		throw KeyStoreException("Uninitialized keystore");

	return _kspi->engineIsCertificateEntry(alias);
}

bool KeyStore::isKeyEntry(const String& alias) throw (KeyStoreException)
{
	if (!_init)
		throw KeyStoreException("Uninitialized keystore");

	return _kspi->engineIsKeyEntry(alias);
}

void KeyStore::load(InputStream* in, const array<javachar>* password) throw (IOException, NoSuchAlgorithmException, CertificateException)
{
	_kspi->engineLoad(in, password);

	_init = true;
}

size_t KeyStore::size() const throw (KeyStoreException)
{
	if (!_init)
		throw KeyStoreException("Uninitialized keystore");

	return _kspi->engineSize();
}

void KeyStore::store(OutputStream& out, const array<javachar>* password) throw (IOException, KeyStoreException, NoSuchAlgorithmException, CertificateException)
{
	if (!_init)
		throw KeyStoreException("Uninitialized keystore");

	_kspi->engineStore(out, password);
}

const String& KeyStore::getType() const throw ()
{
	return _type;
}

const Provider& KeyStore::getProvider() const throw ()
{
	return *_prov;
}
