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

/*!\file CertificateExpiredException.h
 * \ingroup CXX_SECURITY_CERT_m
 */

#ifndef _CLASS_CERTIFICATEEXPIREDEXCEPTION_H
#define _CLASS_CERTIFICATEEXPIREDEXCEPTION_H

#ifdef __cplusplus

#include "beecrypt/c++/security/cert/CertificateException.h"
using beecrypt::security::cert::CertificateException;

namespace beecrypt {
	namespace security {
		namespace cert {
			class BEECRYPTCXXAPI CertificateExpiredException : public CertificateException
			{
				public:
					CertificateExpiredException() throw ();
					CertificateExpiredException(const String&) throw ();
			};
		}
	}
}

#endif

#endif
