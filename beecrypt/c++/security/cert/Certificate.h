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

/*!\file Certificate.h
 * \ingroup CXX_SECURITY_CERT_m
 */

#ifndef _CLASS_CERTIFICATE_H
#define _CLASS_CERTIFICATE_H

#ifdef __cplusplus

#include "beecrypt/c++/array.h"
using beecrypt::array;
#include "beecrypt/c++/security/PublicKey.h"
using beecrypt::security::PublicKey;
#include "beecrypt/c++/security/InvalidKeyException.h"
using beecrypt::security::InvalidKeyException;
#include "beecrypt/c++/security/NoSuchAlgorithmException.h"
using beecrypt::security::NoSuchAlgorithmException;
#include "beecrypt/c++/security/NoSuchProviderException.h"
using beecrypt::security::NoSuchProviderException;
#include "beecrypt/c++/security/SignatureException.h"
using beecrypt::security::SignatureException;
#include "beecrypt/c++/security/cert/CertificateException.h"
using beecrypt::security::cert::CertificateException;

namespace beecrypt {
	namespace security {
		namespace cert {
			class BEECRYPTCXXAPI Certificate
			{
				private:
					String _type;

				protected:
					Certificate(const String& type);

				public:
					virtual ~Certificate();

					virtual bool operator==(const Certificate&) const;

					virtual Certificate* clone() const = 0;

					virtual const bytearray& getEncoded() const = 0;
					virtual const PublicKey& getPublicKey() const = 0;

					virtual void verify(const PublicKey&) throw (CertificateException, NoSuchAlgorithmException, InvalidKeyException, NoSuchProviderException, SignatureException) = 0;
					virtual void verify(const PublicKey&, const String&) throw (CertificateException, NoSuchAlgorithmException, InvalidKeyException, NoSuchProviderException, SignatureException) = 0;

					virtual const String& toString() const throw () = 0;

					const String& getType() const throw ();
			};
		}
	}
}

#endif

#endif
