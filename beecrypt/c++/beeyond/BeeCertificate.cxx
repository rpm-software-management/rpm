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

#include "beecrypt/c++/beeyond/BeeCertificate.h"
#include "beecrypt/c++/beeyond/AnyEncodedKeySpec.h"
#include "beecrypt/c++/io/ByteArrayInputStream.h"
using beecrypt::io::ByteArrayInputStream;
#include "beecrypt/c++/io/ByteArrayOutputStream.h"
using beecrypt::io::ByteArrayOutputStream;
#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;
#include "beecrypt/c++/security/KeyFactory.h"
using beecrypt::security::KeyFactory;
#include "beecrypt/c++/security/Signature.h"
using beecrypt::security::Signature;
#include "beecrypt/c++/security/cert/CertificateFactory.h"
using beecrypt::security::cert::CertificateFactory;

using namespace beecrypt::beeyond;

BeeCertificate::Field::~Field()
{
}

BeeCertificate::UnknownField::UnknownField()
{
}

BeeCertificate::UnknownField::UnknownField(const UnknownField& copy) : encoding(copy.encoding)
{
	type = copy.type;
}

BeeCertificate::UnknownField::~UnknownField()
{
}

BeeCertificate::Field* BeeCertificate::UnknownField::clone() const
{
	return new BeeCertificate::UnknownField(*this);
}

void BeeCertificate::UnknownField::decode(DataInputStream& in) throw (IOException)
{
	encoding.resize(in.available());

	in.readFully(encoding);
}

void BeeCertificate::UnknownField::encode(DataOutputStream& out) const throw (IOException)
{
	out.write(encoding);
}

const javaint BeeCertificate::PublicKeyField::FIELD_TYPE = 0x5055424b; // 'PUBK'

BeeCertificate::PublicKeyField::PublicKeyField()
{
	type = BeeCertificate::PublicKeyField::FIELD_TYPE;
	pub = 0;
}

BeeCertificate::PublicKeyField::PublicKeyField(const PublicKey& key)
{
	type = BeeCertificate::PublicKeyField::FIELD_TYPE;
	pub = key.clone();
}

BeeCertificate::PublicKeyField::~PublicKeyField()
{
	delete pub;
}

BeeCertificate::Field* BeeCertificate::PublicKeyField::clone() const
{
	return new BeeCertificate::PublicKeyField(*pub);
}

void BeeCertificate::PublicKeyField::decode(DataInputStream& in) throw (IOException)
{
	String format;

	in.readUTF(format);

	// no need for a try-catch around this; calling function is expecting a thrown NoSuchAlgorithmException
	KeyFactory* kf = KeyFactory::getInstance(format);

	try
	{
		javaint encsize = in.readInt();

		if (encsize <= 0)
			throw IOException("Invalid key encoding size");

		bytearray enc(encsize);

		in.readFully(enc);

		AnyEncodedKeySpec spec(format, enc);

		pub = kf->generatePublic(spec);

		delete kf;
	}
	catch (...)
	{
		delete kf;
		throw;
	}
}

void BeeCertificate::PublicKeyField::encode(DataOutputStream& out) const throw (IOException)
{
	out.writeUTF(*pub->getFormat());

	const bytearray* pubenc = pub->getEncoded();

	if (!pubenc)
		throw NullPointerException("PublicKey has no encoding");

	out.writeInt(pubenc->size());
	out.write(*pubenc);
}

const javaint BeeCertificate::ParentCertificateField::FIELD_TYPE = 0x43455254; // 'CERT'

BeeCertificate::ParentCertificateField::ParentCertificateField()
{
	type = BeeCertificate::ParentCertificateField::FIELD_TYPE;
	parent = 0;
}

BeeCertificate::ParentCertificateField::ParentCertificateField(const Certificate& cert)
{
	type = BeeCertificate::ParentCertificateField::FIELD_TYPE;
	parent = cert.clone();
}

BeeCertificate::ParentCertificateField::~ParentCertificateField()
{
	delete parent;
}

BeeCertificate::Field* BeeCertificate::ParentCertificateField::clone() const
{
	return new BeeCertificate::ParentCertificateField(*parent);
}

void BeeCertificate::ParentCertificateField::decode(DataInputStream& in) throw (IOException)
{
	String type;

	in.readUTF(type);

	CertificateFactory* cf = CertificateFactory::getInstance(type);

	try
	{
		javaint encsize = in.readInt();

		if (encsize <= 0)
			throw IOException("Invalid certificate encoding size");

		bytearray enc(encsize);

		in.readFully(enc);

		ByteArrayInputStream bin(enc);

		parent = cf->generateCertificate(bin);

		throw RuntimeException();

		delete cf;
	}
	catch (...)
	{
		delete cf;
		throw;
	}
}

