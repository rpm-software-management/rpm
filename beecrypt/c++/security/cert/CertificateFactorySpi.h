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

/*!\file CertificateFactorySpi.h
 * \ingroup CXX_SECURITY_CERT_m
 */

#ifndef _CLASS_CERTIFICATEFACTORYSPI_H
#define _CLASS_CERTIFICATEFACTORYSPI_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/io/InputStream.h"
using beecrypt::io::InputStream;
#include "beecrypt/c++/io/OutputStream.h"
using beecrypt::io::OutputStream;
#include "beecrypt/c++/security/cert/Certificate.h"
using beecrypt::security::cert::Certificate;

#include <vector>
using std::vector;

namespace beecrypt {
	namespace security {
		namespace cert {
			class BEECRYPTCXXAPI CertificateFactorySpi
			{
				friend class CertificateFactory;

				protected:
					virtual Certificate* engineGenerateCertificate(InputStream& in) throw (CertificateException) = 0;
					virtual vector<Certificate*>* engineGenerateCertificates(InputStream& in) throw (CertificateException) = 0;

				public:
					virtual ~CertificateFactorySpi() {};
			};
		}
	}
}

#endif

#endif
