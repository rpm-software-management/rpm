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

/*!\file CertificateFactory.h
 * \ingroup CXX_SECURITY_CERT_m
 */

#ifndef _CLASS_CERTIFICATEFACTORY_H
#define _CLASS_CERTIFICATEFACTORY_H

#ifdef __cplusplus

#include "beecrypt/c++/lang/String.h"
using beecrypt::lang::String;
#include "beecrypt/c++/security/Provider.h"
using beecrypt::security::Provider;
#include "beecrypt/c++/security/NoSuchAlgorithmException.h"
using beecrypt::security::NoSuchAlgorithmException;
#include "beecrypt/c++/security/cert/Certificate.h"
using beecrypt::security::cert::Certificate;
#include "beecrypt/c++/security/cert/CertificateFactorySpi.h"
using beecrypt::security::cert::CertificateFactorySpi;

#include <vector>
using std::vector;

namespace beecrypt {
	namespace security {
		namespace cert {
			class BEECRYPTCXXAPI CertificateFactory
			{
				public:
					static CertificateFactory* getInstance(const String&) throw (NoSuchAlgorithmException);
					static CertificateFactory* getInstance(const String&, const String&) throw (NoSuchAlgorithmException, NoSuchProviderException);
					static CertificateFactory* getInstance(const String&, const Provider&) throw (NoSuchAlgorithmException);

				private:
					CertificateFactorySpi* _cspi;
					String                 _type;
					const Provider*        _prov;

				protected:
					CertificateFactory(CertificateFactorySpi*, const String&, const Provider&);

				public:
					~CertificateFactory();

					Certificate* generateCertificate(InputStream& in) throw (CertificateException);
					vector<Certificate*>* generateCertificates(InputStream& in) throw (CertificateException);

					const String& getType() const throw ();
					const Provider& getProvider() const throw ();
			};
		}
	}
}

#endif

#endif