void BeeCertificate::ParentCertificateField::encode(DataOutputStream& out) const throw (IOException)
{
	out.writeUTF(parent->getType());

	const bytearray& parentenc = parent->getEncoded();
	
	out.writeInt(parentenc.size());
	out.write(parentenc);
}

BeeCertificate::Field* BeeCertificate::instantiateField(javaint type)
{
	switch (type)
	{
	case PublicKeyField::FIELD_TYPE:
		return new PublicKeyField();

	case ParentCertificateField::FIELD_TYPE:
		return new ParentCertificateField();

	default:
		return new UnknownField();
	}
}

const Date BeeCertificate::FOREVER((javalong) -1L);

BeeCertificate::BeeCertificate() : Certificate("BEE")
{
	enc = 0;
}

BeeCertificate::BeeCertificate(InputStream& in) throw (IOException) : Certificate("BEE")
{
	enc = 0;

	DataInputStream dis(in);

	dis.readUTF(issuer);
	dis.readUTF(subject);

	created.setTime(dis.readLong());
	expires.setTime(dis.readLong());

	javaint fieldcount = dis.readInt();
	if (fieldcount < 0)
		throw IOException("field count < 0");

	for (javaint i = 0; i < fieldcount; i++)
	{
		bytearray fenc;

		javaint type = dis.readInt();
		javaint size = dis.readInt();

		fenc.resize(size);

		dis.readFully(fenc);

		ByteArrayInputStream bis(fenc);
		DataInputStream fis(bis);

		Field* f = instantiateField(type);

		try
		{
			f->decode(fis);
			fields.push_back(f);
		}
		catch (...)
		{
			delete f;
			throw;
		}
	}

	dis.readUTF(signature_algorithm);

	javaint siglength = dis.readInt();

	if (siglength < 0)
		throw IOException("signature length < 0");

	if (siglength > 0)
	{
		signature.resize(siglength);
		dis.readFully(signature);
	}
}

BeeCertificate::BeeCertificate(const BeeCertificate& copy) : Certificate("BEE")
{
	issuer = copy.issuer;
	subject = copy.subject;
	created = copy.created;
	expires = copy.expires;
	for (fields_const_iterator it = copy.fields.begin(); it != copy.fields.end(); it++)
		fields.push_back((*it)->clone());
	signature_algorithm = copy.signature_algorithm;
	signature = copy.signature;
	enc = 0;
}

BeeCertificate::~BeeCertificate()
{
	if (enc)
		delete enc;
}

BeeCertificate* BeeCertificate::clone() const
{
	return new BeeCertificate(*this);
}

const bytearray& BeeCertificate::getEncoded() const
{
	if (!enc)
	{
		// The following sequence shouldn't throw an exception
		ByteArrayOutputStream bos;
		DataOutputStream dos(bos);

		dos.writeUTF(issuer);
		dos.writeUTF(subject);
		dos.writeLong(created.getTime());
		dos.writeLong(expires.getTime());
		dos.writeInt(fields.size());

		for (fields_vector::const_iterator it = fields.begin(); it != fields.end(); it++)
		{
			ByteArrayOutputStream bout;
			DataOutputStream dout(bout);

			Field* f = (*it);

			f->encode(dout);
			dout.close();

			bytearray* fenc = bout.toByteArray();

			dos.writeInt(f->type);
			dos.writeInt(fenc->size());
			dos.write(*fenc);

			delete fenc;
		}

		dos.writeUTF(signature_algorithm);
		dos.writeInt(signature.size());
		dos.write(signature);
		dos.close();
		bos.close();

		enc = bos.toByteArray();
	}

	return *enc;
}

const PublicKey& BeeCertificate::getPublicKey() const
{
	for (fields_const_iterator it = fields.begin(); it != fields.end(); it++)
	{
		if ((*it)->type == PublicKeyField::FIELD_TYPE)
		{
			const PublicKeyField* f = dynamic_cast<const PublicKeyField*>(*it);

			if (f)
				return *f->pub;
			else
				throw GeneralSecurityException("Somebody's trying to cheat with a new Field subclass");
		}
	}

	throw CertificateException("BeeCertificate doesn't contain a PublicKey");
}

const Certificate& BeeCertificate::getParentCertificate() const
{
	for (fields_const_iterator it = fields.begin(); it != fields.end(); it++)
	{
		if ((*it)->type == ParentCertificateField::FIELD_TYPE)
		{
			const ParentCertificateField* f = dynamic_cast<const ParentCertificateField*>(*it);

			if (f)
				return *f->parent;
			else
				throw GeneralSecurityException("Somebody's trying to cheat with a new Field subclass");
		}
	}

	throw CertificateException("BeeCertificate doesn't contain a parent Certificate");
}

