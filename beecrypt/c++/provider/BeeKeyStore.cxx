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

#include "beecrypt/aes.h"
#include "beecrypt/pkcs12.h"
#include "beecrypt/sha256.h"

#include "beecrypt/c++/crypto/Mac.h"
using beecrypt::crypto::Mac;
#include "beecrypt/c++/io/ByteArrayInputStream.h"
using beecrypt::io::ByteArrayInputStream;
#include "beecrypt/c++/io/DataInputStream.h"
using beecrypt::io::DataInputStream;
#include "beecrypt/c++/io/DataOutputStream.h"
using beecrypt::io::DataOutputStream;
#include "beecrypt/c++/crypto/MacInputStream.h"
using beecrypt::crypto::MacInputStream;
#include "beecrypt/c++/crypto/MacOutputStream.h"
using beecrypt::crypto::MacOutputStream;
#include "beecrypt/c++/security/SecureRandom.h"
using beecrypt::security::SecureRandom;
#include "beecrypt/c++/beeyond/PKCS12PBEKey.h"
using beecrypt::beeyond::PKCS12PBEKey;
#include "beecrypt/c++/provider/KeyProtector.h"
using beecrypt::provider::KeyProtector;
#include "beecrypt/c++/provider/BeeKeyStore.h"

using namespace beecrypt::provider;

namespace {
	const array<javachar> EMPTY_PASSWORD;
}

#define BKS_MAGIC				((javaint) 0xbeecceec)
#define BKS_VERSION_1			((javaint) 0x1)
#define BKS_PRIVATEKEY_ENTRY	((javaint) 0x1)
#define BKS_CERTIFICATE_ENTRY	((javaint) 0x2)

BeeKeyStore::Entry::~Entry()
{
}

BeeKeyStore::KeyEntry::KeyEntry()
{
}

BeeKeyStore::KeyEntry::KeyEntry(const bytearray& b, const vector<Certificate*>& c)
{
	encryptedkey = b;
	for (vector<Certificate*>::const_iterator it = c.begin(); it != c.end(); it++)
		chain.push_back((*it)->clone());
}

BeeKeyStore::KeyEntry::~KeyEntry()
{
	// delete all the certificates in the chain
	for (size_t i = 0; i < chain.size(); i++)
		delete chain[i];
}

BeeKeyStore::CertEntry::CertEntry()
{
	cert = 0;
}

BeeKeyStore::CertEntry::CertEntry(const Certificate& c)
{
	cert = c.clone();
}

BeeKeyStore::CertEntry::~CertEntry()
{
	if (cert)
	{
		delete cert;
		cert = 0;
	}
}

BeeKeyStore::BeeKeyStore()
{
	_lock.init();
}

BeeKeyStore::~BeeKeyStore()
{
	_lock.lock();
	clearall();
	_lock.unlock();
	_lock.destroy();
}

BeeKeyStore::AliasEnum::AliasEnum(const BeeKeyStore::entry_map& map)
{
	_it = map.begin();
	_end = map.end();
}

BeeKeyStore::AliasEnum::~AliasEnum() throw ()
{
}

bool BeeKeyStore::AliasEnum::hasMoreElements() throw ()
{
	return _it != _end;
}

const void* BeeKeyStore::AliasEnum::nextElement() throw (NoSuchElementException)
{
	if (_it == _end)
		throw NoSuchElementException();

	return (const void*) &((_it++)->first);
}

void BeeKeyStore::clearall()
{
	keyfactory_map::iterator kit = _keyfactories.begin();
	while (kit != _keyfactories.end())
	{
		delete kit->second;
		_keyfactories.erase(kit++);
	}

	certfactory_map::iterator cit = _certfactories.begin();
	while (cit != _certfactories.end())
	{
		delete cit->second;
		_certfactories.erase(cit++);
	}

	entry_map::iterator eit = _entries.begin();
	while (eit != _entries.end())
	{
		delete eit->second;
		_entries.erase(eit++);
	}
}

