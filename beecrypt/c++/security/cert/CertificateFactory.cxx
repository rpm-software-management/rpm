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

#include "beecrypt/c++/security/Security.h"
#include "beecrypt/c++/security/cert/CertificateFactory.h"

using namespace beecrypt::security::cert;

CertificateFactory::CertificateFactory(CertificateFactorySpi* spi, const String& type, const Provider& provider)
{
	_cspi = spi;
	_type = type;
	_prov = &provider;
}

CertificateFactory::~CertificateFactory()
{
	delete _cspi;
}

CertificateFactory* CertificateFactory::getInstance(const String& type) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(type, "CertificateFactory");

	CertificateFactory* result = new CertificateFactory((CertificateFactorySpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

CertificateFactory* CertificateFactory::getInstance(const String& type, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException)
{
	Security::spi* tmp = Security::getSpi(type, "CertificateFactory", provider);

	CertificateFactory* result = new CertificateFactory((CertificateFactorySpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

CertificateFactory* CertificateFactory::getInstance(const String& type, const Provider& provider) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(type, "CertificateFactory", provider);

	CertificateFactory* result = new CertificateFactory((CertificateFactorySpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

Certificate* CertificateFactory::generateCertificate(InputStream& in) throw (CertificateException)
{
	return _cspi->engineGenerateCertificate(in);
}

vector<Certificate*>* CertificateFactory::generateCertificates(InputStream& in) throw (CertificateException)
{
	return _cspi->engineGenerateCertificates(in);
}

const String& CertificateFactory::getType() const throw ()
{
	return _type;
}

const Provider& CertificateFactory::getProvider() const throw ()
{
	return *_prov;
}