void BeeCertificate::verify(const PublicKey& pub) throw (CertificateException, NoSuchAlgorithmException, InvalidKeyException, NoSuchProviderException, SignatureException)
{
	Signature* sig = Signature::getInstance(signature_algorithm);

	try
	{
		sig->initVerify(pub);

		bytearray* tmp = encodeTBS();

		try
		{
			sig->update(*tmp);
			delete tmp;
		}
		catch (...)
		{
			delete tmp;
			throw;
		}

		if (!sig->verify(signature))
			throw CertificateException("signature doesn't match");

		delete sig;
	}
	catch (...)
	{
		delete sig;
		throw;
	}
}

void BeeCertificate::verify(const PublicKey& pub, const String& sigProvider) throw (CertificateException, NoSuchAlgorithmException, InvalidKeyException, NoSuchProviderException, SignatureException)
{
	Signature* sig = Signature::getInstance(signature_algorithm, sigProvider);

	try
	{
		sig->initVerify(pub);

		bytearray* tmp = encodeTBS();

		try
		{
			sig->update(*tmp);
			delete tmp;
		}
		catch (...)
		{
			delete tmp;
			throw;
		}

		if (!sig->verify(signature))
			throw CertificateException("signature doesn't match");

		delete sig;
	}
	catch (...)
	{
		delete sig;
		throw;
	}
}

const String& BeeCertificate::toString() const throw ()
{
	if (!str)
		str = new String();

	return *str;
}

void BeeCertificate::checkValidity() const throw (CertificateExpiredException, CertificateNotYetValidException)
{
	Date now;

	checkValidity(now);
}

void BeeCertificate::checkValidity(const Date& at) const throw (CertificateExpiredException, CertificateNotYetValidException)
{
	if (at.before(created))
		throw CertificateNotYetValidException();

	if (expires != FOREVER)
		if (at.after(expires))
			throw CertificateExpiredException();
}

const Date& BeeCertificate::getNotAfter() const throw ()
{
	return expires;
}

const Date& BeeCertificate::getNotBefore() const throw ()
{
	return created;
}

const bytearray& BeeCertificate::getSignature() const throw ()
{
	return signature;
}

const String& BeeCertificate::getSigAlgName() const throw ()
{
	return signature_algorithm;
}

bool BeeCertificate::hasPublicKey() const
{
	for (fields_vector::const_iterator it = fields.begin(); it != fields.end(); it++)
	{
		switch ((*it)->type)
		{
		case PublicKeyField::FIELD_TYPE:
			// do an extra check with dynamic_cast
			if (dynamic_cast<PublicKeyField*>(*it))
				return true;
			else
				throw GeneralSecurityException("Somebody's trying to cheat with a new Field subclass");
		}
	}
	return false;
}

bool BeeCertificate::hasParentCertificate() const
{
	for (fields_vector::const_iterator it = fields.begin(); it != fields.end(); it++)
	{
		switch ((*it)->type)
		{
		case ParentCertificateField::FIELD_TYPE:
			// do an extra check with dynamic_cast
			if (dynamic_cast<ParentCertificateField*>(*it))
				return true;
			else
				throw GeneralSecurityException("Somebody's trying to cheat with a new Field subclass");
		}
	}
	return false;
}

bytearray* BeeCertificate::encodeTBS() const
{
	ByteArrayOutputStream bos;
	DataOutputStream dos(bos);

	dos.writeUTF(issuer);
	dos.writeUTF(subject);
	dos.writeLong(created.getTime());
	dos.writeLong(expires.getTime());
	dos.writeInt(fields.size());
	for (fields_vector::const_iterator it = fields.begin(); it != fields.end(); it++)
	{
		Field* f = (*it);

		dos.writeInt(f->type);
		f->encode(dos);
	}

	dos.close();
	bos.close();
	return bos.toByteArray();
}

BeeCertificate* BeeCertificate::self(const PublicKey& pub, const PrivateKey& pri, const String& signatureAlgorithm) throw (InvalidKeyException, NoSuchAlgorithmException)
{
	// if the public key doesn't have an encoding, it's not worth going through the effort
	if (!pub.getEncoded())
		throw InvalidKeyException("PublicKey doesn't have an encoding");

	Signature* sig = Signature::getInstance(signatureAlgorithm);

	try
	{
		sig->initSign(pri);

		BeeCertificate* cert = new BeeCertificate();

		try
		{
			// issuer is kept blank
			cert->subject = "PublicKey Certificate";
			cert->expires = FOREVER;
			cert->signature_algorithm = signatureAlgorithm;
			cert->fields.push_back(new PublicKeyField(pub));

			bytearray* tmp = cert->encodeTBS();

			try
			{
				sig->update(*tmp);
				delete tmp;
			}
			catch (...)
			{
				delete tmp;
				throw;
			}

			sig->sign(cert->signature);
		}
		catch (...)
		{
			delete cert;
			throw;
		}

		delete sig;

		return cert;
	}
	catch (...)
	{
		delete sig;
		throw;
	}
}
