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

/*!\file KeyStore.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_KEYSTORE_H
#define _CLASS_KEYSTORE_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/io/InputStream.h"
using beecrypt::io::InputStream;
#include "beecrypt/c++/io/OutputStream.h"
using beecrypt::io::OutputStream;
#include "beecrypt/c++/security/KeyStoreSpi.h"
using beecrypt::security::KeyStoreSpi;
#include "beecrypt/c++/security/KeyStoreException.h"
using beecrypt::security::KeyStoreException;
#include "beecrypt/c++/security/Provider.h"
using beecrypt::security::Provider;
#include "beecrypt/c++/security/NoSuchProviderException.h"
using beecrypt::security::NoSuchProviderException;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI KeyStore
		{
		public:
			static KeyStore* getInstance(const String&) throw (KeyStoreException);
			static KeyStore* getInstance(const String&, const String&) throw (KeyStoreException, NoSuchProviderException);
			static KeyStore* getInstance(const String&, const Provider&) throw (KeyStoreException);

			static const String& getDefaultType();

		private:
			KeyStoreSpi*    _kspi;
			String          _type;
			const Provider* _prov;
			bool            _init;

		protected:
			KeyStore(KeyStoreSpi*, const String&, const Provider&);

		public:
			~KeyStore();

			Enumeration* aliases();
			bool containsAlias(const String&) throw (KeyStoreException);

			const Certificate* getCertificate(const String&) throw (KeyStoreException);
			const String& getCertificateAlias(const Certificate&) throw (KeyStoreException);
			const vector<Certificate*>* getCertificateChain(const String&) throw (KeyStoreException);
			bool isCertificateEntry(const String& alias) throw (KeyStoreException);
			void setCertificateEntry(const String& alias, const Certificate& cert) throw (KeyStoreException);
				
			void deleteEntry(const String&) throw (KeyStoreException);
				
			Key* getKey(const String& alias, const array<javachar>& password) throw (KeyStoreException, NoSuchAlgorithmException, UnrecoverableKeyException);
			bool isKeyEntry(const String& alias) throw (KeyStoreException);
			void setKeyEntry(const String& alias, const bytearray& key, const vector<Certificate*>&) throw (KeyStoreException);
			void setKeyEntry(const String& alias, const Key& key, const array<javachar>& password, const vector<Certificate*>&) throw (KeyStoreException);

			size_t size() const throw (KeyStoreException);

			void load(InputStream* in, const array<javachar>* password) throw (IOException, NoSuchAlgorithmException, CertificateException);
			void store(OutputStream& out, const array<javachar>* password) throw (KeyStoreException, IOException, NoSuchAlgorithmException, CertificateException);

			const String& getType() const throw ();
			const Provider& getProvider() const throw ();
		};
	}
}

#endif

#endif
