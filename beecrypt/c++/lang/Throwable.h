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

/*!\file Throwable.h
 * \ingroup CXX_LANG_m
 */

#ifndef _CLASS_THROWABLE_H
#define _CLASS_THROWABLE_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/lang/String.h"
using beecrypt::lang::String;

namespace beecrypt {
	namespace lang {
		class BEECRYPTCXXAPI Throwable
		{
			private:
				String _msg;

			public:
				Throwable() throw ();
				Throwable(const String&) throw ();
				Throwable(const Throwable&) throw ();

				virtual ~Throwable() throw () {};

				const String& getMessage() const throw ();
		};
	}
}

#endif

#endif
