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

/*!\file KeyStoreSpi.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_KEYSTORESPI_H
#define _CLASS_KEYSTORESPI_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/io/InputStream.h"
using beecrypt::io::InputStream;
#include "beecrypt/c++/io/OutputStream.h"
using beecrypt::io::OutputStream;
#include "beecrypt/c++/security/KeyStoreException.h"
using beecrypt::security::KeyStoreException;
#include "beecrypt/c++/security/NoSuchAlgorithmException.h"
using beecrypt::security::NoSuchAlgorithmException;
#include "beecrypt/c++/security/NoSuchProviderException.h"
using beecrypt::security::NoSuchProviderException;
#include "beecrypt/c++/security/UnrecoverableKeyException.h"
using beecrypt::security::UnrecoverableKeyException;
#include "beecrypt/c++/security/cert/Certificate.h"
using beecrypt::security::cert::Certificate;
#include "beecrypt/c++/security/cert/CertificateException.h"
using beecrypt::security::cert::CertificateException;
#include "beecrypt/c++/util/Date.h"
using beecrypt::util::Date;
#include "beecrypt/c++/util/Enumeration.h"
using beecrypt::util::Enumeration;

#include <typeinfo>
using std::type_info;
#include <vector>
using std::vector;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI KeyStoreSpi
		{
			friend class KeyStore;

			protected:
				virtual Enumeration* engineAliases() = 0;
				virtual bool engineContainsAlias(const String&) = 0;
				
				virtual void engineDeleteEntry(const String&) throw (KeyStoreException) = 0;
				virtual const Date* engineGetCreationDate(const String&) = 0;

				virtual const Certificate* engineGetCertificate(const String&) = 0;
				virtual const String* engineGetCertificateAlias(const Certificate&) = 0;
				virtual const vector<Certificate*>* engineGetCertificateChain(const String&) = 0;
				virtual bool engineIsCertificateEntry(const String& alias) = 0;
				virtual void engineSetCertificateEntry(const String& alias, const Certificate& cert) throw (KeyStoreException) = 0;
				
				virtual Key* engineGetKey(const String& alias, const array<javachar>& password) throw (NoSuchAlgorithmException, UnrecoverableKeyException) = 0;
				virtual bool engineIsKeyEntry(const String& alias) = 0;
				virtual void engineSetKeyEntry(const String& alias, const bytearray& key, const vector<Certificate*>&) throw (KeyStoreException) = 0;
				virtual void engineSetKeyEntry(const String& alias, const Key& key, const array<javachar>& password, const vector<Certificate*>&) throw (KeyStoreException) = 0;

				virtual size_t engineSize() const = 0;

				virtual void engineLoad(InputStream* in, const array<javachar>* password) throw (IOException, CertificateException, NoSuchAlgorithmException) = 0;
				virtual void engineStore(OutputStream& out, const array<javachar>* password) throw (IOException, CertificateException, NoSuchAlgorithmException) = 0;

			public:
				virtual ~KeyStoreSpi() {};
		};
	}
}

#endif

#endif