Enumeration* BeeKeyStore::engineAliases()
{
	return new AliasEnum(_entries);
}

bool BeeKeyStore::engineContainsAlias(const String& alias)
{
	return (_entries[alias] != 0);
}

void BeeKeyStore::engineDeleteEntry(const String& alias) throw (KeyStoreException)
{
	_lock.lock();
	entry_map::iterator it = _entries.find(alias);

	if (it != _entries.end())
	{
		delete it->second;
		_entries.erase(it);
	}
	_lock.unlock();
}

const Date* BeeKeyStore::engineGetCreationDate(const String& alias)
{
	const Date* result = 0;

	_lock.lock();
	entry_map::iterator it = _entries.find(alias);
	if (it != _entries.end())
		result =  &(it->second->date);
	_lock.unlock();
	return result;
}

const Certificate* BeeKeyStore::engineGetCertificate(const String& alias)
{
	const Certificate* result = 0;

	_lock.lock();
	entry_map::iterator it = _entries.find(alias);
	if (it != _entries.end())
	{
		CertEntry* ce = dynamic_cast<CertEntry*>(it->second);
		if (ce)
			result =  ce->cert;
	}
	_lock.unlock();
	return result;
}

const String* BeeKeyStore::engineGetCertificateAlias(const Certificate& cert)
{
	const String* result = 0;

	_lock.lock();
	for (entry_map::const_iterator it = _entries.begin(); it != _entries.end(); ++it)
	{
		const CertEntry* ce = dynamic_cast<const CertEntry*>(it->second);
		if (ce)
		{
			if (cert == *(ce->cert))
			{
				result = &(it->first);
				break;
			}
		}
	}
	_lock.unlock();
	return result;
}

const vector<Certificate*>* BeeKeyStore::engineGetCertificateChain(const String& alias)
{
	const vector<Certificate*>* result = 0;

	_lock.unlock();
	entry_map::iterator it = _entries.find(alias);
	if (it != _entries.end())
	{
		KeyEntry* ke = dynamic_cast<KeyEntry*>(it->second);
		if (ke)
			result = &ke->chain;
	}
	_lock.unlock();
	return result;
}

bool BeeKeyStore::engineIsCertificateEntry(const String& alias)
{
	bool result = false;
	_lock.lock();
	entry_map::iterator it = _entries.find(alias);
	if (it != _entries.end())
		result = (dynamic_cast<CertEntry*>(it->second) != 0);
	_lock.unlock();
	return result;
}

void BeeKeyStore::engineSetCertificateEntry(const String& alias, const Certificate& cert) throw (KeyStoreException)
{
	_entries[alias] = new CertEntry(cert);
}

Key* BeeKeyStore::engineGetKey(const String& alias, const array<javachar>& password) throw (NoSuchAlgorithmException, UnrecoverableKeyException)
{
	Key* result = 0;

	_lock.lock();
	entry_map::iterator it = _entries.find(alias);
	if (it != _entries.end())
	{
		KeyEntry* ke = dynamic_cast<KeyEntry*>(it->second);
		if (ke)
		{
			PKCS12PBEKey pbekey(password, &_salt, _iter);

			try
			{
				KeyProtector p(pbekey);

				result = p.recover(ke->encryptedkey);
			}
			catch (InvalidKeyException e)
			{
				_lock.unlock();
				throw KeyStoreException(e.getMessage());
			}
			catch (...)
			{
				_lock.unlock();
				throw;
			}
		}
	}
	_lock.unlock();

	return result;
}

bool BeeKeyStore::engineIsKeyEntry(const String& alias)
{
	bool result = false;
	_lock.lock();
	entry_map::iterator it = _entries.find(alias);
	if (it != _entries.end())
		result = (dynamic_cast<KeyEntry*>(it->second) != 0);
	_lock.unlock();
	return result;
}

void BeeKeyStore::engineSetKeyEntry(const String& alias, const bytearray& key, const vector<Certificate*>& chain) throw (KeyStoreException)
{
	_lock.lock();
	_entries[alias] = new KeyEntry(key, chain);
	_lock.unlock();
}

