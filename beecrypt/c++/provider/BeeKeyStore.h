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

/*!\file BeeKeyStore.h
 * \ingroup CXX_PROVIDER_m
 */

#ifndef _CLASS_BEEKEYSTORE_H
#define _CLASS_BEEKEYSTORE_H

#ifdef __cplusplus

#include "beecrypt/c++/mutex.h"
using beecrypt::mutex;
#include "beecrypt/c++/security/KeyStoreSpi.h"
using beecrypt::security::KeyStoreSpi;
#include "beecrypt/c++/security/KeyFactory.h"
using beecrypt::security::KeyFactory;
#include "beecrypt/c++/security/cert/CertificateFactory.h"
using beecrypt::security::cert::CertificateFactory;
#include "beecrypt/c++/util/Enumeration.h"
using beecrypt::util::Enumeration;

#include <map>
using std::map;

namespace beecrypt {
	namespace provider {
		/*!\brief The default BeeCrypt KeyStore.
		*/
		class BeeKeyStore : public KeyStoreSpi
		{
		private:
			mutex _lock;
			bytearray _bmac;
			bytearray _salt;
			size_t _iter;

			struct Entry
			{
				Date date;
				virtual ~Entry();
			};

			struct KeyEntry : public Entry
			{
				bytearray encryptedkey;
				vector<Certificate*> chain;

				KeyEntry();
				KeyEntry(const bytearray& key, const vector<Certificate*>&);
				virtual ~KeyEntry();
			};

			struct CertEntry : public Entry
			{
				Certificate* cert;

				CertEntry();
				CertEntry(const Certificate&);
				virtual ~CertEntry();
			};

			typedef map<String, KeyFactory*> keyfactory_map;
			keyfactory_map _keyfactories;

			typedef map<String, CertificateFactory*> certfactory_map;
			certfactory_map _certfactories;

			typedef map<String, Entry*> entry_map;
			entry_map _entries;

			struct AliasEnum : public Enumeration
			{
				entry_map::const_iterator _it;
				entry_map::const_iterator _end;

				AliasEnum(const entry_map&);
				virtual ~AliasEnum() throw ();

				virtual bool hasMoreElements() throw ();
				virtual const void* nextElement() throw (NoSuchElementException);
			};

			void clearall();

		protected:
			virtual Enumeration* engineAliases();

			virtual bool engineContainsAlias(const String& alias);

			virtual void engineDeleteEntry(const String& alias) throw (KeyStoreException);
			virtual const Date* engineGetCreationDate(const String& alias);

			virtual const Certificate* engineGetCertificate(const String& alias);
			virtual const String* engineGetCertificateAlias(const Certificate& cert);
			virtual const vector<Certificate*>* engineGetCertificateChain(const String& alias);
			virtual bool engineIsCertificateEntry(const String& alias);
			virtual void engineSetCertificateEntry(const String& alias, const Certificate& cert) throw (KeyStoreException);

			virtual Key* engineGetKey(const String& alias, const array<javachar>& password) throw (NoSuchAlgorithmException, UnrecoverableKeyException);
			virtual bool engineIsKeyEntry(const String& alias);
			virtual void engineSetKeyEntry(const String& alias, const bytearray& key, const vector<Certificate*>&) throw (KeyStoreException);
			virtual void engineSetKeyEntry(const String& alias, const Key& key, const array<javachar>& password, const vector<Certificate*>&) throw (KeyStoreException);

			virtual size_t engineSize() const;

			virtual void engineLoad(InputStream* in, const array<javachar>* password) throw (IOException, CertificateException, NoSuchAlgorithmException);
			virtual void engineStore(OutputStream& out, const array<javachar>* password) throw (IOException, CertificateException, NoSuchAlgorithmException);

		public:
			BeeKeyStore();
			~BeeKeyStore();
		};
	}
}

#endif

#endif
