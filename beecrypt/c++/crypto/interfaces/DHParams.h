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

/*!\file DHParams.h
 * \ingroup CXX_CRYPTO_INTERFACES_m
 */

#ifndef _INTERFACE_DHPARAMS_H
#define _INTERFACE_DHPARAMS_H

#include "beecrypt/api.h"
#include "beecrypt/mpbarrett.h"

#ifdef __cplusplus

namespace beecrypt {
	namespace crypto {
		namespace interfaces {
			class DHParams
			{
				public:
					virtual const mpbarrett& getP() const throw () = 0;
					virtual const mpnumber& getG() const throw () = 0;
					virtual size_t getL() const throw () = 0;
			};
		}
	}
}

#endif

#endif