void BeeKeyStore::engineSetKeyEntry(const String& alias, const Key& key, const array<javachar>& password, const vector<Certificate*>& chain) throw (KeyStoreException)
{
	PKCS12PBEKey pbekey(password, &_salt, _iter);

	try
	{
		const PrivateKey* pri = dynamic_cast<const PrivateKey*>(&key);
		if (pri)
		{
			KeyProtector p(pbekey);

			bytearray *tmp = p.protect(*pri);

			if (tmp)
				engineSetKeyEntry(alias, *tmp, chain);
			else
				throw KeyStoreException("Failed to protect key");
		}
		else
			throw KeyStoreException("BeeKeyStore only supports storing of PrivateKey objects");
	}
	catch (InvalidKeyException e)
	{
		throw KeyStoreException(e.getMessage());
	}
}

size_t BeeKeyStore::engineSize() const
{
	return _entries.size();
}

void BeeKeyStore::engineLoad(InputStream* in, const array<javachar>* password) throw (IOException, CertificateException, NoSuchAlgorithmException)
{
	_lock.lock();

	if (in == 0)
	{
		randomGeneratorContext rngc;

		/* salt size default is 64 bytes */
		_salt.resize(64);
		/* generate a new salt */
		randomGeneratorContextNext(&rngc, _salt.data(), _salt.size());
		/* set default iteration count */
		_iter = 1024;

		_lock.unlock();

		return;
	}

	Mac* m = 0;

	try
	{
		m = Mac::getInstance("HMAC-SHA-256");

		MacInputStream mis(*in, *m);
		DataInputStream dis(mis);

		mis.on(false);

		javaint magic = dis.readInt();
		javaint version = dis.readInt();

		if (magic != BKS_MAGIC || version != BKS_VERSION_1)
			throw IOException("Invalid KeyStore format");

		clearall();

		javaint saltsize = dis.readInt();
		if (saltsize <= 0)
			throw IOException("Invalid KeyStore salt size");

		_salt.resize(saltsize);
		dis.readFully(_salt);

		_iter = dis.readInt();
		if (_iter <= 0)
			throw IOException("Invalid KeyStore iteration count");

		PKCS12PBEKey pbekey(password ? *password : EMPTY_PASSWORD, &_salt, _iter);

		m->init(pbekey);

		mis.on(true);

		javaint entrycount = dis.readInt();

		if (entrycount <= 0)
			throw IOException("Invalid KeyStore entry count");

		for (javaint i = 0; i < entrycount; i++)
		{
			String alias;

			switch (dis.readInt())
			{
			case BKS_PRIVATEKEY_ENTRY:
				{
					dis.readUTF(alias);

					KeyEntry* e = new KeyEntry;

					try
					{
						e->date.setTime(dis.readLong());

						javaint keysize = dis.readInt();

						if (keysize <= 0)
							throw IOException("Invalid KeyStore key length");

						e->encryptedkey.resize((size_t) keysize);

						dis.readFully(e->encryptedkey);

						javaint certcount = dis.readInt();

						if (certcount <= 0)
							throw IOException("Invalid KeyStore certificate count");

						for (javaint j = 0; j < certcount; j++)
						{
							String type;

							dis.readUTF(type);

							// see if we have a CertificateFactory of this type available
							CertificateFactory* cf = _certfactories[type];
							if (!cf)
							{
								// apparently not; get a new one and cache it
								_certfactories[type] = cf = CertificateFactory::getInstance(type);
							}

							javaint certsize = dis.readInt();

							if (certsize <= 0)
								throw IOException("Invalid KeyStore certificate size");
						
							bytearray cert(certsize);

							dis.readFully(cert);

							ByteArrayInputStream bis(cert);

							e->chain.push_back(cf->generateCertificate(bis));
						}

						_entries[alias] = e;
					}
					catch (...)
					{
						delete e;
						throw;
					}
				}
				break;

			case BKS_CERTIFICATE_ENTRY:
				{
					dis.readUTF(alias);

					CertEntry* e = new CertEntry;

					try
					{
						e->date.setTime(dis.readLong());

						String type;

						dis.readUTF(type);

						// see if we have a CertificateFactory of this type available
						CertificateFactory* cf = _certfactories[type];
						if (!cf)
						{
							// apparently not; get a new one and cache it
							_certfactories[type] = cf = CertificateFactory::getInstance(type);
						}

						javaint certsize = dis.readInt();

						if (certsize <= 0)
							throw IOException("Invalid KeyStore certificate size");

						bytearray cert(certsize);

						dis.readFully(cert);

						ByteArrayInputStream bis(cert);

						e->cert = cf->generateCertificate(bis);

						_entries[alias] = e;
					}
					catch (...)
					{
						delete e;
						throw;
					}
				}
				break;

			default:
				throw IOException("Invalid KeyStore entry tag");
			}
		}

		bytearray computed_mac, original_mac;

		mis.on(false);

		javaint macsize = dis.available();
		if (macsize <= 0)
			throw IOException("Invalid KeyStore MAC size");

		computed_mac = m->doFinal();
		if (macsize != computed_mac.size())
			throw KeyStoreException("KeyStore has been tampered with, or password was incorrect");

		original_mac.resize(macsize);
		dis.readFully(original_mac);

		if (computed_mac != original_mac)
			throw KeyStoreException("KeyStore has been tampered with, or password was incorrect");

		delete m;
	}
	catch (...)
	{
		if (m)
			delete m;

		_lock.unlock();
		throw;
	}

	_lock.unlock();
}

