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

/*!\file BeeCertificate.h
 * \ingroup CXX_BEEYOND_m
 */

#ifndef _CLASS_BEECERTIFICATE_H
#define _CLASS_BEECERTIFICATE_H

#ifdef __cplusplus

#include "beecrypt/c++/array.h"
using beecrypt::array;
#include "beecrypt/c++/io/DataInputStream.h"
using beecrypt::io::DataInputStream;
#include "beecrypt/c++/io/DataOutputStream.h"
using beecrypt::io::DataOutputStream;
#include "beecrypt/c++/provider/BeeCertificateFactory.h"
using beecrypt::provider::BeeCertificateFactory;
#include "beecrypt/c++/security/PublicKey.h"
using beecrypt::security::PublicKey;
#include "beecrypt/c++/security/PrivateKey.h"
using beecrypt::security::PrivateKey;
#include "beecrypt/c++/security/cert/Certificate.h"
using beecrypt::security::cert::Certificate;
#include "beecrypt/c++/security/cert/CertificateExpiredException.h"
using beecrypt::security::cert::CertificateExpiredException;
#include "beecrypt/c++/security/cert/CertificateNotYetValidException.h"
using beecrypt::security::cert::CertificateNotYetValidException;
#include "beecrypt/c++/util/Date.h"
using beecrypt::util::Date;

#include <vector>
using std::vector;

namespace beecrypt {
	namespace beeyond {
		/* We use short certificate chains, embedded in the certificate as parent certificates
		* Issuer is informational
		* Subject is used to identify the type of certificate
		*/
		class BEECRYPTCXXAPI BeeCertificate : public Certificate
		{
			friend class BeeCertificateFactory;

			public:
				static const Date FOREVER;

			protected:
				struct Field
				{
					javaint type;

					virtual ~Field();

					virtual Field* clone() const = 0;

					virtual void decode(DataInputStream&) throw (IOException) = 0;
					virtual void encode(DataOutputStream&) const throw (IOException) = 0;
				};

				struct UnknownField : public Field
				{
					bytearray encoding;

					UnknownField();
					UnknownField(const UnknownField&);
					virtual ~UnknownField();
					
					virtual Field* clone() const;

					virtual void decode(DataInputStream&) throw (IOException);
					virtual void encode(DataOutputStream&) const throw (IOException);
				};

				struct PublicKeyField : public Field
				{
					static const javaint FIELD_TYPE;

					PublicKey* pub;

					PublicKeyField();
					PublicKeyField(const PublicKey& key);
					virtual ~PublicKeyField();

					virtual Field* clone() const;

					virtual void decode(DataInputStream&) throw (IOException);
					virtual void encode(DataOutputStream&) const throw (IOException);
				};

				struct ParentCertificateField : public Field
				{
					static const javaint FIELD_TYPE;

					Certificate* parent;

					ParentCertificateField();
					ParentCertificateField(const Certificate&);
					virtual ~ParentCertificateField();

					virtual Field* clone() const;

					virtual void decode(DataInputStream&) throw (IOException);
					virtual void encode(DataOutputStream&) const throw (IOException);
				};

				virtual Field* instantiateField(javaint type);

			public:
				typedef vector<Field*> fields_vector;
				typedef vector<Field*>::iterator fields_iterator;
				typedef vector<Field*>::const_iterator fields_const_iterator;

			protected:
				String        issuer;
				String        subject;
				Date          created;
				Date          expires;
				fields_vector fields;
				String        signature_algorithm;
				bytearray     signature;

				mutable bytearray* enc;
				mutable String* str;

				BeeCertificate();
				BeeCertificate(InputStream& in) throw (IOException);

				bytearray* encodeTBS() const;

			public:
				BeeCertificate(const BeeCertificate&);
				virtual ~BeeCertificate();

				virtual BeeCertificate* clone() const;

				virtual const bytearray& getEncoded() const;
				virtual const PublicKey& getPublicKey() const;

				virtual void verify(const PublicKey&) throw (CertificateException, NoSuchAlgorithmException, InvalidKeyException, NoSuchProviderException, SignatureException);
				virtual void verify(const PublicKey&, const String&) throw (CertificateException, NoSuchAlgorithmException, InvalidKeyException, NoSuchProviderException, SignatureException);
				virtual const String& toString() const throw ();

				void checkValidity() const throw (CertificateExpiredException, CertificateNotYetValidException);
				void checkValidity(const Date&) const throw (CertificateExpiredException, CertificateNotYetValidException);

				const String& getIssuer() const throw ();
				const String& getSubject() const throw ();

				const Date& getNotAfter() const throw ();
				const Date& getNotBefore() const throw ();

				const bytearray& getSignature() const throw ();
				const String& getSigAlgName() const throw ();

				bool hasPublicKey() const;
				bool hasParentCertificate() const;

				const Certificate& getParentCertificate() const;

			public:
				static BeeCertificate* self(const PublicKey&, const PrivateKey&, const String& sigAlgName) throw (InvalidKeyException, NoSuchAlgorithmException);
		};
	}
}

#endif

#endif