void BeeKeyStore::engineStore(OutputStream& out, const array<javachar>* password) throw (IOException, CertificateException, NoSuchAlgorithmException)
{
	_lock.lock();

	Mac* m = 0;

	try
	{
		m = Mac::getInstance("HMAC-SHA-256");

		PKCS12PBEKey pbekey(password ? *password : EMPTY_PASSWORD, &_salt, _iter);

		m->init(pbekey);

		MacOutputStream mos(out, *m);
		DataOutputStream dos(mos);

		mos.on(false);
		dos.writeInt(BKS_MAGIC);
		dos.writeInt(BKS_VERSION_1);
		dos.writeInt(_salt.size());
		dos.write(_salt);
		dos.writeInt(_iter);
		mos.on(true);
		dos.writeInt(_entries.size());

		for (entry_map::const_iterator it = _entries.begin(); it != _entries.end(); ++it)
		{
			const KeyEntry* ke = dynamic_cast<const KeyEntry*>(it->second);
			if (ke)
			{
				dos.writeInt(BKS_PRIVATEKEY_ENTRY);
				dos.writeUTF(it->first);
				dos.writeLong(ke->date.getTime());
				dos.writeInt(ke->encryptedkey.size());
				dos.write(ke->encryptedkey);
				/* next do all the certificates for this key */
				dos.writeInt(ke->chain.size());
				for (vector<Certificate*>::const_iterator cit = ke->chain.begin(); cit != ke->chain.end(); ++cit)
				{
					const Certificate* cert = *cit;

					dos.writeUTF(cert->getType());
					dos.writeInt(cert->getEncoded().size());
					dos.write(cert->getEncoded());
				}
				continue;
			}

			const CertEntry* ce = dynamic_cast<const CertEntry*>(it->second);
			if (ce)
			{
				dos.writeInt(BKS_CERTIFICATE_ENTRY);
				dos.writeUTF(it->first);
				dos.writeLong(ce->date.getTime());
				dos.writeUTF(ce->cert->getType());
				dos.writeInt(ce->cert->getEncoded().size());
				dos.write(ce->cert->getEncoded());
				continue;
			}

			throw RuntimeException();
		}
		/* don't call close on a FilterOutputStream because the
		 * underlying stream still has to write data!
		 */
		dos.flush();
		mos.flush();

		out.write(m->doFinal());
		out.close();

		_lock.unlock();
	}
	catch (...)
	{
		_lock.unlock();
		throw;
	}
}
